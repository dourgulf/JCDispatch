//
//  JCDispatch.hpp
//  cppdispatch
//
//  Created by dawenhing on 17/03/2017.
//  Copyright © 2017 dawenhing. All rights reserved.
//

#ifndef JCDispatch_hpp
#define JCDispatch_hpp

#include <functional>
#include <memory>
#include <queue>
#include <string>

namespace dispatch
{
    typedef std::function<void ()> block_t;
    
    typedef long priority_t;
    namespace QUEUE_PRIORITY {
        priority_t const HIGH = 2;
        priority_t const DEFAULT = 0;
        priority_t const LOW = -2;
        priority_t const BACKGROUND = -255;
    };
    
    /**
     * 队列任务运行
     */
    struct queueRunner {
        virtual void addTaskWithPriority(const block_t &, priority_t) = 0;
    };
    
    /**
     * 异步队列
     */
    struct queue {
        static std::shared_ptr<queue> mainQueue();        
        virtual void async(block_t) = 0;
        std::string queueName;
        queue(const std::string &name=std::string()): queueName(name) {}
    };
    
    struct globalQueue: public queue {
        virtual void async(block_t) override;
        const priority_t queuePriority;
        globalQueue(priority_t priority = QUEUE_PRIORITY::DEFAULT) : queuePriority(priority) {};         
    };
    
    struct serailQueue: public queue {
        virtual void async(block_t) override;
        std::shared_ptr<queueRunner> runner;
        serailQueue();
    };
    
//    struct cocurrentQueue: public queue {
//        std::shared_ptr<queueRunner> runner;
//        std::string queueName;
//        cocurrentQueue(size_t cocurrentCount);
//    };
    
    /**
     * 结束主线程和所有异步线程
     */
    void exit();
    
    /**
     * @brief 在“主线程”执行此方法，驱动主线程循环，直到exit。
     * @param func 此函数在主线程每次执行队列中的“主线程任务”之前都会执行一次
     * 注意，这个函数是一个忙循环，只适合没有UI线程的单线程应用。
     */
    void runMainLoop(block_t func);
    
    /**
     * @brief 对于具有特定UI线程，处理“主线程任务”的方法发如下：
     * 1. 通过setMainLoopProcessCallback设置一个回调
     * 2. 在回调函数中，向UI主线程发送一个通知，比如iOS的performSelectorInMainThread之类的方法；
     *    （或者对Windows平台，发送一个自定义消息到窗口过程）
     * 3. 在UI主线程（Windows平台窗口过程的）中执行processMainLoop
     */
    void setMainLoopProcessCallback(block_t block);
    void processMainLoop();
}

#endif /* JCDispatch_hpp */
