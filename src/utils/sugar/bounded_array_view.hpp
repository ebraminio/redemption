/*
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

Product name: redemption, a FLOSS RDP proxy
Copyright (C) Wallix 2021
Author(s): Proxies Team
*/

#pragma once

#include "utils/sugar/array_view.hpp"
#include "utils/sugar/bounded_sequence.hpp"


namespace detail
{
    template<class C>
    inline constexpr bool is_noexcept_array_view_data_v
      = noexcept(utils::data(std::declval<C>()));

    template<class C>
    inline constexpr bool is_noexcept_array_view_size_v
      = noexcept(utils::size(std::declval<C>()));

    template<class C>
    inline constexpr bool is_noexcept_array_view_data_size_v
      = is_noexcept_array_view_data_v<C> && is_noexcept_array_view_size_v<C>;


    template<class T>
    struct static_uint
    {};

    template<class T, T N>
    struct static_uint<std::integral_constant<T, N>>
    : std::enable_if<(N >= 0), std::integral_constant<std::size_t, N>>
    {};


    template<class T>
    struct is_constructible_with_elements : std::false_type
    {};

    template<class T>
    struct is_constructible_with_sub_elements : std::false_type
    {};

    template<class T, std::size_t N>
    struct is_constructible_with_sub_elements<std::array<T, N>> : std::true_type
    {};

    template<class F, std::size_t... Ints>
    constexpr auto unroll_indexes(std::index_sequence<Ints...>, F&& f)
    {
        return f(std::integral_constant<std::size_t, Ints>()...);
    }


    template<std::size_t AtMost, std::size_t N>
    constexpr std::size_t limited_sub_at_most
        = (AtMost == std::size_t(-1) ? AtMost : AtMost - N);

    template<class T>
    struct dynamic_array_view_storage
    {
        constexpr dynamic_array_view_storage(T* p, std::size_t n) noexcept
        : p(p), n(n)
        {}

        constexpr T* data() noexcept
        {
            return p;
        }

        constexpr T const* data() const noexcept
        {
            return p;
        }

        constexpr std::size_t size() const noexcept
        {
            return n;
        }

    private:
        T* p;
        std::size_t n;
    };

    template<class T, std::size_t N>
    struct fixed_array_view_storage
    {
        constexpr fixed_array_view_storage(T* p, [[maybe_unused]] std::size_t n) noexcept
        : p(p)
        {
            assert(n == N);
        }

        constexpr T* data() noexcept
        {
            return p;
        }

        constexpr T const* data() const noexcept
        {
            return p;
        }

        static constexpr std::size_t size() noexcept
        {
            return N;
        }

    private:
        T* p;
        std::size_t n;
    };

    template<bool>
    struct select_array_view_storage_impl;

    template<>
    struct select_array_view_storage_impl<false>
    {
        template<class T, std::size_t AtLeast, std::size_t AtMost>
        using f = dynamic_array_view_storage<T>;
    };

    template<>
    struct select_array_view_storage_impl<true>
    {
        template<class T, std::size_t AtLeast, std::size_t AtMost>
        using f = fixed_array_view_storage<T, AtLeast>;
    };

    template<class T, std::size_t AtLeast, std::size_t AtMost>
    using select_array_view_storage
      = typename select_array_view_storage_impl<AtLeast == AtMost>
      ::template f<T, AtLeast, AtMost>;

    template<bool IsSized>
    struct select_sized_type_as
    {
        static_assert(IsSized);

        template<template<class, std::size_t> class C, class T, std::size_t N>
        using type = C<T, N>;
    };
} // namespace detail


template<class T, std::size_t AtLeast, std::size_t AtMost>
struct bounded_array_view
{
    static_assert(AtLeast <= AtMost);

    using element_type = T const;
    using value_type = std::remove_cv_t<T>;
    using reference = T const&;
    using iterator = T const*;
    using pointer = T const*;
    using const_reference = T const&;
    using const_iterator = T const*;
    using const_pointer = T const*;
    using size_type = std::size_t;

public:
    // C++20: enable when AtLeast == AtMost == 0
    constexpr bounded_array_view() = delete;
    constexpr bounded_array_view(bounded_array_view && other) noexcept = default;
    constexpr bounded_array_view(bounded_array_view const & other) noexcept = default;
    constexpr bounded_array_view & operator = (bounded_array_view && other) noexcept = default;
    constexpr bounded_array_view & operator = (bounded_array_view const & other) noexcept = default;

    // C++20: enable when AtLeast == AtMost == 0
    constexpr bounded_array_view(std::nullptr_t /*null*/) = delete;


    static constexpr bounded_array_view assumed(const_pointer p, const_pointer e) noexcept
    {
        assert(p <= e);
        return bounded_array_view(p, std::size_t(e-p));
    }

    static constexpr bounded_array_view assumed(const_pointer p, std::size_t n) noexcept
    {
        return bounded_array_view(p, n);
    }

    template<class U = void, class = std::enable_if_t<AtLeast == AtMost, U>>
    static constexpr bounded_array_view assumed(const_pointer p) noexcept
    {
        return bounded_array_view(p, AtLeast);
    }

