//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_SESSION_HPP
#define BOOST_REQUESTS_IMPL_SESSION_HPP

#include <boost/requests/session.hpp>
#include <boost/url/grammar/string_token.hpp>

namespace boost {
namespace requests {

template< BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, std::shared_ptr<connection_pool>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, pool_ptr))
session::async_get_pool(urls::url_view url, CompletionToken && completion_token)
{
  return asio::async_initiate<
          CompletionToken, void (boost::system::error_code, std::shared_ptr<connection_pool>)>(
          &async_get_pool_impl, completion_token, this, url);
}

}
}
#endif // BOOST_REQUESTS_IMPL_SESSION_HPP
