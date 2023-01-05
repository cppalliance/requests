// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_SOURCES_FILE_HPP
#define BOOST_REQUESTS_SOURCES_FILE_HPP

#include <boost/requests/source.hpp>
#include <boost/beast/core/file.hpp>

#if defined(__cpp_lib_filesystem)
#include <filesystem>
#endif


namespace boost
{
namespace requests
{

struct file_source : source
{
  filesystem::path path;
  beast::file file;
  system::error_code ec;

  file_source(const boost::filesystem::path & file) : path(file.c_str())
  {
    this->file.open(file.string().c_str(), beast::file_mode::read, ec);
  }
  file_source(const std  ::filesystem::path & file) : path(file.c_str())
  {
    this->file.open(file.string().c_str(), beast::file_mode::read, ec);
  }

  ~file_source() = default;
  optional<std::size_t> size(system::error_code & ec) const override
  {
    return file.size(ec);
  };
  void reset() override
  {
    file.seek(0, ec);
  }
  std::pair<std::size_t, bool> read_some(asio::mutable_buffer buffer, system::error_code & ec) override
  {
    if (this->ec)
    {
      ec = this->ec;
      return {0u, true};
    }

    auto n = file.read(buffer.data(), buffer.size(), ec);
    return {n, file.pos(ec) == file.size(ec)};
  }
  core::string_view default_content_type() override
  {
    const auto & mp = default_mime_type_map();
    const auto ext = path.extension().string();
    auto itr = mp.find(ext);
    if (itr != mp.end())
      return itr->second;
    else
      return "text/plain";
  }
};

}
}

#endif //BOOST_REQUESTS_SOURCES_FILE_HPP
