﻿#include "serverconfig.h"

bool CServerConfig::read_server_config_file(const std::string& file_name)
{
    try
    {
        //打印当前工作目录
        //char buffer[256];
        //char* val = _getcwd(buffer, sizeof(buffer));
        //if (val) {
        //    std::cout << buffer << std::endl;
        //}

        //读取文件 （当前工作目录/文件） -- 需要修改当前工作目录
        std::ifstream config_input( file_name );

        if ( ! config_input.good() )
        {
            log_screen("[CServerConfig::read_server_config_file] ", file_name, " file no exist");
            return false;
        }

        //string str;
        //int lineNum = 0; 
        //while ( getline(config_input , str) ) {
        //    std::cout << "Read Data on Line:[" << ++lineNum << "] :" << str << std::endl;
        //}
        //std::cout << "file has line:" << lineNum << std::endl;
      

        //字节流反序列化成json对象
        json json_config = json::parse(config_input);

        //读取相关参数

        //工作线程
        auto config_work_thread = json_config["work thread"];
        config_work_thread_.work_thread_count_ = config_work_thread["work thread count"];
        config_work_thread_.work_timeout_seconds_ = config_work_thread["work time check"];
        config_work_thread_.s2s_timeout_seconds_ = config_work_thread["server to server time check"];
        config_work_thread_.client_connect_timeout_ = config_work_thread["client connect timeout"];
        config_work_thread_.linux_daemonize_ = config_work_thread["linux daemonize"];
        config_work_thread_.io_send_time_check_ = config_work_thread["IO send data check"];

        //包解析dll库
        for ( auto packet_parse : json_config["packet parse library"] )
        {
            CConfigPacketParseInfo config_packet;
            config_packet.packet_parse_id_ = packet_parse["packet parse id"];
            config_packet.packet_parse_path_ = packet_parse["packet parse path"];
            config_packet.packet_parse_file_name_ = packet_parse["packet parse file"];
            config_packet_list_.emplace_back(config_packet);
        }

        //logic dll库
        for ( auto logic_parse : json_config["logic library"] )
        {
            CConfigLogicInfo config_logic;
            config_logic.logic_path_ = logic_parse["logic path"];
            config_logic.logic_file_name_ = logic_parse["logic file"];
            config_logic.logic_param_ = logic_parse["logic param"];
            config_logic_list_.emplace_back(config_logic);
        }

        //tcp服务接口信息
        for ( auto tcp : json_config["tcp server"] )
        {
            CConfigNetIO config_netio;
            config_netio.ip_ = tcp["tcp ip"];
            config_netio.port_ = tcp["tcp port"];
            config_netio.packet_parse_id_ = tcp["packet parse id"];
            config_netio.recv_buff_size_ = tcp["recv buff size"];
            config_netio.send_buff_size_ = tcp["send buff size"];

            //读取ssl相关配置
            if (tcp["ssl server password"] != nullptr)
            {
                config_netio.ssl_server_password_ = tcp["ssl server password"];
            }

            if (tcp["ssl server pem file"] != nullptr)
            {
                config_netio.ssl_server_pem_file_ = tcp["ssl server pem file"];
            }

            if (tcp["ssl dh pem file"] != nullptr)
            {
                config_netio.ssl_dh_pem_file_ = tcp["ssl dh pem file"];
            }

            config_tcp_list_.emplace_back(config_netio);
        }

        //udp服务接口信息
        for ( auto udp : json_config["udp server"] )
        {
            CConfigNetIO config_netio;
            config_netio.ip_ = udp["udp ip"];
            config_netio.port_ = udp["udp port"];
            config_netio.packet_parse_id_ = udp["packet parse id"];
            config_netio.recv_buff_size_ = udp["recv buff size"];
            config_netio.send_buff_size_ = udp["send buff size"];
            if (udp["udp type"] == "BROADCAST")
            {
                config_netio.em_net_type_ = EM_NET_TYPE::NET_TYPE_BROADCAST;
            }
            config_udp_list_.emplace_back(config_netio);
        }

        //kcp服务接口信息
        for ( auto kcp : json_config["kcp server"] )
        {
            CConfigNetIO config_netio;
            config_netio.ip_ = kcp["udp ip"];
            config_netio.port_ = kcp["udp port"];
            config_netio.packet_parse_id_ = kcp["packet parse id"];
            config_netio.recv_buff_size_ = kcp["recv buff size"];
            config_netio.send_buff_size_ = kcp["send buff size"];

            config_kcp_list_.emplace_back(config_netio);
        }

        //tty服务接口信息
        for ( auto tty : json_config["tty server"] )
        {
            CTTyIO config_tty;
            config_tty.tty_name_ = tty["port name"];
            config_tty.tty_port_ = tty["serial port"];
            config_tty.char_size_ = tty["char size"];
            config_tty.packet_parse_id_ = tty["packet parse id"];
            config_tty.recv_buff_size_ = tty["recv buff size"];
            config_tty.send_buff_size_ = tty["send buff size"];
            config_tty_list_.emplace_back(config_tty);
        }

        //输出配置
        auto config_output = json_config["console output"];
        config_output_.file_output_ = config_output["file write"];
        config_output_.file_count_ = config_output["log file count"];
        config_output_.max_file_size_ = (unsigned int)( config_output["max log file size"] ) * 1024;
        config_output_.file_name_ = config_output["file name"];
        config_output_.output_level_ = config_output["output level"];

        //关闭输入流
        config_input.close();

        return true;
    }
    catch ( const json::parse_error& e )
    {
        log_screen("[CServerConfig::read_server_config_file] parse error(", e.what(), ")");
        return false;
    }
}

config_packet_list& CServerConfig::get_config_packet_list()
{
    return config_packet_list_ ;
}


config_logic_list& CServerConfig::get_config_logic_list()
{
    return config_logic_list_ ;
}

config_tcp_list& CServerConfig::get_config_tcp_list()
{
    return config_tcp_list_ ;
}

config_udp_list& CServerConfig::get_config_udp_list()
{
    return config_udp_list_ ;
}

config_kcp_list& CServerConfig::get_config_kcp_list()
{
    return config_kcp_list_ ;
}

config_tty_list& CServerConfig::get_config_tty_list()
{
    return config_tty_list_ ;
}


CConfigConsole& CServerConfig::get_config_console()
{
    return config_output_ ;
}

CConfigWorkThread& CServerConfig::get_config_workthread()
{
    return config_work_thread_ ;
}
