#include "TcpSSLSession.h"
#ifdef SSL_SUPPORT

CTcpSSLSession::CTcpSSLSession(asio::ssl::stream<tcp::socket> socket, asio::io_context* io_context)
    : ssl_socket_(std::move(socket)), io_context_(io_context)
{
}

void CTcpSSLSession::open(uint32 packet_parse_id, uint32 recv_size)
{
    connect_id_ = App_ConnectCounter::instance()->CreateCounter();

    packet_parse_interface_ = App_PacketParseLoader::instance()->GetPacketParseInfo(packet_parse_id);

    session_recv_buffer_.Init(recv_size);

    recv_data_time_ = std::chrono::steady_clock::now();

    //�������ӽ�����Ϣ
    remote_ip_.m_strClientIP = ssl_socket_.lowest_layer().remote_endpoint().address().to_string();
    remote_ip_.m_u2Port = ssl_socket_.lowest_layer().remote_endpoint().port();
    local_ip_.m_strClientIP = ssl_socket_.lowest_layer().local_endpoint().address().to_string();
    local_ip_.m_u2Port = ssl_socket_.lowest_layer().local_endpoint().port();
    packet_parse_interface_->packet_connect_ptr_(connect_id_, remote_ip_, local_ip_, io_type_, App_IoBridge::instance());

    //��ӵ�Ե�ӳ��
    if (true == App_IoBridge::instance()->regedit_session_id(remote_ip_, io_type_, connect_id_))
    {
        io_state_ = EM_SESSION_STATE::SESSION_IO_BRIDGE;
    }

    //�鿴��������Ƿ����Ž���Ϣ
    io_bradge_connect_id_ = App_IoBridge::instance()->get_to_session_id(connect_id_, remote_ip_);
    if (io_bradge_connect_id_ > 0)
    {
        App_WorkThreadLogic::instance()->set_io_bridge_connect_id(connect_id_, io_bradge_connect_id_);
    }

    //����session ӳ��
    App_WorkThreadLogic::instance()->add_thread_session(connect_id_, shared_from_this(), local_ip_, remote_ip_);


    do_handshake();
}

_ClientIPInfo CTcpSSLSession::get_remote_ip(uint32 connect_id)
{
    return remote_ip_;
}

void CTcpSSLSession::close(uint32 connect_id)
{
    auto self(shared_from_this());

    auto recv_data_size = recv_data_size_;
    auto send_data_size = send_data_size_;
    auto io_send_count = io_send_count_;

    EM_CONNECT_IO_TYPE io_type = io_type_;
    _ClientIPInfo remote_ip = remote_ip_;

    io_context_->dispatch([self, connect_id, io_type, remote_ip, recv_data_size, send_data_size, io_send_count]()
        {
            self->ssl_socket_.lowest_layer().close();

            //������շ����ֽ���
            PSS_LOGGER_DEBUG("[CTcpSession::Close]recv:{0}, send:{1} io_send_count:{2}", recv_data_size, send_data_size, io_send_count);

            //�Ͽ�����
            self->packet_parse_interface_->packet_disconnect_ptr_(connect_id, io_type, App_IoBridge::instance());

            App_WorkThreadLogic::instance()->delete_thread_session(connect_id, self);
        });
}

void CTcpSSLSession::set_write_buffer(uint32 connect_id, const char* data, size_t length)
{
    std::lock_guard<std::mutex> lck(send_thread_mutex_);

    session_send_buffer_.append(data, length);
}

void CTcpSSLSession::do_read()
{
    //��������
    //��������������Ͽ����ӣ����ٽ������ݡ�
    if (session_recv_buffer_.get_buffer_size() == 0)
    {
        //���ӶϿ�(���������)
        App_WorkThreadLogic::instance()->close_session_event(connect_id_);
    }

    auto self(shared_from_this());
    ssl_socket_.async_read_some(asio::buffer(session_recv_buffer_.get_curr_write_ptr(), session_recv_buffer_.get_buffer_size()),
        [self](std::error_code ec, std::size_t length)
        {
            self->do_read_some(ec, length);
        });
}

void CTcpSSLSession::clear_write_buffer()
{
    session_send_buffer_.clear();
}

void CTcpSSLSession::do_read_some(std::error_code ec, std::size_t length)
{
    if (!ec)
    {
        recv_data_size_ += length;
        session_recv_buffer_.set_write_data(length);

        //�ж��Ƿ����Ž�
        if (EM_SESSION_STATE::SESSION_IO_BRIDGE == io_state_)
        {
            //������ת�����Žӽӿ�
            auto ret = App_WorkThreadLogic::instance()->do_io_bridge_data(connect_id_, io_bradge_connect_id_, session_recv_buffer_, length, shared_from_this());
            if (1 == ret)
            {
                //Զ��IO�����ѶϿ�
                io_bradge_connect_id_ = 0;
            }
        }
        else
        {
            //�������ݲ��
            vector<std::shared_ptr<CMessage_Packet>> message_list;
            bool ret = packet_parse_interface_->packet_from_recv_buffer_ptr_(connect_id_, &session_recv_buffer_, message_list, io_type_);
            if (!ret)
            {
                //���ӶϿ�(����������ȷ)
                App_WorkThreadLogic::instance()->close_session_event(connect_id_);
            }
            else
            {
                //���½�������ʱ��
                recv_data_time_ = std::chrono::steady_clock::now();

                //�����Ϣ����
                App_WorkThreadLogic::instance()->assignation_thread_module_logic(connect_id_, message_list, shared_from_this());
            }
        }

        //����������
        do_read();
    }
    else
    {
        //���ӶϿ�
        App_WorkThreadLogic::instance()->close_session_event(connect_id_);
    }
}

