#ifndef _ILOGMANAGER_H
#define _ILOGMANAGER_H

#include "ILogObject.h"

class ILogManager
{
public:
    virtual ~ILogManager() {}

    //��������־��¼
    virtual int WriteLogBinary(uint16 u2LogType, const string& strLog) = 0;

    //д����־�ӿ�
    virtual int WriteLog_r(uint16 u2LogType, const string& strLog) = 0;

    //д���ʼ��ӿ�
    virtual int WriteToMail_r(uint16 u2LogType, uint16 u2MailID, const string& strTitle, const string& strLog) = 0;
};

#endif