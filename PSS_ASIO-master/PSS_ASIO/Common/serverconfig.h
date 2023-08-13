#pragma once

#include <iostream>
#include <fstream>
#include "json/json.hpp"
#include "serverconfigtype.h"
#include "define.h"

const std::string config_file_name = "server_config.json";

using json = nlohmann::json;

/**
* @breif CServerConfig 读取服务器配置
*/
class CServerConfig
{
public:
    CServerConfig() = default; //默认构造函数

    bool read_server_config_file(const std::string& file_name = config_file_name);

    config_packet_list& get_config_packet_list();

    config_logic_list& get_config_logic_list();
    
    config_tcp_list& get_config_tcp_list();
    
    config_udp_list& get_config_udp_list();
    
    config_kcp_list& get_config_kcp_list();
    
    config_tty_list& get_config_tty_list();
    
    CConfigConsole& get_config_console();
    
    CConfigWorkThread& get_config_workthread();

private:
    config_packet_list config_packet_list_; //包解析配置
    
    config_logic_list config_logic_list_;
    
    config_tcp_list config_tcp_list_;
    
    config_udp_list config_udp_list_;
    
    config_kcp_list config_kcp_list_;
    
    config_tty_list config_tty_list_;
    
    CConfigConsole config_output_; //控制台输出配置
    
    CConfigWorkThread config_work_thread_;
};
//单例模式
using App_ServerConfig = PSS_singleton<CServerConfig>;
