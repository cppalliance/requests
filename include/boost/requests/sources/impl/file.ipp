//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_SOURCES_IMPL_FILE_IPP
#define BOOST_REQUESTS_SOURCES_IMPL_FILE_IPP

#include <boost/requests/sources/file.hpp>
#include <boost/filesystem/path.hpp>

namespace boost
{
namespace requests
{

file_source::file_source(const boost::filesystem::path & file) : path(file.c_str())
{
  this->file.open(file.string().c_str(), beast::file_mode::read, ec);
}

file_source tag_invoke(const make_source_tag &tag, const boost::filesystem::path & path)
{
  return file_source(path);
}

#if defined(__cpp_lib_filesystem)

file_source::file_source(const std::filesystem::path & file) : path(file.c_str())
{
  this->file.open(file.string().c_str(), beast::file_mode::read, ec);
}

file_source tag_invoke(const make_source_tag &tag, const std::filesystem::path & path)
{
  return file_source(path);
}


#endif


}
}

#endif // BOOST_REQUESTS_SOURCES_IMPL_FILE_IPP
