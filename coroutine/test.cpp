#include <iostream>
#include "../include/coroutine_await.h"

extern void test_await();
extern void test_yield();

int main()
{
    std::cout << "test await!\n";

    test_await();

    std::cout << "test yield!\n";

    test_yield();

    return 0;
}
