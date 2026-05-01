// #include <error.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "soc_system.h"


int main(int argc, char** argv) {
	int fd = 0;

	if (argc != 4) {
		fprintf(stderr, "Usage: %s <duty_cycle:0-255> <direction:0-1> <enable:0-1>\n", argv[0]);
		return -1;
	}

	uint32_t duty_cycle = atoi(argv[1]);
	uint8_t direction = atoi(argv[2]);
	uint8_t enable = atoi(argv[3]);

	if (duty_cycle < 0 || duty_cycle > 255) {
		fprintf(stderr, "Invalid duty cycle. Please enter a value between 0 and 255.\n");
		return -1;
	}

	if (direction < 0 || direction > 1) {
		fprintf(stderr, "Invalid direction. Please enter a value of 0 or 1.\n");
		return -1;
	}

	if (enable < 0 || enable > 1) {
		fprintf(stderr, "Invalid enable. Please enter a value of 0 or 1.\n");
		return -1;
	}

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

	*((uint32_t *)pwm_map) = enable << 8 | direction << 9 | duty_cycle;

	close(fd);
	return 0;
}
