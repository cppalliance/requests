//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#define checked_call(...) BOOST_REQUESTS_CHECK_ERROR(__VA_ARGS__)
#define state(...) BOOST_REQUESTS_STATE(__VA_ARGS__)
#define async_init  BOOST_REQUESTS_AINIT
#define async_state(...) BOOST_REQUESTS_ASTATE(__VA_ARGS__)
#define async_next(...)  BOOST_REQUESTS_ANEXT(__VA_ARGS__)
#define async_goto(...)  BOOST_REQUESTS_AGOTO(__VA_ARGS__)
#define async_complete(...) BOOST_REQUESTS_ACOMPLETE(__VA_ARGS__)