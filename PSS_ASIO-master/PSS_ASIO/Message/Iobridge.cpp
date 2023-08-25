#include "Iobridge.h"


/**
 * @brief �����ͻ���֮��ĻỰ
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

    //�ж������Ƿ��Ѵ��ڣ����������֪ͨ�Ž�session����
    auto connect_info = this->iotoio_.find_io_to_io_list( from_io , from_io_type );

    if ( nullptr != connect_info && connect_info->from_session_id_ > 0 && connect_info->to_session_id_ > 0)
    {
        //���ߵ������Ѿ�������
        App_WorkThreadLogic::instance()->set_io_bridge_connect_id( connect_info->from_session_id_ ,  connect_info->to_session_id_ );

        App_WorkThreadLogic::instance()->set_io_bridge_connect_id( connect_info->to_session_id_ ,  connect_info->from_session_id_ );
    }

    return ret;
}

bool CIoBridge::delete_session_io_mapping(const _ClientIPInfo& from_io, EM_CONNECT_IO_TYPE from_io_type)
{
    //���Ҷ�Ӧ��IO���ر����˵�״̬
    auto s_2_s = iotoio_.find_io_to_io_session_info(from_io, from_io_type);
    if (s_2_s.from_session_id_ > 0 && s_2_s.to_session_id_ > 0)
    {
        //����Ӧ��������Ϊ�߼�ģʽ
        App_WorkThreadLogic::instance()->set_io_bridge_connect_id(0, s_2_s.from_session_id_);
        App_WorkThreadLogic::instance()->set_io_bridge_connect_id(0, s_2_s.to_session_id_);
    }

    return iotoio_.delete_session_io_mapping(from_io, from_io_type);
}

/**
 * @brief  regedit_session_id ע�� session
 * @param from_io  �ͻ���IO��Ϣ
 * @param io_type �������ͣ�Tcp / Udp ��
 * @param session_id  �ỰID
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