    template<class C>
    static constexpr bounded_array_view assumed(C&& a)
        noexcept(detail::is_noexcept_array_view_data_size_v<C&&>)
    {
        using Seq = std::remove_cv_t<std::remove_reference_t<C>>;
        if constexpr (detail::is_bounded_sequence_impl<Seq>::value) {
            using Bounds = typename detail::sequence_to_size_bounds_impl<Seq>::type;
            static_assert(AtLeast <= Bounds::at_least);
            static_assert(Bounds::at_most <= AtMost);
        }
        auto av = make_array_view(static_cast<C&&>(a));
        return bounded_array_view(av.data(), av.size());
    }


    template<class C, class Bounds = sequence_to_size_bounds_t<C>, class = std::enable_if_t<
        (AtLeast <= Bounds::at_least && Bounds::at_most <= AtMost), decltype(void(
            array_view<T>(std::declval<C&&>())
        ))
    >>
    constexpr bounded_array_view(C&& a)
        noexcept(noexcept(array_view<T>(static_cast<C&&>(a))))
    : _array(utils::data(static_cast<C&&>(a)), utils::size(static_cast<C&&>(a)))
    {}


    [[nodiscard]]
    constexpr size_type size() const noexcept
    {
        return _array.size();
    }

    [[nodiscard]]
    constexpr bool empty() const noexcept
    {
        return !size();
    }

    [[nodiscard]]
    constexpr const_pointer data() const noexcept
    {
        return _array.data();
    }

    [[nodiscard]]
    constexpr const_reference front() const noexcept
    {
        assert(size());
        return *data();
    }

    [[nodiscard]]
    constexpr const_reference back() const noexcept
    {
        assert(size());
        return data()[size()-std::size_t(1u)];
    }

    [[nodiscard]]
    constexpr const_iterator begin() const noexcept
    {
        return data();
    }

    [[nodiscard]]
    constexpr const_iterator end() const noexcept
    {
        return data() + size();
    }

    [[nodiscard]]
    constexpr const_reference operator[](size_type i) const noexcept
    {
        assert(i < size());
        return data()[i];
    }

    // TODO free functions
    //@{
    [[nodiscard]]
    constexpr array_view<T> first(size_type n) const noexcept
    {
        assert(n <= size());
        return {data(), n};
    }

    template<std::size_t n>
    [[nodiscard]]
    constexpr bounded_array_view<T, n, n> first() const noexcept
    {
        static_assert(n <= AtLeast);
        return bounded_array_view<T, n, n>::assumed(data(), n);
    }

    template<class N>
    [[nodiscard]]
    constexpr auto first(N const& /*n*/) const noexcept
        -> decltype(first<detail::static_uint<N>::value>())
    {
        return first<detail::static_uint<N>::value>();
    }

    [[nodiscard]]
    constexpr array_view<T> last(size_type n) const noexcept
    {
        assert(n <= size());
        return {data() + size() - n, n};
    }

    template<std::size_t n>
    [[nodiscard]]
    constexpr bounded_array_view<T, n, n> last() const noexcept
    {
        static_assert(n <= AtLeast);
        return bounded_array_view<T, n, n>::assumed(data() + (size() - n), n);
    }

    template<class N>
    [[nodiscard]]
    constexpr auto last(N const& /*n*/) const noexcept
        -> decltype(last<detail::static_uint<N>::value>())
    {
        return last<detail::static_uint<N>::value>();
    }

    [[nodiscard]]
    constexpr array_view<T> from_offset(size_type offset) const noexcept
    {
        assert(offset <= size());
        return {data() + offset, size() - offset};
    }

    template<std::size_t offset>
    [[nodiscard]]
    constexpr bounded_array_view<
        T,
        AtLeast - offset,
        detail::limited_sub_at_most<AtMost, offset>>
    from_offset() const noexcept
    {
        static_assert(offset <= AtLeast);
        return bounded_array_view<
            T,
            AtLeast - offset,
            detail::limited_sub_at_most<AtMost, offset>
        >::assumed(data() + offset, size() - offset);
    }

    template<class Offset>
    [[nodiscard]]
    constexpr auto from_offset(Offset const& /*n*/) const noexcept
        -> decltype(from_offset<detail::static_uint<Offset>::value>())
    {
        return from_offset<detail::static_uint<Offset>::value>();
    }

    [[nodiscard]]
    constexpr array_view<T> subarray(size_type offset, size_type count) const noexcept
    {
        assert(offset <= size() && count <= size() - offset);
        return {data() + offset, count};
    }

    template<std::size_t offset, std::size_t count>
    [[nodiscard]]
    constexpr bounded_array_view<T, count, count> subarray() const noexcept
    {
        static_assert(count + offset <= AtLeast);
        return bounded_array_view<T, count, count>::assumed(data() + offset, count);
    }

    template<class Offset, class Count>
    [[nodiscard]]
    constexpr auto
    subarray(Offset const& /*offset*/, Count const& /*n*/) const noexcept
    -> decltype(subarray<
        detail::static_uint<Offset>::value,
        detail::static_uint<Count>::value
    >())
    {
        return subarray<
            detail::static_uint<Offset>::value,
            detail::static_uint<Count>::value
        >();
    }

