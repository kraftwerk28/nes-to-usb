#ifndef USB_DESCRIPTORS_INCLUDED
#define USB_DESCRIPTORS_INCLUDED

#include "libopencm3/usb/hid.h"
#include "libopencm3/usb/usbd.h"

#define USB_KEYBOARD_ENDPOINT 0x81

typedef struct {
	uint8_t modifier;
	uint8_t : 8;
	uint8_t key_1, key_2, key_3, key_4, key_5, key_6;
} __attribute__((packed)) usb_hid_kbd_report_t;

static const struct usb_device_descriptor usb_device_desc = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = 0,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64,
	.idVendor = 0x1488,
	.idProduct = 0xffff,
	.bcdDevice = 0x0200,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1,
};

const struct usb_endpoint_descriptor hid_endpoint = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_KEYBOARD_ENDPOINT,
	.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
	.wMaxPacketSize = sizeof(usb_hid_kbd_report_t),
	// .bInterval = 0x20,
	.bInterval = 8, // ms
};

static uint8_t KeyboardReportDesc[] = {
	0x05, 0x01,		  // Usage Page (Generic Desktop)
	0x09, 0x06,		  // Usage (Keyboard)
	0xA1, 0x01,		  // Collection (Application)
	0x75, 0x01,		  //     Report Size (1)
	0x95, 0x08,		  //     Report Count (8)
	0x05, 0x07,		  //     Usage Page (Keyboard/Keypad)
	0x19, 0xE0,		  //     Usage Minimum (Keyboard Left Control)
	0x29, 0xE7,		  //     Usage Maximum (Keyboard Right GUI)
	0x15, 0x00,		  //     Logical Minimum (0)
	0x25, 0x01,		  //     Logical Maximum (1)
	0x81, 0x02,		  //     Input (Data,Var,Abs,NWrp,Lin,Pref,NNul,Bit)
	0x95, 0x01,		  //     Report Count (1)
	0x75, 0x08,		  //     Report Size (8)
	0x81, 0x01,		  //     Input (Cnst,Ary,Abs)
	0x95, 0x05,		  //     Report Count (5)
	0x75, 0x01,		  //     Report Size (1)
	0x05, 0x08,		  //     Usage Page (LEDs)
	0x19, 0x01,		  //     Usage Minimum
	0x29, 0x05,		  //     Usage Maximum
	0x91, 0x02,		  //     Output (Data,Var,Abs,NWrp,Lin,Pref,NNul,NVol,Bit)
	0x95, 0x01,		  //     Report Count (1)
	0x75, 0x03,		  //     Report Size (3)
	0x91, 0x01,		  //     Output (Cnst,Ary,Abs,NWrp,Lin,Pref,NNul,NVol,Bit)
	0x95, 0x06,		  //     Report Count (6)
	0x75, 0x08,		  //     Report Size (8)
	0x26, 0xFF, 0x00, //     Logical Maximum (255)
	0x05, 0x07,		  //     Usage Page (Keyboard/Keypad)
	0x19, 0x00,		  //     Usage Minimum
	0x29, 0xFF,		  //     Usage Maximum
	0x81, 0x00,		  //     Input (Data,Ary,Abs)
	0xC0			  // End Collection
};

#define hidReportDescriptor KeyboardReportDesc

static const struct {
	struct usb_hid_descriptor hid_descriptor;
	struct {
		uint8_t bReportDescriptorType;
		uint16_t wDescriptorLength;
	} __attribute__((packed)) hid_report;
} __attribute__((packed)) hid_function = {
	.hid_descriptor =
		{
			.bLength = sizeof(hid_function),
			.bDescriptorType = USB_DT_HID,
			.bcdHID = 0x0100,
			.bCountryCode = 0,
			.bNumDescriptors = 1,
		},
	.hid_report =
		{
			.bReportDescriptorType = USB_DT_REPORT,
			.wDescriptorLength = sizeof(hidReportDescriptor),
		},
};

static const struct usb_interface_descriptor usb_hid_iface_desc = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 1,
	.bInterfaceClass = USB_CLASS_HID,
	.bInterfaceSubClass = 1, /* boot */
	.bInterfaceProtocol = 1, // keyboard
	.iInterface = 0,

	.endpoint = &hid_endpoint,

	.extra = &hid_function,
	.extralen = sizeof(hid_function),
};

static const struct usb_interface usb_iface = {
	.num_altsetting = 1,
	.altsetting = &usb_hid_iface_desc,
};

static const struct usb_config_descriptor usb_conf_desc = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
	.bNumInterfaces = 1,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0xC0,
	.bMaxPower = 0x32,
	.interface = &usb_iface,
};

static const char *usb_strings[] = {
	"Black Sphere Technologies",
	"HID Demo",
	"DEMO",
};

#endif
