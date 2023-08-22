#pragma once

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

/**
 * @brief 消息类型
*/
enum class EM_LOGIC_TYPE
{
    LOGIC_TYPE_RUN = 0, //运行
    LOGIC_TYPE_CLOSE //关闭
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
 * @brief  CLogicTasK  业务逻辑任务
*/
class CLogicTasK
{
public:
    CLogicTasK() = default;

    /**
     * @brief 消息入队
     * @param msg 
    */
    void Put(std::shared_ptr< CLogicMessage > msg)
    {
        this->m_thread_queue.Push(msg);
    }

    /**
     * @brief 开启线程
     * @param u4ThreadID 工作线程ID
    */
    void start(uint32 u4ThreadID)
    {
        this->m_run = true;

        this->m_ttlogic = std::thread(  
                                    [this]()
                                    {
                                        this->svc();
                                    }
                                );

        this->m_u4ThreadID = u4ThreadID;
    }

    void Close()
    {
        this->m_run = false;

        auto pLogicMessage = std::make_shared<CLogicMessage>();
        pLogicMessage->m_emType = EM_LOGIC_TYPE::LOGIC_TYPE_CLOSE;
        
        this->Put(pLogicMessage);

        //PSS_LOGGER_DEBUG("[Close]Thread({0}) put close.", m_u4ThreadID);
        this->m_ttlogic.join();
    }

    /**
     * @brief svc 业务逻辑任务的线程处理函数
    */
    void svc()
    {
        //获取当前时间
        this->m_tvBegin = CTimeStamp::Get_Time_Stamp();

        while ( this->m_run )
        {
            //任务执行的逻辑消息类型
            std::shared_ptr< CLogicMessage > msg;

            //获取消息
            this->m_thread_queue.Pop( msg );

            if ( EM_LOGIC_TYPE::LOGIC_TYPE_RUN == msg->m_emType )
            {
                //获得了数据，进行处理（ 通过"框架定时器" ，添加的需要执行的函数 ）
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
        return this->m_ttlogic.get_id();
    }

private:

    CMessageQueue< std::shared_ptr<CLogicMessage> > m_thread_queue;   //消息队列

    bool m_run = false;

    std::thread m_ttlogic; //“业务逻辑任务”线程
    
    PSS_Time_Point m_tvBegin; //记录当前时间
    
    uint32 m_u4ThreadID = 0; //工作线程ID（业务逻辑ID）
};

class TMS
{
public:
    TMS() = default;

    //初始化
    void Init()
    {
        //创建定时器线程
        this->m_ttTimer = std::thread( [this]()
                                        {
                                            this->m_timerManager.schedule();
                                        }
                                    );
    }

    /**
     * @brief CreateLogic 创建一个逻辑线程
     * @param u4LogicID 工作线程ID
     * @return 
    */
    bool CreateLogic(uint32 u4LogicID)  
    {
        auto f = this->m_mapLogicList.find( u4LogicID );

        if (f != this->m_mapLogicList.end()) //找到该工作线程ID
        {
            return false;
        }
        else //未找到
        {
            //创建“业务逻辑任务”线程，并开启
            auto pLogicTask = std::make_shared<CLogicTasK>();

            pLogicTask->start( u4LogicID );

            this->m_mapLogicList[ u4LogicID ] = pLogicTask;

            //记录[线程ID,逻辑ID]映射关系
            std::thread::id tid = pLogicTask->get_thread_id();
            std::ostringstream thread_id_stream;
            thread_id_stream << tid;
            std::string thread_id_str = thread_id_stream.str();

            this->m_TidtologicidList[thread_id_str] = u4LogicID;
            //cout << "CreateLogic(" << u4LogicID << ")." << endl;
        }

        return true;
    };

    /**
     * @brief GetLogicThreadID 通过当前线程ID，获取对应的逻辑ID
     * @return 
    */
    uint32 GetLogicThreadID()
    {
        //获取当前线程ID
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
     * @param u4LogicID  工作线程ID
     * @param func 所对应的任务执行函数
     * @return 
    */
    bool AddMessage(uint32 u4LogicID, task_function func)
    {
        auto f = this->m_mapLogicList.find( u4LogicID );

        if ( f != this->m_mapLogicList.end( ) ) //工作线程ID已存在
        {
            auto pLogicMessage = std::make_shared< CLogicMessage >();
            pLogicMessage->m_func = func;

            //为该工作线程ID对应的 "业务逻辑任务"，添加需要执行的 消息 ； 
            std::shared_ptr<CLogicTasK> logic_task_ = this->m_mapLogicList[ u4LogicID ] ;
            logic_task_->Put( pLogicMessage );

            return true;

        }
        else //工作线程ID未存在
        {
            return false;
        }
    };



    /**
     * 目前来看：deprecated
     * @brief AddMessage 添加消息(延时) 
     * @param u4LogicID  
     * @param millisecond 
     * @param func 
     * @return 
    */
    brynet::Timer::WeakPtr AddMessage(uint32 u4LogicID, std::chrono::milliseconds millisecond, task_function func)
    {
        brynet::Timer::WeakPtr timer;
        auto f = this->m_mapLogicList.find(u4LogicID);
        
        if (f != this->m_mapLogicList.end() ) //消息已存在
        {
            auto pLogicMessage = std::make_shared<CLogicMessage>();
            pLogicMessage->m_func = func;

            timer = this->m_timerManager.addTimer(millisecond, [this, u4LogicID, pLogicMessage]() {
                this->m_mapLogicList[u4LogicID]->Put(pLogicMessage);
                //cout << "Timer execute is ok." << endl;
                });

            //cout << "Timer add is ok." << endl;
        }

        return timer;
    }

    /**
     * 目前来看：deprecated
     * @brief AddMessage_loop 添加消息(定时器)
     * @param u4LogicID 
     * @param delayseconds 
     * @param millisecond 
     * @param func 
     * @return 
    */
    brynet::Timer::WeakPtr AddMessage_loop(uint32 u4LogicID, std::chrono::seconds delayseconds, std::chrono::milliseconds millisecond, task_function func)
    {
        brynet::Timer::WeakPtr timer;
        auto f = this->m_mapLogicList.find(u4LogicID);
        if (f != this->m_mapLogicList.end())
        {
            auto pLogicMessage = std::make_shared<CLogicMessage>();
            pLogicMessage->m_func = func;

            timer = this->m_timerManager.addTimer_loop(delayseconds, millisecond, [this, u4LogicID, pLogicMessage]() {
                this->m_mapLogicList[u4LogicID]->Put(pLogicMessage);
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
        this->m_timerManager.Close();

        this->m_ttTimer.join();
    }

private:
    using mapthreads = map<uint32, std::shared_ptr<CLogicTasK> >;
    using mapthreadidtologicid = map<std::string, uint32>;
    
    mapthreads m_mapLogicList; //[ 工作线程ID，逻辑任务 ] ； 每一个逻辑任务对应一个线程
  
    mapthreadidtologicid m_TidtologicidList; //[ 真实的线程ID , 工作线程ID ]
    
    std::thread m_ttTimer; //定时器线程
     
    brynet::TimerMgr m_timerManager; //定时器管理者
};

using App_tms = PSS_singleton<TMS>;

