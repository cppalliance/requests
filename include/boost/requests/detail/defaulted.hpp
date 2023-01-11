//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_DEFAULTED_HPP
#define BOOST_REQUESTS_DEFAULTED_HPP

#include <boost/asio/associator.hpp>
#include <boost/asio/async_result.hpp>

namespace boost
{
namespace requests
{
namespace detail
{

template<typename DefaultToken, typename CompletionToken>
struct with_defaulted_token_t
{
  template<typename Token>
  with_defaulted_token_t(Token && token) : token_(std::forward<Token>(token)) {}

  CompletionToken token_;
};

template<typename DefaultToken, typename CompletionToken>
auto with_defaulted_token(CompletionToken && token)
  -> with_defaulted_token_t<DefaultToken, std::decay_t<CompletionToken>>
{
  return {std::forward<CompletionToken>(token)};
}

template<typename Token, typename Handler>
struct with_defaulted_token_handler
{
  template<typename Handler_>
  with_defaulted_token_handler(Handler_ && handler) : handler_(std::forward<Handler_>(handler)) {}

  template <typename Thingy>
  void operator()(system::error_code ec, Thingy thingy)
  {
    using def_t = typename Thingy::template defaulted<Token>;
    std::move(handler_)(ec, def_t(std::move(thingy)));
  }

  Handler handler_;
};

}
}

namespace asio
{

template <typename Token, typename CompletionToken, typename Thingy>
struct async_result<
    boost::requests::detail::with_defaulted_token_t<Token, CompletionToken>,
    void(system::error_code, Thingy)>
    : async_result<CompletionToken,
                   typename detail::append_signature<
                       void(system::error_code, typename Thingy::template defaulted<Token>)>::type>
{
  using signature = void(system::error_code, typename Thingy::template defaulted<Token>);

  template <typename Initiation>
  struct init_wrapper
  {
    init_wrapper(Initiation init)
        : initiation_(BOOST_ASIO_MOVE_CAST(Initiation)(init))
    {
    }

    template <typename Handler, typename... Args>
    void operator()(
        BOOST_ASIO_MOVE_ARG(Handler) handler,
        BOOST_ASIO_MOVE_ARG(Args)... args)
    {
      BOOST_ASIO_MOVE_CAST(Initiation)(initiation_)(
          boost::requests::detail::with_defaulted_token_handler<
              Token,
              typename decay<Handler>::type>(
              BOOST_ASIO_MOVE_CAST(Handler)(handler)),
              BOOST_ASIO_MOVE_CAST(Args)(args)...);
    }

    Initiation initiation_;
  };

  template <typename Initiation, typename RawCompletionToken, typename... Args>
  static BOOST_ASIO_INITFN_DEDUCED_RESULT_TYPE(CompletionToken, signature,
                                               (async_initiate<CompletionToken, signature>(
                                                   declval<init_wrapper<typename decay<Initiation>::type> >(),
                                                   declval<CompletionToken&>(),
                                                   declval<BOOST_ASIO_MOVE_ARG(Args)>()...)))
      initiate(
          BOOST_ASIO_MOVE_ARG(Initiation) initiation,
          BOOST_ASIO_MOVE_ARG(RawCompletionToken) token,
          BOOST_ASIO_MOVE_ARG(Args)... args)
  {
    return async_initiate<CompletionToken, signature>(
        init_wrapper<typename decay<Initiation>::type>(
            BOOST_ASIO_MOVE_CAST(Initiation)(initiation)),
        token.token_,
        BOOST_ASIO_MOVE_CAST(Args)(args)...);
  }
};


template <template <typename, typename> class Associator,
          typename Token, typename Handler, typename DefaultCandidate>
struct associator<Associator,
                  boost::requests::detail::with_defaulted_token_t<Token, Handler>, DefaultCandidate>
    : Associator<Handler, DefaultCandidate>
{
  static typename Associator<Handler, DefaultCandidate>::type
  get(const boost::requests::detail::with_defaulted_token_t<Token, Handler>& h) BOOST_ASIO_NOEXCEPT
  {
    return Associator<Handler, DefaultCandidate>::get(h.handler_);
  }

  static BOOST_ASIO_AUTO_RETURN_TYPE_PREFIX2(
      typename Associator<Handler, DefaultCandidate>::type)
      get(const boost::requests::detail::with_defaulted_token_t<Token, Handler>& h,
          const DefaultCandidate& c) BOOST_ASIO_NOEXCEPT
      BOOST_ASIO_AUTO_RETURN_TYPE_SUFFIX((
          Associator<Handler, DefaultCandidate>::get(h.handler_, c)))
  {
    return Associator<Handler, DefaultCandidate>::get(h.handler_, c);
  }
};


}
}

#endif // BOOST_REQUESTS_DEFAULTED_HPP
