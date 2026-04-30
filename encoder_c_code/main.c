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
	// uint8_t* esl_demo_map = NULL;
	// esl_demo_map = (uint8_t*)mmap(NULL, HPS_0_ARM_A9_0_ESL_BUS_DEMO_0_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, HPS_0_ARM_A9_0_ESL_BUS_DEMO_0_BASE);
	// if (esl_demo_map == MAP_FAILED) {
	// 	perror("Couldn't map bridge.");
	// 	close(fd);
	// 	return -1;
	// }

	uint8_t* encoder_map = NULL;
	encoder_map = (uint8_t*)mmap(NULL, HPS_0_ARM_A9_0_ENCODER_IP_0_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, HPS_0_ARM_A9_0_ENCODER_IP_0_BASE);
	if (encoder_map == MAP_FAILED) {
		perror("Couldn't map encoder.");
		close(fd);
		return -1;
	}

	// *((uint32_t *)esl_demo_map) = 1 << 31 | 0x08;

	while(1) {
		printf("Encoder value: %u\n", *((uint32_t *)encoder_map));
		sleep(1);
	}

	close(fd);
	return 0;
}
