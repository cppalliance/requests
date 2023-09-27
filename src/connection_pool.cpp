//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/connection_pool.hpp>

namespace boost {
namespace requests {

void detail::connection_deleter::operator()(connection_impl * ptr)
{
    if (ptr->borrow_count_ == 0u)
      delete ptr;
    else
    {
      ptr->borrowed_from_ = nullptr;
      if (ptr->borrow_count_ == 0u)
        delete ptr;
    }
}


void connection_pool::lookup(urls::url_view sv, system::error_code & ec)
{
  urls::string_view scheme = "https";
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

  urls::string_view scheme = "https";

  std::unique_lock<std::mutex> lock{this_->mtx_, std::defer_lock};

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
        auto resolver = allocate_unique<asio::ip::tcp::resolver>(
                            asio::get_associated_allocator(self),
                            get_executor());
        resolver->async_resolve(
          std::string(sv.encoded_host_address().data(), sv.encoded_host_address().size()),
          std::string(service.data(), service.size()),
            asio::consign(std::move(self), std::move(resolver)));
      }

      if (!ec && eps.empty())
        BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_found)
      if (ec)
        return;

      lock.lock();
      this_->use_ssl_ = scheme == "https";
      this_->host_ = eps->host_name();
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


auto connection_pool::get_connection(error_code & ec) -> connection
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
    else if (conns_.size() <= limit_) // open another connection then -> we block the entire
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

      boost::intrusive_ptr<detail::connection_impl> nconn = conns_.emplace(ep, new detail::connection_impl(get_executor(), context_, this))->second.get();
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
  boost::intrusive_ptr<detail::connection_impl> nconn = nullptr;
  std::unique_lock<std::mutex> lock{this_->mtx_, std::defer_lock};
  endpoint_type ep;

  template<typename Self>
  void operator()(
      Self && self,
      system::error_code ec = {});
};

template<typename Self>
void connection_pool::async_get_connection_op::operator()(
            Self && self,
            system::error_code ec)
{
  connection conn;
  BOOST_ASIO_CORO_REENTER (this)
  {

    lock.lock();
    do
    {
      if (this_->endpoints_.empty())
      {
        BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_found);
        break;
      }
      if (!this_->free_conns_.empty())
      {
        auto pp = std::move(this_->free_conns_.front());
        this_->free_conns_.erase(this_->free_conns_.begin());
        conn =  connection(pp);
        break;
      }
      // check if we can make more connections. -> open another connection.
      // the race here is that we might open one too many
      else if (this_->conns_.size() < this_->limit_) // open another connection then
      {
        if (this_->endpoints_.empty())
        {
          BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_found);
          break;
        }

        // sort the endpoints by connections that use it
        std::sort(this_->endpoints_.begin(), this_->endpoints_.end(),
                  [this](const endpoint_type &a, const endpoint_type &b) { return this_->conns_.count(a) < this_->conns_.count(b); });
        ep = this_->endpoints_.front();

        nconn = this_->conns_.emplace(ep, new detail::connection_impl(get_executor(), this_->context_, this_))->second.get();

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
      BOOST_REQUESTS_YIELD this_->cv_.async_wait(lock, std::move(self));
    }
    while(!ec);
  }

  if (is_complete())
    self.complete(ec, std::move(conn));
}



struct connection_pool::async_ropen_op : asio::coroutine
{
  constexpr static const char * op_name = "connection_pool::async_ropen_op";

  using executor_type = asio::any_io_executor;
  executor_type get_executor() {return this_->get_executor(); }

  connection_pool * this_;
  beast::http::verb method;
  urls::pct_string_view path;
  http::fields & headers;
  source & src;
  request_options opt;
  cookie_jar * jar;

  connection conn;

  async_ropen_op(connection_pool * this_,
                 beast::http::verb method,
                 urls::pct_string_view path,
                 http::fields & headers,
                 source & src,
                 request_options opts,
                 cookie_jar * jar)
      : this_(this_), method(method), path(path), headers(headers), src(src), opt(std::move(opts)), jar(jar)
  {
  }


