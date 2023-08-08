#include "ProactorUDPHandler.h"

CProactorUDPHandler::CProactorUDPHandler(void)
{
    m_u4RecvPacketCount   = 0;
    m_u4SendPacketCount   = 0;
    m_u4RecvSize          = 0;
    m_u4SendSize          = 0;
    m_szCompletionkey[0]  = '\0';
    m_szAct[0]            = '\0';
    m_u4PacketParseInfoID = 0;
    m_u4MaxRecvSize       = MAX_BUFF_1024;
}

CProactorUDPHandler::~CProactorUDPHandler(void)
{
	m_Read.cancel();
	m_Write.cancel();

	if (nullptr != m_pBlockMessage)
	{
		m_pBlockMessage->release();
	}

	if (nullptr != m_pBlockRecv)
	{
		m_pBlockRecv->release();
	}
}

void CProactorUDPHandler::SetPacketParseInfoID(uint32 u4PacketParseInfoID)
{
    m_u4PacketParseInfoID = u4PacketParseInfoID;
}

int CProactorUDPHandler::OpenAddress(const ACE_INET_Addr& AddrLocal, ACE_Proactor* pProactor)
{
    if(m_skRemote.open(AddrLocal) == -1)
    {
        PSS_LOGGER_DEBUG("[CProactorUDPHandler::OpenAddress]Open error({0}).", errno);
        return -1;
    }

    m_addrLocal = AddrLocal;

    m_pPacketParseInfo = App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID);

    sprintf_safe(m_szCompletionkey, MAX_BUFF_20, "CompUDP");
    sprintf_safe(m_szAct, MAX_BUFF_20, "ActUDP");

    //�����̳߳�ʼ��ͳ��ģ�������
    char szName[MAX_BUFF_50] = {'\0'};
    sprintf_safe(szName, MAX_BUFF_50, "�����߳�");
    m_CommandAccount.InitName(szName, GetXmlConfigAttribute(xmlCommandAccount)->MaxCommandCount);

    //��ʼ��ͳ��ģ�鹦��
    m_CommandAccount.Init(GetXmlConfigAttribute(xmlCommandAccount)->Account,
                          GetXmlConfigAttribute(xmlCommandAccount)->FlowAccount,
                          GetXmlConfigAttribute(xmlThreadInfo)->DisposeTimeout);

	m_pBlockMessage = new ACE_Message_Block(GetXmlConfigAttribute(xmlSendInfo)->MaxBlockSize);
	m_pBlockRecv    = new ACE_Message_Block(GetXmlConfigAttribute(xmlRecvInfo)->RecvBuffSize);

    //���÷��ͳ�ʱʱ�䣨��ΪUDP����ͻ��˲����ڵĻ���sendto������һ��recv����
    //����������һ����ʱ���ø�recv�������޵���ȥ
    struct timeval timeout = {MAX_RECV_UDP_TIMEOUT, 0};
    ACE_OS::setsockopt(m_skRemote.get_handle(), SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    //Linux���޴˶�Ӧ�꣬Ϊ�˿���ͨ�ã��ⲿ��ֻ��windows������
#if PSS_PLATFORM == PLATFORM_WIN
    //����wsaIoctl
    bool blBehavior = false;
    unsigned long lRet = 0;
    int nStatus = ACE_OS::ioctl(m_skRemote.get_handle(), SIO_UDP_CONNRESET, &blBehavior, sizeof(blBehavior), nullptr, 0, &lRet, nullptr, nullptr);

    if(0 != nStatus)
    {
        PSS_LOGGER_DEBUG("[CProactorUDPHandler::OpenAddress]ioctl SIO_UDP_CONNRESET error.");
    }

#endif

    if(m_Read.open(*this, m_skRemote.get_handle(), m_szCompletionkey, pProactor) == -1)
    {
        PSS_LOGGER_DEBUG("[CProactorUDPHandler::OpenAddress]m_Read error.");
        return -1;
    }

    if(m_Write.open(*this, m_skRemote.get_handle(), m_szCompletionkey, pProactor) == -1)
    {
        PSS_LOGGER_DEBUG("[CProactorUDPHandler::OpenAddress]m_Write error.");
        return -1;
    }

    //��ʼ�������
    m_TimeConnectInfo.Init(GetXmlConfigAttribute(xmlClientData)->RecvPacketCount,
                           GetXmlConfigAttribute(xmlClientData)->RecvDataMax,
                           GetXmlConfigAttribute(xmlClientData)->SendPacketCount,
                           GetXmlConfigAttribute(xmlClientData)->SendDataMax);
    //��ʼ��PacketParse
    m_pPacketParse = std::make_shared<CPacketParse>();

    size_t stRecvLen = m_pBlockRecv->size();
    int nRecvSize = (int)m_Read.recv(m_pBlockRecv, stRecvLen, 0, PF_INET, m_szAct);
    return nRecvSize;
}

