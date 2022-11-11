//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_CONNECTION_POOL_HPP
#define BOOST_REQUESTS_IMPL_CONNECTION_POOL_HPP

#include <boost/requests/connection_pool.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/yield.hpp>

namespace boost {
namespace requests {

template<typename Stream>
void basic_connection_pool<Stream>::lookup(urls::authority_view sv, system::error_code & ec)
{
  constexpr auto protocol = detail::has_ssl_v<Stream> ? "https" : "http";

  const auto service = sv.has_port() ? sv.port() : protocol;
  using lock_type = asem::lock_guard<detail::basic_mutex<executor_type>>;

  lock_type lock = asem::lock(mutex_, ec);
  if (ec)
    return;
  resolver_type resolver{get_executor()};
  auto eps = resolver.resolve(sv.encoded_host_name(), service, ec);

  if (eps.empty())
    ec = asio::error::not_found;
  else
    host_ = eps->host_name();

  const auto r =
      boost::adaptors::transform(
              eps,
              [](const typename resolver_type::results_type::value_type  res)
              {
                return res.endpoint();
              });

  endpoints_.assign(r.begin(), r.end());
}

template<typename Stream>
struct basic_connection_pool<Stream>::async_lookup_op : asio::coroutine
{
  basic_connection_pool<Stream> * this_;
  const  urls::authority_view sv;
  constexpr static auto protocol = detail::has_ssl_v<Stream> ? "https" : "http";
  const core::string_view service = sv.has_port() ? sv.port() : protocol;
  std::shared_ptr<resolver_type> resolver;

  using mutex_type = detail::basic_mutex<executor_type>;
  using lock_type = asem::lock_guard<mutex_type>;

  template<typename Self>
  void operator()(Self && self, std::array<std::size_t, 2u> = {},
                  system::error_code ec1 = {}, lock_type = {},
                  system::error_code ec2 = {}, typename resolver_type::results_type eps = {})
  {
    if (!resolver)
      resolver = std::allocate_shared<resolver_type>(self.get_allocator(), this_->get_executor());


    if (ec1)
      return self.complete(ec1);

    if (ec2)
      return self.complete(ec2);

    reenter(this)
    {
      yield asio::experimental::make_parallel_group(
          asem::async_lock(this_->mutex_, asio::deferred),
          resolver->async_resolve(sv.encoded_host_name(), service, asio::deferred)
          ).async_wait(asio::experimental::wait_for_all(), std::move(self));
      if (eps.empty())
        return self.complete(asio::error::not_found);
      else
        this_->host_ = eps->host_name();

      const auto r =
          boost::adaptors::transform(
              eps,
              [](const typename resolver_type::results_type::value_type  res)
              {
                return res.endpoint();
              });

      this_->endpoints_.assign(r.begin(), r.end());
      self.complete(error_code{});
    }
  }
};

template<typename T>
auto foo(T &&);

template<typename Stream>
template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code))
basic_connection_pool<Stream>::async_lookup(urls::authority_view av,
                                            CompletionToken && completion_token)
{
  return asio::async_compose<CompletionToken, void(system::error_code)>(
      async_lookup_op{{}, this, av},
      completion_token,
      mutex_
  );
}

