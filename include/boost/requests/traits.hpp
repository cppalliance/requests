// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_TRAITS_HPP
#define BOOST_REQUESTS_TRAITS_HPP

#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/ssl/stream.hpp>

#include <boost/beast/http/basic_dynamic_body.hpp>
#include <boost/beast/http/basic_file_body.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/span_body.hpp>
#include <boost/beast/http/vector_body.hpp>

namespace boost
{
namespace requests
{


template<typename Socket>
struct is_stream_socket : std::false_type
{
};

template<typename Protocol, typename Executor>
struct is_stream_socket<asio::basic_stream_socket < Protocol, Executor>> : std::true_type
{
};


template<typename Stream>
using is_ssl_stream = std::is_base_of <asio::ssl::stream_base, std::decay_t<Stream>>;

template<typename T, typename U = void>
struct deduced_body;

template<typename Char, typename Traits, typename Allocator>
struct deduced_body<std::basic_string<Char, Traits, Allocator>, void>
{
    using type = beast::http::basic_string_body<Char, Traits, Allocator>;
};


template<typename T>
struct deduced_body<T, std::enable_if_t<std::is_convertible<T, beast::span<char>>::value>>
{
    using type = beast::http::span_body<T>;
};


template<std::size_t Size>
struct deduced_body<char[Size], void>
{
    using type = beast::http::span_body<char>;
};

template<typename File>
struct deduced_body<File, std::enable_if_t<beast::is_file<File>::value>>
{
    using type = beast::http::basic_file_body<File>;
};

template<typename T>
struct deduced_body<T, typename asio::enable_if<asio::is_dynamic_buffer<T>::value>::type>
{
    using type = beast::http::basic_dynamic_body<T>;
};


template<>
struct deduced_body<typename beast::http::empty_body::value_type, void>
{
    using type = beast::http::empty_body;
};

template<typename T>
struct deduced_body<beast::span<T>, void>
{
    using type = beast::http::span_body<T>;
};


template<typename T, typename Allocator>
struct deduced_body<std::vector<T, Allocator>, void>
{
    using type = beast::http::vector_body<T, Allocator>;
};



template<typename T>
using deduced_body_t = typename deduced_body<T>::type;

}
}

#endif //BOOST_REQUESTS_TRAITS_HPP
