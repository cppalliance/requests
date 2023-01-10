// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_SOURCES_JSON_HPP
#define BOOST_REQUESTS_SOURCES_JSON_HPP

#include <boost/requests/source.hpp>
#include <boost/json/fwd.hpp>

namespace boost
{

namespace requests
{

struct json_source : source
{
  boost::json::value data;
  boost::json::serializer ser;

  BOOST_REQUESTS_DECL json_source(json::value data);

  ~json_source() = default;
  optional<std::size_t> size() const override {return none;};
  BOOST_REQUESTS_DECL void reset() override;
  BOOST_REQUESTS_DECL std::pair<std::size_t, bool> read_some(void * data, std::size_t size, system::error_code & ec) override;
  core::string_view default_content_type() override {return "application/json";}
};

template<typename Json>
auto tag_invoke(const make_source_tag&, Json && f)
    -> std::enable_if_t<
        std::is_same<std::decay_t<Json>, json::value>::value ||
        std::is_same<std::decay_t<Json>, json::array>::value ||
        std::is_same<std::decay_t<Json>, json::object>::value ||
        std::is_same<std::decay_t<Json>, json::string>::value,
        json_source>
{
  return json_source(std::forward<Json>(f));
}

}
}

#endif //BOOST_REQUESTS_SOURCES_JSON_HPP
