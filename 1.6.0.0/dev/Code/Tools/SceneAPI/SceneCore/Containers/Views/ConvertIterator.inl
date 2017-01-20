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
                template<typename Iterator, typename ReturnType>
                ConvertIterator<Iterator, ReturnType, void>::ConvertIterator(Iterator iterator, Function converter)
                    : m_iterator(iterator)
                    , m_converter(converter)
                {
                }

                template<typename Iterator, typename ReturnType>
                auto ConvertIterator<Iterator, ReturnType, void>::operator++()->RootIterator &
                {
                    ++m_iterator;
                    return *static_cast<RootIterator*>(this);
                }

                template<typename Iterator, typename ReturnType>
                auto ConvertIterator<Iterator, ReturnType, void>::operator++(int)->RootIterator
                {
                    RootIterator result = *static_cast<RootIterator*>(this);
                    ++m_iterator;
                    return result;
                }

                template<typename Iterator, typename ReturnType>
                const Iterator& ConvertIterator<Iterator, ReturnType, void>::GetBaseIterator()
                {
                    return m_iterator;
                }

                //
                // input_iterator_tag
                //
                template<typename Iterator, typename ReturnType>
                ConvertIterator<Iterator, ReturnType, AZStd::input_iterator_tag>::ConvertIterator(Iterator iterator, Function converter)
                    : super(iterator, converter)
                {
                }

                template<typename Iterator, typename ReturnType>
                auto ConvertIterator<Iterator, ReturnType, AZStd::input_iterator_tag>::operator*()->reference
                {
                    AZ_Assert(m_converter, "No valid conversion function set for ConvertIterator.");
                    return m_converter(*m_iterator);
                }

                template<typename Iterator, typename ReturnType>
                auto ConvertIterator<Iterator, ReturnType, AZStd::input_iterator_tag>::GetPointer(AZStd::true_type castablePointer)->pointer
                {
                    AZ_Assert(m_converter, "No valid conversion function set for ConvertIterator.");
                    return &(m_converter(*m_iterator));
                }

                template<typename Iterator, typename ReturnType>
                auto ConvertIterator<Iterator, ReturnType, AZStd::input_iterator_tag>::GetPointer(AZStd::false_type intermediateValue)->pointer
                {
                    AZ_Assert(m_converter, "No valid conversion function set for ConvertIterator.");
                    return pointer(m_converter(*m_iterator));
                }

                template<typename Iterator, typename ReturnType>
                auto ConvertIterator<Iterator, ReturnType, AZStd::input_iterator_tag>::operator->()->pointer
                {
                    return GetPointer(ConvertIteratorTypeHelper<Iterator, ReturnType>::PointerConditionType());
                }

                template<typename Iterator, typename ReturnType>
                bool ConvertIterator<Iterator, ReturnType, AZStd::input_iterator_tag>::operator==(const RootIterator& rhs) const
                {
                    return m_iterator == rhs.m_iterator;
                }
                template<typename Iterator, typename ReturnType>
                bool ConvertIterator<Iterator, ReturnType, AZStd::input_iterator_tag>::operator!=(const RootIterator& rhs) const
                {
                    return m_iterator != rhs.m_iterator;
                }

                //
                // forward_iterator_tag
                //
                template<typename Iterator, typename ReturnType>
                ConvertIterator<Iterator, ReturnType, AZStd::forward_iterator_tag>::ConvertIterator(Iterator iterator, Function converter)
                    : super(iterator, converter)
                {
                }

                template<typename Iterator, typename ReturnType>
                ConvertIterator<Iterator, ReturnType, AZStd::forward_iterator_tag>::ConvertIterator()
                    : super(Iterator(), nullptr)
                {
                }

                //
                // bidirectional_iterator_tag
                //
                template<typename Iterator, typename ReturnType>
                ConvertIterator<Iterator, ReturnType, AZStd::bidirectional_iterator_tag>::ConvertIterator(Iterator iterator, Function converter)
                    : super(iterator, converter)
                {
                }

                template<typename Iterator, typename ReturnType>
                ConvertIterator<Iterator, ReturnType, AZStd::bidirectional_iterator_tag>::ConvertIterator()
                    : super()
                {
                }

                template<typename Iterator, typename ReturnType>
                auto ConvertIterator<Iterator, ReturnType, AZStd::bidirectional_iterator_tag>::operator--()->RootIterator &
                {
                    --m_iterator;
                    return *static_cast<RootIterator*>(this);
                }

                template<typename Iterator, typename ReturnType>
                auto ConvertIterator<Iterator, ReturnType, AZStd::bidirectional_iterator_tag>::operator--(int)->RootIterator
                {
                    RootIterator result = *static_cast<RootIterator*>(this);
                    --m_iterator;
                    return result;
                }

                //
                // random_access_iterator_tag
                //
                template<typename Iterator, typename ReturnType>
                ConvertIterator<Iterator, ReturnType, AZStd::random_access_iterator_tag>::ConvertIterator(Iterator iterator, Function converter)
                    : super(iterator, converter)
                {
                }

                template<typename Iterator, typename ReturnType>
                ConvertIterator<Iterator, ReturnType, AZStd::random_access_iterator_tag>::ConvertIterator()
                    : super()
                {
                }

                template<typename Iterator, typename ReturnType>
                auto ConvertIterator<Iterator, ReturnType, AZStd::random_access_iterator_tag>::operator[](size_t index)->reference
                {
                    AZ_Assert(m_converter, "No valid conversion function set for ConvertIterator.");
                    return m_converter(m_iterator[index]);
                }

                template<typename Iterator, typename ReturnType>
                bool ConvertIterator<Iterator, ReturnType, AZStd::random_access_iterator_tag>::operator<(const RootIterator& rhs) const
                {
                    return m_iterator < rhs.m_iterator;
                }
                template<typename Iterator, typename ReturnType>
                bool ConvertIterator<Iterator, ReturnType, AZStd::random_access_iterator_tag>::operator>(const RootIterator& rhs) const
                {
                    return m_iterator > rhs.m_iterator;
                }
                template<typename Iterator, typename ReturnType>
                bool ConvertIterator<Iterator, ReturnType, AZStd::random_access_iterator_tag>::operator<=(const RootIterator& rhs) const
                {
                    return m_iterator <= rhs.m_iterator;
                }
                template<typename Iterator, typename ReturnType>
                bool ConvertIterator<Iterator, ReturnType, AZStd::random_access_iterator_tag>::operator>=(const RootIterator& rhs) const
                {
                    return m_iterator >= rhs.m_iterator;
                }

                template<typename Iterator, typename ReturnType>
                auto ConvertIterator<Iterator, ReturnType, AZStd::random_access_iterator_tag>::operator+(size_t n) const->RootIterator
                {
                    RootIterator result = *static_cast<const RootIterator*>(this);
                    result.m_iterator += n;
                    return result;
                }

                template<typename Iterator, typename ReturnType>
                auto ConvertIterator<Iterator, ReturnType, AZStd::random_access_iterator_tag>::operator-(size_t n) const->RootIterator
                {
                    RootIterator result = *static_cast<const RootIterator*>(this);
                    result.m_iterator -= n;
                    return result;
                }

                template<typename Iterator, typename ReturnType>
                auto ConvertIterator<Iterator, ReturnType, AZStd::random_access_iterator_tag>::operator-(const RootIterator& rhs) const->difference_type
                {
                    return m_iterator - rhs.m_iterator;
                }

                template<typename Iterator, typename ReturnType>
                auto ConvertIterator<Iterator, ReturnType, AZStd::random_access_iterator_tag>::operator+=(size_t n)->RootIterator &
                {
                    m_iterator += n;
                    return *static_cast<RootIterator*>(this);
                }

                template<typename Iterator, typename ReturnType>
                auto ConvertIterator<Iterator, ReturnType, AZStd::random_access_iterator_tag>::operator-=(size_t n)->RootIterator &
                {
                    m_iterator -= n;
                    return *static_cast<RootIterator*>(this);
                }

                //
                // continuous_random_access_iterator_tag
                //
                template<typename Iterator, typename ReturnType>
                ConvertIterator<Iterator, ReturnType, AZStd::continuous_random_access_iterator_tag>::ConvertIterator(Iterator iterator, Function converter)
                    : super(iterator, converter)
                {
                }

                template<typename Iterator, typename ReturnType>
                ConvertIterator<Iterator, ReturnType, AZStd::continuous_random_access_iterator_tag>::ConvertIterator()
                    : super()
                {
                }

                // Utility functions
                template<typename Iterator, typename Function>
                ConvertIterator<Iterator, typename ConvertIteratorFunctionHelper<Iterator, Function>::ReturnType>
                MakeConvertIterator(Iterator iterator, Function converter)
                {
                    using ReturnType = typename ConvertIteratorFunctionHelper<Iterator, Function>::ReturnType;
                    return ConvertIterator<Iterator, ReturnType>(iterator, converter);
                }

                template<typename Iterator, typename Function>
                View<ConvertIterator<Iterator, typename ConvertIteratorFunctionHelper<Iterator, Function>::ReturnType> >
                MakeConvertView(Iterator begin, Iterator end, Function converter)
                {
                    using ReturnType = typename ConvertIteratorFunctionHelper<Iterator, Function>::ReturnType;
                    return View<ConvertIterator<Iterator, ReturnType> >(
                        MakeConvertIterator(begin, converter),
                        MakeConvertIterator(end, converter));
                }

                template<typename ViewType, typename Function>
                View<ConvertIterator<typename ViewType::iterator, typename ConvertIteratorFunctionHelper<typename ViewType::iterator, Function>::ReturnType> >
                MakeConvertView(ViewType& view, Function converter)
                {
                    return MakeConvertView(view.begin(), view.end(), converter);
                }
            } // Views
        } // Containers
    } // SceneAPI
} // AZ