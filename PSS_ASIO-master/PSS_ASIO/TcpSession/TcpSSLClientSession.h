﻿#pragma once
#ifdef SSL_SUPPORT

#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <asio.hpp>
#include <asio/ssl.hpp>

#include "define.h"
#include "tms.hpp"
#include "SendBuffer.h"
#include "SessionBuffer.hpp"
#include "LoadPacketParse.h"
#include "ConnectCounter.h"
#include "ISession.h"
#include "ModuleLogic.h"
#include "Iobridge.h"

using namespace asio::ip;
using std::placeholders::_1;
using std::placeholders::_2;

class CTcpSSLClientSession : public std::enable_shared_from_this<CTcpSSLClientSession>, public ISession
{
public:
    explicit CTcpSSLClientSession(asio::io_context* io_context);

    bool start(const CConnect_IO_Info& io_info);

    virtual void set_io_bridge_connect_id(uint32 from_io_connect_id, uint32 to_io_connect_id) override final;

    virtual _ClientIPInfo get_remote_ip(uint32 connect_id) override final;
    
    virtual void set_write_buffer(uint32 connect_id, const char* data, size_t length) override final; //写入些缓冲

    virtual void do_write(uint32 connect_id) override final;
    
    virtual void do_write_immediately(uint32 connect_id, const char* data, size_t length) override final;

    virtual void close(uint32 connect_id) override final;

    virtual void add_send_finish_size(uint32 connect_id, size_t send_length)  override final;

    virtual EM_CONNECT_IO_TYPE get_io_type() override final;

    virtual uint32 get_mark_id(uint32 connect_id) override final;

    virtual std::chrono::steady_clock::time_point& get_recv_time(uint32 connect_id = 0) override final;

    virtual bool format_send_packet(uint32 connect_id, std::shared_ptr<CMessage_Packet> message, std::shared_ptr<CMessage_Packet> format_message) override final;

    virtual bool is_need_send_format() override final;

    void do_read();

    void clear_write_buffer();

    void do_read_some(std::error_code ec, std::size_t length);

    void send_write_fail_to_logic(const std::string& write_fail_buffer, std::size_t buffer_length);

private:
    bool verify_certificate(bool preverified, asio::ssl::verify_context& ctx);

    void handshake();

private:
    asio::io_context* io_context_ = nullptr;
    asio::ssl::stream<tcp::socket> ssl_socket_; //tcp 
    asio::ssl::context ssl_ctx_;
    uint32 packet_parse_id_ = 0;

    uint32 server_id_  = 0;
    uint32 connect_id_ = 0;
    uint32 io_bradge_connect_id_ = 0;
    CSessionBuffer session_recv_buffer_;
    CSessionBuffer session_send_buffer_;

    _ClientIPInfo remote_ip_;
    _ClientIPInfo local_ip_;

    shared_ptr<_Packet_Parse_Info> packet_parse_interface_ = nullptr;

    size_t recv_data_size_  = 0;
    size_t send_data_size_  = 0;
    std::chrono::steady_clock::time_point recv_data_time_ = std::chrono::steady_clock::now();

    EM_CONNECT_IO_TYPE io_type_ = EM_CONNECT_IO_TYPE::CONNECT_IO_SSL;
    EM_SESSION_STATE io_state_ = EM_SESSION_STATE::SESSION_IO_LOGIC;
};
#endif
