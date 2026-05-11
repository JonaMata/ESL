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

void home_yaw() {
	uint16_t prev_enc = 0;
	uint16_t enc = 0;
	get_encoders(&enc, NULL);
	do {
		set_pwm(20, false, true, 0, false, false, false, false);
		sleep(0.01);
		prev_enc = enc;
		get_encoders(&enc, NULL);
	} while (prev_enc-enc != 0);
	set_pwm(0, false, false, 0, false, false, true, false);
	set_pwm(0, false, false, 0, false, false, false, false);
}

void home_pitch() {
	uint16_t prev_enc = 0;
	uint16_t enc = 0;
	get_encoders(NULL, &enc);
	do {
		set_pwm(0, false, false, 20, false, true, false, false);
		sleep(0.01);
		prev_enc = enc;
		get_encoders(NULL, &enc);
	} while (prev_enc-enc != 0);
	set_pwm(0, false, false, 0, false, false, false, true);
	set_pwm(0, false, false, 0, false, false, false, false);
}

void dance() {
	uint16_t yaw_enc = 0;
	uint16_t pitch_enc = 0;
	bool yaw_dir = false;
	bool pitch_dir = false;
	while(1) {
		get_encoders(&yaw_enc, &pitch_enc);
		if (yaw_enc > 100) {
			yaw_dir = !yaw_dir;
		} else if (yaw_enc < 10) {
			yaw_dir = !yaw_dir;
		}
		if (pitch_enc > 100) {
			pitch_dir = !pitch_dir;
		} else if (pitch_enc < 10) {
			pitch_dir = !pitch_dir;
		}
		set_pwm(20, yaw_dir, true, 20, pitch_dir, true, false, false);
		sleep(0.01);
	}
}

int main(int argc, char** argv) {
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

	home_yaw();
	home_pitch();

	dance();


	close(fd);
	return 0;
}
