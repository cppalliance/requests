//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_TE_HANDLER_HPP
#define BOOST_REQUESTS_TE_HANDLER_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/cancellation_signal.hpp>

namespace boost {
namespace requests {


namespace detail
{

template<typename Signature,
         typename Executor = asio::any_io_executor,
         typename Allocator = std::allocator<void>,
         typename CancellationSlot = asio::cancellation_slot>
struct poly_handler_base;

template<typename ... Args,
          typename Executor,
          typename Allocator,
          typename CancellationSlot>
struct poly_handler_base<void(Args...), Executor, Allocator, CancellationSlot>
{

  using executor_type = Executor;
  executor_type get_executor() const
  {
    return executor;
  }

  using allocator_type = Allocator;
  allocator_type get_allocator() const
  {
    return allocator;
  }

  using cancellation_slot_type = asio::cancellation_slot;
  cancellation_slot_type get_cancellation_slot() const
  {
    return cancellation_slot;
  }
  executor_type executor;
  allocator_type allocator;
  cancellation_slot_type cancellation_slot;

  poly_handler_base(Executor executor,
                    Allocator allocator,
                    CancellationSlot cancellation_slot
                    ) : executor(std::move(executor)), allocator(std::move(allocator)), cancellation_slot(std::move(cancellation_slot))
  {
  }

  virtual void operator()(Args  && ... args) && = 0;

  virtual ~poly_handler_base() = default;
};

template<typename CompletionHandler,
          typename Signature,
          typename Executor = asio::any_io_executor,
          typename Allocator = std::allocator<void>,
          typename CancellationSlot = asio::cancellation_slot>
struct poly_handler_impl;


template<typename CompletionHandler,
          typename ... Args,
          typename Executor,
          typename Allocator,
          typename CancellationSlot>
struct poly_handler_impl<CompletionHandler, void(Args...), Executor, Allocator, CancellationSlot> final
    : poly_handler_base<void(Args...), Executor, Allocator, CancellationSlot>
{

  poly_handler_impl(Executor executor,
                    Allocator allocator,
                    CancellationSlot cancellation_slot,
                    CompletionHandler completion_handler) :
                                                            poly_handler_base<void(Args...), Executor, Allocator, CancellationSlot>(
                                                                std::move(executor), std::move(allocator), std::move(cancellation_slot)),
                                                            handler(std::move(completion_handler))
  {
  }

  CompletionHandler handler;

  void operator()(Args  && ... args) && override
  {
    std::move(handler)(std::forward<Args>(args)...);
  }
};

template<typename Signature,
          typename Executor = asio::any_io_executor,
          typename Allocator = std::allocator<void>,
          typename CancellationSlot = asio::cancellation_slot>
struct poly_handler;


template<typename ... Args,
          typename Executor,
          typename Allocator,
          typename CancellationSlot>
struct poly_handler<void(Args...), Executor, Allocator, CancellationSlot> final
{
  using executor_type = Executor;
  executor_type get_executor() const
  {
    assert(ptr_);
    return ptr_->get_executor();
  }

  using allocator_type = Allocator;
  allocator_type get_allocator() const
  {
    assert(ptr_);
    return ptr_->get_allocator();
  }

  using cancellation_slot_type = asio::cancellation_slot;
  cancellation_slot_type get_cancellation_slot() const
  {
    assert(ptr_);
    return ptr_->get_cancellation_slot();
  }

  template<typename CompletionToken>
  poly_handler(CompletionToken && token,
               Executor executor,
               Allocator allocator,
               CancellationSlot cancellation_slot)
      : ptr_(std::make_unique<
             poly_handler_impl<std::decay_t<CompletionToken>, void(Args...), Executor, Allocator, CancellationSlot>>(
            std::move(executor), std::move(allocator),
            std::move(cancellation_slot), std::forward<CompletionToken>(token)))
  {

  }

  void operator()(Args  ... args)
  {
    assert(ptr_);
    std::move(*std::exchange(ptr_, nullptr))(std::move(args)...);
  };

private:
  std::unique_ptr<poly_handler_base<void(Args...), Executor, Allocator, CancellationSlot>> ptr_;
};



}



template<typename Signature,
          typename Executor = asio::any_io_executor,
          typename Allocator = std::allocator<void>,
          typename CancellationSlot = asio::cancellation_slot>
struct basic_async_op;

template<typename ... Args,
          typename Executor,
          typename Allocator,
          typename CancellationSlot>
struct basic_async_op<void(Args...), Executor, Allocator, CancellationSlot>
{

  using executor_type = Executor;
  executor_type get_executor() const
  {
    assert(impl_);
    return impl_->executor;
  }

  using allocator_type = Allocator;
  allocator_type get_allocator() const
  {
    assert(impl_);
    return impl_->allocator;
  }

  using cancellation_slot_type = asio::cancellation_slot;
  cancellation_slot_type get_cancellation_slot() const
  {
    assert(impl_);
    return impl_->cancellation_slot;
  }

  template <
      BOOST_ASIO_COMPLETION_TOKEN_FOR(void (Args...))
          CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                               void (Args...))
  operator()(BOOST_ASIO_MOVE_ARG(CompletionToken) token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
  {
    return asio::async_initiate<CompletionToken, void(Args...)>(initiate{impl_.get()}, token);
  }


  struct base
  {
    Executor executor;
    Allocator allocator;
    CancellationSlot cancellation_slot;
    base(Executor executor,
         Allocator allocator,
         CancellationSlot cancellation_slot
         ) : executor(std::move(executor)), allocator(std::move(allocator)), cancellation_slot(std::move(cancellation_slot))
    {
    }

    virtual ~base() = default;
    virtual void operator()(detail::poly_handler<void(Args...), Executor, Allocator, CancellationSlot>) = 0;
  };

  template<typename Initiation, typename ... Ts>
  struct impl final : base
  {
    Initiation initiation;
    std::tuple<Ts...> args;

    impl(Executor executor,
         Allocator allocator,
         CancellationSlot cancellation_slot,
         Initiation init, Ts && ... args) :
                                          base(std::move(executor), std::move(allocator), std::move(cancellation_slot)),
                                          initiation(std::move(init)), args(std::forward<Ts>(args)...) {}

    template<std::size_t ... Idx>
    void invoke(detail::poly_handler<void(Args...), Executor, Allocator, CancellationSlot> ph,
                std::index_sequence<Idx...>)
    {
      std::move(initiation)(std::move(ph), std::get<Idx>(args)...);
    }

    void operator()(detail::poly_handler<void(Args...), Executor, Allocator, CancellationSlot> ph) override
    {
      this->template invoke(std::move(ph), std::make_index_sequence<sizeof...(Ts)>{});
    }

  };

  basic_async_op(std::unique_ptr<base> impl) : impl_(std::move(impl)) {}

  struct initiate
  {
    base * ptr;

    template <typename Handler>
    void operator()(Handler handler)
    {
      detail::poly_handler<void(Args...), Executor, Allocator, CancellationSlot> h{
          std::move(handler),
          asio::get_associated_executor(handler, ptr->executor),
          asio::get_associated_allocator(handler, ptr->allocator),
          asio::get_associated_cancellation_slot(handler, ptr->cancellation_slot)};

      (*ptr)(std::move(h));
    }
  };

private:
  std::unique_ptr<base> impl_;
};

}
}

#endif // BOOST_REQUESTS_TE_HANDLER_HPP
