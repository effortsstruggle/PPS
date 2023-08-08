#pragma once

#include "define.h"
#include "TimerManager.hpp"
#include "singleton.h"

/**
* PSS_Timer_Manager 框架定时器
*/
class PSS_Timer_Manager
{
public:
    void Start()
    {
        this->m_timerMgr = std::make_shared<brynet::TimerMgr>();

        this->m_ttTimerThread = std::thread( 
            [this]()
            {
                //线程执行函数
                this->m_timerMgr->schedule(); 

                PSS_LOGGER_DEBUG("[PSS_Timer_Manager::start]End.");
            } 
        );
    }

    void Close()
    {
        if (nullptr != this->m_timerMgr)
        {
            this->m_timerMgr->Close();
            this->m_ttTimerThread.join();
        }
    }

    brynet::TimerMgr::Ptr GetTimerPtr() const
    {
        return this->m_timerMgr;
    }

private:
    brynet::TimerMgr::Ptr m_timerMgr;
    std::thread           m_ttTimerThread;
};

using App_TimerManager = PSS_singleton<PSS_Timer_Manager>;
