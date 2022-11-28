//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_DETAIL_REDIRECT_VALUE_HPP
#define BOOST_REQUESTS_DETAIL_REDIRECT_VALUE_HPP

#include <boost/asio/associator.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/system/error_code.hpp>

namespace boost
{
namespace requests
{
namespace detail
{

template <typename CompletionToken, typename Value>
class redirect_value_t
{
public:
  template <typename CompletionToken_>
  redirect_value_t(CompletionToken_ && completion_token, Value& value)
      : token_(std::forward<CompletionToken_>(completion_token)), value_(value)
  {
  }

  //private:
  CompletionToken token_;
  Value& value_;
};

template <typename CompletionToken, typename Value>
inline redirect_value_t<typename std::decay<CompletionToken>::type, Value> redirect_value(
    BOOST_ASIO_MOVE_ARG(CompletionToken) completion_token, Value& value)
{
  return redirect_value_t<typename std::decay<CompletionToken>::type, Value>(
      BOOST_ASIO_MOVE_CAST(CompletionToken)(completion_token), value);
}


// Class to adapt a redirect_error_t as a completion handler.
template <typename Handler, typename Value>
class redirect_value_handler
{
public:
  typedef void result_type;
  
  template <typename CompletionToken>
  redirect_value_handler(redirect_value_t<CompletionToken, Value> e)
      : value_(e.value_),
        handler_(BOOST_ASIO_MOVE_CAST(CompletionToken)(e.token_))
  {
  }

  template <typename RedirectedHandler>
  redirect_value_handler(Value& value,
                         BOOST_ASIO_MOVE_ARG(RedirectedHandler) h)
      : value_(value),
        handler_(BOOST_ASIO_MOVE_CAST(RedirectedHandler)(h))
  {
  }

  template <typename... Args>
  void operator()(const boost::system::error_code& ec,
                  Value value)
  {
    value_ = std::move(value);
    BOOST_ASIO_MOVE_OR_LVALUE(Handler)(handler_)(ec);
  }

  Value& value_;
  Handler handler_;
};

}
}

namespace asio
{


template <typename CompletionToken, typename Value>
struct async_result<requests::detail::redirect_value_t<CompletionToken, Value>,
                    void(system::error_code, Value)>
{
  typedef typename async_result<CompletionToken, void(system::error_code)>::return_type return_type;

  template <typename Initiation>
  struct init_wrapper
  {
    template <typename Init>
    init_wrapper(Value& value, BOOST_ASIO_MOVE_ARG(Init) init)
        : value_(value),
          initiation_(BOOST_ASIO_MOVE_CAST(Init)(init))
    {
    }

    template <typename Handler, typename... Args>
    void operator()(
        BOOST_ASIO_MOVE_ARG(Handler) handler,
        BOOST_ASIO_MOVE_ARG(Args)... args)
    {
      BOOST_ASIO_MOVE_CAST(Initiation)(initiation_)(
          requests::detail::redirect_value_handler<
            typename decay<Handler>::type, Value>(
              value_, BOOST_ASIO_MOVE_CAST(Handler)(handler)),
          BOOST_ASIO_MOVE_CAST(Args)(args)...);
    }

    Value& value_;
    Initiation initiation_;
  };

  template <typename Initiation, typename RawCompletionToken, typename... Args>
  static return_type initiate(
      BOOST_ASIO_MOVE_ARG(Initiation) initiation,
      BOOST_ASIO_MOVE_ARG(RawCompletionToken) token,
      BOOST_ASIO_MOVE_ARG(Args)... args)
  {
    return async_initiate<CompletionToken, void(system::error_code)>(
        init_wrapper<typename decay<Initiation>::type>(
            token.value_, BOOST_ASIO_MOVE_CAST(Initiation)(initiation)),
        token.token_, BOOST_ASIO_MOVE_CAST(Args)(args)...);
  }

};

template <template <typename, typename> class Associator,
          typename Handler, typename Value, typename DefaultCandidate>
struct associator<Associator, requests::detail::redirect_value_handler<Handler, Value>, DefaultCandidate>
    : Associator<Handler, DefaultCandidate>
{
  static typename Associator<Handler, DefaultCandidate>::type
  get(const requests::detail::redirect_value_handler<Handler, Value>& h) BOOST_ASIO_NOEXCEPT
  {
    return Associator<Handler, DefaultCandidate>::get(h.handler_);
  }

  static BOOST_ASIO_AUTO_RETURN_TYPE_PREFIX2(
      typename Associator<Handler, DefaultCandidate>::type)
      get(const requests::detail::redirect_value_handler<Handler, Value>& h,
          const DefaultCandidate& c) BOOST_ASIO_NOEXCEPT
      BOOST_ASIO_AUTO_RETURN_TYPE_SUFFIX((
          Associator<Handler, DefaultCandidate>::get(h.handler_, c)))
  {
    return Associator<Handler, DefaultCandidate>::get(h.handler_, c);
  }
};

}
}

#endif // BOOST_REQUESTS_DETAIL_REDIRECT_VALUE_HPP
