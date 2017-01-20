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

#include <utility>
#include <AzCore/std/utils.h>
#include <AzCore/std/typetraits/is_same.h>
#include <AzCore/std/typetraits/is_base_of.h>
#include <AzCore/std/iterator.h>
#include <SceneAPI/SceneCore/Containers/Views/View.h>
#include <SceneAPI/SceneCore/Containers/Utilities/ProxyPointer.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            namespace Views
            {
                // Merges two iterators together that have a pair relation.
                //      Both iterators must point to the same relative entry and must contain
                //      the same amount of remaining increments (and decrements if appropriate).
                //      If the categories of the given iterators differ the PairIterator
                //      will only support functionality shared between them. The categories
                //      must be implementations of or be related to the categories defined by
                //      the C++ standard.
                // Example:
                //      vector<string> names;
                //      vector<int> values;
                //      auto view = MakePairView(names.begin(), names.end(), values.begin(), values.end());
                //      for(auto it : view)
                //      {
                //          printf("%s has value %i\n", it.first.c_str(), it.second);
                //      }

                namespace Internal
                {
                    template<typename FirstIterator, typename SecondIterator>
                    struct PairIteratorCategory
                    {
                        static const bool s_sameCategory = AZStd::is_same<
                                typename AZStd::iterator_traits<FirstIterator>::iterator_category,
                                typename AZStd::iterator_traits<SecondIterator>::iterator_category>::value;

                        // True if both categories are the same.
                        // True if FirstIterator has the lower category in the hierarchy
                        // False if ValueItator has the lower category or is unrelated.
                        static const bool s_firstIteratorCategoryIsBaseOfSecondIterator = AZStd::is_base_of<
                                typename AZStd::iterator_traits<FirstIterator>::iterator_category,
                                typename AZStd::iterator_traits<SecondIterator>::iterator_category>::value;

                        // True if both categories are the same.
                        // True if SecondIterator has the lower category in the hierarchy
                        // False if FirstItator has the lower category or is unrelated.
                        static const bool s_SecondIteratorCategoryIsBaseOfFirstIterator = AZStd::is_base_of<
                                typename AZStd::iterator_traits<SecondIterator>::iterator_category,
                                typename AZStd::iterator_traits<FirstIterator>::iterator_category>::value;

                        static_assert(s_sameCategory || (s_firstIteratorCategoryIsBaseOfSecondIterator != s_SecondIteratorCategoryIsBaseOfFirstIterator),
                            "The iterator categories for the first and second in the PairIterator are unrelated categories.");

                        using Category = typename AZStd::conditional<s_firstIteratorCategoryIsBaseOfSecondIterator,
                                    typename AZStd::iterator_traits<FirstIterator>::iterator_category,
                                    typename AZStd::iterator_traits<SecondIterator>::iterator_category>::type;
                    };
                }

                template<typename FirstIterator, typename SecondIterator,
                    typename Category = typename Internal::PairIteratorCategory<FirstIterator, SecondIterator>::Category>
                class PairIterator
                {
                };

                //
                // Base iterator
                //
                template<typename FirstIterator, typename SecondIterator>
                class PairIterator<FirstIterator, SecondIterator, void>
                {
                public:
                    using value_type = AZStd::pair<typename AZStd::iterator_traits<FirstIterator>::value_type, typename AZStd::iterator_traits<SecondIterator>::value_type>;
                    using difference_type = AZStd::ptrdiff_t;
                    using reference = AZStd::pair<typename AZStd::iterator_traits<FirstIterator>::reference, typename AZStd::iterator_traits<SecondIterator>::reference>;
                    using pointer = ProxyPointer<reference>;
                    using iterator_category = typename Internal::PairIteratorCategory<FirstIterator, SecondIterator>::Category;

                    using RootIterator = PairIterator<FirstIterator, SecondIterator, iterator_category>;

                    PairIterator(FirstIterator First, SecondIterator second);
                    PairIterator(const PairIterator&) = default;

                    PairIterator& operator=(const PairIterator&) = default;

                    RootIterator& operator++();
                    RootIterator operator++(int);

                    const FirstIterator& GetFirstIterator();
                    const SecondIterator& GetSecondIterator();

                protected:
                    FirstIterator m_first;
                    SecondIterator m_second;
                };

                //
                // Input iterator
                //
                template<typename FirstIterator, typename SecondIterator>
                class PairIterator<FirstIterator, SecondIterator, AZStd::input_iterator_tag>
                    : public PairIterator<FirstIterator, SecondIterator, void>
                {
                public:
                    using super = PairIterator<FirstIterator, SecondIterator, void>;

                    PairIterator(FirstIterator first, SecondIterator second);
                    PairIterator(const PairIterator& rhs) = default;

                    bool operator==(const RootIterator& rhs) const;
                    bool operator!=(const RootIterator& rhs) const;

                    reference operator*();
                    pointer operator->();
                };

                //
                // Forward iterator
                //
                template<typename FirstIterator, typename SecondIterator>
                class PairIterator<FirstIterator, SecondIterator, AZStd::forward_iterator_tag>
                    : public PairIterator<FirstIterator, SecondIterator, AZStd::input_iterator_tag>
                {
                public:
                    using super = PairIterator<FirstIterator, SecondIterator, AZStd::input_iterator_tag>;

                    PairIterator(FirstIterator first, SecondIterator second);
                    PairIterator(const PairIterator& rhs) = default;
                    PairIterator();
                };

                //
                // Bidirectional iterator
                //
                template<typename FirstIterator, typename SecondIterator>
                class PairIterator<FirstIterator, SecondIterator, AZStd::bidirectional_iterator_tag>
                    : public PairIterator<FirstIterator, SecondIterator, AZStd::forward_iterator_tag>
                {
                public:
                    using super = PairIterator<FirstIterator, SecondIterator, AZStd::forward_iterator_tag>;

                    PairIterator(FirstIterator first, SecondIterator second);
                    PairIterator(const PairIterator& rhs) = default;
                    PairIterator();

                    RootIterator& operator--();
                    RootIterator operator--(int);
                };

                //
                // Random Access iterator
                //
                template<typename FirstIterator, typename SecondIterator>
                class PairIterator<FirstIterator, SecondIterator, AZStd::random_access_iterator_tag>
                    : public PairIterator<FirstIterator, SecondIterator, AZStd::bidirectional_iterator_tag>
                {
                public:
                    using super = PairIterator<FirstIterator, SecondIterator, AZStd::bidirectional_iterator_tag>;

                    PairIterator(FirstIterator first, SecondIterator second);
                    PairIterator(const PairIterator& rhs) = default;
                    PairIterator();

                    reference operator[](size_t index);

                    bool operator<(const RootIterator& rhs) const;
                    bool operator>(const RootIterator& rhs) const;
                    bool operator<=(const RootIterator& rhs) const;
                    bool operator>=(const RootIterator& rhs) const;

                    RootIterator operator+(size_t n) const;
                    RootIterator operator-(size_t n) const;
                    difference_type operator-(const RootIterator& rhs) const;
                    RootIterator& operator+=(size_t n);
                    RootIterator& operator-=(size_t n);
                };

                //
                // Continuous Random Access iterator
                //
                template<typename FirstIterator, typename SecondIterator>
                class PairIterator<FirstIterator, SecondIterator, AZStd::continuous_random_access_iterator_tag>
                    : public PairIterator<FirstIterator, SecondIterator, AZStd::random_access_iterator_tag>
                {
                public:
                    using super = PairIterator<FirstIterator, SecondIterator, AZStd::random_access_iterator_tag>;

                    PairIterator(FirstIterator first, SecondIterator second);
                    PairIterator(const PairIterator& rhs) = default;
                    PairIterator();
                };

                //
                // Utility functions
                //
                template<typename FirstIterator, typename SecondIterator>
                PairIterator<FirstIterator, SecondIterator> MakePairIterator(FirstIterator first, SecondIterator second);

                template<typename FirstIterator, typename SecondIterator>
                View<PairIterator<FirstIterator, SecondIterator> >
                MakePairView(FirstIterator firstBegin, FirstIterator firstEnd, SecondIterator secondBegin, SecondIterator secondEnd);

                template<typename FirstView, typename SecondView>
                View<PairIterator<typename FirstView::iterator, typename SecondView::iterator> >
                MakePairView(FirstView& firstView, SecondView& secondView);
            } // Views
        } // Containers
    } // SceneAPI
} // AZ

namespace AZStd
{
    // iterator swap
    template<typename First, typename Second>
    void iter_swap(AZ::SceneAPI::Containers::Views::PairIterator<First, Second> lhs, AZ::SceneAPI::Containers::Views::PairIterator<First, Second> rhs);
}

#include <SceneAPI/SceneCore/Containers/Views/PairIterator.inl>