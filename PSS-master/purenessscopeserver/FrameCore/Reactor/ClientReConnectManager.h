#ifndef _CLIENTCONNECTMANAGER_H
#define _CLIENTCONNECTMANAGER_H

#include "ace/Connector.h"
#include "ace/SOCK_Connector.h"

#include "TimerManager.hpp"
#include "BaseClientConnectManager.h"
#include "ReactorUDPClient.h"
#include "HashTable.h"
#include "XmlConfig.h"
#include "ConnectClient.h"
#include <unordered_map>

const uint32 RE_CONNECT_SERVER_TIMEOUT = 100 * 1000;
const uint32 WAIT_FOR_RECONNECT_FINISH = 5000;

using CConnectClientConnector = ACE_Connector<CConnectClient, ACE_SOCK_CONNECTOR>;

class CReactorClientInfo
{
public:
    CReactorClientInfo()  = default;

    bool Init(int nServerID, const char* pIP, uint16 u2Port, uint8 u1IPType, CConnectClientConnector* pReactorConnect, IClientMessage* pClientMessage, ACE_Reactor* pReactor, uint32 u4PacketParseID);  //��ʼ�����ӵ�ַ�Ͷ˿�
    void SetLocalAddr(const char* pIP, uint16 u2Port, uint8 u1IPType);                         //�󶨱��ص�IP�Ͷ˿�
    bool Run(bool blIsReady, EM_Server_Connect_State emState = EM_Server_Connect_State::SERVER_CONNECT_RECONNECT);  //��ʼ����
    bool SendData(ACE_Message_Block* pmblk);                                               //��������
    int  GetServerID() const;                                                              //�õ�������ID
    bool Close();                                                                          //�رշ���������
    void SetConnectClient(CConnectClient* pConnectClient);                                 //��������״̬
    CConnectClient* GetConnectClient();                                                    //�õ�ProConnectClientָ��
    IClientMessage* GetClientMessage();                                                    //��õ�ǰ����Ϣ����ָ��
    ACE_INET_Addr GetServerAddr() const;                                                   //��÷������ĵ�ַ
    EM_Server_Connect_State GetServerConnectState() const;                                 //�õ���ǰ����״̬
    void SetServerConnectState(EM_Server_Connect_State objState);                          //���õ�ǰ����״̬
    void SetPacketParseID(uint32 u4PacketParseID);                                         //���ý�����ID
    uint32 GetPacketParseID() const;                                                       //��ý�����ID

private:
    ACE_INET_Addr              m_AddrLocal;                               //���ص����ӵ�ַ������ָ����
    ACE_INET_Addr              m_AddrServer;                              //Զ�̷������ĵ�ַ
    CConnectClient*            m_pConnectClient  = nullptr;               //��ǰ���Ӷ���
    CConnectClientConnector*   m_pReactorConnect = nullptr;               //Connector���Ӷ���
    IClientMessage*            m_pClientMessage  = nullptr;               //�ص������࣬�ص����ش���ͷ������ݷ���
    uint32                     m_u4PacketParseID = 0;                     //�����õ�PacketParseID
    ACE_Reactor*               m_pReactor        = nullptr;               //��¼ʹ�õķ�Ӧ��
    bool                       m_blIsLocal       = false;                 //�Ƿ���Ҫ�ƶ����ض˿�
    int                        m_nServerID       = 0;                     //Զ�̷�������ID
    EM_Server_Connect_State    m_emConnectState  = EM_Server_Connect_State::SERVER_CONNECT_READY;  //����״̬
};

class CClientReConnectManager : public IClientManager
{
public:
    CClientReConnectManager(void) = default;

    bool Init(ACE_Reactor* pReactor);
    bool Connect(int nServerID, const char* pIP, uint16 u2Port, uint8 u1IPType, IClientMessage* pClientMessage, uint32 u4PacketParseID) final;                                                                 //���ӷ�����(TCP)
    bool Connect(int nServerID, const char* pIP, uint16 u2Port, uint8 u1IPType, const char* pLocalIP, uint16 u2LocalPort, uint8 u1LocalIPType, IClientMessage* pClientMessage, uint32 u4PacketParseID) final;  //���ӷ�����(TCP)��ָ�����ص�ַ

    bool ConnectFrame(int nServerID, const char* pIP, uint16 u2Port, uint8 u1IPType, uint32 u4PacketParseID) final;                //����ָ���ķ�������������PacketParseID
    bool ConnectFrame(int nServerID, const char* pIP, uint16 u2Port, uint8 u1IPType, const char* pLocalIP, uint16 u2LocalPort, uint8 u1LocalIPType, uint32 u4PacketParseID) final;    //����ָ���ķ�������������PacketParseID

