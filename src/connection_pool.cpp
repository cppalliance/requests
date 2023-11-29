//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/connection_pool.hpp>

namespace boost {
namespace requests {

void connection_pool::lookup(urls::url_view sv, system::error_code & ec)
{
  core::string_view scheme = "https";
  if (sv.has_scheme())
    scheme = sv.scheme();

  if (scheme == "unix")
    // all good, no lookup needed
  {
    std::lock_guard<std::mutex> lock{mtx_};
    if (ec)
      return;

    use_ssl_ = false;
    host_ = "localhost";
    endpoints_ = {asio::local::stream_protocol::endpoint(
                        std::string(
                          sv.encoded_target().data(),
                          sv.encoded_target().size()
                        ))};
  }
  else if (scheme == "http" || scheme == "https")
  {
    asio::ip::tcp::resolver resolver{get_executor()};
    const auto service = sv.has_port() ? sv.port() : scheme;
    auto eps = resolver.resolve(
            std::string(sv.encoded_host_address().data(), sv.encoded_host_address().size()),
            std::string(service.data(), service.size()), ec);

    if (!ec && eps.empty())
      BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_found)
    if (ec)
      return;

    std::lock_guard<std::mutex> lock{mtx_};
    if (ec)
      return;
    use_ssl_ = scheme == "https";
    host_ = eps->host_name();

    if (sv.has_port() &&
        (
           (sv.scheme_id() == urls::scheme::http && sv.port() != "80")
        || (sv.scheme_id() == urls::scheme::https && sv.port() != "443")
        ))
      (host_ += ":") += sv.port();

    const auto r =
        boost::adaptors::transform(
            eps,
            [](const typename asio::ip::tcp::resolver::results_type::value_type  res)
            {
              return res.endpoint();
            });

    endpoints_.assign(r.begin(), r.end());
  }
  else
  {
    BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::invalid_argument);
    return ;
  }

  // clean up!
  for (auto itr = conns_.begin(); itr != conns_.end(); )
  {
    auto it = std::find(endpoints_.begin(), endpoints_.end(), itr->first);
    if (it != endpoints_.end())
      itr ++;
    else
      itr = conns_.erase(itr);
  }
}


struct connection_pool::async_lookup_op : asio::coroutine
{
  constexpr static const char * op_name = "connection_pool::async_lookup_op";

  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  connection_pool * this_;
  const urls::url_view sv;

  core::string_view scheme = "https";

  std::unique_lock<std::mutex> lock{this_->mtx_, std::defer_lock};

  std::shared_ptr<asio::ip::tcp::resolver> resolver_;

  async_lookup_op(connection_pool * this_, urls::url_view sv)
      : this_(this_), sv(sv) {}

  template<typename Self>
  void operator()(Self &&  self, system::error_code ec = {}, typename asio::ip::tcp::resolver::results_type eps = {});
};


template<typename Self>
void connection_pool::async_lookup_op::operator()(Self && self,
                                                  system::error_code ec,
                                                  typename asio::ip::tcp::resolver::results_type eps)
{
  BOOST_ASIO_CORO_REENTER(this)
  {
    if (sv.has_scheme())
      scheme = sv.scheme();

    if (scheme == "unix")
      // all good, no lookup needed
    {
      lock.lock();
      this_->use_ssl_ = false;
      this_->host_ = "localhost";
      this_->endpoints_ = {asio::local::stream_protocol::endpoint(
          std::string(sv.encoded_target().data(), sv.encoded_target().size()))};
    }
    else if (scheme == "http" || scheme == "https")
    {

      BOOST_REQUESTS_YIELD
      {
        auto service = sv.has_port() ? sv.port() : scheme;
        resolver_ = std::allocate_shared<asio::ip::tcp::resolver>(
                            asio::get_associated_allocator(self),
                            get_executor());
        resolver_->async_resolve(
          std::string(sv.encoded_host_address().data(), sv.encoded_host_address().size()),
          std::string(service.data(), service.size()), std::move(self));
      }
      resolver_.reset();

      if (!ec && eps.empty())
        BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_found)
      if (ec)
        return;

      lock.lock();
      this_->use_ssl_ = scheme == "https";
      this_->host_ = eps->host_name();

      if (sv.has_port() &&
          (
              (sv.scheme_id() == urls::scheme::http && sv.port() != "80")
              || (sv.scheme_id() == urls::scheme::https && sv.port() != "443")
                  ))
        (this_->host_ += ":") += sv.port();
      const auto r =
          boost::adaptors::transform(
              eps,
              [](const typename asio::ip::tcp::resolver::results_type::value_type  res)
              {
                return res.endpoint();
              });

      this_->endpoints_.assign(r.begin(), r.end());
    }
    else
    {
      BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::invalid_argument);
      lock.unlock();
      return ;
    }

    // clean up!
    for (auto itr = this_->conns_.begin(); itr != this_->conns_.end(); )
    {
      auto it = std::find(this_->endpoints_.begin(), this_->endpoints_.end(), itr->first);
      if (it != this_->endpoints_.end())
        itr ++;
      else
        itr = this_->conns_.erase(itr);
    }
    lock.unlock();
  }
  if (is_complete())
    self.complete(ec);
}


