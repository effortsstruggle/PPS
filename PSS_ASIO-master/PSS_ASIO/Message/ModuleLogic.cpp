﻿#include "ModuleLogic.h"

void CModuleLogic::init_logic(const command_to_module_function& command_to_module_function, uint16 work_thread_id)
{
    this->modules_interface_.copy_from_module_list( command_to_module_function );

    this->work_thread_id_ = work_thread_id;
}

void CModuleLogic::add_session(uint32 connect_id, shared_ptr<ISession> session, const _ClientIPInfo& local_info, const _ClientIPInfo& romote_info)
{
    this->sessions_interface_.add_session_interface(connect_id, session, local_info, romote_info);
}

shared_ptr<ISession> CModuleLogic::get_session_interface(uint32 connect_id)
{
    auto ret = this->sessions_interface_.get_session_interface( connect_id );

#ifdef GCOV_TEST
    auto local_ip = sessions_interface_.get_session_local_ip(connect_id);
    PSS_LOGGER_DEBUG("[CModuleLogic::get_session_interface]local IP={0}:{1}",
        local_ip.m_strClientIP,
        local_ip.m_u2Port);

    auto remote_ip = sessions_interface_.get_session_remote_ip(connect_id);
    PSS_LOGGER_DEBUG("[CModuleLogic::get_session_interface]remote IP={0}:{1}",
        remote_ip.m_strClientIP,
        remote_ip.m_u2Port);

    uint32 connect_timeout = 30;
    check_session_io_timeout(connect_timeout);
    get_work_thread_timeout();
#endif
    return ret;
}

void CModuleLogic::delete_session_interface(uint32 connect_id)
{
    sessions_interface_.delete_session_interface(connect_id);
}

/**
 * @brief close 
*/
void CModuleLogic::close()
{
    this->modules_interface_.close();
}

/**
 * @brief do_thread_module_logic 执行逻辑（执行插件加载进来的命令）
 * @param source 
 * @param recv_packet 
 * @param send_packet 
 * @return 
*/
int CModuleLogic::do_thread_module_logic(const CMessage_Source& source, std::shared_ptr<CMessage_Packet> recv_packet, std::shared_ptr<CMessage_Packet> send_packet)
{
    this->last_dispose_command_id_ = recv_packet->command_id_;

    this->work_thread_state_ = ENUM_WORK_THREAD_STATE::WORK_THREAD_BEGIN;
    
    //处理插件加载进来的命令
    auto ret = this->modules_interface_.do_module_message( source ,  recv_packet ,  send_packet );
    
    this->work_thread_state_ = ENUM_WORK_THREAD_STATE::WORK_THREAD_END;
    
    this->work_thread_run_time_ = std::chrono::steady_clock::now();

    return ret;
}

uint16 CModuleLogic::get_work_thread_id() const 
{
    return work_thread_id_;
}

int CModuleLogic::get_work_thread_timeout() const
{
    if(ENUM_WORK_THREAD_STATE::WORK_THREAD_BEGIN == this->work_thread_state_)
    {
        auto interval_seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - this->work_thread_run_time_);
        return (int)interval_seconds.count();
    }
    else
    {
        return 0;
    }
}

void CModuleLogic::check_session_io_timeout(uint32 connect_timeout) const
{
    vector<CSessionIO_Cancel> session_list;
    sessions_interface_.check_session_io_timeout(connect_timeout, session_list);
    for (const auto& session_io : session_list)
    {
        PSS_LOGGER_DEBUG("[CModuleLogic::check_session_io_timeout]work_thread_id_={0}, session_id={1} is timeout.", this->work_thread_id_, session_io.session_id_);
        App_WorkThreadLogic::instance()->close_session_event(session_io.session_id_);
    }
}

uint16 CModuleLogic::get_last_dispose_command_id() const
{
    //返回最后一个处理的命令ID
    return this->last_dispose_command_id_;
}

void CModuleLogic::each_session_id(const session_func& session_fn) const
{
    this->sessions_interface_.each_session_id(session_fn);
}

