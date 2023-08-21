#pragma once

#include <iostream>
#include "define.h"
#include "asio.hpp"
#include "tms.hpp"

#include "SendBuffer.h"
#include "SessionBuffer.hpp"
#include "LoadPacketParse.h"
#include "ConnectCounter.h"
#include "ISession.h"
#include "ModuleLogic.h"
#include "Iobridge.h"

/**
 * @brief CTTyServer tty服务器
*/
class CTTyServer : public std::enable_shared_from_this<CTTyServer>, public ISession
{
public:
    CTTyServer(uint32 packet_parse_id, uint32 max_recv_size, uint32 max_send_size);

    void start(asio::io_context* io_context, const std::string& tty_name, uint16 tty_port, uint8 char_size, uint32 server_id);

    virtual void set_io_bridge_connect_id(uint32 from_io_connect_id, uint32 to_io_connect_id) override final;

    virtual _ClientIPInfo get_remote_ip(uint32 connect_id) override final;

    virtual void set_write_buffer(uint32 connect_id, const char* data, size_t length) override final;
    //发送数据
    virtual void do_write(uint32 connect_id) override final;

    virtual void do_write_immediately(uint32 connect_id, const char* data, size_t length) override final;

    virtual void close(uint32 connect_id) override final;

    virtual void add_send_finish_size(uint32 connect_id, size_t send_length) override final;

    virtual EM_CONNECT_IO_TYPE get_io_type()  override final;

    virtual uint32 get_mark_id(uint32 connect_id) override final;

    virtual std::chrono::steady_clock::time_point& get_recv_time(uint32 connect_id) override final;

    virtual bool format_send_packet(uint32 connect_id, std::shared_ptr<CMessage_Packet> message, std::shared_ptr<CMessage_Packet> format_message) override final;

    virtual bool is_need_send_format() override final;

    void do_read_some(std::error_code ec, std::size_t length);

    void send_write_fail_to_logic(const std::string write_fail_buffer, std::size_t buffer_length);

private:
    //接收数据
    void do_receive();

    void clear_write_buffer();

    bool add_serial_port(asio::io_context* io_context, const std::string& tty_name, uint16 tty_port, uint8 char_size);

private:
    asio::io_context* io_context_ = nullptr;
    std::string tty_name_;

    std::shared_ptr<asio::serial_port> serial_port_param_= nullptr; //串口IO句柄

    uint32 connect_id_ = 0;
    uint32 server_id_ = 0;
    uint32 io_bradge_connect_id_ = 0;

    _ClientIPInfo remote_ip_;
    _ClientIPInfo local_ip_;

    size_t recv_data_size_ = 0;
    size_t send_data_size_ = 0;
    std::chrono::steady_clock::time_point recv_data_time_ = std::chrono::steady_clock::now();

    CSessionBuffer session_recv_buffer_;
    CSessionBuffer session_send_buffer_;
    shared_ptr<_Packet_Parse_Info> packet_parse_interface_ = nullptr;

    EM_CONNECT_IO_TYPE io_type_ = EM_CONNECT_IO_TYPE::CONNECT_IO_TTY;
    EM_SESSION_STATE io_state_ = EM_SESSION_STATE::SESSION_IO_LOGIC;
};

