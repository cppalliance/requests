//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#include <boost/asio.hpp>

#include <boost/asio/experimental/append.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/requests/async_semaphore.hpp>

#include <random>
#include <iostream>
#include "doctest.h"

namespace boost::requests
{
namespace experimental
{
}   // namespace experimental
}   // namespace boost::requests

namespace asio = boost::asio;
using namespace boost::asio;
using namespace boost::asio::experimental;
using namespace std::literals;
using namespace experimental::awaitable_operators;
using namespace boost::requests;

awaitable< void >
co_sleep(std::chrono::milliseconds ms)
{
    auto t = use_awaitable.as_default_on(
        steady_timer(co_await this_coro::executor, ms));
    co_await t.async_wait();
}

awaitable< void >
timeout(std::chrono::milliseconds ms)
{
    auto t = steady_timer(co_await this_coro::executor, ms);
    co_await t.async_wait(as_tuple(use_awaitable));
}

awaitable< void >
bot(int n, async_semaphore &sem, std::chrono::milliseconds deadline)
{
    auto say = [ident = "bot " + std::to_string(n) + " : "](auto &&...args)
    {
        std::cout << ident;
        ((std::cout << args), ...);
    };

    if (say("approaching semaphore\n"); !sem.try_acquire())
    {
        say("waiting up to ", deadline.count(), "ms\n");
        auto then = std::chrono::steady_clock::now();
        if ((co_await(sem.async_acquire(as_tuple(use_awaitable)) ||
                      timeout(deadline)))
                .index() == 1)
        {
            say("got bored waiting after ", deadline.count(), "ms\n");
            co_return;
        }
        else
        {
            say("semaphore acquired after ",
                std::chrono::duration_cast< std::chrono::milliseconds >(
                    std::chrono::steady_clock::now() - then)
                    .count(),
                "ms\n");
        }
    }
    else
    {
        say("semaphore acquired immediately\n");
    }

    co_await co_sleep(500ms);
    say("work done\n");

    sem.release();
    say("passed semaphore\n");
}

TEST_SUITE_BEGIN("async_semaphore");

TEST_CASE("test_value")
{
    asio::io_context ioc;
    async_semaphore sem{ioc.get_executor(), 0};


    CHECK(sem.value() == 0);

    sem.release();
    CHECK(sem.value() == 1);
    sem.release();
    CHECK(sem.value() == 2);

    sem.try_acquire();
    CHECK(sem.value() == 1);

    sem.try_acquire();
    CHECK(sem.value() == 0);

    sem.async_acquire(asio::detached);
    CHECK(sem.value() == -1);
    sem.async_acquire(asio::detached);
    CHECK(sem.value() == -2);
}

TEST_CASE("main")
{
    int res = 0;

    auto ioc  = asio::io_context(BOOST_ASIO_CONCURRENCY_HINT_UNSAFE);
    auto sem  = async_semaphore(ioc.get_executor(), 10);
    auto rng  = std::random_device();
    auto ss   = std::seed_seq { rng(), rng(), rng(), rng(), rng() };
    auto eng  = std::default_random_engine(ss);
    auto dist = std::uniform_int_distribution< unsigned int >(1000, 10000);

    auto random_time = [&eng, &dist]
    { return std::chrono::milliseconds(dist(eng)); };
    for (int i = 0; i < 100; i += 2)
        co_spawn(ioc, bot(i, sem, random_time()), detached);
    ioc.run();
}

TEST_CASE("test_sync")
{
    asio::io_context  ioc;
    int errors = 0;
    async_semaphore se2{ioc.get_executor(), 3}; //allow at most three in parallel

    std::vector<int> order; // isn't 100% defined!

    static int concurrent = 0;

    auto op =
            [&](int id, auto && token)
            {
                return asio::co_spawn(ioc, [&, id]() -> asio::awaitable<void>
                {
                    CHECK(concurrent <= 3);
                    concurrent ++;
                    printf("Entered %d\n", id);

                    asio::steady_timer tim{co_await asio::this_coro::executor, std::chrono::milliseconds{10}};
                    co_await tim.async_wait(asio::use_awaitable);
                    printf("Exited %d\n", id);
                    concurrent --;
                }, std::move(token));
            };

    synchronized(se2, std::bind(op, 0, std::placeholders::_1), asio::detached);
    synchronized(se2, std::bind(op, 2, std::placeholders::_1), asio::detached);
    synchronized(se2, std::bind(op, 4, std::placeholders::_1), asio::detached);
    synchronized(se2, std::bind(op, 6, std::placeholders::_1), asio::detached);
    synchronized(se2, std::bind(op, 8, std::placeholders::_1), asio::detached);
    synchronized(se2, std::bind(op, 10, std::placeholders::_1), asio::detached);
    synchronized(se2, std::bind(op, 12, std::placeholders::_1), asio::detached);
    synchronized(se2, std::bind(op, 14, std::placeholders::_1), asio::detached);

    ioc.run();

}

TEST_SUITE_END();