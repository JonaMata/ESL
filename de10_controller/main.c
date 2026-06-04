// #include <error.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>

#include "soc_system.h"

#include "xxsubmod.h"
#include "yysubmod.h"

XXDouble yaw_u [2 + 1];
XXDouble yaw_y [2 + 1];
YYDouble pitch_u [3 + 1];
YYDouble pitch_y [1 + 1];

uint8_t* jiwy_map = NULL;

void set_pwm(uint8_t yaw_duty_cycle, bool yaw_direction, bool yaw_enable, uint8_t pitch_duty_cycle, bool pitch_direction, bool pitch_enable, bool yaw_reset, bool pitch_reset) {
    *((uint32_t *)jiwy_map) = yaw_duty_cycle | yaw_enable << 8 | yaw_direction << 9
        | pitch_duty_cycle << 10 | pitch_enable << 18 | pitch_direction << 19
        | yaw_reset << 20 | pitch_reset << 21;
}

void get_encoders(uint16_t* yaw_encoder, uint16_t* pitch_encoder) {
    uint32_t encoder_values = *((uint32_t *)(jiwy_map));
    *yaw_encoder = encoder_values & 0xFFFF;
    *pitch_encoder = (encoder_values >> 16) & 0xFFFF;
}

void home_yaw() {
    uint16_t prev_enc = 0;
    uint16_t enc = 0;
    uint16_t enc_no = 0;
    get_encoders(&enc, &enc_no);
    set_pwm(20, true, true, 0, false, false, false, false);
    sleep(1);
    do {
        printf("Homing... Current encoder: %d\n", enc);
        sleep(1);
        prev_enc = enc;
        get_encoders(&enc, &enc_no);
    } while (prev_enc-enc != 0);
    set_pwm(20, true, true, 0, false, false, true, false);
    set_pwm(0, false, false, 0, false, false, false, false);
}

void home_pitch() {
    uint16_t prev_enc = 0;
    uint16_t enc = 0;
    uint16_t enc_no = 0;
    get_encoders(&enc, &enc_no);
    set_pwm(0, false, false, 20, true, true, false, false);
    sleep(1);
    do {
        printf("Homing... Current encoder: %d\n", enc);
        sleep(1);
        prev_enc = enc;
        get_encoders(&enc_no, &enc);
    } while (prev_enc-enc != 0);
    set_pwm(0, false, false, 20, true, true, false, true);
    set_pwm(0, false, false, 0, false, false, false, false);
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

    timer_t timer_id;
    struct sigevent sev;
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGUSR1;
    sev.sigev_value.sival_ptr = &timer_id;


    struct itimerspec its;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 1e6;
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 1e6;

    timer_create(CLOCK_MONOTONIC, &sev, &timer_id);

    // create a sigset for sigwait to wait for SIGUSR1
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR1);
    sigprocmask(SIG_BLOCK, &sigset, NULL);

    timer_settime(timer_id, 0, &its, NULL);

    printf("Start homing...\n");
    home_yaw();
    home_pitch();
    printf("Homing complete.\n");


    /* Initialize the inputs and outputs with correct initial values */
    yaw_u[0] = 0.0;		/* in */
    yaw_u[1] = 0.0;		/* position */
    pitch_u[0] = 0.0;   /* corr */
    pitch_u[1] = 0.0;	/* in */
    pitch_u[2] = 0.0;	/* position */

    yaw_y[0] = 0.0;		/* corr */
    yaw_y[1] = 0.0;		/* out */
    pitch_y[0] = 0.0;	/* out */

	XXInitializeSubmodel (yaw_u, yaw_y, xx_time);
    YYInitializeSubmodel (pitch_u, pitch_y, yy_time);

    uint16_t yaw_encoder;
    uint16_t pitch_encoder;
    uint16_t prev_yaw_encoder = 0;
    uint16_t prev_pitch_encoder = 0;
    int raw_yaw_position = 0;
    int raw_pitch_position = 0;

    printf("Starting control loop...\n");
    int counter = 1200;
    bool count_dir = true;
    int setpoint = 5000;
    while (1) {
        int sig;
        sigwait(&sigset, &sig);
        printf("Timer tick\n");
        get_encoders(&yaw_encoder, &pitch_encoder);

        int yaw_diff = yaw_encoder - prev_yaw_encoder;
        prev_yaw_encoder = yaw_encoder;
        if (abs(yaw_diff) > 32768) {
            if (yaw_diff > 0) {
                yaw_diff -= 65536;
            } else {
                yaw_diff += 65536;
            }
        }
        raw_yaw_position += yaw_diff;
        XXDouble yaw_position = (XXDouble)raw_yaw_position / 21708.8 * 2 * 3.1415926;
        yaw_u[0] = (double)setpoint / 21708.8 * 2 * 3.1415926;
        yaw_u[1] = yaw_position;
        XXCalculateSubmodel (yaw_u, yaw_y, xx_time);
        uint8_t yaw_duty_cycle = (uint8_t)(abs(yaw_y[1] * 255));
        if (yaw_duty_cycle > 64) yaw_duty_cycle = 64;
        bool yaw_direction = yaw_y[1] < 0;

        int pitch_diff = pitch_encoder - prev_pitch_encoder;
        prev_pitch_encoder = pitch_encoder;
        if (abs(pitch_diff) > 32768) {
            if (pitch_diff > 0) {
                pitch_diff -= 65536;
            } else {
                pitch_diff += 65536;
            }
        }
        raw_pitch_position += pitch_diff;
        YYDouble pitch_position = (YYDouble)raw_pitch_position / 21708.8 * 2 * 3.1415926;
        pitch_u[1] = (double)setpoint / 21708.8 * 2 * 3.1415926;
        pitch_u[2] = pitch_position;
        YYCalculateSubmodel (pitch_u, pitch_y, yy_time);
        uint8_t pitch_duty_cycle = (uint8_t)(abs(pitch_y[0] * 255));
        if (pitch_duty_cycle > 64) pitch_duty_cycle = 64;
        bool pitch_direction = pitch_y[0] < 0;

        set_pwm(yaw_duty_cycle, yaw_direction, true, pitch_duty_cycle, pitch_direction, true, false, false);
        // if (count_dir) {
        //     counter++;
        //     if (counter >= 2000) {
        //         count_dir = false;
        //         setpoint = 2000;
        //     }
        // } else {
        //     counter--;
        //     if (counter <= 500) {
        //         count_dir = true;
        //         setpoint = 500;
        //     }
        // }
    }


	XXTerminateSubmodel (yaw_u, yaw_y, xx_time);
    YYTerminateSubmodel (pitch_u, pitch_y, yy_time);


	close(fd);
	return 0;
}