void CProactorUDPHandler::Close(uint32 u4ConnectID)
{
    App_UDPConnectIDManager::instance()->DeleteConnectID(u4ConnectID);
}

void CProactorUDPHandler::handle_read_dgram(const ACE_Asynch_Read_Dgram::Result& result)
{
    //��ΪUDP��һ���Է������������ݰ������Բ�����ճ������
    ACE_Message_Block* pMb = result.message_block();
    int nTran = (int)result.bytes_transferred();

    ACE_INET_Addr addrRemote;
    result.remote_address(addrRemote);

    bool blNew = false;
    uint32 u4ConnectID = App_UDPConnectIDManager::instance()->GetConnetID(addrRemote.get_host_addr(),
        addrRemote.get_port_number(),
        blNew);

    if (true == blNew)
    {
        //������µ����ӣ����������ӽ����¼�
        Send_Hander_Event(u4ConnectID, PACKET_CONNECT, addrRemote);
    }

    if(nTran != 0)
    {
        //��������
        if (false == CheckMessage(u4ConnectID, pMb, (uint32)nTran, addrRemote))
        {
            PSS_LOGGER_DEBUG("[CProactorUDPHandler::handle_read_dgram]CheckMessage error.");
        }
    }
    else
    {
        PSS_LOGGER_DEBUG("[CProactorUDPHandler::handle_read_dgram]result.bytes_transferred() is 0.");
    }

    //���ã�������һ�����ݰ�
    pMb->reset();
    size_t stRecvLen = pMb->size();
    m_Read.recv(pMb, stRecvLen, 0, PF_INET, m_szAct);
}

bool CProactorUDPHandler::SendMessage(const CSendMessageInfo& objSendMessageInfo, uint32& u4PacketSize)
{
    _ClientIPInfo objClientIPInfo = App_UDPConnectIDManager::instance()->GetConnectIP(objSendMessageInfo.u4ConnectID);
    if (objClientIPInfo.m_u2Port == 0)
    {
        //û���ҵ�Ҫ���͵Ķ˿ڣ����ڷ���
        PSS_LOGGER_DEBUG("[CProactorUDPHandler::SendMessage]no find ConnectID={0}.", objSendMessageInfo.u4ConnectID);
        return false;
    }

    _Send_Message_Param obj_Send_Message_Param;
    obj_Send_Message_Param.m_u4PacketParseInfoID = m_u4PacketParseInfoID;
    obj_Send_Message_Param.m_blDlete             = objSendMessageInfo.blDelete;
    obj_Send_Message_Param.m_u2Port              = objClientIPInfo.m_u2Port;
    obj_Send_Message_Param.m_strClientIP         = objClientIPInfo.m_strClientIP;
    obj_Send_Message_Param.m_u2CommandID         = objSendMessageInfo.u2CommandID;
    obj_Send_Message_Param.m_emSendType          = objSendMessageInfo.emSendType;

    u4PacketSize = objSendMessageInfo.pBuffPacket->GetPacketLen();

    bool blState = Udp_Common_Send_Message(obj_Send_Message_Param,
        objSendMessageInfo.pBuffPacket,
        m_skRemote,
        m_pPacketParseInfo,
        m_pBlockMessage);

    if (true == blState)
    {
        SaveProSendInfo(objSendMessageInfo.pBuffPacket->GetPacketLen());
    }


    return true;
}

