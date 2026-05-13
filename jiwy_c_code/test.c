// #include <error.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdbool.h>

#include "soc_system.h"

uint8_t* jiwy_map = NULL;

void set_pwm(uint8_t yaw_duty_cycle, bool yaw_direction, bool yaw_enable, uint8_t pitch_duty_cycle, bool pitch_direction, bool pitch_enable, bool yaw_reset, bool pitch_reset) {
	*((uint32_t *)jiwy_map) = yaw_duty_cycle | yaw_enable << 8 | yaw_direction << 9
		| pitch_duty_cycle << 10 | pitch_enable << 18 | pitch_direction << 19
		| yaw_reset << 20 | pitch_reset << 21;
}

void get_encoders(uint16_t* yaw_encoder, uint16_t* pitch_encoder) {
	uint32_t encoder_values = *((uint32_t *)(jiwy_map + 4));
	*yaw_encoder = encoder_values & 0xFFFF;
	*pitch_encoder = (encoder_values >> 16) & 0xFFFF;
}

int main(int argc, char** argv) {

	if (argc != 7) {
		fprintf(stderr, "Usage: %s <duty_cycle:0-255> <direction:0-1> <enable:0-1>\n", argv[0]);
		return -1;
	}

	uint32_t yaw_duty_cycle = atoi(argv[1]);
	uint8_t yaw_direction = atoi(argv[2]);
	uint8_t yaw_enable = atoi(argv[3]);
	uint32_t pitch_duty_cycle = atoi(argv[4]);
	uint8_t pitch_direction = atoi(argv[5]);
	uint8_t pitch_enable = atoi(argv[6]);

	if (yaw_duty_cycle < 0 || yaw_duty_cycle > 255) {
		fprintf(stderr, "Invalid yaw duty cycle. Please enter a value between 0 and 255.\n");
		return -1;
	}

	if (yaw_direction < 0 || yaw_direction > 1) {
		fprintf(stderr, "Invalid yaw direction. Please enter a value of 0 or 1.\n");
		return -1;
	}

	if (yaw_enable < 0 || yaw_enable > 1) {
		fprintf(stderr, "Invalid yaw enable. Please enter a value of 0 or 1.\n");
		return -1;
	}

	if (pitch_duty_cycle < 0 || pitch_duty_cycle > 255) {
		fprintf(stderr, "Invalid pitch duty cycle. Please enter a value between 0 and 255.\n");
		return -1;
	}

	if (pitch_direction < 0 || pitch_direction > 1) {
		fprintf(stderr, "Invalid pitch direction. Please enter a value of 0 or 1.\n");
		return -1;
	}

	if (pitch_enable < 0 || pitch_enable > 1) {
		fprintf(stderr, "Invalid pitch enable. Please enter a value of 0 or 1.\n");
		return -1;
	}

	int fd = 0;


	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) {
		perror("Couldn't open /dev/mem\n");
		return -1;
	}

	jiwy_map = (uint8_t*)mmap(NULL, HPS_0_ARM_A9_0_JIWY_0_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, HPS_0_ARM_A9_0_JIWY_0_BASE);
	if (jiwy_map == MAP_FAILED) {
		perror("Couldn't map JIWY.");
		close(fd);
		return -1;
	}

	set_pwm(0,0,0,0,0,0,true,true);
	set_pwm(yaw_duty_cycle, yaw_direction, yaw_enable, pitch_duty_cycle, pitch_direction, pitch_enable, false, false);


	while(1) {
		printf("Encoder value: %u\n", *((uint32_t *)jiwy_map));
		sleep(1);
	}

	close(fd);
	return 0;
}
