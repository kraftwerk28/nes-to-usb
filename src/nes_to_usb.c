#include "libopencm3/cm3/nvic.h"
#include "libopencm3/cm3/systick.h"
#include "libopencm3/stm32/dma.h"
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/rcc.h"
#include "libopencm3/stm32/timer.h"
#include "libopencm3/usb/hid.h"
#include "libopencm3/usb/usbd.h"

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "task.h"

#include "pinout.h"
#include "usb_descriptors.h"
#include "utils.h"

// USB Keyboard scan codes
#define KEY_RIGHT 0x4f // Keyboard Right Arrow
#define KEY_LEFT 0x50  // Keyboard Left Arrow
#define KEY_DOWN 0x51  // Keyboard Down Arrow
#define KEY_UP 0x52	   // Keyboard Up Arrow
#define KEY_Z 0x1d	   // Keyboard z and Z
#define KEY_A 0x04	   // Keyboard a and A
#define KEY_I 0x0c	   // Keyboard i and I
#define KEY_J 0x0d	   // Keyboard j and J
#define KEY_K 0x0e	   // Keyboard k and K
#define KEY_L 0x0f	   // Keyboard l and L
#define KEY_M 0x10	   // Keyboard m and M
#define KEY_N 0x11	   // Keyboard n and N
#define KEY_ENTER 0x28 // Keyboard Return (ENTER)

#define KEY_MOD_LALT 0x04
#define KEY_MOD_LSHIFT 0x02
#define KEY_MOD_LMETA 0x08
#define IS_MOD_MASK (1 << 8)

// NES Controller buttons
enum NES_BUTTON {
	NES_A = 0x01,
	NES_B = 0x02,
	NES_SELECT = 0x04,
	NES_START = 0x08,
	NES_UP = 0x10,
	NES_DOWN = 0x20,
	NES_LEFT = 0x40,
	NES_RIGHT = 0x80,
};

const uint16_t p1_keymap[8] = {
	KEY_Z, KEY_A, KEY_MOD_LALT | IS_MOD_MASK, KEY_ENTER, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
};
const uint16_t p2_keymap[8] = {
	KEY_M, KEY_N, KEY_MOD_LSHIFT | IS_MOD_MASK, KEY_MOD_LMETA | IS_MOD_MASK, KEY_I, KEY_J, KEY_K, KEY_L,
};

typedef struct {
	uint8_t p1, p1_old;
	uint8_t p2, p2_old;
} nes_button_state_t;

typedef struct {
	nes_button_state_t btn;
	uint8_t state;
} nes_fsm_t;

static nes_fsm_t nes_fsm = {0};

static QueueHandle_t nes_queue = NULL;

static uint8_t usb_ctl_buf[128] = {0};
static usbd_device *usb_device;
static usb_hid_kbd_report_t report = {0};

static void nes_clk_tick(void) {
	if (nes_fsm.state % 2 == 0) {
		if (nes_fsm.state == 0) {
			gpio_set(NES_GPIO_LATCH_PORT, NES_GPIO_LATCH_PIN);
			nes_fsm.btn.p1 = 0;
			nes_fsm.btn.p2 = 0;
		} else {
			gpio_set(NES_GPIO_CLK_PORT, NES_GPIO_CLK_PIN);
		}
	} else {
		if (nes_fsm.state == 1) {
			gpio_clear(NES_GPIO_LATCH_PORT, NES_GPIO_LATCH_PIN);
		} else {
			gpio_clear(NES_GPIO_CLK_PORT, NES_GPIO_CLK_PIN);
		}
		if (!gpio_get(NES_GPIO_MISO_1_PORT, NES_GPIO_MISO_1_PIN)) {
			nes_fsm.btn.p1 |= 1 << (nes_fsm.state / 2);
		}
		if (!gpio_get(NES_GPIO_MISO_2_PORT, NES_GPIO_MISO_2_PIN)) {
			nes_fsm.btn.p2 |= 1 << (nes_fsm.state / 2);
		}
	}
	nes_fsm.state++;
	if (nes_fsm.state == 16) {
		if (nes_fsm.btn.p1 != nes_fsm.btn.p1_old || nes_fsm.btn.p2 != nes_fsm.btn.p2_old) {
			xQueueSendFromISR(nes_queue, &nes_fsm, NULL);
			nes_fsm.btn.p1_old = nes_fsm.btn.p1;
			nes_fsm.btn.p2_old = nes_fsm.btn.p2;
		}
		nes_fsm.state = 0;
		timer_disable_counter(TIM4);
		timer_set_counter(TIM4, 0);
	}
}

