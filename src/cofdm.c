#include <stdio.h> 
#include <fcntl.h> 
#include <errno.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <net/if.h> 
#include <features.h> 
#include <sys/ioctl.h> 
#include <sys/types.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <netinet/ip.h> 
#include <netinet/tcp.h> 
#include <sys/socket.h> 
#include <net/ethernet.h> 
#include <netpacket/packet.h>
#include <sys/resource.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdio.h>
#include <ctype.h>
#include <pthread.h>

#include "debug.h"

typedef struct{
	int packetArpFd;
	int packetIpFd;
	int hpiFd;
	int alive;
	char* devName;
}cap_dev_t;

static cap_dev_t capDev = {
	.devName = "eth0",
};

//#define HPI_DBG
#define DENABLE_CORE

//#define MAX_DATA_LEN 1520//1518 roud to 1520 mtu 1500
#define MAX_DATA_LEN 1518//3200 + 18 roud to 3220 mtu 3200
//#define MAX_BUF_LEN (4 + MAX_DATA_LEN)// 4 bytes hpi header added
//#define MAX_BUF_LEN (1600)// 4 bytes hpi header added
#define MAX_BUF_LEN (3240)// 4 bytes hpi header added

#define MAX(a, b) ((a) > (b)? (a):(b))

static inline int Select(int nfds, fd_set *readfds, fd_set *writefds,
			fd_set *exceptfds, struct timeval *timeout)
{
	int ret = select(nfds, readfds, writefds, exceptfds, timeout);

	if (ret < 0) {
		if (errno == EINTR) {
			DBG("select interrupted!");
		} else {
			ERR("select error:%s!", strerror(errno));
			exit(-1);}
	}else if (ret == 0) {
		ERR("select timeout!");
	}

	return ret;
}

static inline void write_hpi(int fd, char* buf, int len)
{
	int i = 0 ,ret = 0;
	do {
		ret = write(fd, buf, len);
		i++;
#ifdef HPI_DBG		
		if (i > 1)
			DBG("HPI BUSY (%d)", i);
#endif
	} while (ret == -1);
}

static int setPromisc(char *interface, int fd)
{
	struct ifreq ifr;
	strcpy(ifr.ifr_name, interface);
	if(ioctl(fd, SIOCGIFFLAGS, &ifr) == -1) {
		perror("iotcl()");
		return -1;
	}

	ifr.ifr_flags |= IFF_PROMISC;
	if(ioctl(fd, SIOCSIFFLAGS, &ifr) == -1) {
		perror("iotcl()");
		return -1;
	}

	return 0;
}
 
static int unsetPromisc(char *interface, int fd)
{
	struct ifreq ifr;
	strcpy(ifr.ifr_name, interface);
	if(ioctl(fd, SIOCGIFFLAGS, &ifr) == -1) {
		perror("iotcl()");
		return -1;
	}

	ifr.ifr_flags &= ~IFF_PROMISC;
	if(ioctl(fd, SIOCSIFFLAGS, &ifr) == -1) {
		perror("iotcl()");
		return -1;
	}

	return 0;
}

static void hdExit()
{
	int i;
	DBG("ERROR or SIGINT/SIGTERM or STOP received!");

	capDev.alive = 0;
	sleep(5);
	unsetPromisc(capDev.devName, capDev.packetIpFd);
	close(capDev.packetIpFd);
	close(capDev.packetArpFd);
	close(capDev.hpiFd);
	exit(0);
}

static void initSighandler()
{
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, hdExit);
	signal(SIGTERM, hdExit);
}

