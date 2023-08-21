#ifndef PSS_CONSOLE_OUTPUT_H
#define PSS_CONSOLE_OUTPUT_H

//��Ļ����ܿ�
//add by freeyes

//����ʹ�õ������ĸ�ʽ������
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/fmt/bin_to_hex.h"

#include "singleton.h"

namespace spd = spdlog;

//���������
#define PSS_LOGGER_DEBUG(...) SPDLOG_LOGGER_DEBUG(spdlog::default_logger(), __VA_ARGS__)
#define PSS_LOGGER_INFO(...) SPDLOG_LOGGER_INFO(spdlog::default_logger(), __VA_ARGS__)
#define PSS_LOGGER_WARN(...) SPDLOG_LOGGER_WARN(spdlog::default_logger(), __VA_ARGS__)
#define PSS_LOGGER_ERROR(...) SPDLOG_LOGGER_ERROR(spdlog::default_logger(), __VA_ARGS__)

//���ָ�����ȵĶ���������
inline void pss_output_binary(const char* buffer, int begin, int end)
{
    spdlog::default_logger()->info("[data]{:n}.", spdlog::to_hex(buffer + begin, buffer + end));
}

//������ýṹ
class Console_Output_Info
{
public:
    bool m_blTunOn               = true;              //trueΪ�ļ����, falseΪ��Ļ���
    int  m_nLogFileMaxSize       = 1024000;           
    int  m_nFileCount            = 0;
    std::string m_strConsoleName = "./consoleoutput";
    std::string m_strLevel       = "info";
};

/**
 * @brief CConsoleOutput ��������� (�о� ��־��� spdlog��)
*/
class CConsoleOutput
{
public:
    CConsoleOutput() = default;

    void Init( Console_Output_Info obj_Console_Output_Info )
    {
        spdlog::drop_all();

        this->m_Console_Output_Info = obj_Console_Output_Info;

        std::shared_ptr<spdlog::logger> console = nullptr;

        if ( ! this->m_Console_Output_Info.m_blTunOn  )
        {
            //��Ļ���
            console = spdlog::stdout_color_mt("console");
        }
        else
        {
            //�ļ����
            console = spdlog::rotating_logger_mt("console",
                this->m_Console_Output_Info.m_strConsoleName.c_str(),
                this->m_Console_Output_Info.m_nLogFileMaxSize,
                this->m_Console_Output_Info.m_nFileCount);
        }

        //�������̧ͷ
        console->set_pattern("[%Y-%m-%d %H:%M:%S.%e:%t][%@] %v");
        
        spd::level::level_enum console_level = spdlog::level::info;
        //��������ȼ�
        if ( this->m_Console_Output_Info.m_strLevel == "info" )
        {
            console_level = spdlog::level::info;
        }
        else if (this->m_Console_Output_Info.m_strLevel == "debug")
        {
            console_level = spdlog::level::debug;
        }
        else if (this->m_Console_Output_Info.m_strLevel == "warn")
        {
            console_level = spdlog::level::warn;
        }
        else if (this->m_Console_Output_Info.m_strLevel == "error")
        {
            console_level = spdlog::level::err;
        }

        console->set_level(console_level);

        console->flush_on(console_level);
        
        spdlog::set_default_logger(console);
    };

private:
    Console_Output_Info m_Console_Output_Info;
};

using App_ConsoleOutput = PSS_singleton<CConsoleOutput>;


#endif
