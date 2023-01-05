// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_SOURCE_IPP
#define BOOST_REQUESTS_SOURCE_IPP

#include <boost/requests/source.hpp>
#include <boost/requests/sources/buffer.hpp>
#include <boost/requests/sources/file.hpp>
#include <boost/requests/sources/form.hpp>
#include <boost/requests/sources/json.hpp>

namespace boost
{
namespace requests
{

source_ptr tag_invoke(const make_source_tag &, source_ptr ptr)
{
  return ptr;
}

source_ptr tag_invoke(const make_source_tag &, source & src)
{
  return source_ptr(&src, [](source *) {});
}

source_ptr tag_invoke(const make_source_tag & tag, asio::const_buffer cb)
{
  return std::allocate_shared<buffer_source>(tag.get_allocator(), cb);
}

source_ptr tag_invoke(const make_source_tag & tag, urls::params_encoded_view pev)
{
  return std::allocate_shared<form_source>(tag.get_allocator(), pev);
}

source_ptr tag_invoke(const make_source_tag &tag, form f)
{
  return std::allocate_shared<form_source>(tag.get_allocator(), std::move(f));
}

source_ptr tag_invoke(const make_source_tag &tag, const boost::filesystem::path & path)
{
  return std::allocate_shared<file_source>(tag.get_allocator(), path);
}

#if defined(__cpp_lib_filesystem)
source_ptr tag_invoke(const make_source_tag &tag, const std::filesystem::path & path)
{
  return std::allocate_shared<file_source>(tag.get_allocator(), path);
}
#endif

source_ptr tag_invoke(const make_source_tag &tag, const boost::json::value & f)
{
  return std::allocate_shared<json_source>(tag.get_allocator(), f);
}


}
}

#endif //BOOST_REQUESTS_SOURCE_IPP
