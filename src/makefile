CC=arm-linux-gcc
STRIP=arm-linux-strip
#SRCS=$(wildcard *.c)
TARGET=cofdm
SRCS=cofdm.c
#CFLAGS=-ggdb -W -Wall -Wno-unused -D_GNU_SOURCE -I./ -lc -lpthread
#CFLAGS=-mapcs -mtune=arm920t -mcpu=arm920t -O3 -W -Wall -Wno-unused -D_GNU_SOURCE -I./ -lc -lpthread
CFLAGS=-mtune=arm920t -mcpu=arm920t -O3 -W -Wall -Wno-unused -D_GNU_SOURCE -I./ -lc -lpthread
#CFLAGS=-mtune=arm920t -mcpu=arm920t -ggdb -W -Wall -Wno-unused -D_GNU_SOURCE -I./ -lc -lpthread

all:
	$(CC) -o $(TARGET) $(SRCS) $(CFLAGS)
	$(STRIP) $(TARGET)

clean:
	rm -rf *.o $(TARGET) $(ETHTARGET)

