//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_SOURCE_IMPL_FORM_IPP
#define BOOST_REQUESTS_SOURCE_IMPL_FORM_IPP

#include <boost/requests/sources/form.hpp>

namespace boost
{
namespace requests
{

source_ptr tag_invoke(make_source_tag, form form_, container::pmr::memory_resource * res)
{
  return std::allocate_shared<form_source>(
      container::pmr::polymorphic_allocator<void>(res),
      std::move(form_));
}

}
}

#endif // BOOST_REQUESTS_SOURCE_IMPL_FORM_IPP
