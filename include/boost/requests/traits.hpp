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
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/span_body.hpp>
#include <boost/beast/http/vector_body.hpp>

namespace boost
{
namespace requests
{

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



template<typename T>
struct request_body_traits;

template<typename T>
struct response_body_traits;

template<typename Char, typename Traits, typename Alloc>
struct request_body_traits<std::basic_string<Char, Traits, Alloc>>
{
    template<typename Alloc_>
    void set_content_type(beast::http::basic_fields<Alloc_> & fields,
                          system::error_code & ec)
    {
        auto itr = fields.find(beast::http::field::content_type);
        if (itr == fields.end())
            fields.set(beast::http::field::content_type, "text/plain");
    }

    using body_type = beast::http::basic_string_body<Char, Traits, Alloc>;

    body_type make_body(const std::basic_string<Char, Traits, Alloc> & str)
    {
        return body_type(str);
    }

    body_type make_body(std::basic_string<Char, Traits, Alloc> && str)
    {
        return body_type(std::move(str));
    }
};

template<typename Char, typename Traits, typename Alloc>
struct response_body_traits<std::basic_string<Char, Traits, Alloc>>
{
    template<typename Alloc_>
    void set_accepted_content_type(beast::http::basic_fields<Alloc_> & fields,
                                   system::error_code & ec)
    {
        fields.set(beast::http::field::accept, "text/*, application/*");
    }

    using body_type = beast::http::basic_string_body<Char, Traits, Alloc>;
    using result_type = std::basic_string<Char, Traits, Alloc>;

    body_type make_body(Alloc alloc = Alloc{})
    {
        return body_type(std::move(alloc));
    }

    body_type make_body(const result_type & res)
    {
        return body_type(res.get_allocator());
    }

    template<typename Fields>
    result_type make_result(beast::http::response<body_type, Fields> & res)
    {
        return std::move(res.body());
    }
};

// this if for capture by ref
template<typename Char, typename Traits, typename Alloc>
struct response_body_traits<std::basic_string<Char, Traits, Alloc>&>
        : response_body_traits<std::basic_string<Char, Traits, Alloc>>
{
    using result_type = void;
};

template<>
struct request_body_traits<beast::http::empty_body::value_type>
{
    template<typename Alloc_>
    void set_content_type(beast::http::basic_fields<Alloc_> & fields, system::error_code & ec)
    {
    }

    using body_type = beast::http::empty_body;
    body_type make_body(const beast::http::empty_body::value_type &)
    {
        return body_type();
    }
};

}
}

#endif //BOOST_REQUESTS_TRAITS_HPP
