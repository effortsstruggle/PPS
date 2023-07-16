#ifndef _RANDOMNUMBER_H
#define _RANDOMNUMBER_H

//����������ĺ���
//����ָ����Χ���������������

#include <stdlib.h>
#include "define.h"

#if PSS_PLATFORM == PLATFORM_WIN
#include "windows.h"
#include "wincrypt.h"
#else
#include <unistd.h>
#include <fcntl.h>
#endif

class CRandomNumber
{
public:
    CRandomNumber(void) : m_nMinNumber(0), m_nMaxNumber(0)
    {
    };

    ~CRandomNumber(void)
    {
    };

    void SetRange(int nMinNumber, int nMaxNumber)
    {
        m_nMinNumber = nMinNumber;
        m_nMaxNumber = nMaxNumber + 1;
    };

    int GetRandom(int nRandType = 0)
    {
        //��֤��������ֲ����Ǹ���
        if (m_nMaxNumber <= m_nMinNumber || m_nMaxNumber < 0 || m_nMinNumber < 0 || m_nMinNumber == 2147483647)
        {
            return 0;
        }

        int nRandomSeed = 0;

        if (0 == nRandType)
        {
            //�����0������α������ٶȿ졣
            nRandomSeed = GetRandomSeed_Logic("/dev/urandom");
        }
        else
        {
            //���򣬲�����������ٶ�����
            nRandomSeed = GetRandomSeed_Logic("/dev/random");
        }

        return m_nMinNumber + nRandomSeed % (m_nMaxNumber - m_nMinNumber);
    };                //����㷨��0Ĭ����α�����1�������

private:
    int GetRandomSeed_Logic(const char* pPath)
    {
        int rnum = 0;
#if defined WIN32
        HCRYPTPROV hProvider = 0;
        const DWORD dwLength = sizeof(int);
        BYTE pbBuffer[dwLength] = {};
        ::CryptAcquireContext(&hProvider, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
        ::CryptGenRandom(hProvider, dwLength, pbBuffer);
        rnum = *(int*)pbBuffer;
        ::CryptReleaseContext(hProvider, 0);
#else
        int fd = open(pPath, O_RDONLY);

        if (fd != -1)
        {
            size_t stReadLen = read(fd, (void*)&rnum, sizeof(int));

            if (stReadLen != sizeof(int))
            {
                //��ȡ�ļ�ʧ��
                rnum = 0;
            }

            close(fd);
        }

#endif

        //������Ӳ��ܲ�������
        if (rnum < 0)
        {
            rnum = 0 - rnum;
        }

        return rnum;
    };

private:
    int m_nMinNumber;
    int m_nMaxNumber;
};
#endif