static void capInit()
{
	int i, ret, sockFd;
	struct sockaddr_ll sll;
	struct ifreq ifstruct;

	sockFd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP));

	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	strcpy(ifstruct.ifr_name, capDev.devName);
	ioctl(sockFd, SIOCGIFINDEX, &ifstruct);
	sll.sll_ifindex = ifstruct.ifr_ifindex;
	//sll.sll_protocol = htons(ETH_P_ALL);//might be changed
	sll.sll_protocol = htons(ETH_P_IP);//might be changed
	//sll.sll_protocol = htons(ETH_P_ARP);//might be changed
	//sll.sll_protocol = htons(ETH_P_RARP);//might be changed
	if(bind(sockFd, (struct sockaddr *)&sll, sizeof(sll)) == -1) {
		perror("bind()");
		exit(1);
	}
	setPromisc(capDev.devName, sockFd);
	capDev.packetIpFd = sockFd;

	sockFd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
	
	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	strcpy(ifstruct.ifr_name, capDev.devName);
	ioctl(sockFd, SIOCGIFINDEX, &ifstruct);
	sll.sll_ifindex = ifstruct.ifr_ifindex;
	//sll.sll_protocol = htons(ETH_P_ALL);//might be changed
	//sll.sll_protocol = htons(ETH_P_IP);//might be changed
	sll.sll_protocol = htons(ETH_P_ARP);//might be changed
	//sll.sll_protocol = htons(ETH_P_RARP);//might be changed
	if(bind(sockFd, (struct sockaddr *)&sll, sizeof(sll)) == -1) {
		perror("bind()");
		exit(2);
	}
	capDev.packetArpFd = sockFd;

	capDev.hpiFd = open("/dev/hpi", O_RDWR);
	if (capDev.hpiFd < 0) {
		perror("cann't open device /dev/hpi");
		exit(3);
	}

	capDev.alive = 1;
}

#define IP_PROTOCOL_TCP    0x06
#define IP_PROTOCOL_UDP 0x11

#define OFFSET_DMAC    0
#define OFFSET_SMAC    6
#define OFFSET_ETHER    12
#define OFFSET_IP_PROT    23
#define OFFSET_SIP    26
#define OFFSET_DIP    30
#define OFFSET_SPORT    34
#define OFFSET_DPORT    36

typedef unsigned char uchar;
typedef enum __ETHER_TYPE {
    ETHER_IP = 0,       /* 0x0800 */
    ETHER_ARP,        /* 0x0806 */
    ETHER_PPP,        /* 0x880b */
    ETHER_MPLS_U,        /* 0x8847 */
    ETHER_MPLS_M,        /* 0x8848 */
    ETHER_PPPOE_DIS,    /* 0x8863 */
    ETHER_PPPOE_SESS,    /* 0x8864 */
    ETHER_EAPOL,        /* 0x888e */
    ETHER_LWAPP,        /* 0x88bb */
    ETHER_MAX         /* 0xffff */
}ETHER_TYPE_T;

uchar* Ether_Type_Desc[] = { \
    "IP", \
    "ARP", \
    "PPP", \
    "MPLS_UNICAST", \
    "MPLS_MULTICAST", \
    "PPPOE_DISCOVERY", \
    "PPPOE_SESSION", \
    "EAPOL", \
    "LWAPP", \
    "Unknown" \
};

typedef enum __IP_PROT_TYPE {
    IP_PROT_TCP = 0,     /* 0x06 */
    IP_PROT_UDP,        /* 0x11 */
    IP_PROT_MAX,        /* 0xFF */
}IP_PROT_TYPE_T;

char* Ip_Protocol_Desc[] = { \
    "TCP", \
    "UDP", \
    "Unknown" \
};

typedef enum __UDP_PORT_TYPE {
    UDP_PORT_CAPWAP_C = 0,    /* 0x2fbf */
    UDP_PORT_CAPWAP_D,    /* 0x2fbe */
    UDP_PORT_DNS,        /* 0x35 */
    UDP_PORT_DHCPS,        /* 0x43 */
    UDP_PORT_DHCPC,        /* 0x44 */    
    UDP_PORT_MAX
}UDP_PORT_TYPE_T;

char* Udp_Port_Desc[] = { \
    "capwap control", \
    "capwap data", \
    "dns", \
    "dhcp server", \
    "dhcp client", \
    "Unknown" \
};

typedef enum __TCP_PORT_TYPE {
    TCP_PORT_FTP_C = 0,     /* 0x15 */
    TCP_PORT_FTP_D,        /* 0x16 */
    TCP_PORT_TELNET,    /* 0x17 */
    TCP_PORT_SMTP,        /* 0x19 */
    TCP_PORT_HTTP,        /* 0x50 */
    TCP_PORT_MAX
}TCP_PORT_TYPE_T;

