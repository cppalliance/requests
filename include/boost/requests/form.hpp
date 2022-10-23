//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_FORM_HPP
#define BOOST_REQUESTS_FORM_HPP

#include <boost/url/url.hpp>
#include <initializer_list>

namespace boost {
namespace requests {


struct form
{
  urls::url storage;

  form(std::initializer_list<urls::param_view> params)
  {
    storage.params().assign(std::move(params));
  }

  template<typename Container>
  form(Container && ct)
  {
    storage.params().assign(std::begin(ct), std::end(ct));
  }

};


}
}

#endif // BOOST_REQUESTS_FORM_HPP