template<typename Stream>
auto basic_connection_pool<Stream>::get_connection(error_code & ec) -> std::shared_ptr<connection_type>
{

  auto lock = asem::lock(mutex_, ec);
  if (ec)
    return nullptr;

  // find an idle connection
  auto itr = std::find_if(conns_.begin(), conns_.end(),
                          [](const std::pair<const endpoint_type, std::shared_ptr<connection_type>> & conn)
                          {
                            return conn.second->working_requests() == 0u;
                          });

  if (itr != conns_.end())
    return itr->second;

  // check if we can make more connections. -> open another connection.
  // the race here is that we might open one too many
  if (conns_.size() <= limit_) // open another connection then -> we block the entire
  {
    if (endpoints_.empty())
    {
      ec = asio::error::not_found;
      return nullptr;
    }

    //sort the endpoints by connections that use it
    std::sort(endpoints_.begin(), endpoints_.end(),
              [this](const endpoint_type & a, const endpoint_type & b)
              {
                return conns_.count(a) < conns_.count(b);
              });
    const auto ep = endpoints_.front();
    std::shared_ptr<connection_type> nconn = this->template make_connection_<connection_type>(get_executor());
    nconn->set_host(host_);
    nconn->connect(ep, ec);
    if (ec)
      return nullptr;

    if (ec)
      return nullptr;

    conns_.emplace(ep, nconn);
    return nconn;

  }

  // find the one with the lowest usage
  itr = std::min_element(conns_.begin(), conns_.end(),
                         [](const std::pair<const endpoint_type, std::shared_ptr<connection_type>> & lhs,
                            const std::pair<const endpoint_type, std::shared_ptr<connection_type>> & rhs)
                         {
                           return (lhs.second->working_requests() + (lhs.second->is_open() ? 0 : 1))
                                < (rhs.second->working_requests() + (rhs.second->is_open() ? 0 : 1));
                         });
  if (itr == conns_.end())
  {
    ec = asio::error::not_found;
    return nullptr;
  }
  else
    return itr->second;
}

template<typename Stream>
struct basic_connection_pool<Stream>::async_get_connection_op : asio::coroutine
{
  basic_connection_pool<Stream> * this_;
  using lock_type = asem::lock_guard<detail::basic_mutex<executor_type>>;
  using conn_t = boost::unordered_multimap<endpoint_type,
                                           std::shared_ptr<connection_type>,
                                           detail::endpoint_hash<endpoint_type>>;
  typename conn_t::iterator itr;

  std::shared_ptr<connection_type> nconn = nullptr;
  endpoint_type ep;

  template<typename Self>
  void operator()(Self && self, system::error_code ec = {}, lock_type lock = {})
  {
    reenter (this)
    {
      yield asem::async_lock(this_->mutex_, std::move(self));
      if (ec)
        return self.complete(ec, {nullptr});

      // find an idle connection
      itr = std::find_if(this_->conns_.begin(), this_->conns_.end(),
                         [](const std::pair<const endpoint_type, std::shared_ptr<connection_type>> & conn)
                         {
                           return conn.second->working_requests() == 0u;
                         });

      if (itr != this_->conns_.end())
        return self.complete(error_code{}, itr->second);

      // check if we can make more connections. -> open another connection.
      // the race here is that we might open one too many
      if (this_->conns_.size() < this_->limit_) // open another connection then -> we block the entire
      {
        if (this_->endpoints_.empty())
          return self.complete(asio::error::not_found, nullptr);

        //sort the endpoints by connections that use it
        std::sort(this_->endpoints_.begin(), this_->endpoints_.end(),
                  [this](const endpoint_type & a, const endpoint_type & b)
                  {
                    return this_->conns_.count(a) < this_->conns_.count(b);
                  });
        ep = this_->endpoints_.front();
        nconn = this_->template make_connection_<connection_type>(this_->get_executor());
        nconn->set_host(this_->host_);
        // this isn't ideal, since we don't have connect going on in parallel.

        yield nconn->async_connect(ep, asio::append(std::move(self), std::move(lock))); // don't unlock here.
        if (ec)
          return self.complete(ec, {nullptr});

        this_->conns_.emplace(ep, nconn);
        lock = {};
        return self.complete(error_code{}, std::move(nconn));
      }
      // find the one with the lowest usage
      itr = std::min_element(this_->conns_.begin(), this_->conns_.end(),
                             [](const std::pair<const endpoint_type, std::shared_ptr<connection_type>> & lhs,
                                const std::pair<const endpoint_type, std::shared_ptr<connection_type>> & rhs)
                             {
                               return (lhs.second->working_requests() + (lhs.second->is_open() ? 0 : 1))
                                    < (rhs.second->working_requests() + (rhs.second->is_open() ? 0 : 1));
                             });
      lock = {};
      if (itr == this_->conns_.end())
        return self.complete(asio::error::not_found, nullptr);
      else
        return self.complete(error_code{}, itr->second);
    }

  }
};

