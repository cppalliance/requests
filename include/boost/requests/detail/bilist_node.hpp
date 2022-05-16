#ifndef BOOST_REQUESTS_DETAIL_BILIST_NODE_HPP
#define BOOST_REQUESTS_DETAIL_BILIST_NODE_HPP

#include <boost/asio/detail/config.hpp>
#include <cstddef>

namespace boost::requests
{
namespace detail
{
struct bilist_node
{
    inline bilist_node();

    bilist_node(bilist_node const &) = delete;

    bilist_node &
    operator=(bilist_node const &) = delete;

    inline ~bilist_node();

    inline void
    unlink();

    inline void
    link_before(bilist_node *next);

    inline std::size_t size() const;

    bilist_node *next_;
    bilist_node *prev_;
};

bilist_node::bilist_node()
: next_(this)
, prev_(this)
{
}

bilist_node::~bilist_node()
{
}

inline void
bilist_node::unlink()
{
    auto p   = prev_;
    auto n   = next_;
    n->prev_ = p;
    p->next_ = n;
}

void
bilist_node::link_before(bilist_node *next)
{
    next_        = next;
    prev_        = next->prev_;
    prev_->next_ = this;
    next->prev_  = this;
}

std::size_t
bilist_node::size() const
{
    std::size_t sz = 0;
    for (auto p = next_; p != this; p = p->next_)
        sz++;
    return sz;
}

}   // namespace detail
}   // namespace boost::requests

#endif
