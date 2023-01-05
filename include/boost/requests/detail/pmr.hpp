// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_REQUESTS_DETAIL_PMR_HPP
#define BOOST_REQUESTS_DETAIL_PMR_HPP

#include <boost/container/pmr/memory_resource.hpp>

namespace boost {
namespace requests {
namespace detail {

struct pmr_deleter
{
  container::pmr::memory_resource *res;

  constexpr pmr_deleter(container::pmr::memory_resource *res = container::pmr::get_default_resource()) noexcept: res(
      res) {}

  template<typename T>
  void operator()(T *ptr)
  {
    ptr->~T();
    res->deallocate(ptr, sizeof(T), alignof(T));
  }
};

template<typename T, typename ... Args>
std::unique_ptr <T, pmr_deleter> make_pmr(container::pmr::memory_resource *res, Args &&... args)
{
  void *raw = res->allocate(sizeof(T), alignof(T));
  try
  {
    return {new(raw) T(std::forward<Args>(args)...), res};

  }
  catch (...)
  {
    res->deallocate(raw, sizeof(T), alignof(T));
    throw;
  }
}

}
}
}

#endif //BOOST_REQUESTS_DETAIL_PMR_HPP
