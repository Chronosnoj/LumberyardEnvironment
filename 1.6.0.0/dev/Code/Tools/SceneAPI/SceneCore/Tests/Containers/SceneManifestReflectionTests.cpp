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
#include <AzCore/IO/GenericStreams.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Serialization/ObjectStream.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <SceneAPI/SceneCore/Containers/SceneManifest.h>
#include <SceneAPI/SceneCore/Containers/SceneManifestReflection.h>
#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ManifestBool.h>
#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ManifestInt.h>
#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ManifestDouble.h>
#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ManifestString.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            struct ManifestWrapper
            {
                AZ_RTTI(ManifestWrapper, "{1062B620-071F-4982-84F7-828EDAFC3D36}");
                AZ_CLASS_ALLOCATOR(ManifestWrapper, AZ::SystemAllocator, 0)

                static void Reflect(AZ::ReflectContext* context)
                {
                    AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
                    if (serializeContext)
                    {
                        serializeContext->
                            Class<ManifestWrapper>()->
                            Version(1)->
                            Field("manifest", &ManifestWrapper::m_manifest);
                    }
                }

                SceneManifest m_manifest;
            };

            struct SceneManifestReflectionTest
                : public ::testing::Test
            {
                SceneManifestReflectionTest()
                    : m_memoryStream(AZ::IO::MemoryStream(m_buffer, sizeof(m_buffer), 0))
                {
                    DataTypes::ManifestBool::Reflect(&m_context);
                    DataTypes::ManifestInt::Reflect(&m_context);
                    DataTypes::ManifestDouble::Reflect(&m_context);
                    DataTypes::ManifestString::Reflect(&m_context);

                    ManifestWrapper::Reflect(&m_context);
                }

                char m_buffer[16 * 1024];
                AZ::SerializeContext m_context;
                AZ::IO::MemoryStream m_memoryStream;
                ManifestWrapper m_manifest;
            };

            // Serialization
            TEST_F(SceneManifestReflectionTest, Serialization_EmptyManifest_SavingSuccessful)
            {
                bool result = AZ::Utils::SaveObjectToStream(m_memoryStream, AZ::ObjectStream::ST_JSON, &m_manifest, &m_context);
                EXPECT_TRUE(result);
            }

            TEST_F(SceneManifestReflectionTest, Serialization_ManifestWithBool_SavingSuccessful)
            {
                m_manifest.m_manifest.AddEntry("Test", AZStd::make_shared<DataTypes::ManifestBool>(true));

                bool result = AZ::Utils::SaveObjectToStream(m_memoryStream, AZ::ObjectStream::ST_JSON, &m_manifest, &m_context);
                EXPECT_TRUE(result);
            }

            TEST_F(SceneManifestReflectionTest, Serialization_ManifestWithInt_SavingSuccessful)
            {
                m_manifest.m_manifest.AddEntry("Test", AZStd::make_shared<DataTypes::ManifestInt>(42));

                bool result = AZ::Utils::SaveObjectToStream(m_memoryStream, AZ::ObjectStream::ST_JSON, &m_manifest, &m_context);
                EXPECT_TRUE(result);
            }

            TEST_F(SceneManifestReflectionTest, Serialization_ManifestWithDouble_SavingSuccessful)
            {
                m_manifest.m_manifest.AddEntry("Test", AZStd::make_shared<DataTypes::ManifestDouble>(42.0));

                bool result = AZ::Utils::SaveObjectToStream(m_memoryStream, AZ::ObjectStream::ST_JSON, &m_manifest, &m_context);
                EXPECT_TRUE(result);
            }

            TEST_F(SceneManifestReflectionTest, Serialization_ManifestWithString_SavingSuccessful)
            {
                m_manifest.m_manifest.AddEntry("Test", AZStd::make_shared<DataTypes::ManifestString>("TestValue"));

                bool result = AZ::Utils::SaveObjectToStream(m_memoryStream, AZ::ObjectStream::ST_JSON, &m_manifest, &m_context);
                EXPECT_TRUE(result);
            }

            TEST_F(SceneManifestReflectionTest, Serialization_ManifestWithMultipleEntries_SavingSuccessful)
            {
                m_manifest.m_manifest.AddEntry("Test0", AZStd::make_shared<DataTypes::ManifestBool>(true));
                m_manifest.m_manifest.AddEntry("Test1", AZStd::make_shared<DataTypes::ManifestInt>(42));
                m_manifest.m_manifest.AddEntry("Test2", AZStd::make_shared<DataTypes::ManifestDouble>(42.0));
                m_manifest.m_manifest.AddEntry("Test3", AZStd::make_shared<DataTypes::ManifestString>("TestValue"));

                bool result = AZ::Utils::SaveObjectToStream(m_memoryStream, AZ::ObjectStream::ST_JSON, &m_manifest, &m_context);
                EXPECT_TRUE(result);
            }

            // Deserialization
            TEST_F(SceneManifestReflectionTest, Deserialization_EmptyManifest_SavingSuccessful)
            {
                bool result = AZ::Utils::SaveObjectToStream(m_memoryStream, AZ::ObjectStream::ST_JSON, &m_manifest, &m_context);
                ASSERT_TRUE(result);

                m_memoryStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
                ManifestWrapper* loaded = AZ::Utils::LoadObjectFromStream<ManifestWrapper>(m_memoryStream, &m_context);
                EXPECT_NE(nullptr, loaded);
                delete loaded;
            }

            TEST_F(SceneManifestReflectionTest, Deserialization_ManifestWithBool_SavingSuccessful)
            {
                m_manifest.m_manifest.AddEntry("Test", AZStd::make_shared<DataTypes::ManifestBool>(true));

                bool result = AZ::Utils::SaveObjectToStream(m_memoryStream, AZ::ObjectStream::ST_JSON, &m_manifest, &m_context);
                ASSERT_TRUE(result);

                m_memoryStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
                ManifestWrapper* loaded = AZ::Utils::LoadObjectFromStream<ManifestWrapper>(m_memoryStream, &m_context);
                ASSERT_NE(nullptr, loaded);

                ASSERT_NE(SceneManifest::s_invalidIndex, loaded->m_manifest.FindIndex("Test"));

                AZStd::shared_ptr<DataTypes::ManifestBool> value = azrtti_cast<DataTypes::ManifestBool*>(loaded->m_manifest.FindValue("Test"));
                ASSERT_NE(nullptr, value);
                EXPECT_EQ(true, value->GetValue());
                delete loaded;
            }

            TEST_F(SceneManifestReflectionTest, Deserialization_ManifestWithInt_SavingSuccessful)
            {
                m_manifest.m_manifest.AddEntry("Test", AZStd::make_shared<DataTypes::ManifestInt>(42));

                bool result = AZ::Utils::SaveObjectToStream(m_memoryStream, AZ::ObjectStream::ST_JSON, &m_manifest, &m_context);
                ASSERT_TRUE(result);

                m_memoryStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
                ManifestWrapper* loaded = AZ::Utils::LoadObjectFromStream<ManifestWrapper>(m_memoryStream, &m_context);
                ASSERT_NE(nullptr, loaded);

                ASSERT_NE(SceneManifest::s_invalidIndex, loaded->m_manifest.FindIndex("Test"));

                AZStd::shared_ptr<DataTypes::ManifestInt> value = azrtti_cast<DataTypes::ManifestInt*>(loaded->m_manifest.FindValue("Test"));
                ASSERT_NE(nullptr, value);
                EXPECT_EQ(42, value->GetValue());
                delete loaded;
            }

            TEST_F(SceneManifestReflectionTest, Deserialization_ManifestWithDouble_SavingSuccessful)
            {
                m_manifest.m_manifest.AddEntry("Test", AZStd::make_shared<DataTypes::ManifestDouble>(42.0));

                bool result = AZ::Utils::SaveObjectToStream(m_memoryStream, AZ::ObjectStream::ST_JSON, &m_manifest, &m_context);
                ASSERT_TRUE(result);

                m_memoryStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
                ManifestWrapper* loaded = AZ::Utils::LoadObjectFromStream<ManifestWrapper>(m_memoryStream, &m_context);
                ASSERT_NE(nullptr, loaded);

                ASSERT_NE(SceneManifest::s_invalidIndex, loaded->m_manifest.FindIndex("Test"));

                AZStd::shared_ptr<DataTypes::ManifestDouble> value = azrtti_cast<DataTypes::ManifestDouble*>(loaded->m_manifest.FindValue("Test"));
                ASSERT_NE(nullptr, value);
                EXPECT_EQ(42.0, value->GetValue());
                delete loaded;
            }

            TEST_F(SceneManifestReflectionTest, Deserialization_ManifestWithString_SavingSuccessful)
            {
                m_manifest.m_manifest.AddEntry("Test", AZStd::make_shared<DataTypes::ManifestString>("TestValue"));

                bool result = AZ::Utils::SaveObjectToStream(m_memoryStream, AZ::ObjectStream::ST_JSON, &m_manifest, &m_context);
                ASSERT_TRUE(result);

                m_memoryStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
                ManifestWrapper* loaded = AZ::Utils::LoadObjectFromStream<ManifestWrapper>(m_memoryStream, &m_context);
                ASSERT_NE(nullptr, loaded);

                ASSERT_NE(SceneManifest::s_invalidIndex, loaded->m_manifest.FindIndex("Test"));

                AZStd::shared_ptr<DataTypes::ManifestString> value = azrtti_cast<DataTypes::ManifestString*>(loaded->m_manifest.FindValue("Test"));
                ASSERT_NE(nullptr, value);
                EXPECT_STREQ("TestValue", value->GetValue().c_str());
                delete loaded;
            }

            TEST_F(SceneManifestReflectionTest, Deserialization_ManifestWithMultipleEntries_SavingSuccessful)
            {
                m_manifest.m_manifest.AddEntry("Test0", AZStd::make_shared<DataTypes::ManifestBool>(true));
                m_manifest.m_manifest.AddEntry("Test1", AZStd::make_shared<DataTypes::ManifestInt>(42));
                m_manifest.m_manifest.AddEntry("Test2", AZStd::make_shared<DataTypes::ManifestDouble>(42.0));
                m_manifest.m_manifest.AddEntry("Test3", AZStd::make_shared<DataTypes::ManifestString>("TestValue"));

                bool result = AZ::Utils::SaveObjectToStream(m_memoryStream, AZ::ObjectStream::ST_JSON, &m_manifest, &m_context);
                ASSERT_TRUE(result);

                m_memoryStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
                ManifestWrapper* loaded = AZ::Utils::LoadObjectFromStream<ManifestWrapper>(m_memoryStream, &m_context);
                ASSERT_NE(nullptr, loaded);

                ASSERT_NE(SceneManifest::s_invalidIndex, loaded->m_manifest.FindIndex("Test0"));
                AZStd::shared_ptr<DataTypes::ManifestBool> value0 = azrtti_cast<DataTypes::ManifestBool*>(loaded->m_manifest.FindValue("Test0"));
                ASSERT_NE(nullptr, value0);
                EXPECT_EQ(true, value0->GetValue());

                ASSERT_NE(SceneManifest::s_invalidIndex, loaded->m_manifest.FindIndex("Test1"));
                AZStd::shared_ptr<DataTypes::ManifestInt> value1 = azrtti_cast<DataTypes::ManifestInt*>(loaded->m_manifest.FindValue("Test1"));
                ASSERT_NE(nullptr, value1);
                EXPECT_EQ(42, value1->GetValue());

                ASSERT_NE(SceneManifest::s_invalidIndex, loaded->m_manifest.FindIndex("Test2"));
                AZStd::shared_ptr<DataTypes::ManifestDouble> value2 = azrtti_cast<DataTypes::ManifestDouble*>(loaded->m_manifest.FindValue("Test2"));
                ASSERT_NE(nullptr, value2);
                EXPECT_EQ(42.0, value2->GetValue());

                ASSERT_NE(SceneManifest::s_invalidIndex, loaded->m_manifest.FindIndex("Test3"));
                AZStd::shared_ptr<DataTypes::ManifestString> value3 = azrtti_cast<DataTypes::ManifestString*>(loaded->m_manifest.FindValue("Test3"));
                ASSERT_NE(nullptr, value3);
                EXPECT_STREQ("TestValue", value3->GetValue().c_str());
                delete loaded;
            }
        } // Containers
    } // SceneAPI
} // AZ