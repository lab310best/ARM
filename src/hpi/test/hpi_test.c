#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#include "../hpi.h"

#define BUF_LEN 3240
//#define BUF_LEN 1600
//#define TIMES (1024)
#define TIMES (1024)
#define RCV_FILE "/root/rcv.bin"
#define SIZE_1M 80//80*3240*8/2

static char buf[BUF_LEN*2];

static int g_can_snd = 1, g_can_rcv = 1, hpi_fd, g_snd_num = 0, g_rcv_num = 0;
static void dbg_str(const char* src)
{
	int i = 0;

	while (i < BUF_LEN) {
		printf("0x%2x ", src[i]);
		i++;
		if ((i%16) == 0)
			printf("\n");
	}
}

#define HPI_DBG

static inline void write_hpi(int fd, char* buf, int len)
{
	int i = -1 ,ret = 0;
	do {
		ret = write(fd, buf, len);
		i++;
#ifdef HPI_DBG		
		if (i != 0)
			printf("HPI BUSY (%d)\n", i);
#endif
	} while (ret == -1);
}

static void *rcvthr(void *arg)
{
	char* expect_p = buf;
	char tmp[BUF_LEN];
	int ret, i = 0;

	pthread_detach(pthread_self());

	while(g_can_rcv) {
		ret = read(hpi_fd, tmp, BUF_LEN);
		g_rcv_num++;
		if (ret < 0)
			perror("read failed...");
		else if (strncmp(tmp, expect_p, BUF_LEN) == 0) {
			printf("Verify suceed!\n");
			expect_p++;
			i++;
			if (i == 256) {
				expect_p = buf;
				i = 0;
			}
		} else {
			printf("\n%dth Frame Verify failed!\n\n\nExpect:\n\n", g_rcv_num);
			dbg_str(expect_p);
			printf("\n\nRecv:\n\n");
			dbg_str(tmp);
			raise(SIGINT);
		}
	}

	return NULL;
}

static void hdExit()
{
	printf("ERROR or SIGINT/SIGTERM or STOP received!\n");

	g_can_snd = 0;
	sleep(6);
	g_can_rcv = 0;
	printf("%d snd, %d rcv.\n", g_snd_num, g_rcv_num);
	exit(0);
}

static void initSighandler()
{
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, hdExit);
	signal(SIGTERM, hdExit);
}

