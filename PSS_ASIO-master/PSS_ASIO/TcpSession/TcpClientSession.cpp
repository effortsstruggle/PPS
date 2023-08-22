﻿#include "TcpClientSession.h"

CTcpClientSession::CTcpClientSession(asio::io_context* io_context)
    : socket_(*io_context), io_context_(io_context)
{
}

/**
 * @brief 开始建立Tcp链接
 * @param io_info 
 * @return 
*/
bool CTcpClientSession::start(const CConnect_IO_Info& io_info)
{
    this->server_id_ = io_info.server_id;

    this->packet_parse_interface_ = App_PacketParseLoader::instance()->GetPacketParseInfo(io_info.packet_parse_id);

    this->session_recv_buffer_.Init(io_info.recv_size);

    this->session_send_buffer_.Init(io_info.send_size);

    //赋值对应的IP和端口信息
    //本地IP及端口
    this->local_ip_.m_strClientIP = io_info.client_ip;
    this->local_ip_.m_u2Port = io_info.client_port;
    //服务器
    this->remote_ip_.m_strClientIP = io_info.server_ip;
    this->remote_ip_.m_u2Port = io_info.server_port;

    asio::error_code connect_error;

    //判断链接是否需要指定客户端IP和端口
    if ( io_info.client_port > 0 && io_info.client_ip.length() > 0)
    {
        asio::ip::tcp::endpoint localEndpoint(asio::ip::address::from_string( io_info.client_ip ), io_info.client_port );
        this->socket_.open( asio::ip::tcp::v4() , connect_error );
        this->socket_.set_option( asio::ip::tcp::socket::reuse_address(true) );

        try
        {
            this->socket_.bind( localEndpoint, connect_error );
        }
        catch (std::system_error const& ex)
        {
            PSS_LOGGER_ERROR("[CTcpClientSession::start] bind addr error remote:[{}:{}] local:[{}:{}] ex.what:{}.", io_info.server_ip, io_info.server_port,io_info.client_ip,io_info.client_port, ex.what());
        }
    }

    //异步链接
    tcp::endpoint end_point(asio::ip::address::from_string(io_info.server_ip.c_str()), io_info.server_port);
    
    tcp::resolver::results_type::iterator endpoint_iter;

    this->socket_.async_connect(  end_point, 
                                            std::bind(&CTcpClientSession::handle_connect, this , std::placeholders::_1 , endpoint_iter) 
                                        );
    return true;
}

void CTcpClientSession::close(uint32 connect_id)
{
    auto self(shared_from_this());

    auto recv_data_size = recv_data_size_;
    auto send_data_size = send_data_size_;

    EM_CONNECT_IO_TYPE io_type = io_type_;
    _ClientIPInfo remote_ip = remote_ip_;

    //清除点对点转发消息映射
    App_IoBridge::instance()->unregedit_session_id(remote_ip, io_type_);

    //放到收发线程去处理
    io_context_->dispatch([self, connect_id, recv_data_size, send_data_size, io_type, remote_ip]() {
        self->socket_.close();

        //输出接收发送字节数
        PSS_LOGGER_DEBUG("[CTcpClientSession::Close]recv:{0}, send:{1}", recv_data_size, send_data_size);

        //断开连接
        self->packet_parse_interface_->packet_disconnect_ptr_(connect_id, io_type, App_IoBridge::instance());

        //发送链接断开消息
        App_WorkThreadLogic::instance()->delete_thread_session(connect_id, self);
        });

}

void CTcpClientSession::set_write_buffer(uint32 connect_id, const char* data, size_t length)
{
    if (session_send_buffer_.get_buffer_size() <= length)
    {
        //发送些缓冲已经满了
        PSS_LOGGER_DEBUG("[CTcpSession::set_write_buffer]connect_id={} is full.", connect_id);
        return;
    }

    std::memcpy(session_send_buffer_.get_curr_write_ptr(),
        data,
        length);
    session_send_buffer_.set_write_data(length);
}

/**
 * @brief 读取数据
*/
void CTcpClientSession::do_read()
{
    //接收数据
    auto self( shared_from_this() );

    auto connect_id = this->connect_id_;

    //如果缓冲已满，断开连接，不再接受数据。
    if (session_recv_buffer_.get_buffer_size() == 0)
    {
        //链接断开(缓冲撑满了)
        App_tms::instance()->AddMessage(1, [self, connect_id]() {
            self->close(connect_id);
            });
    }

	socket_.async_read_some(
		asio::buffer(session_recv_buffer_.get_curr_write_ptr(), session_recv_buffer_.get_buffer_size()),
		[self](std::error_code ec, std::size_t length)
		{
			self->do_read_some(ec, length);
		}
	);
}

_ClientIPInfo CTcpClientSession::get_remote_ip(uint32 connect_id)
{
    return remote_ip_;
}

