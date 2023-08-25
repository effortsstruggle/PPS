#pragma once

#include <iostream>
#include "define.h"
#include "asio.hpp"
#include "tms.hpp"

#include "Iobridge.h"
#include "SendBuffer.h"
#include "SessionBuffer.hpp"
#include "LoadPacketParse.h"
#include "ConnectCounter.h"
#include "ISession.h"
#include "ModuleLogic.h"

using asio::ip::tcp;

class CTcpClientSession : public std::enable_shared_from_this<CTcpClientSession>, public ISession
{
public:
    explicit CTcpClientSession(asio::io_context* io_context);

    bool start(const CConnect_IO_Info& io_info);

    virtual void set_io_bridge_connect_id(uint32 from_io_connect_id, uint32 to_io_connect_id)  override final;

    virtual _ClientIPInfo get_remote_ip(uint32 connect_id) override final;

    virtual void set_write_buffer(uint32 connect_id, const char* data, size_t length) override final; //写入些缓冲
    
    virtual void do_write(uint32 connect_id) override final;
    
    virtual void do_write_immediately(uint32 connect_id, const char* data, size_t length) override final;

    virtual void close(uint32 connect_id)  override final;

    virtual void add_send_finish_size(uint32 connect_id, size_t send_length) override final;

    virtual EM_CONNECT_IO_TYPE get_io_type() override final;

    virtual uint32 get_mark_id(uint32 connect_id) override final;

    virtual std::chrono::steady_clock::time_point& get_recv_time(uint32 connect_id = 0) override final;

    virtual bool format_send_packet(uint32 connect_id, std::shared_ptr<CMessage_Packet> message, std::shared_ptr<CMessage_Packet> format_message) override final;

    virtual bool is_need_send_format() override final;

    virtual bool is_connect() override final;

    void clear_write_buffer();

    void do_read_some(std::error_code ec, std::size_t length);

    void handle_connect(const asio::error_code& ec, tcp::resolver::results_type::iterator endpoint_iter);

    void send_write_fail_to_logic(const std::string& write_fail_buffer, std::size_t buffer_length);

    void do_io_bridge_error_to_logic(std::shared_ptr<CMessage_Packet> bridge_packet);

private:
    void do_read();

private:
    tcp::socket socket_; //tpc 套接字

    asio::io_context* io_context_ = nullptr;
    uint32 server_id_  = 0; //服务器ID

    uint32 connect_id_ = 0; //连接ID

    uint32 io_bradge_connect_id_ = 0; //与当前会话绑定的桥接ID

    bool is_connect_ = false; //是否成功建立连接标志
    

    CSessionBuffer session_recv_buffer_;    //接收缓冲区
    CSessionBuffer session_send_buffer_;    //发送缓冲区
    size_t recv_data_size_ = 0; 
    size_t send_data_size_ = 0;
    std::chrono::steady_clock::time_point recv_data_time_ = std::chrono::steady_clock::now(); //接收数据耗时

    _ClientIPInfo remote_ip_;    //服务器IP
    _ClientIPInfo local_ip_;   //客户端IP
    shared_ptr<_Packet_Parse_Info> packet_parse_interface_ = nullptr; //协议解析动态库



    EM_CONNECT_IO_TYPE io_type_ = EM_CONNECT_IO_TYPE::CONNECT_IO_SERVER_TCP; //io 类型

    EM_SESSION_STATE io_state_ = EM_SESSION_STATE::SESSION_IO_LOGIC;  //io状态 ？？？？
};
