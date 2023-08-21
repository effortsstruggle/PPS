#pragma once

//�ṩIO�Žӷ���ע�ᣨ�ͻ���֮��ͨ�ţ�
//add by freeeyes

#include "IotoIo.h"
#include "ModuleLogic.h"
class CIoBridge : public IIoBridge
{
public:
    virtual ~CIoBridge() = default;

    bool add_session_io_mapping(const _ClientIPInfo& from_io, 
                                                    EM_CONNECT_IO_TYPE from_io_type, 
                                                    const _ClientIPInfo& to_io,
                                                    EM_CONNECT_IO_TYPE to_io_type,
                                                    ENUM_IO_BRIDGE_TYPE bridge_type = ENUM_IO_BRIDGE_TYPE::IO_BRIDGE_BATH) final;

    bool delete_session_io_mapping(const _ClientIPInfo& from_io, EM_CONNECT_IO_TYPE from_io_type) final;

    bool regedit_session_id(const _ClientIPInfo& from_io, EM_CONNECT_IO_TYPE io_type, uint32 session_id);
    void unregedit_session_id(const _ClientIPInfo& from_io, EM_CONNECT_IO_TYPE io_type);
    uint32 get_to_session_id(uint32 session_id, const _ClientIPInfo& from_io);

private:
    CIotoIo iotoio_;

};

using App_IoBridge = PSS_singleton<CIoBridge>;
