#pragma once

#include <iostream>
#ifndef __unix
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <sys/time.h>
#include <cstring>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#endif
#include "kcp/ikcp.h"

#if defined(WIN32)
#pragma warning(disable : 5208) 
#endif

typedef struct {
    std::string ipstr;
    int port;

    ikcpcb* pkcp;

    int sockfd;
    struct sockaddr_in addr;//��ŷ������Ľṹ��

    char buff[488] = { '\0' };//����շ�����Ϣ

}kcpObj;

void itimeofday(long* sec, long* usec);

IINT64 iclock64(void);

IUINT32 iclock();

void isleep(unsigned long millisecond);

int udpOutPut(const char* buf, int len, ikcpcb* kcp, void* user);

int init(kcpObj* send);

void loop(kcpObj* send);


