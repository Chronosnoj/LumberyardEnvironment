#pragma once

/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <AzCore/std/utils.h>
#include <AzCore/std/iterator.h>
#include <AzCore/std/functional.h>
#include <AzCore/std/typetraits/is_base_of.h>
#include <SceneAPI/SceneCore/Containers/Views/View.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            namespace Views
            {
                // Skips values in the iterator it wraps based on a given function's result.
                //      The predicate function takes the reference of the value of iterator as its
                //      argument and must return a boolean, where true means to accept the given
                //      iterator value and false if FilterIterator needs to skip to the next value.
                //      For complex iterators it's often easier to use decltype to derive the argument
                //      to the predicate function instead of fully specifying it. (See example below)
                // Note:
                //      Because skipping happens while iterating, random access iterators will degrade
                //      to bidirectional iterators.
                // Example:
                //      vector<int> list = { 10, 20, 30, 40, 50 };
                //      auto view = MakeFilterView(list.begin(), list.end(),
                //          [](int value) -> bool
                //      {
                //          return value >= 25;
                //      });
                //      for (auto it : view)
                //      {
                //          printf( "%i ", it );
                //      }
                //      result: 30 40 50
                // Example:
                //      vector<int> list = { 10, 20, 30, 40, 50 };
                //      auto convertView = MakeConvertView<uint64_t>(list.begin(), list.end());
                //      auto filteredView = MakeFilterView(convertView,
                //          // Easier to read and automatically reflects any changes made in the view stack.
                //          [](const decltype(*convertView.begin())& object) -> bool
                //      {
                //          return value >= 25;
                //      });
                template<typename Iterator, typename Category = typename AZStd::iterator_traits<Iterator>::iterator_category>
                class FilterIterator
                {
                };

                //
                // Base iterator
                //
                template<typename Iterator>
                class FilterIterator<Iterator, void>
                {
                public:
                    using value_type = typename AZStd::iterator_traits<Iterator>::value_type;
                    using difference_type = typename AZStd::iterator_traits<Iterator>::difference_type;
                    using pointer = typename AZStd::iterator_traits<Iterator>::pointer;
                    using reference = typename AZStd::iterator_traits<Iterator>::reference;
                    using iterator_category = typename AZStd::iterator_traits<Iterator>::iterator_category;

                    using RootIterator = FilterIterator<Iterator, iterator_category>;

                    using Predicate = AZStd::function<bool(const reference)>;

                    FilterIterator(Iterator iterator, Iterator end, const Predicate& predicate);
                    FilterIterator(const FilterIterator&) = default;
                    FilterIterator& operator=(const FilterIterator&) = default;

                    RootIterator& operator++();
                    RootIterator operator++(int);

                    const Iterator& GetBaseIterator();

                protected:
                    // Pseudo default constructor.
                    //      This is used because default constructing later on would trigger a predicate
                    //      on an default constructed iterator, causing a crash when either using the
                    //      predicate or dereferencing the iterator when trying to move forward.
                    explicit FilterIterator(Iterator defaultIterator);
                    void MoveToNext();

                    Iterator m_iterator;
                    Iterator m_end;
                    Predicate m_predicate;
                };

                //
                // Input iterator
                //
                template<typename Iterator>
                class FilterIterator<Iterator, AZStd::input_iterator_tag>
                    : public FilterIterator<Iterator, void>
                {
                public:
                    using super = FilterIterator<Iterator, void>;

                    FilterIterator(Iterator iterator, Iterator end, const Predicate& predicate);
                    FilterIterator(const FilterIterator& rhs) = default;

                    bool operator==(const RootIterator& rhs) const;
                    bool operator!=(const RootIterator& rhs) const;

                    reference operator*();
                    pointer operator->();

                protected:
                    // Used when iterator is raw pointer
                    pointer GetPointer(AZStd::true_type);
                    // Used when iterator is an object
                    pointer GetPointer(AZStd::false_type);

                    // Pseudo default constructor.
                    explicit FilterIterator(Iterator defaultIterator);
                };

                //
                // Forward iterator
                //
                template<typename Iterator>
                class FilterIterator<Iterator, AZStd::forward_iterator_tag>
                    : public FilterIterator<Iterator, AZStd::input_iterator_tag>
                {
                public:
                    using super = FilterIterator<Iterator, AZStd::input_iterator_tag>;

                    FilterIterator(Iterator iterator, Iterator end, const Predicate& predicate);
                    FilterIterator(const FilterIterator& rhs) = default;
                    FilterIterator();
                };

                //
                // Bidirectional iterator
                //
                template<typename Iterator>
                class FilterIterator<Iterator, AZStd::bidirectional_iterator_tag>
                    : public FilterIterator<Iterator, AZStd::forward_iterator_tag>
                {
                public:
                    using super = FilterIterator<Iterator, AZStd::forward_iterator_tag>;

                    FilterIterator(Iterator iterator, Iterator end, const Predicate& predicate);
                    FilterIterator(Iterator iterator, Iterator begin, Iterator end, const Predicate& predicate);
                    FilterIterator(const FilterIterator& rhs) = default;
                    FilterIterator();

                    RootIterator& operator--();
                    RootIterator operator--(int);

                protected:
                    void MoveToPrevious();

                    Iterator m_begin;
                };

                //
                // Random Access iterator
                //      Because individual elements have to be inspected to know if they should be accepted, treat the random access iterator
                //      as a bidirectional iterator to force algorithms to inspect individual elements.
                //
                template<typename Iterator>
                class FilterIterator<Iterator, AZStd::random_access_iterator_tag>
                    : public FilterIterator<Iterator, AZStd::bidirectional_iterator_tag>
                {
                public:
                    using super = FilterIterator<Iterator, AZStd::bidirectional_iterator_tag>;
                    using iterator_category = AZStd::bidirectional_iterator_tag;

                    FilterIterator(Iterator iterator, Iterator end, const Predicate& predicate);
                    FilterIterator(Iterator iterator, Iterator begin, Iterator end, const Predicate& predicate);
                    FilterIterator(const FilterIterator& rhs) = default;
                    FilterIterator();
                };

                //
                // Continuous Random Access iterator
                //      See Random Access iterator
                //
                template<typename Iterator>
                class FilterIterator<Iterator, AZStd::continuous_random_access_iterator_tag>
                    : public FilterIterator<Iterator, AZStd::bidirectional_iterator_tag>
                {
                public:
                    using super = FilterIterator<Iterator, AZStd::bidirectional_iterator_tag>;
                    using iterator_category = AZStd::bidirectional_iterator_tag;

                    FilterIterator(Iterator iterator, Iterator end, const Predicate& predicate);
                    FilterIterator(Iterator iterator, Iterator begin, Iterator end, const Predicate& predicate);
                    FilterIterator(const FilterIterator& rhs) = default;
                    FilterIterator();
                };

                //
                // Utility functions
                //
                namespace Internal
                {
                    template<typename Iterator>
                    struct FilterIteratorNeedsFullRange
                    {
                        // True if the Iterator can move backwards, which requires an end and a begin iterator, otherwise false and only
                        // an end iterator is needed.
                        static const bool value = AZStd::is_base_of<AZStd::bidirectional_iterator_tag, typename Iterator::iterator_category>::value;
                    };
                }

                template<typename Iterator>
                FilterIterator<Iterator> MakeFilterIterator(Iterator current, Iterator end, const typename FilterIterator<Iterator>::Predicate& predicate);

                template<typename Iterator, typename AZStd::enable_if<Internal::FilterIteratorNeedsFullRange<Iterator>::value>::type>
                FilterIterator<Iterator> MakeFilterIterator(Iterator current, Iterator begin, Iterator end, const typename FilterIterator<Iterator>::Predicate& predicate);

                template<typename Iterator>
                View<FilterIterator<Iterator> > MakeFilterView(Iterator current, Iterator end, const typename FilterIterator<Iterator>::Predicate& predicate);

                template<typename Iterator, typename AZStd::enable_if<Internal::FilterIteratorNeedsFullRange<Iterator>::value>::type>
                View<FilterIterator<Iterator> > MakeFilterView(Iterator current, Iterator begin, Iterator end, const typename FilterIterator<Iterator>::Predicate& predicate);

                template<typename ViewType>
                View<FilterIterator<typename ViewType::iterator> > MakeFilterView(ViewType& view, const typename FilterIterator<typename ViewType::iterator>::Predicate& predicate);
            } // Views
        } // Containers
    } // SceneAPI
} // AZ

#include <SceneAPI/SceneCore/Containers/Views/FilterIterator.inl>