/**
 * @brief init_work_thread_logic 初始化工作线程逻辑
 * @param thread_count 工作线程数
 * @param timeout_seconds  超时秒数
 * @param connect_timeout  连接超时
 * @param io_send_time_check  IO发生时间检查
 * @param logic_list 业务逻辑列表
 * @param session_service 
*/
void CWorkThreadLogic::init_work_thread_logic(  int thread_count ,  
                                                uint16 timeout_seconds , 
                                                uint32 connect_timeout , 
                                                uint16 io_send_time_check , 
                                                const config_logic_list& logic_list , 
                                                ISessionService* session_service ) 
{
    //初始化线程数
    this->thread_count_ = (uint16)thread_count;

    this->connect_timeout_ = connect_timeout;             
    
    this->io_send_time_check_ = io_send_time_check;

    App_tms::instance()->Init();

    //如果存在发送周期，则启动一个定时器，定时检测发送缓冲
    if ( this->io_send_time_check_ > 0 )
    {
        auto timer_ptr_send_check = App_TimerManager::instance()->GetTimerPtr()->addTimer_loop(  chrono::seconds(1) , chrono::milliseconds(this->io_send_time_check_) , 
                                                                                                                                                                    [this]()
                                                                                                                                                                    {
                                                                                                                                                                        //发送检查和发送数据消息
                                                                                                                                                                        App_tms::instance()->AddMessage(0, 
                                                                                                                                                                            [this]() 
                                                                                                                                                                            {
                                                                                                                                                                                this->send_io_buffer();
                                                                                                                                                                            }
                                                                                                                                                                        );
                                                                                                                                                                    }
                                                                                                                                                             );
    }

    //初始化本地与插件的会话
    this->load_module_.set_session_service( session_service );

    //加载业务逻辑插件列表（将模块接口及模块指令等信息加载到该内存中）
    for ( const auto& logic_library : logic_list )
    {
        //初始化 load_module_ 属性 ( 业务逻辑模块 )
        this->load_module_.load_plugin_module(
                                                logic_library.logic_path_, 
                                                logic_library.logic_file_name_, 
                                                logic_library.logic_param_
                                             );

#ifdef GCOV_TEST
        plugin_work_thread_buffer_message_list_.clear();
        plugin_work_thread_buffer_Func_list_.clear();

        //测试输出和关闭
        PSS_LOGGER_DEBUG("[load_module_] module is exist = {0}", load_module_.get_module_exist(logic_library.logic_file_name_.c_str()));
        PSS_LOGGER_DEBUG("[load_module_] name = {0}", load_module_.get_module_file_path(logic_library.logic_file_name_.c_str()));
        PSS_LOGGER_DEBUG("[load_module_] name = {0}", load_module_.get_module_param(logic_library.logic_file_name_.c_str()));

        //测试卸载模块
        load_module_.unload_plugin_module(logic_library.logic_file_name_, true);

        //测试重新加载模块
        load_module_.load_plugin_module(logic_library.logic_path_,
            logic_library.logic_file_name_,
            logic_library.logic_param_);
#endif
    }

#ifdef GCOV_TEST
    PSS_LOGGER_DEBUG("[load_module_] count = {0}", load_module_.get_module_count());
    vector<std::string> modeinfo_list;
    load_module_.get_all_module_name(modeinfo_list);
    if (modeinfo_list.size() != (uint32)load_module_.get_module_count())
    {
        PSS_LOGGER_DEBUG("[load_module_] count is fail", load_module_.get_module_count());
    }

    //查询指定的消息
    auto module_info = load_module_.find_module_info("libTest_Logic.so");
    if (nullptr == module_info)
    {
        PSS_LOGGER_DEBUG("[load_module_]no find libTest_Logic.so");
    }
#endif

    //显示所有的 注册消息(指令) 以及 对应的模块（处理指令的函数）
    PSS_LOGGER_DEBUG("[ load_module_ ]>>>>>>>>>>>>>>>>>");

    for (const auto& command_info : this->load_module_.get_module_function_list( )  )
    {
        PSS_LOGGER_DEBUG("[load_module_]register command id={0}", command_info.first);
    }

    PSS_LOGGER_DEBUG("[ load_module_ ] >>>>>>>>>>>>>>>>> ");

    //每个“工作线程ID” ， 创建对应的执行“业务逻辑任务”线程
    /**
    *  1. thread_count ： 本地的工作线程ID ， plugin_work_thread_buffer_list_ 插件的工作线程ID
    *  2. 每个"工作线程ID",对应一个“业务逻辑任务”线程
    *  3.“业务逻辑任务”中，维持一个消息队列，用来存储该“业务逻辑任务线程”需要执行的消息
    */
    for (int i = 0 ; i < thread_count ; i++ )
    {
        auto thread_logic = make_shared< CModuleLogic >();
        //绑定 “工作线程ID”与“[模块指令，指令的处理接口]”
        thread_logic->init_logic( this->load_module_.get_module_function_list( ) , (uint16)i ); 

        this->thread_module_list_.emplace_back( thread_logic );

        //初始化线程（thread_count线程，创建thread_count逻辑任务）
        App_tms::instance()->CreateLogic(i);
    }

    //“业务逻辑模块”初始化结束标志
    this->module_init_finish_ = true;



    //创建插件使用的线程 ( 业务逻辑模块未初始化完毕时，以下代码才能起到作用 )
    for ( auto thread_id : this->plugin_work_thread_buffer_list_ )
    {
        //查找线程是否已经存在
        auto f = this->plugin_work_thread_list_.find( thread_id );

        if ( f != this->plugin_work_thread_list_.end() ) //该工作线程ID已存在存在
        {
            continue;
        }

        auto thread_logic = make_shared<CModuleLogic>();
        //绑定 “插件使用的线程ID”与“[模块指令，指令的处理接口]”
        thread_logic->init_logic(  this->load_module_.get_module_function_list() , (uint16)thread_id  );

        this->plugin_work_thread_list_[ thread_id ] = thread_logic;

        //初始化线程
        App_tms::instance()->CreateLogic(thread_id);
    }
    this->plugin_work_thread_buffer_list_.clear();

    //等待10毫秒，让所有线程创建完毕
    std::this_thread::sleep_for(std::chrono::milliseconds(10));


    /**
    *   处理模块加载的指令信息
    */
    //加载插件投递事件(  业务逻辑模块未初始化完毕时，以下代码才能起到作用 ）
    for (const auto& plugin_events : this->plugin_work_thread_buffer_message_list_)
    {
        this->do_frame_message( 
                                plugin_events.tag_thread_id_,
                                plugin_events.message_tag_,
                                plugin_events.send_packet_,
                                plugin_events.delay_timer_ 
                            );

    }
    this->plugin_work_thread_buffer_message_list_.clear();

    //加载插件逻辑(  业务逻辑模块未初始化完毕时，以下代码才能起到作用 )
    for (const auto& plugin_logic : this->plugin_work_thread_buffer_Func_list_)
    {
        this->do_work_thread_logic (
                                                        plugin_logic->tag_thread_id_,
                                                        plugin_logic->delay_timer_,
                                                        plugin_logic->func_
                                                    );
    }
    this->plugin_work_thread_buffer_Func_list_.clear();

    //定时检查任务，定时检查服务器状态
    App_TimerManager::instance()->GetTimerPtr()->addTimer_loop(chrono::seconds(0), chrono::seconds(timeout_seconds), 
                                                                                                            [this, timeout_seconds]()
                                                                                                            {
                                                                                                                //添加到数据队列处理
                                                                                                                App_tms::instance()->AddMessage(0, [this, timeout_seconds] ()
                                                                                                                                                                            {
                                                                                                                                                                                this->run_check_task(timeout_seconds);
                                                                                                                                                                            });
                                                                                                            }
    );
    
}