bool CProactorUDPHandler::PutSendPacket(uint32 u4ConnectID, ACE_Message_Block* pMbData, uint32 u4Size, const PSS_Time_Point& tvSend)
{
    //��������
	_ClientIPInfo objClientIPInfo = App_UDPConnectIDManager::instance()->GetConnectIP(u4ConnectID);
	if (objClientIPInfo.m_u2Port == 0)
	{
		//û���ҵ�Ҫ���͵Ķ˿ڣ����ڷ���
        PSS_LOGGER_DEBUG("[CProactorUDPHandler::PutSendPacket]no find ConnectID={0}.", u4ConnectID);
		return false;
	}

	ACE_INET_Addr AddrRemote;
	int nErr = AddrRemote.set(objClientIPInfo.m_u2Port, objClientIPInfo.m_strClientIP.c_str());

	if (nErr != 0)
	{
        PSS_LOGGER_DEBUG("[PutSendPacket]set_address error[{0}].", errno);
		return false;
	}

	auto u3SendSize = (uint32)m_skRemote.send(pMbData->rd_ptr(), pMbData->length(), AddrRemote);

	if (u3SendSize != u4Size)
	{
		return false;
	}
    else
    {
	    SaveProSendInfo(u3SendSize);
        m_atvOutput = tvSend;
    }

    return true;
}

void CProactorUDPHandler::SetIsLog(bool blIsLog)
{
    ACE_UNUSED_ARG(blIsLog);
    //UDP�ݲ�ʵ��
}

bool CProactorUDPHandler::Device_Send_Data(const char* pData, ssize_t nLen)
{
    PSS_LOGGER_DEBUG("[CProactorUDPHandler::Device_Send_Data]{0}, size={1}.", fmt::ptr(pData), nLen);
    return true;
}

_ClientConnectInfo CProactorUDPHandler::GetClientConnectInfo()
{
    _ClientConnectInfo ClientConnectInfo;
    ClientConnectInfo.m_blValid       = true;
    ClientConnectInfo.m_u4ConnectID   = 0;
    ClientConnectInfo.m_u4AliveTime   = 0;
    ClientConnectInfo.m_u4BeginTime   = (uint32)CTimeStamp::Get_Time_use_second(m_atvInput);
    ClientConnectInfo.m_u4AllRecvSize = m_u4RecvSize;
    ClientConnectInfo.m_u4AllSendSize = m_u4SendSize;
    ClientConnectInfo.m_u4RecvCount   = m_u4RecvPacketCount;
    ClientConnectInfo.m_u4SendCount   = m_u4SendPacketCount;
    return ClientConnectInfo;
}

