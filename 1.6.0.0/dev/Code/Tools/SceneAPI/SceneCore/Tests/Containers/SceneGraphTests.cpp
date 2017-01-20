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

#include <string>
#include <AzTest/AzTest.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <SceneAPI/SceneCore/Containers/SceneGraph.h>
#include <SceneAPI/SceneCore/DataTypes/IGraphObject.h>
#include <SceneAPI/SceneCore/Mocks/DataTypes/MockIGraphObject.h>


namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            TEST(SceneGraphTest, Constructor_Default_HasRoot)
            {
                SceneGraph testSceneGraph;
                EXPECT_TRUE(testSceneGraph.GetRoot().IsValid());
            }


            TEST(SceneGraphTest, Find_NonExistantNode_IsNotValid)
            {
                SceneGraph testSceneGraph;
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.Find("NonExistantNodeName");
                EXPECT_FALSE(testNodeIndex.IsValid());
            }

            // GetNodeCount
            TEST(SceneGraphTest, GetNodeCount_CountForEmptyGraph_Returns1)
            {
                SceneGraph testSceneGraph;
                size_t count = testSceneGraph.GetNodeCount();
                EXPECT_EQ(1, count);
            }


            //AddSibling
            TEST(SceneGraphTest, AddSibling_NodeValid_NodeIndexValid)
            {
                SceneGraph testSceneGraph;
                AZStd::shared_ptr<DataTypes::MockIGraphObject> testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddSibling(testSceneGraph.GetRoot(), "testObject", AZStd::move(testDataObject));
                EXPECT_TRUE(testNodeIndex.IsValid());
            }

            TEST(SceneGraphTest, AddSibling_NodeHasSiblingAlready_NodeIndexValidAndNotFirstNode)
            {
                SceneGraph testSceneGraph;
                AZStd::shared_ptr<DataTypes::MockIGraphObject> testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex firstNode = testSceneGraph.AddSibling(testSceneGraph.GetRoot(), "testObject", AZStd::move(testDataObject));

                testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex secondNode = testSceneGraph.AddSibling(testSceneGraph.GetRoot(), "testObject1", AZStd::move(testDataObject));
                EXPECT_TRUE(secondNode.IsValid());
                EXPECT_NE(firstNode, secondNode);
            }

            TEST(SceneGraphTest, AddSibling_NodeInvalid_NodeIndexIsNotValid)
            {
                SceneGraph testSceneGraph;
                AZStd::shared_ptr<DataTypes::MockIGraphObject> testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex invalidNodeIndex = testSceneGraph.Find("NonExistantNodeName");
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddSibling(invalidNodeIndex, "testObject", AZStd::move(testDataObject));

                EXPECT_FALSE(testNodeIndex.IsValid());
            }

            TEST(SceneGraphTest, AddSibling_RootNoData_NodeIndexValid)
            {
                SceneGraph testSceneGraph;
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddSibling(testSceneGraph.GetRoot(), "testObject");
                EXPECT_TRUE(testNodeIndex.IsValid());
            }

            //AddChild (implementation depends on AddSibling)
            TEST(SceneGraphTest, AddChild_ParentValid_NodeIndexValid)
            {
                SceneGraph testSceneGraph;
                AZStd::shared_ptr<DataTypes::MockIGraphObject> testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), "testObject", AZStd::move(testDataObject));
                EXPECT_TRUE(testNodeIndex.IsValid());
            }

            TEST(SceneGraphTest, AddChild_ParentValidNoData_NodeIndexValid)
            {
                SceneGraph testSceneGraph;
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), "testObject");
                EXPECT_TRUE(testNodeIndex.IsValid());
            }

            TEST(SceneGraphTest, AddChild_ParentHasChildAlready_NodeIndexValidNotEqualFirst)
            {
                SceneGraph testSceneGraph;
                AZStd::shared_ptr<DataTypes::MockIGraphObject> testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex firstIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), "testObject", AZStd::move(testDataObject));

                testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex secondIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), "testObject1", AZStd::move(testDataObject));

                EXPECT_TRUE(secondIndex.IsValid());
                EXPECT_NE(firstIndex, secondIndex);
            }

            TEST(SceneGraphTest, AddChild_InvalidNameUsed_AssertTriggered)
            {
                SceneGraph testSceneGraph;
                EXPECT_DEATH(testSceneGraph.AddChild(testSceneGraph.GetRoot(), "Invalid.Name"), ".*");
            }

            TEST(SceneGraphTest, AddChild_DuplicateNameUsed_AssertTriggered)
            {
                SceneGraph testSceneGraph;
                testSceneGraph.AddChild(testSceneGraph.GetRoot(), "testObject");
                EXPECT_DEATH(testSceneGraph.AddChild(testSceneGraph.GetRoot(), "testObject"), ".*");
            }

            TEST(SceneGraphTest, AddChild_ParentIsEndPoint_AssertTriggered)
            {
                SceneGraph testSceneGraph;
                testSceneGraph.MakeEndPoint(testSceneGraph.GetRoot());

                AZStd::shared_ptr<DataTypes::MockIGraphObject> testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                EXPECT_DEATH(testSceneGraph.AddChild(testSceneGraph.GetRoot(), "testObject", AZStd::move(testDataObject)), ".*");
            }

            TEST(SceneGraphTest, AddChild_ParentInvalid_NodeIndexIsNotValid)
            {
                SceneGraph testSceneGraph;
                AZStd::shared_ptr<DataTypes::MockIGraphObject> testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex invalidNodeIndex = testSceneGraph.Find("NonExistantNodeName");
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddChild(invalidNodeIndex, "testObject", AZStd::move(testDataObject));
                EXPECT_FALSE(testNodeIndex.IsValid());
            }


            //HasNodeContent
            TEST(SceneGraphTest, HasNodeContent_AddChildCalledwithData_NodeHasData)
            {
                SceneGraph testSceneGraph;
                AZStd::shared_ptr<DataTypes::MockIGraphObject> testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), "testObject", AZStd::move(testDataObject));
                EXPECT_TRUE(testSceneGraph.HasNodeContent(testNodeIndex));
            }

            TEST(SceneGraphTest, HasNodeContent_AddChildCalledwithNoData_NodeHasNoData)
            {
                SceneGraph testSceneGraph;
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), "testObject");
                EXPECT_FALSE(testSceneGraph.HasNodeContent(testNodeIndex));
            }

            TEST(SceneGraphTest, HasNodeContent_AddSiblingCalledwithData_NodeHasData)
            {
                SceneGraph testSceneGraph;
                AZStd::shared_ptr<DataTypes::MockIGraphObject> testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddSibling(testSceneGraph.GetRoot(), "testObject", AZStd::move(testDataObject));
                EXPECT_TRUE(testSceneGraph.HasNodeContent(testNodeIndex));
            }

            TEST(SceneGraphTest, HasNodeContent_AddSiblingCalledwithNoData_NodeHasNoData)
            {
                SceneGraph testSceneGraph;
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddSibling(testSceneGraph.GetRoot(), "testObject");
                EXPECT_FALSE(testSceneGraph.HasNodeContent(testNodeIndex));
            }

            // IsNodeEndPoint
            TEST(SceneGraphTest, IsNodeEndPoint_NewNodesAreNotEndPoints_NodeIsNotAnEndPoint)
            {
                SceneGraph testSceneGraph;
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddSibling(testSceneGraph.GetRoot(), "testObject");
                EXPECT_FALSE(testSceneGraph.IsNodeEndPoint(testNodeIndex));
            }

            //GetNodeContent
            TEST(SceneGraphTest, GetNodeContent_IntDataChildAddedToRoot_CanGetNodeData)
            {
                SceneGraph testSceneGraph;
                AZStd::shared_ptr<DataTypes::MockIGraphObject> testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>(1);
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), "testObject", AZStd::move(testDataObject));

                AZStd::shared_ptr<DataTypes::MockIGraphObject> storedValue = azrtti_cast<DataTypes::MockIGraphObject*>(testSceneGraph.GetNodeContent(testNodeIndex));
                ASSERT_NE(nullptr, storedValue);
                EXPECT_EQ(1, storedValue->m_id);
            }

            TEST(SceneGraphTest, GetNodeContent_NoData_NodeDataIsNullPtr)
            {
                SceneGraph testSceneGraph;
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), "testObject");

                AZStd::shared_ptr<DataTypes::IGraphObject> storedValue = testSceneGraph.GetNodeContent(testNodeIndex);
                EXPECT_EQ(nullptr, storedValue);
            }


            //Find
            TEST(SceneGraphTest, Find_OnRootwithChild_NodeIndexIsCorrect)
            {
                SceneGraph testSceneGraph;
                AZStd::shared_ptr<DataTypes::MockIGraphObject> testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), "TestObject", AZStd::move(testDataObject));

                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find("TestObject");
                EXPECT_TRUE(foundIndex.IsValid());
                EXPECT_EQ(foundIndex, testNodeIndex);
            }

            TEST(SceneGraphTest, Find_OnRootwithChildwithChild_NodeIndexIsCorrect)
            {
                SceneGraph testSceneGraph;
                AZStd::shared_ptr<DataTypes::MockIGraphObject> testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex firstChildNodeIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), "FirstChild", AZStd::move(testDataObject));

                testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddChild(firstChildNodeIndex, "FirstChildofFirstChild", AZStd::move(testDataObject));

                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find("FirstChild.FirstChildofFirstChild");
                EXPECT_TRUE(foundIndex.IsValid());
                EXPECT_EQ(foundIndex, testNodeIndex);
            }

            TEST(SceneGraphTest, Find_OnRootwithSecondChild_NodeIndexIsCorrect)
            {
                SceneGraph testSceneGraph;
                AZStd::shared_ptr<DataTypes::MockIGraphObject> testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex firstChildNodeIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), "FirstChild", AZStd::move(testDataObject));

                testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), "SecondChild", AZStd::move(testDataObject));

                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find("SecondChild");
                EXPECT_TRUE(foundIndex.IsValid());
                EXPECT_EQ(foundIndex, testNodeIndex);
            }

            TEST(SceneGraphTest, Find_OnNodewithSecondChildLookingForSecondChild_NodeIndexIsCorrect)
            {
                SceneGraph testSceneGraph;

                AZStd::shared_ptr<DataTypes::MockIGraphObject> testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex testRootNodeIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), "testRoot", AZStd::move(testDataObject));

                testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex firstChildNodeIndex = testSceneGraph.AddChild(testRootNodeIndex, "FirstChild", AZStd::move(testDataObject));

                testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddChild(testRootNodeIndex, "SecondChild", AZStd::move(testDataObject));

                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find(testRootNodeIndex, "SecondChild");
                EXPECT_TRUE(foundIndex.IsValid());
                EXPECT_EQ(foundIndex, testNodeIndex);
            }

            TEST(SceneGraphTest, Find_ParentDoesNotHaveThisChild_NodeIndexIsNotValid)
            {
                SceneGraph testSceneGraph;

                AZStd::shared_ptr<DataTypes::MockIGraphObject> testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex testRootNodeIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), "testRoot", AZStd::move(testDataObject));

                testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex testRootNodeSiblingIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), "testRootSibling", AZStd::move(testDataObject));

                testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex firstChildNodeIndex = testSceneGraph.AddChild(testRootNodeIndex, "FirstChild", AZStd::move(testDataObject));

                testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>();
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddChild(testRootNodeIndex, "SecondChild", AZStd::move(testDataObject));

                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find(testRootNodeSiblingIndex, "SecondChild");
                EXPECT_FALSE(foundIndex.IsValid());
            }

            //SetContent
            TEST(SceneGraphTest, SetContent_EmptyNodeByReference_NewValueConfirmed)
            {
                SceneGraph testSceneGraph;
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), "testNode");

                AZStd::shared_ptr<DataTypes::MockIGraphObject> testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>(1);
                bool result = testSceneGraph.SetContent(testNodeIndex, testDataObject);
                EXPECT_TRUE(result);

                AZStd::shared_ptr<DataTypes::MockIGraphObject> storedValue = azrtti_cast<DataTypes::MockIGraphObject*>(testSceneGraph.GetNodeContent(testNodeIndex));
                ASSERT_NE(nullptr, storedValue);
                EXPECT_EQ(1, storedValue->m_id);
            }

            TEST(SceneGraphTest, SetContent_EmptyNodeByMove_NewValueConfirmed)
            {
                SceneGraph testSceneGraph;
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), "testNode");

                bool result = testSceneGraph.SetContent(testNodeIndex, AZStd::make_shared<DataTypes::MockIGraphObject>(1));
                EXPECT_TRUE(result);

                AZStd::shared_ptr<DataTypes::MockIGraphObject> storedValue = azrtti_cast<DataTypes::MockIGraphObject*>(testSceneGraph.GetNodeContent(testNodeIndex));
                ASSERT_NE(nullptr, storedValue);
                EXPECT_EQ(1, storedValue->m_id);
            }

            TEST(SceneGraphTest, SetContent_ExistingNodeByReference_NewFloatConfirmed)
            {
                SceneGraph testSceneGraph;

                AZStd::shared_ptr<DataTypes::MockIGraphObject> testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>(1);
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), "testNode", AZStd::move(testDataObject));

                testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>(2);
                bool result = testSceneGraph.SetContent(testNodeIndex, testDataObject);
                EXPECT_TRUE(result);

                AZStd::shared_ptr<DataTypes::MockIGraphObject> storedValue = azrtti_cast<DataTypes::MockIGraphObject*>(testSceneGraph.GetNodeContent(testNodeIndex));
                ASSERT_NE(nullptr, storedValue);
                EXPECT_EQ(2, storedValue->m_id);
            }

            TEST(SceneGraphTest, SetContent_ExistingNodeByMove_NewFloatConfirmed)
            {
                SceneGraph testSceneGraph;

                AZStd::shared_ptr<DataTypes::MockIGraphObject> testDataObject = AZStd::make_shared<DataTypes::MockIGraphObject>(1);
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), "testNode", AZStd::move(testDataObject));

                bool result = testSceneGraph.SetContent(testNodeIndex, AZStd::make_shared<DataTypes::MockIGraphObject>(2));
                EXPECT_TRUE(result);

                AZStd::shared_ptr<DataTypes::MockIGraphObject> storedValue = azrtti_cast<DataTypes::MockIGraphObject*>(testSceneGraph.GetNodeContent(testNodeIndex));
                ASSERT_NE(nullptr, storedValue);
                EXPECT_EQ(2, storedValue->m_id);
            }

            // MakeEndPoint
            TEST(SceneGraphTest, MakeEndPoint_MarkNodeAsEndPoint_NodeIsAnEndPoint)
            {
                SceneGraph testSceneGraph;
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddSibling(testSceneGraph.GetRoot(), "testObject");
                testSceneGraph.MakeEndPoint(testNodeIndex);
                EXPECT_TRUE(testSceneGraph.IsNodeEndPoint(testNodeIndex));
            }

            TEST(SceneGraphTest, MakeEndPoint_AddChildToEndPointNode_FailsToAddChild)
            {
                SceneGraph testSceneGraph;
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddSibling(testSceneGraph.GetRoot(), "testObject");
                testSceneGraph.MakeEndPoint(testNodeIndex);

                EXPECT_DEATH(testSceneGraph.AddChild(testNodeIndex, "testObject2"), ".*");
            }

            TEST(SceneGraphTest, MakeEndPoint_AddSiblingToEndPointNode_SiblingAdded)
            {
                SceneGraph testSceneGraph;
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddSibling(testSceneGraph.GetRoot(), "testObject");
                testSceneGraph.MakeEndPoint(testNodeIndex);

                SceneGraph::NodeIndex result = testSceneGraph.AddSibling(testNodeIndex, "testObject2");
                EXPECT_TRUE(result.IsValid());
            }

            //GetNodeName/Data These are testing use cases that haven't been covered
            TEST(SceneGraphTest, GetNodeName_NodeExists_ReturnsCorrectName)
            {
                SceneGraph testSceneGraph;

                std::string expectedNodeName("TestNode");

                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), expectedNodeName.c_str());
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find(expectedNodeName);
                EXPECT_TRUE(foundIndex.IsValid());
                const std::string nodeName = testSceneGraph.GetNodeName(foundIndex);
                EXPECT_EQ(expectedNodeName, nodeName);
            }

            TEST(SceneGraphTest, GetNodeName_InvalidNode_Invalid)
            {
                SceneGraph testSceneGraph;
                SceneGraph::NodeIndex testNodeIndex = testSceneGraph.Find("NonExistantNodeName");

                const std::string nodeName = testSceneGraph.GetNodeName(testNodeIndex);
                std::string testString("<Invalid>");
                EXPECT_EQ("<Invalid>", nodeName);
            }

            // Clear
            TEST(SceneGraphTest, Clear_ClearningEmptyGraph_NoChangeToTheNodeCount)
            {
                SceneGraph testSceneGraph;
                testSceneGraph.Clear();
                EXPECT_EQ(1, testSceneGraph.GetNodeCount());
            }

            // IsValidName
            TEST(SceneGraphTest, IsValidName_NullPtrPassed_ReturnsFalse)
            {
                EXPECT_FALSE(SceneGraph::IsValidName(nullptr));
            }

            TEST(SceneGraphTest, IsValidName_EmptyStringGiven_ReturnsFalse)
            {
                std::string emptyString;
                EXPECT_FALSE(SceneGraph::IsValidName(emptyString));
                EXPECT_FALSE(SceneGraph::IsValidName(emptyString.c_str()));
            }

            TEST(SceneGraphTest, IsValidName_ValidStringGiven_ReturnsTrue)
            {
                std::string validString = "valid";
                EXPECT_TRUE(SceneGraph::IsValidName(validString));
                EXPECT_TRUE(SceneGraph::IsValidName(validString.c_str()));
            }

            TEST(SceneGraphTest, IsValidName_StringContainsInvalidCharacter_ReturnsFalse)
            {
                std::string invalidString = "inva.lid";
                EXPECT_FALSE(SceneGraph::IsValidName(invalidString));
                EXPECT_FALSE(SceneGraph::IsValidName(invalidString.c_str()));
            }

            // GetShortName
            TEST(SceneGraphTest, GetShortName_StringContainsParentSeparatorAndChild_ReturnsChild)
            {
                std::string name = "parent.child";
                EXPECT_STREQ("child", SceneGraph::GetShortName(name).c_str());
                EXPECT_STREQ("child", SceneGraph::GetShortName(name.c_str()).c_str());
            }

            TEST(SceneGraphTest, GetShortName_EmptyString_ReturnsEmptyString)
            {
                std::string name = "";
                EXPECT_STREQ("", SceneGraph::GetShortName(name).c_str());
                EXPECT_STREQ("", SceneGraph::GetShortName(name.c_str()).c_str());
            }

            TEST(SceneGraphTest, GetShortName_NullPtrForString_ReturnsEmptyString)
            {
                EXPECT_STREQ("", SceneGraph::GetShortName(nullptr).c_str());
            }

            TEST(SceneGraphTest, GetShortName_SingleSeparator_ReturnsEmptyString)
            {
                std::string name = ".";
                EXPECT_STREQ("", SceneGraph::GetShortName(name).c_str());
                EXPECT_STREQ("", SceneGraph::GetShortName(name.c_str()).c_str());
            }

            TEST(SceneGraphTest, GetShortName_SeparatorIsLastCharacter_ReturnsEmptyString)
            {
                std::string name = "parent.";
                EXPECT_STREQ("", SceneGraph::GetShortName(name).c_str());
                EXPECT_STREQ("", SceneGraph::GetShortName(name.c_str()).c_str());
            }

            TEST(SceneGraphTest, GetShortName_SeparatorIsFirstCharacter_ReturnsChild)
            {
                std::string name = ".child";
                EXPECT_STREQ("child", SceneGraph::GetShortName(name).c_str());
                EXPECT_STREQ("child", SceneGraph::GetShortName(name.c_str()).c_str());
            }

            TEST(SceneGraphTest, GetShortName_MultipeSeparators_ReturnsLastItem)
            {
                std::string name = "item0.item1.item2";
                EXPECT_STREQ("item2", SceneGraph::GetShortName(name).c_str());
                EXPECT_STREQ("item2", SceneGraph::GetShortName(name.c_str()).c_str());
            }

            //  tests run on a prearranged, more complex configuration
            class SceneGraphTests
                : public ::testing::Test
            {
            public:
                SceneGraphTests()
                {
                }

            protected:
                void SetUp()
                {
                    /*---------------------------------------\
                    |       Root                             |
                    |        |         |                     |
                    |        A         B                     |
                    |        |        /|\                    |
                    |        C       I J K                   |
                    |      / | \          \                  |
                    |     D  E  F          L                 |
                    |       / \                              |
                    |      G   H                             |
                    \---------------------------------------*/

                    //Build up the graph
                    SceneGraph::NodeIndex testNodeIndex = testSceneGraph.AddChild(testSceneGraph.GetRoot(), "A", AZStd::make_shared<DataTypes::MockIGraphObject>(1));
                    SceneGraph::NodeIndex indexA = testNodeIndex;
                    SceneGraph::NodeIndex indexB = testSceneGraph.AddSibling(indexA, "B", AZStd::make_shared<DataTypes::MockIGraphObject>(2));

                    testNodeIndex = testSceneGraph.AddChild(testNodeIndex, "C", AZStd::make_shared<DataTypes::MockIGraphObject>(3));
                    
                    testNodeIndex = testSceneGraph.AddChild(testNodeIndex, "D", AZStd::make_shared<DataTypes::MockIGraphObject>(4));
                    SceneGraph::NodeIndex EIndex = testSceneGraph.AddSibling(testNodeIndex, "E", AZStd::make_shared<DataTypes::MockIGraphObject>(5));
                    testNodeIndex = testSceneGraph.AddSibling(testNodeIndex, "F", AZStd::make_shared<DataTypes::MockIGraphObject>(6));
                    testNodeIndex = testSceneGraph.AddChild(EIndex, "G", AZStd::make_shared<DataTypes::MockIGraphObject>(7));
                    testNodeIndex = testSceneGraph.AddSibling(testNodeIndex, "H", AZStd::make_shared<DataTypes::MockIGraphObject>(8));

                    
                    testNodeIndex = testSceneGraph.AddChild(indexB, "I", AZStd::make_shared<DataTypes::MockIGraphObject>(9));
                    testNodeIndex = testSceneGraph.AddChild(indexB, "J", AZStd::make_shared<DataTypes::MockIGraphObject>(10));
                    testNodeIndex = testSceneGraph.AddChild(indexB, "K", AZStd::make_shared<DataTypes::MockIGraphObject>(11));
                    testNodeIndex = testSceneGraph.AddChild(testNodeIndex, "L", AZStd::make_shared<DataTypes::MockIGraphObject>(12));
                }

                void TearDown()
                {
                }

                SceneGraph testSceneGraph;

                static const int nodeValueA = 1;
                static const int nodeValueB = 2;
                static const int nodeValueC = 3;
                static const int nodeValueD = 4;
                static const int nodeValueE = 5;
                static const int nodeValueF = 6;
                static const int nodeValueG = 7;
                static const int nodeValueH = 8;
                static const int nodeValueI = 9;
                static const int nodeValueJ = 10;
                static const int nodeValueK = 11;
                static const int nodeValueL = 12;

                static const size_t totalNodeCount = 12 + 1; // +1 for the root node.
            };

            //Find's
            TEST_F(SceneGraphTests, FindCharPointer_E_IsValid)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find("A.C.E");
                EXPECT_TRUE(foundIndex.IsValid());
            }

            TEST_F(SceneGraphTests, FindString_E_IsValid)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find(std::string("A.C.E"));
                EXPECT_TRUE(foundIndex.IsValid());
            }

            TEST_F(SceneGraphTests, FindRootCharPointer_G_IsValid)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find(std::string("A.C.E"));
                foundIndex = testSceneGraph.Find(foundIndex, "G");
                EXPECT_TRUE(foundIndex.IsValid());
            }

            TEST_F(SceneGraphTests, FindRootString_G_IsValid)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find(std::string("A.C.E"));
                foundIndex = testSceneGraph.Find(foundIndex, std::string("G"));
                EXPECT_TRUE(foundIndex.IsValid());
            }

            TEST_F(SceneGraphTests, FindRootCharPointer_Z_NotValid)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find(std::string("A.C.E"));
                foundIndex = testSceneGraph.Find(foundIndex, "Z");
                EXPECT_FALSE(foundIndex.IsValid());
            }

            //Node Find/GetNodeData integrity
            TEST_F(SceneGraphTests, GetNodeData_A_ValidValue)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find("A");
                EXPECT_TRUE(foundIndex.IsValid());
                
                AZStd::shared_ptr<DataTypes::MockIGraphObject> storedValue = azrtti_cast<DataTypes::MockIGraphObject*>(testSceneGraph.GetNodeContent(foundIndex));
                ASSERT_NE(nullptr, storedValue);
                EXPECT_EQ(nodeValueA, storedValue->m_id);
            }

            TEST_F(SceneGraphTests, GetNodeData_B_ValidValue)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find("B");
                EXPECT_TRUE(foundIndex.IsValid());
                
                AZStd::shared_ptr<DataTypes::MockIGraphObject> storedValue = azrtti_cast<DataTypes::MockIGraphObject*>(testSceneGraph.GetNodeContent(foundIndex));
                ASSERT_NE(nullptr, storedValue);
                EXPECT_EQ(nodeValueB, storedValue->m_id);
            }

            TEST_F(SceneGraphTests, GetNodeData_C_ValidValue)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find("A.C");
                EXPECT_TRUE(foundIndex.IsValid());
                
                AZStd::shared_ptr<DataTypes::MockIGraphObject> storedValue = azrtti_cast<DataTypes::MockIGraphObject*>(testSceneGraph.GetNodeContent(foundIndex));
                ASSERT_NE(nullptr, storedValue);
                EXPECT_EQ(nodeValueC, storedValue->m_id);
            }

            TEST_F(SceneGraphTests, GetNodeData_D_ValidValue)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find("A.C.D");
                EXPECT_TRUE(foundIndex.IsValid());
                
                AZStd::shared_ptr<DataTypes::MockIGraphObject> storedValue = azrtti_cast<DataTypes::MockIGraphObject*>(testSceneGraph.GetNodeContent(foundIndex));
                ASSERT_NE(nullptr, storedValue);
                EXPECT_EQ(nodeValueD, storedValue->m_id);
            }

            TEST_F(SceneGraphTests, GetNodeData_E_ValidValue)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find("A.C.E");
                EXPECT_TRUE(foundIndex.IsValid());
                
                AZStd::shared_ptr<DataTypes::MockIGraphObject> storedValue = azrtti_cast<DataTypes::MockIGraphObject*>(testSceneGraph.GetNodeContent(foundIndex));
                ASSERT_NE(nullptr, storedValue);
                EXPECT_EQ(nodeValueE, storedValue->m_id);
            }

            TEST_F(SceneGraphTests, GetNodeData_F_ValidValue)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find("A.C.F");
                EXPECT_TRUE(foundIndex.IsValid());
                
                AZStd::shared_ptr<DataTypes::MockIGraphObject> storedValue = azrtti_cast<DataTypes::MockIGraphObject*>(testSceneGraph.GetNodeContent(foundIndex));
                ASSERT_NE(nullptr, storedValue);
                EXPECT_EQ(nodeValueF, storedValue->m_id);
            }

            TEST_F(SceneGraphTests, GetNodeData_G_ValidValue)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find("A.C.E.G");
                EXPECT_TRUE(foundIndex.IsValid());
                
                AZStd::shared_ptr<DataTypes::MockIGraphObject> storedValue = azrtti_cast<DataTypes::MockIGraphObject*>(testSceneGraph.GetNodeContent(foundIndex));
                ASSERT_NE(nullptr, storedValue);
                EXPECT_EQ(nodeValueG, storedValue->m_id);
            }

            TEST_F(SceneGraphTests, GetNodeData_H_ValidValue)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find("A.C.E.H");
                EXPECT_TRUE(foundIndex.IsValid());
                
                AZStd::shared_ptr<DataTypes::MockIGraphObject> storedValue = azrtti_cast<DataTypes::MockIGraphObject*>(testSceneGraph.GetNodeContent(foundIndex));
                ASSERT_NE(nullptr, storedValue);
                EXPECT_EQ(nodeValueH, storedValue->m_id);
            }

            TEST_F(SceneGraphTests, GetNodeData_I_ValidValue)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find("B.I");
                EXPECT_TRUE(foundIndex.IsValid());
                
                AZStd::shared_ptr<DataTypes::MockIGraphObject> storedValue = azrtti_cast<DataTypes::MockIGraphObject*>(testSceneGraph.GetNodeContent(foundIndex));
                ASSERT_NE(nullptr, storedValue);
                EXPECT_EQ(nodeValueI, storedValue->m_id);
            }

            TEST_F(SceneGraphTests, GetNodeData_J_ValidValue)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find("B.J");
                EXPECT_TRUE(foundIndex.IsValid());

                AZStd::shared_ptr<DataTypes::MockIGraphObject> storedValue = azrtti_cast<DataTypes::MockIGraphObject*>(testSceneGraph.GetNodeContent(foundIndex));
                ASSERT_NE(nullptr, storedValue);
                EXPECT_EQ(nodeValueJ, storedValue->m_id);
            }

            TEST_F(SceneGraphTests, GetNodeData_K_ValidValue)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find("B.K");
                EXPECT_TRUE(foundIndex.IsValid());

                AZStd::shared_ptr<DataTypes::MockIGraphObject> storedValue = azrtti_cast<DataTypes::MockIGraphObject*>(testSceneGraph.GetNodeContent(foundIndex));
                ASSERT_NE(nullptr, storedValue);
                EXPECT_EQ(nodeValueK, storedValue->m_id);
            }

            TEST_F(SceneGraphTests, GetNodeData_L_ValidValue)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find("B.K.L");
                EXPECT_TRUE(foundIndex.IsValid());

                AZStd::shared_ptr<DataTypes::MockIGraphObject> storedValue = azrtti_cast<DataTypes::MockIGraphObject*>(testSceneGraph.GetNodeContent(foundIndex));
                ASSERT_NE(nullptr, storedValue);
                EXPECT_EQ(nodeValueL, storedValue->m_id);
            }


            //Has Relations
            TEST_F(SceneGraphTests, HasNodeSibling_GHasSibling_True)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find(std::string("A.C.E.G"));
                EXPECT_TRUE(testSceneGraph.HasNodeSibling(foundIndex));
            }

            TEST_F(SceneGraphTests, HasNodeSibling_HHasNoSibling_False)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find(std::string("A.C.E.H"));
                EXPECT_FALSE(testSceneGraph.HasNodeSibling(foundIndex));
            }

            TEST_F(SceneGraphTests, HasNodeSibling_LHasNoSibling_False)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find(std::string("B.K.L"));
                EXPECT_FALSE(testSceneGraph.HasNodeSibling(foundIndex));
            }

            TEST_F(SceneGraphTests, HasNodeChild_EHasChild_True)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find(std::string("A.C.E"));
                EXPECT_TRUE(testSceneGraph.HasNodeChild(foundIndex));
            }

            TEST_F(SceneGraphTests, HasNodeChild_GHasNoChild_False)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find(std::string("A.C.E.G"));
                EXPECT_FALSE(testSceneGraph.HasNodeChild(foundIndex));
            }

            TEST_F(SceneGraphTests, HasNodeParent_GHasParent_True)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.Find(std::string("A.C.E.G"));
                EXPECT_TRUE(testSceneGraph.HasNodeParent(foundIndex));
            }

            TEST_F(SceneGraphTests, HasNodeParent_RootHasNoParent_False)
            {
                SceneGraph::NodeIndex foundIndex = testSceneGraph.GetRoot();
                EXPECT_FALSE(testSceneGraph.HasNodeParent(foundIndex));
            }

            //GetNodeRelations
            TEST_F(SceneGraphTests, GetNodeParent_G_ReturnsE)
            {
                SceneGraph::NodeIndex sourceIndex = testSceneGraph.Find(std::string("A.C.E.G"));
                SceneGraph::NodeIndex targetIndex = testSceneGraph.Find(std::string("A.C.E"));
                SceneGraph::NodeIndex foundIndex = testSceneGraph.GetNodeParent(sourceIndex);
                EXPECT_EQ(targetIndex, foundIndex);
            }

            TEST_F(SceneGraphTests, GetNodeParent_RootNoParent_NotValid)
            {
                SceneGraph::NodeIndex sourceIndex = testSceneGraph.GetRoot();
                SceneGraph::NodeIndex foundIndex = testSceneGraph.GetNodeParent(sourceIndex);
                EXPECT_FALSE(foundIndex.IsValid());
            }

            TEST_F(SceneGraphTests, GetNodeSibling_GHasSibling_ReturnsH)
            {
                SceneGraph::NodeIndex sourceIndex = testSceneGraph.Find(std::string("A.C.E.G"));
                SceneGraph::NodeIndex targetIndex = testSceneGraph.Find(std::string("A.C.E.H"));
                SceneGraph::NodeIndex foundIndex = testSceneGraph.GetNodeSibling(sourceIndex);
                EXPECT_EQ(targetIndex, foundIndex);
            }

            TEST_F(SceneGraphTests, GetNodeSibling_HEndOfList_NotValid)
            {
                SceneGraph::NodeIndex sourceIndex = testSceneGraph.Find(std::string("A.C.E.H"));
                SceneGraph::NodeIndex foundIndex = testSceneGraph.GetNodeSibling(sourceIndex);
                EXPECT_FALSE(foundIndex.IsValid());
            }

            TEST_F(SceneGraphTests, GetNodeSibling_LNoSiblings_NotValid)
            {
                SceneGraph::NodeIndex sourceIndex = testSceneGraph.Find(std::string("B.K.L"));
                SceneGraph::NodeIndex foundIndex = testSceneGraph.GetNodeSibling(sourceIndex);
                EXPECT_FALSE(foundIndex.IsValid());
            }

            TEST_F(SceneGraphTests, GetNodeChild_EHasChild_ReturnsG)
            {
                SceneGraph::NodeIndex sourceIndex = testSceneGraph.Find(std::string("A.C.E"));
                SceneGraph::NodeIndex targetIndex = testSceneGraph.Find(std::string("A.C.E.G"));
                SceneGraph::NodeIndex foundIndex = testSceneGraph.GetNodeChild(sourceIndex);
                EXPECT_TRUE(foundIndex.IsValid());
                EXPECT_EQ(targetIndex, foundIndex);
            }

            TEST_F(SceneGraphTests, GetNodeChild_GNoChildren_NotValid)
            {
                SceneGraph::NodeIndex sourceIndex = testSceneGraph.Find(std::string("A.C.E.G"));
                SceneGraph::NodeIndex foundIndex = testSceneGraph.GetNodeChild(sourceIndex);
                EXPECT_FALSE(foundIndex.IsValid());
            }

            TEST_F(SceneGraphTests, GetNodeChild_ConvertToHierarchConvertToNodeIndex_ProducedIterator)
            {
                SceneGraph::NodeIndex sourceIndex = testSceneGraph.Find("A");

                SceneGraph::HierarchyStorageConstData::iterator storageIterator = testSceneGraph.ConvertToHierarchyIterator(sourceIndex);

                SceneGraph::NodeIndex nodeIndex = testSceneGraph.ConvertToNodeIndex(storageIterator);
                SceneGraph::NodeIndex testIndex = sourceIndex;
                EXPECT_EQ(nodeIndex, testIndex);
            }

            // GetNodeCount - continued
            TEST_F(SceneGraphTests, GetNodeCount_GetCountOfFilledTree_ReturnsNumberOfNodes)
            {
                EXPECT_EQ(totalNodeCount, testSceneGraph.GetNodeCount());
            }

            // Clear - continued
            TEST_F(SceneGraphTests, Clear_ClearFilledTree_ClearedWithDefaultAdded)
            {
                testSceneGraph.Clear();

                EXPECT_EQ(1, testSceneGraph.GetNodeCount());
                EXPECT_TRUE(testSceneGraph.GetRoot().IsValid());
                EXPECT_STREQ("", testSceneGraph.GetNodeName(testSceneGraph.GetRoot()).c_str());
                EXPECT_EQ(nullptr, testSceneGraph.GetNodeContent(testSceneGraph.GetRoot()));
            }

            /*
            The following APIs are not covered in this test implementation

            inline NodeIndex ConvertToNodeIndex(HierarchyStorageConstData::iterator iterator) const;
            inline NodeIndex ConvertToNodeIndex(NameStorageConstData::iterator iterator) const;
            inline NodeIndex ConvertToNodeIndex(ContentStorageData::iterator iterator) const;
            inline NodeIndex ConvertToNodeIndex(ContentStorageConstData::iterator iterator) const;

            inline HierarchyStorageConstData GetHierarchyStorage() const;
            inline NameStorageConstData GetNameStorage() const;
            inline ContentStorageData GetContentStorage();
            inline ContentStorageConstData GetContentStorage() const;
            */
        }
    }
}

