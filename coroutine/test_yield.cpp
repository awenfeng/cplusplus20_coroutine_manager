#include <iostream>
#include "../include/coroutine_yield.h"

#if defined _WIN64
#include <Windows.h>
inline uint64_t get_tick_count()
{
    return GetTickCount64();
}

inline void sleep(int ms)
{
    Sleep(ms);
}
#endif

#if defined __linux__
#include <sys/time.h>
#include <thread.h>
inline uint64_t get_tick_count()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint64_t)ts.tv_sec * (uint64_t)1000 + (uint64_t)ts.tv_nsec / (uint64_t)1000000;
}
#endif

using namespace coroutine_yield;

coroutine_manager* coroutine_manager::instance = nullptr;

coroutine_t coroutine1_yield_for_seconds(float seconds)
{
    std::cout << "coroutine1_yield_seconds begin ..." << std::endl;

    wait_for_seconds _wait(seconds);

    co_yield &_wait;

    std::cout << "coroutine1_yield_seconds end, " << seconds << std::endl;
}

coroutine_t coroutine2_yield_for_frame()
{
    std::cout << "coroutine2_yield_for_frame begin ..." << std::endl;

    wait_for_frame _wait;

    co_yield &_wait;

    std::cout << "coroutine2_yield_for_frame end, " << std::endl;
}

coroutine_t coroutine3_yield_for_event(int event_id, float seconds)
{
    std::cout << "coroutine3_yield_for_event begin ..., event_id:" << event_id << std::endl;

    wait_for_event _wait(event_id, seconds);

    co_yield &_wait;

    if (nullptr == _wait.result)
    {
        std::cout << "coroutine3_yield_for_event end, timeout" << " event_id:" << event_id << std::endl;
    }
    else
    {
        std::cout << "coroutine3_yield_for_event end, " << " event_id:" << event_id << std::endl;
    }
}

coroutine_t coroutine4_yield_for_coroutine_group(uint64_t* coroutines, size_t count)
{
    std::cout << "coroutine4_yield_for_coroutine_group begin ..., " << std::endl;

    wait_for_coroutine_group wait(coroutines, count);
    co_yield &wait;

    std::cout << "coroutine4_yield_for_coroutine_group end, " << std::endl;
}

void test_yield()
{
    coroutine_manager coroutine_manager(get_tick_count());
    coroutine_manager::instance = &coroutine_manager;

    std::vector<uint64_t> coroutines;

    coroutines.emplace_back(coroutine_manager::instance->create_coroutine(coroutine1_yield_for_seconds(1.0f)));
    coroutines.emplace_back(coroutine_manager::instance->create_coroutine(coroutine2_yield_for_frame()));
    coroutines.emplace_back(coroutine_manager::instance->create_coroutine(coroutine3_yield_for_event(1, 5.0f)));

    uint64_t wait_id = coroutine_manager::instance->create_coroutine(coroutine4_yield_for_coroutine_group(coroutines.data(), coroutines.size()));

    float result = 10.0f;
    coroutine_manager::instance->trigger_event(1, &result);

    while (true)
    {
        coroutine_manager::instance->update(get_tick_count());
        if (!coroutine_manager::instance->exists_coroutine(wait_id))
            break;

        sleep(10);
    }
}