/**
 * @brief  init_communication_service  初始化IO通信服务
 * @param communicate_service 
*/
void CWorkThreadLogic::init_communication_service( ICommunicationInterface* communicate_service )
{
       this->communicate_service_ = communicate_service;
}

/**
 * @brief close 释放工作线程类的所有资源
*/
void CWorkThreadLogic::close()
{
    if (nullptr != this->communicate_service_)
    {
        this->communicate_service_->close();
    }

    //关闭所有的客户端(异步)
    for ( auto f : this->thread_module_list_ )
    {
        App_tms::instance()->AddMessage(    f->get_work_thread_id(), 
                                                                    [this, f]()
                                                                    {
                                                                        f->each_session_id( 
                                                                                                            [this, f] (uint32 session_id) 
                                                                                                            {
                                                                                                                PSS_LOGGER_DEBUG("[CWorkThreadLogic::close]session_id ({0}) is close", session_id);
                                                                                                                this->close_session_event(session_id);
                                                                                                            }
                                                                                                      );
                                                                     }
        );
    }

    //关闭所有的扩展工作线程
    for (const auto& f : this->plugin_work_thread_list_)
    {
        f.second->close();
    }

    //关闭线程操作
    App_tms::instance()->Close();

    //释放对应模块接口
    for (auto f : this->thread_module_list_)
    {
        f->close();
    }

    this->thread_module_list_.clear();

    //关闭插件操作
    this->load_module_.Close();
}

void CWorkThreadLogic::do_work_thread_frame_events(uint16 command_id , uint32 mark_id , const std::string& remote_ip , uint16 remote_port , EM_CONNECT_IO_TYPE io_type)
{
    auto module_logic = this->thread_module_list_[0];

    CMessage_Source source;
    auto recv_packet = std::make_shared<CMessage_Packet>();
    auto send_packet = std::make_shared<CMessage_Packet>();

    recv_packet->command_id_ = command_id;

    if (recv_packet->command_id_ == LOGIC_CONNECT_SERVER_ERROR)
    {
        source.connect_id_ = 0;
        source.work_thread_id_ = module_logic->get_work_thread_id();
        source.connect_mark_id_ = mark_id;
        source.remote_ip_.m_strClientIP = remote_ip;
        source.remote_ip_.m_u2Port = remote_port;
    }
    else if (recv_packet->command_id_ == LOGIC_LISTEN_SERVER_ERROR)
    {
        source.connect_id_ = 0;
        source.work_thread_id_ = module_logic->get_work_thread_id();
        source.connect_mark_id_ = mark_id;
        source.local_ip_.m_strClientIP = remote_ip;
        source.local_ip_.m_u2Port = remote_port;
    }
    source.type_ = io_type;

    module_logic->do_thread_module_logic(source, recv_packet, send_packet);
}

void CWorkThreadLogic::add_frame_events(uint16 command_id, uint32 mark_id, const std::string& remote_ip, uint16 remote_port, EM_CONNECT_IO_TYPE io_type)
{
    //添加框架通知事件
    App_tms::instance()->AddMessage( 0, 
                                                              [this, command_id, mark_id, remote_ip, remote_port, io_type]() 
                                                              {
                                                                    this->do_work_thread_frame_events(command_id, mark_id, remote_ip, remote_port, io_type);
                                                              }
    );
}

