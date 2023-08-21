#pragma once
#include <string>
#include <vector>
#include "define.h"

/**
*   配置文件参数信息数据结构定义
*/
//定义参数类型表 

//工作线程配置类
class CConfigWorkThread
{
public:
    int linux_daemonize_ = 0;           //0是不守护，前台执行，1是守护，后台执行
    int work_thread_count_ = 1;         //工作线程数
    int work_timeout_seconds_ = 60;     //工作超时
    int s2s_timeout_seconds_ = 60;      // s2s超时
    int client_connect_timeout_ = 0;    //客户端连接超时
    int io_send_time_check_ = 0;        
};

//协议解析配置类（动态库）
class CConfigPacketParseInfo
{
public:
    int packet_parse_id_ = 0; //id
    std::string packet_parse_path_; //解析文件路径
    std::string packet_parse_file_name_; //解析文件名称
};
using config_packet_list = std::vector<CConfigPacketParseInfo>;

/**
 * @brief CConfigLogicInfo 业务逻辑配置类
*/
class CConfigLogicInfo
{
public:
    std::string logic_path_;//路径
    std::string logic_file_name_;//文件名称
    std::string logic_param_; //参数
};
using config_logic_list = std::vector<CConfigLogicInfo>;

// tcp/udp/kcp  IO配置列表
using config_tcp_list = std::vector<CConfigNetIO>;
using config_udp_list = std::vector<CConfigNetIO>;
using config_kcp_list = std::vector<CConfigNetIO>;

/**
 * @brief CTTyIO  TTy IO配置类（电传打印机）
*/
class CTTyIO
{
public:
    std::string tty_name_;
    unsigned int tty_port_ = 9000;
    int char_size_ = 8;
    unsigned int packet_parse_id_ = 0;
    unsigned int recv_buff_size_ = 1024;
    unsigned int send_buff_size_ = 1024;
};
using config_tty_list = std::vector<CTTyIO>;

/**
 * @brief  CConfigConsole 输出配置类
*/
class CConfigConsole
{
public:
    bool file_output_ = false;
    int file_count_ = 3;
    int max_file_size_ = 1048576;
    std::string file_name_;
    std::string output_level_ = "debug";
};
