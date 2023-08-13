#ifndef _TIMEMANAGER_H
#define _TIMEMANAGER_H

#include <functional>
#include <queue>
#include <memory>
#include <vector>
#include <chrono>
#include <mutex>
#include <iostream>
#include <condition_variable>
#include <thread>

//当定时器中没有数据，最多等待的时间
const int timer_default_wait = 300;

//定时器组件
namespace brynet {

    enum class EM_TIMER_STATE
    {
        TIMER_STATE_ADD_TIMER = 0, //已添加
        TIMER_STATE_EXECUTE_TIMER, //已执行
    };

    //循环状态
    enum class ENUM_WHILE_STATE
    {
        WHILE_STATE_CONTINUE = 0,
        WHILE_STATE_BREAK,
    };

    //定时器类型
    enum class ENUM_TIMER_TYPE
    {
        TIMER_TYPE_ONCE = 0,
        TIMER_TYPE_LOOP,
    };

    class TimerMgr;

    /**
    *   定时器类
    */
    class Timer final
    {
    public:
        using Ptr = std::shared_ptr<Timer>;
        using WeakPtr = std::weak_ptr<Timer>;
        using Callback = std::function<void(void)>;

        Timer( std::chrono::steady_clock::time_point  startTime,
                    std::chrono::nanoseconds lastTime,
                    std::chrono::seconds delayTime,
                    ENUM_TIMER_TYPE timertype,
                    Callback&& callback  )
                    :   mCallback(std::move(callback)),
                        mStartTime(startTime),
                        mLastTime(lastTime),
                        mDelayTime(delayTime),
                        mTimerType(timertype)
        {


        }

        const std::chrono::steady_clock::time_point& getStartTime() const
        {
            return mStartTime;
        }

        const std::chrono::nanoseconds& getLastTime() const
        {
            return mLastTime;
        }

        std::chrono::nanoseconds getLeftTime()
        {
            //获取时间
            const auto now = std::chrono::steady_clock::now();

            auto delayTime = this->mDelayTime;
            
            this->mDelayTime = std::chrono::seconds(0);
            
            return getLastTime() - ( now - getStartTime() ) + delayTime;
        }

        void  cancel()
        {
            std::call_once( mExecuteOnceFlag ,  [this]() {
                mCallback = nullptr;
                }  
            );
        }

        ENUM_TIMER_TYPE get_timer_type()
        {
            return mTimerType;
        }

    private:


        void operator() ()
        {
            if (mTimerType == ENUM_TIMER_TYPE::TIMER_TYPE_ONCE)
            {
                Callback callback;
                //何时调用？？？？？？？？
                std::call_once( mExecuteOnceFlag ,  [&callback, this]() {
                    //移动
                    callback = std::move(mCallback); 
                    mCallback = nullptr;
                    }
                );

                if (callback != nullptr)
                {
                    callback(); //执行任务； 
                }
            }
            else
            {
                if (mCallback != nullptr)
                {
                    mCallback(); //
                }
                mStartTime = std::chrono::steady_clock::now();
            }
        }

        friend class TimerMgr; //友元类 

        std::once_flag                                               mExecuteOnceFlag; //执行一次的标志
        Callback                                                        mCallback; //定时器执行的任务
        std::chrono::steady_clock::time_point           mStartTime; //开始时间
        std::chrono::nanoseconds                            mLastTime; //结束时间
        std::chrono::seconds                                    mDelayTime; //延迟时间 （到达结束的时间，最多延迟多长时间，以秒为单位）
       
        ENUM_TIMER_TYPE                                     mTimerType = ENUM_TIMER_TYPE::TIMER_TYPE_ONCE;
    };

    /**
   *定时器管理者
   */
    class TimerMgr final
    {

    public:
        using Ptr = std::shared_ptr<TimerMgr>;

        /**
         * @brief  addTimer_loop 添加循环任务
         * @tparam F 
         * @tparam ...TArgs 
         * @param delayseconds 延迟时间
         * @param timeout  超时时间
         * @param callback   回调函数（定时器所执行的任务）
         * @param ...args  回调函数的参数
         * @return 
        */
        template<typename F, typename ...TArgs>
        Timer::WeakPtr  addTimer_loop( std::chrono::seconds  delayseconds , std::chrono::nanoseconds timeout , F&& callback , TArgs&& ...args)
        {
            auto timer = std::make_shared<Timer>(
                                                                                std::chrono::steady_clock::now(),
                                                                                std::chrono::nanoseconds(timeout),
                                                                                delayseconds,
                                                                                ENUM_TIMER_TYPE::TIMER_TYPE_LOOP,
                                                                                std::bind(std::forward<F>(callback), std::forward<TArgs>(args)...) 
                                                                            );

            this->mtx_queue.lock();

            mTimers.push(timer);
            
            this->mtx_queue.unlock();

            //唤醒线程
            timer_wakeup_state = EM_TIMER_STATE::TIMER_STATE_ADD_TIMER;
            
            cv.notify_one();

            return timer;
        }

        template<typename F, typename ...TArgs>
        Timer::WeakPtr  addTimer(  std::chrono::nanoseconds timeout  ,  F&& callback , TArgs&& ...args )
        {
            auto timer = std::make_shared<Timer>(   
                                                                                std::chrono::steady_clock::now(),
                                                                                std::chrono::nanoseconds(timeout),
                                                                                std::chrono::seconds(0),
                                                                                ENUM_TIMER_TYPE::TIMER_TYPE_ONCE,
                                                                                std::bind(std::forward<F>(callback), std::forward<TArgs>(args)...) 
                                                                            );

            this->mtx_queue.lock();
            
            this->mTimers.push(timer);
            
            this->mtx_queue.unlock();

            //唤醒线程
            timer_wakeup_state = EM_TIMER_STATE::TIMER_STATE_ADD_TIMER;
            
            cv.notify_one();

            return timer;
        }