void CWorkThreadLogic::add_thread_session(uint32 connect_id, shared_ptr<ISession> session, const _ClientIPInfo& local_info, const _ClientIPInfo& romote_info)
{
    //session 建立连接
    uint16 curr_thread_index = connect_id % this->thread_count_ ;

    auto module_logic = this->thread_module_list_[ curr_thread_index ];

    auto server_id = session->get_mark_id( connect_id );

    if (server_id > 0)
    {
        //关联服务器间链接
        this->communicate_service_->set_connect_id(server_id, connect_id);
    }

    //向插件告知链接建立消息
    App_tms::instance()->AddMessage(   curr_thread_index, 
                                                                [session, connect_id, module_logic, local_info, romote_info]() 
                                                                {
                                                                    module_logic->add_session( connect_id, session, local_info, romote_info);

                                                                    CMessage_Source source;
                                                                    auto recv_packet = std::make_shared<CMessage_Packet>();
                                                                    auto send_packet = std::make_shared<CMessage_Packet>();

                                                                    recv_packet->command_id_ = LOGIC_COMMAND_CONNECT;

                                                                    source.connect_id_ = connect_id;
                                                                    source.work_thread_id_ = module_logic->get_work_thread_id();
                                                                    source.type_ = session->get_io_type();
                                                                    source.connect_mark_id_ = session->get_mark_id(connect_id);
                                                                    source.local_ip_ = local_info;
                                                                    source.remote_ip_ = romote_info;

                                                                    module_logic->do_thread_module_logic( source , recv_packet, send_packet);

                                                                }
    );
}

void CWorkThreadLogic::delete_thread_session(uint32 connect_id, shared_ptr<ISession> session)
{
    //session 连接断开
    uint16 curr_thread_index = connect_id % thread_count_;
    auto module_logic = thread_module_list_[curr_thread_index];

    auto server_id = session->get_mark_id(connect_id);
    if (server_id > 0)
    {
        //取消服务器间链接
        communicate_service_->set_connect_id(server_id, 0);
    }

    //向插件告知链接断开消息
    App_tms::instance()->AddMessage(curr_thread_index, [session, connect_id, module_logic]() {
        CMessage_Source source;
        auto recv_packet = std::make_shared<CMessage_Packet>();
        auto send_packet = std::make_shared<CMessage_Packet>();

        recv_packet->command_id_ = LOGIC_COMMAND_DISCONNECT;

        source.connect_id_ = connect_id;
        source.work_thread_id_ = module_logic->get_work_thread_id();
        source.type_ = session->get_io_type();
        source.connect_mark_id_ = session->get_mark_id(connect_id);

        module_logic->do_thread_module_logic(source, recv_packet, send_packet);

        //回收链接
        module_logic->delete_session_interface(connect_id);
        });
}

void CWorkThreadLogic::close_session_event(uint32 connect_id)
{
    //session 关闭事件分发
    uint16 curr_thread_index = connect_id % thread_count_;
    auto module_logic = thread_module_list_[curr_thread_index];

    App_tms::instance()->AddMessage(curr_thread_index, [module_logic, connect_id]() {
        auto session = module_logic->get_session_interface(connect_id);
        if (session != nullptr)
        {
            session->close(connect_id);
        }
        });
}

int CWorkThreadLogic::assignation_thread_module_logic(const uint32 connect_id, const vector<shared_ptr<CMessage_Packet>>& message_list, shared_ptr<ISession> session)
{
    //处理线程的投递
    uint16 curr_thread_index = connect_id % thread_count_;
    auto module_logic = thread_module_list_[curr_thread_index];

#ifdef GCOV_TEST
        PSS_LOGGER_DEBUG("[CWorkThreadLogic::assignation_thread_module_logic]({0}) curr_thread_index={1}).", connect_id, curr_thread_index);
#endif
        //添加到数据队列处理
        App_tms::instance()->AddMessage(curr_thread_index, [this, session, connect_id, message_list, module_logic]() {
                //插件逻辑处理
                this->do_work_thread_module_logic( session , connect_id , message_list , module_logic);
            });

#ifdef GCOV_TEST
    //测试连接自检
    uint32 check_timeout = 120;
    run_check_task(check_timeout);
#endif
    return 0;
}

int CWorkThreadLogic::assignation_thread_module_logic_iotoio_error(const uint32 connect_id, const vector<shared_ptr<CMessage_Packet>>& message_list, shared_ptr<ISession> session)
{
    uint16 curr_thread_index = connect_id % thread_count_;
    auto module_logic = thread_module_list_[curr_thread_index];

    vector<shared_ptr<CMessage_Packet>> message_list_iotoio;
    for (auto& recv_packet : message_list)
    {
        recv_packet->command_id_ = LOGIC_IOTOIO_CONNECT_NO_EXIST;
        message_list_iotoio.emplace_back(recv_packet);
    }

    App_tms::instance()->AddMessage(curr_thread_index, [this, session, connect_id, message_list_iotoio, module_logic]() {
        do_work_thread_module_logic(session, connect_id, message_list_iotoio, module_logic);
        });

    return 0;
}

int CWorkThreadLogic::assignation_thread_module_logic_with_events(const uint32 connect_id, const vector<shared_ptr<CMessage_Packet>>& message_list, shared_ptr<ISession> session)
{
    //处理线程的投递(内部消息事件的投递)
    uint16 curr_thread_index = connect_id % thread_count_;
    auto module_logic = thread_module_list_[curr_thread_index];

    //添加到数据队列处理
    App_tms::instance()->AddMessage(curr_thread_index, [this, session, connect_id, message_list, module_logic]() {
        do_work_thread_module_logic(session, connect_id, message_list, module_logic);
        });

    return 0;
}

