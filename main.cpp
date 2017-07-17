//
//  JCDispatch.hpp
//  cppdispatch
//
//  Created by dawenhing on 17/03/2017.
//  Copyright © 2017 dawenhing. All rights reserved.
//

#include "JCDispatch.hpp"

#include <assert.h>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

int main(int argc, const char * argv[])
{
    auto main_thread_id = std::this_thread::get_id();
    
    for (unsigned task = 0; task < 6; ++task)
    for (unsigned priority = 0; priority < 6; ++priority)
    {
        dispatch::globalQueue(priority).async([=]
        {
            assert(std::this_thread::get_id() != main_thread_id);
            
            std::string task_string = std::to_string(task);
            
            std::string palceholder(1+priority*5, ' ');
            
            dispatch::globalQueue::mainQueue()->async([=]
            {
                assert(std::this_thread::get_id() == main_thread_id);
                
                std::cout << palceholder << task_string << std::endl;
            });
        });
    }

    dispatch::globalQueue(dispatch::QUEUE_PRIORITY::BACKGROUND).async([]
    {
        std::chrono::milliseconds timespan(1); // or whatever
        std::this_thread::sleep_for(timespan);
        std::cout << "exit" << std::endl;
        dispatch::exit();
    });

    dispatch::runMainLoop([]{
        std::cout << ".";
    });
    
    return 0;
}

