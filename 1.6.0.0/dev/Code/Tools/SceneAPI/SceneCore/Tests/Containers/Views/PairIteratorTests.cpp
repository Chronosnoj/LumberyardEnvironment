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

#define _SCL_SECURE_NO_WARNINGS

#include <AzTest/AzTest.h>
#include <algorithm>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/containers/list.h>
#include <AzCore/std/typetraits/is_same.h>
#include <AzCore/std/iterator.h>
#include <AzCore/std/sort.h>
#include <SceneAPI/SceneCore/Containers/Views/PairIterator.h>
#include <SceneAPI/SceneCore/Tests/Containers/Views/IteratorTestsBase.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            namespace Views
            {
                namespace Internal
                {
                    TEST(PairIteratorCategoryTests, Declaration_SameCategory_TwoIteratorsHaveEqualCategory)
                    {
                        using Iterator = AZStd::vector<int>::iterator;
                        using CategoryInfo = PairIteratorCategory<Iterator, Iterator>;

                        EXPECT_TRUE(CategoryInfo::s_sameCategory);
                        EXPECT_TRUE(CategoryInfo::s_firstIteratorCategoryIsBaseOfSecondIterator);
                        EXPECT_TRUE(CategoryInfo::s_SecondIteratorCategoryIsBaseOfFirstIterator);
                        bool isSameCategory = AZStd::is_same<AZStd::iterator_traits<Iterator>::iterator_category, CategoryInfo::Category>::value;
                        EXPECT_TRUE(isSameCategory);
                    }

                    TEST(PairIteratorCategoryTests, Declaration_DifferentCategoryWithFirstHighest_NotTheSameCategoryAndPicksLowestCategory)
                    {
                        using IteratorHigh = AZStd::vector<int>::iterator;
                        using IteratorLow = AZStd::list<int>::iterator;
                        using CategoryInfo = PairIteratorCategory<IteratorHigh, IteratorLow>;

                        EXPECT_FALSE(CategoryInfo::s_sameCategory);
                        EXPECT_FALSE(CategoryInfo::s_firstIteratorCategoryIsBaseOfSecondIterator);
                        EXPECT_TRUE(CategoryInfo::s_SecondIteratorCategoryIsBaseOfFirstIterator);
                        bool isSameCategory = AZStd::is_same<AZStd::iterator_traits<IteratorLow>::iterator_category, CategoryInfo::Category>::value;
                        EXPECT_TRUE(isSameCategory);
                    }

                    TEST(PairIteratorCategoryTests, Declaration_DifferentCategoryWithFirstLowest_NotTheSameCategoryAndPicksLowestCategory)
                    {
                        using IteratorHigh = AZStd::vector<int>::iterator;
                        using IteratorLow = AZStd::list<int>::iterator;
                        using CategoryInfo = PairIteratorCategory<IteratorLow, IteratorHigh>;

                        EXPECT_FALSE(CategoryInfo::s_sameCategory);
                        EXPECT_TRUE(CategoryInfo::s_firstIteratorCategoryIsBaseOfSecondIterator);
                        EXPECT_FALSE(CategoryInfo::s_SecondIteratorCategoryIsBaseOfFirstIterator);
                        bool isSameCategory = AZStd::is_same<AZStd::iterator_traits<IteratorLow>::iterator_category, CategoryInfo::Category>::value;
                        EXPECT_TRUE(isSameCategory);
                    }
                }

                template<typename CollectionType>
                class PairIteratorTests
                    : public IteratorTypedTestsBase<CollectionType>
                {
                public:
                    void AddElementPair(int first, int second)
                    {
                        AddElement(m_firstContainer, first);
                        AddElement(m_secondContainer, second);
                    }

                    PairIteratorTests()
                    {
                        AddElementPair(42, 88);
                        AddElementPair(142, 188);
                    }
                    ~PairIteratorTests() override = default;

                    void Clear()
                    {
                        m_firstContainer.clear();
                        m_secondContainer.clear();
                    }

                    CollectionType m_firstContainer;
                    CollectionType m_secondContainer;
                };

                TYPED_TEST_CASE_P(PairIteratorTests);

                TYPED_TEST_P(PairIteratorTests, MakePairIterator_BuildFromTwoSeparateIterators_StoredIteratorsMatchTheGivenIterators)
                {
                    auto iterator = MakePairIterator(m_firstContainer.begin(), m_secondContainer.begin());
                    EXPECT_EQ(iterator.GetFirstIterator(), m_firstContainer.begin());
                    EXPECT_EQ(iterator.GetSecondIterator(), m_secondContainer.begin());
                }

                TYPED_TEST_P(PairIteratorTests, MakePairIterator_BuildFromTwoSeparateIterators_FirstAndSecondInContainersCanBeAccessedThroughIterator)
                {
                    auto iterator = MakePairIterator(m_firstContainer.begin(), m_secondContainer.begin());
                    EXPECT_EQ(iterator->first, *m_firstContainer.begin());
                    EXPECT_EQ(iterator->second, *m_secondContainer.begin());
                }

                TYPED_TEST_P(PairIteratorTests, MakePairView_CreateFromIterators_IteratorsInViewMatchExplicitlyCreatedIterators)
                {
                    auto begin = MakePairIterator(m_firstContainer.begin(), m_secondContainer.begin());
                    auto end = MakePairIterator(m_firstContainer.end(), m_secondContainer.end());

                    auto view = MakePairView(m_firstContainer.begin(), m_firstContainer.end(), m_secondContainer.begin(), m_secondContainer.end());
                    EXPECT_EQ(view.begin(), begin);
                    EXPECT_EQ(view.end(), end);
                }

                TYPED_TEST_P(PairIteratorTests, MakePairView_CreateFromViews_IteratorsInViewMatchExplicitlyCreatedIterators)
                {
                    auto firstView = MakeView(m_firstContainer.begin(), m_firstContainer.end());
                    auto secondView = MakeView(m_secondContainer.begin(), m_secondContainer.end());

                    auto begin = MakePairIterator(m_firstContainer.begin(), m_secondContainer.begin());
                    auto end = MakePairIterator(m_firstContainer.end(), m_secondContainer.end());

                    auto view = MakePairView(firstView, secondView);
                    EXPECT_EQ(view.begin(), begin);
                    EXPECT_EQ(view.end(), end);
                }

                TYPED_TEST_P(PairIteratorTests, OperatorStar_DereferencingChangesFirst_FirstChangeIsPassedToContainer)
                {
                    auto iterator = MakePairIterator(m_firstContainer.begin(), m_secondContainer.begin());
                    (*iterator).first = 4;

                    EXPECT_EQ(4, *m_firstContainer.begin());
                }

                TYPED_TEST_P(PairIteratorTests, OperatorStar_DereferencingChangesSecond_SecondsChangeIsPassedToContainer)
                {
                    auto iterator = MakePairIterator(m_firstContainer.begin(), m_secondContainer.begin());
                    (*iterator).second = 4;

                    EXPECT_EQ(4, *m_secondContainer.begin());
                }


                TYPED_TEST_P(PairIteratorTests, OperatorArrow_DereferencingChangesFirst_FirstChangeIsPassedToContainer)
                {
                    auto iterator = MakePairIterator(m_firstContainer.begin(), m_secondContainer.begin());
                    iterator->first = 4;

                    EXPECT_EQ(4, *m_firstContainer.begin());
                }

                TYPED_TEST_P(PairIteratorTests, OperatorArrow_DereferencingChangesSecond_SecondsChangeIsPassedToContainer)
                {
                    auto iterator = MakePairIterator(m_firstContainer.begin(), m_secondContainer.begin());
                    iterator->second = 4;

                    EXPECT_EQ(4, *m_secondContainer.begin());
                }

                TYPED_TEST_P(PairIteratorTests, PreIncrementOperator_IncrementingMovesBothIterators_BothStoredIteratorsMoved)
                {
                    auto iterator = MakePairIterator(m_firstContainer.begin(), m_secondContainer.begin());
                    ++iterator;

                    auto cmpFirst = m_firstContainer.begin();
                    auto cmpSecond = m_secondContainer.begin();
                    ++cmpFirst;
                    ++cmpSecond;

                    EXPECT_EQ(iterator.GetFirstIterator(), cmpFirst);
                    EXPECT_EQ(iterator.GetSecondIterator(), cmpSecond);
                }

                TYPED_TEST_P(PairIteratorTests, PostIncrementOperator_IncrementingMovesBothIterators_BothStoredIteratorsMoved)
                {
                    auto iterator = MakePairIterator(m_firstContainer.begin(), m_secondContainer.begin());
                    iterator++;

                    auto cmpFirst = m_firstContainer.begin();
                    auto cmpSecond = m_secondContainer.begin();
                    ++cmpFirst;
                    ++cmpSecond;

                    EXPECT_EQ(iterator.GetFirstIterator(), cmpFirst);
                    EXPECT_EQ(iterator.GetSecondIterator(), cmpSecond);
                }

                TYPED_TEST_P(PairIteratorTests, Algorithms_Generate_FirstContainerFilledWithTheFirstAndSecondContainerFilledWithSecondInGivenPair)
                {
                    Clear();
                    m_firstContainer.resize(10);
                    m_secondContainer.resize(10);

                    auto view = MakePairView(m_firstContainer.begin(), m_firstContainer.end(), m_secondContainer.begin(), m_secondContainer.end());
                    AZStd::generate(view.begin(), view.end(),
                        []() -> AZStd::pair<int, int>
                    {
                        return AZStd::make_pair(3, 9);
                    });

                    for (auto it : m_firstContainer)
                    {
                        EXPECT_EQ(3, it);
                    }
                    for (auto it : m_secondContainer)
                    {
                        EXPECT_EQ(9, it);
                    }
                }

                REGISTER_TYPED_TEST_CASE_P(PairIteratorTests,
                    MakePairIterator_BuildFromTwoSeparateIterators_StoredIteratorsMatchTheGivenIterators,
                    MakePairIterator_BuildFromTwoSeparateIterators_FirstAndSecondInContainersCanBeAccessedThroughIterator,
                    MakePairView_CreateFromIterators_IteratorsInViewMatchExplicitlyCreatedIterators,
                    MakePairView_CreateFromViews_IteratorsInViewMatchExplicitlyCreatedIterators,
                    OperatorStar_DereferencingChangesFirst_FirstChangeIsPassedToContainer,
                    OperatorStar_DereferencingChangesSecond_SecondsChangeIsPassedToContainer,
                    OperatorArrow_DereferencingChangesFirst_FirstChangeIsPassedToContainer,
                    OperatorArrow_DereferencingChangesSecond_SecondsChangeIsPassedToContainer,
                    PreIncrementOperator_IncrementingMovesBothIterators_BothStoredIteratorsMoved,
                    PostIncrementOperator_IncrementingMovesBothIterators_BothStoredIteratorsMoved,
                    Algorithms_Generate_FirstContainerFilledWithTheFirstAndSecondContainerFilledWithSecondInGivenPair);

                INSTANTIATE_TYPED_TEST_CASE_P(CommonTests, PairIteratorTests, BasicCollectionTypes);

                // The following tests are done as standalone tests as not all iterators support this functionality
                TEST(PairIteratorTest, PreDecrementIterator_DecrementingMovesBothIterators_BothStoredIteratorsMoved)
                {
                    AZStd::vector<int> firstContainer = { 42, 142 };
                    AZStd::vector<int> secondContainer = { 88, 188 };

                    auto iterator = MakePairIterator(firstContainer.begin(), secondContainer.begin());
                    ++iterator;
                    --iterator;

                    EXPECT_EQ(iterator.GetFirstIterator(), firstContainer.begin());
                    EXPECT_EQ(iterator.GetSecondIterator(), secondContainer.begin());
                }

                TEST(PairIteratorTest, PostDecrementIterator_DecrementingMovesBothIterators_BothStoredIteratorsMoved)
                {
                    AZStd::vector<int> firstContainer = { 42, 142 };
                    AZStd::vector<int> secondContainer = { 88, 188 };

                    auto iterator = MakePairIterator(firstContainer.begin(), secondContainer.begin());
                    ++iterator;
                    iterator--;

                    EXPECT_EQ(iterator.GetFirstIterator(), firstContainer.begin());
                    EXPECT_EQ(iterator.GetSecondIterator(), secondContainer.begin());
                }

                TEST(PairIteratorTest, Algorithms_Sort_BothListSortedByFirstThenSecondAndPairsNotBroken)
                {
                    AZStd::vector<int> firstContainer = { 105, 106, 101, 104, 103, 108 };
                    AZStd::vector<int> secondContainer = { 205, 206, 201, 204, 203, 208 };

                    auto view = MakePairView(firstContainer.begin(), firstContainer.end(), secondContainer.begin(), secondContainer.end());
                    AZStd::sort(view.begin(), view.end());

                    EXPECT_EQ((*view.begin()).first + 100, (*view.begin()).second);
                    for (auto it = view.begin() + 1; it != view.end(); ++it)
                    {
                        auto previousIt = it - 1;
                        EXPECT_LT((*previousIt).first, (*it).first);
                        EXPECT_EQ((*it).first + 100, (*it).second);
                    }
                }

                TEST(PairIteratorTest, Algorithms_Reverse_SecondssAreInDescendingOrder)
                {
                    AZStd::vector<int> firstContainer = { 1, 2, 3, 4, 5 };
                    AZStd::vector<int> secondContainer = { 1, 2, 3, 4, 5 };

                    auto view = MakePairView(firstContainer.begin(), firstContainer.end(), secondContainer.begin(), secondContainer.end());
                    AZStd::reverse(view.begin(), view.end());

                    for (auto it = view.begin() + 1; it != view.end(); ++it)
                    {
                        auto previousIt = it - 1;
                        EXPECT_GT(*previousIt, *it);
                    }
                }
            } // Views
        } // Containers
    } // SceneAPI
} // AZ

#undef _SCL_SECURE_NO_WARNINGS