void CWorkThreadLogic::do_work_thread_module_logic(shared_ptr<ISession> session, const uint32 connect_id, const vector<shared_ptr<CMessage_Packet>>& message_list, shared_ptr<CModuleLogic> module_logic) const
{
    CMessage_Source source;
    CMessage_Packet send_packet;

    source.connect_id_ = connect_id;
    source.work_thread_id_ = module_logic->get_work_thread_id();
    source.type_ = session->get_io_type();
    source.connect_mark_id_ = session->get_mark_id(connect_id);

    //执行接收到的消息 
    for (auto recv_packet : message_list)
    {

        auto curr_send_packet = std::make_shared<CMessage_Packet>();
        module_logic->do_thread_module_logic(source, recv_packet, curr_send_packet);

        if (curr_send_packet->buffer_.size() == 0)
        {
            continue;
        }

        auto io_session = module_logic->get_session_interface(connect_id);
        if (nullptr == io_session)
        {
            continue;
        }

        //在这里添加对curr_send_packet的格式化
        if (io_session->is_need_send_format() == true)
        {
            auto curr_format_send_packet = std::make_shared<CMessage_Packet>();
            if( true == io_session->format_send_packet(source.connect_id_, curr_send_packet, curr_format_send_packet) )
            {
                //将格式化后的数据填充到send_packet
                send_packet.buffer_.append(curr_format_send_packet->buffer_.c_str(), curr_format_send_packet->buffer_.size());
            }
        }
        else
        {
            //将格式化后的数据填充到send_packet
            send_packet.buffer_.append(curr_send_packet->buffer_.c_str(), curr_send_packet->buffer_.size());
        }
    }

    if ( send_packet.buffer_.size() > 0 )
    {
        //有需要发送的内容
        session->set_write_buffer(connect_id, send_packet.buffer_.c_str(), send_packet.buffer_.size());
        session->do_write(connect_id);
    }
}

void CWorkThreadLogic::do_io_message_delivery(uint32 connect_id, std::shared_ptr<CMessage_Packet> send_packet, shared_ptr<CModuleLogic> module_logic)
{
    auto session = module_logic->get_session_interface(connect_id);
    if (nullptr != session)
    {
        //这里调用格式化发送过程
        if (session->is_need_send_format() == true)
        {
            //需要重新格式化数据
            auto format_packet = std::make_shared<CMessage_Packet>();
            if (false == session->format_send_packet(connect_id, send_packet, format_packet))
            {
                return;
            }

            if (io_send_time_check_ > 0)
            {
                session->set_write_buffer(connect_id,
                    format_packet->buffer_.c_str(),
                    format_packet->buffer_.size());
            }
            else
            {
                session->do_write_immediately(connect_id,
                    format_packet->buffer_.c_str(),
                    format_packet->buffer_.size());
            }
        }
        else
        {
            if (io_send_time_check_ > 0)
            {
                session->set_write_buffer(connect_id,
                    send_packet->buffer_.c_str(),
                    send_packet->buffer_.size());
            }
            else
            {
                //不需要格式化数据，直接发送
                session->do_write_immediately(connect_id,
                    send_packet->buffer_.c_str(),
                    send_packet->buffer_.size());
            }
        }
    }
    else
    {
        //查找是不是服务器间链接，如果是，则调用重连。
        auto server_id = communicate_service_->get_server_id(connect_id);
        if (server_id > 0)
        {
            //重连服务器
            communicate_service_->reset_connect(server_id);
        }
    }
}

/**
 * @brief  do_plugin_thread_module_logic 处理插件（客户端）发送的【指令逻辑】
 * @param module_logic 
 * @param message_tag  客户端IP
 * @param recv_packet 
*/
void CWorkThreadLogic::do_plugin_thread_module_logic(std::shared_ptr<CModuleLogic> module_logic, const std::string& message_tag, std::shared_ptr<CMessage_Packet> recv_packet) const
{
    //添加到数据队列处理
    App_tms::instance()->AddMessage(  module_logic->get_work_thread_id()  , 
                                                                [ message_tag , recv_packet , module_logic ]() {

                                                                    auto send_packet = std::make_shared<CMessage_Packet>();
                                        

                                                                    CMessage_Source source;
                                                                    //模块逻辑，对应的工作线程ID
                                                                    source.work_thread_id_ = module_logic->get_work_thread_id();

                                                                    source.remote_ip_.m_strClientIP = message_tag;
                                        
                                                                    source.type_ = EM_CONNECT_IO_TYPE::CONNECT_IO_FRAME; //连接的类型

                                                                    module_logic->do_thread_module_logic( source , recv_packet , send_packet );

                                                                    //内部模块回调不在处理 send_packet 部分。

                                                                }
                                );
}

