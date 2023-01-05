// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_SOURCES_JSON_HPP
#define BOOST_REQUESTS_SOURCES_JSON_HPP

#include <boost/requests/source.hpp>
#include <boost/json/value.hpp>
#include <boost/json/serializer.hpp>

namespace boost
{
namespace requests
{

struct json_source : source
{
  boost::json::value data;
  boost::json::serializer ser;

  json_source(json::value data) : data(std::move(data))
  {
    ser.reset(&this->data);
  }

  ~json_source() = default;
  optional<std::size_t> size() const override {return none;};
  void reset() override
  {
    ser.reset(&data);
  }
  std::pair<std::size_t, bool> read_some(asio::mutable_buffer buffer, system::error_code & ec) override
  {
    auto n = ser.read(static_cast<char *>(buffer.data()), buffer.size());
    return {n.size(), !ser.done()};
  }
  core::string_view default_content_type() override {return "application/json";}
};

}
}

#endif //BOOST_REQUESTS_SOURCES_JSON_HPP
