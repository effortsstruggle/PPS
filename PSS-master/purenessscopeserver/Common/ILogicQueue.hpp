#ifndef ILOGICQUEUE_H
#define ILOGICQUEUE_H

#include "define.h"
#include <string>

//add �빤���߳��޹ص�ҵ����Ϣ���е��߼�ҵ����,����̳к�ʵ�� liuruiqi
enum ThreadReturn
{
    THREAD_Task_Exit = 0,    //��ǰ�߳��˳�
    THREAD_Task_Finish,      //��ǰ�߳��������
};

enum ThreadError
{
    THREAD_Error_Timeout = 0,    //�߳�����
};

class ILogicQueue
{
public:
    //��ʼ������ ����:�߼�ID, ����, ���ʱ��(��)
    ILogicQueue(uint32 u4LogicThreadID, uint32 u4Timeout, std::string& strDesc) : m_u4LogicThreadID(u4LogicThreadID),
        m_u4Timeout(u4Timeout),
        m_strDesc(strDesc)
    {
    };

    virtual ~ILogicQueue() {};
public:
    //��ȡ�߳�ID
    uint32 GetLogicThreadID()
    {
        return m_u4LogicThreadID;
    };

    //��ʱ
    void SetTimeOut(uint32 u4Timeout)
    {
        m_u4Timeout = u4Timeout;
    };

    uint32 GetTimeOut()
    {
        return m_u4Timeout;
    };

    //����
    std::string GetDescriptor()
    {
        return m_strDesc;
    };

public:
    //��ʼ������
    virtual bool Init() = 0;
    //ִ���߼�
    virtual ThreadReturn Run(int nMessage, void* arg) = 0;
    //������
    virtual uint32 Error(int nErrorID) = 0;
    //�˳��ƺ�
    virtual void Exit() = 0;
private:
    ILogicQueue& operator=(const ILogicQueue&) = delete;
private:
    uint32      m_u4LogicThreadID;
    uint32      m_u4Timeout;
    std::string m_strDesc;
};


#endif // ILOGIC_H