/**
 * @brief 创建【插件工作线程】，并创建业务逻辑
 * @param thread_id  工作线程ID标识
 * @return 
*/
bool CWorkThreadLogic::create_frame_work_thread(uint32 thread_id)
{
    std::lock_guard <std::recursive_mutex> lock(plugin_timer_mutex_);

    if (thread_id < this->thread_count_ ) //判断是否小于线程数
    {
        PSS_LOGGER_DEBUG("[ CWorkThreadLogic::create_frame_work_thread ] thread id must more than config thread count.");
        return false;
    }

    if ( false == this->module_init_finish_ )  //"默认的业务逻辑模块"是否初始化完成
    {
        //如果模块还没全部启动完毕，将这个创建线程的过程，放入vector里面，等模块全部加载完毕，启动。
        this->plugin_work_thread_buffer_list_.emplace_back(thread_id);
    }
    else
    {
        //查找这个线程ID是否已经存在了
        auto f = this->plugin_work_thread_list_.find( thread_id );

        if (f != plugin_work_thread_list_.end()) //该工作线程ID标识 已存在
        {
            PSS_LOGGER_DEBUG("[CWorkThreadLogic::create_frame_work_thread]thread id already exist.");
            return false;
        }

        //创建线程
        auto thread_logic = make_shared< CModuleLogic >();
        thread_logic->init_logic( this->load_module_.get_module_function_list() , (uint16)thread_id );
        this->plugin_work_thread_list_[ thread_id ] = thread_logic;

        //初始化线程
        App_tms::instance()->CreateLogic(thread_id);
    }

    return true;
}

bool CWorkThreadLogic::close_frame_work_thread(uint32 thread_id)
{
    std::lock_guard <std::recursive_mutex> lock(plugin_timer_mutex_);

    //不能结束工作线程
    if (thread_id < thread_count_)
    {
        PSS_LOGGER_DEBUG("[CWorkThreadLogic::close_frame_work_thread]thread id must more than config thread count.");
        return false;
    }

    //查找这个线程ID是否已经存在了
    auto f = plugin_work_thread_list_.find(thread_id);
    if (f != plugin_work_thread_list_.end())
    {
        //关闭线程
        f->second->close();
        plugin_work_thread_list_.erase(f);
        return true;
    }

    return false;
}

bool CWorkThreadLogic::delete_frame_message_timer(uint64 timer_id)
{
    std::lock_guard <std::recursive_mutex> lock(plugin_timer_mutex_);

    auto f = plgin_timer_list_.find(timer_id);
    if (f != plgin_timer_list_.end())
    {
        auto timer = f->second;
        auto timer_ptr = timer.lock();
        if (nullptr != timer_ptr)
        {
            timer_ptr->cancel();
        }

        plgin_timer_list_.erase(f);
        return true;
    }
    else
    {
        return false;
    }
}

uint16 CWorkThreadLogic::get_io_work_thread_count() const
{
    return thread_count_;
}

uint16 CWorkThreadLogic::get_plugin_work_thread_count() const
{
    return (uint16)plugin_work_thread_list_.size();
}

int CWorkThreadLogic::module_run(const std::string& module_name, std::shared_ptr<CMessage_Packet> send_packet, std::shared_ptr<CMessage_Packet> return_packet)
{
    return this->load_module_.plugin_in_name_to_module_run(module_name, send_packet, return_packet);
}

uint32 CWorkThreadLogic::get_curr_thread_logic_id() const
{
    return App_tms::instance()->GetLogicThreadID();
}

void CWorkThreadLogic::send_io_buffer() const
{
    //到时间了，群发数据
    for (auto module_logic : thread_module_list_)
    {
        module_logic->each_session_id([module_logic](uint32 session_id) {
            //将缓冲中的数据发送出去
            auto session = module_logic->get_session_interface(session_id);
            session->do_write(session_id);
            });
    }
}

/**
 * @brief 设置桥接ID
 * @param from_io_connect_id 
 * @param to_io_connect_id 
 * @return 
*/
bool CWorkThreadLogic::set_io_bridge_connect_id(uint32 from_io_connect_id, uint32 to_io_connect_id)
{
    auto curr_post_thread_index = to_io_connect_id % thread_count_;

    auto post_module_logic = this->thread_module_list_[ curr_post_thread_index ];

    auto session_io = post_module_logic->get_session_interface( to_io_connect_id ) ;
    if (nullptr == session_io)
    {
        //没找到对应链接
        return false;
    }
    else
    {
        //设置端到端的桥接id
        session_io->set_io_bridge_connect_id(from_io_connect_id, to_io_connect_id);

        return true;
    }
}

