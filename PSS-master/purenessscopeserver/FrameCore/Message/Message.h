#ifndef _MESSAGE_H
#define _MESSAGE_H

#include "ace/Singleton.h"
#include "ace/Thread_Mutex.h"
#include "MessageBlockManager.h"
#include "BuffPacket.h"
#include "IMessage.h"
#include "ObjectPoolManager.h"
#include "IHandler.h"

//������Ϣ��Ϣ
class CMessage : public IMessage
{
public:
    CMessage(void) = default;

    ACE_Message_Block* GetMessageHead();
    ACE_Message_Block* GetMessageBody();

    _MessageBase* GetMessageBase() final;

    bool GetPacketHead(_PacketInfo& PacketInfo) final;
    bool GetPacketBody(_PacketInfo& PacketInfo) final;
    bool SetPacketHead(ACE_Message_Block* pmbHead) final;
    bool SetPacketBody(ACE_Message_Block* pmbBody) final;

private:
    _MessageBase  m_MessageBase;

    ACE_Message_Block* m_pmbHead          = nullptr;    //��ͷ����
    ACE_Message_Block* m_pmbBody          = nullptr;   //���岿��
};

enum class EM_WORKTHREAD_DIRECT
{
    EM_WORKTHREAD_DIRECT_INPUT = 0,
    EM_WORKTHREAD_DIRECT_OUTPUT,
};

//�����̵߳Ĵ�����Ϣ�ṹ
class CWorkThreadMessage
{
public:
    int                m_nHashID         = 0;                                                //Hash����ID
	uint32             m_u4WorkThreadID  = 0;                                                //�߳�ID
	uint16             m_u2Cmd           = 0;                                                //����ID
    uint32             m_u4PacketParseID = 0;                                                //��ConnectID��Ӧ�Ľ�����ID
    uint32             m_u4ConnectID     = 0;                                                //ConnectID
    EM_PACKET_RESOURCE m_emResouceType   = EM_PACKET_RESOURCE::PACKET_RESOURCE_FROM_CLIENT;  //������Դ�������������ݰ����߿ͻ������ݰ�
    EM_CONNECT_IO_TYPE m_emPacketType    = EM_CONNECT_IO_TYPE::CONNECT_IO_TCP;               //��Ϣ���ͣ�TCP����UDP,����TTY
    EM_WORKTHREAD_DIRECT m_emDirect      = EM_WORKTHREAD_DIRECT::EM_WORKTHREAD_DIRECT_INPUT;  //���ջ��Ƿ���
    ACE_Message_Block* m_pmbRecvHead     = nullptr;                                          //���հ�ͷ����
	ACE_Message_Block* m_pmbRecvBody     = nullptr;                                          //���հ��岿��

    IHandler*          m_pHandler        = nullptr;                                          //Handlerָ��
    PSS_Time_Point     m_tvMessage;                                                          //��Ϣ����ʱ��
	ACE_INET_Addr      m_AddrRemote;                                                         //���ݰ�����ԴIP��Ϣ
	ACE_INET_Addr      m_AddrListen;                                                         //���ݰ���Դ����IP��Ϣ
     
    CSendMessageInfo   m_SendMessageInfo;                                                    //������������           

    CWorkThreadMessage() = default;

    void SetHashID(int nHashID);

    int GetHashID() const;

    void Clear();

    void Close();
};

//һ�������߳���Ϣ���ݼ���
class CWorkThreadMessageList
{
public:
    CWorkThreadMessageList() = default;

    uint32 m_u4WorkThreadID = 0;
    vector<shared_ptr<CWorkThreadMessage>> m_vecList;
};

//�߳���ά����Handler����ܹ�
class CWorkThread_Handler_info
{
public:
    IHandler*          m_pHandler = nullptr;                                             //Handlerָ��
    PSS_Time_Point     m_tvInput;                                                        //��Ϣ����ʱ��
    PSS_Time_Point     m_tvOutput;                                                       //��Ϣ����ʱ��
    std::string        m_strLocalIP;                                                     //����IP
    uint16             m_u2LocalPort;                                                    //�����˿�
	std::string        m_strRemoteIP;                                                    //�ͻ���IP
    uint16             m_u2RemotePort;                                                   //�ͻ��˶˿�
	uint32             m_u4LocalPort;                                                    //�����˿�
	uint32             m_InPacketCount  = 0;                                             //���հ�����
	uint32             m_OutPacketCount = 0;                                             //���Ͱ�����
	uint32             m_RecvSize       = 0;                                             //�����ֽ���
	uint32             m_SendSize       = 0;                                             //�����ֽ���
    uint32             m_u4ConnectID    = 0;                                             //���Ӷ���ID 
    EM_CONNECT_IO_TYPE m_emPacketType   = EM_CONNECT_IO_TYPE::CONNECT_IO_SERVER_TCP;     //��������
    int                m_nHashID        = 0;                                             //Hash����ID

    void SetHashID(int nHashID)
    {
        m_nHashID = nHashID;
    };

    int GetHashID() const
    {
        return m_nHashID;
    };

	void Clear()
	{
		m_pHandler = nullptr;
		m_InPacketCount = 0;
		m_OutPacketCount = 0;
		m_RecvSize = 0;
		m_SendSize = 0;
	}
};

//CWorkThreadMessage�����
class CDeviceHandlerPool : public CObjectPoolManager<CWorkThread_Handler_info, ACE_Recursive_Thread_Mutex>
{
public:
    CDeviceHandlerPool() = default;

	static void Init_Callback(int nIndex, CWorkThread_Handler_info* pHandler);
	static void Close_Callback(int nIndex, CWorkThread_Handler_info* pHandler);

    CWorkThread_Handler_info* Create();
	bool Delete(CWorkThread_Handler_info* pHandler);

	int GetUsedCount();
	int GetFreeCount();

	void GetCreateInfoList(vector<_Object_Create_Info>& objCreateList);
};

#endif