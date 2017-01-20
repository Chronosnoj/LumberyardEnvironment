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

#include <AzCore/Debug/Trace.h>
#include <AzCore/std/typetraits/remove_pointer.h>
#include <AzCore/std/typetraits/remove_reference.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            namespace Views
            {
                //
                // Base iterator
                //
                template<typename FirstIterator, typename SecondIterator>
                PairIterator<FirstIterator, SecondIterator, void>::PairIterator(FirstIterator first, SecondIterator second)
                    : m_first(first)
                    , m_second(second)
                {
                }

                template<typename FirstIterator, typename SecondIterator>
                auto PairIterator<FirstIterator, SecondIterator, void>::operator++()->RootIterator &
                {
                    ++m_first;
                    ++m_second;
                    return *static_cast<RootIterator*>(this);
                }

                template<typename FirstIterator, typename SecondIterator>
                auto PairIterator<FirstIterator, SecondIterator, void>::operator++(int)->RootIterator
                {
                    RootIterator result = *static_cast<RootIterator*>(this);
                    ++m_first;
                    ++m_second;
                    return result;
                }

                template<typename FirstIterator, typename SecondIterator>
                auto PairIterator<FirstIterator, SecondIterator, void>::GetFirstIterator()->const FirstIterator&
                {
                    return m_first;
                }

                template<typename FirstIterator, typename SecondIterator>
                auto PairIterator<FirstIterator, SecondIterator, void>::GetSecondIterator()->const SecondIterator&
                {
                    return m_second;
                }

                //
                // Input iterator
                //
                template<typename FirstIterator, typename SecondIterator>
                PairIterator<FirstIterator, SecondIterator, AZStd::input_iterator_tag>::PairIterator(FirstIterator first, SecondIterator second)
                    : super(first, second)
                {
                }

                template<typename FirstIterator, typename SecondIterator>
                bool PairIterator<FirstIterator, SecondIterator, AZStd::input_iterator_tag>::operator==(const RootIterator& rhs) const
                {
                    return m_first == rhs.m_first && m_second == rhs.m_second;
                }

                template<typename FirstIterator, typename SecondIterator>
                bool PairIterator<FirstIterator, SecondIterator, AZStd::input_iterator_tag>::operator!=(const RootIterator& rhs) const
                {
                    return m_first != rhs.m_first || m_second != rhs.m_second;
                }

                template<typename FirstIterator, typename SecondIterator>
                auto PairIterator<FirstIterator, SecondIterator, AZStd::input_iterator_tag>::operator*()->reference
                {
                    return reference(*m_first, *m_second);
                }

                template<typename FirstIterator, typename SecondIterator>
                auto PairIterator<FirstIterator, SecondIterator, AZStd::input_iterator_tag>::operator->()->pointer
                {
                    return pointer(operator*());
                }

                //
                // Forward iterator
                //
                template<typename FirstIterator, typename SecondIterator>
                PairIterator<FirstIterator, SecondIterator, AZStd::forward_iterator_tag>::PairIterator(FirstIterator first, SecondIterator second)
                    : super(first, second)
                {
                }

                template<typename FirstIterator, typename SecondIterator>
                PairIterator<FirstIterator, SecondIterator, AZStd::forward_iterator_tag>::PairIterator()
                    : super(FirstIterator(), SecondIterator())
                {
                }

                //
                // Bidirectional iterator
                //
                template<typename FirstIterator, typename SecondIterator>
                PairIterator<FirstIterator, SecondIterator, AZStd::bidirectional_iterator_tag>::PairIterator(FirstIterator first, SecondIterator second)
                    : super(first, second)
                {
                }

                template<typename FirstIterator, typename SecondIterator>
                PairIterator<FirstIterator, SecondIterator, AZStd::bidirectional_iterator_tag>::PairIterator()
                    : super()
                {
                }

                template<typename FirstIterator, typename SecondIterator>
                auto PairIterator<FirstIterator, SecondIterator, AZStd::bidirectional_iterator_tag>::operator--()->RootIterator &
                {
                    --m_first;
                    --m_second;
                    return *static_cast<RootIterator*>(this);
                }

                template<typename FirstIterator, typename SecondIterator>
                auto PairIterator<FirstIterator, SecondIterator, AZStd::bidirectional_iterator_tag>::operator--(int)->RootIterator
                {
                    RootIterator result = *static_cast<RootIterator*>(this);
                    --m_first;
                    --m_second;
                    return result;
                }

                //
                // Random Access iterator
                //
                template<typename FirstIterator, typename SecondIterator>
                PairIterator<FirstIterator, SecondIterator, AZStd::random_access_iterator_tag>::PairIterator(FirstIterator first, SecondIterator second)
                    : super(first, second)
                {
                }

                template<typename FirstIterator, typename SecondIterator>
                PairIterator<FirstIterator, SecondIterator, AZStd::random_access_iterator_tag>::PairIterator()
                    : super()
                {
                }

                template<typename FirstIterator, typename SecondIterator>
                auto PairIterator<FirstIterator, SecondIterator, AZStd::random_access_iterator_tag>::operator[](size_t index)->reference
                {
                    return reference(m_first[index], m_second[index]);
                }

                template<typename FirstIterator, typename SecondIterator>
                bool IsSmaller(const FirstIterator& lhsFirst, const FirstIterator& lhsSecond, const SecondIterator& rhsFirst, const SecondIterator& rhsSecond)
                {
                    return (lhsFirst < rhsFirst || (!(rhsFirst < lhsFirst) && lhsSecond < rhsSecond));
                }

                template<typename FirstIterator, typename SecondIterator>
                bool PairIterator<FirstIterator, SecondIterator, AZStd::random_access_iterator_tag>::operator<(const RootIterator& rhs) const
                {
                    return IsSmaller(m_first, m_second, rhs.m_first, rhs.m_second);
                }

                template<typename FirstIterator, typename SecondIterator>
                bool PairIterator<FirstIterator, SecondIterator, AZStd::random_access_iterator_tag>::operator>(const RootIterator& rhs) const
                {
                    return IsSmaller(rhs.m_first, rhs.m_second, m_first, m_second);
                }

                template<typename FirstIterator, typename SecondIterator>
                bool PairIterator<FirstIterator, SecondIterator, AZStd::random_access_iterator_tag>::operator<=(const RootIterator& rhs) const
                {
                    return !IsSmaller(rhs.m_first, rhs.m_second, m_first, m_second);
                }

                template<typename FirstIterator, typename SecondIterator>
                bool PairIterator<FirstIterator, SecondIterator, AZStd::random_access_iterator_tag>::operator>=(const RootIterator& rhs) const
                {
                    return !IsSmaller(m_first, m_second, rhs.m_first, rhs.m_second);
                }

                template<typename FirstIterator, typename SecondIterator>
                auto PairIterator<FirstIterator, SecondIterator, AZStd::random_access_iterator_tag>::operator+(size_t n) const->RootIterator
                {
                    RootIterator result = *static_cast<const RootIterator*>(this);
                    result.m_first += n;
                    result.m_second += n;
                    return result;
                }

                template<typename FirstIterator, typename SecondIterator>
                auto PairIterator<FirstIterator, SecondIterator, AZStd::random_access_iterator_tag>::operator-(size_t n) const->RootIterator
                {
                    RootIterator result = *static_cast<const RootIterator*>(this);
                    result.m_first -= n;
                    result.m_second -= n;
                    return result;
                }

                template<typename FirstIterator, typename SecondIterator>
                auto PairIterator<FirstIterator, SecondIterator, AZStd::random_access_iterator_tag>::operator-(const RootIterator& rhs) const->difference_type
                {
                    difference_type result = m_first - rhs.m_first;
                    AZ_Assert((m_second - rhs.m_second) == result, "First and second in PairIterator have gone out of lockstep.");
                    return result;
                }

                template<typename FirstIterator, typename SecondIterator>
                auto PairIterator<FirstIterator, SecondIterator, AZStd::random_access_iterator_tag>::operator+=(size_t n)->RootIterator &
                {
                    m_first += n;
                    m_second += n;
                    return *static_cast<RootIterator*>(this);
                }

                template<typename FirstIterator, typename SecondIterator>
                auto PairIterator<FirstIterator, SecondIterator, AZStd::random_access_iterator_tag>::operator-=(size_t n)->RootIterator &
                {
                    m_first -= n;
                    m_second -= n;
                    return *static_cast<RootIterator*>(this);
                }

                //
                // Continuous Random Access iterator
                //
                template<typename FirstIterator, typename SecondIterator>
                PairIterator<FirstIterator, SecondIterator, AZStd::continuous_random_access_iterator_tag>::PairIterator(
                    FirstIterator first, SecondIterator second)
                    : super(first, second)
                {
                }

                template<typename FirstIterator, typename SecondIterator>
                PairIterator<FirstIterator, SecondIterator, AZStd::continuous_random_access_iterator_tag>::PairIterator()
                    : super()
                {
                }

                //
                // Utility functions
                //
                template<typename FirstIterator, typename SecondIterator>
                PairIterator<FirstIterator, SecondIterator> MakePairIterator(FirstIterator first, SecondIterator second)
                {
                    return PairIterator<FirstIterator, SecondIterator>(first, second);
                }

                template<typename FirstIterator, typename SecondIterator>
                View<PairIterator<FirstIterator, SecondIterator> >
                MakePairView(FirstIterator firstBegin, FirstIterator firstEnd, SecondIterator secondBegin, SecondIterator secondEnd)
                {
                    return View<PairIterator<FirstIterator, SecondIterator> >(
                        PairIterator<FirstIterator, SecondIterator>(firstBegin, secondBegin),
                        PairIterator<FirstIterator, SecondIterator>(firstEnd, secondEnd));
                }

                template<typename FirstView, typename SecondView>
                View<PairIterator<typename FirstView::iterator, typename SecondView::iterator> >
                MakePairView(FirstView& firstView, SecondView& secondView)
                {
                    return MakePairView(firstView.begin(), firstView.end(), secondView.begin(), secondView.end());
                }
            } // Views
        } // Containers
    } // SceneAPI
} // AZ

namespace AZStd
{
    // iterator swap
    template<typename First, typename Second>
    void iter_swap(AZ::SceneAPI::Containers::Views::PairIterator<First, Second> lhs, AZ::SceneAPI::Containers::Views::PairIterator<First, Second> rhs)
    {
        remove_pointer<remove_reference<First>::type>::type tmpFirst = (*lhs).first;
        remove_pointer<remove_reference<Second>::type>::type tmpSecond = (*lhs).second;

        (*lhs).first = (*rhs).first;
        (*lhs).second = (*rhs).second;

        (*rhs).first = tmpFirst;
        (*rhs).second = tmpSecond;
    }
}