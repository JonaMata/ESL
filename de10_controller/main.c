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

// #include "xxsubmod.h"

// XXDouble u [2 + 1];
// XXDouble y [2 + 1];

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
        set_pwm(40, true, true, 0, false, false, false, false);
        sleep(0.01);
        prev_enc = enc;
        get_encoders(&enc, NULL);
    } while (prev_enc-enc != 0);
    set_pwm(0, false, false, 0, false, false, true, false);
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

    // timer_t timer_id;
    // struct sigevent sev;
    // sev.sigev_notify = SIGEV_SIGNAL;
    // sev.sigev_signo = SIGUSR1;
    // sev.sigev_value.sival_ptr = &timer_id;


    // struct itimerspec its;
    // its.it_interval.tv_sec = 0;
    // its.it_interval.tv_nsec = 1e6;
    // its.it_value.tv_sec = 0;
    // its.it_value.tv_nsec = 1e6;

    // timer_create(CLOCK_MONOTONIC, &sev, &timer_id);

    // // create a sigset for sigwait to wait for SIGUSR1
    // sigset_t sigset;
    // sigemptyset(&sigset);
    // sigaddset(&sigset, SIGUSR1);
    // sigprocmask(SIG_BLOCK, &sigset, NULL);

    // timer_settime(timer_id, 0, &its, NULL);


    home_yaw();


    /* Initialize the inputs and outputs with correct initial values */
    // u[0] = 0.0;		/* in */
    // u[1] = 0.0;		/* position */

    // y[0] = 0.0;		/* corr */
    // y[1] = 0.0;		/* out */

	// XXInitializeSubmodel (u, y, xx_time);

    // uint16_t yaw_encoder;

    // while (1) {
    //     int sig;
    //     sigwait(&sigset, &sig);
    //     get_encoders(&yaw_encoder, NULL);
    //     XXDouble position = (XXDouble)yaw_encoder / 5000.0 * 2 * 3.1415926;

    //     u[1] = position;
    //     XXCalculateSubmodel (u, y, xx_time);
    //     uint8_t duty_cycle = (uint8_t)(abs(y[1] * 255));
    //     bool direction = y[1] < 0;
    //     set_pwm(duty_cycle, direction, true, 0, false, false, false, false);
    // }


	// XXTerminateSubmodel (u, y, xx_time);


	close(fd);
	return 0;
}
