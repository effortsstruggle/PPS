//ʵ������PSS��PacketParse����
//�򻯽ӿڣ��Ժ����ķ�ʽʵ��
//add by freeeyes

#include "define.h"
#include "IMessageBlockManager.h"

#include "ace/svc_export.h"

#ifdef WIN32
#ifdef PACKETPARSE_INTERFACE_EXPORTS
#define DECLDIR __declspec(dllexport)
#else
#define DECLDIR __declspec(dllimport)
#endif
#else
#define DECLDIR ACE_Svc_Export
#endif

//0����ӡ����������־��1Ϊ��ӡ
#define PACKETPARSE_SHOW_LOG 0
enum
{
    PACKETPARSE_SHOW_OFF = 0,
    PACKETPARSE_SHOW_ON
};

#define PACKETPARSE_SHOW_BEGIN if(PACKETPARSE_SHOW_LOG == PACKETPARSE_SHOW_ON) {
#define PACKETPARSE_SHOW_END }

using namespace PSS;

extern "C"
{
    DECLDIR bool Parse_Packet_Head_Info(uint32 u4ConnectID, ACE_Message_Block* pmbHead, IMessageBlockManager* pMessageBlockManager, _Head_Info* pHeadInfo, EM_CONNECT_IO_TYPE emIOType);
    DECLDIR bool Parse_Packet_Body_Info(uint32 u4ConnectID, ACE_Message_Block* pmbbody, IMessageBlockManager* pMessageBlockManager, _Body_Info* pBodyInfo, EM_CONNECT_IO_TYPE emIOType);
    DECLDIR uint8 Parse_Packet_Stream(uint32 u4ConnectID, ACE_Message_Block* pCurrMessage, IMessageBlockManager* pMessageBlockManager, _Packet_Info* pPacketInfo, EM_CONNECT_IO_TYPE emIOType);
    DECLDIR bool Make_Send_Packet(uint32 u4ConnectID, const char* pData, uint32 u4Len, ACE_Message_Block* pMbData, uint16 u2CommandID);
    DECLDIR uint32 Make_Send_Packet_Length(uint32 u4ConnectID, uint32 u4DataLen, uint16 u2CommandID);
    DECLDIR bool Connect(uint32 u4ConnectID, _ClientIPInfo objClientIPInfo, _ClientIPInfo const& objLocalIPInfo);
    DECLDIR void DisConnect(uint32 u4ConnectID);
    DECLDIR void Close();

    //������ͷ����Ҫ���pHeadInfo���ݽṹ����ɺ����_Head_Info�����ݽṹ
    bool Parse_Packet_Head_Info(uint32 u4ConnectID, ACE_Message_Block* pmbHead, IMessageBlockManager* pMessageBlockManager, _Head_Info* pHeadInfo, EM_CONNECT_IO_TYPE emIOType)
    {
		ACE_UNUSED_ARG(emIOType);
        if(NULL == pHeadInfo || NULL == pMessageBlockManager)
        {
            return false;
        }

        //�������������Ĵ���
        char* pData  = (char* )pmbHead->rd_ptr();
        uint32 u4Len = (uint32)pmbHead->length();
        uint32 u4Pos = 0;

        uint32 u2CmdID       = 0;           //CommandID
        uint32 u4BodyLen     = 0;           //���峤��

        //������ͷ
        memcpy_safe((char* )&pData[u4Pos], (uint32)sizeof(uint16), (char* )&u2CmdID, (uint32)sizeof(uint16));
        u4Pos += sizeof(uint16);
        memcpy_safe((char* )&pData[u4Pos], (uint32)sizeof(uint32), (char* )&u4BodyLen, (uint32)sizeof(uint32));
        u4Pos += sizeof(uint32);

        PACKETPARSE_SHOW_BEGIN
        OUR_DEBUG((LM_INFO, "[CPacketParse::SetPacketHead]u4ConnectID=%d,m_u2CmdID=%d,m_u4BodyLen=%d.\n",
                   u4ConnectID,
                   u2CmdID,
                   u4BodyLen));
        PACKETPARSE_SHOW_END

        //�������IDΪ0����Ϊ�����
        if(0 == u2CmdID)
        {
            return false;
        }

        //��䷵�ظ���ܵ����ݰ�ͷ��Ϣ
        pHeadInfo->m_u4HeadSrcLen      = u4Len;
        pHeadInfo->m_u4HeadCurrLen     = u4Len;
        pHeadInfo->m_u2PacketCommandID = u2CmdID;
        pHeadInfo->m_pmbHead           = pmbHead;
        pHeadInfo->m_u4BodySrcLen      = u4BodyLen;

        return true;
    }

    //�������壬��Ҫ���pBodyInfo���ݽṹ����ɺ����_Body_Info�����ݽṹ
    bool Parse_Packet_Body_Info(uint32 u4ConnectID, ACE_Message_Block* pmbbody, IMessageBlockManager* pMessageBlockManager, _Body_Info* pBodyInfo, EM_CONNECT_IO_TYPE emIOType)
    {
		ACE_UNUSED_ARG(emIOType);
        if(NULL == pBodyInfo || NULL == pMessageBlockManager)
        {
            return false;
        }

        PACKETPARSE_SHOW_BEGIN
        OUR_DEBUG((LM_INFO, "[CPacketParse::Parse_Packet_Body_Info]u4ConnectID=%d,pmbbody=%d.\n", u4ConnectID, pmbbody->length()));
        PACKETPARSE_SHOW_END

        //��䷵�ظ���ܵİ�����Ϣ
        pBodyInfo->m_u4BodySrcLen  = (uint32)pmbbody->length();
        pBodyInfo->m_u4BodyCurrLen = (uint32)pmbbody->length();
        pBodyInfo->m_pmbBody       = pmbbody;

        return true;
    }

    //��ģʽ�ݽ����������ɹ���Ҫ���_Packet_Info�ṹ
    uint8 Parse_Packet_Stream(uint32 u4ConnectID, ACE_Message_Block* pCurrMessage, IMessageBlockManager* pMessageBlockManager, _Packet_Info* pPacketInfo, EM_CONNECT_IO_TYPE emIOType)
    {
		ACE_UNUSED_ARG(emIOType);
        //�������������Ĵ���
        if(NULL == pCurrMessage || NULL == pMessageBlockManager || NULL == pPacketInfo)
        {
            return PACKET_GET_ERROR;
        }

        PACKETPARSE_SHOW_BEGIN
        OUR_DEBUG((LM_INFO, "[CPacketParse::Parse_Packet_Stream]u4ConnectID=%d,pCurrMessage=%d.\n", u4ConnectID, pCurrMessage->length()));
        PACKETPARSE_SHOW_END

        return PACKET_GET_ENOUGH;
    }

    //ƴ�����ݷ��ذ������еķ������ݰ�����������
    bool Make_Send_Packet(uint32 u4ConnectID, const char* pData, uint32 u4Len, ACE_Message_Block* pMbData, uint16 u2CommandID)
    {
        if(NULL == pData || NULL == pMbData || 0 == u2CommandID)
        {
            return false;
        }

        PACKETPARSE_SHOW_BEGIN
        OUR_DEBUG((LM_INFO, "[CPacketParse::Make_Send_Packet]Make_Send_Packet=%d,u4Len=%d.\n", u4ConnectID, u4Len));
        PACKETPARSE_SHOW_END

        //ƴװ���ݰ�
        memcpy_safe((char* )&u4Len, (uint32)sizeof(uint32), pMbData->wr_ptr(), (uint32)sizeof(uint32));
        pMbData->wr_ptr(sizeof(uint32));
        memcpy_safe((char* )pData, u4Len, pMbData->wr_ptr(), u4Len);
        pMbData->wr_ptr(u4Len);

        return true;
    }

    //�õ��������ݰ��ĳ���
    uint32 Make_Send_Packet_Length(uint32 u4ConnectID, uint32 u4DataLen, uint16 u2CommandID)
    {
        if (0 == u2CommandID)
        {
            return 0;
        }

        PACKETPARSE_SHOW_BEGIN
        OUR_DEBUG((LM_INFO, "[CPacketParse::Make_Send_Packet_Length]Make_Send_Packet=%d,u4DataLen=%d.\n", u4ConnectID, u4DataLen));
        PACKETPARSE_SHOW_END

        return u4DataLen + sizeof(uint32);
    }

    //�����ӵ�һ�ν�����ʱ�򣬷��صĽӿ��������Լ��Ĵ�����
    bool Connect(uint32 u4ConnectID, _ClientIPInfo objClientIPInfo, _ClientIPInfo const& objLocalIPInfo)
    {
        PACKETPARSE_SHOW_BEGIN
        OUR_DEBUG((LM_INFO, "[CPacketParse::Connect]u4ConnectID=%d,objClientIPInfo=(%s:%d),objLocalIPInfo=(%s:%d).\n",
                   u4ConnectID,
                   objClientIPInfo.m_szClientIP,
                   objClientIPInfo.m_u2Port,
                   objLocalIPInfo.m_szClientIP,
                   objLocalIPInfo.m_u2Port));
        return false;
        PACKETPARSE_SHOW_END

        return true;
    }

    //�����ӶϿ���ʱ�򣬷������Լ��Ĵ���
    void DisConnect(uint32 u4ConnectID)
    {
        PACKETPARSE_SHOW_BEGIN
        OUR_DEBUG((LM_INFO, "[CPacketParse::Connect]u4ConnectID=%d\n", u4ConnectID));
        PACKETPARSE_SHOW_END
    }

    //����˳���ʱ�����
    void Close()
    {
        OUR_DEBUG((LM_INFO, "[CPacketParse::Close]exit.\n"));
    }
}