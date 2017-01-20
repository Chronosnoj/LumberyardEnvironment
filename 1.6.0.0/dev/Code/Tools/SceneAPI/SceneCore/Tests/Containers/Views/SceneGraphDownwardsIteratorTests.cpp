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

/*
* This suite of tests focus on the unique features the SceneGraphDownwardsIterator
* adds as an iterator. The basic functionality and iterator conformity is tested
* in the Iterator Conformity Tests (see IteratorConformityTests.cpp).
*/

#include <AzTest/AzTest.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <SceneAPI/SceneCore/Containers/SceneGraph.h>
#include <SceneAPI/SceneCore/DataTypes/IGraphObject.h>
#include <SceneAPI/SceneCore/Containers/Views/SceneGraphDownwardsIterator.h>
#include <SceneAPI/SceneCore/Mocks/DataTypes/MockIGraphObject.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            namespace Views
            {
                class SceneGraphDownwardsIteratorTest
                    : public ::testing::Test
                {
                public:
                    virtual ~SceneGraphDownwardsIteratorTest() = default;

                    void SetUp()
                    {
                        /*---------------------------------------\
                        |      Root                              |
                        |       |                                |
                        |       A                                |
                        |     / | \                              |
                        |    B  C  D                             |
                        |      / \                               |
                        |     E   F                              |
                        \---------------------------------------*/

                        //Build up the graph
                        SceneGraph::NodeIndex root = m_graph.GetRoot();
                        m_graph.SetContent(root, AZStd::make_shared<DataTypes::MockIGraphObject>(0));

                        SceneGraph::NodeIndex indexA = m_graph.AddChild(root, "A", AZStd::make_shared<DataTypes::MockIGraphObject>(1));
                        SceneGraph::NodeIndex indexB = m_graph.AddChild(indexA, "B", AZStd::make_shared<DataTypes::MockIGraphObject>(2));
                        SceneGraph::NodeIndex indexC = m_graph.AddSibling(indexB, "C", AZStd::make_shared<DataTypes::MockIGraphObject>(3));
                        m_graph.AddSibling(indexC, "D", AZStd::make_shared<DataTypes::MockIGraphObject>(4));

                        SceneGraph::NodeIndex indexE = m_graph.AddChild(indexC, "E", AZStd::make_shared<DataTypes::MockIGraphObject>(5));
                        m_graph.AddSibling(indexE, "F", AZStd::make_shared<DataTypes::MockIGraphObject>(6));
                    }

                    SceneGraph::HierarchyStorageConstData::iterator GetRootHierarchyIterator()
                    {
                        return m_graph.ConvertToHierarchyIterator(m_graph.GetRoot());
                    }

                    SceneGraph::HierarchyStorageConstData::iterator GetDeepestHierarchyIterator()
                    {
                        SceneGraph::NodeIndex index = m_graph.Find("A.C.F");
                        return m_graph.ConvertToHierarchyIterator(index);
                    }

                    SceneGraph m_graph;
                };

                template<typename T>
                class SceneGraphDownwardsIteratorContext
                    : public SceneGraphDownwardsIteratorTest
                {
                public:
                    using Traversal = T;
                };

                TYPED_TEST_CASE_P(SceneGraphDownwardsIteratorContext);

                TYPED_TEST_P(SceneGraphDownwardsIteratorContext, MakeSceneGraphDownwardsIterator_FunctionComparedWithExplicitlyDeclaredIterator_IteratorsAreEqual)
                {
                    auto lhsIterator = MakeSceneGraphDownwardsIterator<Traversal>(m_graph, m_graph.GetNameStorage().begin());
                    auto rhsIterator = SceneGraphDownwardsIterator<SceneGraph::NameStorage::const_iterator, Traversal>(m_graph, m_graph.GetNameStorage().begin());
                    EXPECT_EQ(lhsIterator, rhsIterator);
                }

                TYPED_TEST_P(SceneGraphDownwardsIteratorContext, MakeSceneGraphDownwardsIterator_ExtendedFunctionComparedWithExplicitlyDeclaredIterator_IteratorsAreEqual)
                {
                    auto lhsIterator = MakeSceneGraphDownwardsIterator<Traversal>(m_graph, GetRootHierarchyIterator(), m_graph.GetNameStorage().begin(), true);
                    auto rhsIterator = SceneGraphDownwardsIterator<SceneGraph::NameStorage::const_iterator, Traversal>(m_graph, GetRootHierarchyIterator(), m_graph.GetNameStorage().begin(), true);
                    EXPECT_EQ(lhsIterator, rhsIterator);
                }

                TYPED_TEST_P(SceneGraphDownwardsIteratorContext, MakeSceneGraphDownwardsIterator_NodeAndHierarchyVersions_IteratorsAreEqual)
                {
                    SceneGraph::NodeIndex index = m_graph.Find("A.C");
                    auto hierarchy = m_graph.ConvertToHierarchyIterator(index);

                    auto indexIterator = MakeSceneGraphDownwardsIterator<Traversal>(m_graph, index, m_graph.GetNameStorage().begin(), true);
                    auto hierarchyIterator = MakeSceneGraphDownwardsIterator<Traversal>(m_graph, hierarchy, m_graph.GetNameStorage().begin(), true);
                    EXPECT_EQ(indexIterator, hierarchyIterator);
                }

                TYPED_TEST_P(SceneGraphDownwardsIteratorContext, MakeSceneGraphDownwardsView_FunctionComparedWithExplicitlyDeclaredIterators_ViewHasEquivalentBeginAndEnd)
                {
                    auto view = MakeSceneGraphDownwardsView<Traversal>(m_graph, m_graph.GetNameStorage().begin());
                    auto beginIterator = SceneGraphDownwardsIterator<SceneGraph::NameStorage::const_iterator, Traversal>(m_graph, m_graph.GetNameStorage().begin());
                    auto endIterator = SceneGraphDownwardsIterator<SceneGraph::NameStorage::const_iterator, Traversal>();

                    EXPECT_EQ(view.begin(), beginIterator);
                    EXPECT_EQ(view.end(), endIterator);
                }

                TYPED_TEST_P(SceneGraphDownwardsIteratorContext, MakeSceneGraphDownwardsView_ExtendedFunctionComparedWithExplicitlyDeclaredIterators_ViewHasEquivalentBeginAndEnd)
                {
                    auto view = MakeSceneGraphDownwardsView<Traversal>(m_graph, GetRootHierarchyIterator(), m_graph.GetNameStorage().begin(), true);
                    auto beginIterator = SceneGraphDownwardsIterator<SceneGraph::NameStorage::const_iterator, Traversal>(m_graph, GetRootHierarchyIterator(), m_graph.GetNameStorage().begin(), true);
                    auto endIterator = SceneGraphDownwardsIterator<SceneGraph::NameStorage::const_iterator, Traversal>();

                    EXPECT_EQ(view.begin(), beginIterator);
                    EXPECT_EQ(view.end(), endIterator);
                }

                TYPED_TEST_P(SceneGraphDownwardsIteratorContext, MakeSceneGraphDownwardsView_NodeAndHierarchyVersions_IteratorsInViewsAreEqual)
                {
                    SceneGraph::NodeIndex index = m_graph.Find("A.C");
                    auto hierarchy = m_graph.ConvertToHierarchyIterator(index);

                    auto indexView = MakeSceneGraphDownwardsView<Traversal>(m_graph, index, m_graph.GetNameStorage().begin(), true);
                    auto hierarchyView = MakeSceneGraphDownwardsView<Traversal>(m_graph, hierarchy, m_graph.GetNameStorage().begin(), true);

                    EXPECT_EQ(indexView.begin(), hierarchyView.begin());
                    EXPECT_EQ(indexView.end(), hierarchyView.end());
                }

                TYPED_TEST_P(SceneGraphDownwardsIteratorContext, Dereference_GetRootIteratorValue_ReturnsRelativeValueFromGivenValueIterator)
                {
                    auto iterator = MakeSceneGraphDownwardsIterator<Traversal>(m_graph, GetRootHierarchyIterator(), m_graph.GetNameStorage().begin(), true);
                    EXPECT_STREQ("", (*iterator).c_str());
                }

                TYPED_TEST_P(SceneGraphDownwardsIteratorContext, Dereference_GetDeepestIteratorValue_ReturnsRelativeValueFromGivenValueIterator)
                {
                    auto iterator = MakeSceneGraphDownwardsIterator<Traversal>(m_graph, GetDeepestHierarchyIterator(), m_graph.GetNameStorage().begin(), true);
                    EXPECT_STREQ("A.C.F", (*iterator).c_str());
                }

                TYPED_TEST_P(SceneGraphDownwardsIteratorContext, Dereference_ValueIteratorNotSyncedWithHierarchyIteratorIfNotRequested_ReturnedValueMatchesOriginalValueIterator)
                {
                    auto valueIterator = m_graph.GetNameStorage().begin() + 2;
                    auto iterator = MakeSceneGraphDownwardsIterator<Traversal>(m_graph, GetDeepestHierarchyIterator(), valueIterator, false);
                    EXPECT_STREQ((*valueIterator).c_str(), (*iterator).c_str());
                }

                TYPED_TEST_P(SceneGraphDownwardsIteratorContext, Dereference_DereferencingThroughStarAndArrowOperator_ValuesAreEqual)
                {
                    auto valueIterator = m_graph.GetNameStorage().begin();
                    auto iterator = MakeSceneGraphDownwardsIterator<Traversal>(m_graph, GetDeepestHierarchyIterator(), valueIterator, true);
                    EXPECT_STREQ(iterator->c_str(), (*iterator).c_str());
                }

                TYPED_TEST_P(SceneGraphDownwardsIteratorContext, GetHierarchyIterator_MatchesWithNodeInformationAfterMove_NameEqualToNodeIndexedName)
                {
                    auto valueIterator = m_graph.GetNameStorage().begin();
                    auto iterator = MakeSceneGraphDownwardsIterator<Traversal>(m_graph, GetRootHierarchyIterator(), valueIterator, true);
                    iterator++;

                    SceneGraph::HierarchyStorageConstIterator hierarchyIterator = iterator.GetHierarchyIterator();
                    SceneGraph::NodeIndex index = m_graph.ConvertToNodeIndex(hierarchyIterator);

                    EXPECT_STREQ(m_graph.GetNodeName(index).c_str(), iterator->c_str());
                }

                TYPED_TEST_P(SceneGraphDownwardsIteratorContext, IncrementOperator_MovePastEnd_ReturnsEndIterator)
                {
                    auto valueIterator = m_graph.GetNameStorage().begin();
                    auto iterator = MakeSceneGraphDownwardsIterator<Traversal>(m_graph, GetDeepestHierarchyIterator(), valueIterator, true);
                    iterator++;
                    auto endIterator = SceneGraphDownwardsIterator<SceneGraph::NameStorage::const_iterator, Traversal>();
                    EXPECT_EQ(endIterator, iterator);
                }

                TYPED_TEST_P(SceneGraphDownwardsIteratorContext, EmptyGraph_CanDetectEmptyGraph_BeginPlusOneAndEndIteratorAreEqual)
                {
                    SceneGraph emptyGraph;

                    auto beginIterator = MakeSceneGraphDownwardsIterator<Traversal>(emptyGraph, emptyGraph.GetHierarchyStorage().begin(), emptyGraph.GetNameStorage().begin(), true);
                    beginIterator++; // Even an empty graph will have at least a root entry.
                    auto endIterator = SceneGraphDownwardsIterator<SceneGraph::NameStorage::const_iterator, Traversal>();
                    EXPECT_EQ(beginIterator, endIterator);
                }

                TYPED_TEST_P(SceneGraphDownwardsIteratorContext, EmptyGraph_CanDetectEmptyGraphFromView_BeginPlusOneAndEndIteratorAreEqual)
                {
                    SceneGraph emptyGraph;

                    auto view = MakeSceneGraphDownwardsView<Traversal>(emptyGraph, emptyGraph.GetHierarchyStorage().begin(), emptyGraph.GetNameStorage().begin(), true);
                    EXPECT_EQ(++view.begin(), view.end());
                }

                TYPED_TEST_P(SceneGraphDownwardsIteratorContext, Algorithm_RangeForLoop_CanSuccesfullyRun)
                {
                    auto sceneView = MakeSceneGraphDownwardsView<Traversal>(m_graph, GetRootHierarchyIterator(), m_graph.GetNameStorage().begin(), true);
                    for (auto& it : sceneView)
                    {
                    }
                }

                TYPED_TEST_P(SceneGraphDownwardsIteratorContext, IncrementOperator_TouchesAllNodes_NumberOfIterationStepsMatchesNumberOfNodesInGraph)
                {
                    size_t entryCount = m_graph.GetHierarchyStorage().end() - m_graph.GetHierarchyStorage().begin();
                    size_t localCount = 0;

                    auto sceneView = MakeSceneGraphDownwardsView<Traversal>(m_graph, GetRootHierarchyIterator(), m_graph.GetNameStorage().begin(), true);
                    for (auto& it : sceneView)
                    {
                        localCount++;
                    }
                    EXPECT_EQ(entryCount, localCount);
                }

                TYPED_TEST_P(SceneGraphDownwardsIteratorContext, ValueIterator_NonSceneGraphIterator_ExternalIteratorValuesMatchSceneGraphValues)
                {
                    // Commonly containers in the scene graph will be used, but it is possible to specify other containers that shadow
                    //      the scene graph but don't belong to it. This test checks if this works correctly by comparing the values
                    //      stored in the scene graph with the same values stored in an external container.
                    //      (See constructor of SceneGraphUpwardsIterator for more details on arguments.)
                    AZStd::vector<int> values = { 0, 1, 2, 3, 4, 5, 6 };

                    auto sceneView = MakeSceneGraphDownwardsView<Traversal>(m_graph, m_graph.GetContentStorage().begin());
                    auto valuesView = MakeSceneGraphDownwardsView<Traversal>(m_graph, values.begin());

                    auto sceneIterator = sceneView.begin();
                    auto valuesIterator = valuesView.begin();

                    while (sceneIterator != sceneView.end())
                    {
                        EXPECT_NE(valuesView.end(), valuesIterator);

                        DataTypes::MockIGraphObject* stored = azrtti_cast<DataTypes::MockIGraphObject*>(sceneIterator->get());
                        ASSERT_NE(stored, nullptr);

                        EXPECT_EQ(stored->m_id, *valuesIterator);

                        valuesIterator++;
                        sceneIterator++;
                    }
                }

                TYPED_TEST_P(SceneGraphDownwardsIteratorContext, Algorithms_Find_AlgorithmFindsRequestedName)
                {
                    auto sceneView = MakeSceneGraphDownwardsView<Traversal>(m_graph, m_graph.GetNameStorage().begin());
                    auto result = AZStd::find(sceneView.begin(), sceneView.end(), "A.C");
                    auto compare = m_graph.ConvertToHierarchyIterator(m_graph.Find("A.C"));

                    EXPECT_EQ(compare, result.GetHierarchyIterator());
                }

                TYPED_TEST_P(SceneGraphDownwardsIteratorContext, Algorithms_FindIf_FindsValue3InNodeAdotC)
                {
                    SceneGraph::NodeIndex index = m_graph.Find("A.C");
                    auto sceneView = MakeSceneGraphDownwardsView<Traversal>(m_graph, m_graph.GetContentStorage().begin());
                    auto result = AZStd::find_if(sceneView.begin(), sceneView.end(),
                        [](const AZStd::shared_ptr<DataTypes::IGraphObject>& object) -> bool
                        {
                            const DataTypes::MockIGraphObject* value = azrtti_cast<const DataTypes::MockIGraphObject*>(object.get());
                            return value && value->m_id == 3;
                        });
                    ASSERT_NE(sceneView.end(), result);
                   
                    const DataTypes::MockIGraphObject* value = azrtti_cast<const DataTypes::MockIGraphObject*>(result->get());
                    ASSERT_NE(nullptr, value);
                    EXPECT_EQ(3, value->m_id);
                }

                REGISTER_TYPED_TEST_CASE_P(SceneGraphDownwardsIteratorContext,
                    MakeSceneGraphDownwardsIterator_FunctionComparedWithExplicitlyDeclaredIterator_IteratorsAreEqual,
                    MakeSceneGraphDownwardsIterator_ExtendedFunctionComparedWithExplicitlyDeclaredIterator_IteratorsAreEqual,
                    MakeSceneGraphDownwardsIterator_NodeAndHierarchyVersions_IteratorsAreEqual,
                    MakeSceneGraphDownwardsView_FunctionComparedWithExplicitlyDeclaredIterators_ViewHasEquivalentBeginAndEnd,
                    MakeSceneGraphDownwardsView_ExtendedFunctionComparedWithExplicitlyDeclaredIterators_ViewHasEquivalentBeginAndEnd,
                    MakeSceneGraphDownwardsView_NodeAndHierarchyVersions_IteratorsInViewsAreEqual,
                    Dereference_GetRootIteratorValue_ReturnsRelativeValueFromGivenValueIterator,
                    Dereference_GetDeepestIteratorValue_ReturnsRelativeValueFromGivenValueIterator,
                    Dereference_ValueIteratorNotSyncedWithHierarchyIteratorIfNotRequested_ReturnedValueMatchesOriginalValueIterator,
                    Dereference_DereferencingThroughStarAndArrowOperator_ValuesAreEqual,
                    GetHierarchyIterator_MatchesWithNodeInformationAfterMove_NameEqualToNodeIndexedName,
                    IncrementOperator_MovePastEnd_ReturnsEndIterator,
                    EmptyGraph_CanDetectEmptyGraph_BeginPlusOneAndEndIteratorAreEqual,
                    EmptyGraph_CanDetectEmptyGraphFromView_BeginPlusOneAndEndIteratorAreEqual,
                    Algorithm_RangeForLoop_CanSuccesfullyRun,
                    IncrementOperator_TouchesAllNodes_NumberOfIterationStepsMatchesNumberOfNodesInGraph,
                    ValueIterator_NonSceneGraphIterator_ExternalIteratorValuesMatchSceneGraphValues,
                    Algorithms_Find_AlgorithmFindsRequestedName,
                    Algorithms_FindIf_FindsValue3InNodeAdotC
                    );

                using Group = ::testing::Types<DepthFirst, BreadthFirst>;
                INSTANTIATE_TYPED_TEST_CASE_P(SceneGraphDownwardsIteratorTests, SceneGraphDownwardsIteratorContext, Group);

                TEST_F(SceneGraphDownwardsIteratorTest, DepthFirst_IncrementOperator_MoveUpTheTree_IteratorReturnsParentOfPreviousIteration)
                {
                    auto iterator = MakeSceneGraphDownwardsIterator<DepthFirst>(m_graph, m_graph.GetNameStorage().begin());
                    EXPECT_STREQ("", iterator->c_str());
                    EXPECT_STREQ("A", (++iterator)->c_str());
                    EXPECT_STREQ("A.B", (++iterator)->c_str());
                    EXPECT_STREQ("A.C", (++iterator)->c_str());
                    EXPECT_STREQ("A.C.E", (++iterator)->c_str());
                    EXPECT_STREQ("A.C.F", (++iterator)->c_str());
                    EXPECT_STREQ("A.D", (++iterator)->c_str());
                }

                TEST_F(SceneGraphDownwardsIteratorTest, BreadthFirst_IncrementOperator_MoveUpTheTree_IteratorReturnsParentOfPreviousIteration)
                {
                    auto iterator = MakeSceneGraphDownwardsIterator<BreadthFirst>(m_graph, m_graph.GetNameStorage().begin());
                    EXPECT_STREQ("", iterator->c_str());
                    EXPECT_STREQ("A", (++iterator)->c_str());
                    EXPECT_STREQ("A.B", (++iterator)->c_str());
                    EXPECT_STREQ("A.C", (++iterator)->c_str());
                    EXPECT_STREQ("A.D", (++iterator)->c_str());
                    EXPECT_STREQ("A.C.E", (++iterator)->c_str());
                    EXPECT_STREQ("A.C.F", (++iterator)->c_str());
                }

                TEST_F(SceneGraphDownwardsIteratorTest, DepthFirst_Algorithms_Copy_AllValuesCopiedToNewArray)
                {
                    AZStd::vector<std::string> names(7);
                    auto sceneView = MakeSceneGraphDownwardsView<DepthFirst>(m_graph, m_graph.GetNameStorage().begin());
                    AZStd::copy(sceneView.begin(), sceneView.end(), names.begin());

                    EXPECT_STREQ("", names[0].c_str());
                    EXPECT_STREQ("A", names[1].c_str());
                    EXPECT_STREQ("A.B", names[2].c_str());
                    EXPECT_STREQ("A.C", names[3].c_str());
                    EXPECT_STREQ("A.C.E", names[4].c_str());
                    EXPECT_STREQ("A.C.F", names[5].c_str());
                    EXPECT_STREQ("A.D", names[6].c_str());
                }

                TEST_F(SceneGraphDownwardsIteratorTest, BreadthFirst_Algorithms_Copy_AllValuesCopiedToNewArray)
                {
                    AZStd::vector<std::string> names(7);
                    auto sceneView = MakeSceneGraphDownwardsView<BreadthFirst>(m_graph, m_graph.GetNameStorage().begin());
                    AZStd::copy(sceneView.begin(), sceneView.end(), names.begin());

                    EXPECT_STREQ("", names[0].c_str());
                    EXPECT_STREQ("A", names[1].c_str());
                    EXPECT_STREQ("A.B", names[2].c_str());
                    EXPECT_STREQ("A.C", names[3].c_str());
                    EXPECT_STREQ("A.D", names[4].c_str());
                    EXPECT_STREQ("A.C.E", names[5].c_str());
                    EXPECT_STREQ("A.C.F", names[6].c_str());
                }

                TEST(SceneGraphDownwardsIterator, DepthFirst_IncrementOperator_EdgeCase_NodesAreListedInCorrectOrder)
                {
                    /*---------------------------------------\
                    |      Root                              |
                    |       |                                |
                    |       A                                |
                    |     / |                                |
                    |    B  C                                |
                    |   /  /                                 |
                    |  D  E                                  |
                    |    /                                   |
                    |   F                                    |
                    \---------------------------------------*/

                    SceneGraph graph;
                    SceneGraph::NodeIndex root = graph.GetRoot();
                    
                    SceneGraph::NodeIndex indexA = graph.AddChild(root, "A");
                    SceneGraph::NodeIndex indexB = graph.AddChild(indexA, "B");
                    SceneGraph::NodeIndex indexC = graph.AddSibling(indexB, "C");

                    graph.AddChild(indexB, "D");

                    SceneGraph::NodeIndex indexE = graph.AddChild(indexC, "E");
                    graph.AddChild(indexE, "F");

                    auto iterator = MakeSceneGraphDownwardsIterator<DepthFirst>(graph, graph.GetNameStorage().begin());
                    EXPECT_STREQ("", iterator->c_str());
                    EXPECT_STREQ("A", (++iterator)->c_str());
                    EXPECT_STREQ("A.B", (++iterator)->c_str());
                    EXPECT_STREQ("A.B.D", (++iterator)->c_str());
                    EXPECT_STREQ("A.C", (++iterator)->c_str());
                    EXPECT_STREQ("A.C.E", (++iterator)->c_str());
                    EXPECT_STREQ("A.C.E.F", (++iterator)->c_str());
                }

                TEST(SceneGraphDownwardsIterator, BreadthFirst_IncrementOperator_EdgeCase_NodesAreListedInCorrectOrder)
                {
                    /*---------------------------------------\
                    |      Root                              |
                    |       |                                |
                    |       A                                |
                    |     / |                                |
                    |    B  C                                |
                    |   /  /                                 |
                    |  D  E                                  |
                    |    /                                   |
                    |   F                                    |
                    \---------------------------------------*/

                    SceneGraph graph;
                    SceneGraph::NodeIndex root = graph.GetRoot();
                    
                    SceneGraph::NodeIndex indexA = graph.AddChild(root, "A");
                    SceneGraph::NodeIndex indexB = graph.AddChild(indexA, "B");
                    SceneGraph::NodeIndex indexC = graph.AddSibling(indexB, "C");

                    graph.AddChild(indexB, "D");

                    SceneGraph::NodeIndex indexE = graph.AddChild(indexC, "E");
                    graph.AddChild(indexE, "F");

                    auto iterator = MakeSceneGraphDownwardsIterator<BreadthFirst>(graph, graph.GetNameStorage().begin());
                    EXPECT_STREQ("", iterator->c_str());
                    EXPECT_STREQ("A", (++iterator)->c_str());
                    EXPECT_STREQ("A.B", (++iterator)->c_str());
                    EXPECT_STREQ("A.C", (++iterator)->c_str());
                    EXPECT_STREQ("A.B.D", (++iterator)->c_str());
                    EXPECT_STREQ("A.C.E", (++iterator)->c_str());
                    EXPECT_STREQ("A.C.E.F", (++iterator)->c_str());
                }
            } // Views
        } // Containers
    } // SceneAPI
} // AZ
