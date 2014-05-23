/**
\file
\brief Main header file for dsb::sequence.
*/
#ifndef DSB_SEQUENCE_HPP
#define DSB_SEQUENCE_HPP

#include <cassert>
#include <iterator>
#include <memory>


namespace dsb
{

/**
\brief  Module that defines a sequence iteration abstraction, similar to
        iterators, but simpler.

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

`ValueT` is the type of the elements in the sequence.

\see Sequence
*/
template<typename ValueT>
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

    The returned reference is only required to be valid until the next
    time Next() is called.
    */
    virtual ValueT& Next() = 0;

    // Allows deletions through a base class pointer.
    virtual ~ISequenceImpl() { }
};


/**
\brief  A class that is used to iterate over a sequence of elements of
        type `ValueT`.

This class defines two functions, Empty() and Next().  The former returns
whether we have reached the end of the sequence, and the latter returns
a reference to the element at the head of the range and simultaneously
iterates past it.  The idiomatic way to iterate a sequence is thus:
~~~~{.cpp}
while (!sequence.Empty()) {
    auto& element = seq.Next();
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
template<typename ValueT>
class Sequence
{
public:
    /// Constructs a new sequence with the given implementation.
    Sequence(std::shared_ptr<ISequenceImpl<ValueT>> impl
                = std::shared_ptr<ISequenceImpl<ValueT>>())
        : m_impl(impl)
    { }

    typedef ValueT ValueType;

    /// Returns whether we have reached the end of the sequence.
    bool Empty() { return !m_impl.get() || m_impl->Empty(); }

    /**
    \brief  Returns the element currently at the front of the sequence
            and iterates one step past it.

    Calling this function is only allowed if Empty() has, or would have,
    returned `false`.

    In general, the returned reference is only guaranteed to be valid until
    the next time Next() is called.
    */
    ValueT& Next() { return m_impl->Next(); }

private:
    std::shared_ptr<ISequenceImpl<ValueT>> m_impl;
};


namespace detail
{
    // Helper template that strips the * off a pointer type.
    template<typename T> struct DerefType { };
    template<typename T> struct DerefType<T*> { typedef T Type; };
}


/**
\brief  A sequence implementation that allows iteration with a pair of
        standard iterators.

`Iterator` is the iterator type.  The sequence is valid as long as the
iterators are valid (which again depends on the type of container iterated).

\see ArraySequence, ContainerSequence
*/
template<typename Iterator>
class IteratorSequenceImpl : public ISequenceImpl<
    typename detail::DerefType<
        typename std::iterator_traits<Iterator>::pointer
    >::Type>
{
public:
    /**
    \brief  Constructor that takes the "begin" and "end" iterators of a
            sequence.

    The two iterators must be comparable.
    */
    IteratorSequenceImpl(Iterator begin, Iterator end)
        : m_begin(begin), m_end(end)
    { }

    bool Empty()
    {
        return m_begin == m_end;
    }

    typename detail::DerefType<typename std::iterator_traits<Iterator>::pointer>::Type&
        Next()
    {
        return *(m_begin++);
    }

private:
    Iterator m_begin;
    Iterator m_end;
};


/**
\brief  Convenience function which returns a Sequence that iterates over the
        entire contents of a standard container.

The sequence is backed by an IteratorSequenceImpl.
*/
template<typename Container>
Sequence<typename std::iterator_traits<typename Container::iterator>::value_type>
    ContainerSequence(Container& c)
{
    typedef typename Container::iterator I;
    typedef typename std::iterator_traits<I>::value_type V;
    return Sequence<V>(std::make_shared<IteratorSequenceImpl<I>>(
        c.begin(), c.end()));
}

// Same as the above, but for const containers.
template<typename Container>
Sequence<const typename std::iterator_traits<typename Container::iterator>::value_type>
    ContainerSequence(const Container& c)
{
    typedef typename Container::const_iterator I;
    typedef const typename std::iterator_traits<I>::value_type V;
    return Sequence<V>(std::make_shared<IteratorSequenceImpl<I>>(
        c.cbegin(), c.cend()));
}


/**
\brief  Convenience function which returns a Sequence that iterates over the
        entire contents of an array.

The sequence is backed by an IteratorSequenceImpl.
*/
template<typename ValueT>
Sequence<ValueT> ArraySequence(ValueT* pointer, size_t length)
{
    return Sequence<ValueT>(std::make_shared<IteratorSequenceImpl<ValueT*>>(
        pointer, pointer + length));
}


/**
\brief  A sequence implementation that allows iteration over the mapped values
        of a `std::map` or `std::unordered_map` (or any other type that has
        the same API).

The class stores and uses a pair of map iterators, so the sequence remains
valid under the same circumstances as the iterators.
*/
template<typename Map>
class MapValueSequenceImpl : public ISequenceImpl<typename Map::mapped_type>
{
public:
    /// Constructor that takes a map object.
    MapValueSequenceImpl(Map& map)
        : m_begin(map.begin()), m_end(map.end())
    { }

    bool Empty()
    {
        return m_begin == m_end;
    }

    typename Map::mapped_type& Next()
    {
        return (m_begin++)->second;
    }

private:
    typename Map::iterator m_begin;
    typename Map::iterator m_end;
};


/**
\brief  Convenience function that returns a sequence representation of the
        mapped values in a `std::map` or `std::unordered_map` (or any other
        type that has the same API).

This function allows for easy construction of a Sequence backed by a
MapValueSequenceImpl, for example:
~~~{.cpp}
std::map<int, double> m;
Sequence<double> s = MapValueSequence(m);
~~~
*/
template<typename Map>
Sequence<typename Map::mapped_type> MapValueSequence(Map& map)
{
    return Sequence<typename Map::mapped_type>(
        std::make_shared<MapValueSequenceImpl<Map>>(map));
}


}}      // namespace
#endif  // header guard
