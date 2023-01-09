// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_SOURCES_FORM_HPP
#define BOOST_REQUESTS_SOURCES_FORM_HPP

#include <boost/requests/source.hpp>
#include <boost/beast/core/file.hpp>


#if defined(__cpp_lib_filesystem)
#include <filesystem>
#endif


namespace boost
{
namespace requests
{

struct form_source : source
{
  urls::url storage;
  urls::params_encoded_view param_view{storage.encoded_params()};
  std::size_t pos{1u};

  form_source(urls::params_encoded_view js) : param_view(js)
  {
  }
  form_source(urls::url storage) : storage(std::move(storage))
  {
  }

  form_source(form frm) : storage(std::move(frm.storage))
  {
  }

  ~form_source() = default;
  optional<std::size_t> size( ) const override
  {
    return param_view.size() - 1u;
  };
  void reset() override
  {
    pos = 1u;
  }
  std::pair<std::size_t, bool> read_some(asio::mutable_buffer buffer, system::error_code & ec) override
  {
    const auto left = param_view.buffer().size() - pos;
    const auto sz = buffer.size();
    auto dst = static_cast<char*>(buffer.data());
    auto res = std::char_traits<char>::copy(dst, param_view.buffer().data() + pos, (std::min)(left, sz));
    std::size_t n = std::distance(dst, res);
    pos += n;
    return {n, pos != param_view.buffer().size()};
  }

  core::string_view default_content_type() override { return "application/x-www-form-urlencoded"; }
};

inline form_source tag_invoke(const make_source_tag&, urls::params_encoded_view pev)
{
  return form_source(pev);

}

template<typename Form>
inline auto tag_invoke(const make_source_tag&, Form && f)
    -> std::enable_if_t<std::is_same<std::decay_t<Form>, form_source>::value, form_source>
{
  return form_source(std::move(f));
}

}
}

#endif //BOOST_REQUESTS_SOURCES_FILE_HPP