    bool ConnectUDP(int nServerID, const char* pIP, uint16 u2Port, uint8 u1IPType, EM_UDP_TYPE emType, IClientUDPMessage* pClientUDPMessage) final;                                //����һ��ָ��UDP�����ӣ�UDP��
    bool ReConnect(int nServerID);                                                                                                  //��������һ��ָ���ķ�����(TCP)
    bool CloseByClient(int nServerID);                                                                                              //Զ�̱����ر�(TCP)
    bool Close(int nServerID) final;                                                                                                //�ر�����
    bool CloseUDP(int nServerID) final;                                                                                             //�ر����ӣ�UDP��
    bool ConnectErrorClose(int nServerID);                                                                                          //���ڲ����������ʧ�ܣ���ProConnectClient����
    bool SendData(int nServerID, char*& pData, int nSize, bool blIsDelete = true) final;                                            //��������
    bool SendDataUDP(int nServerID, const char* pIP, uint16 u2Port, char*& pMessage, uint32 u4Len, bool blIsDelete = true) final;   //�������ݣ�UDP��
    bool SetHandler(int nServerID, CConnectClient* pConnectClient);                                                                 //��ָ����CProConnectClient*�󶨸�nServerID
    IClientMessage* GetClientMessage(int nServerID) final;                                                                          //���ClientMessage����
    bool StartConnectTask(uint16 u2IntervalTime = CONNECT_LIMIT_RETRY) final;                                                           //�����Զ������Ķ�ʱ��
    void CancelConnectTask() final;                                                                                                 //�ر�������ʱ��
    void Close() final;                                                                                                             //�ر���������
    ACE_INET_Addr GetServerAddr(int nServerID);                                                                                     //�õ�ָ����������Զ�̵�ַ������Ϣ
    bool SetServerConnectState(int nServerID, EM_Server_Connect_State objState);                                                    //����ָ�����ӵ�����״̬
    bool GetServerIPInfo(int nServerID, _ClientIPInfo& objServerIPInfo) final;                                                      //�õ�һ��nServerID��Ӧ��ServerIP��Ϣ
    bool DeleteIClientMessage(IClientMessage* pClientMessage) final;                                                                //ɾ��һ���������ڽ�����IClientMessage

    void GetConnectInfo(vecClientConnectInfo& VecClientConnectInfo);      //���ص�ǰ������ӵ���Ϣ��TCP��
    void GetUDPConnectInfo(vecClientConnectInfo& VecClientConnectInfo);   //���ص�ǰ������ӵ���Ϣ��UDP��
    EM_Server_Connect_State GetConnectState(int nServerID) final;         //�õ�һ����ǰ����״̬
    uint32 GetPacketParseID(int nServerID) final;                         //�õ���ǰ��PacketParseID

    int timer_task(brynet::TimerMgr::Ptr timerMgr);                       //��ʱ����
    void start_new_task(brynet::TimerMgr::Ptr timerMgr);                  //���ö�ʱ��


private:
    shared_ptr<CReactorClientInfo> ConnectTcpInit(int nServerID);
    shared_ptr<CReactorUDPClient> ConnectUdpInit(int nServerID);

public:
    using hashmapClientTCPList = unordered_map<int, shared_ptr<CReactorClientInfo>>;
    using hashmapClientUDPList = unordered_map<int, shared_ptr<CReactorUDPClient>>;
    hashmapClientTCPList            m_objClientTCPList;                            //TCP�ͻ�������
    hashmapClientUDPList            m_objClientUDPList;                            //UDP�ͻ�������
    CConnectClientConnector         m_ReactorConnect;                              //Reactor���ӿͻ��˶���
    ACE_Recursive_Thread_Mutex      m_ThreadWritrLock;                             //�߳���
    ACE_Reactor*                    m_pReactor               = nullptr;            //��ǰ�ķ�Ӧ��
    bool                            m_blReactorFinish        = false;              //Reactor�Ƿ��Ѿ�ע��
    bool                            m_blTimerState           = true;               //��ʱ���Ƿ�����
    uint16                          m_u2ThreadTimeCheck = CONNECT_LIMIT_RETRY;     //��ʱ���ļ��ʱ��
    uint32                          m_u4ConnectServerTimeout = 0;                  //���Ӽ��ʱ��
    uint32                          m_u4MaxPoolCount         = 0;                  //���ӳص�����
    EM_S2S_Run_State                m_emS2SRunState          = EM_S2S_Run_State::S2S_Run_State_Init; //��ǰ��������״̬
    vector<CS2SConnectGetRandyInfo> m_GetReadyInfoList;                            //��Ҫ�ȴ����ӵ�������Ϣ
};

using App_ClientReConnectManager = PSS_singleton<CClientReConnectManager>;
#endif