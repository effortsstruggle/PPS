#include "LoadPacketParse.h"



CLoadPacketParse::~CLoadPacketParse()
{
    //释放所有动态库
    this->close();
}

/**
 * @brief dispaly_error_message 加载失败，显示错误信息
 * @param func_name 
 * @param packet_parse_file 
 * @param pPacketParseInfo 
*/
void CLoadPacketParse::dispaly_error_message(const std::string& func_name, 
                                                                            const std::string& packet_parse_file, 
                                                                            std::shared_ptr<_Packet_Parse_Info> pPacketParseInfo)  const
{
    PSS_LOGGER_DEBUG("[ CLoadPacketParse::dispaly_error_message ] PacketID={0} strModuleName = {1}, Function {2} is error!", 
                                        pPacketParseInfo->m_u4PacketParseID,
                                        func_name,
                                        packet_parse_file);
    if (nullptr != pPacketParseInfo->m_hModule)
    {
        //关闭dll
        CLoadLibrary::PSS_dlClose( pPacketParseInfo->m_hModule );
    }
}

/**
* @breif:  LoadPacketInfo 加载包解析插件信息 (从配置文件获取)
* @author: wq
* @date: 2023/08/08
* @param1: u4PacketParseID   唯一标识
* @param2: packet_parse_path 路径
* @param3: packet_parse_file 解析文件名称（动态库）
*/
bool CLoadPacketParse::LoadPacketInfo( uint32 u4PacketParseID, const std::string& packet_parse_path, const std::string& packet_parse_file)
{
    //隐式加载PacketParse接口
    auto pPacketParseInfo = std::make_shared< _Packet_Parse_Info >();

    if (nullptr == pPacketParseInfo)
    {
        PSS_LOGGER_DEBUG("[CLoadPacketParse::LoadPacketInfo] pPacketParseInfo is nullptr!");
        return false;
    }

#ifdef GCOV_TEST
    dispaly_error_message("test", "test logic", pPacketParseInfo);
#endif
    //解析动态库唯一ID标识
    pPacketParseInfo->m_u4PacketParseID   = u4PacketParseID;
    
    std::string strFilePath;
    strFilePath = fmt::format("{0}{1}", packet_parse_path, packet_parse_file);

    //加载动态库模块
    pPacketParseInfo->m_hModule = CLoadLibrary::PSS_dlopen(strFilePath.c_str(), RTLD_NOW);

    if(nullptr == pPacketParseInfo->m_hModule)
    {
        PSS_LOGGER_DEBUG("[CLoadPacketParse::LoadPacketInfo] strModuleName = {0}, pModuleInfo->hModule is nullptr({1})!", packet_parse_file, CLoadLibrary::PSS_dlerror());
        return false;
    }

    //获取解析函数的接口地址（函数指针）
    pPacketParseInfo->packet_from_recv_buffer_ptr_ = (packet_from_recv_buffer)CLoadLibrary::PSS_dlsym(pPacketParseInfo->m_hModule, "parse_packet_from_recv_buffer");
    
    if(nullptr == pPacketParseInfo->m_hModule || !pPacketParseInfo->packet_from_recv_buffer_ptr_)
    {
        this->dispaly_error_message("parse_packet_from_recv_buffer", packet_parse_file, pPacketParseInfo);
        return false;
    }

    pPacketParseInfo->is_need_send_format_ptr_ = (is_need_send_format)CLoadLibrary::PSS_dlsym(pPacketParseInfo->m_hModule, "is_need_send_format");
    if (nullptr == pPacketParseInfo->m_hModule || !pPacketParseInfo->is_need_send_format_ptr_)
    {
        dispaly_error_message("is_need_send_format", packet_parse_file, pPacketParseInfo);
        return false;
    }

    pPacketParseInfo->parse_format_send_buffer_ptr_ = (parse_format_send_buffer)CLoadLibrary::PSS_dlsym(pPacketParseInfo->m_hModule, "parse_packet_format_send_buffer");
    if (nullptr == pPacketParseInfo->m_hModule || !pPacketParseInfo->parse_format_send_buffer_ptr_)
    {
        dispaly_error_message("parse_packet_format_send_buffer", packet_parse_file, pPacketParseInfo);
        return false;
    }

    pPacketParseInfo->packet_connect_ptr_ = (packet_connect)CLoadLibrary::PSS_dlsym(pPacketParseInfo->m_hModule, "connect");

    if(nullptr == pPacketParseInfo->m_hModule || !pPacketParseInfo->packet_connect_ptr_)
    {
        dispaly_error_message("Connect", packet_parse_file, pPacketParseInfo);
        return false;
    }
    
    pPacketParseInfo->packet_disconnect_ptr_ = (packet_disconnect)CLoadLibrary::PSS_dlsym(pPacketParseInfo->m_hModule, "disconnect");

    if(nullptr == pPacketParseInfo->m_hModule || !pPacketParseInfo->packet_disconnect_ptr_)
    {
        dispaly_error_message("DisConnect", packet_parse_file, pPacketParseInfo);
        return false;
    }

    pPacketParseInfo->packet_load_ptr_ = (packet_load)CLoadLibrary::PSS_dlsym(pPacketParseInfo->m_hModule, "packet_load");

    if (nullptr == pPacketParseInfo->m_hModule || !pPacketParseInfo->packet_load_ptr_)
    {
        dispaly_error_message("Load", packet_parse_file, pPacketParseInfo);
        return false;
    }

    pPacketParseInfo->packet_close_ptr_ = (packet_close)CLoadLibrary::PSS_dlsym(pPacketParseInfo->m_hModule, "packet_close");

    if (nullptr == pPacketParseInfo->m_hModule || !pPacketParseInfo->packet_close_ptr_)
    {
        dispaly_error_message("Close", packet_parse_file, pPacketParseInfo);
        return false;
    }

    pPacketParseInfo->packet_set_output_ptr_ = (packet_set_output)CLoadLibrary::PSS_dlsym(pPacketParseInfo->m_hModule, "set_output");

    if (nullptr == pPacketParseInfo->m_hModule || !pPacketParseInfo->packet_set_output_ptr_)
    {
        dispaly_error_message("Set_output", packet_parse_file, pPacketParseInfo);
        return false;
    }

    //建立Hash映射
    this->m_objPacketParseList[ pPacketParseInfo->m_u4PacketParseID ] =  pPacketParseInfo;


    /**
    *  以下两个调用，需要查看其他封装的动态库
    */
    //设置”第三方动态库“的输出对象
    pPacketParseInfo->packet_set_output_ptr_( spdlog::default_logger() );
    //调用初始化
    pPacketParseInfo->packet_load_ptr_(  App_IoBridge::instance() );

    PSS_LOGGER_DEBUG("[CLoadPacketParse::LoadPacketInfo] load {0} OK!", packet_parse_file);
    return true;
}

