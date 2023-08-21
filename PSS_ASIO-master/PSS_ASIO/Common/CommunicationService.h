#pragma once

#include "ICommunicationService.h"
#include "TcpClientSession.h"
#include "UdpClientSession.h"
#include "TcpSSLClientSession.h"
#include "TtyServer.h"
#include "tms.hpp"
#include "CoreTimer.hpp"


/**
 * @brief 管理服务器间链接，自动重连的类 （心跳机制，使用定时器实现）
*/
//add by freeeyes

/**
 * @brief  CCommunicationIOInfo IO通信结构
*/
class CCommunicationIOInfo
{
public:
    std::shared_ptr<ISession> session_;
    
    uint32 connect_id_; //连接ID
    
    CConnect_IO_Info io_info_; //链接信息

    EM_CONNECT_IO_TYPE io_type_; //连接IO的类型

};

using Communication_funtion = std::function<void(CCommunicationIOInfo&)>;

/**
 * @brief 服务器间通信
*/
class CCommunicationService : public ICommunicationInterface
{
public:
    ~CCommunicationService() final = default;

    void init_communication_service(asio::io_context* io_service_context, uint16 timeout_seconds) final;

    bool add_connect(const CConnect_IO_Info& io_info, EM_CONNECT_IO_TYPE io_type) final;

    void set_connect_id(uint32 server_id, uint32 connect_id) final;

    void close_connect(uint32 server_id) final;

    bool is_exist(uint32 server_id) final;

    void close() final;

    uint32 get_server_id(uint32 connect_id) final;

    void reset_connect(uint32 server_id) final;

    void run_check_task();

    void io_connect(CCommunicationIOInfo& connect_info);

    void run_server_to_server();

    void each(Communication_funtion communication_funtion);

private:
    using communication_list = unordered_map<uint32, CCommunicationIOInfo>; //Hash映射
    using server_connect_id_list = unordered_map<uint32, uint32>; //Hash映射
    communication_list communication_list_;  //[服务器ID ，IO通信类]
    server_connect_id_list server_connect_id_list_;//[连接ID , 服务器ID]

    std::recursive_mutex mutex_; //嵌套锁/递归锁（可多次上锁，来获得对互斥量对象的多层使用权；释放量时，需要调用与该lock层次相同的unlock）
    
    asio::io_context* io_service_context_ = nullptr; //上下文环境

    bool communication_is_run_ = false; //记录是否启动链接
};

using App_CommunicationService = PSS_singleton<CCommunicationService>;