    [[nodiscard]]
    constexpr array_view<T> drop_front(size_type count) const noexcept
    {
        assert(count <= size());
        return {data() + count, size() - count};
    }

    template<std::size_t count>
    [[nodiscard]]
    constexpr bounded_array_view<T, AtLeast - count, detail::limited_sub_at_most<AtMost, count>>
    drop_front() const noexcept
    {
        static_assert(count <= AtLeast);
        return bounded_array_view<T, AtLeast - count, detail::limited_sub_at_most<AtMost, count>>
            ::assumed(data() + count, size() - count);
    }

    template<class Count>
    [[nodiscard]]
    constexpr auto drop_front(Count const& /*n*/) const noexcept
        -> decltype(drop_front<detail::static_uint<Count>::value>())
    {
        return drop_front<detail::static_uint<Count>::value>();
    }

    [[nodiscard]]
    constexpr array_view<T> drop_back(size_type count) const noexcept
    {
        assert(count <= size());
        return {data(), size() - count};
    }

    template<std::size_t count>
    [[nodiscard]]
    constexpr bounded_array_view<T, AtLeast - count, detail::limited_sub_at_most<AtMost, count>>
    drop_back() const noexcept
    {
        static_assert(count <= AtLeast);
        return bounded_array_view<T, AtLeast - count, detail::limited_sub_at_most<AtMost, count>>
            ::assumed(data(), size() - count);
    }

    template<class Count>
    [[nodiscard]]
    constexpr auto drop_back(Count const& /*n*/) const noexcept
        -> decltype(drop_back<detail::static_uint<Count>::value>())
    {
        return drop_back<detail::static_uint<Count>::value>();
    }

    template<class C>
    constexpr C as() const
    {
        if constexpr (AtLeast != AtMost) {
            return array_view<T>(data(), size()).template as<C>();
        }
        else if constexpr (detail::is_constructible_with_sub_elements<C>::value) {
            return detail::unroll_indexes(std::make_index_sequence<AtLeast>(), [this](auto... i) {
                return C{{data()[i]...}};
            });
        }
        else if constexpr (detail::is_constructible_with_elements<C>::value) {
            return detail::unroll_indexes(std::make_index_sequence<AtLeast>(), [this](auto... i) {
                return C{data()[i]...};
            });
        }
        else {
            return array_view<T>(data(), size()).template as<C>();
        }
    }

    template<template<class...> class C>
    constexpr auto as() const
    {
        return array_view<T>(data(), size()).template as<C>();
    }

    template<template<class, std::size_t> class C>
    constexpr auto as() const
    {
        return as<
            typename detail::select_sized_type_as<AtLeast == AtMost>
            ::template type<C, T, AtLeast>
        >();
    }
    //@}

private:
    explicit constexpr bounded_array_view(const_pointer p, std::size_t len) noexcept
    : _array(p, len)
    {
        assert(AtLeast <= len);
        assert(len <= AtMost);
    }

    detail::select_array_view_storage<T const, AtLeast, AtMost> _array;
};


template<class T, class Bounds = sequence_to_size_bounds_t<T>>
bounded_array_view(T&&) -> bounded_array_view<
    detail::value_type_array_view_from_t<T&&>,
    Bounds::at_least, Bounds::at_most
>;


template<class T, std::size_t AtLeast, std::size_t AtMost>
struct writable_bounded_array_view
{
    using element_type = T;
    using value_type = std::remove_cv_t<T>;
    using reference = T&;
    using iterator = T*;
    using pointer = T*;
    using const_reference = T const&;
    using const_iterator = T const*;
    using const_pointer = T const*;
    using size_type = std::size_t;

    static constexpr std::size_t at_least = AtLeast;
    static constexpr std::size_t at_most = AtMost;

public:
    // C++20: enable when AtLeast == AtMost == 0
    constexpr writable_bounded_array_view() = delete;
    constexpr writable_bounded_array_view(writable_bounded_array_view && other) noexcept = default;
    constexpr writable_bounded_array_view(writable_bounded_array_view const & other) noexcept = default;
    constexpr writable_bounded_array_view & operator = (writable_bounded_array_view && other) noexcept = default;
    constexpr writable_bounded_array_view & operator = (writable_bounded_array_view const & other) noexcept = default;

    // C++20: enable when AtLeast == AtMost == 0
    constexpr writable_bounded_array_view(std::nullptr_t /*null*/) = delete;

    static constexpr writable_bounded_array_view assumed(pointer p, const_pointer e) noexcept
    {
        assert(p <= e);
        return writable_bounded_array_view(p, std::size_t(e-p));
    }

    static constexpr writable_bounded_array_view assumed(pointer p, std::size_t n) noexcept
    {
        return writable_bounded_array_view(p, n);
    }

    template<class U = void, class = std::enable_if_t<AtLeast == AtMost, U>>
    static constexpr writable_bounded_array_view assumed(pointer p) noexcept
    {
        return writable_bounded_array_view(p, AtLeast);
    }