void tim4_isr() {
	if (timer_get_flag(TIM4, TIM_SR_UIF)) {
		timer_clear_flag(TIM4, TIM_SR_UIF);
		nes_clk_tick();
	}
}

static enum usbd_request_return_codes hid_control_request(
	usbd_device *device, struct usb_setup_data *req, uint8_t **buf, uint16_t *len,
	void (**complete)(usbd_device *, struct usb_setup_data *)) {
	if ((req->bmRequestType != 0x81) || (req->bRequest != USB_REQ_GET_DESCRIPTOR) || (req->wValue != 0x2200))
		return USBD_REQ_NOTSUPP;

	// Handle the HID report descriptor
	*buf = (uint8_t *)hidReportDescriptor;
	*len = sizeof(hidReportDescriptor);

	return USBD_REQ_HANDLED;
}

static void usb_set_config_callback(usbd_device *device, uint16_t value) {
	usbd_ep_setup(device, 0x81, USB_ENDPOINT_ATTR_INTERRUPT, sizeof(usb_hid_kbd_report_t), NULL);
	usbd_register_control_callback(
		device, USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE, USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
		hid_control_request);
	timer_enable_counter(TIM3);
}

static void configure_usb_keyboard(void) {
	rcc_periph_clock_enable(RCC_USB);
	rcc_periph_clock_enable(RCC_GPIOA);
	uint8_t nstrings = sizeof(usb_strings) / sizeof(usb_strings[0]);
	uint16_t bufsize = sizeof(usb_ctl_buf);
	usb_device = usbd_init(
		&st_usbfs_v1_usb_driver, &usb_device_desc, &usb_conf_desc, usb_strings, nstrings, usb_ctl_buf, bufsize);
	usbd_register_set_config_callback(usb_device, usb_set_config_callback);
}

static void configure_timers(void) {
	rcc_periph_clock_enable(RCC_TIM2);
	timer_set_prescaler(TIM2, rcc_get_timer_clk_freq(TIM2) / 1000 / 50 - 1);
	timer_set_period(TIM2, 1000 - 1);
	timer_set_oc_mode(TIM2, TIM_OC4, TIM_OCM_PWM1);
	timer_set_oc_polarity_low(TIM2, TIM_OC4);
	timer_set_oc_value(TIM2, TIM_OC4, 0);
	timer_enable_oc_output(TIM2, TIM_OC4);
	timer_enable_counter(TIM2);

	nvic_enable_irq(NVIC_TIM4_IRQ);
	nvic_set_priority(NVIC_TIM4_IRQ, configMAX_SYSCALL_INTERRUPT_PRIORITY);

	uint32_t clk_half_period_μs = 6, poll_freq_hz = 50;

	rcc_periph_clock_enable(RCC_TIM4);
	timer_set_prescaler(TIM4, rcc_get_timer_clk_freq(TIM4) / 1e6 - 1);
	timer_set_period(TIM4, clk_half_period_μs - 1);
	timer_enable_irq(TIM4, TIM_DIER_UIE);
	timer_slave_set_trigger(TIM4, TIM_SMCR_TS_ITR2);
	timer_slave_set_mode(TIM4, TIM_SMCR_SMS_TM);

	rcc_periph_clock_enable(RCC_TIM3);
	timer_set_prescaler(TIM3, rcc_get_timer_clk_freq(TIM3) / 1e3 / 2 - 1);
	timer_set_period(TIM3, (1000 / poll_freq_hz) * 2 - 1);
	timer_set_master_mode(TIM3, TIM_CR2_MMS_UPDATE);
}

