// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_FACADE_HPP
#define BOOST_REQUESTS_FACADE_HPP

#include <boost/requests/detail/variadic.hpp>
#include <boost/requests/traits.hpp>

namespace boost {
namespace requests {
namespace detail {
struct deduced_body_type_t {};
}

using empty = beast::http::empty_body::value_type;

/// Install all nice methods on the object
template<typename Derived>
struct facade
{
    template<typename ResponseBody = detail::deduced_body_type_t, typename ... Ops>
    auto get(core::string_view path, Ops && ... ops)
        -> beast::http::response<ResponseBody>
    {
        // Op can also be
        return request_impl_<ResponseBody>(beast::http::verb::get, path,
                                           empty{}, std::forward<Ops>(ops)...);
    }

    template< typename ... Ops>
    auto head(core::string_view path, Ops && ... ops)
        -> beast::http::response<beast::http::empty_body>
    {
        return request_impl_<beast::http::empty_body>(beast::http::verb::head, path, empty{}, 
                                                beast::http::empty_body{}, std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = detail::deduced_body_type_t,  typename RequestBody, typename ... Ops>
    auto post(core::string_view path, RequestBody && body, Ops && ... ops)
        -> beast::http::response<ResponseBody>
    {
        return request_impl_<ResponseBody>(beast::http::verb::post, path,
                                           std::forward<RequestBody>(body), std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = empty,
             typename RequestBody, typename ... Ops>
    auto put(core::string_view path, RequestBody && body, Ops && ... ops) -> beast::http::response<ResponseBody>
    {
        return request_impl_<ResponseBody>(beast::http::verb::put, path,
                                           std::forward<RequestBody>(body), std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = empty, typename ... Ops>
    auto delete_(core::string_view path, Ops && ... ops) -> beast::http::response<ResponseBody>
    {
        return request_impl_<ResponseBody>(beast::http::verb::delete_, path,
                                           empty{}, std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = detail::deduced_body_type_t, typename ... Ops>
    auto connect(core::string_view path, Ops && ... ops)
        -> beast::http::response<ResponseBody>
    {
        return request_impl_<ResponseBody>(beast::http::verb::connect, path,
                                           empty{}, std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = detail::deduced_body_type_t, typename ... Ops>
    auto options(core::string_view path, Ops && ... ops)
        -> beast::http::response<ResponseBody>
    {
        return request_impl_<ResponseBody>(beast::http::verb::options, path,
                                           empty{}, std::forward<Ops>(ops)...);
    }

    template<typename ... Ops>
    auto trace(core::string_view path, Ops && ... ops)
        -> beast::http::response<beast::http::empty_body>
    {
        return request_impl_<beast::http::empty_body>(beast::http::verb::trace, path,
                                                      empty{}, std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = detail::deduced_body_type_t,
             typename RequestBody, typename ... Ops>
    auto patch(core::string_view path, RequestBody && body, Ops && ... ops)
        -> beast::http::response<ResponseBody>
    {
        return request_impl_<ResponseBody>(beast::http::verb::patch, path,
                                           std::forward<RequestBody>(body), std::forward<Ops>(ops)...);
    }


    template<typename ResponseBody = detail::deduced_body_type_t, typename ... Ops>
    auto async_get(core::string_view path, Ops && ... ops)
     //   ->
    {
        // Op can also be
        return async_request_impl_<ResponseBody>(beast::http::verb::get, path, empty{}, std::forward<Ops>(ops)...);
    }


    template< typename ... Ops>
    auto async_head(core::string_view path, Ops && ... ops)
        -> beast::http::response<beast::http::empty_body>
    {
        return async_request_impl_<beast::http::empty_body>(beast::http::verb::head, path, empty{},
                                                      beast::http::empty_body{}, std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = detail::deduced_body_type_t,  typename RequestBody, typename ... Ops>
    auto async_post(core::string_view path, RequestBody && body, Ops && ... ops)
        -> beast::http::response<ResponseBody>
    {
        return async_request_impl_<ResponseBody>(beast::http::verb::post, path,
                                           std::forward<RequestBody>(body), std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = empty,
            typename RequestBody, typename ... Ops>
    auto async_put(core::string_view path, RequestBody && body, Ops && ... ops) -> beast::http::response<ResponseBody>
    {
        return async_request_impl_<ResponseBody>(beast::http::verb::put, path,
                                           std::forward<RequestBody>(body), std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = empty, typename ... Ops>
    auto async_delete_(core::string_view path, Ops && ... ops)
        -> beast::http::response<ResponseBody>
    {
        return async_request_impl_<ResponseBody>(beast::http::verb::delete_, path,
                                                 empty{}, std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = detail::deduced_body_type_t, typename ... Ops>
    auto async_connect(core::string_view path, Ops && ... ops)
        -> beast::http::response<ResponseBody>
    {
        return async_request_impl_<ResponseBody>(beast::http::verb::connect, path,
                                                 empty{}, std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = detail::deduced_body_type_t, typename ... Ops>
    auto async_options(core::string_view path, Ops && ... ops)
        -> beast::http::response<ResponseBody>
    {
        return async_request_impl_<ResponseBody>(beast::http::verb::options, path,
                                                 empty{}, std::forward<Ops>(ops)...);
    }

    template<typename ... Ops>
    auto async_trace(core::string_view path, Ops && ... ops)
        -> beast::http::response<beast::http::empty_body>
    {
        return async_request_impl_<beast::http::empty_body>(beast::http::verb::trace, path,
                                                            empty{}, std::forward<Ops>(ops)...);
    }

    template<typename ResponseBody = detail::deduced_body_type_t,
            typename RequestBody, typename ... Ops>
    auto async_patch(core::string_view path, RequestBody && body, Ops && ... ops)
        -> beast::http::response<ResponseBody>
    {
        return async_request_impl_<ResponseBody>(beast::http::verb::patch, path,
                                                 std::forward<RequestBody>(body), std::forward<Ops>(ops)...);
    }
  private:
    template<typename ResponseBody, typename RequestBody, typename ... Ops>
    auto request_impl_(core::string_view path, RequestBody && body, Ops && ... ops)
        -> beast::http::response<ResponseBody>;

};


}
}

#endif //BOOST_REQUESTS_FACADE_HPP
