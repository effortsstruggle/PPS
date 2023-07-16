#include "TcpSession.h"

CTcpSession::CTcpSession(asio::io_context& io_context)
    : socket_(io_context)
{
}

void CTcpSession::start(uint32 connect_id, uint32 buffer_size, string server_ip, uint16 server_port)
{
    connect_id_ = connect_id;

    session_recv_buffer_.Init(buffer_size);
    session_send_buffer_.Init(buffer_size);

    //��������
    tcp::endpoint end_point(asio::ip::address::from_string(server_ip.c_str()), server_port);
    asio::error_code connect_error;
    socket_.connect(end_point, connect_error);

    if (connect_error)
    {
        //���ӽ���ʧ��
        PSS_LOGGER_DEBUG("[CTcpSession::start]error({})", connect_error.message());
    }
    else
    {
        //�������ӽ�����Ϣ
        _ClientIPInfo remote_ip;
        _ClientIPInfo local_ip;
        remote_ip.m_strClientIP = socket_.remote_endpoint().address().to_string();
        remote_ip.m_u2Port = socket_.remote_endpoint().port();
        local_ip.m_strClientIP = socket_.local_endpoint().address().to_string();
        local_ip.m_u2Port = socket_.local_endpoint().port();

        PSS_LOGGER_DEBUG("[CTcpSession::start]remote({0}:{1})", remote_ip.m_strClientIP, remote_ip.m_u2Port);
        PSS_LOGGER_DEBUG("[CTcpSession::start]local({0}:{1})", local_ip.m_strClientIP, local_ip.m_u2Port);

        do_read();
    }
}

void CTcpSession::Close()
{
    socket_.close();

    //������շ����ֽ���
    PSS_LOGGER_DEBUG("[CTcpSession::Close]recv:{0}, send:{1}", recv_data_size_, send_data_size_);

}

void CTcpSession::do_read()
{
    //��������
    auto self(shared_from_this());

    //��������������Ͽ����ӣ����ٽ������ݡ�
    if (session_recv_buffer_.get_buffer_size() == 0)
    {
        //���ӶϿ�(���������)
        App_tms::instance()->AddMessage(1, [self]() {
            self->Close();
            });
    }

    socket_.async_read_some(asio::buffer(session_recv_buffer_.get_curr_write_ptr(), session_recv_buffer_.get_buffer_size()),
        [this, self](std::error_code ec, std::size_t length)
        {
            if (!ec)
            {
                recv_data_size_ += length;
                session_recv_buffer_.set_write_data(length);
                PSS_LOGGER_DEBUG("[CTcpSession::do_write]recv length={}.", length);

                std::memcpy(session_send_buffer_.get_curr_write_ptr(),
                    session_recv_buffer_.read(),
                    length);
                session_send_buffer_.set_write_data(length);

                //�������ݲ��
                
                session_recv_buffer_.move(length);
                //����������
                self->do_read();
            }
            else
            {
                //���ӶϿ�
                Close();
            }
        });
}

void CTcpSession::do_write()
{
    //��װ��������
    auto send_buffer = make_shared<CSendBuffer>();
    send_buffer->data_.append(session_send_buffer_.read(), session_send_buffer_.get_write_size());
    send_buffer->buffer_length_ = session_send_buffer_.get_write_size();
    session_send_buffer_.move(session_send_buffer_.get_write_size());

    //PSS_LOGGER_DEBUG("[CTcpSession::do_write]send_buffer->buffer_length_={}.", send_buffer->buffer_length_);

    clear_write_buffer(send_buffer->buffer_length_);

    //�첽����
    auto self(shared_from_this());
    asio::async_write(socket_, asio::buffer(send_buffer->data_.c_str(), send_buffer->buffer_length_),
        [self, send_buffer](std::error_code ec, std::size_t length)
        {
            if (ec)
            {
                //��ʱ������
                std::cout << "[CTcpSession::do_write](" << ec.value() << ") mseeage(" << ec.message() << ")" << std::endl;
            }
            else
            {
                self->add_send_finish_size(length);
            }
        });
}

void CTcpSession::set_write_buffer(const char* data, size_t length)
{
    std::memcpy(session_send_buffer_.get_curr_write_ptr(), 
        data,
        length);
    session_send_buffer_.set_write_data(length);
}

void CTcpSession::clear_write_buffer(size_t length)
{
    session_send_buffer_.move(length);
}

void CTcpSession::add_send_finish_size(size_t send_length)
{
    send_data_size_ += send_length;
}

