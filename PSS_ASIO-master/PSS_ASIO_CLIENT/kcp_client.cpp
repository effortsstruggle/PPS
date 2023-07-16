#include "kcp_client.h"

/* get system time */
void itimeofday(long* sec, long* usec)
{
#if defined(__unix)
    struct timeval time;
    gettimeofday(&time, NULL);
    if (sec) *sec = time.tv_sec;
    if (usec) *usec = time.tv_usec;
#else
    static long mode = 0, addsec = 0;
    BOOL retval;
    static IINT64 freq = 1;
    IINT64 qpc;
    if (mode == 0) {
        retval = QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
        freq = (freq == 0) ? 1 : freq;
        retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
        addsec = (long)time(NULL);
        addsec = addsec - (long)((qpc / freq) & 0x7fffffff);
        mode = 1;
    }
    retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
    retval = retval * 2;
    if (sec) *sec = (long)(qpc / freq) + addsec;
    if (usec) *usec = (long)((qpc % freq) * 1000000 / freq);
#endif
}

/* get clock in millisecond 64 */
IINT64 iclock64(void)
{
    long s, u;
    IINT64 value;
    itimeofday(&s, &u);
    value = ((IINT64)s) * 1000 + (u / 1000);
    return value;
}

IUINT32 iclock()
{
    return (IUINT32)(iclock64() & 0xfffffffful);
}

/* sleep in millisecond */
void isleep(unsigned long millisecond)
{
#ifdef __unix 	/* usleep( time * 1000 ); */
    usleep((millisecond << 10) - (millisecond << 4) - (millisecond << 3));
#elif defined(_WIN32)
    Sleep(millisecond);
#endif
}



int udpOutPut(const char* buf, int len, ikcpcb* kcp, void* user) {

    //  printf("ʹ��udpOutPut��������\n");

    kcpObj* send = (kcpObj*)user;

    printf("[udpOutPut]send len=%d.\n", len);

    //������Ϣ
    int n = sendto(send->sockfd, buf, len, 0, (struct sockaddr*)&send->addr, sizeof(struct sockaddr_in));
    if (n >= 0)
    {
        //���ظ����ͣ������������
        //printf("udpOutPut-send: �ֽ� =%d bytes   ����=[%s]\n", n ,buf+24);//24�ֽڵ�KCP
        printf("[udpOutPut]send length=%d.\n", n);
        return n;
    }
    else
    {
        printf("udpOutPut: %d bytes send, error\n", n);
        return -1;
    }
}


int init(kcpObj* send)
{
    send->sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (send->sockfd < 0)
    {
        perror("socket error!");
        exit(1);
    }

    memset(&send->addr, 0, sizeof(send->addr));

    //���÷�����ip��port
    send->addr.sin_family = AF_INET;
    send->addr.sin_addr.s_addr = inet_addr((char*)send->ipstr.c_str());
    send->addr.sin_port = htons(send->port);

    printf("sockfd = %d ip = %s  port = %d\n", send->sockfd, send->ipstr.c_str(), send->port);

    return 0;
}

void loop(kcpObj* send)
{
    int n, ret;
#if defined(WIN32)
    int recvlen = sizeof(struct sockaddr_in);
#else
    unsigned int recvlen = sizeof(struct sockaddr_in);
#endif
    ikcp_update(send->pkcp, iclock());

    while (1)
    {
        //ikcp_update����ikcp_flush��ikcp_flush�����Ͷ����е�����ͨ���²�Э��UDP���з���
        ikcp_update(send->pkcp, iclock());//���ǵ���һ�����ξ������ã�Ҫloop����

        char buf[512] = { 0 };

        //��������Ϣ
        n = recvfrom(send->sockfd, buf, 512, 0, (struct sockaddr*)&send->addr, &recvlen);

        if (n < 0)//����Ƿ���UDP���ݰ�
            continue;

        printf("UDP  size  = %d.\n", n);

        //Ԥ��������:����ikcp_input�������ݽ���KCP����Щ�����п�����KCP���Ʊ��ģ�����������Ҫ�����ݡ� 
        //kcp���յ��²�Э��UDP�����������ݵײ�����bufferת����kcp�����ݰ���ʽ
        ret = ikcp_input(send->pkcp, buf, n);

        int recv_length = 0;
        while (1)
        {
            //kcp�����յ���kcp���ݰ���ԭ��֮ǰkcp���͵�buffer����		
            ret = ikcp_recv(send->pkcp, buf, n);
            if (ret < 0)//���ikcp_recv��ȡ��������	
            {
                printf("ikcp_recv success, get recv_length = %d\n", recv_length);
                break;
            }
            else
            {
                recv_length += ret;
            }
        }

        printf("get kcp server recv, ip = %s  port = %d, recv_length=%d.\n", inet_ntoa(send->addr.sin_addr), ntohs(send->addr.sin_port), recv_length);

        //����Ӧ���
        ikcp_update(send->pkcp, iclock());

        if (recv_length > 0)
        {
            printf("get data ok, and break.\n");
            break;
        }

    }

}