//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_DETAIL_SSL_HPP
#define BOOST_REQUESTS_DETAIL_SSL_HPP

#include <boost/asio/ssl/stream_base.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/requests/detail/config.hpp>

extern "C" { typedef struct ssl_st SSL; }

namespace boost {
namespace requests {
namespace detail {

template<typename Stream, bool B>
auto get_ssl_layer_impl(Stream & str,
                        std::true_type /* is ssl */,
                        std::bool_constant<B>) -> Stream *
{
  return &str;
}

template<typename Stream>
auto get_ssl_layer_impl(Stream & str,
                        std::false_type /* is ssl */,
                        std::true_type /* lowest layer */) -> std::nullptr_t
{
  return nullptr;
}

template<typename Stream>
auto get_ssl_layer_impl(Stream & str, std::false_type /* is ssl */, std::false_type /* lowest layer */)
    -> decltype(get_ssl_layer_impl(str.next_layer(),
                                   std::is_base_of<boost::asio::ssl::stream_base,
                                                   typename Stream::next_layer_type>{},
                                   std::is_same<typename Stream::next_layer_type,
                                                boost::beast::lowest_layer_type<Stream>>{})
                                   )
{
  return get_ssl_layer_impl(str.next_layer(),
                            std::is_base_of<boost::asio::ssl::stream_base,
                                            typename Stream::next_layer_type>{},
                            std::is_same<typename Stream::next_layer_type,
                                         boost::beast::lowest_layer_type<Stream>>{});
}

template<typename Stream>
auto get_ssl_layer(Stream & str)
    -> decltype(get_ssl_layer_impl(str,
                                   std::is_base_of<boost::asio::ssl::stream_base, Stream>{},
                                   std::is_same<Stream, boost::beast::lowest_layer_type<Stream>>{}))
{
  return get_ssl_layer_impl(str,
                            std::is_base_of<boost::asio::ssl::stream_base, Stream>{},
                            std::is_same<Stream, boost::beast::lowest_layer_type<Stream>>{});
}

template<typename Stream>
using has_ssl = std::bool_constant<!std::is_null_pointer_v<decltype(get_ssl_layer(std::declval<std::decay_t<Stream>&>()))>>;

template<typename Stream>
constexpr bool has_ssl_v = has_ssl<Stream>::value;

BOOST_REQUESTS_DECL bool do_verify_host(SSL * ssl, const std::string & host);

template <typename Stream>
inline bool verify_host_impl(Stream *stream, const std::string & host)
{
  return do_verify_host(stream->native_handle(), host);
}

inline bool verify_host_impl(std::nullptr_t, const std::string &) {return true;}


template<typename Stream>
bool verify_host(Stream & str, const std::string &host)
{
  return verify_host_impl(get_ssl_layer(str), host);
}

}
}
}

#if defined(BOOST_REQUESTS_HEADER_ONLY)
#include <boost/requests/detail/impl/ssl.ipp>
#endif
#endif // BOOST_REQUESTS_DETAIL_SSL_HPP
