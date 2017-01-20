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

#include <AzTest/AzTest.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <SceneAPI/SceneCore/Containers/SceneManifest.h>
#include <SceneAPI/SceneCore/DataTypes/IManifestObject.h>
#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ManifestInt.h>
#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ManifestDouble.h>
#include <string>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            class SceneManifestTest
                : public ::testing::Test
            {
            public:
                SceneManifestTest()
                {
                    m_firstDataObject = AZStd::make_shared<DataTypes::ManifestInt>(1);
                    m_secondDataObject = AZStd::make_shared<DataTypes::ManifestDouble>(10.0f);
                    m_testDataObject = AZStd::make_shared<DataTypes::ManifestDouble>(20.0f);

                    m_testManifest.AddEntry("TestEntry1", m_firstDataObject);
                    m_testManifest.AddEntry("TestEntry2", m_secondDataObject);
                    m_testManifest.AddEntry("TestEntry3", m_testDataObject);

                    DataTypes::ManifestInt::Reflect(&m_context);
                    DataTypes::ManifestDouble::Reflect(&m_context);
                }

                AZStd::shared_ptr<DataTypes::ManifestInt> m_firstDataObject;
                AZStd::shared_ptr<DataTypes::ManifestDouble> m_secondDataObject;
                AZStd::shared_ptr<DataTypes::ManifestDouble> m_testDataObject;
                SceneManifest m_testManifest;
                SerializeContext m_context;
            };

            TEST(SceneManifest, IsEmpty_Empty_True)
            {
                SceneManifest testManifest;
                EXPECT_TRUE(testManifest.IsEmpty());
            }

            TEST(SceneManifest, AddEntry_CharNameInt_ResultTrue)
            {
                SceneManifest testManifest;
                AZStd::shared_ptr<DataTypes::ManifestInt> testDataObject = AZStd::make_shared<DataTypes::ManifestInt>(100);
                bool result = testManifest.AddEntry("TestEntry", AZStd::move(testDataObject));
                EXPECT_TRUE(result);
            }

            //dependent on AddEntry
            TEST(SceneManifest, IsEmpty_NotEmpty_False)
            {
                SceneManifest testManifest;
                AZStd::shared_ptr<DataTypes::ManifestInt> testDataObject = AZStd::make_shared<DataTypes::ManifestInt>(100);
                bool result = testManifest.AddEntry("TestEntry", AZStd::move(testDataObject));
                EXPECT_FALSE(testManifest.IsEmpty());
            }

            //dependent on AddEntry and IsEmpty
            TEST(SceneManifest, Clear_NotEmpty_EmptyTrue)
            {
                SceneManifest testManifest;
                AZStd::shared_ptr<DataTypes::ManifestInt> testDataObject = AZStd::make_shared<DataTypes::ManifestInt>(100);
                bool result = testManifest.AddEntry("TestEntry", AZStd::move(testDataObject));
                EXPECT_FALSE(testManifest.IsEmpty());
                testManifest.Clear();
                EXPECT_TRUE(testManifest.IsEmpty());
            }

            TEST(SceneManifest, AddEntry_EmptyCharNameInt_ResultTrue)
            {
                SceneManifest testManifest;
                AZStd::shared_ptr<DataTypes::ManifestInt> testDataObject = AZStd::make_shared<DataTypes::ManifestInt>(100);
                bool result = testManifest.AddEntry("", AZStd::move(testDataObject));
                EXPECT_TRUE(result);
            }

            TEST(SceneManifest, AddEntry_StringNameInt_ResultTrue)
            {
                SceneManifest testManifest;
                AZStd::shared_ptr<DataTypes::ManifestInt> testDataObject = AZStd::make_shared<DataTypes::ManifestInt>(100);
                bool result = testManifest.AddEntry(AZStd::string("TestEntry"), AZStd::move(testDataObject));
                EXPECT_TRUE(result);
            }

            TEST(SceneManifest, AddEntry_DuplicateStringNameInt_ResultFalse)
            {
                SceneManifest testManifest;
                AZStd::shared_ptr<DataTypes::ManifestInt> testDataObject = AZStd::make_shared<DataTypes::ManifestInt>(1);
                testManifest.AddEntry(AZStd::string("TestEntry"), AZStd::move(testDataObject));

                testDataObject = AZStd::make_shared<DataTypes::ManifestInt>(2);
                bool result = testManifest.AddEntry(AZStd::string("TestEntry"), AZStd::move(testDataObject));
                EXPECT_FALSE(result);
            }

            TEST(SceneManifest, AddEntry_RValueNameInt_ResultTrue)
            {
                AZStd::string name("TestEntry");
                SceneManifest testManifest;
                AZStd::shared_ptr<DataTypes::ManifestInt> testDataObject = AZStd::make_shared<DataTypes::ManifestInt>(1);
                bool result = testManifest.AddEntry(AZStd::move(name), AZStd::move(testDataObject));

                EXPECT_TRUE(result);
            }





            //FindValues
            TEST(SceneManifest, FindValue_CharptrNameInList_ResultValid)
            {
                SceneManifest testManifest;
                AZStd::shared_ptr<DataTypes::ManifestInt> testDataObject = AZStd::make_shared<DataTypes::ManifestInt>(1);
                testManifest.AddEntry("TestEntry", AZStd::move(testDataObject));

                AZStd::shared_ptr<DataTypes::IManifestObject> resultObject = testManifest.FindValue("TestEntry");
                EXPECT_NE(nullptr, resultObject);
            }

            TEST(SceneManifest, FindValue_NameInList_ResultValid)
            {
                SceneManifest testManifest;
                AZStd::shared_ptr<DataTypes::ManifestInt> testDataObject = AZStd::make_shared<DataTypes::ManifestInt>(1);
                testManifest.AddEntry("TestEntry", AZStd::move(testDataObject));

                AZStd::shared_ptr<DataTypes::IManifestObject> resultObject = testManifest.FindValue(AZStd::string("TestEntry"));
                EXPECT_NE(nullptr, resultObject);
            }

            TEST(SceneManifest, FindValue_NameNotInList_ResultNotValid)
            {
                SceneManifest testManifest;
                AZStd::shared_ptr<DataTypes::ManifestInt> testDataObject = AZStd::make_shared<DataTypes::ManifestInt>(1);
                testManifest.AddEntry("TestEntry", AZStd::move(testDataObject));

                AZStd::shared_ptr<DataTypes::IManifestObject> resultObject = testManifest.FindValue(AZStd::string("NotTestEntry"));
                EXPECT_EQ(nullptr, resultObject);
            }

            TEST(SceneManifest, FindValueConst_CharPtrNameInList_ResultValid)
            {
                SceneManifest testManifest;
                const SceneManifest& testRefManifest = testManifest;
                AZStd::shared_ptr<DataTypes::ManifestInt> testDataObject = AZStd::make_shared<DataTypes::ManifestInt>(1);
                testManifest.AddEntry("TestEntry", AZStd::move(testDataObject));

                AZStd::shared_ptr<const DataTypes::IManifestObject> resultObject = testRefManifest.FindValue("TestEntry");
                EXPECT_NE(nullptr, resultObject);
            }

            TEST(SceneManifest, FindValueConst_CharPtrNameNotInList_ResultNotValid)
            {
                SceneManifest testManifest;
                const SceneManifest& testRefManifest = testManifest;
                AZStd::shared_ptr<DataTypes::ManifestInt> testDataObject = AZStd::make_shared<DataTypes::ManifestInt>(1);
                testManifest.AddEntry("TestEntry", AZStd::move(testDataObject));

                AZStd::shared_ptr<const DataTypes::IManifestObject> resultObject = testRefManifest.FindValue("NotTestEntry");
                EXPECT_EQ(nullptr, resultObject);
            }

            TEST(SceneManifest, FindValueConst_NameInList_ResultValid)
            {
                SceneManifest testManifest;
                AZStd::shared_ptr<DataTypes::ManifestInt> testDataObject = AZStd::make_shared<DataTypes::ManifestInt>(1);
                testManifest.AddEntry("TestEntry", AZStd::move(testDataObject));

                AZStd::shared_ptr<const DataTypes::IManifestObject> resultObject = testManifest.FindValue(AZStd::string("TestEntry"));
                EXPECT_NE(nullptr, resultObject);
            }

            TEST(SceneManifest, FindValueConst_NameNotInList_ResultNotValid)
            {
                SceneManifest testManifest;
                AZStd::shared_ptr<DataTypes::ManifestInt> testDataObject = AZStd::make_shared<DataTypes::ManifestInt>(1);
                testManifest.AddEntry("TestEntry", AZStd::move(testDataObject));

                AZStd::shared_ptr<const DataTypes::IManifestObject> resultObject = testManifest.FindValue(AZStd::string("NotTestEntry"));
                EXPECT_EQ(nullptr, resultObject);
            }

            //RemoveEntry
            TEST(SceneManifest, RemoveEntry_NameInList_ResultTrueAndNotStillInList)
            {
                SceneManifest testManifest;
                AZStd::shared_ptr<DataTypes::ManifestInt> testDataObject = AZStd::make_shared<DataTypes::ManifestInt>(1);
                testManifest.AddEntry("TestEntry", AZStd::move(testDataObject));

                bool result = testManifest.RemoveEntry(AZStd::string("TestEntry"));
                EXPECT_TRUE(result);
                AZStd::shared_ptr<DataTypes::IManifestObject> resultObject = testManifest.FindValue(AZStd::string("TestEntry"));
                EXPECT_EQ(nullptr, resultObject);
            }

            TEST(SceneManifest, RemoveEntry_NameNotInList_ResultFalse)
            {
                SceneManifest testManifest;
                AZStd::shared_ptr<DataTypes::ManifestInt> testDataObject = AZStd::make_shared<DataTypes::ManifestInt>(1);
                testManifest.AddEntry("TestEntry", AZStd::move(testDataObject));

                bool result = testManifest.RemoveEntry(AZStd::string("NotTestEntry"));
                EXPECT_FALSE(result);
            }

            TEST_F(SceneManifestTest, FindValue_AfterRemoveFirstEntryInList_ResultTrueFindSecondEntryByNameSuccess)
            {
                SceneManifest testManifest;

                AZStd::shared_ptr<DataTypes::ManifestInt> firstDataObject = AZStd::make_shared<DataTypes::ManifestInt>(1);
                testManifest.AddEntry("TestEntry1", AZStd::move(firstDataObject));

                AZStd::shared_ptr<DataTypes::ManifestDouble> secondDataObject = AZStd::make_shared<DataTypes::ManifestDouble>(10.0);
                testManifest.AddEntry("TestEntry2", AZStd::move(secondDataObject));

                AZStd::shared_ptr<DataTypes::ManifestDouble> testDataObject = AZStd::make_shared<DataTypes::ManifestDouble>(20.0);
                testManifest.AddEntry("TestEntry3", AZStd::move(testDataObject));

                bool result = testManifest.RemoveEntry(AZStd::string("TestEntry1"));
                EXPECT_TRUE(result);

                AZStd::shared_ptr<DataTypes::IManifestObject> resultObject = testManifest.FindValue(AZStd::string("TestEntry2"));
                ASSERT_TRUE(resultObject);

                AZStd::shared_ptr<DataTypes::ManifestDouble> doubleObject = azrtti_cast<DataTypes::ManifestDouble*>(resultObject);
                ASSERT_TRUE(doubleObject);
                EXPECT_EQ(10.0, doubleObject->GetValue());
            }

            // GetEntryCount
            TEST(SceneManifest, GetEntryCount_EmptyManifest_CountIsZero)
            {
                SceneManifest testManifest;
                EXPECT_TRUE(testManifest.IsEmpty());
                EXPECT_EQ(0, testManifest.GetEntryCount());
            }

            TEST_F(SceneManifestTest, GetEntryCount_FilledManifest_CountIsThree)
            {
                EXPECT_EQ(3, m_testManifest.GetEntryCount());
            }

            // GetName
            TEST_F(SceneManifestTest, GetName_ValidIndex_ResultIsStringTestEntry2)
            {
                EXPECT_STREQ("TestEntry2", m_testManifest.GetName(1).c_str());
            }

            TEST_F(SceneManifestTest, GetName_InvalidIndex_ReturnsEmptyString)
            {
                EXPECT_STREQ("", m_testManifest.GetName(42).c_str());
            }

            // GetValue
            TEST_F(SceneManifestTest, GetValue_ValidIndex_ReturnsFloat10)
            {
                AZStd::shared_ptr<DataTypes::ManifestDouble> result = azrtti_cast<DataTypes::ManifestDouble*>(m_testManifest.GetValue(1));
                ASSERT_TRUE(result);
                EXPECT_EQ(10.0, result->GetValue());
            }

            TEST_F(SceneManifestTest, GetValue_InvalidIndex_ReturnsNullPtr)
            {
                EXPECT_EQ(nullptr, m_testManifest.GetValue(42));
            }

            // FindIndex
            TEST_F(SceneManifestTest, FindIndex_ValidNameByCharPtr_ResultIsOne)
            {
                EXPECT_EQ(1, m_testManifest.FindIndex("TestEntry2"));
            }

            TEST_F(SceneManifestTest, FindIndex_ValidNameByString_ResultIsOne)
            {
                EXPECT_EQ(1, m_testManifest.FindIndex(AZStd::string("TestEntry2")));
            }

            TEST_F(SceneManifestTest, FindIndex_InvalidNameByCharPtr_ResultIsInvalidIndex)
            {
                EXPECT_EQ(SceneManifest::s_invalidIndex, m_testManifest.FindIndex("Invalid"));
            }

            TEST_F(SceneManifestTest, FindIndex_InvalidNameByCharNullPtr_ResultIsInvalidIndex)
            {
                EXPECT_EQ(SceneManifest::s_invalidIndex, m_testManifest.FindIndex(static_cast<const char*>(nullptr)));
            }

            TEST_F(SceneManifestTest, FindIndex_InvalidNameByString_ResultIsInvalidIndex)
            {
                EXPECT_EQ(SceneManifest::s_invalidIndex, m_testManifest.FindIndex(AZStd::string("Invalid")));
            }

            TEST_F(SceneManifestTest, FindIndex_ValidValue_ResultIsOne)
            {
                EXPECT_EQ(1, m_testManifest.FindIndex(m_secondDataObject.get()));
            }

            TEST_F(SceneManifestTest, FindIndex_InvalidValueFromSharedPtr_ResultIsInvalidIndex)
            {
                AZStd::shared_ptr<DataTypes::IManifestObject> invalid = AZStd::make_shared<DataTypes::ManifestInt>(42);
                EXPECT_EQ(SceneManifest::s_invalidIndex, m_testManifest.FindIndex(invalid.get()));
            }

            TEST_F(SceneManifestTest, FindIndex_InvalidValueFromNullptr_ResultIsInvalidIndex)
            {
                DataTypes::IManifestObject* invalid = nullptr;
                EXPECT_EQ(SceneManifest::s_invalidIndex, m_testManifest.FindIndex(invalid));
            }

            // SaveToStream
            TEST_F(SceneManifestTest, SaveToStream_SaveFilledManifestToMemoryStream_ReturnsTrue)
            {
                char buffer[64 * 1024];
                IO::MemoryStream memoryStream(IO::MemoryStream(buffer, sizeof(buffer), 0));
                bool result = m_testManifest.SaveToStream(memoryStream, &m_context);
                EXPECT_TRUE(result);
            }

            TEST_F(SceneManifestTest, SaveToStream_SaveEmptyManifestToMemoryStream_ReturnsTrue)
            {
                char buffer[64 * 1024];
                IO::MemoryStream memoryStream(IO::MemoryStream(buffer, sizeof(buffer), 0));
                
                SceneManifest empty;
                bool result = empty.SaveToStream(memoryStream, &m_context);
                EXPECT_TRUE(result);
            }

            // LoadFromStream
            TEST_F(SceneManifestTest, LoadFromStream_LoadEmptyManifestFromMemoryStream_ReturnsTrue)
            {
                char buffer[64 * 1024];
                IO::MemoryStream memoryStream(IO::MemoryStream(buffer, sizeof(buffer), 0));

                SceneManifest empty;
                bool result = empty.SaveToStream(memoryStream, &m_context);
                ASSERT_TRUE(result);

                memoryStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);

                SceneManifest loaded;
                result = loaded.LoadFromStream(memoryStream, &m_context);
                EXPECT_TRUE(result);
                EXPECT_TRUE(loaded.IsEmpty());
            }

            TEST_F(SceneManifestTest, LoadFromStream_LoadFilledManifestFromMemoryStream_ReturnsTrue)
            {
                char buffer[64 * 1024];
                IO::MemoryStream memoryStream(IO::MemoryStream(buffer, sizeof(buffer), 0));
                bool result = m_testManifest.SaveToStream(memoryStream, &m_context);
                ASSERT_TRUE(result);

                memoryStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);

                SceneManifest loaded;
                result = loaded.LoadFromStream(memoryStream, &m_context);
                EXPECT_TRUE(result);
                
                ASSERT_EQ(loaded.GetEntryCount(), m_testManifest.GetEntryCount());
                for (size_t i = 0; i < loaded.GetEntryCount(); ++i)
                {
                    EXPECT_STRCASEEQ(loaded.GetName(i).c_str(), m_testManifest.GetName(i).c_str());
                }
            }
        }
    }
}