/**
 * @brief  do_io_bridge_data 接收数据 转发给“桥接服务器”处理
 * @param connect_id 
 * @param io_bradge_connect_id_ 
 * @param session_recv_buffer 
 * @param length 
 * @param session 
 * @return 
*/
int CWorkThreadLogic::do_io_bridge_data(uint32 connect_id, uint32 io_bradge_connect_id_, CSessionBuffer& session_recv_buffer, std::size_t length, shared_ptr<ISession> session)
{
    int ret = 0;
    auto bridge_packet = std::make_shared<CMessage_Packet>();
    bridge_packet->buffer_.append( session_recv_buffer.read() , length );

    if ( io_bradge_connect_id_ > 0)
    {
            if ( false == this->send_io_bridge_message( io_bradge_connect_id_, bridge_packet ) )
            {
                //发送失败，将数据包会给 ”本地的业务逻辑 “去处理
                std::vector<std::shared_ptr<CMessage_Packet>> message_error_list;
                bridge_packet->command_id_ = LOGIC_IOTOIO_DATA_ERROR;
                message_error_list.emplace_back(bridge_packet);

                //添加消息处理
                this->assignation_thread_module_logic( connect_id, message_error_list, session);

                ret = 1;
            }
            else
            {
                ret = 0;
            }
    }
    else
    {
        //发送失败，将数据包重新交给给业务逻辑去处理
        vector<std::shared_ptr<CMessage_Packet>> message_error_list;
        bridge_packet->command_id_ = LOGIC_IOTOIO_DATA_ERROR;
        message_error_list.emplace_back(bridge_packet);

        //添加消息处理
        this->assignation_thread_module_logic(connect_id, message_error_list, session);
        ret = 2;
    }

    session_recv_buffer.move(length);

    return ret;
}

void CWorkThreadLogic::send_io_message(uint32 connect_id, std::shared_ptr<CMessage_Packet> send_packet)
{
    //处理线程的投递
    uint16 curr_thread_index = connect_id % thread_count_;
    auto module_logic = thread_module_list_[curr_thread_index];

    //添加到数据队列处理
    App_tms::instance()->AddMessage(curr_thread_index, [this, connect_id, send_packet, module_logic]() {
        do_io_message_delivery(connect_id, send_packet, module_logic);
        });
}

bool CWorkThreadLogic::send_io_bridge_message(uint32 io_bridge_connect_id, std::shared_ptr<CMessage_Packet> send_packet)
{
    uint16 curr_thread_index = io_bridge_connect_id % thread_count_;

    auto module_logic = thread_module_list_[ curr_thread_index ];

    //通过桥接id , 获取对应的会话
    auto session = module_logic->get_session_interface( io_bridge_connect_id );

    if (nullptr != session)
    {
        //通过桥接ID  ，获取对应的桥接会话 ； 
        session->do_write_immediately( io_bridge_connect_id, send_packet->buffer_.c_str(), send_packet->buffer_.size() );

        return true;
    }
    else
    {
        return false;
    }
}

bool CWorkThreadLogic::connect_io_server(const CConnect_IO_Info& io_info, EM_CONNECT_IO_TYPE io_type)
{
    //寻找当前server_id是否存在
    if (true == communicate_service_->is_exist(io_info.server_id))
    {
        PSS_LOGGER_DEBUG("[CWorkThreadLogic::connect_io_server]server_id={0} is exist.",io_info.server_id);
        return false;
    }
    else
    {
        return communicate_service_->add_connect(io_info, io_type);
    }
}

void CWorkThreadLogic::close_io_server(uint32 server_id)
{
    communicate_service_->close_connect(server_id);
}

uint32 CWorkThreadLogic::get_io_server_id(uint32 connect_id)
{
    return communicate_service_->get_server_id(connect_id);
}

void CWorkThreadLogic::run_check_task(uint32 timeout_seconds) const
{
    //检测所有工作线程状态
    for (auto module_logic : thread_module_list_)
    {
        auto work_thread_timeout = module_logic->get_work_thread_timeout();
        if (work_thread_timeout > (int)timeout_seconds)
        {
            PSS_LOGGER_ERROR("[CWorkThreadLogic::run_check_task]work thread{0} is block, timeout={1}, last_command_id={2}.", 
                module_logic->get_work_thread_id(),
                work_thread_timeout,
                module_logic->get_last_dispose_command_id());

            //发送消息通知插件
            CMessage_Source source;
            source.work_thread_id_ = module_logic->get_work_thread_id();

            auto recv_packet = std::make_shared<CMessage_Packet>();
            auto send_packet = std::make_shared<CMessage_Packet>();

            recv_packet->command_id_ = LOGIC_THREAD_DEAD_LOCK;
            recv_packet->buffer_ = JSON_MODULE_THREAD_ID + std::to_string(module_logic->get_work_thread_id())
                + JSON_MODULE_COMMAND_ID + std::to_string(module_logic->get_last_dispose_command_id())
                + JSON_MODULE_WORK_THREAD_TIMEOUT + std::to_string(work_thread_timeout) + JSON_MODULE_END;

            thread_module_list_[0]->do_thread_module_logic(source, recv_packet, send_packet);

        }
    }

    //检测所有的tcp链接状态
    if (0 < connect_timeout_)
    {
        uint32 connect_timeout = connect_timeout_;
        for (auto module_logic : thread_module_list_)
        {
            App_tms::instance()->AddMessage(module_logic->get_work_thread_id(), [connect_timeout, module_logic]() {
                module_logic->check_session_io_timeout(connect_timeout);
                });
        }
    }


    PSS_LOGGER_DEBUG("[CWorkThreadLogic::run_check_task]check is ok.");
}