auto connection_pool::borrow_connection(error_code & ec) -> connection
{
  std::unique_lock<std::mutex> lock{mtx_};
  do
  {

    if (!free_conns_.empty())
    {
      auto pp = std::move(free_conns_.front());
      free_conns_.erase(free_conns_.begin());
      return connection(pp);
    }
    // check if we can make more connections. -> open another connection.
    // the race here is that we might open one too many
    else if (conns_.size() < limit_) // open another connection then -> we block the entire
    {
      if (endpoints_.empty())
      {
        BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_found);
        return connection();
      }

      // sort the endpoints by connections that use it
      std::sort(endpoints_.begin(), endpoints_.end(),
                [this](const endpoint_type &a, const endpoint_type &b) { return conns_.count(a) < conns_.count(b); });
      const auto ep = endpoints_.front();

      std::shared_ptr<detail::connection_impl> nconn =
          conns_.emplace(ep, std::make_shared<detail::connection_impl>(get_executor(), context_))->second;
      // no one else will grab it from there, bc it's not in free_conns_
      lock.unlock();
      nconn->use_ssl(use_ssl_);
      nconn->set_host(host_);
      nconn->connect(ep, ec);
      if (ec)
        return connection();
      else
        return connection(std::move(nconn));
    }
    cv_.wait(lock);
  }
  while(!ec);

  return connection();
}

auto connection_pool::steal_connection(error_code & ec) -> connection
{
  std::unique_lock<std::mutex> lock{mtx_};
  do
  {

    if (!free_conns_.empty())
    {
      auto pp = std::move(free_conns_.front());
      free_conns_.erase(free_conns_.begin());
      auto itr = std::find_if(
          conns_.begin(), conns_.end(),
          [&](const std::pair<endpoint_type , std::shared_ptr<detail::connection_impl>> & p)
          {
            return p.second == pp;
          });
      conns_.erase(itr);
      return connection(pp);
    }
    // check if we can make more connections. -> open another connection.
    // the race here is that we might open one too many
    else if (conns_.size() <  limit_) // open another connection then -> we block the entire
    {
      if (endpoints_.empty())
      {
        BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_found);
        return connection();
      }

      // sort the endpoints by connections that use it
      std::sort(endpoints_.begin(), endpoints_.end(),
                [this](const endpoint_type &a, const endpoint_type &b) { return conns_.count(a) < conns_.count(b); });
      const auto ep = endpoints_.front();

      std::shared_ptr<detail::connection_impl> nconn = std::make_shared<detail::connection_impl>(get_executor(), context_);
      // no one else will grab it from there, bc it's not in free_conns_
      lock.unlock();
      nconn->use_ssl(use_ssl_);
      nconn->set_host(host_);
      nconn->connect(ep, ec);
      if (ec)
        return connection();
      else
        return connection(std::move(nconn));
    }
    cv_.wait(lock);
  }
  while(!ec);

  return connection();
}


template<bool Steal>
struct connection_pool::async_get_connection_op : asio::coroutine
{
  constexpr static const char * op_name = "connection_pool::async_get_connection_op";

  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  connection_pool * this_;

  async_get_connection_op(connection_pool * this_) : this_(this_) {}

  using conn_t = boost::unordered_multimap<endpoint_type,
                                           std::shared_ptr<detail::connection_impl>,
                                           detail::endpoint_hash>;
  typename conn_t::iterator itr;
  std::shared_ptr<detail::connection_impl> nconn = nullptr;
  std::unique_lock<std::mutex> lock{this_->mtx_, std::defer_lock};
  endpoint_type ep;
  connection conn;

  template<typename Self>
  void operator()(
      Self && self,
      system::error_code ec = {});
};

