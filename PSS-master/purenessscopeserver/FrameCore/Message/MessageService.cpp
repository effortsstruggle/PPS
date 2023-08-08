// MessageService.h
// ������Ϣ���߳��࣬Connect�ὫҪ������CMessage���������Щ�߳��д�����
// ��Ϊ��Щ���£���Ϊ�����˾ͻ�������ȴ���֣�������ȴ�����ң������ģ����±����������
// add by freeeyes
// 2009-01-26

#include "MessageService.h"

CMessageService::CMessageService()
{
    uint16 u2ThreadTimeOut = GetXmlConfigAttribute(xmlThreadInfo)->ThreadTimeout;

    if(u2ThreadTimeOut == 0)
    {
        m_u2ThreadTimeOut = MAX_MSG_THREADTIMEOUT;
    }
    else
    {
        m_u2ThreadTimeOut = u2ThreadTimeOut;
    }
}

void CMessageService::Init(uint32 u4ThreadID, uint32 u4MaxQueue, uint32 u4LowMask, uint32 u4HighMask, bool blIsCpuAffinity)
{
    m_u4MaxQueue    = u4MaxQueue;
    m_u4HighMask    = u4HighMask;
    m_u4LowMask     = u4LowMask;

    //�����߳���Ϣ
    m_u4ThreadID = u4ThreadID;
    m_ThreadInfo.m_u4ThreadID   = u4ThreadID;

    m_objWorkThreadProcess.m_objPacketCounter.m_u1ThreadID = (uint8)u4ThreadID;

    //���õ�ǰ�߳��Ƿ�CPU��
    m_blIsCpuAffinity = blIsCpuAffinity;

    m_u4WorkQueuePutTime = GetXmlConfigAttribute(xmlThreadInfo)->PutQueueTimeout * 1000;

    //��ʼ���߳�AI
    m_WorkThreadAI.Init(GetXmlConfigAttribute(xmlThreadInfoAI)->AI,
                        GetXmlConfigAttribute(xmlThreadInfoAI)->TimeoutCount,
                        GetXmlConfigAttribute(xmlThreadInfoAI)->CheckTime,
                        GetXmlConfigAttribute(xmlThreadInfoAI)->TimeoutCount,
                        GetXmlConfigAttribute(xmlThreadInfoAI)->StopTime,
                        GetXmlConfigAttribute(xmlThreadInfoAI)->ReturnDataType,
                        GetXmlConfigAttribute(xmlThreadInfoAI)->ReturnData.c_str());

    m_u1PacketCounter = GetXmlConfigAttribute(xmlMessage)->Packet_Counter;

    //��ʼ�����ͻ���
    m_objBuffSendPacket.Init(DEFINE_PACKET_SIZE, GetXmlConfigAttribute(xmlMessage)->Msg_Buff_Max_Size);
    m_objBuffSendPacket.SetNetSort(GetXmlConfigAttribute(xmlNetWorkMode)->NetByteOrder);

    //�������ĵȴ�ʱ��(��λ�Ǻ���)
    m_u4MaxRecvWait = GetXmlConfigAttribute(xmlClientInfo)->CheckAliveTime * 1000;

    m_PerformanceCounter.init("WorkThread", 10000);
}

bool CMessageService::Start()
{
    if(0 != open())
    {
        m_emThreadState = MESSAGE_SERVICE_THREAD_STATE::THREAD_STOP;
        return false;
    }
    else
    {
        m_emThreadState = MESSAGE_SERVICE_THREAD_STATE::THREAD_RUN;
    }

    return true;
}

int CMessageService::open()
{
    m_blRun = true;

    PSS_LOGGER_DEBUG("[CMessageService::open] m_u4HighMask = [{0}] m_u4LowMask = [{1}].", m_u4HighMask, m_u4LowMask);

    //����һ���̶߳��д���
    m_ttQueue = std::thread([this]()
        {
            svc();
        });

    return 0;
}

int CMessageService::svc(void)
{
	//�ж��Ƿ�Ҫ��CPU
	if (true == m_blIsCpuAffinity)
	{
#if PSS_PLATFORM == PLATFORM_WIN
		SetThreadAffinityMask(GetCurrentThread(), m_u4ThreadID);
#else
		cpu_set_t mask;
		CPU_ZERO(&mask);
		CPU_SET(m_u4ThreadID, &mask);

		if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) != 0)
		{
            PSS_LOGGER_DEBUG("[CMessageService::svc]({0})cound not get thread affinity.", m_u4ThreadID);
		}

#endif
	}

    while(m_blRun)
    {
        shared_ptr<CWorkThreadMessageList> msgList;
        m_objThreadQueue.Pop(msgList);

        if (false == Dispose_Queue(msgList))
        {
            break;
        }
        msgList->m_vecList.clear();
    }

	//�ر����е�����
	vector<shared_ptr<CWorkThread_Handler_info>> handlerList;

    for_each(m_objHandlerList.begin(), m_objHandlerList.end(), [&handlerList](const std::pair<uint32, shared_ptr<CWorkThread_Handler_info>>& iter) {
        handlerList.emplace_back(iter.second);
        });

    for (auto handler_info : handlerList)
    {
        handler_info->m_pHandler->Close(handler_info->m_u4ConnectID);
    }

    m_objHandlerList.clear();

    //�ر�����ָ�
    CloseCommandList();

    m_objClientCommandList.clear();

    m_WorkThreadAI.Close();

    PSS_LOGGER_DEBUG("[CMessageService::svc] svc finish!");
    return 0;
}

bool CMessageService::PutMessage(shared_ptr<CWorkThreadMessageList> pMessage)
{
    m_objThreadQueue.Push(pMessage);

    return true;
}

bool CMessageService::PutUpdateCommandMessage(uint32 u4UpdateIndex)
{
    auto pMessage = std::make_shared<CWorkThreadMessage>();

    //������Ϣ���ͺ͸���Index
    pMessage->m_emPacketType = EM_CONNECT_IO_TYPE::COMMAND_UPDATE;
    pMessage->m_u4ConnectID = u4UpdateIndex;

    auto pMessageLst = std::make_shared<CWorkThreadMessageList>();
    pMessageLst->m_vecList.emplace_back(pMessage);

    m_objThreadQueue.Push(pMessageLst);

    return true;
}

