﻿#pragma once

/**
* 线程处理队列，处理消息队列和定时消息
*/ 
//add by freeeyes

#include "define.h"
#include "ThreadQueue.h"
#include "TimerManager.hpp"
#include "TimeStamp.hpp"
#include <map>
#include "singleton.h"

using namespace std;

enum class EM_LOGIC_TYPE
{
    LOGIC_TYPE_RUN = 0,
    LOGIC_TYPE_CLOSE
};

/**
 * @brief CLogicMessage 逻辑消息
*/
class CLogicMessage
{
public:
    task_function m_func;
    EM_LOGIC_TYPE m_emType = EM_LOGIC_TYPE::LOGIC_TYPE_RUN;
};

/**
 * @brief  CLogicTasK  逻辑任务
*/
class CLogicTasK
{
public:
    CLogicTasK() = default;

    /**
     * @brief 消息入队
     * @param msg 
    */
    void Put(std::shared_ptr<CLogicMessage> msg)
    {
        this->m_thread_queue.Push(msg);
    }

    void start(uint32 u4ThreadID)
    {
        m_run = true;

        m_ttlogic = std::thread(  
                                                [this]()
                                                {
                                                    this->svc();
                                                }
                                            );
        m_u4ThreadID = u4ThreadID;
    }

    void Close()
    {
        m_run = false;

        auto pLogicMessage = std::make_shared<CLogicMessage>();
        pLogicMessage->m_emType = EM_LOGIC_TYPE::LOGIC_TYPE_CLOSE;
        Put(pLogicMessage);

        //PSS_LOGGER_DEBUG("[Close]Thread({0}) put close.", m_u4ThreadID);
        m_ttlogic.join();
    }

    void svc()
    {
        m_tvBegin = CTimeStamp::Get_Time_Stamp();

        while ( this->m_run )
        {
            shared_ptr<CLogicMessage> msg;

            //获取消息
            this->m_thread_queue.Pop( msg );

            if ( EM_LOGIC_TYPE::LOGIC_TYPE_RUN == msg->m_emType )
            {
                //获得了数据，进行处理
                msg->m_func();
            }
            else
            {
                break;
            }
        }

        //PSS_LOGGER_DEBUG("[Close]Thread({0}) is over.", m_u4ThreadID);
    }

    std::thread::id get_thread_id()
    {
        return m_ttlogic.get_id();
    }

private:
    //消息队列
    CMessageQueue< shared_ptr<CLogicMessage> > m_thread_queue;

    bool m_run = false;

    std::thread m_ttlogic;
    
    PSS_Time_Point m_tvBegin;
    
    uint32 m_u4ThreadID = 0;
};

class TMS
{
public:
    TMS() = default;

    //初始化
    void Init()
    {
        //创建定时器线程
        m_ttTimer = std::thread([this]()
            {
                m_timerManager.schedule();
            }
        );
    }

    bool CreateLogic(uint32 u4LogicID)  //创建一个逻辑线程
    {
        auto f = m_mapLogicList.find(u4LogicID);
        if (f != m_mapLogicList.end())
        {
            return false;
        }
        else
        {
            //创建线程
            auto pLogicTask = std::make_shared<CLogicTasK>();

            pLogicTask->start(u4LogicID);

            m_mapLogicList[u4LogicID] = pLogicTask;

            //记录映射关系
            std::thread::id tid = pLogicTask->get_thread_id();
            std::ostringstream thread_id_stream;
            thread_id_stream << tid;
            std::string thread_id_str = thread_id_stream.str();

            m_TidtologicidList[thread_id_str] = u4LogicID;
            //cout << "CreateLogic(" << u4LogicID << ")." << endl;
        }

        return true;
    };