bool CProactorUDPHandler::CheckMessage(uint32 u4ConnectID, ACE_Message_Block* pMbData, uint32 u4Len, ACE_INET_Addr addrRemote)
{
    m_atvInput = CTimeStamp::Get_Time_Stamp();

    if(m_pPacketParseInfo->m_u1PacketParseType == PACKET_WITHHEAD)
    {
        m_pPacketParse->SetPacket_Head_Src_Length(m_pPacketParseInfo->m_u4OrgLength);

        if(u4Len < m_pPacketParse->GetPacketHeadSrcLen())
        {
            return false;
        }

        //�����������ݰ�ת��ΪPacketParse����
        ACE_Message_Block* pMBHead = App_MessageBlockManager::instance()->Create(m_pPacketParse->GetPacketHeadSrcLen());
        memcpy_safe((char* )pMbData->rd_ptr(), m_pPacketParse->GetPacketHeadSrcLen(), (char* )pMBHead->wr_ptr(), m_pPacketParse->GetPacketHeadSrcLen());
        pMBHead->wr_ptr(m_pPacketParse->GetPacketHeadSrcLen());

        bool blRet = Udp_Common_Recv_Head(u4ConnectID, pMBHead, m_pPacketParse, m_pPacketParseInfo, u4Len);

        if (false == blRet)
        {
            App_MessageBlockManager::instance()->Close(pMBHead);
            return false;
        }

        //��ָ������ƶ�
        pMbData->rd_ptr(m_pPacketParse->GetPacketHeadSrcLen());

        //�����������
        if(m_pPacketParse->GetPacketBodySrcLen() > 0)
        {
            ACE_Message_Block* pMBBody = App_MessageBlockManager::instance()->Create(m_pPacketParse->GetPacketBodySrcLen());
            memcpy_safe((char* )pMbData->rd_ptr(), m_pPacketParse->GetPacketBodySrcLen(), (char* )pMBBody->wr_ptr(), m_pPacketParse->GetPacketBodySrcLen());
            pMBBody->wr_ptr(m_pPacketParse->GetPacketBodySrcLen());

            bool blStateBody = Udp_Common_Recv_Body(u4ConnectID, pMBBody, m_pPacketParse, m_pPacketParseInfo);

            if(false  == blStateBody)
            {
                App_MessageBlockManager::instance()->Close(pMBHead);
                App_MessageBlockManager::instance()->Close(pMBBody);

                m_pPacketParse->Clear();
                return false;
            }

        }
    }
    else
    {
        //������������
        if (false == Udp_Common_Recv_Stream(u4ConnectID, pMbData, m_pPacketParse, m_pPacketParseInfo))
        {
            return false;
        }
    }

	//�������ݰ�
	if (false == Udp_Common_Send_WorkThread(m_MakePacket, u4ConnectID, m_pPacketParse, addrRemote, m_addrLocal, m_atvInput))
	{
		return false;
	}

    m_u4RecvSize += u4Len;
    m_u4RecvPacketCount++;

    return true;
}

void CProactorUDPHandler::SaveProSendInfo(uint32 u4Len)
{
    m_atvOutput = CTimeStamp::Get_Time_Stamp();
    m_u4SendSize += u4Len;
    m_u4SendPacketCount++;
}

void CProactorUDPHandler::Send_Hander_Event(uint32 u4ConnandID, uint8 u1Option, ACE_INET_Addr addrRemote)
{
	_MakePacket objMakePacket;

	objMakePacket.m_u4ConnectID     = u4ConnandID;
	objMakePacket.m_pPacketParse    = nullptr;
	objMakePacket.m_u1Option        = u1Option;
	objMakePacket.m_AddrRemote      = addrRemote;
	objMakePacket.m_u4PacketParseID = m_u4PacketParseInfoID;
	objMakePacket.m_pHandler        = this;
	objMakePacket.m_tvRecv          = m_atvInput;
    objMakePacket.m_emPacketType    = EM_CONNECT_IO_TYPE::CONNECT_IO_UDP;
    objMakePacket.m_AddrListen      = m_addrLocal;

	Send_MakePacket_Queue(m_MakePacket, objMakePacket);
}

void CProactorUDPHandler::GetFlowInfo(uint32& u4FlowIn, uint32& u4FlowOut)
{
    u4FlowIn  = m_CommandAccount.GetFlowIn();
    u4FlowOut = m_CommandAccount.GetFlowOut();
}

void CProactorUDPHandler::SetRecvSize(uint32 u4MaxRecvSize)
{
    m_u4MaxRecvSize = u4MaxRecvSize;
}

uint32 CProactorUDPHandler::GetRecvSize()
{
    return m_u4MaxRecvSize;
}