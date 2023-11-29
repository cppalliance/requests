//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_CONNECTION_POOL_HPP
#define BOOST_REQUESTS_IMPL_CONNECTION_POOL_HPP

#include <boost/requests/connection_pool.hpp>

namespace boost {
namespace requests {



template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code))
connection_pool::async_lookup(urls::url_view av, CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken, void (boost::system::error_code)>(
      &async_lookup_impl,
      completion_token,
      this, av);
}

template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, connection)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, connection))
connection_pool::async_borrow_connection(CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken, void (error_code, connection)>(
      &async_borrow_connection_impl,
      completion_token,
      this);
}

template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, connection)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, connection))
connection_pool::async_steal_connection(CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken, void (error_code, connection)>(
      &async_steal_connection_impl,
      completion_token,
      this);
}


}
}

#endif // BOOST_REQUESTS_IMPL_CONNECTION_POOL_HPP
