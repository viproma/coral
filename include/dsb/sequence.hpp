/**
\file
\brief Main header file for dsb::sequence.
*/
#ifndef DSB_SEQUENCE_HPP
#define DSB_SEQUENCE_HPP

#include <cassert>
#include <iterator>
#include <memory>
#include <type_traits>

#include "dsb/config.h"


namespace dsb
{

/**
\brief  Module that defines a sequence iteration abstraction, similar to
        standard iterators, but simpler.

The central class in this module is dsb::sequence::Sequence.  Please see
its documentation for more information.
*/
namespace sequence
{


/**
\brief  An interface for sequence implementations.

To create a new sequence type (e.g. to support a new kind of backing storage),
create a class which inherits and implements this interface, and pass an
object of that class to the Sequence constructor.

`ElementT` is the type of the elements in the sequence, i.e., the return type
of Next().

\see Sequence
*/
template<typename ElementT>
class ISequenceImpl
{
public:
    /// Returns whether we have reached the end of the sequence.
    virtual bool Empty() = 0;

    /**
    \brief  Returns the element currently at the front of the sequence
            and iterates one step past it.

    It is OK to assume that Empty() has, or would have, returned `false`
    before this function is called (but good practice to verify it with
    an assertion).
    */
    virtual ElementT& Next() = 0;

    // Allows deletions through a base class pointer.
    virtual ~ISequenceImpl() { }
};


/**
\brief  A class that is used to iterate over a sequence of elements of
        type `ElementT`.

This class defines two functions, Empty() and Next().  The former returns
whether we have reached the end of the sequence, and the latter returns a
reference to the element at the head of the range and simultaneously iterates
past it.  The idiomatic way to iterate a sequence is thus:
~~~~{.cpp}
while (!sequence.Empty()) {
    auto element = seq.Next();
    // Do stuff with element here
}
~~~~

Sequence objects fulfil the main purpose of standard iterators, but
are different from them in several ways, some of which are advantages
and others of which are drawbacks, depending on the situation:

  - A Sequence currently only allows one-way iteration.  (In that respect
    it is similar to an [input iterator](http://www.cplusplus.com/reference/iterator/InputIterator/).)

  - A Sequence is actually more like a *pair* of iterators: the beginning
    and the end of the sequence.  This makes it easier to use, because it
    is only necessary to keep track of one object instead of two.  If `b`
    is the "begin" iterator and `e` is the "end" iterator, then Empty() is
    equivalent to `b==e`, while Next() is equivalent to `*b++`.

  - Iterators are typically value types, while Sequence has reference
    semantics.  That is, if `a` is a sequence, and we set `b = a`, then
    calling either `a.Next()` or `b.Next()` will iterate both sequences
    one step.

  - The type of a Sequence only depends on the value of its elements, and
    not on the underlying container.  That is, an object of type
    `Sequence<int>` may be used to iterate over the contents of a
    `std::vector<int>`, an `std::list<int>`, or even a plain array of
    `int`s.

  - A Sequence typically has worse performance than an iterator.  This is
    related to the previous point.  Polymorphic classes are used "under the
    hood" to hide the type of the underlying container, which means that
    calls to Empty() and Next() are indirect (virtual), and that memory for
    the underlying objects has to be dynamically allocated.

The Sequence class is just a thin wrapper around a `std::shared_ptr` to
an object of type ISequenceImpl, which contains the actual implementation
of the sequence.  (This is how the type of the underlying container gets
hidden.)  To create a new sequence type it is therefore necessary to
create a class that implements the ISequenceImpl interface.

Like iterators, a Sequence object may be invalidated by changes to the
underlying storage.  The circumstances under which this does or doesn't
happen are defined by the specific ISequenceImpl implementation used.
*/
template<typename ElementT>
class Sequence
{
public:
    /// Constructs a new sequence with the given implementation.
    Sequence(std::shared_ptr<ISequenceImpl<ElementT>> impl
                = std::shared_ptr<ISequenceImpl<ElementT>>())
        : m_impl(impl)
    { }

    typedef ElementT ElementType;

    /// Returns whether we have reached the end of the sequence.
    bool Empty() { return !m_impl.get() || m_impl->Empty(); }

