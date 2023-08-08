#pragma once

#include "IBuffPacket.h"
#include "ClientCommand.h"
#include "IObject.h"
#include "Trace.h"

#include <string>

//����ͻ�������(TCP)
//Ĭ������ID������CLIENT_LINK_USER�Ժ�����֣�֮ǰ������ID�Ǳ�����
const uint16 COMMAND_BASE           = 0x1000;
const uint16 COMMAND_RETURN_ALIVE   = 0xf000;
const uint16 NET_VERSION            = 0x6800;			//�汾
const uint16 COMMAND_HEARTBEAT      = 0x2100;			//����
const uint16 COMMAND_TESTREPLY      = 0x2101;			//���Իظ���ͬ����
const uint16 COMMAND_TESTREPLY_SYNC = 0x2102;			//���Իظ����첽��

#define MESSAGE_FUNCTION_BEGIN(x) switch(x) {
#define MESSAGE_FUNCTION(x,y,z,h) case x: { y(z,h); break; }
#define MESSAGE_FUNCTION_END }

using namespace std;

class CBaseCommand : public CClientCommand
{
public:
    CBaseCommand(void);
    ~CBaseCommand(void);

    virtual int DoMessage(IMessage* pMessage, bool& bDeleteFlag, IBuffPacket* pSendBuffPacket);
    void SetServerObject(CServerObject* pServerObject);

    void ReadIniFile(const char* pIniFileName);

private:
    int Do_Connect(IMessage* pMessage, IBuffPacket* pSendBuffPacket);
    int Do_DisConnect(IMessage* pMessage, IBuffPacket* pSendBuffPacket);
    int Do_ClientSendTimeout(IMessage* pMessage, IBuffPacket* pSendBuffPacket);
    int Do_Base(IMessage* pMessage, IBuffPacket* pSendBuffPacket);
    int Do_ClientSendOk(IMessage* pMessage, IBuffPacket* pSendBuffPacket);
    int Do_ReplyTest(IMessage* pMessage, IBuffPacket* pSendBuffPacket);
    int Do_ReplyTest_Sync(IMessage* pMessage, IBuffPacket* pSendBuffPacket);
    int SendClient(_PacketInfo BodyPacket, short nCommand, uint32 nConnectId, bool nEncrypt, IBuffPacket* pSendBuffPacket);

private:
    CServerObject* m_pServerObject;
    int            m_nCount;
};