shared_ptr<CWorkThread_Handler_info> CMessageService::ProcessRecvMessage(shared_ptr<CWorkThreadMessage> pMessage, uint32 u4ThreadID)
{
    //������ת����Message����
    CMessage objRecvMessage;
    shared_ptr<CWorkThread_Handler_info> pWorkThread_Handler_info = nullptr;
    auto f = m_objHandlerList.find(pMessage->m_u4ConnectID);

    if (m_objHandlerList.end() != f)
    {
        pWorkThread_Handler_info = f->second;
    }

    //��¼���һ�δ���������ID
    m_ThreadInfo.m_u2CommandID = pMessage->m_u2Cmd;

    //�ж���Ϣ����
    if (CLIENT_LINK_CONNECT == pMessage->m_u2Cmd ||
        CLINET_LINK_TTY_CONNECT == pMessage->m_u2Cmd)
    {
        //�������Ӷ���
        pWorkThread_Handler_info = std::make_shared<CWorkThread_Handler_info>();

        pWorkThread_Handler_info->m_strLocalIP   = pMessage->m_AddrListen.get_host_addr();
        pWorkThread_Handler_info->m_u2LocalPort  = pMessage->m_AddrListen.get_port_number();
		pWorkThread_Handler_info->m_strRemoteIP  = pMessage->m_AddrRemote.get_host_addr();
		pWorkThread_Handler_info->m_u2RemotePort = pMessage->m_AddrRemote.get_port_number();

        pWorkThread_Handler_info->m_u4ConnectID  = pMessage->m_u4ConnectID;
        pWorkThread_Handler_info->m_pHandler     = pMessage->m_pHandler;
        pWorkThread_Handler_info->m_emPacketType = pMessage->m_emPacketType;

        //Ѱ�Ҷ����Ƿ��Ѿ�����
        if (f != m_objHandlerList.end())
        {
            m_objHandlerList.erase(f);
        }

        m_objHandlerList[pMessage->m_u4ConnectID] = pWorkThread_Handler_info;

		//д��������־
        string strLog = fmt::format("Connection from [{0}:{1}] ConnectID={2}.",
            pWorkThread_Handler_info->m_strRemoteIP,
            pWorkThread_Handler_info->m_u2RemotePort,
            pMessage->m_u4ConnectID);

		AppLogManager::instance()->WriteLog_r(LOG_SYSTEM_CONNECT, strLog);
    }

	if (nullptr == pWorkThread_Handler_info)
	{
        PSS_LOGGER_DEBUG("[CMessageService::ProcessMessage] [{0}]({1})pWorkThread_Handler_info is nullptr.",
            u4ThreadID,
            pMessage->m_u4ConnectID);
		DeleteMessage(pMessage);
		return nullptr;
	}

    //���½��հ���ʱ��
    pWorkThread_Handler_info->m_tvInput = pMessage->m_tvMessage;

    uint32 u4PacletHeadLength = 0;
    uint32 u4PacletBodyLength = 0;
    if (nullptr != pMessage->m_pmbRecvHead)
    {
        u4PacletHeadLength = (uint32)pMessage->m_pmbRecvHead->length();
    }

    if (nullptr != pMessage->m_pmbRecvBody)
    {
        u4PacletBodyLength = (uint32)pMessage->m_pmbRecvBody->length();
    }

	if (pMessage->m_u2Cmd >= CLIENT_LINK_USER)
	{
		//����Handlerͳ����Ϣ
		pWorkThread_Handler_info->m_InPacketCount++;
		pWorkThread_Handler_info->m_RecvSize += u4PacletHeadLength + u4PacletBodyLength;
		pWorkThread_Handler_info->m_tvInput = pMessage->m_tvMessage;

        if (m_u1PacketCounter != 0)
        {
            m_objWorkThreadProcess.AddPacketIn(u4PacletHeadLength + u4PacletBodyLength,
                CTimeStamp::Get_Time_of_Minute(pMessage->m_tvMessage));
        }

        //�����߳�ͳ����Ϣ
        m_ThreadInfo.m_u4RecvPacketCount++;
	}

    //ƴ����Ϣ
    objRecvMessage.GetMessageBase()->m_u2Cmd          = pMessage->m_u2Cmd;
    objRecvMessage.GetMessageBase()->m_strClientIP    = pWorkThread_Handler_info->m_strRemoteIP;
    objRecvMessage.GetMessageBase()->m_u2ClientPort   = pWorkThread_Handler_info->m_u2RemotePort;
    objRecvMessage.GetMessageBase()->m_strListenIP    = pWorkThread_Handler_info->m_strLocalIP;
    objRecvMessage.GetMessageBase()->m_u2ListenPort   = pWorkThread_Handler_info->m_u2LocalPort;
    objRecvMessage.GetMessageBase()->m_tvRecvTime     = pMessage->m_tvMessage;
    objRecvMessage.GetMessageBase()->m_u4ConnectID    = pMessage->m_u4ConnectID;
    objRecvMessage.GetMessageBase()->m_u4HeadSrcSize  = u4PacletHeadLength;
    objRecvMessage.GetMessageBase()->m_u4BodySrcSize  = u4PacletBodyLength;
    objRecvMessage.GetMessageBase()->m_emPacketType   = pMessage->m_emPacketType;
    objRecvMessage.GetMessageBase()->m_emResouceType  = pMessage->m_emResouceType;
    objRecvMessage.GetMessageBase()->m_u4WorkThreadID = m_u4ThreadID;

    objRecvMessage.SetPacketHead(pMessage->m_pmbRecvHead);
    objRecvMessage.SetPacketBody(pMessage->m_pmbRecvBody);

    //����������߳��Լ����
    m_ThreadInfo.m_tvUpdateTime = pMessage->m_tvMessage;
    m_ThreadInfo.m_u4State = THREADSTATE::THREAD_RUNBEGIN;

    //��Ҫ���������ݷŵ��߼������ĵط�ȥ
    uint16 u2CommandID = objRecvMessage.GetMessageBase()->m_u2Cmd;          //���ݰ���CommandID

    uint16 u2CommandCount = 0;      //������ô���
    bool   blDeleteFlag   = true;   //�����Ƿ�ɾ����Ĭ����ɾ��

    DoMessage(&objRecvMessage, u2CommandID, u2CommandCount, blDeleteFlag);

    //����ǶϿ���Ϣ�������﷢�����߼��߳�ȥ����
    if (CLIENT_LINK_CDISCONNET == pMessage->m_u2Cmd ||
        CLIENT_LINK_SDISCONNET == pMessage->m_u2Cmd ||
        CLINET_LINK_TTY_DISCONNECT == pMessage->m_u2Cmd)
    {
		//д��־
        string strLog = fmt::format("Close Connection from [{0}:{1}] RecvSize = {2}, RecvCount = {3}, SendCount = {4}, SendSize = {5}.",
            pWorkThread_Handler_info->m_strRemoteIP,
            pWorkThread_Handler_info->m_u2RemotePort,
            pWorkThread_Handler_info->m_InPacketCount,
            pWorkThread_Handler_info->m_RecvSize,
            pWorkThread_Handler_info->m_OutPacketCount,
            pWorkThread_Handler_info->m_SendSize);

		AppLogManager::instance()->WriteLog_r(LOG_SYSTEM_CONNECT, strLog);

        pWorkThread_Handler_info->m_pHandler->Close(pMessage->m_u4ConnectID);
        m_objHandlerList.erase(f);
    }

    DeleteMessage(pMessage);

    m_ThreadInfo.m_u4State = THREADSTATE::THREAD_RUNEND;

    return pWorkThread_Handler_info;
}

