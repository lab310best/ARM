#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#define DEBUG 1

#ifdef DEBUG
#define DBG(format, arg...) do { \
time_t _cur_t = time(NULL); \
struct tm * _cur = localtime(&_cur_t); \
printf("(%02d:%02d:%02d|DEBUG):%s:" format "\n" , _cur->tm_hour, _cur->tm_min, _cur->tm_sec, __FUNCTION__ , ## arg); \
} while (0);
#else
#define DBG(format, arg...)
#endif

#define ERR(format, arg...) do { \
time_t _cur_t = time(NULL); \
struct tm * _cur = localtime(&_cur_t); \
printf("(%02d:%02d:%02d|ERROR):%s:" format "\n" , _cur->tm_hour, _cur->tm_min, _cur->tm_sec, __FUNCTION__ , ## arg); \
} while (0);

#define PRINTF(format, arg...) do { \
time_t _cur_t = time(NULL); \
struct tm * _cur = localtime(&_cur_t); \
printf("(%02d:%02d:%02d|PRINTF):%s:" format , _cur->tm_hour, _cur->tm_min, _cur->tm_sec, __FUNCTION__ , ## arg); \
} while (0);

#define _FUNC_IN DBG("_func_in_")
#define _FUNC_OUT DBG("_func_out_")

#endif
