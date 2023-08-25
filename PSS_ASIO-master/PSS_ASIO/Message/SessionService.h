#pragma once

//传递给插件的调用接口 (插件Session)
//add by freeeyes

#include "ISessionService.h"
#include "ModuleLogic.h"
#include "serverconfig.h"
#include "QueueService.h"
#include "Iobridge.h"

class CSessionService : public ISessionService
{
public:
    ~CSessionService() final = default;

    virtual void get_server_listen_info(std::vector<CConfigNetIO>& io_list, EM_CONNECT_IO_TYPE io_type) override final;

    virtual void send_io_message(uint32 connect_id, std::shared_ptr<CMessage_Packet> send_packet) override final;

    virtual bool connect_io_server(const CConnect_IO_Info& io_info, EM_CONNECT_IO_TYPE io_type) override final;

    virtual void close_io_session(uint32 connect_id) override final;

    virtual bool send_frame_message(uint16 tag_thread_id, const std::string& message_tag, std::shared_ptr<CMessage_Packet> send_packet, CFrame_Message_Delay delay_timer) override final;

    virtual bool run_work_thread_logic(uint16 tag_thread_id, CFrame_Message_Delay delay_timer, const task_function& func) override final;

    virtual bool create_frame_work_thread(uint32 thread_id) override final;

    virtual bool close_frame_work_thread(uint32 thread_id) override final;

    virtual bool delete_frame_message_timer(uint64 timer_id) override final;

    virtual uint16 get_io_work_thread_count() override final;

    virtual uint16 get_plugin_work_thread_count() override final;

    virtual int module_run(const std::string& module_name, std::shared_ptr<CMessage_Packet> send_packet, std::shared_ptr<CMessage_Packet> return_packet) override final;

    virtual uint32 get_curr_thread_logic_id() override final;
    
    virtual bool add_plugin_api(const std::string& api_name, const plugin_api_logic& func) override final;

    virtual std::string do_plugin_api(const std::string& api_name, const std::string& api_func_param) override final;
   
    virtual bool add_session_io_mapping(const _ClientIPInfo& from_io, EM_CONNECT_IO_TYPE from_io_type, const _ClientIPInfo& to_io, EM_CONNECT_IO_TYPE to_io_type, ENUM_IO_BRIDGE_TYPE bridge_type = ENUM_IO_BRIDGE_TYPE::IO_BRIDGE_BATH) override final;
    virtual bool delete_session_io_mapping(const _ClientIPInfo& from_io, EM_CONNECT_IO_TYPE from_io_type) override final;

    virtual bool create_queue(shm_queue::shm_key key, size_t message_size = shm_queue_list_size, int message_count = shm_queue_list_count) override final;
    virtual bool send_queue_message(shm_queue::shm_key key, const char* message_text, size_t len) override final;
    virtual bool set_close_function(shm_queue::shm_key key, const shm_queue::queue_close_func& close_func) override final;
    virtual bool set_error_function(shm_queue::shm_key key, const shm_queue::queue_error_func& error_func) override final;
    virtual bool set_recv_function(shm_queue::shm_key key, const shm_queue::queue_recv_message_func& fn_logic) override final;
    virtual bool close(shm_queue::shm_key key) override final;

    void close();

private:
    std::unordered_map<std::string, plugin_api_logic> func_list_; //[ api名称，api接口函数 ]

};

using App_SessionService = PSS_singleton<CSessionService>;
