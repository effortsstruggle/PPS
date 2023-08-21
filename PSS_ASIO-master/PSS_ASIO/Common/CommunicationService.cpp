#include "CommunicationService.h"

/**
 * @brief init_communication_service 初始化通信服务
 * @param io_service_context 
 * @param timeout_seconds 
*/
void CCommunicationService::init_communication_service(asio::io_context* io_service_context, uint16 timeout_seconds)
{
    //读取配置文件，链接服务器
    io_service_context_ = io_service_context;

    //定时检查任务，检查服务器间链接的状态。（ 程序运行期间，一直检测该会话是否处于链接状态 ） 心跳机制
    App_TimerManager::instance()->GetTimerPtr()->addTimer_loop(  chrono::seconds(0) , chrono::seconds(timeout_seconds) , 
                                                                [this]()
                                                                {
                                                                    //添加到数据队列处理
                                                                    App_tms::instance()->AddMessage( 0 ,[this]() 
                                                                                                        {
                                                                                                            this->run_check_task();
                                                                                                        }
                                                                                                    );
                                                                }
                                                            );
}

/**
 * @brief 添加服务器链接
 * @param io_info 
 * @param io_type 
 * @return 
*/
bool CCommunicationService::add_connect(const CConnect_IO_Info& io_info, EM_CONNECT_IO_TYPE io_type)
{
    std::lock_guard <std::recursive_mutex> lock(mutex_);
    CCommunicationIOInfo connect_info;
    connect_info.connect_id_ = 0;
    connect_info.io_info_ = io_info;
    connect_info.io_type_ = io_type;

    this->io_connect( connect_info );

    return true;
}

void CCommunicationService::set_connect_id(uint32 server_id, uint32 connect_id)
{
    auto f = communication_list_.find(server_id);
    
    if (f != communication_list_.end())
    {
        
        f->second.connect_id_ = connect_id;

        if ( connect_id == 0 )
        {
            f->second.session_ = nullptr;

            //删除映射关系
            this->server_connect_id_list_.erase(connect_id);
        }
        else
        {
            //添加映射关系
            this->server_connect_id_list_[connect_id] = server_id;
        }
    }
}

/**
 * @brief io_connect 建立IO链接，并接收数据
 * @param connect_info 
*/
void CCommunicationService::io_connect( CCommunicationIOInfo& connect_info )
{
<<<<<<< Updated upstream
    communication_list_[ connect_info.io_info_.server_id ] = connect_info;
    
=======
    this->communication_list_[  connect_info.io_info_.server_id  ] = connect_info;
   
>>>>>>> Stashed changes
    if (false == communication_is_run_)
    {
        //还在初始化中，不启动链接
        return;
    }

    if ( connect_info.io_type_ == EM_CONNECT_IO_TYPE::CONNECT_IO_TCP )
    {
        //IO是TCP
        auto tcp_client_session = make_shared<CTcpClientSession>(io_service_context_);
		//建立网络链接
		if (true == tcp_client_session->start(connect_info.io_info_))
		{
            //存储通信类
            this->communication_list_[connect_info.io_info_.server_id].session_ = tcp_client_session;
		}
	}
    else if(connect_info.io_type_ == EM_CONNECT_IO_TYPE::CONNECT_IO_UDP)
    {
        //IO是UDP
        auto udp_client_session = make_shared<CUdpClientSession>(io_service_context_);
        
        udp_client_session->start(connect_info.io_info_);

        //存储通信类
        this->communication_list_[ connect_info.io_info_.server_id ].session_ = udp_client_session;
    }
    else if (connect_info.io_type_ == EM_CONNECT_IO_TYPE::CONNECT_IO_TTY)
    {
        //IO是TTY
		auto tty_client_session = make_shared<CTTyServer>(
			connect_info.io_info_.packet_parse_id,
			connect_info.io_info_.recv_size,
			connect_info.io_info_.send_size
			);

        //开启串口，接收数据
		tty_client_session->start(
			io_service_context_,
			connect_info.io_info_.server_ip,
			connect_info.io_info_.server_port,
			8,
			connect_info.io_info_.server_id
		);

        //存储通信类
       this->communication_list_[connect_info.io_info_.server_id].session_ = tty_client_session;
    }
    else if (connect_info.io_type_ == EM_CONNECT_IO_TYPE::CONNECT_IO_SSL)
    {
#ifdef SSL_SUPPORT
        auto ssl_client_session = make_shared<CTcpSSLClientSession>(io_service_context_);
        
        ssl_client_session->start(connect_info.io_info_);

       this->communication_list_[connect_info.io_info_.server_id].session_ = ssl_client_session;
#else
        PSS_LOGGER_DEBUG("[CCommunicationService::io_connect]you mest use SSL_SUPPORT macro support ths ssl.");
#endif
    }
}

void CCommunicationService::run_server_to_server()
{
    //启动链接
    this->communication_is_run_ = true;

    this->run_check_task();
}

void CCommunicationService::close_connect(uint32 server_id)
{
    std::lock_guard <std::recursive_mutex> lock(mutex_);
    communication_list_.erase(server_id);

    auto connect_id = get_server_id(server_id);
    if (connect_id > 0)
    {
        server_connect_id_list_.erase(connect_id);
    }
}

bool CCommunicationService::is_exist(uint32 server_id)
{
    std::lock_guard <std::recursive_mutex> lock(mutex_);
    auto f = communication_list_.find(server_id);
    if (f != communication_list_.end())
    {
        return true;
    }
    else
    {
        return false;
    }
}

/**
 * @brief each 转发接口，为每个IO通信请求链接
 * @param communication_funtion 
*/
void CCommunicationService::each(Communication_funtion communication_funtion)
{
    for (auto& client_info : this->communication_list_)
    {
        communication_funtion( client_info.second );
    }
}

/**
 * @brief run_check_task 运行检测任务
*/
void CCommunicationService::run_check_task()
{
    //lock_guard自身作用域中，具有构造时加锁，析构时释放锁
    std::lock_guard <std::recursive_mutex> lock(mutex_);

    PSS_LOGGER_DEBUG("[CCommunicationService::run_check_task]begin size={}.", communication_list_.size());

<<<<<<< Updated upstream
    each(  
            [this]( CCommunicationIOInfo& io_info ) {
                if ( io_info.session_ == nullptr || false == io_info.session_->is_connect() )
                {
                    //重新建立链接
                    this->io_connect(io_info);
                }
            }
        );
=======
    this->each(  
                        [this]( CCommunicationIOInfo& io_info) {
                            if ( io_info.session_ == nullptr || false == io_info.session_->is_connect() )
                            {
                                //重新建立链接
                                this->io_connect(io_info);
                            }
                        }
            );
>>>>>>> Stashed changes

    PSS_LOGGER_DEBUG("[CCommunicationService::run_check_task]end.");
}

void CCommunicationService::close()
{
    std::lock_guard <std::recursive_mutex> lock(mutex_);

    this->server_connect_id_list_.clear();
    this->communication_list_.clear();
}

uint32 CCommunicationService::get_server_id(uint32 connect_id)
{
    std::lock_guard <std::recursive_mutex> lock(mutex_);
    auto f = server_connect_id_list_.find(connect_id);
    if (f != server_connect_id_list_.end())
    {
        return f->second;
    }
    else
    {
        return 0;
    }
}


/**
 * @brief 重新链接
 * @param server_id 
*/
void CCommunicationService::reset_connect(uint32 server_id)
{
    std::lock_guard <std::recursive_mutex> lock(mutex_);
    auto f = communication_list_.find(server_id);
    if (f != communication_list_.end())
    {
        this->io_connect( f->second );
    }
}