bool CMessageService::ProcessSendMessage(shared_ptr<CWorkThreadMessage> pMessage, uint32 u4ThreadID)
{
    int nRet = true;
	auto f = m_objHandlerList.find(pMessage->m_SendMessageInfo.u4ConnectID);

    if (m_objHandlerList.end() != f)
    {
        auto pWorkThread_Handler_info = f->second;
        uint32 u4PacketSize = 0;
        nRet = pWorkThread_Handler_info->m_pHandler->SendMessage(pMessage->m_SendMessageInfo, u4PacketSize);

        if (m_u1PacketCounter != 0)
        {
            m_objWorkThreadProcess.AddPacketOut(u4PacketSize,
                CTimeStamp::Get_Time_of_Minute(pMessage->m_tvMessage));
        }
    }
    else
    {
        nRet = false;
        PSS_LOGGER_DEBUG("[CMessageService::ProcessSendMessage](ThreadID={0})(CommandID={1})Handler is nullptr.",
            u4ThreadID,
            pMessage->m_u4ConnectID);
    }

    DeleteMessage(pMessage);
    return nRet;
}

bool CMessageService::ProcessSendClose(shared_ptr<CWorkThreadMessage> pMessage, uint32 u4ThreadID)
{
	int nRet = true;
	auto f = m_objHandlerList.find(pMessage->m_u4ConnectID);

	if (m_objHandlerList.end() != f)
	{
        auto pWorkThread_Handler_info = f->second;
        pWorkThread_Handler_info->m_pHandler->Close(pWorkThread_Handler_info->m_u4ConnectID);
        m_objHandlerList.erase(f);
	}
	else
	{
		nRet = false;
        PSS_LOGGER_DEBUG("[CMessageService::ProcessSendClose](ThreadID={0})(CommandID={1})Handler is nullptr.",
			u4ThreadID,
			pMessage->m_u4ConnectID);
	}

    DeleteMessage(pMessage);
    return nRet;
}

bool CMessageService::ProcessSendIsLog(shared_ptr<CWorkThreadMessage> pMessage, uint32 u4ThreadID)
{
    ACE_UNUSED_ARG(u4ThreadID);
    int nRet = true;
    auto f = m_objHandlerList.find(pMessage->m_u4ConnectID);

    if (m_objHandlerList.end() != f)
    {
        auto pWorkThread_Handler_info = f->second;
        if (pMessage->m_SendMessageInfo.nMessageID == 0)
        {
            pWorkThread_Handler_info->m_pHandler->SetIsLog(false);
        }
        else
        {
            pWorkThread_Handler_info->m_pHandler->SetIsLog(true);
        }
    }

    DeleteMessage(pMessage);
    return nRet;
}

int CMessageService::Close()
{
    if (true == m_blRun)
    {
        m_blRun = false;

        //��һ����Ϣ�������߳���ֹ��
        auto p = std::make_shared<CWorkThreadMessage>();
        p->m_emPacketType = EM_CONNECT_IO_TYPE::WORKTHREAD_CLOSE;

        auto pMessageLst = std::make_shared<CWorkThreadMessageList>();
        pMessageLst->m_vecList.emplace_back(p);
        PutMessage(pMessageLst);

        //�ȴ��̴߳������
        m_ttQueue.join();

        PSS_LOGGER_DEBUG("[CMessageService::Close] Close Finish.");
    }

    return 0;
}

bool CMessageService::SaveThreadInfoData(const PSS_Time_Point& tvNow)
{
    //��������߳��Լ�
    auto time_internal_millisecond = CTimeStamp::Get_Time_Difference(tvNow, m_ThreadInfo.m_tvUpdateTime);
    auto time_interval_second = (uint16)(time_internal_millisecond / 1000);

    //��ʼ�鿴�߳��Ƿ�ʱ
    if(m_ThreadInfo.m_u4State == THREADSTATE::THREAD_RUNBEGIN && time_interval_second > m_u2ThreadTimeOut)
    {
        string strLog = fmt::format("[WorkThread_timeout] pThreadInfo = {0} State = {1} Time = [{2}] PacketCount = {3} LastCommand = {4:#x} PacketTime = {5} TimeOut > {6}{7} QueueCount = {8}.",
            m_ThreadInfo.m_u4ThreadID,
            m_ThreadInfo.m_u4State,
            CTimeStamp::Get_DateTime(m_ThreadInfo.m_tvUpdateTime),
            m_ThreadInfo.m_u4RecvPacketCount,
            m_ThreadInfo.m_u2CommandID,
            m_ThreadInfo.m_u2PacketTime,
            m_u2ThreadTimeOut,
            time_interval_second,
            m_objThreadQueue.Size());

        AppLogManager::instance()->WriteLog_r(LOG_SYSTEM_WORKTHREAD, strLog);

        strLog = fmt::format("[CMessageService::handle_timeout] ThreadID = {0} Thread is reset.",
            m_u4ThreadID);
        //���������̣߳���Ҫ������Ӧ���߳�
        AppLogManager::instance()->WriteLog_r(LOG_SYSTEM_WORKTHREAD, strLog);
        return false;
    }
    else
    {
        string strLog = fmt::format("[WorkThread_nomal] pThreadInfo = {0} State = {1} Time = [{2}] PacketCount = {3} LastCommand = {4:#x} PacketTime = {5} QueueCount = {6}.",
            m_ThreadInfo.m_u4ThreadID,
            m_ThreadInfo.m_u4State,
            CTimeStamp::Get_DateTime(m_ThreadInfo.m_tvUpdateTime),
            m_ThreadInfo.m_u4RecvPacketCount,
            m_ThreadInfo.m_u2CommandID,
            m_ThreadInfo.m_u2PacketTime,
            m_objThreadQueue.Size());

        AppLogManager::instance()->WriteLog_r(LOG_SYSTEM_WORKTHREAD, strLog);

        m_ThreadInfo.m_u4CurrPacketCount = 0;
        return true;
    }
}

void CMessageService::CloseCommandList()
{
    //������ǰ�����б�
    vector<shared_ptr<CClientCommandList>> vecClientCommandList;

    for_each(m_objClientCommandList.begin(), m_objClientCommandList.end(), [&vecClientCommandList](const std::pair<uint16, shared_ptr<CClientCommandList>>& iter) {
        vecClientCommandList.emplace_back(iter.second);
        });

    for (auto ClientCommand : vecClientCommandList)
    {
        ClientCommand->Close();
    }

    m_objClientCommandList.clear();
}

