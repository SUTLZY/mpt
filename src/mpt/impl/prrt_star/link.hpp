// Software License Agreement (BSD-3-Clause)
//
// Copyright 2018 The University of North Carolina at Chapel Hill
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.

//! @author Jeff Ichnowski

#pragma once
#ifndef MPT_IMPL_PRRT_STAR_LINK_HPP
#define MPT_IMPL_PRRT_STAR_LINK_HPP

#include "node.hpp"
#include <atomic>
#include <cassert>

namespace unc::robotics::mpt::impl::prrt_star {
    template <typename State, typename Distance, bool concurrent>
    class Link;

    // Specialization of Link for concurrent RRT*
    template <typename State, typename Distance>
    class Link<State, Distance, true> {
        using Node = prrt_star::Node<State, Distance, true>;

        Node *node_;
        Link *parent_;
        Distance cost_;

        std::atomic<Link*> firstChild_{nullptr};
        std::atomic<Link*> nextSibling_{nullptr};

    public:
        Link(const Link&) = delete;
        Link(Link&&) = delete;

        explicit Link(Node *node)
            : node_(node), parent_(nullptr), cost_(0)
        {
        }

        Link(Node *node, Link *parent, Distance cost)
            : node_(node), parent_(parent), cost_(cost)
        {
            Link *next = parent->firstChild_.load(std::memory_order_relaxed);
            do {
                nextSibling_.store(next, std::memory_order_relaxed);
            } while (!parent->firstChild_.compare_exchange_weak(
                         next, this,
                         std::memory_order_release,
                         std::memory_order_relaxed));
        }

        Node* node() {
            return node_;
        }

        const Node* node() const {
            return node_;
        }

        Distance cost() const {
            return cost_;
        }

        const Link *parent() const {
            return parent_;
        }

        Link *firstChild(std::memory_order order) {
            return firstChild_.load(order);
        }

        bool casFirstChild(Link*& expect, Link* value, std::memory_order success, std::memory_order failure) {
            return firstChild_.compare_exchange_weak(expect, value, success, failure);
        }

        Link *nextSibling(std::memory_order order) {
            return nextSibling_.load(order);
        }
    };

    // Specialization of Link for non-concurrent version of RRT*.  In
    // the non-concurrent version, Nodes and Links are 1:1, so Links
    // are stored in the Node instead of allocated separately.  To
    // store it in the node and make it easy to traverse between the
    // two, Link is base class of Node.
    template <typename State, typename Distance>
    class Link<State, Distance, false> {
        using Node = prrt_star::Node<State, Distance, false>;

        Link *parent_;
        Distance cost_;

        Link *firstChild_{nullptr};
        Link *nextSibling_;

    protected:
        Link(const Link&) = delete;
        Link(Link&&) = delete;

        Link()
            : parent_(nullptr)
            , cost_(0)
            , nextSibling_{nullptr}
        {
        }

        Link(Link *parent, Distance cost)
            : parent_(parent)
            , cost_(cost)
            , nextSibling_(parent->firstChild_)
        {
            parent->firstChild_ = this;
        }

    public:
        Node* node() {
            return static_cast<Node*>(this);
        }

        const Node* node() const {
            return static_cast<const Node*>(this);
        }

        Distance cost() const {
            return cost_;
        }

        void setCost(Distance cost) {
            assert(0 <= cost && cost <= cost_);
            cost_ = cost;
        }

        const Link *parent() const {
            return parent_;
        }

        void setParent(Link *newParent) {
            assert(newParent != nullptr && newParent->cost() <= cost_ && newParent != parent_);

            // remove from old parent
            if (parent_ != nullptr) {
                Link *c = parent_->firstChild_;
                if (c == this) {
                    // this node is the first child, remove it from
                    // the parent by updating the parent's chlid list
                    // to start at the next link in the list.
                    parent_->firstChild_ = nextSibling_;
                } else {
                    // since the old parent must contain the child, it
                    // is safe to leave out a nullptr check for end of
                    // list.
                    Link *prev;
                    while ((c = (prev = c)->nextSibling_) != this)
                        assert(c != nullptr);
                    prev->nextSibling_ = nextSibling_;
                }
            }

            parent_ = newParent;
            nextSibling_ = newParent->firstChild_;
            newParent->firstChild_ = this;
        }

        Link *firstChild(std::memory_order) {
            return firstChild_;
        }

        Link *nextSibling(std::memory_order) {
            return nextSibling_;
        }
    };
}

#endif
