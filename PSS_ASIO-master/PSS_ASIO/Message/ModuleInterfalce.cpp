#include "ModuleInterfalce.h"

/**
 * @brief copy_from_module_list 复制[模块指令,模块处理参数列表]
 * @param command_to_module_function 
*/
void CModuleInterface::copy_from_module_list(const command_to_module_function& command_to_module_function)
{
    this->command_to_module_function_.clear();

    this->command_to_module_function_ = command_to_module_function;
}

/**
 * @brief do_module_message 处理指令接口
 * @param source 
 * @param recv_packet 
 * @param send_packet 
 * @return 
*/
int CModuleInterface::do_module_message(const CMessage_Source& source, std::shared_ptr<CMessage_Packet> recv_packet, std::shared_ptr<CMessage_Packet> send_packet)
{
    auto f = this->command_to_module_function_.find( recv_packet->command_id_ );
    if (f != this->command_to_module_function_.end()) //存在该指令
    {
        //执行插件函数 (插件的do_module_message 或 指定的指令的接口)
        return f->second(source, recv_packet, send_packet);
    }
    else //不存在该指令
    {
        PSS_LOGGER_WARN("[CModuleInterface::do_module_message]no find command_id({0}-->{1})", source.connect_id_, recv_packet->command_id_);
        return -1;
    }
}

void CModuleInterface::close()
{
    command_to_module_function_.clear();
}