shared_ptr<CClientCommandList> CMessageService::GetClientCommandList(uint16 u2CommandID)
{
    auto f = m_objClientCommandList.find(u2CommandID);
    if (m_objClientCommandList.end() != f)
    {
        return f->second;
    }
    else
    {
        return nullptr;
    }
}

bool CMessageService::DoMessage(IMessage* pMessage, uint16& u2CommandID, uint16& u2Count, bool& bDeleteFlag)
{
    if (nullptr == pMessage)
    {
        PSS_LOGGER_DEBUG("[CMessageService::DoMessage] pMessage is nullptr.");
        return false;
    }

    //�Ÿ���Ҫ�̳е�ClientCommand��ȥ����
    shared_ptr<CClientCommandList> pClientCommandList = GetClientCommandList(u2CommandID);

    if (nullptr == pClientCommandList)
    {
		//û���ҵ���Ӧ��ע��ָ��������define.h������쳣�����¼�쳣������־
		if (u2CommandID >= CLIENT_LINK_USER)
		{
            string strLog = fmt::format("[CommandID={0}][HeadLen={1}][BodyLen={2}] is not plugin dispose.",
                u2CommandID,
                pMessage->GetMessageBase()->m_u4HeadSrcSize,
                pMessage->GetMessageBase()->m_u4BodySrcSize);

            AppLogManager::instance()->WriteLog_r(LOG_SYSTEM_ERROR, strLog);
		}

        PSS_LOGGER_DEBUG("[CMessageService::DoMessage] pClientCommandList no find({0}).", u2CommandID);
        return false;
    }

    int nCount = pClientCommandList->GetCount();

    for (int i = 0; i < nCount; i++)
    {
        auto pClientCommandInfo = pClientCommandList->GetClientCommandIndex(i);
        if (nullptr != pClientCommandInfo)
        {
            //�жϵ�ǰ��Ϣ�Ƿ���ָ���ļ����˿�
            if (pClientCommandInfo->m_objListenIPInfo.m_u2Port > 0 &&
                (pClientCommandInfo->m_objListenIPInfo.m_strClientIP != pMessage->GetMessageBase()->m_strListenIP ||
                    pClientCommandInfo->m_objListenIPInfo.m_u2Port != pMessage->GetMessageBase()->m_u2ListenPort))
            {
                continue;
            }

            //��ǵ�ǰ��������״̬
            pClientCommandInfo->m_pClientCommand->DoMessage(pMessage, bDeleteFlag, &m_objBuffSendPacket);

        }

        //��¼������ô���
        u2Count = (uint16)nCount;
    }

    return true;
}

_ThreadInfo* CMessageService::GetThreadInfo()
{
    return &m_ThreadInfo;
}

void CMessageService::GetAIInfo(_WorkThreadAIInfo& objAIInfo) const
{
    m_WorkThreadAI.GetAIInfo(objAIInfo);
}

uint32 CMessageService::GetThreadID() const
{
    return m_u4ThreadID;
}

NAMESPACE::uint32 CMessageService::GetThreadID()
{
    return m_u4ThreadID;
}

uint32 CMessageService::GetHandlerCount() const
{
    return (uint32)m_objHandlerList.size();
}

void CMessageService::CopyMessageManagerList()
{
    CloseCommandList();

    auto pHashtCommandList = App_MessageManager::instance()->GetHashCommandList();

	vector<shared_ptr<CClientCommandList>> vecClientCommandList;

    for_each(pHashtCommandList.begin(), pHashtCommandList.end(), [&vecClientCommandList](const std::pair<uint16, shared_ptr<CClientCommandList>>& iter) {
        //���ָ����Ϣ
        vecClientCommandList.emplace_back(iter.second);
        });

	for (auto pClientCommandList : vecClientCommandList)
	{
        if (nullptr == pClientCommandList)
        {
            continue;
        }

		auto pCurrClientCommandList = std::make_shared<CClientCommandList>();

		if (nullptr == pCurrClientCommandList)
		{
			continue;
		}

		pCurrClientCommandList->SetCommandID(pClientCommandList->GetCommandID());
		pCurrClientCommandList->SetCommandTimeout(pClientCommandList->GetCommandTimeout());

		for (int j = 0; j < pClientCommandList->GetCount(); j++)
		{
			auto pClientCommandInfo = pClientCommandList->GetClientCommandIndex(j);

			if (nullptr != pClientCommandInfo)
			{
				pCurrClientCommandList->AddClientCommand(pClientCommandInfo->m_pClientCommand, 
                    pClientCommandInfo->m_strModuleName.c_str(), 
                    nullptr);
			}
		}

        m_objClientCommandList[pClientCommandList->GetCommandID()] = pCurrClientCommandList;
	}
}

void CMessageService::GetAITO(vecCommandTimeout& objTimeout) const
{
    m_WorkThreadAI.GetAllTimeout(m_u4ThreadID, objTimeout);
}

void CMessageService::GetAITF(vecCommandTimeout& objTimeout) const
{
    m_WorkThreadAI.GetAllForbiden(m_u4ThreadID, objTimeout);
}

void CMessageService::SetAI(uint8 u1AI, uint32 u4DisposeTime, uint32 u4WTCheckTime, uint32 u4WTStopTime)
{
    m_WorkThreadAI.ReSet(u1AI, u4DisposeTime, u4WTCheckTime, u4WTStopTime);
}

void CMessageService::SetThreadState(MESSAGE_SERVICE_THREAD_STATE emState)
{
    m_emThreadState = emState;
}

MESSAGE_SERVICE_THREAD_STATE CMessageService::GetThreadState() const
{
    return m_emThreadState;
}

THREADSTATE CMessageService::GetStepState() const
{
    return m_ThreadInfo.m_u4State;
}

uint32 CMessageService::GetUsedMessageCount()
{
    return (uint32)m_objThreadQueue.Size();
}

shared_ptr<CWorkThreadMessage> CMessageService::CreateMessage() const
{
    return std::make_shared<CWorkThreadMessage>();
}

void CMessageService::DeleteMessage(shared_ptr<CWorkThreadMessage> pMessage) const
{
    //��������
    pMessage->Close();
}

void CMessageService::GetFlowPortList(const PSS_Time_Point& tvNow, vector<CWorkThread_Packet_Info>& vec_Port_Data_Account)
{
    vec_Port_Data_Account.push_back(m_objWorkThreadProcess.GetCurrInfo(CTimeStamp::Get_Time_of_Minute(tvNow)));
}