    template<class C>
    static constexpr writable_bounded_array_view assumed(C&& a)
        noexcept(detail::is_noexcept_array_view_data_size_v<C&&>)
    {
        using Seq = std::remove_cv_t<std::remove_reference_t<C>>;
        if constexpr (detail::is_bounded_sequence_impl<Seq>::value) {
            using Bounds = typename detail::sequence_to_size_bounds_impl<Seq>::type;
            static_assert(AtLeast <= Bounds::at_least);
            static_assert(Bounds::at_most <= AtMost);
        }
        auto av = make_writable_array_view(static_cast<C&&>(a));
        return writable_bounded_array_view(av.data(), av.size());
    }


    template<class C, class Bounds = sequence_to_size_bounds_t<C>, class = std::enable_if_t<
        (AtLeast <= Bounds::at_least && Bounds::at_most <= AtMost), decltype(void(
            writable_array_view<T>(std::declval<C&&>())
        ))
    >>
    explicit constexpr writable_bounded_array_view(C&& a)
        noexcept(noexcept(writable_array_view<T>(static_cast<C&&>(a))))
    : _array(utils::data(static_cast<C&&>(a)), utils::size(static_cast<C&&>(a)))
    {}

    template<class U, std::size_t AtLeast2, std::size_t AtMost2, class = std::enable_if_t<
        (AtLeast <= AtLeast2 && AtMost2 <= AtMost), decltype(void(
            *static_cast<pointer*>(nullptr) = static_cast<U*>(nullptr)
        ))
    >>
    constexpr writable_bounded_array_view(
        writable_bounded_array_view<U, AtLeast2, AtMost2> a
    ) noexcept
    : _array(a.data(), a.size())
    {}


    [[nodiscard]]
    constexpr size_type size() const noexcept
    {
        return _array.size();
    }

    [[nodiscard]]
    constexpr bool empty() const noexcept
    {
        return !size();
    }

    [[nodiscard]]
    constexpr const_pointer data() const noexcept
    {
        return _array.data();
    }

    [[nodiscard]]
    constexpr pointer data() noexcept
    {
        return _array.data();
    }

    [[nodiscard]]
    constexpr const_reference front() const noexcept/*(disable_if_n<0>::value)*/
    {
        assert(size());
        return *data();
    }

    [[nodiscard]]
    constexpr const_reference back() const noexcept/*(disable_if_n<0>::value)*/
    {
        assert(size());
        return data()[size()-std::size_t(1u)];
    }

    [[nodiscard]]
    constexpr reference front() noexcept/*(disable_if_n<0>::value)*/
    {
        assert(size());
        return *data();
    }

    [[nodiscard]]
    constexpr reference back() noexcept/*(disable_if_n<0>::value)*/
    {
        assert(size());
        return data()[size()-std::size_t(1u)];
    }

    [[nodiscard]]
    constexpr const_iterator begin() const noexcept
    {
        return data();
    }

    [[nodiscard]]
    constexpr const_iterator end() const noexcept
    {
        return data() + size();
    }

    [[nodiscard]]
    constexpr iterator begin() noexcept
    {
        return data();
    }

    [[nodiscard]]
    constexpr iterator end() noexcept
    {
        return data() + size();
    }

    [[nodiscard]]
    constexpr reference operator[](size_type i) noexcept/*(disable_if_n<0>::value)*/
    {
        assert(i < size());
        return data()[i];
    }

    [[nodiscard]]
    constexpr const_reference operator[](size_type i) const noexcept/*(disable_if_n<0>::value)*/
    {
        assert(i < size());
        return data()[i];
    }

    // TODO free functions
    //@{
    [[nodiscard]]
    constexpr writable_array_view<T> first(size_type n) noexcept
    {
        assert(n <= size());
        return writable_array_view<T>{data(), n};
    }

    template<std::size_t n>
    [[nodiscard]]
    constexpr writable_bounded_array_view<T, n, n> first() noexcept
    {
        static_assert(n <= AtLeast);
        return writable_bounded_array_view<T, n, n>::assumed(data(), n);
    }

    template<class N>
    [[nodiscard]]
    constexpr auto first(N const& /*n*/) noexcept
        -> decltype(first<detail::static_uint<N>::value>())
    {
        return first<detail::static_uint<N>::value>();
    }

    [[nodiscard]]
    constexpr array_view<T> first(size_type n) const noexcept
    {
        assert(n <= size());
        return {data(), n};
    }

    template<std::size_t n>
    [[nodiscard]]
    constexpr bounded_array_view<T, n, n> first() const noexcept
    {
        static_assert(n <= AtLeast);
        return bounded_array_view<T, n, n>::assumed(data(), n);
    }

    template<class N>
    [[nodiscard]]
    constexpr auto first(N const& /*n*/) const noexcept
        -> decltype(first<detail::static_uint<N>::value>())
    {
        return first<detail::static_uint<N>::value>();
    }


    [[nodiscard]]
    constexpr writable_array_view<T> last(size_type n) noexcept
    {
        assert(n <= size());
        return writable_array_view<T>{data() + size() - n, n};
    }

