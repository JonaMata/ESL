// #include <error.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdbool.h>

#define SPEED 1000

// Function to generate the 32-bit PWM control string based on the input parameters
uint32_t generate_PWM_string(uint8_t yaw_duty_cycle, bool yaw_direction, bool yaw_enable, uint8_t pitch_duty_cycle, bool pitch_direction, bool pitch_enable, bool yaw_reset, bool pitch_reset) {
	return yaw_duty_cycle | yaw_enable << 8 | yaw_direction << 9
		| pitch_duty_cycle << 10 | pitch_enable << 18 | pitch_direction << 19
		| yaw_reset << 20 | pitch_reset << 21;
}

// Function to extract and print the yaw and pitch encoder values from the received buffer
void print_encoders(char *RXBuf) {
  int yaw_enc = RXBuf[0] & 0x65535; // mask 16 bits
  int pitch_enc = (RXBuf[0] >> 16) & 0x65535; // move 16 bits and mask them.
	printf("Yaw Encoder: %u, Pitch Encoder: %u\n", yaw_enc, pitch_enc);
}

double time_time(void) {
  struct timeval tv;
  double t;

  gettimeofday(&tv, 0);

  t = (double)tv.tv_sec + ((double)tv.tv_usec / 1E6);

  return t;
}

int spiOpen(unsigned spiChan, unsigned spiBaud, unsigned spiFlags) {
  int i, fd;
  char spiMode;
  char spiBits = 8;
  char dev[32];

  spiMode = spiFlags & 3;
  spiBits = 8;

  sprintf(dev, "/dev/spidev0.%d", spiChan);

  if ((fd = open(dev, O_RDWR)) < 0) {
    return -1;
  }

  if (ioctl(fd, SPI_IOC_WR_MODE, &spiMode) < 0) {
    close(fd);
    return -2;
  }

  if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &spiBits) < 0) {
    close(fd);
    return -3;
  }

  if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &spiBaud) < 0) {
    close(fd);
    return -4;
  }

  return fd;
}

int spiClose(int fd) { return close(fd); }

int spiRead(int fd, unsigned speed, char *buf, unsigned count) {
  int err;
  struct spi_ioc_transfer spi;

  memset(&spi, 0, sizeof(spi));

  spi.tx_buf = (unsigned)NULL;
  spi.rx_buf = (unsigned)buf;
  spi.len = count;
  spi.speed_hz = speed;
  spi.delay_usecs = 0;
  spi.bits_per_word = 8;
  spi.cs_change = 0;

  err = ioctl(fd, SPI_IOC_MESSAGE(1), &spi);

  return err;
}

int spiWrite(int fd, unsigned speed, char *buf, unsigned count) {
  int err;
  struct spi_ioc_transfer spi;

  memset(&spi, 0, sizeof(spi));

  spi.tx_buf = (unsigned)buf;
  spi.rx_buf = (unsigned)NULL;
  spi.len = count;
  spi.speed_hz = speed;
  spi.delay_usecs = 0;
  spi.bits_per_word = 8;
  spi.cs_change = 0;

  err = ioctl(fd, SPI_IOC_MESSAGE(1), &spi);

  return err;
}

int spiXfer(int fd, unsigned speed, char *txBuf, char *rxBuf, unsigned count) {
  int err;
  struct spi_ioc_transfer spi;

  memset(&spi, 0, sizeof(spi));

  spi.tx_buf = (unsigned long)txBuf;
  spi.rx_buf = (unsigned long)rxBuf;
  spi.len = count;
  spi.speed_hz = speed;
  spi.delay_usecs = 0;
  spi.bits_per_word = 8;
  spi.cs_change = 0;

  err = ioctl(fd, SPI_IOC_MESSAGE(1), &spi);

  return err;
}

#define MAX_SPI_BUFSIZ 8192

char RXBuf[MAX_SPI_BUFSIZ];
char TXBuf[MAX_SPI_BUFSIZ];

int main(int argc, char** argv) {

	if (argc != 7) {
		fprintf(stderr, "Usage: %s <duty_cycle:0-255> <direction:0-1> <enable:0-1>\n", argv[0]);
		return -1;
	}

  // Parse and validate command-line arguments
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

	int i, fd;

	fd = spiOpen(1, SPEED, 0);
	if (fd < 0) {
		printf("Couldn't open SPI device: %d\n", fd);
		return -1;
	}

	// set_pwm(0,0,0,0,0,0,true,true);
	// set_pwm(yaw_duty_cycle, yaw_direction, yaw_enable, pitch_duty_cycle, pitch_direction, pitch_enable, false, false);

	uint16_t yaw_enc = 0;
	uint16_t pitch_enc = 0;

	while(1) {
		TXBuf[0] = generate_PWM_string(yaw_duty_cycle, yaw_direction, yaw_enable, pitch_duty_cycle, pitch_direction, pitch_enable, false, false);
		printf("Sent PWM control string: 0x%08X\n", TXBuf[0]);
    spiXfer(fd, SPEED, TXBuf, RXBuf, 1);
		print_encoders(RXBuf);
		sleep(1);
	}

	close(fd);
	return 0;
}