char* Tcp_Port_Desc[] = { \
    "ftp control", \
    "ftp data", \
    "telnet", \
    "smtp", \
    "http", \
    "Unknown" \
};

static void prcap(char *rcvBuf, int nLen)
{
	int cur = 0;

	while((nLen - cur) >= 16) {
		int i;
		printf("%04x [", cur);
		for(i = 0; i < 16; i++)
			printf("%02x ", rcvBuf[cur+i]);
		printf("]\t[");
	
		for(i = 0; i < 16; i++) {
			char ch = rcvBuf[cur+i];
			if (isalnum(ch))
				printf("%c",ch);
			else
				printf(".");
		}
		printf("]\n");
		cur += 16;
	}
	
	if(nLen > cur) {
		int i = cur;
		printf("%04x [", cur);
		while(i < nLen)
			printf("%02x ", rcvBuf[i++]);
		printf("]");
		i = nLen - cur;
		i = 16-i;
		while(i--)
			printf("   ");
		printf("\t[");
	
		i = cur;
		while(i < nLen) {
			char ch = rcvBuf[i++];
			if(isalnum(ch))
				printf("%c", ch);
			else
				printf(".");
		}
		printf("]\n");
	}
	printf("\n");
}

static void decap(uchar * packet, int totalBytes)
{
    uchar cur;
    uchar cByte;
    ETHER_TYPE_T eType;
    IP_PROT_TYPE_T iType;
    UDP_PORT_TYPE_T uType;
    TCP_PORT_TYPE_T tType;
    int protocol;
    int number = 0;
    
    printf("%-12s:","DST MAC");
    while(number<totalBytes)
    {
        cur = ((uchar*) packet)[number];

        if(number<6)
        {    
            printf("%02x%s", \
                    cur, \
                    number<5?":":"");
            number++;
        }
        else if(number<12)    
        {
            if(number==6)
                printf("\n%-12s:","SRC MAC");
            printf("%02x%s", \
                    cur, \
                    number<11?":":"");
            number++;
        }
        else if(number==12)
        {
            char tmp =((char*) packet)[number+1];

            if(cur==0x08 && tmp == 0x00) eType = ETHER_IP;
            else if (cur==0x08 && tmp == 0x06) eType = ETHER_ARP;
            else eType = ETHER_MAX;
            number += 2; 
            printf("\n%-12s:[%s][0x%02x%02x]","ETH TYPE",Ether_Type_Desc[eType],cur,tmp);        
        }
        else
        {
        switch(eType)
        {
        case ETHER_IP:
            if(number==OFFSET_IP_PROT)    
            {
                protocol = (int)cur;
                switch(protocol)
                {
                case 0x06:
                    iType = IP_PROT_TCP;
                    break;
                case 0x11:
                    iType = IP_PROT_UDP;
                    break;
                default:
                    iType = IP_PROT_MAX;
                }
                printf("\n%-12s:[%s][%d]"," IP PROT",Ip_Protocol_Desc[iType],protocol);
            }
            else if(number>=OFFSET_SIP && number<OFFSET_DIP)
            {            
                if(number==OFFSET_SIP)
                    printf("\n%-12s:","SRC IP");
                printf("%d%s", \
                    cur,(number<OFFSET_DIP-1)?".":"");
            }
            else if(number>=OFFSET_DIP && number<OFFSET_DIP+4)
            {
                if(number==OFFSET_DIP)
                    printf("\n%-12s:","DST IP");
                printf("%d%s", \
                    cur,(number<OFFSET_DIP+3)?".":"");
            }
            else if(number>=OFFSET_SPORT && number<OFFSET_DPORT)
            {
                switch(iType)
                {
                case IP_PROT_TCP:
                    number += 1;
                    cByte = ((uchar*)packet)[number];

                    if(0x00==cur&&0x15==cByte) tType = TCP_PORT_FTP_C;
                    else if(0x00==cur&&0x16==cByte) tType = TCP_PORT_FTP_D;
                    else if(0x00==cur&&0x17==cByte) tType = TCP_PORT_TELNET;
                    else if(0x00==cur&&0x50==cByte) tType = TCP_PORT_HTTP;
                    else tType = TCP_PORT_MAX;

                    printf("\n%-12s:[%s][0x%02x%02x]","SRC PORT", \
                        (tType == TCP_PORT_MAX)? "???" : Tcp_Port_Desc[tType],cur,cByte);
                    number += 1;
                    cur = ((uchar*)packet)[number];
                    number += 1;
                    cByte = ((uchar*)packet)[number];
                    
                    if(0x00==cur&&0x15==cByte) tType = TCP_PORT_FTP_C;
                    else if(0x00==cur&&0x16==cByte) tType = TCP_PORT_FTP_D;
                    else if(0x00==cur&&0x17==cByte) tType = TCP_PORT_TELNET;
                    else if(0x00==cur&&0x50==cByte) tType = TCP_PORT_HTTP;
                    else tType = TCP_PORT_MAX;
                    
                    printf("\n%-12s:[%s][0x%02x%02x]","DST PORT", \
                        ((tType==TCP_PORT_MAX) ? "???" : Tcp_Port_Desc[tType]),cur,cByte);
                    break;
                case IP_PROT_UDP:
                    number += 1;
                    cByte = ((uchar*)packet)[number];

                    if(0x00==cur&&0x35==cByte) uType = UDP_PORT_DNS;
                    else if(0x00==cur&&0x43==cByte) uType = UDP_PORT_DHCPS;
                    else if(0x00==cur&&0x44==cByte) uType = UDP_PORT_DHCPC;
                    else uType = UDP_PORT_MAX;

                    printf("\n%-12s:[%s][0x%02x%02x]","SRC PORT", \
                        ((uType==UDP_PORT_MAX)? "???" : Udp_Port_Desc[uType]),cur,cByte);
                    
                    number += 1;
                    cur = ((uchar*)packet)[number];
                    number += 1;
                    cByte = ((uchar*)packet)[number];
                    
                    if(0x00==cur&&0x35==cByte) uType = UDP_PORT_DNS;
                    else if(0x00==cur&&0x43==cByte) uType = UDP_PORT_DHCPS;
                    else if(0x00==cur&&0x44==cByte) uType = UDP_PORT_DHCPC;
                    else uType = UDP_PORT_MAX;
                    
                    printf("\n%-12s:[%s][0x%02x%02x]","DST PORT", \
                        ((uType==UDP_PORT_MAX)?"???":Udp_Port_Desc[uType]),cur,cByte);
                    break;
                default:
                    ;
                }
            }
            else {}
        break;
    
        case ETHER_ARP:
        break;

        case ETHER_MAX:
        break;

        default:
         break;    
        }
            number++;
        }    
    }

	printf("\n\n");
}