/**
 * @brief  GetPacketParseInfo 获取“协议解析库”
 * @param u4PacketParseID 
 * @return 
*/
shared_ptr<_Packet_Parse_Info> CLoadPacketParse::GetPacketParseInfo(uint32 u4PacketParseID)
{
    auto f = this->m_objPacketParseList.find( u4PacketParseID );

    if( this->m_objPacketParseList.end() == f )
    {
        //这里打印输出错误
        PSS_LOGGER_DEBUG("[CLoadPacketParse::GetPacketParseInfo]can't find u4PacketParseID({0}).", u4PacketParseID);
        return nullptr;
    }
    else
    {
        return f->second;
    }
}

void CLoadPacketParse::close()
{
    PSS_LOGGER_DEBUG("[CLoadPacketParse::Close]Begin.");
    //清理所有已存在的指针
    for_each( this->m_objPacketParseList.begin() ,
              this->m_objPacketParseList.end(), 
              [](const std::pair<uint32, shared_ptr<_Packet_Parse_Info>>& iter) 
              {
                    //关闭模块接口
                    iter.second->packet_close_ptr_(App_IoBridge::instance());
                    CLoadLibrary::PSS_dlClose(iter.second->m_hModule);
              }
           );

   this->m_objPacketParseList.clear();
    PSS_LOGGER_DEBUG("[CLoadPacketParse::Close]End.");
}
