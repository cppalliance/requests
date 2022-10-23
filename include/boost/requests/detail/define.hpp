//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#define checked_call(...) BOOST_REQUESTS_CHECK_ERROR(__VA_ARGS__)
#define state(...) BOOST_REQUESTS_STATE(__VA_ARGS__)