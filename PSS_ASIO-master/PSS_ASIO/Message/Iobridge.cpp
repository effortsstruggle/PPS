#include "Iobridge.h"


/**
 * @brief 两个客户端之间的会话
 * @param from_io 
 * @param from_io_type 
 * @param to_io 
 * @param to_io_type 
 * @param bridge_type 
 * @return 
*/
bool CIoBridge::add_session_io_mapping(const _ClientIPInfo& from_io, EM_CONNECT_IO_TYPE from_io_type, const _ClientIPInfo& to_io, EM_CONNECT_IO_TYPE to_io_type, ENUM_IO_BRIDGE_TYPE bridge_type)
{
    auto ret = false;

    ret = this->iotoio_.add_session_io_mapping(from_io, from_io_type, to_io, to_io_type, bridge_type);

    //判断链接是否已存在，如果存在则通知桥接session对象
    auto connect_info = this->iotoio_.find_io_to_io_list( from_io , from_io_type );

    if ( nullptr != connect_info && connect_info->from_session_id_ > 0 && connect_info->to_session_id_ > 0)
    {
        //两边的链接已经存在了
        App_WorkThreadLogic::instance()->set_io_bridge_connect_id( connect_info->from_session_id_ ,  connect_info->to_session_id_ );

        App_WorkThreadLogic::instance()->set_io_bridge_connect_id( connect_info->to_session_id_ ,  connect_info->from_session_id_ );
    }

    return ret;
}

bool CIoBridge::delete_session_io_mapping(const _ClientIPInfo& from_io, EM_CONNECT_IO_TYPE from_io_type)
{
    //查找对应的IO并关闭两端的状态
    auto s_2_s = iotoio_.find_io_to_io_session_info(from_io, from_io_type);
    if (s_2_s.from_session_id_ > 0 && s_2_s.to_session_id_ > 0)
    {
        //将对应链接设置为逻辑模式
        App_WorkThreadLogic::instance()->set_io_bridge_connect_id(0, s_2_s.from_session_id_);
        App_WorkThreadLogic::instance()->set_io_bridge_connect_id(0, s_2_s.to_session_id_);
    }

    return iotoio_.delete_session_io_mapping(from_io, from_io_type);
}

/**
 * @brief  regedit_session_id 注册会话机制
 * @param from_io  客户端IO信息
 * @param io_type 连接类型（Tcp / Udp ）
 * @param session_id  会话ID
 * @return 
*/
bool CIoBridge::regedit_session_id(const _ClientIPInfo& from_io, EM_CONNECT_IO_TYPE io_type, uint32 session_id)
{
    return  iotoio_.regedit_session_id( from_io , io_type , session_id );
}

void CIoBridge::unregedit_session_id(const _ClientIPInfo& from_io, EM_CONNECT_IO_TYPE io_type)
{
    return iotoio_.unregedit_session_id(from_io, io_type);
}

uint32 CIoBridge::get_to_session_id(uint32 session_id, const _ClientIPInfo& from_io)
{
    return iotoio_.get_to_session_id( session_id, from_io );
}