bool CMessageService::Synchronize_SendPostMessage(shared_ptr<CWorkThread_Handler_info> pHandlerInfo, const PSS_Time_Point& tvMessage)
{
	//ͬ����������
    uint32 u4SendLength = m_objBuffSendPacket.GetPacketLen();

    auto pBlockSend = App_MessageBlockManager::instance()->Create(u4SendLength);

    memcpy_safe(m_objBuffSendPacket.GetData(),
        u4SendLength,
        pBlockSend->rd_ptr(),
        u4SendLength);

    pBlockSend->wr_ptr(u4SendLength);

    pHandlerInfo->m_pHandler->PutSendPacket(pHandlerInfo->m_u4ConnectID, pBlockSend, u4SendLength, tvMessage);
    pBlockSend->release();
    m_objBuffSendPacket.Clear();

    //����Handlerͳ����Ϣ
    pHandlerInfo->m_OutPacketCount++;
    pHandlerInfo->m_SendSize += u4SendLength;
    pHandlerInfo->m_tvOutput = tvMessage;

    if (m_u1PacketCounter != 0)
    {
        m_objWorkThreadProcess.AddPacketOut(u4SendLength,
            CTimeStamp::Get_Time_of_Minute(tvMessage));
    }

    return true;
}

bool CMessageService::SendPostMessage(const CSendMessageInfo& objSendMessageInfo)
{
    //�����ݷ������
    auto pWorkThreadMessage = CreateMessage();

    pWorkThreadMessage->m_u4ConnectID     = objSendMessageInfo.u4ConnectID;
    pWorkThreadMessage->m_u2Cmd           = objSendMessageInfo.u2CommandID;
    pWorkThreadMessage->m_emDirect        = EM_WORKTHREAD_DIRECT::EM_WORKTHREAD_DIRECT_OUTPUT;
    pWorkThreadMessage->m_SendMessageInfo = objSendMessageInfo;

    auto pMessageList = std::make_shared<CWorkThreadMessageList>();
    pMessageList->m_vecList.emplace_back(pWorkThreadMessage);

    return PutMessage(pMessageList);
}

bool CMessageService::SendCloseMessage(uint32 u4ConnectID)
{
	//�����ݷ������
	auto pWorkThreadMessage = CreateMessage();

	pWorkThreadMessage->m_emDirect    = EM_WORKTHREAD_DIRECT::EM_WORKTHREAD_DIRECT_OUTPUT;
	pWorkThreadMessage->m_u2Cmd       = CLINET_LINK_HANDLER_CLOSE;
    pWorkThreadMessage->m_u4ConnectID = u4ConnectID;

    auto pMessageList = std::make_shared<CWorkThreadMessageList>();
    pMessageList->m_vecList.emplace_back(pWorkThreadMessage);

	return PutMessage(pMessageList);
}

_ClientIPInfo CMessageService::GetClientIPInfo(uint32 u4ConnectID)
{
    _ClientIPInfo objClientIPInfo;
	auto f = m_objHandlerList.find(u4ConnectID);

    if (m_objHandlerList.end() != f)
    {
        auto pWorkThread_Handler_info = f->second;
        objClientIPInfo.m_strClientIP = pWorkThread_Handler_info->m_strRemoteIP;
        objClientIPInfo.m_u2Port      = pWorkThread_Handler_info->m_u2RemotePort;
    }

    return objClientIPInfo;
}

NAMESPACE::_ClientIPInfo CMessageService::GetLocalIPInfo(uint32 u4ConnectID)
{
	_ClientIPInfo objClientIPInfo;
	auto f = m_objHandlerList.find(u4ConnectID);

    if (m_objHandlerList.end() != f)
	{
        auto pWorkThread_Handler_info = f->second;
		objClientIPInfo.m_strClientIP = pWorkThread_Handler_info->m_strLocalIP;
		objClientIPInfo.m_u2Port = pWorkThread_Handler_info->m_u2LocalPort;
	}

    return objClientIPInfo;
}

void CMessageService::Check_Handler_Recv_Timeout()
{
    if (0 == m_u4MaxRecvWait)
    {
        //�����ֱ���˳�
        return;
    }

    vector<shared_ptr<CWorkThread_Handler_info>> vecList;

    for_each(m_objHandlerList.begin(), m_objHandlerList.end(), [&vecList](const std::pair<uint32, shared_ptr<CWorkThread_Handler_info>>& iter) {
        vecList.emplace_back(iter.second);
        });

    auto tvNow = CTimeStamp::Get_Time_Stamp();

    for (shared_ptr<CWorkThread_Handler_info> pHandlerInfo : vecList)
    {
        if (EM_CONNECT_IO_TYPE::CONNECT_IO_TCP != pHandlerInfo->m_emPacketType 
            && EM_CONNECT_IO_TYPE::CONNECT_IO_UDP != pHandlerInfo->m_emPacketType)
        {
            //�������TCP���ӣ��������Ӽ��
            continue;
        }

        auto time_interval_millisecond = CTimeStamp::Get_Time_Difference(tvNow, pHandlerInfo->m_tvInput);
        if (m_u4MaxRecvWait < time_interval_millisecond)
        {
            PSS_LOGGER_DEBUG("[CMessageService::Check_Handler_Recv_Timeout]u2CommandID={0} last tvNow={1}.", pHandlerInfo->m_u4ConnectID, CTimeStamp::Get_DateTime(tvNow));
            PSS_LOGGER_DEBUG("[CMessageService::Check_Handler_Recv_Timeout]u2CommandID={0} last m_tvInput={1}.", pHandlerInfo->m_u4ConnectID, CTimeStamp::Get_DateTime(pHandlerInfo->m_tvInput));
            PSS_LOGGER_DEBUG("[CMessageService::Check_Handler_Recv_Timeout]u2CommandID={0} is recv timeout.", pHandlerInfo->m_u4ConnectID);

            //��ʱ�ˣ��������ӶϿ���Ϣ
			CSendMessageInfo objSendMessageInfo;

			objSendMessageInfo.u2CommandID = CLIENT_LINK_SDISCONNET;
			objSendMessageInfo.u4ConnectID = pHandlerInfo->m_u4ConnectID;

			SendPostMessage(objSendMessageInfo);
        }
    }
}

EM_Client_Connect_status CMessageService::GetConnectState(uint32 u4ConnectID)
{
    auto f = m_objHandlerList.find(u4ConnectID);
    if (m_objHandlerList.end() == f)
    {
        return EM_Client_Connect_status::CLIENT_CONNECT_NO_EXIST;
    }
    else
    {
        return EM_Client_Connect_status::CLIENT_CONNECT_EXIST;
    }
}

