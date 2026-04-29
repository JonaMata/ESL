// main.c
// Contains a simple SPI generator, as it simply just writes increasing numbers to the PICO line
// each transmit sequence.
// Heavily inspired by https://forums.raspberrypi.com/viewtopic.php?t=304828#p1856388
// gcc -o main main.c

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

#define LOOPS 10000
#define SPEED 1000
#define BYTES 3

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

int bytes = BYTES;
int speed = SPEED;
int loops = LOOPS;

int main(int argc, char *argv[]) {
  int i, fd;
  double start, diff, sps;

  if (argc > 1)
    bytes = atoi(argv[1]);
  else
    printf("./spi-driver-speed [bytes [bps [loops] ] ]\n\n");

  if ((bytes < 1) || (bytes > MAX_SPI_BUFSIZ))
    bytes = BYTES;

  if (argc > 2)
    speed = atoi(argv[2]);
  if ((speed < 32000) || (speed > 250000000))
    speed = SPEED;

  if (argc > 3)
    loops = atoi(argv[3]);
  if ((loops < 1) || (loops > 10000000))
    loops = LOOPS;

  fd = spiOpen(1, speed, 0);

  start = time_time();

  if (fd < 0)
    return 1;

  for (i = 0; i < loops; i++) {
    if (i % 2) {
      TXBuf[0] = 255;
    } else {
      TXBuf[0] = 0;
    }
    spiXfer(fd, speed, TXBuf, RXBuf, bytes);
  }

  diff = time_time() - start;

  close(fd);

  printf("sps=%.1f: %d bytes @ %d bps (loops=%d time=%.1f)\n",
         (double)loops / diff, bytes, speed, loops, diff);
}
