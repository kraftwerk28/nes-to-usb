#include <stdint.h>

#include "libopencm3/cm3/itm.h"
#include "libopencm3/stm32/rcc.h"

#include "utils.h"

uint32_t rand_seed = 1234567890;
static uint32_t m = (1 << 31), a = 1103515245, c = 12345;

uint32_t rand() {
	rand_seed = (a * rand_seed + c) % m;
	return rand_seed;
}

void wait_itm_available() {
#if ENABLE_ITM
	for (uint32_t i = 0; i < 800000; i++) {
	}
	while ((ITM_TCR & ITM_TCR_ITMENA) == 0) {
	}
#endif
}

static uint8_t itm_send_char(uint32_t channel, uint8_t ch) {
	while ((ITM_STIM8(0) & ITM_STIM_FIFOREADY) == 0) {
	}
	ITM_STIM8(0) = ch;
	return ch;
}

int _write(int fd, char *ptr, int len) {
	int i = 0;
#if ENABLE_ITM
	for (; i < len && ptr[i]; i++) {
		itm_send_char(fd, ptr[i]);
		if (ptr[i] == '\n') {
			itm_send_char(fd, '\r');
		}
	}
#endif
	return i;
}

// Suppress linker warnings
void _close(void) {}
void _fstat(void) {}
void _getpid(void) {}
void _isatty(void) {}
void _kill(void) {}
void _lseek(void) {}
void _read(void) {}