void CMessageService::UpdateCommandList(uint32 u4UpdateIndex)
{
    if (u4UpdateIndex > 0)
    {
        int nReload = App_ModuleLoader::instance()->UnloadListUpdate(u4UpdateIndex);

        if (1 == nReload)
        {
            //��Ҫ֪ͨ����ٸ���һ�¸���(���µļ�����Ч)
            App_MessageServiceGroup::instance()->PutUpdateCommandMessage(App_MessageManager::instance()->GetUpdateIndex());
        }

        //ͬ�������б�
        CopyMessageManagerList();
    }
}

bool CMessageService::Dispose_Queue(shared_ptr<CWorkThreadMessageList> msgList)
{
    if (EM_CONNECT_IO_TYPE::COMMAND_UPDATE == msgList->m_vecList[0]->m_emPacketType)
    {
        //����ǵ�ǰ����ˢ��
        UpdateCommandList(msgList->m_vecList[0]->m_u4ConnectID);

        return true;
    }

    if (EM_CONNECT_IO_TYPE::WORKTHREAD_CLOSE == msgList->m_vecList[0]->m_emPacketType)
    {
        //�߳��˳�
        return true;
    }

    if (EM_WORKTHREAD_DIRECT::EM_WORKTHREAD_DIRECT_INPUT == msgList->m_vecList[0]->m_emDirect)
    {
        //�������ݿ����Ƕ����һ�δ�����Ȼ�������Ҫͬ�����͵�����һ���ͳ�ȥ,���Ч��
        shared_ptr<CWorkThread_Handler_info> pWorkThread_Handler_info = nullptr;

        for (auto msg : msgList->m_vecList)
        {
            pWorkThread_Handler_info = ProcessRecvMessage(msg, m_u4ThreadID);
        }
        
        //һ��ͬ������
        if (m_objBuffSendPacket.GetPacketLen() > 0 && nullptr != pWorkThread_Handler_info)
        {
            Synchronize_SendPostMessage(pWorkThread_Handler_info, msgList->m_vecList[0]->m_tvMessage);
        }
    }
    else
    {
        //�Ƿ������ݺ������ر�����
        if (CLIENT_LINK_SDISCONNET == msgList->m_vecList[0]->m_u2Cmd)
        {
            //�ر�����
            ProcessSendClose(msgList->m_vecList[0], m_u4ThreadID);
        }
        else if (CLINET_LINK_IS_LOG == msgList->m_vecList[0]->m_u2Cmd)
        {
            //������־
            ProcessSendIsLog(msgList->m_vecList[0], m_u4ThreadID);
        }
        else
        {
            //�Ƿ�������
            ProcessSendMessage(msgList->m_vecList[0], m_u4ThreadID);
        }
    }

    return true;
}

//==========================================================
CMessageServiceGroup::CMessageServiceGroup()
{
    uint16 u2ThreadTimeCheck = GetXmlConfigAttribute(xmlThreadInfo)->ThreadTimeCheck;

    if(u2ThreadTimeCheck == 0)
    {
        m_u2ThreadTimeCheck = MAX_MSG_TIMEDELAYTIME;
    }
    else
    {
        m_u2ThreadTimeCheck = u2ThreadTimeCheck;
    }
}

int CMessageServiceGroup::timer_task(brynet::TimerMgr::Ptr timerMgr)
{
    auto tv = CTimeStamp::Get_Time_Stamp();

    //��鳬ʱ������
    CheckRecvTimeout();

    //������й����߳�
    if (false == CheckWorkThread(tv))
    {
        PSS_LOGGER_DEBUG("[CMessageServiceGroup::handle_timeout]CheckWorkThread is fail.");
    }

    //��¼ͳ��CPU���ڴ��ʹ��
    if (false == CheckCPUAndMemory())
    {
        PSS_LOGGER_DEBUG("[CMessageServiceGroup::handle_timeout]CheckCPUAndMemory is fail.");
    }

    //������в��״̬
    if (false == CheckPlugInState())
    {
        PSS_LOGGER_DEBUG("[CMessageServiceGroup::handle_timeout]CheckPlugInState is fail.");
    }

    if (m_blTimerState)
    {
        //������ʱ��
        start_new_task(timerMgr);
    }

    return 0;
}

void CMessageServiceGroup::start_new_task(brynet::TimerMgr::Ptr timerMgr)
{
    PSS_LOGGER_DEBUG("[CMessageServiceGroup::start_new_task]new timer({0}) is set.", m_u2ThreadTimeCheck);
    auto timer = timerMgr->addTimer(std::chrono::seconds(m_u2ThreadTimeCheck), [this, timerMgr]() {
        timer_task(timerMgr);
        });
}

bool CMessageServiceGroup::Init(uint32 u4ThreadCount, uint32 u4MaxQueue, uint32 u4LowMask)
{
    //ɾ����ǰ������CMessageService����

    //��¼��ǰ����
    m_u4MaxQueue     = u4MaxQueue;
    m_u4HighMask     = u4MaxQueue * 8; //����ĸ�ˮλ�겻��������Ϊ׼����������������*ָ���С����
    m_u4LowMask      = u4LowMask;
    m_u2CurrThreadID = 0;

    //ʱ��ģʽ����
    PSS_LOGGER_DEBUG("[CMessageServiceGroup::Init]Timing sequence Start.");

    //��õ�ǰCPU������
#if PSS_PLATFORM == PLATFORM_WIN
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    m_u2CpuNumber = (uint16)si.dwNumberOfProcessors;
#else
    m_u2CpuNumber = (uint16)sysconf(_SC_NPROCESSORS_CONF);
#endif

    PSS_LOGGER_DEBUG("[CMessageServiceGroup::Init]CPU NUmber is {0}.", m_u2CpuNumber);

    //��ʼ�����е�Message����
    for (uint32 i = 0; i < u4ThreadCount; i++)
    {
        auto pMessageService = std::make_shared<CMessageService>();

        if (nullptr == pMessageService)
        {
            PSS_LOGGER_DEBUG("[CMessageServiceGroup::Init]({0})pMessageService is nullptr.", i);
            return false;
        }

        //���CPU���ڹ����߳����������Զ����̵߳�CPU
        if (m_u2CpuNumber >= u4ThreadCount)
        {
            pMessageService->Init(i, u4MaxQueue, u4LowMask, m_u4HighMask, true);
        }
        else
        {
            pMessageService->Init(i, u4MaxQueue, u4LowMask, m_u4HighMask, false);
        }

        m_vecMessageService.emplace_back(pMessageService);
    }

    return true;
}

bool CMessageServiceGroup::PutMessage(shared_ptr<CWorkThreadMessageList> pMessage)
{
    //�ж��Ƿ�ΪTCP�������������ConnectID���֡�UDP���������һ��

    //�õ������߳�ID
    auto pMessageService = m_vecMessageService[pMessage->m_u4WorkThreadID];

    if (false == pMessageService->PutMessage(pMessage))
    {
        PSS_LOGGER_DEBUG("[CMessageServiceGroup::PutMessage]({0})pMessageService fail.", pMessageService->GetThreadID());
    }

    return true;
}