static void configure_gpio(void) {
	uint32_t rcc[] = {NES_GPIO_RCC};
	for (uint32_t i = 0; i < sizeof(rcc) / sizeof(rcc[0]); i++) {
		rcc_periph_clock_enable(rcc[i]);
	}

	gpio_set_mode(NES_GPIO_LATCH_PORT, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, NES_GPIO_LATCH_PIN);
	gpio_clear(NES_GPIO_LATCH_PORT, NES_GPIO_LATCH_PIN);

	gpio_set_mode(NES_GPIO_CLK_PORT, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, NES_GPIO_CLK_PIN);
	gpio_clear(NES_GPIO_CLK_PORT, NES_GPIO_CLK_PIN);

	gpio_set_mode(NES_GPIO_MISO_1_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, NES_GPIO_MISO_1_PIN);
	gpio_set(NES_GPIO_MISO_1_PORT, NES_GPIO_MISO_1_PIN); // pull miso high

	gpio_set_mode(NES_GPIO_MISO_2_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, NES_GPIO_MISO_2_PIN);
	gpio_set(NES_GPIO_MISO_2_PORT, NES_GPIO_MISO_2_PIN); // pull miso high

	// Controller keypress indicator
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_AFIO);
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO11);
	gpio_set(GPIOB, GPIO11);
	// Remap TIM2_CH4 to on-board LED (PB11)
	AFIO_MAPR |= AFIO_MAPR_TIM2_REMAP_PARTIAL_REMAP2;
}

static void event_to_report(
	usb_hid_kbd_report_t *report, uint8_t *buttons, uint8_t *old_buttons, uint16_t *keymap, bool *report_changed) {
	uint8_t *report_keys = &report->key_1;
	for (uint8_t i = 0; i < 8; i++) {
		if ((*buttons >> i & 1) && !(*old_buttons >> i & 1)) {
			if (keymap[i] & IS_MOD_MASK) {
				report->modifier |= keymap[i] & 0xff;
			} else {
				for (uint8_t j = 0; j < 6; j++) {
					if (report_keys[j] == 0) {
						report_keys[j] = keymap[i];
						*report_changed = true;
						break;
					}
				}
			}
		}
		if (!(*buttons >> i & 1) && (*old_buttons >> i & 1)) {
			if (keymap[i] & IS_MOD_MASK) {
				report->modifier &= ~(keymap[i] & 0xff);
			} else {
				for (uint8_t j = 0; j < 6; j++) {
					if (report_keys[j] == keymap[i]) {
						report_keys[j] = 0;
						*report_changed = true;
						break;
					}
				}
			}
		}
	}
}

static void nes_task_fn(void *arg) {
	nes_button_state_t btn_event = {0};
	for (;;) {
		if (!xQueueReceive(nes_queue, &btn_event, portMAX_DELAY))
			continue;

		if (btn_event.p1 && btn_event.p2) {
			timer_set_oc_value(TIM2, TIM_OC4, 100);
		} else if (btn_event.p1) {
			timer_set_oc_value(TIM2, TIM_OC4, 50);
		} else if (btn_event.p2) {
			timer_set_oc_value(TIM2, TIM_OC4, 10);
		} else {
			timer_set_oc_value(TIM2, TIM_OC4, 0);
		}

		bool report_changed = false;
		if (btn_event.p1 != btn_event.p1_old) {
			event_to_report(&report, &btn_event.p1, &btn_event.p1_old, (uint16_t *)p1_keymap, &report_changed);
		}
		if (btn_event.p2 != btn_event.p2_old) {
			event_to_report(&report, &btn_event.p2, &btn_event.p2_old, (uint16_t *)p2_keymap, &report_changed);
		}
		if (report_changed) {
			usbd_ep_write_packet(usb_device, 0x81, &report, sizeof(report));
		}
	}
}

static void usb_task_fn(void *arg) {
	for (;;) {
		usbd_poll(usb_device);
	}
}

int main(void) {
	wait_itm_available();
	rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);

	nes_queue = xQueueCreate(10, sizeof(nes_button_state_t));
	configASSERT(nes_queue != NULL);

	configure_gpio();
	configure_timers();
	configure_usb_keyboard();

	xTaskCreate(nes_task_fn, "nes", 1024, NULL, 2, NULL);
	xTaskCreate(usb_task_fn, "usb", 1024, NULL, 1, NULL);

	vTaskStartScheduler();
	for (;;) {
	}
	return 0;
}
