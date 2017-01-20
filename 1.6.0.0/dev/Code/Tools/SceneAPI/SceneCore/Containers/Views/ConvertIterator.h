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

#include <type_traits> // For std::result_of
#include <AzCore/std/iterator.h>
#include <AzCore/std/typetraits/is_pointer.h>
#include <AzCore/std/typetraits/is_reference.h>
#include <AzCore/std/typetraits/remove_reference.h>
#include <AzCore/std/utils.h>
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
                // Transform allows the value of the given iterator to be converted to the assigned TransformType when dereferencing.
                // A typical use case would be to change a "const shared_ptr<type>" to "const shared<const type>" for read-only
                // iteration of a vector<shared_ptr<type>>.
                // template argument Iterator: the base iterator which will be used as the given iterator.
                // template argument TransformType: the target type the value of base iterator will be converted to.
                //                                  This requires the type stored in iterator to have conversion to this type.
                // template argument Category: the iterator classification. ConvertIterator will automatically derive this
                //                             from the given iterator and match its behavior.
                // Example:
                //      ConvertIterator<Vector<shared_ptr<Object>>::iterator, shared_ptr<const Object>>;
                //
                // WARNING
                //      Types used for conversion that are not convertible through a reference and pointer will return their result
                //      by-value instead of by-reference. This may cause some unexpected behavior the user should be aware of.

                template<typename Iterator>
                struct ConvertIteratorArgumentHelper
                {
                    static const bool s_argumentCondition = typename AZStd::is_pointer<typename AZStd::iterator_traits<Iterator>::value_type>::value;

                    using ArgumentType = typename AZStd::conditional<s_argumentCondition,
                                typename AZStd::iterator_traits<Iterator>::value_type,
                                typename AZStd::iterator_traits<Iterator>::reference>::type;
                };

                template<typename Iterator, typename Function>
                struct ConvertIteratorFunctionHelper
                {
                    using ArgumentType = typename ConvertIteratorArgumentHelper<Iterator>::ArgumentType;
                    // There's currently no AZStd equivalent for std::result_of
                    using ReturnType = typename std::result_of<Function(ArgumentType)>::type;
                };

                template<typename Iterator, typename ReturnType>
                struct ConvertIteratorTypeHelper
                {
                    using ArgumentType = typename ConvertIteratorArgumentHelper<Iterator>::ArgumentType;
                    using Function = ReturnType(*)(ArgumentType);

                    static const bool s_pointerCondition = typename AZStd::is_reference<ReturnType>::value;
                    using PointerConditionType = typename AZStd::is_reference<ReturnType>::type;

                    using reference = ReturnType;
                    using pointer = typename AZStd::conditional<s_pointerCondition,
                                typename AZStd::remove_reference<ReturnType>::type*, ProxyPointer<ReturnType> >::type;
                    using value_type = typename AZStd::remove_reference<ReturnType>::type;
                };


                template<typename Iterator, typename ReturnType, typename Category = typename AZStd::iterator_traits<Iterator>::iterator_category>
                class ConvertIterator
                {
                };

                //
                // Base iterator
                //
                template<typename Iterator, typename ReturnType>
                class ConvertIterator<Iterator, ReturnType, void>
                {
                public:
                    using value_type = typename ConvertIteratorTypeHelper<Iterator, ReturnType>::value_type;
                    using difference_type = typename AZStd::iterator_traits<Iterator>::difference_type;
                    using pointer = typename ConvertIteratorTypeHelper<Iterator, ReturnType>::pointer;
                    using reference = typename ConvertIteratorTypeHelper<Iterator, ReturnType>::reference;
                    using iterator_category = typename AZStd::iterator_traits<Iterator>::iterator_category;

                    using Function = typename ConvertIteratorTypeHelper<Iterator, ReturnType>::Function;

                    using RootIterator = ConvertIterator<Iterator, ReturnType, iterator_category>;

                    ConvertIterator(Iterator iterator, Function converter);
                    ConvertIterator(const ConvertIterator&) = default;

                    ConvertIterator& operator=(const ConvertIterator&) = default;

                    RootIterator& operator++();
                    RootIterator operator++(int);

                    const Iterator& GetBaseIterator();
                protected:
                    Iterator m_iterator;
                    Function m_converter;
                };

                //
                // input_iterator_tag
                //
                template<typename Iterator, typename ReturnType>
                class ConvertIterator<Iterator, ReturnType, AZStd::input_iterator_tag>
                    : public ConvertIterator<Iterator, ReturnType, void>
                {
                public:
                    using super = ConvertIterator<Iterator, ReturnType, void>;

                    ConvertIterator(Iterator iterator, Function converter);
                    ConvertIterator(const ConvertIterator&) = default;

                    reference operator*();
                    pointer operator->();

                    bool operator==(const RootIterator& rhs) const;
                    bool operator!=(const RootIterator& rhs) const;

                protected:
                    // Used for pointer casts that don't require an intermediate value.
                    pointer GetPointer(AZStd::true_type castablePointer);
                    // Used for pointer casts that require an intermediate proxy object.
                    pointer GetPointer(AZStd::false_type intermediateValue);
                };

                //
                // forward_iterator_tag
                //
                template<typename Iterator, typename ReturnType>
                class ConvertIterator<Iterator, ReturnType, AZStd::forward_iterator_tag>
                    : public ConvertIterator<Iterator, ReturnType, AZStd::input_iterator_tag>
                {
                public:
                    using super = ConvertIterator<Iterator, ReturnType, AZStd::input_iterator_tag>;

                    ConvertIterator(Iterator iterator, Function converter);
                    ConvertIterator(const ConvertIterator&) = default;
                    ConvertIterator();
                };

                //
                // bidirectional_iterator_tag
                //
                template<typename Iterator, typename ReturnType>
                class ConvertIterator<Iterator, ReturnType, AZStd::bidirectional_iterator_tag>
                    : public ConvertIterator<Iterator, ReturnType, AZStd::forward_iterator_tag>
                {
                public:
                    using super = ConvertIterator<Iterator, ReturnType, AZStd::forward_iterator_tag>;

                    ConvertIterator(Iterator iterator, Function converter);
                    ConvertIterator(const ConvertIterator&) = default;
                    ConvertIterator();

                    RootIterator& operator--();
                    RootIterator operator--(int);
                };

                //
                // random_access_iterator_tag
                //
                template<typename Iterator, typename ReturnType>
                class ConvertIterator<Iterator, ReturnType, AZStd::random_access_iterator_tag>
                    : public ConvertIterator<Iterator, ReturnType, AZStd::bidirectional_iterator_tag>
                {
                public:
                    using super = ConvertIterator<Iterator, ReturnType, AZStd::bidirectional_iterator_tag>;

                    ConvertIterator(Iterator iterator, Function converter);
                    ConvertIterator(const ConvertIterator&) = default;
                    ConvertIterator();

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

                // continuous_random_access_iterator_tag
                template<typename Iterator, typename ReturnType>
                class ConvertIterator<Iterator, ReturnType, AZStd::continuous_random_access_iterator_tag>
                    : public ConvertIterator<Iterator, ReturnType, AZStd::random_access_iterator_tag>
                {
                public:
                    using super = ConvertIterator<Iterator, ReturnType, AZStd::random_access_iterator_tag>;

                    ConvertIterator(Iterator iterator, Function converter);
                    ConvertIterator(const ConvertIterator&) = default;
                    ConvertIterator();
                };

                // Utility functions
                template<typename Iterator, typename Function>
                ConvertIterator<Iterator, typename ConvertIteratorFunctionHelper<Iterator, Function>::ReturnType>
                MakeConvertIterator(Iterator iterator, Function converter);

                template<typename Iterator, typename Function>
                View<ConvertIterator<Iterator, typename ConvertIteratorFunctionHelper<Iterator, Function>::ReturnType> >
                MakeConvertView(Iterator begin, Iterator end, Function converter);

                template<typename ViewType, typename Function>
                View<ConvertIterator<typename ViewType::iterator, typename ConvertIteratorFunctionHelper<typename ViewType::iterator, Function>::ReturnType> >
                MakeConvertView(ViewType& view, Function converter);
            } // Views
        } // Containers
    } // SceneAPI
} // AZ

#include <SceneAPI/SceneCore/Containers/Views/ConvertIterator.inl>