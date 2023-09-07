#include "ServerService.h"

#if PSS_PLATFORM == PLATFORM_WIN

BOOL WINAPI ConsoleHandlerRoutine(DWORD dwCtrlType)
{
    //CTRL_CLOSE_EVENT 窗口关闭事件 及 Ctrl+C 事件
    if (dwCtrlType == CTRL_CLOSE_EVENT || dwCtrlType == CTRL_C_EVENT)
    {
        //在这里处理窗口服务器关闭回收资源
        PSS_LOGGER_DEBUG("[ConsoleHandlerRoutine]server is close.");
        App_ServerService::instance()->stop_service();
        return TRUE;
    }

    return FALSE;
}

#endif

#if PSS_PLATFORM == PLATFORM_UNIX

inline void daemonize()
{
    pid_t pid;

    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    if (setpgrp() == -1)
    {
        perror("setpgrp failure");
    }

    signal(SIGHUP, SIG_IGN);

    if ((pid = fork()) < 0)
    {
        perror("fork failure");
        exit(1);
    }
    else if (pid > 0)
    {
        exit(0);
    }

    setsid();

    signal(SIGCLD, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

}

#endif

/**
* @breif: init_servce 初始化服务器根据配置文件
* @author: wq
* @data: 2023/08/08
* @param1: pss_config_file_name  配置文件名称（默认为 server_config.json）
* @return: bool 初始化是否成功 
*/
bool CServerService::init_servce(const std::string& pss_config_file_name)
{
    //指定当前目录，防止访问文件失败
    #if PSS_PLATFORM == PLATFORM_WIN
        TCHAR szFileName[ MAX_PATH ] = { 0 };
        //获取模块文件名
        ::GetModuleFileName( 0 , szFileName , MAX_PATH ) ;

        LPTSTR pszEnd = _tcsrchr( szFileName , TEXT('\\') );

        if ( pszEnd != 0)
        {
            pszEnd++;
            *pszEnd = 0;
        }
    #endif

    //读取配置文件
    if ( false == App_ServerConfig::instance()->read_server_config_file(pss_config_file_name) )
    {
        PSS_LOGGER_DEBUG("[CServerService::init_servce] configure file {0} read fail. ", pss_config_file_name);
        return false;
    }

    PSS_LOGGER_DEBUG("[CServerService::init_servce] configure file {0} read ok. ", pss_config_file_name );

    #if PSS_PLATFORM == PLATFORM_UNIX
        if (App_ServerConfig::instance()->get_config_workthread().linux_daemonize_ != 0)
        {
            //Linux 开启守护
            daemonize();
        }
    #endif

    const auto& config_output = App_ServerConfig::instance()->get_config_console() ;
    Init_Console_Output(  config_output.file_output_,
						  config_output.file_count_,
						  config_output.max_file_size_,
						  config_output.file_name_,
						  config_output.output_level_
						);

    //初始化PacketParse插件，将“动态库加载到内存中”存起来
    for ( const auto&  packet_parse  :  App_ServerConfig::instance()->get_config_packet_list() )
    {
        bool is_load_success_ = false; 
        is_load_success_ = App_PacketParseLoader::instance()->LoadPacketInfo( packet_parse.packet_parse_id_ ,  packet_parse.packet_parse_path_ , packet_parse.packet_parse_file_name_);
        if (false == is_load_success_)
        {
            PSS_LOGGER_DEBUG("[CServerService::init_servce] load error.");
        }
    }

    #if PSS_PLATFORM == PLATFORM_WIN
        ::SetConsoleCtrlHandler(ConsoleHandlerRoutine, TRUE);
    #endif

    //注册监控中断事件(LINUX)
    asio::signal_set  signals( this->io_context_ , SIGINT , SIGTERM );

    signals.async_wait
    ( 
        [this] (std::error_code ec, int)
        {
            PSS_LOGGER_DEBUG("[ CServerService::init_servce] server is error({0}) . ", ec.message());
            this->io_context_.stop();
        }
    );

     //测试记录二进制
    #ifdef GCOV_TEST
        char test_buffer[20] = { "freeeyes" };
        pss_output_binary( test_buffer ,  0  , 3 );
    #endif
    
    //初始化框架定时器 ,并开启定时器线程
    App_TimerManager::instance()->Start();

    //初始化服务器间IO通信服务
    uint16 timeout_ = (uint16)App_ServerConfig::instance()->get_config_workthread().s2s_timeout_seconds_ ;
    App_CommunicationService::instance()->init_communication_service( &this->io_context_ , timeout_ );
    App_WorkThreadLogic::instance()->init_communication_service(  App_CommunicationService::instance() );

    //初始化执行库
    CConfigWorkThread& config_work_thread_ = App_ServerConfig::instance()->get_config_workthread();
    App_WorkThreadLogic::instance()->init_work_thread_logic(
                                                                config_work_thread_.work_thread_count_ , 
                                                                (uint16)config_work_thread_.work_timeout_seconds_  ,
                                                                (uint32)config_work_thread_.client_connect_timeout_  ,
                                                                (uint16)config_work_thread_.io_send_time_check_  ,
                                                                App_ServerConfig::instance()->get_config_logic_list()  , 
                                                                App_SessionService::instance()
                                                            );


    //加载Tcp监听
    for( auto tcp_server : App_ServerConfig::instance()->get_config_tcp_list() )
    {
        if (tcp_server.ssl_server_password_ != "" && tcp_server.ssl_server_pem_file_ != ""  && tcp_server.ssl_dh_pem_file_ != "" ) //SSL
        {
        #ifdef SSL_SUPPORT
            auto tcp_ssl_service = std::make_shared< CTcpSSLServer >(  io_context_,
																	tcp_server.ip_,
																	tcp_server.port_,
																	tcp_server.packet_parse_id_,
																	tcp_server.recv_buff_size_,
																	tcp_server.ssl_server_password_,
																	tcp_server.ssl_server_pem_file_,
																	tcp_server.ssl_dh_pem_file_ 
																);

            this->tcp_ssl_service_list_.emplace_back(tcp_ssl_service);
        #else
            PSS_LOGGER_DEBUG("[CServerService::init_servce]you must set SSL_SUPPORT macro on compilation options.");
        #endif
        }
        else //TCP
        {
            //正常的tcp链接
            auto tcp_service = std::make_shared< CTcpServer >(
                                                                io_context_ ,
                                                                tcp_server.ip_ ,
                                                                tcp_server.port_ ,
                                                                tcp_server.packet_parse_id_ ,
                                                                tcp_server.recv_buff_size_ 
                                                            );

            this->tcp_service_list_.emplace_back( tcp_service );
        }
    }

    //加载UDP监听
    for (auto udp_server : App_ServerConfig::instance()->get_config_udp_list())
    {
        auto udp_service = std::make_shared< CUdpServer >(
                                                            io_context_, 
                                                            udp_server.ip_,
                                                            udp_server.port_,
                                                            udp_server.packet_parse_id_,
                                                            udp_server.recv_buff_size_,
                                                            udp_server.send_buff_size_,
                                                            udp_server.em_net_type_
                                                        );
        udp_service->start();

        this->udp_service_list_.emplace_back( udp_service );
    }

    //加载KCP监听
    for (auto kcp_server : App_ServerConfig::instance()->get_config_kcp_list())
    {
        auto kcp_service = std::make_shared<CKcpServer>(
                                                            io_context_,
                                                            kcp_server.ip_,
                                                            kcp_server.port_,
                                                            kcp_server.packet_parse_id_,
                                                            kcp_server.recv_buff_size_,
                                                            kcp_server.send_buff_size_
                                                        );
        kcp_service->start();

        this->kcp_service_list_.emplace_back(kcp_service);
    }

    //加载tty监听
    for (auto tty_server : App_ServerConfig::instance()->get_config_tty_list())
    {
        auto tty_service = make_shared<CTTyServer>(
                                                    tty_server.packet_parse_id_,
                                                    tty_server.recv_buff_size_,
                                                    tty_server.send_buff_size_
                                                  );
        tty_service->start( 
                            &io_context_, 
                            tty_server.tty_name_, 
                            (uint16)tty_server.tty_port_,
                            (uint8)tty_server.char_size_,
                            0 
                          );
        this->tty_service_list_.emplace_back(tty_service);
    }

    //启动服务器间的链接
    App_CommunicationService::instance()->run_server_to_server();

    //开始运行
    io_context_.run();


    PSS_LOGGER_DEBUG(" [ CServerService::init_servce ] server is over. ");
    this->close_service();

    return true;
}

/**
 * @brief close_service 关闭服务
*/
void CServerService::close_service()
{
    PSS_LOGGER_DEBUG("[CServerService::close_service]begin.");

    //关闭框架定时器
    App_TimerManager::instance()->Close();

    //停止服务间消息队列数据接收
    App_QueueSessionManager::instance()->close();

    //停止所有的TCP监听(TCP)
    for (const auto& tcp_service : tcp_service_list_)
    {
        tcp_service->close();
    }

#ifdef SSL_SUPPORT
    //停止所有的SSL监听
    for (const auto& tcp_ssl_service : tcp_ssl_service_list_)
    {
        tcp_ssl_service->close();
    }
#endif

    //清理所有kcp资源
    for (const auto& kcp_service : kcp_service_list_)
    {
        kcp_service->close_all();
    }

    this->tcp_service_list_.clear();

    App_SessionService::instance()->close();

    App_WorkThreadLogic::instance()->close();

    App_PacketParseLoader::instance()->close();

    PSS_LOGGER_DEBUG("[CServerService::close_service]end.");
}

void CServerService::stop_service()
{
    //停止，回收清理
    io_context_.stop();
}