static void initruntime()
{
	struct rlimit limit;
#ifdef DENABLE_CORE
	limit.rlim_cur = RLIM_INFINITY;
	limit.rlim_max = RLIM_INFINITY;
#else
	limit.rlim_cur = 0;
	limit.rlim_max = 0;
#endif
	 if (setrlimit(RLIMIT_CORE, &limit) == -1)
	 	perror("setrlimit for RLIMIT_CORE");

}

/*
ether ------> hpi means enc
hpi ------> ether means dec
	*/
static void *encthr(void *arg)
{
	ssize_t nLen;
	unsigned short len;
	char sndBuf[MAX_BUF_LEN];
	char* rcvBuf = (sndBuf + 2);
	fd_set rdset;
	int maxFd = MAX(capDev.packetIpFd, capDev.packetArpFd);
	struct timeval timeout;
	int packetNum = 0;

	pthread_detach(pthread_self());

	//DBG("start...");

	maxFd += 1;
	while(capDev.alive) {
		FD_ZERO(&rdset);
		FD_SET(capDev.packetIpFd, &rdset);
		FD_SET(capDev.packetArpFd, &rdset);

		if (Select(maxFd, &rdset, NULL, NULL, NULL) <= 0)
			continue;

		if (FD_ISSET(capDev.packetIpFd, &rdset)) {
			nLen = recvfrom(capDev.packetIpFd, rcvBuf, MAX_DATA_LEN, MSG_TRUNC,
					NULL, NULL);
			if (nLen < 0) {
				fprintf(stderr, "recvfrom error:[%s]\n", strerror(errno));
				break;
			}
			len = (nLen - 4);
			//DBG("%d bytes on ip wire and captured", len);
			//prcap(rcvBuf, len);
			//decap(rcvBuf, len);
			//crc must be dropped, but header len is added, so the nLen is exactlly we expected
			memcpy(sndBuf, &len, 2);
			write(capDev.hpiFd, sndBuf, nLen-2);
		}

		if (FD_ISSET(capDev.packetArpFd, &rdset)) {
			nLen = recvfrom(capDev.packetArpFd, rcvBuf, MAX_DATA_LEN, MSG_TRUNC,
					NULL, NULL);
			if (nLen < 0) {
				fprintf(stderr, "recvfrom error:[%s]\n", strerror(errno));
				break;
			}
			len = (nLen - 4);
			//DBG("%d bytes on arp wire and captured", len);
			//prcap(rcvBuf, len);
			//decap(rcvBuf, len);
			//crc must be dropped, but header len is added, so the nLen is exactlly we expected
			memcpy(sndBuf, &len, 2);
			write(capDev.hpiFd, sndBuf, nLen-2);
		}
	}
	
	return NULL;
}