    template<std::size_t n>
    [[nodiscard]]
    constexpr writable_bounded_array_view<T, n, n> last() noexcept
    {
        static_assert(n <= AtLeast);
        return writable_bounded_array_view<T, n, n>
            ::assumed(data() + (size() - n));
    }

    template<class N>
    [[nodiscard]]
    constexpr auto last(N const& /*n*/) noexcept
        -> decltype(last<detail::static_uint<N>::value>())
    {
        return last<detail::static_uint<N>::value>();
    }

    [[nodiscard]]
    constexpr array_view<T> last(size_type n) const noexcept
    {
        assert(n <= size());
        return {data() + size() - n, n};
    }

    template<std::size_t n>
    [[nodiscard]]
    constexpr bounded_array_view<T, n, n> last() const noexcept
    {
        static_assert(n <= AtLeast);
        return bounded_array_view<T, n, n>::assumed(data() + (size() - n), n);
    }

    template<class N>
    [[nodiscard]]
    constexpr auto last(N const& /*n*/) const noexcept
        -> decltype(last<detail::static_uint<N>::value>())
    {
        return last<detail::static_uint<N>::value>();
    }


    [[nodiscard]]
    constexpr writable_array_view<T> from_offset(size_type offset) noexcept
    {
        assert(offset <= size());
        return writable_array_view<T>{data() + offset, size() - offset};
    }

    template<std::size_t offset>
    [[nodiscard]]
    constexpr writable_bounded_array_view<
        T,
        AtLeast - offset,
        detail::limited_sub_at_most<AtMost, offset>>
    from_offset() noexcept
    {
        static_assert(offset <= AtLeast);
        return bounded_array_view<
            T,
            AtLeast - offset,
            detail::limited_sub_at_most<AtMost, offset>
        >::assumed(data() + offset, size() - offset);
    }

    template<class Offset>
    [[nodiscard]]
    constexpr auto from_offset(Offset const& /*n*/) noexcept
        -> decltype(from_offset<detail::static_uint<Offset>::value>())
    {
        return from_offset<detail::static_uint<Offset>::value>();
    }

    [[nodiscard]]
    constexpr array_view<T> from_offset(size_type offset) const noexcept
    {
        assert(offset <= size());
        return {data() + offset, size() - offset};
    }

    template<std::size_t offset>
    [[nodiscard]]
    constexpr bounded_array_view<
        T,
        AtLeast - offset,
        detail::limited_sub_at_most<AtMost, offset>>
    from_offset() const noexcept
    {
        static_assert(offset <= AtLeast);
        return bounded_array_view<
            T,
            AtLeast - offset,
            detail::limited_sub_at_most<AtMost, offset>
        >::assumed(data() + offset, size() - offset);
    }

    template<class Offset>
    [[nodiscard]]
    constexpr auto from_offset(Offset const& /*n*/) const noexcept
        -> decltype(from_offset<detail::static_uint<Offset>::value>())
    {
        return from_offset<detail::static_uint<Offset>::value>();
    }


    [[nodiscard]]
    constexpr writable_array_view<T> subarray(size_type offset, size_type count) noexcept
    {
        assert(offset <= size() && count <= size() - offset);
        return writable_array_view<T>{data() + offset, count};
    }

    template<std::size_t offset, std::size_t count>
    [[nodiscard]]
    constexpr writable_bounded_array_view<T, count, count> subarray() noexcept
    {
        static_assert(count + offset <= AtLeast);
        return writable_bounded_array_view<T, count, count>
            ::assumed(data() + offset, count);
    }

    template<class Offset, class Count>
    [[nodiscard]]
    constexpr auto
    subarray(Offset const& /*offset*/, Count const& /*n*/) noexcept
    -> decltype(subarray<
        detail::static_uint<Offset>::value,
        detail::static_uint<Count>::value
    >())
    {
        return subarray<
            detail::static_uint<Offset>::value,
            detail::static_uint<Count>::value
        >();
    }

    [[nodiscard]]
    constexpr array_view<T> subarray(size_type offset, size_type count) const noexcept
    {
        assert(offset <= size() && count <= size() - offset);
        return {data() + offset, count};
    }

    template<std::size_t offset, std::size_t count>
    [[nodiscard]]
    constexpr bounded_array_view<T, count, count> subarray() const noexcept
    {
        static_assert(count + offset <= AtLeast);
        return bounded_array_view<T, count, count>::assumed(data() + offset, count);
    }

    template<class Offset, class Count>
    [[nodiscard]]
    constexpr auto
    subarray(Offset const& /*offset*/, Count const& /*n*/) const noexcept
    -> decltype(subarray<
        detail::static_uint<Offset>::value,
        detail::static_uint<Count>::value
    >())
    {
        return subarray<
            detail::static_uint<Offset>::value,
            detail::static_uint<Count>::value
        >();
    }


    [[nodiscard]]
    constexpr writable_array_view<T> drop_front(size_type count) noexcept
    {
        assert(count <= size());
        return writable_array_view<T>{data() + count, size() - count};
    }

