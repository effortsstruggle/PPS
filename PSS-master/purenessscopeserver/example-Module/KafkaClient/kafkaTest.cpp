// Base.cpp : 一个TCP的测试类
//用于PurenessScopeServer的测试和使用
//add by freeeyes
//2011-09-20

#include "BaseCommand.h"
#include "IObject.h"

#include "ace/svc_export.h"

#ifdef WIN32
#ifdef TEST_TCP_BUILD_DLL
#define DECLDIR __declspec(dllexport)
#else
#define DECLDIR __declspec(dllimport)
#endif
#else
#define DECLDIR ACE_Svc_Export
#endif

static const char* g_szDesc      = "kafka Client Logic"; //模块的描述文字
static const char* g_szName      = "kafkaClient";        //模块的名字
static const char* g_szModuleKey = "kafkaClient";        //模块的Key

extern "C"
{
    DECLDIR int LoadModuleData(CServerObject* pServerObject);
    DECLDIR int UnLoadModuleData();
    DECLDIR const char* GetDesc();
    DECLDIR const char* GetName();
    DECLDIR const char* GetModuleKey();
    DECLDIR int DoModuleMessage(uint16 u2CommandID, IBuffPacket* pBuffPacket, IBuffPacket* pReturnBuffPacket);
    DECLDIR bool GetModuleState(uint32& u4ErrorID);

}

static CBaseCommand* g_pBaseCommand = NULL;
CServerObject*       g_pServerObject = NULL;

int LoadModuleData(CServerObject* pServerObject)
{
    g_pServerObject = pServerObject;
    OUR_DEBUG((LM_INFO, "[Base LoadModuleData] Begin.\n"));

    if(NULL != g_pBaseCommand)
    {
        SAFE_DELETE(g_pBaseCommand);
    }

    g_pBaseCommand = new CBaseCommand();

    if(g_pServerObject != NULL)
    {
        g_pBaseCommand->SetServerObject(pServerObject);
    }
    else
    {
        OUR_DEBUG((LM_INFO, "[Base LoadModuleData] pServerObject is NULL.\n"));
    }

    IMessageManager* pMessageManager = g_pServerObject->GetMessageManager();

    if(NULL != pMessageManager)
    {
        //纯服务模块，挂载zookeeper客户端相关操作。不接收相关外部指令
    }
    else
    {
        OUR_DEBUG((LM_INFO, "[Base LoadModuleData] pMessageManager = NULL.\n"));
    }

    g_pBaseCommand->Init(pServerObject->GetModuleInfo()->GetModuleParam(g_szName));
    OUR_DEBUG((LM_INFO, "[Base LoadModuleData] End.\n"));

    return 0;
}

int UnLoadModuleData()
{
    OUR_DEBUG((LM_INFO, "[Base UnLoadModuleData] Begin.\n"));

    if(g_pServerObject != NULL)
    {
        IMessageManager* pMessageManager = g_pServerObject->GetMessageManager();

        if(NULL != pMessageManager)
        {
            //纯服务模块
            pMessageManager = NULL;
        }
    }

    g_pBaseCommand->Fini();
    SAFE_DELETE(g_pBaseCommand);
    OUR_DEBUG((LM_INFO, "[Base UnLoadModuleData] End.\n"));
    return 0;
}

const char* GetDesc()
{
    return g_szDesc;
}

const char* GetName()
{
    return g_szName;
}

const char* GetModuleKey()
{
    return g_szModuleKey;
}

//用于模块间的调用接口
int DoModuleMessage(uint16 u2CommandID, IBuffPacket* pBuffPacket, IBuffPacket* pReturnBuffPacket)
{
    if(0 == u2CommandID || NULL == pBuffPacket || NULL == pReturnBuffPacket)
    {
        return 1;
    }

    return 0;
}

//交给框架使用，用于框架定时巡检插件状态
//默认这里返回true，如果你需要对框架内部细节做监控。
//请在这里实现之，根据错误的不同，返回false的同时，设置ErrorID
//框架会根据这个设置，发送邮件给指定的邮箱
bool GetModuleState(uint32& u4ErrorID)
{
    ACE_Time_Value tvNow = ACE_OS::gettimeofday();

    u4ErrorID = 0;
    return true;
}