bool CMessageServiceGroup::PutUpdateCommandMessage(uint32 u4UpdateIndex) const
{
    //�����й����߳�Ⱥ������������Ϣ
    for (auto pMessageService : m_vecMessageService)
    {
        if (nullptr != pMessageService && false == pMessageService->PutUpdateCommandMessage(u4UpdateIndex))
        {
            PSS_LOGGER_DEBUG("[CMessageServiceGroup::PutMessage]({0})pMessageService fail.", pMessageService->GetThreadID());
            return false;
        }
    }

    return true;
}

void CMessageServiceGroup::Close()
{
    if (false == KillTimer())
    {
        PSS_LOGGER_DEBUG("[CMessageServiceGroup::Close]KillTimer error.");
    }

    ACE_Time_Value tvSleep(0, 1000);

    for (auto pMessageService : m_vecMessageService)
    {
        if (nullptr != pMessageService && 0 != pMessageService->Close())
        {
            PSS_LOGGER_DEBUG("[CMessageServiceGroup::Close]({0})pMessageService fail.", pMessageService->GetThreadID());
        }

        ACE_OS::sleep(tvSleep);
    }

    m_vecMessageService.clear();

}

bool CMessageServiceGroup::Start()
{
    if(StartTimer() == false)
    {
        return false;
    }

    PSS_LOGGER_DEBUG("[CMessageServiceGroup::Start]Work thread count={0}.", m_vecMessageService.size());

    for (auto pMessageService : m_vecMessageService)
    {
        if (nullptr != pMessageService && false == pMessageService->Start())
        {
            PSS_LOGGER_DEBUG("[CMessageServiceGroup::Start]({0})WorkThread is fail.", pMessageService->GetThreadID());
            return false;
        }

        PSS_LOGGER_DEBUG("[CMessageServiceGroup::Start]({0})WorkThread is OK.", pMessageService->GetThreadID());
    }

    return true;
}

bool CMessageServiceGroup::StartTimer()
{
    PSS_LOGGER_DEBUG("[CMessageServiceGroup::StartTimer] begin....");

    m_blTimerState = true;

    timer_task(App_TimerManager::instance()->GetTimerPtr());

    return true;
}

bool CMessageServiceGroup::KillTimer()
{
    PSS_LOGGER_DEBUG("[CMessageServiceGroup::KillTimer] begin....");

    if(m_blTimerState == true)
    {
        m_blTimerState = false;
    }

    PSS_LOGGER_DEBUG("[CMessageServiceGroup::KillTimer] end....");
    return true;
}

bool CMessageServiceGroup::CheckRecvTimeout() const
{
    for (auto pWorkThread : m_vecMessageService)
    {
        pWorkThread->Check_Handler_Recv_Timeout();
    }

    return true;
}

bool CMessageServiceGroup::CheckWorkThread(const PSS_Time_Point& tvNow) const
{
    for (auto pMessageService : m_vecMessageService)
    {
        if (nullptr != pMessageService && false == pMessageService->SaveThreadInfoData(tvNow))
        {
            PSS_LOGGER_DEBUG("[CMessageServiceGroup::CheckWorkThread]SaveThreadInfo error.");
        }

        //Ŀǰ�ڹ����̷߳���������ʱ����ɱ�������̣߳�ɱ�˹����̻߳ᵼ��һЩ�ڴ����⡣
    }

    return true;
}

bool CMessageServiceGroup::CheckCPUAndMemory(bool blTest) const
{
    if (GetXmlConfigAttribute(xmlMonitor)->CpuAndMemory == 1 || true == blTest)
    {
#if PSS_PLATFORM == PLATFORM_WIN
        auto u4CurrCpu = (uint32)GetProcessCPU_Idel();
#else
        auto u4CurrCpu = (uint32)GetProcessCPU_Idel_Linux();
#endif

        //������Messageblock,BuffPacket,MessageCount,�ڴ��С
        uint32 u4MessageCount = GetUsedMessageCount();

        if (u4CurrCpu > GetXmlConfigAttribute(xmlMonitor)->CpuMax || u4MessageCount > GetXmlConfigAttribute(xmlMonitor)->MemoryMax)
        {
            PSS_LOGGER_DEBUG("[CMessageServiceGroup::handle_timeout]CPU Rote={0},u4MessageCount={1} ALERT.", u4CurrCpu, u4MessageCount);

            string strLog = fmt::format("[Monitor] CPU Rote={0},u4MessageCount={1}.",
                u4CurrCpu, 
                u4MessageCount);
            AppLogManager::instance()->WriteLog_r(LOG_SYSTEM_MONITOR, strLog);
        }
    }

    return true;
}

bool CMessageServiceGroup::Send_Post_Message(const CSendMessageInfo& objSendMessageInfo)
{
	//�õ�����߳�ID
	uint32 u4ThreadID = GetWorkThreadID(objSendMessageInfo.u4ConnectID, 
        EM_CONNECT_IO_TYPE::CONNECT_IO_TCP);

    auto pMessageService = m_vecMessageService[u4ThreadID];

    return pMessageService->SendPostMessage(objSendMessageInfo);
}

bool CMessageServiceGroup::Send_Close_Message(uint32 u4ConnectID)
{
	//�õ�����߳�ID
	uint32 u4ThreadID = GetWorkThreadID(u4ConnectID,
		EM_CONNECT_IO_TYPE::CONNECT_IO_TCP);

	auto pMessageService = m_vecMessageService[u4ThreadID];

    return pMessageService->SendCloseMessage(u4ConnectID);
}

NAMESPACE::_ClientIPInfo CMessageServiceGroup::GetClientIPInfo(uint32 u4ConnectID)
{
	//�õ�����߳�ID
	uint32 u4ThreadID = GetWorkThreadID(u4ConnectID,
		EM_CONNECT_IO_TYPE::CONNECT_IO_TCP);

	auto pMessageService = m_vecMessageService[u4ThreadID];

    return pMessageService->GetClientIPInfo(u4ConnectID);
}

_ClientIPInfo CMessageServiceGroup::GetLocalIPInfo(uint32 u4ConnectID)
{
	//�õ�����߳�ID
	uint32 u4ThreadID = GetWorkThreadID(u4ConnectID,
		EM_CONNECT_IO_TYPE::CONNECT_IO_TCP);

	auto pMessageService = m_vecMessageService[u4ThreadID];

	return pMessageService->GetLocalIPInfo(u4ConnectID);
}

uint32 CMessageServiceGroup::GetHandlerCount() const
{
    uint32 u4HandlerCount = 0;
	for (auto pMessageService : m_vecMessageService)
	{
		if (nullptr != pMessageService)
		{
            u4HandlerCount += pMessageService->GetHandlerCount();
		}
	}

    return u4HandlerCount;
}

