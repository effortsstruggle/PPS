#pragma once

#include "define.h"

//add by freeeyes
//�������������ã���������IO�Žӵ�Ч�ʷ��ڵ�һλ��
//��ô����ӿ�Ӧ�ý�����PacketParseȥ����connect��disconnect

//ʵ��IO�Žӵ�����
class IIoBridge
{
public:
    virtual bool add_session_io_mapping(const _ClientIPInfo& from_io, EM_CONNECT_IO_TYPE from_io_type, const _ClientIPInfo& to_io, EM_CONNECT_IO_TYPE to_io_type, ENUM_IO_BRIDGE_TYPE bridge_type = ENUM_IO_BRIDGE_TYPE::IO_BRIDGE_BATH) = 0;
    virtual bool delete_session_io_mapping(const _ClientIPInfo& from_io, EM_CONNECT_IO_TYPE from_io_type) = 0;
};
