#ifndef _BASECLIENTCONNECTMANAGER_H
#define _BASECLIENTCONNECTMANAGER_H

//����ʵ��Client*ConnectManager�Ĺ�������
//add by freeeyes
#include "IClientManager.h"
#include "MessageBlockManager.h"

//�����ӿڷ�������Զ�̷�����ʧ����Ϣ
void Common_Send_ConnectError(ACE_Message_Block* pmblk, const ACE_INET_Addr& objAddrServer, IClientMessage* pClientMessage);

bool Common_Send_Data(ACE_Message_Block* pmblk, IClientMessage* pClientMessage, ACE_Message_Block*& pSend);

#endif