EM_Client_Connect_status CMessageServiceGroup::GetConnectState(uint32 u4ConnectID)
{
    //�õ�����߳�ID
    uint32 u4ThreadID = GetWorkThreadID(u4ConnectID,
        EM_CONNECT_IO_TYPE::CONNECT_IO_TCP);

    return m_vecMessageService[u4ThreadID]->GetConnectState(u4ConnectID);
}

bool CMessageServiceGroup::CheckPlugInState() const
{
    vector<shared_ptr<_ModuleInfo>> vecModeInfo;
    App_ModuleLoader::instance()->GetAllModuleInfo(vecModeInfo);

    for (auto pModuleInfo : vecModeInfo)
    {
        if (nullptr != pModuleInfo)
        {
            uint32 u4ErrorID = 0;
            bool blModuleState = pModuleInfo->GetModuleState(u4ErrorID);

            if (false == blModuleState)
            {
                string strTitle = "ModuleStateError";

                string strLog = fmt::format("Module ErrorID={0}.",
                    u4ErrorID);

                //�����ʼ�
                AppLogManager::instance()->WriteToMail_r(LOG_SYSTEM_MONITOR, 
                    1,
                    strTitle.c_str(),
                    strLog);
            }
        }
    }

    return true;
}

void CMessageServiceGroup::AddDyringIP(const char* pClientIP, uint16 u2MaxCount)
{
    return m_objMessageDyeingManager.AddDyringIP(pClientIP, u2MaxCount);
}

bool CMessageServiceGroup::AddDyeingCommand(uint16 u2CommandID, uint16 u2MaxCount)
{
    return m_objMessageDyeingManager.AddDyeingCommand(u2CommandID, u2MaxCount);
}

void CMessageServiceGroup::GetDyeingCommand(vec_Dyeing_Command_list& objList) const
{
    m_objMessageDyeingManager.GetDyeingCommand(objList);
}

void CMessageServiceGroup::GetFlowPortList(vector<CWorkThread_Packet_Info>& vec_Port_Data_Account) const
{
    vec_Port_Data_Account.clear();
    auto tvNow = CTimeStamp::Get_Time_Stamp();

    for (auto pMessageService : m_vecMessageService)
    {
        pMessageService->GetFlowPortList(tvNow, vec_Port_Data_Account);
    }
}

void CMessageServiceGroup::GetThreadInfo(vector<_ThreadInfo>& vecWorkThreadList) const
{
    for (auto pMessageService : m_vecMessageService)
    {
        vecWorkThreadList.emplace_back(*pMessageService->GetThreadInfo());
    }
}

uint32 CMessageServiceGroup::GetUsedMessageCount() const
{
    uint32 u4Count = 0;
    for (auto pMessageService : m_vecMessageService)
    {
        u4Count += pMessageService->GetUsedMessageCount();
    }

    return u4Count;
}

uint32 CMessageServiceGroup::GetWorkThreadCount() const
{
    return (uint32)m_vecMessageService.size();
}

uint32 CMessageServiceGroup::GetWorkThreadIDByIndex(uint32 u4Index)
{
    if(u4Index >= m_vecMessageService.size())
    {
        return (uint32)0;
    }
    else
    {
        return (uint32)m_vecMessageService[u4Index]->GetThreadInfo()->m_u4ThreadID;
    }
}

void CMessageServiceGroup::GetWorkThreadAIInfo(vecWorkThreadAIInfo& objvecWorkThreadAIInfo) const
{
    objvecWorkThreadAIInfo.clear();

    for (auto pMessageService : m_vecMessageService)
    {
        _WorkThreadAIInfo objWorkThreadAIInfo;

        if (nullptr != pMessageService)
        {
            pMessageService->GetAIInfo(objWorkThreadAIInfo);
            objWorkThreadAIInfo.m_u4ThreadID = pMessageService->GetThreadID();
            objvecWorkThreadAIInfo.push_back(objWorkThreadAIInfo);
        }
    }
}

void CMessageServiceGroup::GetAITO(vecCommandTimeout& objTimeout) const
{
    objTimeout.clear();

    for (auto pMessageService : m_vecMessageService)
    {
        if (nullptr != pMessageService)
        {
            pMessageService->GetAITO(objTimeout);
        }
    }
}

void CMessageServiceGroup::GetAITF(vecCommandTimeout& objTimeout) const
{
    objTimeout.clear();

    for (auto pMessageService : m_vecMessageService)
    {
        pMessageService->GetAITF(objTimeout);
    }
}

void CMessageServiceGroup::SetAI(uint8 u1AI, uint32 u4DisposeTime, uint32 u4WTCheckTime, uint32 u4WTStopTime) const
{
    for (auto pMessageService : m_vecMessageService)
    {
        pMessageService->SetAI(u1AI, u4DisposeTime, u4WTCheckTime, u4WTStopTime);
    }
}

shared_ptr<CWorkThreadMessage> CMessageServiceGroup::CreateMessage(uint32 u4ConnectID, EM_CONNECT_IO_TYPE u1PacketType)
{
    uint32 u4ThreadID = 0;
    u4ThreadID = GetWorkThreadID(u4ConnectID, u1PacketType);

    auto pMessageService = m_vecMessageService[u4ThreadID];

    if (nullptr != pMessageService)
    {
        auto pWorkThreadMessage = pMessageService->CreateMessage();
        //�����̵߳�ID
        pWorkThreadMessage->m_u4WorkThreadID = u4ThreadID;
        return pWorkThreadMessage;
    }
    else
    {
        return nullptr;
    }
}

void CMessageServiceGroup::DeleteMessage(shared_ptr<CWorkThreadMessage> pMessage)
{
    m_vecMessageService[pMessage->m_u4WorkThreadID]->DeleteMessage(pMessage);
}

void CMessageServiceGroup::CopyMessageManagerList() const
{
    //��ʼ�����е�Message����
    for (auto pMessageService : m_vecMessageService)
    {
        if (nullptr == pMessageService)
        {
            PSS_LOGGER_DEBUG("[CMessageServiceGroup::CopyMessageManagerList]({0})pMessageService is nullptr.", pMessageService->GetThreadID());
        }
        else
        {
            pMessageService->CopyMessageManagerList();
        }
    }
}

uint32 CMessageServiceGroup::GetWorkThreadID(uint32 u4ConnectID, EM_CONNECT_IO_TYPE emPackeType) const
{
    ACE_UNUSED_ARG(emPackeType);
    uint32 u4ThreadID = 0;

    if (m_vecMessageService.size() == 0)
    {
        return u4ThreadID;
    }

    return u4ConnectID % (uint32)m_vecMessageService.size();
}