    template<std::size_t count>
    [[nodiscard]]
    constexpr writable_bounded_array_view<T, AtLeast - count, detail::limited_sub_at_most<AtMost, count>>
    drop_front() noexcept
    {
        static_assert(count <= AtLeast);
        return writable_bounded_array_view<T, AtLeast - count, detail::limited_sub_at_most<AtMost, count>>
            ::assumed(data() + count, size() - count);
    }

    template<class Count>
    [[nodiscard]]
    constexpr auto drop_front(Count const& /*n*/) noexcept
        -> decltype(drop_front<detail::static_uint<Count>::value>())
    {
        return drop_front<detail::static_uint<Count>::value>();
    }

    [[nodiscard]]
    constexpr array_view<T> drop_front(size_type count) const noexcept
    {
        assert(count <= size());
        return {data() + count, size() - count};
    }

    template<std::size_t count>
    [[nodiscard]]
    constexpr bounded_array_view<T, AtLeast - count, detail::limited_sub_at_most<AtMost, count>>
    drop_front() const noexcept
    {
        static_assert(count <= AtLeast);
        return bounded_array_view<T, AtLeast - count, detail::limited_sub_at_most<AtMost, count>>
            ::assumed(data() + count, size() - count);
    }

    template<class Count>
    [[nodiscard]]
    constexpr auto drop_front(Count const& /*n*/) const noexcept
        -> decltype(drop_front<detail::static_uint<Count>::value>())
    {
        return drop_front<detail::static_uint<Count>::value>();
    }


    [[nodiscard]]
    constexpr writable_array_view<T> drop_back(size_type count) noexcept
    {
        assert(count <= size());
        return writable_array_view<T>{data(), size() - count};
    }

    template<std::size_t count>
    [[nodiscard]]
    constexpr writable_bounded_array_view<T, AtLeast - count, detail::limited_sub_at_most<AtMost, count>>
    drop_back() noexcept
    {
        static_assert(count <= AtLeast);
        return writable_bounded_array_view<T, AtLeast - count, detail::limited_sub_at_most<AtMost, count>>
            ::assumed(data(), size() - count);
    }

    template<class Count>
    [[nodiscard]]
    constexpr auto drop_back(Count const& /*n*/) noexcept
        -> decltype(drop_back<detail::static_uint<Count>::value>())
    {
        return drop_back<detail::static_uint<Count>::value>();
    }

    [[nodiscard]]
    constexpr array_view<T> drop_back(size_type count) const noexcept
    {
        assert(count <= size());
        return {data(), size() - count};
    }

    template<std::size_t count>
    [[nodiscard]]
    constexpr bounded_array_view<T, AtLeast - count, detail::limited_sub_at_most<AtMost, count>>
    drop_back() const noexcept
    {
        static_assert(count <= AtLeast);
        return bounded_array_view<T, AtLeast - count, detail::limited_sub_at_most<AtMost, count>>
            ::assumed(data(), size() - count);
    }

    template<class Count>
    [[nodiscard]]
    constexpr auto drop_back(Count const& /*n*/) const noexcept
        -> decltype(drop_back<detail::static_uint<Count>::value>())
    {
        return drop_back<detail::static_uint<Count>::value>();
    }



    template<class C>
    constexpr C as() const
    {
        if constexpr (AtLeast != AtMost) {
            return array_view<T>(data(), size()).template as<C>();
        }
        else if constexpr (detail::is_constructible_with_sub_elements<C>::value) {
            return detail::unroll_indexes(std::make_index_sequence<size()>(), [this](auto... i) {
                return C{{data()[i]...}};
            });
        }
        else if constexpr (detail::is_constructible_with_elements<C>::value) {
            return detail::unroll_indexes(std::make_index_sequence<size()>(), [this](auto... i) {
                return C{data()[i]...};
            });
        }
        else {
            return array_view<T>(data(), size()).template as<C>();
        }
    }

    template<template<class...> class C>
    constexpr auto as() const
    {
        return array_view<T>(data(), size()).template as<C>();
    }

    template<template<class, std::size_t> class C>
    constexpr auto as() const
    {
        return as<
            typename detail::select_sized_type_as<AtLeast == AtMost>
            ::template type<C, T, AtLeast>
        >();
    }

    template<class C>
    constexpr C as()
    {
        if constexpr (AtLeast != AtMost) {
            return writable_array_view<T>(data(), size()).template as<C>();
        }
        else if constexpr (detail::is_constructible_with_sub_elements<C>::value) {
            return detail::unroll_indexes(std::make_index_sequence<size()>(), [this](auto... i) {
                return C{{data()[i]...}};
            });
        }
        else if constexpr (detail::is_constructible_with_elements<C>::value) {
            return detail::unroll_indexes(std::make_index_sequence<size()>(), [this](auto... i) {
                return C{data()[i]...};
            });
        }
        else {
            return writable_array_view<T>(data(), size()).template as<C>();
        }
    }

    template<template<class...> class C>
    constexpr auto as()
    {
        return writable_array_view<T>(data(), size()).template as<C>();
    }