  async_ropen_op(connection_pool * this_,
                 beast::http::verb method,
                 urls::url_view path,
                 http::fields & headers,
                 source & src,
                 request_options opt,
                 cookie_jar * jar)
      : this_(this_), method(method), path(path.encoded_resource()),
        headers(headers), src(src), opt(std::move(opt)), jar(jar)
  {
  }

  template<typename Self>
  void operator()(Self && self,
                  boost::system::error_code ec = {},
                  variant2::variant<variant2::monostate, connection, stream> res = variant2::monostate());

};


template<typename Self>
void connection_pool::async_ropen_op::operator()(
    Self && self,
    boost::system::error_code ec,
    variant2::variant<variant2::monostate, connection, stream> res)
{
    BOOST_ASIO_CORO_REENTER(this)
    {
      BOOST_REQUESTS_YIELD this_->async_get_connection(std::move(self));
      conn = std::move(variant2::get<1>(res));
      if (!ec && !conn)
        BOOST_REQUESTS_ASSIGN_EC(ec, asio::error::not_found);
      if (ec)
        break;

      BOOST_REQUESTS_YIELD conn.async_ropen(method, path, headers, src, std::move(opt), jar, std::move(self));
      return self.complete(ec, variant2::get<2>(std::move(res)));
    }
  if (is_complete())
    self.complete(ec, stream{get_executor(), nullptr});

}

connection_pool::connection_pool(connection_pool && lhs)
  : use_ssl_(lhs.use_ssl_), context_(lhs.context_), cv_(lhs.cv_.get_executor()), host_(std::move(lhs.host_)),
    endpoints_(std::move(lhs.endpoints_)), limit_(lhs.limit_),
    conns_(std::move(lhs.conns_))
{
  BOOST_ASSERT(std::count_if(
      conns_.begin(), conns_.end(),
      [](const std::pair<const endpoint_type, std::unique_ptr<detail::connection_impl, detail::connection_deleter>> & p)
      {
        return p.second->borrow_count_ > 0u;
      }) == 0u);

  for (auto & p : conns_)
    p.second->borrowed_from_ = this;

  free_conns_ = std::move(lhs.free_conns_);
}

connection_pool::~connection_pool()
{
  for (auto & p : conns_)
    p.second->borrowed_from_ = nullptr;
}

void connection_pool::return_connection_(detail::connection_impl * conn)
{
  if (!conn->is_open())
    return drop_connection_(conn);

  std::lock_guard<std::mutex> lock{mtx_};
  free_conns_.push_back(std::move(conn));
  cv_.notify_all();
}

void connection_pool::drop_connection_(const detail::connection_impl * conn)
{
  std::lock_guard<std::mutex> lock{mtx_};
  auto itr = std::find_if(conns_.begin(), conns_.end(),
                          [&](const std::pair<const endpoint_type,
                                              std::unique_ptr<detail::connection_impl, detail::connection_deleter>> & e)
                          {
                            return e.second.get() == conn;
                          });
  if (itr != conns_.end())
  {
    conns_.erase(itr);
    cv_.notify_all();
  }
}

void connection_pool::async_ropen_impl(asio::any_completion_handler<void(error_code, stream)> handler,
                                       connection_pool * this_, http::verb method,
                                       urls::pct_string_view path, http::fields & headers,
                                       source & src, request_options opt, cookie_jar * jar)
{
  return asio::async_compose<
      asio::any_completion_handler<void(error_code, stream)>, void(error_code, stream)>(
      async_ropen_op{this_, method, path, headers, src, std::move(opt), jar},
      handler, this_->get_executor());
}

void connection_pool::async_get_connection_impl(
    asio::any_completion_handler<void(error_code, connection)> handler,
    connection_pool * this_)
{
  return asio::async_compose<
      asio::any_completion_handler<void(error_code, connection)>, void(error_code, connection)>(
        async_get_connection_op{this_},
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