    /**
    \brief  Returns a reference to the element currently at the front of the
            sequence and iterates one step past it.

    Calling this function is only allowed if Empty() has, or would have,
    returned `false`.
    
    The returned reference may be invalidated by the next call to Next().
    */
    ElementT& Next() { return m_impl->Next(); }

private:
    std::shared_ptr<ISequenceImpl<ElementT>> m_impl;
};


namespace detail
{
    // std::iterator_traits::value type strips off the `const` qualifier,
    // but we want to keep it when specifying the element type of a sequence.
    template<typename Iterator>
    struct Value
    {
        typedef typename std::remove_reference<typename std::iterator_traits<Iterator>::reference>::type type;
    };
}


// Implementation class for ElementsOf()
template<typename Iterator>
class IteratorSequenceImpl
    : public ISequenceImpl<typename detail::Value<Iterator>::type>
{
public:
    IteratorSequenceImpl(Iterator begin, Iterator end) : m_begin(begin), m_end(end) { }

    bool Empty() DSB_FINAL override { return m_begin == m_end; }

    typename std::iterator_traits<Iterator>::reference Next() DSB_FINAL override
    {
        return *(m_begin++);
    }

private:
    Iterator m_begin;
    Iterator m_end;
};


/**
\brief  Returns a sequence that wraps a pair of standard iterators.

The sequence is valid as long as the iterators are valid (which again depends
on the type of container iterated).
*/
template<typename Iterator>
Sequence<typename detail::Value<Iterator>::type> ElementsOf(
    Iterator begin, Iterator end)
{
    return Sequence<detail::Value<Iterator>::type>(
        std::make_shared<IteratorSequenceImpl<Iterator>>(begin, end));
}


/**
\brief  Convenience function which returns a Sequence that iterates over the
        entire contents of a standard container.

This function simply forwards to ElementsOf(), using the "begin" and "end"
iterators of `c`.
*/
template<typename Container>
Sequence<typename detail::Value<typename Container::iterator>::type>
    ElementsOf(Container& c)
{
    return ElementsOf(c.begin(), c.end());
}

// Same as the above, but for const containers.
template<typename Container>
Sequence<typename std::remove_reference<typename std::iterator_traits<typename Container::const_iterator>::reference>::type>
    ElementsOf(const Container& c)
{
    return ElementsOf(c.cbegin(), c.cend());
}


/**
\brief  Convenience function which returns a Sequence that iterates over the
        entire contents of an array.

This function simply forwards to ElementsOf(), using `pointer` and
`pointer+length` as iterators.
*/
template<typename ElementT>
Sequence<ElementT> ElementsOf(ElementT* pointer, size_t length)
{
    return ElementsOf(pointer, pointer + length);
}


// Implementation class for ValuesOf()
template<typename Map>
class MapValueSequenceImpl : public ISequenceImpl<typename Map::mapped_type>
{
public:
    /// Constructor that takes a map object.
    MapValueSequenceImpl(Map& map)
        : m_begin(map.begin()), m_end(map.end())
    { }

    bool Empty() DSB_FINAL override
    {
        return m_begin == m_end;
    }

    typename Map::mapped_type& Next() DSB_FINAL override
    {
        return (m_begin++)->second;
    }

private:
    typename Map::iterator m_begin;
    typename Map::iterator m_end;
};


/**
\brief  Returns a sequence representation of the mapped values in a `std::map`
        or `std::unordered_map` (or any other type with a compatible API).

The sequence stores and uses a pair of map iterators (i.e., those returned
by `map.begin()` and `map.end()`), and thus remains valid under the same
circumstances as the iterators.

Example:
~~~{.cpp}
std::map<int, double> m;
Sequence<double> s = ValuesOf(m);
~~~
*/
template<typename Map>
Sequence<typename Map::mapped_type> ValuesOf(Map& map)
{
    return Sequence<typename Map::mapped_type>(
        std::make_shared<MapValueSequenceImpl<Map>>(map));
}


// Implementation class for EmptySequence()
template<typename ElementT>
class EmptySequenceImpl : public ISequenceImpl<ElementT>
{
public:
    bool Empty() DSB_FINAL override { return true; }

    ElementT& Next() DSB_FINAL override
    {
        assert(!"Next() called on empty sequence");
        return *(static_cast<ElementT*>(nullptr));
    }
};


/// Returns an empty sequence, i.e. one for which Empty() is always `true`.
template<typename ElementT>
Sequence<ElementT> EmptySequence()
{
    return Sequence<ElementT>(std::make_shared<EmptySequenceImpl<ElementT>>());
}


// Implementation details for Only()
namespace detail
{
    template<typename ElementT>
    class OnlyImpl : public ISequenceImpl<ElementT>
    {
    public:
        OnlyImpl(const ElementT& value)
            : m_empty(false), m_value(value)
        { }

    private:
        bool Empty() DSB_FINAL override { return m_empty; }

        ElementT& Next() DSB_FINAL override
        {
            assert(!m_empty);
            m_empty = true;
            return m_value;
        }

        bool m_empty;
        ElementT m_value;
    };

    // This overload accepts only rvalue refs. when `ElementT` is specified.
    template<typename ElementT>
    Sequence<ElementT> Only(ElementT&& elem)
    {
        return Sequence<ElementT>(std::make_shared<OnlyImpl<ElementT>>(elem));
    }

    // This version accepts only lvalue references.
    template<typename ElementT>
    Sequence<ElementT> Only(ElementT& elem)
    {
        return ElementsOf(&elem, 1);
    }
}


/**
\brief  Returns a sequence which contains one element.

If `element` is an rvalue, it will be cached inside the sequence object, and
Next() will return a reference to the cached value.  If it is an lvalue, Next()
will return a reference to the original value.
*/
template<typename ElementT>
Sequence<typename std::remove_reference<ElementT>::type> Only(ElementT&& element)
{
    return detail::Only<typename std::remove_reference<ElementT>::type>
                       (std::forward<ElementT>(element));
}


// Implementation class for ReadOnly().
template<typename ElementT>
class ReadOnlySequenceImpl : public ISequenceImpl<const ElementT>
{
public:
    ReadOnlySequenceImpl(Sequence<ElementT> wrapThis) : m_wrapped(wrapThis) { }
    bool Empty() DSB_FINAL override { return m_wrapped.Empty(); }
    const ElementT& Next() DSB_FINAL override { return m_wrapped.Next(); }
private:
    Sequence<ElementT> m_wrapped;
};


/**
\brief  Returns a sequence which provides a read-only view of the elements in
        another sequence.
*/
template<typename ElementT>
Sequence<const ElementT> ReadOnly(Sequence<ElementT> sequence)
{
    return Sequence<const ElementT>(
        std::make_shared<ReadOnlySequenceImpl<ElementT>>(sequence));
}


}}      // namespace
#endif  // header guard
