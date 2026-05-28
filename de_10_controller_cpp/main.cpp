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

#include "PositionControllerPan.h"
#include "PositionControllerTilt.h"


XXDouble u_yaw [2 + 1];
XXDouble y_yaw [2 + 1];
XXDouble u_pitch [2 + 1];
XXDouble y_pitch [2 + 1];

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
    set_pwm(40, true, true, 0, false, false, false, false);
    sleep(1);
    do {
        printf("Homing... Current encoder: %d\n", enc);
        sleep(1);
        prev_enc = enc;
        get_encoders(&enc, &enc_no);
    } while (prev_enc-enc != 0);
    set_pwm(0, false, false, 0, false, false, true, false);
    set_pwm(0, false, false, 0, false, false, false, false);
}

void home_pitch() {
    uint16_t prev_enc = 0;
    uint16_t enc = 0;
    uint16_t enc_no = 0;
    get_encoders(&enc_no, &enc);
    set_pwm(0, false, false, 40, true, true, false, false);
    sleep(1);
    do {
        printf("Homing... Current encoder: %d\n", enc);
        sleep(1);
        prev_enc = enc;
        get_encoders(&enc_no, &enc);
    } while (prev_enc-enc != 0);
    set_pwm(0, false, false, 0, false, false, false, true);
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
    printf("Homing complete.\n");


    /* Initialize the inputs and outputs with correct initial values */
    u_yaw[0] = 0.0;		/* in */
    u_yaw[1] = 0.0;		/* position */

    y_yaw[0] = 0.0;		/* corr */
    y_yaw[1] = 0.0;		/* out */

    u_pitch[0] = 0.0;		/* in */
    u_pitch[1] = 0.0;		/* position */

    y_pitch[0] = 0.0;		/* corr */
    y_pitch[1] = 0.0;		/* out */


	PositionControllerPan yawController;
	PositionControllerTilt pitchController;

	yawController.Initialize(u_yaw, y_yaw, 0.0);
	pitchController.Initialize(u_pitch, y_pitch, 0.0);

    uint16_t yaw_encoder;
    uint16_t pitch_encoder;
    uint16_t prev_yaw_encoder = 0;
    uint16_t prev_pitch_encoder = 0;
    int raw_yaw = 0;
    int raw_pitch = 0;

    printf("Starting control loop...\n");
    int counter = 1200;
    bool count_dir = true;
    int setpoint = 5000;
    while (1) {
        int sig;
        sigwait(&sigset, &sig);
        printf("Timer tick\n");
        get_encoders(&yaw_encoder, &pitch_encoder);
        int diff_yaw = yaw_encoder - prev_yaw_encoder;
        prev_yaw_encoder = yaw_encoder;
        if (abs(diff_yaw) > 32768) {
            if (diff_yaw > 0) {
                diff_yaw -= 65536;
            } else {
                diff_yaw += 65536;
            }
        }

        int diff_pitch = pitch_encoder - prev_pitch_encoder;
        prev_pitch_encoder = pitch_encoder;
        if (abs(diff_pitch) > 32768) {
            if (diff_pitch > 0) {
                diff_pitch -= 65536;
            } else {
                diff_pitch += 65536;
            }
        }

        raw_yaw += diff_yaw;
        raw_pitch += diff_pitch;
        XXDouble yaw_position = (XXDouble)raw_yaw / 21708.8 * 2 * 3.1415926;
        XXDouble pitch_position = (XXDouble)raw_pitch / 21708.8 * 2 * 3.1415926;

        u_yaw[0] = (double)setpoint / 21708.8 * 2 * 3.1415926;
        u_yaw[1] = yaw_position;

        u_pitch[0] = (double)setpoint / 21708.8 * 2 * 3.1415926;
        u_pitch[1] = pitch_position;

        yawController.Calculate(u_yaw, y_yaw);
        pitchController.Calculate(u_pitch, y_pitch);
        uint8_t yaw_duty_cycle = (uint8_t)(abs(y_yaw[1] * 255));
        uint8_t pitch_duty_cycle = (uint8_t)(abs(y_pitch[1] * 255));
        // if (duty_cycle > 128) duty_cycle = 128;
        bool yaw_direction = y_yaw[1] < 0;
        bool pitch_direction = y_pitch[1] < 0;
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


	
	yawController.Terminate (u, y);
    pitchController.Terminate (u, y);


	close(fd);
	return 0;
}
