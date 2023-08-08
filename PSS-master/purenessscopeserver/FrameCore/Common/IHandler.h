#ifndef PSS_IHANDLER_H
#define PSS_IHANDLER_H

//�ڹ����߳��еĶ�����󷽷���
//���ڼ�¼��WorkThread��

#include "define.h"
#include "SendMessage.h"

class IHandler
{
public:
	virtual ~IHandler() = default;

	virtual void Close(uint32 u4ConnectID) = 0;

	virtual bool SendMessage(const CSendMessageInfo& objSendMessageInfo, uint32& u4PacketSize) = 0;

	virtual bool PutSendPacket(uint32 u4ConnectID, ACE_Message_Block* pMbData, uint32 u4Size, const PSS_Time_Point& tvSend) = 0;

	virtual void SetIsLog(bool blIsLog) = 0;

	virtual bool Device_Send_Data(const char* pData, ssize_t nLen) = 0;
};

#endif