void CTcpSSLSession::send_write_fail_to_logic(const std::string& write_fail_buffer, std::size_t buffer_length)
{
    vector<std::shared_ptr<CMessage_Packet>> message_ssl_recv_list;
    auto ssl_recv_write_fail_packet = std::make_shared<CMessage_Packet>();
    ssl_recv_write_fail_packet->command_id_ = LOGIC_THREAD_WRITE_IO_ERROR;
    ssl_recv_write_fail_packet->buffer_.append(write_fail_buffer.c_str(), buffer_length);
    message_ssl_recv_list.emplace_back(ssl_recv_write_fail_packet);

    //дIOʧ����Ϣ�ύ���߼����
    App_WorkThreadLogic::instance()->assignation_thread_module_logic_with_events(connect_id_,
        message_ssl_recv_list, 
        shared_from_this());
}

void CTcpSSLSession::do_write(uint32 connect_id)
{
    std::lock_guard<std::mutex> lck(send_thread_mutex_);

    if (is_send_finish_ == false || session_send_buffer_.size() == 0)
    {
        //�ϴη���û����ɻ����Ѿ��������
        return;
    }

    is_send_finish_ = false;

    //��װ��������
    auto send_buffer = make_shared<CSendBuffer>();
    send_buffer->data_ = session_send_buffer_;
    send_buffer->buffer_length_ = session_send_buffer_.size();

    clear_write_buffer();

    auto self(shared_from_this());
    io_context_->dispatch([self, send_buffer, connect_id]()
        {
            //�첽����
            asio::async_write(self->ssl_socket_, asio::buffer(send_buffer->data_.c_str(), send_buffer->buffer_length_),
                [self, send_buffer, connect_id](std::error_code ec, std::size_t length)
                {
                    if (ec)
                    {
                        //����ҵ���߼�����ʧ��
                        PSS_LOGGER_DEBUG("[CTcpSession::do_write]({0})write error({1}).", connect_id, ec.message());

                        self->send_write_fail_to_logic(send_buffer->data_, length);
                    }
                    else
                    {
                        self->add_send_finish_size(connect_id, length);

                        //��������
                        self->do_write(connect_id);
                    }
                });
        });
}

void CTcpSSLSession::do_write_immediately(uint32 connect_id, const char* data, size_t length)
{
    set_write_buffer(connect_id, data, length);
    do_write(connect_id);
}

void CTcpSSLSession::add_send_finish_size(uint32 connect_id, size_t send_length)
{
    std::lock_guard<std::mutex> lck(send_thread_mutex_);

    send_data_size_ += send_length;

    io_send_count_++;

    is_send_finish_ = true;
}

EM_CONNECT_IO_TYPE CTcpSSLSession::get_io_type()
{
    return io_type_;
}

uint32 CTcpSSLSession::get_mark_id(uint32 connect_id)
{
    PSS_UNUSED_ARG(connect_id);
    return 0;
}

std::chrono::steady_clock::time_point& CTcpSSLSession::get_recv_time(uint32 connect_id)
{
    PSS_UNUSED_ARG(connect_id);
    return recv_data_time_;
}

void CTcpSSLSession::set_io_bridge_connect_id(uint32 from_io_connect_id, uint32 to_io_connect_id)
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

bool CTcpSSLSession::format_send_packet(uint32 connect_id, std::shared_ptr<CMessage_Packet> message, std::shared_ptr<CMessage_Packet> format_message)
{
    return packet_parse_interface_->parse_format_send_buffer_ptr_(connect_id, message, format_message, get_io_type());
}

bool CTcpSSLSession::is_need_send_format()
{
    return packet_parse_interface_->is_need_send_format_ptr_();
}

void CTcpSSLSession::do_handshake()
{
  	std::cout << "[do_handshake] Begin" << std::endl;
    auto self(shared_from_this());
    ssl_socket_.async_handshake(asio::ssl::stream_base::server, 
        [this, self](const std::error_code& error)
        {
          if (!error)
          {
          	std::cout << "[do_handshake] read" << std::endl;
            do_read();
          }
          else
          {
          	std::cout << "[do_handshake] error:" << error.message() << std::endl;
          }
        });
}

#endif
