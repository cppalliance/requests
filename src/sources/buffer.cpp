//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/requests/sources/buffer.hpp>

namespace boost
{
namespace requests
{
namespace sources
{

source_ptr tag_invoke(make_source_tag, asio::const_buffer cb)
{
  return std::make_shared<buffer_source>(cb);
}


}
}
}
