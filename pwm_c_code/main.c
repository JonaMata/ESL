#include <error.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "soc_system.h"


int main(int argc, char** argv) {
	int fd = 0;

	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) {
		perror("Couldn't open /dev/mem\n");
		return -1;
	}

	uint8_t* pwm_map = NULL;
	pwm_map = (uint8_t*)mmap(NULL, HPS_0_ARM_A9_0_PWM_IP_0_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, HPS_0_ARM_A9_0_PWM_IP_0_BASE);
	if (pwm_map == MAP_FAILED) {
		perror("Couldn't map PWM.");
		close(fd);
		return -1;
	}

	*((uint32_t *)pwm_map) = 1 << 8 | 1 << 9 | 127;

	close(fd);
	return 0;
}
