//
//  JCDispatch.cpp
//  cppdispatch
//
//  Created by dawenhing on 17/03/2017.
//  Copyright © 2017 dawenhing. All rights reserved.
//

#include "JCDispatch.hpp"

#include <algorithm>
#include <condition_variable>
#include <ctime>
#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include <cmath>

namespace dispatch {
    struct queueImpl {
        const priority_t priority;
        std::queue<block_t> tasks;
        bool isRunning = false;
        queueImpl(priority_t priority): priority(priority){};
    };
    
    struct threadPool: public queueRunner {
        threadPool();
        static std::shared_ptr<threadPool> &sharedPool();
        virtual ~threadPool();
        
        virtual void addTaskWithPriority(const block_t &, priority_t) override;

        bool stop = false;
        
        typedef std::shared_ptr<queueImpl> queue_ptr_t;
        
        bool getFreeQueue(queue_ptr_t *) const;
        void startTaskInQueue(const queue_ptr_t &);
        void stopTaskInQueue(const queue_ptr_t &);
        
        
        std::mutex mainThreadMutex;
        std::queue<block_t> mainQueue;
        
        std::mutex mutex;
        std::map<priority_t, queue_ptr_t> queues;
        std::condition_variable condition;
        std::vector<std::thread> threads;
        
        // 可以限定最大并发线程数量，如果是1，则相当于是序列执行
        size_t maxCocurrent;
        // 返回当前并发线程的数量
        size_t cocurrentCount();
        
        block_t mainLoopNeedUpdate;
        void addWorkers();
    };
    
    threadPool::threadPool(): maxCocurrent(0) {}
    
    threadPool::~threadPool() {
        stop = true;
        condition.notify_all();
        for (auto &thread: threads) {
            thread.join();
        }
    }    
    bool threadPool::getFreeQueue(queue_ptr_t* outQueue) const {
        auto finded = std::find_if(queues.rbegin(),
                                   queues.rend(),
                                   [](const std::pair<priority_t, queue_ptr_t>& iterator) {
                                       return !iterator.second->isRunning;
                                   });
        
        bool isFinded = (finded != queues.rend());
        if (isFinded)
            *outQueue = finded->second;
        
        return  isFinded;
    }
    
    void threadPool::startTaskInQueue(const queue_ptr_t& queue) {
        queue->isRunning = true;
    }
    
    void threadPool::addTaskWithPriority(const block_t &task, priority_t priority) {
        std::unique_lock<std::mutex> lock(mutex);
        
        auto queue = queues[priority];
        if (!queue) {
            queue = std::make_shared<queueImpl>(priority);
            queues[priority] = queue;
        }
        
        queue->tasks.push(task);
        size_t requiredThreads = maxCocurrent;
        if (requiredThreads == 0) {
            requiredThreads = std::round(std::log(queues.size()) + 1);
            size_t maxThreads = std::max<size_t>(std::thread::hardware_concurrency(), 2);
            requiredThreads = std::min(maxThreads, requiredThreads);
        }
        for (size_t i=threads.size(); i<requiredThreads; i++) {
            addWorkers();
        }
        condition.notify_all();
    }
    
    size_t threadPool::cocurrentCount() {
        return (unsigned)threads.size();        
    }
    
    void threadPool::stopTaskInQueue(const queue_ptr_t &queue) {
        std::unique_lock<std::mutex> lock(mutex);
        
        queue->isRunning = false;
        if (queue->tasks.size() == 0) {
            queues.erase(queues.find(queue->priority));
        }
        condition.notify_all();
    }
    
    void threadPool::addWorkers() {
        threads.push_back(std::thread([=] {
            block_t task;
            threadPool::queue_ptr_t queue;
            
            while(true) {
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    
                    while(!stop && !getFreeQueue(&queue)) {
                        condition.wait(lock);
                    }
                    
                    if(stop) {
                        return;
                    }
                    
                    task = queue->tasks.front();
                    queue->tasks.pop();
                    
                    startTaskInQueue(queue);
                }
                task();
                stopTaskInQueue(queue);
            }
        }));
    }

    void globalQueue::async(block_t task) {
        threadPool::sharedPool()->addTaskWithPriority(task, queuePriority);
    };
    
    serailQueue::serailQueue() {
        std::shared_ptr<threadPool> tp = std::make_shared<threadPool>();
        tp->maxCocurrent = 1;
        queueRunner = tp;
    }
    
    void serailQueue::async(block_t task) {
        queueRunner->addTaskWithPriority(task, 0);
    }
    
    std::shared_ptr<threadPool> &threadPool::sharedPool() {
        static std::once_flag flag;
        static std::shared_ptr<threadPool> sharedPool;
        std::call_once(flag, [] {
            sharedPool = std::make_shared<threadPool>();
        });
        return sharedPool;
    }
    
    struct mainQueue : queue {
        virtual void async(block_t task) override;
        mainQueue() {};
    };
    
    void mainQueue::async(block_t task) {
        auto pool = threadPool::sharedPool();
        std::lock_guard<std::mutex> lock(pool->mainThreadMutex);
        pool->mainQueue.push(task);
        if (pool->mainLoopNeedUpdate != nullptr)
            pool->mainLoopNeedUpdate();
    }
    
    std::shared_ptr<queue> queue::mainQueue() {
        return std::make_shared<dispatch::mainQueue>();
    }
    
    void processMainLoop() {
        auto pool = threadPool::sharedPool();
        std::unique_lock<std::mutex> lock(pool->mainThreadMutex);
        while (!pool->mainQueue.empty()) {
            auto task = pool->mainQueue.front();
            pool->mainQueue.pop();
            task();
        }
    }
    
    void runMainLoop(block_t func) {
        auto mainQueue = queue::mainQueue();
        while (!threadPool::sharedPool()->stop) {
            mainQueue->async(func);
            processMainLoop();
        }
    }
    
    void exit() {
        threadPool::sharedPool()->stop = true;
    }
    
    void setMainLoopProcessCallback(block_t callback) {
        threadPool::sharedPool()->mainLoopNeedUpdate = callback;
    }
}