static void *decthr(void *arg)
{
	ssize_t nLen;
	short len;
	char rcvBuf[MAX_BUF_LEN];
	char *cur_p;
	int packetLenLeft;

	pthread_detach(pthread_self());

	//DBG("start...");

	while(capDev.alive) {
		nLen = read(capDev.hpiFd, rcvBuf, MAX_BUF_LEN);
		//DBG("%d bytes on hpi and captured", nLen);
		memcpy(&len, rcvBuf, 2);
		cur_p = (rcvBuf+2);
		packetLenLeft = (MAX_BUF_LEN - 2);

		while ((len > 0) && (len <= 1514)) {
			//DBG("Got len(%d) from HPI.", len);
			//prcap(cur_p, len);
			//decap(cur_p, len);
			if (sendto(capDev.packetIpFd, cur_p, len, 0, NULL, 0) < 0) {
				perror("Unabele to send");
			}
			//DBG("Send len(%d) to eth0", len);
			packetLenLeft -= len;
			if (packetLenLeft < 3) {
				//DBG("reach end of packet, %d left", packetLenLeft);
				break;
			}
			cur_p += len;
			memcpy(&len, cur_p, 2);
			cur_p += 2;
			packetLenLeft -= 2;
		}

		//DBG("One HPI frame finished!(len = %d)", len);
	}

	return NULL;
}

int main(int argc, char * argv[])
{
	ssize_t nLen;
	pthread_t encThrId, decThrId;
	struct sched_param mysched;
	int policy, prio;

	initruntime();
	initSighandler();
	capInit();

	if (argc == 3) {
		switch (*argv[1]) {
			case '0':
				policy = SCHED_FIFO;
				prio = atoi(argv[2]);
				mysched.sched_priority = prio;
				if( sched_setscheduler( 0, policy, &mysched ) == -1 ) {
					perror("sched_setscheduler");
					exit(0);
				}
				printf("set as SCHED_FIFO : %d\n", prio);
				break;
		
			case '1':
				policy = SCHED_RR;
				prio = atoi(argv[2]);
				mysched.sched_priority = prio;
				if( sched_setscheduler( 0, policy, &mysched ) == -1 ) {
					perror("sched_setscheduler");
					exit(0);
				}
				printf("set as SCHED_RR : %d\n", prio);
				break;
		
			default:
				break;
		}

	}
	
	if (pthread_create(&encThrId, NULL, encthr, NULL)) {
		ERR("create thread enc failed!");
		return -1;
	}

	if (pthread_create(&decThrId, NULL, decthr, NULL)) {
		ERR("create thread dec failed!");
		return -1;
	}

	while (capDev.alive) {
		sleep(10);
	}

	sleep(6);
	return 0;
}