int main(int argc, char *argv[])
{
	int i, j, ret, times = TIMES * 3;
	char tmp[BUF_LEN];
	char * cur_p = NULL;
	struct timeval start, end;
	FILE* fp = NULL;
	unsigned short* pshort = NULL;
	pthread_t rcvThrId;
	struct timeval timeout;
	int time_us;

	hpi_fd = open("/dev/hpi", O_RDWR);
	if (hpi_fd < 0) {
		perror("Open device /dev/hpi failed.");
		return 0;
	}

	if (argc < 2) {
		printf("Usage:\n\t0 -> clear_int\t1 -> read print\t2 -> write\t"
				"3 -> write & read\t4 -> read & write\t"
				"5 -> fast write rate\n\t6 -> read file\t"
				"7 -> read rate\t8 -> snd & rcv\n");
		return 0;
	}

	if ((argc == 3)&&(argv[2] != NULL)) {
		time_us = atoi(argv[2]);
	} else
		time_us = 1000;

	switch (*argv[1]) {
		case '0':
			printf("HPI_CLRINT:\n");
			ioctl(hpi_fd, HPI_CLRINT, 0);
			break;

		case '1':
			i = 0, j = 0;
			while(1) {
				memset(buf, 0, sizeof(buf));
				ret = read(hpi_fd, buf, BUF_LEN);
				i++;
				if (i%500 == 0) {
					i = 0;
					printf("%8d blocks with each 500*3240 Byte size received.\n", j++);
				}
				printf("\n\n%dth: recev %d from hpi.\n\n\n", i, ret);
				dbg_str(buf);
			}
			break;

		case '2':
			for (i = 0; i < sizeof(buf); i++)
				buf[i] = i%256;
			printf("data initialized....\n");
			i = 0;
			cur_p = buf;
			while (1) {
				ret = write(hpi_fd, cur_p, BUF_LEN);
				i++;
				cur_p++;
				if (i == 256) {
					i = 0;
					cur_p = buf;
				}
				if (ret < 0)
					perror("write failed...");
				else
					printf("%dth: send %d to hpi.\n", i, ret);
				sleep(1);
			}
			break;

		case '3':
			for (i = 0; i < sizeof(buf); i++)
				//buf[i] = (int)(255.0 * (rand()/(RAND_MAX + 1.0)));
				buf[i] = (i%256);
			i = 0;
			printf("data initialized....\n");
			gettimeofday(&start, NULL);
			while (times--) {
				cur_p = (buf + i%BUF_LEN);

				if (i == 256) {
					i = 0;
					cur_p = buf;
				}

				ret = write(hpi_fd, cur_p, BUF_LEN);
				if (ret < 0)
					perror("write failed...");
				else {
					//printf("send %d to hpi.\n", ret);
					ret = read(hpi_fd, tmp, BUF_LEN);
					if (ret < 0)
						perror("read failed...");
					else {
						if (strncmp(cur_p, tmp, BUF_LEN) == 0) {
							printf("Verify succeed!\n");
							i++;
						}
						else {
							printf("\nVerify failed!\n\n\nSend:\n\n");
							dbg_str(cur_p);
							printf("Recv:\n");
							dbg_str(tmp);
							i++;
						}
					}
				}
				//usleep(3000);
			}
			gettimeofday(&end, NULL);
			printf("Avg ByteRate:%fKB/S\n",
				(((BUF_LEN+0.0)*320.0)/((end.tv_sec - start.tv_sec)+((0.0+end.tv_usec - start.tv_usec)/1000000.0))));
			break;

		case '4':
			while (1) {
				ret = read(hpi_fd, tmp, BUF_LEN);
				write_hpi(hpi_fd, tmp, BUF_LEN);
				printf("read and acked!\n");
			}
			break;

		case '5':
			for (i = 0; i < sizeof(buf); i++)
				buf[i] = i%256;
			printf("data initialized....\n");
			times = 10240;
			gettimeofday(&start, NULL);
			while (times--) {
				ret = write(hpi_fd, buf, BUF_LEN);
			}
			gettimeofday(&end, NULL);
			printf("Avg ByteRate:%fKB/S\n",
				10*((BUF_LEN+0.0)/((end.tv_sec - start.tv_sec)+((0.0+end.tv_usec - start.tv_usec)/1000000.0))));
			break;

		case '6':
			i = 0;
			fp = fopen(RCV_FILE, "w");
			if (fp == NULL) {
				printf("open %s failed.\n", RCV_FILE);
				return 0;
			}
			while(1) {
				memset(buf, 0, sizeof(buf));
				ret = read(hpi_fd, buf, BUF_LEN);
				i++;
				pshort = (unsigned short *)buf;
				j = 0;
				while(j < (BUF_LEN/2)) {
					fprintf(fp, "0x%4X\n", *pshort);
					pshort++;
					j++;
				}
				printf("%d frames written!\n", i);
				fflush(fp);
				if (i == (SIZE_1M*60)) {
					printf("%d packets rcvd.\n", SIZE_1M*60);
					fflush(fp);
					fclose(fp);
					return 0;
				}
			}
			break;

		case '7':
			for (i = 0; i < sizeof(buf); i++)
				//buf[i] = (int)(255.0 * (rand()/(RAND_MAX + 1.0)));
				buf[i] = (i%256);
			i = 0;
			printf("data initialized....\n");
			gettimeofday(&start, NULL);
			while (i < 1024) {
				ret = read(hpi_fd, buf, BUF_LEN);
				i++;
			}
			gettimeofday(&end, NULL);
			printf("Avg ByteRate:%fKB/S\n",
				((BUF_LEN+0.0)/((end.tv_sec - start.tv_sec)+((0.0+end.tv_usec - start.tv_usec)/1000000.0))));
			break;

		case '8':
			initSighandler();
			for (i = 0; i < sizeof(buf); i++)
				buf[i] = (i%256);

			if (pthread_create(&rcvThrId, NULL, rcvthr, NULL)) {
				printf("create thread rcv failed!\n");
				return -1;
			}

			sleep(4);
			printf("data initialized....Start now\n");

			cur_p = buf;
			i = 0;
			while(g_can_snd) {
				if (i == 256) {
					cur_p = buf;
					i = 0;
					printf("0x%x sent.\n", g_snd_num);
				}
				ret = write(hpi_fd, cur_p, BUF_LEN);
				g_snd_num++;
				if (ret < 0)
					perror("write failed...");				
				cur_p++;
				i++;

				timeout.tv_sec = 0;
				timeout.tv_usec = time_us;
				select(0, NULL, NULL, NULL, &timeout);
			}
			pause();
			
			break;

		default:
			printf("Usage:\n\t0 -> clear_int\t1 -> read print\t2 -> write\t"
					"3 -> write & read\t4 -> read & write\t"
					"5 -> fast write rate\n\t6 -> read file\t"
					"7 -> read rate\t8 -> snd & rcv\n");
	}

	//close(hpi_fd);
	return 0;
}