    uint32 GetLogicThreadID()
    {
        std::thread::id tid = std::this_thread::get_id();
        std::ostringstream thread_id_stream;
        thread_id_stream << tid;
        std::string thread_id_str = thread_id_stream.str();

        auto f = m_TidtologicidList.find(thread_id_str);
        if (f != m_TidtologicidList.end())
        {
            return f->second;
        }
        else
        {
            //没有找到工作线程
            return 0;
        }
    }

    //关闭一个逻辑线程
    bool CloseLogic(uint32 u4LogicID)  
    {
        auto f = m_mapLogicList.find(u4LogicID);
        if (f != m_mapLogicList.end())
        {
            auto pLogicTask = f->second;
            
            //关闭映射关系
            std::thread::id tid = pLogicTask->get_thread_id();
            std::ostringstream thread_id_stream;
            thread_id_stream << tid;
            std::string thread_id_str = thread_id_stream.str();
            m_TidtologicidList.erase(thread_id_str);
            
            pLogicTask->Close();
            m_mapLogicList.erase(f);
            return true;
        }
        else
        {
            return false;
        }
    }

    /**
     * @brief AddMessage 添加消息(即时)
     * @param u4LogicID  消息ID
     * @param func 所对应的任务执行函数
     * @return 
    */
    bool AddMessage(uint32 u4LogicID, task_function func)
    {
        auto f = this->m_mapLogicList.find( u4LogicID );

        if ( f != this->m_mapLogicList.end( ) )
        {
            auto pLogicMessage = std::make_shared< CLogicMessage >();

            pLogicMessage->m_func = func;
            
            this->m_mapLogicList[ u4LogicID ]->Put( pLogicMessage );
            
            return true;
        }
        else
        {
            return false;
        }
    };

    //添加消息(延时)
    brynet::Timer::WeakPtr AddMessage(uint32 u4LogicID, std::chrono::milliseconds millisecond, task_function func)
    {
        brynet::Timer::WeakPtr timer;
        auto f = m_mapLogicList.find(u4LogicID);
        if (f != m_mapLogicList.end())
        {
            auto pLogicMessage = std::make_shared<CLogicMessage>();
            pLogicMessage->m_func = func;

            timer = m_timerManager.addTimer(millisecond, [this, u4LogicID, pLogicMessage]() {
                m_mapLogicList[u4LogicID]->Put(pLogicMessage);
                //cout << "Timer execute is ok." << endl;
                });

            //cout << "Timer add is ok." << endl;
        }

        return timer;
    }

    //添加消息(定时器)
    brynet::Timer::WeakPtr AddMessage_loop(uint32 u4LogicID, std::chrono::seconds delayseconds, std::chrono::milliseconds millisecond, task_function func)
    {
        brynet::Timer::WeakPtr timer;
        auto f = m_mapLogicList.find(u4LogicID);
        if (f != m_mapLogicList.end())
        {
            auto pLogicMessage = std::make_shared<CLogicMessage>();
            pLogicMessage->m_func = func;

            timer = m_timerManager.addTimer_loop(delayseconds, millisecond, [this, u4LogicID, pLogicMessage]() {
                m_mapLogicList[u4LogicID]->Put(pLogicMessage);
                //cout << "Timer execute is ok." << endl;
                });

            //cout << "Timer add is ok." << endl;
        }

        return timer;
    }

    //关闭系统
    void Close()
    {
        for_each( this->m_mapLogicList.begin() ,  this->m_mapLogicList.end() , 
            [ ] (const std::pair<uint32, shared_ptr<CLogicTasK>>& iter) 
            {
                iter.second->Close();
            }
        );

        //关闭定时器
        m_timerManager.Close();

        m_ttTimer.join();
    }

private:
    using mapthreads = map<uint32, std::shared_ptr<CLogicTasK> >;
    using mapthreadidtologicid = map<std::string, uint32>;
    
    mapthreads m_mapLogicList;
  
    mapthreadidtologicid m_TidtologicidList;
    
    std::thread m_ttTimer;

    brynet::TimerMgr m_timerManager;
};

using App_tms = PSS_singleton<TMS>;

