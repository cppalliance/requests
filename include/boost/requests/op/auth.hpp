// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_AUTH_HPP
#define BOOST_REQUESTS_AUTH_HPP

#include <boost/beast/http/message.hpp>
#include <boost/beast/core/detail/base64.hpp>

namespace boost
{
namespace requests
{

// https://github.com/psf/requests/blob/b0e025ade7ed30ed53ab61f542779af7e024932e/requests/auth.py
struct basic_auth
{
    beast::string_view username;
    beast::string_view password;

    std::string lazy_buffer;

    template<typename Body, typename Fields>
    void prepare(beast::http::request<Body, Fields> & req)
    {
        if (lazy_buffer.empty())
        {
            auto sz = beast::detail::base64::encoded_size(username.size() + 1 + password.size());
            std::string res;
            res.resize(sizeof("Basic") + sz);
            constexpr beast::string_view prefix = "Basic ";
            auto itr = std::copy(prefix.begin(), prefix.end(), res.begin());
            const auto data = std::string(username) + ":" + std::string(password);
            beast::detail::base64::encode(&*itr, data.data(), data.size());
            lazy_buffer = std::move(res);
        }
        req.set(beast::http::field::authorization, lazy_buffer);
    }

    template<typename Body, typename Fields>
    void complete(beast::http::response<Body, Fields> & ) {}

};

struct bearer
{
    beast::string_view token;
    std::string lazy_buffer;

    template<typename Body, typename Fields>
    void prepare(beast::http::request<Body, Fields> & req)
    {
        if (lazy_buffer.empty())
            lazy_buffer = "Bearer " + std::string(token);
        req.set(beast::http::field::authorization, lazy_buffer);
    }

    template<typename Body, typename Fields>
    void complete(beast::http::response<Body, Fields> & ) {}

};


}
}

#endif //BOOST_REQUESTS_AUTH_HPP