/**
 * @brief  send_frame_message 发送消息
 * @param tag_thread_id 
 * @param message_tag 
 * @param send_packet 
 * @param delay_timer 
 * @return 
*/
bool CWorkThreadLogic::send_frame_message( uint16 tag_thread_id, const std::string& message_tag, std::shared_ptr<CMessage_Packet> send_packet, CFrame_Message_Delay delay_timer)
{
    if (false == this->module_init_finish_) //当模块未完全加载完毕时
    {
        CDelayPluginMessage plugin_message;
        plugin_message.tag_thread_id_ = tag_thread_id;
        plugin_message.message_tag_ = message_tag;
        plugin_message.send_packet_ = send_packet;
        plugin_message.delay_timer_ = delay_timer;

        this->plugin_work_thread_buffer_message_list_.emplace_back( plugin_message );
        
        return true;
    }

    return this->do_frame_message(tag_thread_id, message_tag, send_packet, delay_timer);
}

/**
 * @brief do_frame_message 处理 插件（客户端）发送至本地的消息（指令）
 * @param tag_thread_id 
 * @param message_tag 
 * @param send_packet 
 * @param delay_timer 
 * @return 
*/
bool CWorkThreadLogic::do_frame_message(uint16 tag_thread_id, const std::string& message_tag, std::shared_ptr<CMessage_Packet> send_packet , CFrame_Message_Delay delay_timer )
{
    auto f = this->plugin_work_thread_list_.find(  tag_thread_id  );

    if ( f == this->plugin_work_thread_list_.end() )
    {
        return false;
    }

    //获取对应的“业务逻辑”
    auto plugin_logic_= f->second ;

    if ( delay_timer.delay_seconds_ == std::chrono::seconds(0) )
    {
        //不需要延时，立刻投递
        this->do_plugin_thread_module_logic( plugin_logic_ ,  message_tag  , send_packet  );
    }
    else
    {
        //需要延时，延时后投递
        auto timer_ptr = App_TimerManager::instance()->GetTimerPtr()->addTimer( delay_timer.delay_seconds_ , 
                                                                                [ this ,  plugin_logic_ ,  message_tag  ,  send_packet  ,  delay_timer ] ()
                                                                                {
                                                                                    //对定时器列表操作加锁
                                                                                    this->plugin_timer_mutex_.lock();

                                                                                    this->plgin_timer_list_.erase( delay_timer.timer_id_ );
                                                                                    
                                                                                    this->plugin_timer_mutex_.unlock();

                                                                                    //延时到期，进行投递
                                                                                    this->do_plugin_thread_module_logic( plugin_logic_ , message_tag, send_packet);
                                                                                }
                                                                             );

        //添加映射关系(只有在定时器ID > 0的时候才能删除)
        if ( delay_timer.timer_id_ > 0)
        {
            std::lock_guard <std::recursive_mutex> lock(this->plugin_timer_mutex_);
            this->plgin_timer_list_[ delay_timer.timer_id_ ] = timer_ptr;
        }
    }

    return true;
}

/**
 * @brief  run_work_thread_logic  运行工作线程逻辑（谁来运行？？？？）
 * @param tag_thread_id 
 * @param delay_timer 
 * @param func 
 * @return 
*/
bool CWorkThreadLogic::run_work_thread_logic(uint16 tag_thread_id, CFrame_Message_Delay delay_timer, const task_function& func)
{
    if (false == this->module_init_finish_)
    {
        auto plugin_func = std::make_shared<CDelayPluginFunc>();

        plugin_func->tag_thread_id_ = tag_thread_id;
        plugin_func->func_ = func;
        plugin_func->delay_timer_ = delay_timer;

        this->plugin_work_thread_buffer_Func_list_.emplace_back(plugin_func);

        return true;
    }

    return this->do_work_thread_logic(tag_thread_id, delay_timer, func);
}

/**
 * @brief  do_work_thread_logic 执行工作线程逻辑（谁来执行？？？？？）
 * @param tag_thread_id 
 * @param delay_timer 
 * @param func 
 * @return 
*/
bool CWorkThreadLogic::do_work_thread_logic(uint16 tag_thread_id, CFrame_Message_Delay delay_timer, const task_function& func)
{
    auto f = this->plugin_work_thread_list_.find( tag_thread_id );

    if (f == this->plugin_work_thread_list_.end())
    {
        return false;
    }

    if (delay_timer.delay_seconds_ == std::chrono::seconds(0))
    {
        //立刻执行线程函数
        App_tms::instance()->AddMessage( tag_thread_id , func );
    }
    else
    {
        //需要延时，延时后投递
        auto timer_ptr = App_TimerManager::instance()->GetTimerPtr()->addTimer(delay_timer.delay_seconds_ , 
                                                                                [this, tag_thread_id, delay_timer, func]()
                                                                                {
                                                                                    //对定时器列表操作加锁
                                                                                    plugin_timer_mutex_.lock();
                                                                                    plgin_timer_list_.erase(delay_timer.timer_id_);
                                                                                    plugin_timer_mutex_.unlock();

                                                                                    //延时到期，进行投递
                                                                                    App_tms::instance()->AddMessage( tag_thread_id , func );
                                                                                }
                                                                            );

        //添加映射关系(只有在定时器ID > 0的时候才能删除)
        if (delay_timer.timer_id_ > 0)
        {
            std::lock_guard <std::recursive_mutex> lock(plugin_timer_mutex_);
            plgin_timer_list_[delay_timer.timer_id_] = timer_ptr;
        }
    }

    return true;
}