template<typename Stream>
template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (system::error_code, std::shared_ptr<basic_connection<Stream>>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (system::error_code, std::shared_ptr<basic_connection<Stream>>))
basic_connection_pool<Stream>::async_get_connection(CompletionToken && completion_token)
{
  return asio::async_compose<CompletionToken, void(system::error_code, std::shared_ptr<connection_type>)>(
      async_get_connection_op{{}, this},
      completion_token,
      mutex_
  );
}

template<typename Stream>
template<typename RequestBody, typename Allocator>
struct basic_connection_pool<Stream>::async_request_op : asio::coroutine
{
  basic_connection_pool<Stream> *this_;
  beast::http::verb method;
  urls::pct_string_view path;
  RequestBody body;
  request_settings req;


  template<typename Self>
  void operator()(Self && self, error_code ec = {}, std::shared_ptr<connection_type> conn = nullptr)
  {

    reenter(this)
    {
      yield this_->async_get_connection(std::move(self));
      if (!ec && conn == nullptr)
        return self.complete(asio::error::not_found, response{req.get_allocator()});
      if (ec)
        return self.complete(ec, response{req.get_allocator()});
      yield conn->async_request(method, path, std::forward<RequestBody>(body), std::move(req), std::move(self));

    }
  }

  template<typename Self>
  void operator()(Self && self, error_code ec, response res)
  {
    self.complete(ec, std::move(res));
  }
};

template<typename Stream>
template<typename RequestBody, typename Allocator,
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code,
                                         response))
basic_connection_pool<Stream>::async_request(beast::http::verb method,
                                             urls::pct_string_view path,
                                             RequestBody && body,
                                             request_settings req,
                                             CompletionToken && completion_token)
{
  return asio::async_compose<CompletionToken, void(system::error_code, response)>(
      async_request_op<RequestBody, Allocator>{{}, this, method, path,
                                               std::forward<RequestBody>(body), std::move(req)},
      completion_token,
      mutex_
  );
}



template<typename Stream>
template<typename Allocator>
struct basic_connection_pool<Stream>::async_download_op : asio::coroutine
{
  basic_connection_pool<Stream> *this_;
  urls::pct_string_view path;
  filesystem::path download_path;
  request_settings req;

  template<typename Self>
  void operator()(Self && self, error_code ec = {}, std::shared_ptr<connection_type> conn = nullptr)
  {
    reenter(this)
    {
      yield this_->async_get_connection(std::move(self));
      if (!ec && conn == nullptr)
        ec =  asio::error::not_found;
      if (ec)
        return self.complete(ec, response{req.get_allocator()});

      yield conn->async_download(path, std::move(req), std::move(download_path), std::move(self));
    }
  }

  template<typename Self>
  void operator()(Self && self, error_code ec, response res)
  {
    self.complete(ec, std::move(res));
  }
};


template<typename Stream>
template<typename Allocator,
         BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                               response)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                   void (boost::system::error_code,
                                         response))
basic_connection_pool<Stream>::async_download(urls::pct_string_view path,
                                              request_settings req,
                                              filesystem::path download_path,
                                              CompletionToken && completion_token)

{
  return asio::async_compose<CompletionToken, void(system::error_code, response)>(
      async_download_op<Allocator>{{}, this, path, download_path, std::move(req)},
      completion_token,
      mutex_
  );
}

}
}

#include <boost/asio/unyield.hpp>

#endif // BOOST_REQUESTS_IMPL_CONNECTION_POOL_HPP
