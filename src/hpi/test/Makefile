CC=arm-linux-gcc
STRIP=arm-linux-strip
TARGET=hpi_test
SRCS=hpi_test.c
CFLAGS=-mapcs -mtune=arm920t -mcpu=arm920t -D_GNU_SOURCE -g -o3 -Wall -funroll-loops -I./ -lc -lpthread

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -rf *.o $(TARGET)

