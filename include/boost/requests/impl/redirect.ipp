//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_IMPL_REDIRECT_IPP
#define BOOST_REQUESTS_IMPL_REDIRECT_IPP

#include <boost/requests/redirect.hpp>

namespace boost
{
namespace requests
{

bool should_redirect(redirect_mode mode,
                     urls::url_view current,
                     urls::url_view target,
                     const public_suffix_list & pse)
{
    if (mode == any)
        return true;

    const auto get_port =
            [](urls::url_view uv) -> std::uint16_t
            {
                const auto num = uv.port_number();
                if (num != 0)
                    return num;
                const auto scheme = uv.scheme_id();

                switch (scheme)
                {
                    case urls::scheme::ws:   BOOST_FALLTHROUGH;
                    case urls::scheme::http: BOOST_FALLTHROUGH;
                    case urls::scheme::none:
                        return 80; //default to port 80 for everything
                    case urls::scheme::wss:   BOOST_FALLTHROUGH;
                    case urls::scheme::https: return 443;
                    default: return 0u;
                }
            };

    // TODO: handle encoding/decoding
    const auto target_domain = target.encoded_host();
    const auto current_domain = current.encoded_host();
    switch (mode)
    {
        case private_domain:
        {
            // find the match of the domains
            const auto pp = std::mismatch(
                    current_domain.rbegin(), current_domain.rend(),
                    target_domain.rbegin(), target_domain.rend());

            auto current_itr = pp.first.base();
            auto target_itr = pp.second.base();
            if (current_itr == current_domain.end())
                return false;


            if (current_itr != current_domain.begin() && *std::prev(current_itr) != '.')
            {
                const auto itr2 = std::find(current_itr, current_domain.end(), '.');
                std::advance(target_itr, std::distance(current_itr, itr2));
            }

            if (target_itr != target_domain.begin() && *std::prev(target_itr) != '.')
                target_itr = std::find(target_itr, target_domain.end(), '.');
            if (*target_itr == '.')
                target_itr++;

            const auto common = target_domain.substr(std::distance(target_domain.begin(), target_itr));
            return !is_public_suffix(common, pse);
        }
        case subdomain:
            if (target_domain.ends_with(current_domain))
            {
                if (target_domain.size() == current_domain.size())
                    return true;
                else
                    return target_domain[target_domain.size() - current_domain.size() - 1u] == '.';
            }
        case domain:
            return target_domain == current_domain;
        case endpoint:
        {
            if (target_domain == current_domain)
            {
                auto target_port = get_port(target);
                auto current_port = get_port(current);
                return target_port != 0 && target_port == current_port;
            }
        }
        default: return false;
    }

}


}
}

#endif //BOOST_REQUESTS_IMPL_REDIRECT_IPP