        void addTimer(const Timer::Ptr& timer)
        {
            std::unique_lock <std::mutex> lck(mtx);

            mTimers.push(timer);

            //唤醒线程
            timer_wakeup_state = EM_TIMER_STATE::TIMER_STATE_ADD_TIMER;

            cv.notify_one();
        }

        
        void Close()
        {
            std::unique_lock <std::mutex> lck(mtx);
            
            this->timer_run_ = false;
            
            //唤醒线程
            this->timer_wakeup_state = EM_TIMER_STATE::TIMER_STATE_ADD_TIMER;

            this->cv.notify_one();
        }

        /**
         * @brief timer_run 运行定时器
         * @return 
        */
        ENUM_WHILE_STATE timer_run()
        {
                std::unique_lock <std::mutex> lck(mtx);

                if (mTimers.empty())
                {
                    //当前没有定时器，等待唤醒
                    this->cv.wait_for( lck, std::chrono::seconds( timer_default_wait )  );
                    // std::cout << "wake up" << std::endl;
                }

                if (  ! this->timer_run_  )
                {
                    //std::cout << "end up" << std::endl;
                    return ENUM_WHILE_STATE::WHILE_STATE_BREAK;
                }

                if (  this->mTimers.empty()  )
                {
                    return ENUM_WHILE_STATE::WHILE_STATE_CONTINUE;
                }

                this->mtx_queue.lock();
                //取出优先级最高的定时器
                auto tmp = this->mTimers.top();
                this->mtx_queue.unlock();

                //条件变量同步
                auto timer_wait = tmp->getLeftTime();
                if ( timer_wait > std::chrono::nanoseconds::zero() ) //判断该定时器是否到期
                {
                    //还需要等待下一个到期时间
                    cv.wait_for(lck, timer_wait);
                    //std::cout << "[cv]cv.wait_for=" << timer_wait.count() << std::endl;

                    if (timer_wakeup_state == EM_TIMER_STATE::TIMER_STATE_ADD_TIMER)
                    {
                        return ENUM_WHILE_STATE::WHILE_STATE_CONTINUE; //继续运行定时器
                    }
                }

                if (! timer_run_ )
                {
                    return ENUM_WHILE_STATE::WHILE_STATE_BREAK;
                }

                this->mtx_queue.lock();
                
                this->mTimers.pop();

                //执行 定时器所需要的 任务函数
                (*tmp)( ) ;

                //如果是循环消息，则自动添加。（循环消息就是以该标志“TIMER_TYPE_LOOP”构建）
                if (ENUM_TIMER_TYPE::TIMER_TYPE_LOOP == tmp->get_timer_type() && tmp->mCallback != nullptr)
                {
                    //重新插入
                    this->mTimers.push(tmp);
                }

                this->mtx_queue.unlock();

                //更改定时器的状态为“已执行状态”
                timer_wakeup_state = EM_TIMER_STATE::TIMER_STATE_EXECUTE_TIMER;

                return ENUM_WHILE_STATE::WHILE_STATE_CONTINUE;
        }

        //线程执行函数
        void schedule()
        {
            this->timer_run_ = true;

            while ( this->timer_run_  )
            {
                auto while_result = this->timer_run();

                if ( while_result == ENUM_WHILE_STATE::WHILE_STATE_BREAK )
                {
                    break;
                }
                else
                {
                    continue;
                }
            }

            //std::cout << "[TimerMgr::schedule]Time manager is end.\n" << std::endl;
        }

        bool isEmpty()
        {
            std::unique_lock <std::mutex> lck(mtx);
            return  this->mTimers.empty();
        }

        // if timer empty, return zero
        std::chrono::nanoseconds nearLeftTime()
        {
            std::unique_lock <std::mutex> lck(mtx);
            if (this->mTimers.empty())
            {
                return std::chrono::nanoseconds::zero();
            }

            auto result = this->mTimers.top()->getLeftTime();

            if (result < std::chrono::nanoseconds::zero())
            {
                return std::chrono::nanoseconds::zero();
            }

            return result;
        }

        void clear()
        {
            std::unique_lock <std::mutex> lck(mtx);
            while ( ! this->mTimers.empty( ) )
            {
                mTimers.pop( );
            }
        }

    private:
        //内部类
        class CompareTimer
        {
        public:
            //按定时器执行时间排序
            bool operator() (  const Timer::Ptr&  left  ,  const Timer::Ptr&  right  ) const
            {
                const auto startDiff = left->getStartTime() - right->getStartTime();
                const auto lastDiff = left->getLastTime() - right->getLastTime();
                const auto diff = startDiff.count() + lastDiff.count();
                return diff > 0;
            }
        };

        //优先级队列（比较定时器优先级）
        std::priority_queue< Timer::Ptr ,  std::vector<Timer::Ptr> , CompareTimer >  mTimers;

        std::mutex mtx;

        std::recursive_mutex mtx_queue;

        std::condition_variable cv;

        bool timer_run_ = false; //定时器是否运行标志

        EM_TIMER_STATE timer_wakeup_state = EM_TIMER_STATE::TIMER_STATE_EXECUTE_TIMER ;
    };

}

#endif