    template<template<class, std::size_t> class C>
    constexpr auto as()
    {
        return as<
            typename detail::select_sized_type_as<AtLeast == AtMost>
            ::template type<C, T, AtLeast>
        >();
    }
    //@}

private:
    explicit constexpr writable_bounded_array_view(pointer p, std::size_t len) noexcept
    : _array(p, len)
    {
        assert(AtLeast <= len);
        assert(len <= AtMost);
    }

    detail::select_array_view_storage<T, AtLeast, AtMost> _array;
};


template<class T, class Bounds = sequence_to_size_bounds_t<T>>
writable_bounded_array_view(T&&) -> writable_bounded_array_view<
    detail::value_type_array_view_from_t<T&&>,
    Bounds::at_least, Bounds::at_most
>;


namespace detail
{
    template<class T, std::size_t AtLeast, std::size_t AtMost>
    struct sequence_to_size_bounds_impl<bounded_array_view<T, AtLeast, AtMost>>
    {
        using type = size_bounds<AtLeast, AtMost>;
    };

    template<class T, std::size_t AtLeast, std::size_t AtMost>
    struct sequence_to_size_bounds_impl<writable_bounded_array_view<T, AtLeast, AtMost>>
    {
        using type = size_bounds<AtLeast, AtMost>;
    };
} // namespace detail



template<class T, std::size_t AtLeast, std::size_t AtMost>
constexpr bounded_array_view<T, AtLeast, AtMost>
make_bounded_array_view(bounded_array_view<T, AtLeast, AtMost> av) noexcept
{
    return av;
}

template<class T, std::size_t AtLeast, std::size_t AtMost>
constexpr bounded_array_view<T, AtLeast, AtMost>
make_bounded_array_view(writable_bounded_array_view<T, AtLeast, AtMost> av) noexcept
{
    return av;
}

template<class Cont>
constexpr auto make_bounded_array_view(Cont const& cont)
    noexcept(noexcept(bounded_array_view{cont}))
-> decltype(bounded_array_view{cont})
{
    return {cont};
}

template<std::size_t AtLeast, std::size_t AtMost, class Cont,
    class AV = bounded_array_view<
        detail::value_type_array_view_from_t<Cont const&>,
        AtLeast, AtMost
    >
>
constexpr AV make_bounded_array_view(Cont const& cont)
    noexcept(detail::is_noexcept_array_view_data_size_v<Cont const&>)
{
    return AV::assumed(cont);
}

template<std::size_t AtLeast, std::size_t AtMost, class T, std::size_t N>
constexpr bounded_array_view<T, AtLeast, AtMost>
make_bounded_array_view(T const (&arr)[N]) noexcept
{
    static_assert(AtLeast <= N);
    static_assert(N <= AtMost);
    return bounded_array_view<T, AtLeast, AtMost>::assumed(arr, N);
}

template<class T, std::size_t N>
constexpr bounded_array_view<T, N, N>
make_bounded_array_view(T const (&arr)[N]) noexcept
{
    return bounded_array_view<T, N, N>::assumed(arr, N);
}


template<class T, std::size_t AtLeast, std::size_t AtMost>
constexpr writable_bounded_array_view<T, AtLeast, AtMost>
make_writable_bounded_array_view(writable_bounded_array_view<T, AtLeast, AtMost> av) noexcept
{
    return av;
}

template<class Cont>
constexpr auto make_writable_bounded_array_view(Cont& cont)
    noexcept(noexcept(writable_bounded_array_view{cont}))
-> decltype(writable_bounded_array_view{cont})
{
    return writable_bounded_array_view{cont};
}

template<std::size_t AtLeast, std::size_t AtMost, class Cont,
    class AV = bounded_array_view<
        detail::value_type_array_view_from_t<Cont const&>,
        AtLeast, AtMost
    >
>
constexpr AV make_writable_bounded_array_view(Cont const& cont)
    noexcept(detail::is_noexcept_array_view_data_size_v<Cont const&>)
{
    return AV::assumed(cont);
}

template<std::size_t AtLeast, std::size_t AtMost, class T, std::size_t N>
constexpr writable_bounded_array_view<T, AtLeast, AtMost>
make_writable_bounded_array_view(T const (&arr)[N]) noexcept
{
    static_assert(AtLeast <= N);
    static_assert(N <= AtMost);
    return writable_bounded_array_view<T, AtLeast, AtMost>::assumed(arr, N);
}

template<class T, std::size_t N>
constexpr writable_bounded_array_view<T, N, N>
make_writable_bounded_array_view(T (&arr)[N]) noexcept
{
    return writable_bounded_array_view<T, N, N>::assumed(arr, N);
}


template<class T, std::size_t N>
using sized_array_view = bounded_array_view<T, N, N>;

template<class T, std::size_t N>
using writable_sized_array_view = writable_bounded_array_view<T, N, N>;

template<std::size_t N> using sized_chars_view = sized_array_view<char, N>;
template<std::size_t N> using sized_u8_array_view = sized_array_view<std::uint8_t, N>;