template<bool Steal>
template<typename Self>
void connection_pool::async_get_connection_op<Steal>::operator()(
            Self && self,
            system::error_code ec)
{
  BOOST_ASIO_CORO_REENTER (this)
  {
    lock.lock();
    do
    {
      if (this_->endpoints_.empty())
      {
        BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_found);
        goto immediate;
      }
      if (!this_->free_conns_.empty())
      {
        auto pp = std::move(this_->free_conns_.front());
        this_->free_conns_.erase(this_->free_conns_.begin());
        conn = connection(pp);
        BOOST_IF_CONSTEXPR (Steal)
        {
          auto itr = std::find_if(
              this_->conns_.begin(),
              this_->conns_.end(),
              [&](const std::pair<endpoint_type , std::shared_ptr<detail::connection_impl>> & p)
              {
                return p.second == pp;
              });
          this_->conns_.erase(itr);
        }
        goto immediate;
      }
      // check if we can make more connections. -> open another connection.
      // the race here is that we might open one too many
      else if (this_->conns_.size() < this_->limit_) // open another connection then
      {
        if (this_->endpoints_.empty())
        {
          BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_found);
          goto immediate;
        }

        // sort the endpoints by connections that use it
        std::sort(this_->endpoints_.begin(), this_->endpoints_.end(),
                  [this](const endpoint_type &a, const endpoint_type &b) { return this_->conns_.count(a) < this_->conns_.count(b); });
        ep = this_->endpoints_.front();

        BOOST_IF_CONSTEXPR (Steal)
          nconn = std::make_shared<detail::connection_impl>(get_executor(), this_->context_);
        else
          nconn = this_->conns_.emplace(ep, std::make_shared<detail::connection_impl>(get_executor(), this_->context_))->second;

        // no one else will grab it from there, bc it's not in free_conns_
        lock.unlock();
        nconn->use_ssl(this_->use_ssl_);
        nconn->set_host(this_->host_);

        BOOST_REQUESTS_YIELD nconn->async_connect(ep, std::move(self));
        if (ec == system::errc::address_not_available)
        {
          lock.lock();
          auto itr = std::find(this_->endpoints_.begin(), this_->endpoints_.end(), ep);
          if (itr != this_->endpoints_.end())
            this_->endpoints_.erase(itr);

          if (this_->endpoints_.empty())
            this_->cv_.notify_all();
          ec.clear();
          continue;
        }
        else if (ec)
          break;
        else
        {
          conn = connection{std::move(nconn)};
          break;
        }
      }
      BOOST_REQUESTS_YIELD this_->cv_.async_wait(*lock.mutex(), std::move(self));
    }
    while(!ec);
    if (false) immediate:
    {
      lock.unlock();
      BOOST_REQUESTS_YIELD asio::dispatch(
          asio::get_associated_immediate_executor(self, this_->get_executor()),
          std::move(self));
    }
  }
  if (is_complete())
  {
    if (lock.owns_lock())
      lock.unlock();
    self.complete(ec, std::move(conn));
  }

}


connection_pool::connection_pool(connection_pool && lhs)
  : use_ssl_(lhs.use_ssl_), context_(lhs.context_), cv_(lhs.cv_.get_executor()), host_(std::move(lhs.host_)),
    endpoints_(std::move(lhs.endpoints_)), limit_(lhs.limit_),
    conns_(std::move(lhs.conns_))
{
  // If this triggers, you're moving a pool with borrowed connections
  BOOST_ASSERT(conns_.size() != free_conns_.size());
  free_conns_ = std::move(lhs.free_conns_);
}

connection_pool::~connection_pool()
{
}

void connection_pool::return_connection(connection conn)
{
  std::lock_guard<std::mutex> lock{mtx_};
  BOOST_ASSERT(
              std::find_if(
                  conns_.begin(),
                  conns_.end(),
                  [&](const std::pair<endpoint_type,
                                      std::shared_ptr<detail::connection_impl>> & p)
                  {
                    return p.second == conn.impl_;
                  }) != conns_.end());
  free_conns_.push_back(std::move(conn.impl_));
  cv_.notify_all();
}

void connection_pool::remove_connection(const connection &conn)
{
  std::lock_guard<std::mutex> lock{mtx_};
  auto itr = std::find_if(
                 conns_.begin(),
                 conns_.end(),
                 [&](const std::pair<endpoint_type,
                            std::shared_ptr<detail::connection_impl>> & p)
                 {
                   return p.second == conn.impl_;
                 });

  if (itr != conns_.end())
  {
    conns_.erase(itr);
    cv_.notify_all();
  }

}


void connection_pool::async_borrow_connection_impl(
    asio::any_completion_handler<void(error_code, connection)> handler,
    connection_pool * this_)
{

  return asio::async_compose<
      asio::any_completion_handler<void(error_code, connection)>, void(error_code, connection)>(
      async_get_connection_op<false>{this_},
      handler, this_->get_executor());
}


void  connection_pool::async_steal_connection_impl(
    asio::any_completion_handler<void(error_code, connection)> handler,
    connection_pool * this_)
{
  return asio::async_compose<
      asio::any_completion_handler<void(error_code, connection)>, void(error_code, connection)>(
      async_get_connection_op<true>{this_},
      handler, this_->get_executor());

}

void connection_pool::async_lookup_impl(
    asio::any_completion_handler<void(error_code)> handler,
    connection_pool * this_, urls::url_view av)
{
  return asio::async_compose<
      asio::any_completion_handler<void(error_code)>, void(error_code)>(
      async_lookup_op{this_, av},
      handler, this_->get_executor());
}

}
}