void CTcpClientSession::do_write_immediately(uint32 connect_id, const char* data, size_t length)
{
    if (is_connect_ == false)
    {
        PSS_LOGGER_DEBUG("[CTcpClientSession::do_write_immediately]({0}), connect is not ready.", connect_id);
        return;
    }

    //组装发送数据
    auto send_buffer = make_shared<CSendBuffer>();
    send_buffer->data_.append(data, length);
    send_buffer->buffer_length_ = length;

    //异步发送
    auto self(shared_from_this());

    io_context_->dispatch([self, connect_id, send_buffer]() {
        asio::async_write(self->socket_, asio::buffer(send_buffer->data_.c_str(), send_buffer->buffer_length_),
            [self, connect_id, send_buffer](std::error_code ec, std::size_t send_length)
            {
                if (ec)
                {
                    //发送IO消息写入失败
                    PSS_LOGGER_DEBUG("[CTcpClientSession::do_write_immediately]({0}), message({1})", connect_id, ec.message());
                    self->send_write_fail_to_logic(send_buffer->data_, send_length);
                }
                else
                {
                    self->add_send_finish_size(connect_id, send_length);
                }
            });
        });
}

void CTcpClientSession::do_write(uint32 connect_id)
{
    if (is_connect_ == false)
    {
        PSS_LOGGER_DEBUG("[CTcpClientSession::do_write]({0}), connect is not ready.", connect_id);
        return;
    }

    //组装发送数据
    auto send_buffer = make_shared<CSendBuffer>();
    send_buffer->data_.append(session_send_buffer_.read(), session_send_buffer_.get_write_size());
    send_buffer->buffer_length_ = session_send_buffer_.get_write_size();

    clear_write_buffer();

    //异步发送
    auto self(shared_from_this());
    asio::async_write(socket_, asio::buffer(send_buffer->data_.c_str(), send_buffer->buffer_length_),
        [self, send_buffer, connect_id](std::error_code ec, std::size_t length)
        {
            if (ec)
            {
                //向逻辑线程投递发送失败消息
                PSS_LOGGER_DEBUG("[CTcpClientSession::do_write]write error({0}).", ec.message());
                self->send_write_fail_to_logic(send_buffer->data_, length);
            }
            else
            {
                self->add_send_finish_size(connect_id, length);
            }
        });
}

void CTcpClientSession::add_send_finish_size(uint32 connect_id, size_t send_length)
{
    send_data_size_ += send_length;
}

EM_CONNECT_IO_TYPE CTcpClientSession::get_io_type()
{
    return io_type_;
}

uint32 CTcpClientSession::get_mark_id(uint32 connect_id)
{
    PSS_UNUSED_ARG(connect_id);
    return server_id_;
}

std::chrono::steady_clock::time_point& CTcpClientSession::get_recv_time(uint32 connect_id)
{
    PSS_UNUSED_ARG(connect_id);
    return recv_data_time_;
}

void CTcpClientSession::set_io_bridge_connect_id(uint32 from_io_connect_id, uint32 to_io_connect_id)
{
    if (to_io_connect_id > 0)
    {
        io_state_ = EM_SESSION_STATE::SESSION_IO_BRIDGE;
        io_bradge_connect_id_ = from_io_connect_id;
    }
    else
    {
        io_state_ = EM_SESSION_STATE::SESSION_IO_LOGIC;
        io_bradge_connect_id_ = 0;
    }
}

bool CTcpClientSession::format_send_packet(uint32 connect_id, std::shared_ptr<CMessage_Packet> message, std::shared_ptr<CMessage_Packet> format_message)
{
    return packet_parse_interface_->parse_format_send_buffer_ptr_(connect_id, message, format_message, get_io_type());
}

bool CTcpClientSession::is_need_send_format()
{
    return packet_parse_interface_->is_need_send_format_ptr_();
}

bool CTcpClientSession::is_connect()
{
    return is_connect_;
}

void CTcpClientSession::clear_write_buffer()
{
    session_send_buffer_.move(session_send_buffer_.get_write_size());
}