template<class T, std::size_t Size>
constexpr sized_array_view<T, Size>
make_sized_array_view(sized_array_view<T, Size> av) noexcept
{
    return av;
}

template<class T, std::size_t Size>
constexpr sized_array_view<T, Size>
make_sized_array_view(writable_sized_array_view<T, Size> av) noexcept
{
    return av;
}

template<class Cont, class AV = sized_array_view<
    detail::value_type_array_view_from_t<Cont const&>,
    detail::size_bounds_to_static_size<sequence_to_size_bounds_t<Cont>>::value
>>
constexpr AV make_sized_array_view(Cont const& cont)
    noexcept(detail::is_noexcept_array_view_data_size_v<Cont const&>)
{
    return AV::assumed(cont);
}

template<class T, std::size_t N>
constexpr sized_array_view<T, N>
make_sized_array_view(T const (&arr)[N]) noexcept
{
    return sized_array_view<T, N>::assumed(&arr[0]);
}

template<std::size_t N, class T, class AV
    = sized_array_view<detail::value_type_array_view_from_t<T&&>, N>>
constexpr AV make_sized_array_view(T&& x)
    noexcept(noexcept(AV::assumed(static_cast<T&&>(x))))
{
    return AV::assumed(static_cast<T&&>(x));
}

template<std::size_t N, class T>
constexpr sized_array_view<T, N> make_sized_array_view(T const* x) noexcept
{
    return sized_array_view<T, N>::assumed(x);
}


// TODO renamed to zstring_array
template<std::size_t N>
constexpr sized_array_view<char, N-1>
cstr_sized_array_view(char const (&str)[N]) noexcept
{
    return sized_array_view<char, N-1>::assumed(str);
}

// TODO renamed to zstring_array
// forbidden: sized_array_view is for litterals
template<std::size_t N>
sized_array_view<char, N-1> cstr_sized_array_view(char (&str)[N]) = delete;


template<class T, std::size_t Size>
constexpr writable_sized_array_view<T, Size>
make_writable_sized_array_view(writable_sized_array_view<T, Size> av) noexcept
{
    return av;
}

template<class Cont, class AV = writable_sized_array_view<
    detail::value_type_array_view_from_t<Cont const&>,
    detail::size_bounds_to_static_size<sequence_to_size_bounds_t<Cont>>::value
>>
constexpr AV make_writable_sized_array_view(Cont& cont)
    noexcept(detail::is_noexcept_array_view_data_size_v<Cont const&>)
{
    return AV::assumed(cont);
}

template<class T, std::size_t N>
constexpr writable_sized_array_view<T, N>
make_writable_sized_array_view(T (&arr)[N]) noexcept
{
    return writable_sized_array_view<T, N>::assumed(&arr[0]);
}


template<std::size_t N, class T, class AV
    = writable_sized_array_view<detail::value_type_array_view_from_t<T&&>, N>>
constexpr AV make_writable_sized_array_view(T&& x)
    noexcept(noexcept(AV::assumed(static_cast<T&&>(x))))
{
    return AV::assumed(static_cast<T&&>(x));
}

template<std::size_t N, class T>
constexpr writable_sized_array_view<T, N>
make_writable_sized_array_view(T* x) noexcept
{
    return writable_sized_array_view<T, N>::assumed(x);
}


namespace detail
{
    template<char... cs>
    struct static_constexpr_array
    {
        static constexpr char data[sizeof...(cs)+1] {cs..., char(0)};
    };
}

REDEMPTION_DIAGNOSTIC_PUSH()
REDEMPTION_DIAGNOSTIC_CLANG_IGNORE("-Wgnu-string-literal-operator-template")
REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wpedantic")
template<class C, C... cs>
constexpr sized_array_view<char, sizeof...(cs)> operator "" _sized_av() noexcept
{
    static_assert(std::is_same_v<C, char>);

    return sized_array_view<char, sizeof...(cs)>
        ::assumed(detail::static_constexpr_array<cs...>::data);
}
REDEMPTION_DIAGNOSTIC_POP()


template<std::size_t N>
using sized_chars_view = bounded_array_view<char, N, N>;

template<std::size_t N>
using sized_u8_array_view = bounded_array_view<std::uint8_t, N, N>;

template<std::size_t N>
using sized_writable_u8_array_view = writable_bounded_array_view<std::uint8_t, N, N>;


template<std::size_t AtLeast, std::size_t AtMost>
using bounded_chars_view = bounded_array_view<char, AtLeast, AtMost>;

template<std::size_t AtLeast, std::size_t AtMost>
using bounded_u8_array_view = bounded_array_view<std::uint8_t, AtLeast, AtMost>;

template<std::size_t AtLeast, std::size_t AtMost>
using bounded_writable_u8_array_view = writable_bounded_array_view<std::uint8_t, AtLeast, AtMost>;


template<class T, class Bounds>
using bounded_array_view_with
  = bounded_array_view<T, Bounds::at_least, Bounds::at_most>;

template<class T, class Bounds>
using writable_bounded_array_view_with
  = writable_bounded_array_view<T, Bounds::at_least, Bounds::at_most>;

