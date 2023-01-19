//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/detail/executor_with_default.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/deferred.hpp>


#include "../doctest.h"
#include "../string_maker.hpp"

using namespace boost;

TEST_CASE("executor_with_default")
{
  using boost::requests::detail::strip_executor_with_default_t;
  CHECK(typeid(asio::any_io_executor)               == typeid(strip_executor_with_default_t<asio::any_io_executor>));
  CHECK(typeid(asio::io_context::executor_type)     == typeid(strip_executor_with_default_t<asio::io_context::executor_type>));
  CHECK(typeid(asio::strand<asio::any_io_executor>) == typeid(strip_executor_with_default_t<asio::strand<asio::any_io_executor>>));

  CHECK(typeid(asio::any_io_executor)               == typeid(strip_executor_with_default_t<asio::deferred_t::executor_with_default<asio::any_io_executor>>));
  CHECK(typeid(asio::io_context::executor_type)     == typeid(strip_executor_with_default_t<asio::deferred_t::executor_with_default<asio::io_context::executor_type>>));
  CHECK(typeid(asio::strand<asio::any_io_executor>) == typeid(strip_executor_with_default_t<asio::deferred_t::executor_with_default<asio::strand<asio::any_io_executor>>>));
}