/**
 * @brief 
 * @param ec 
 * @param length 
*/
void CTcpClientSession::do_read_some(std::error_code ec, std::size_t length)
{
    if (!ec)
    {
        //更新接收缓冲区
        recv_data_size_ += length;
        this->session_recv_buffer_.set_write_data(length);
        
        PSS_LOGGER_DEBUG("[CTcpClientSession::do_write]recv length={}.", length);

        //判断是否有桥接
        if (EM_SESSION_STATE::SESSION_IO_BRIDGE == io_state_)
        {
            //将数据转发给桥接接口
            auto ret = App_WorkThreadLogic::instance()->do_io_bridge_data(connect_id_, io_bradge_connect_id_, session_recv_buffer_, length, shared_from_this());
            if (1 == ret)
            {
                //远程IO链接已断开
                io_bradge_connect_id_ = 0;
            }
        }
        else
        {
            //处理数据拆包( 存储解析完成的数据 )
            vector< std::shared_ptr<CMessage_Packet> > message_list;
            //调用”协议解析动态库“，解析数据 
            bool ret = packet_parse_interface_->packet_from_recv_buffer_ptr_( this->connect_id_ , &this->session_recv_buffer_ , message_list , this->io_type_);
            if (!ret)
            {
                //链接断开(解析包不正确)
                App_WorkThreadLogic::instance()->close_session_event(connect_id_);
            }
            else
            {
                this->recv_data_time_ = std::chrono::steady_clock::now();

                //添加消息处理 
                App_WorkThreadLogic::instance()->assignation_thread_module_logic(this->connect_id_ , message_list , shared_from_this() );
            }
        }

        //继续读数据
        this->do_read();
    }
    else
    {
        //链接断开
        App_WorkThreadLogic::instance()->close_session_event(connect_id_);
        is_connect_ = false;
    }
}

/**
 * @brief 异步链接，处理函数
 * @param ec 
 * @param endpoint_iter 
*/
void CTcpClientSession::handle_connect(const asio::error_code& ec, tcp::resolver::results_type::iterator endpoint_iter)
{
    if (! ec )
    {
        this->is_connect_ = true;

        this->connect_id_ = App_ConnectCounter::instance()->CreateCounter();

        this->recv_data_time_ = std::chrono::steady_clock::now();

        //处理链接建立消息
        this->remote_ip_.m_strClientIP = socket_.remote_endpoint().address().to_string();
        this->remote_ip_.m_u2Port = socket_.remote_endpoint().port();
        this->local_ip_.m_strClientIP = socket_.local_endpoint().address().to_string();
        this->local_ip_.m_u2Port = socket_.local_endpoint().port();

        PSS_LOGGER_DEBUG("[CTcpClientSession::start]remote({0}:{1})", remote_ip_.m_strClientIP, remote_ip_.m_u2Port);
        PSS_LOGGER_DEBUG("[CTcpClientSession::start]local({0}:{1})", local_ip_.m_strClientIP, local_ip_.m_u2Port);

        this->packet_parse_interface_->packet_connect_ptr_( connect_id_ , remote_ip_ , local_ip_ , io_type_ , App_IoBridge::instance());

        //添加点对点映射
        if (true == App_IoBridge::instance()->regedit_session_id(this->remote_ip_ , this->io_type_ , this->connect_id_))
        {
            io_state_ = EM_SESSION_STATE::SESSION_IO_BRIDGE;
        }

        //查看这个链接是否有桥接信息
        this->io_bradge_connect_id_ = App_IoBridge::instance()->get_to_session_id(this->connect_id_ , this->remote_ip_);
        if (this->io_bradge_connect_id_ > 0)
        {
            App_WorkThreadLogic::instance()->set_io_bridge_connect_id(this->connect_id_ , this->io_bradge_connect_id_);
        }

#ifdef GCOV_TEST
        //测试发送写入失败回调消息
        std::string write_fail_text = "test write fail";
        send_write_fail_to_logic(write_fail_text, write_fail_text.length());
#endif
        //添加映射关系
        App_WorkThreadLogic::instance()->add_thread_session(this->connect_id_, shared_from_this(), this->local_ip_, this->remote_ip_);

        //接收IO数据
        this->do_read();

    }
    else
    {
        this->is_connect_ = false;

        //连接建立失败
        PSS_LOGGER_DEBUG("[CTcpClientSession::handle_connect]({0}:{1}  ==> {2}:{3})error({4})", 
            local_ip_.m_strClientIP,
            local_ip_.m_u2Port,
            remote_ip_.m_strClientIP,
            remote_ip_.m_u2Port,
            ec.message());

        //发送消息给逻辑块
        App_WorkThreadLogic::instance()->add_frame_events( LOGIC_CONNECT_SERVER_ERROR,
                                                                                                server_id_,
                                                                                                remote_ip_.m_strClientIP,
                                                                                                remote_ip_.m_u2Port,
                                                                                                io_type_
        );
    }

}

void CTcpClientSession::send_write_fail_to_logic(const std::string& write_fail_buffer, std::size_t buffer_length)
{
    vector<std::shared_ptr<CMessage_Packet>> message_tcp_connect_list;
    auto write_fail_packet = std::make_shared<CMessage_Packet>();
    write_fail_packet->command_id_ = LOGIC_THREAD_WRITE_IO_ERROR;
    write_fail_packet->buffer_.append(write_fail_buffer.c_str(), buffer_length);
    message_tcp_connect_list.emplace_back(write_fail_packet);

    //写IO失败消息提交给逻辑插件
    App_WorkThreadLogic::instance()->assignation_thread_module_logic_with_events(connect_id_, 
        message_tcp_connect_list, 
        shared_from_this());
}

