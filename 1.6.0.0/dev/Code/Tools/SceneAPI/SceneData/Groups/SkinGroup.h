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

#include <AzCore/Memory/Memory.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <SceneAPI/SceneCore/DataTypes/Groups/ISkinGroup.h>
#include <SceneAPI/SceneData/ManifestBase/SceneNodeSelectionList.h>

namespace AZ
{
    class ReflectContex;

    namespace SceneAPI
    {
        namespace DataTypes
        {
            IRule;
        }

        namespace SceneData
        {
            class SkinGroup : public DataTypes::ISkinGroup
            {
            public:
                AZ_RTTI(SkinGroup, "{A3217B13-79EA-4487-9A13-5D382EA9077A}", DataTypes::ISkinGroup);
                AZ_CLASS_ALLOCATOR_DECL

                DataTypes::ISceneNodeSelectionList& GetSceneNodeSelectionList() override;
                const DataTypes::ISceneNodeSelectionList& GetSceneNodeSelectionList() const override;

                size_t GetRuleCount() const override;
                AZStd::shared_ptr<AZ::SceneAPI::DataTypes::IRule> GetRule(size_t index) const override;
                void AddRule(const AZStd::shared_ptr<AZ::SceneAPI::DataTypes::IRule>& rule) override;
                void AddRule(AZStd::shared_ptr<AZ::SceneAPI::DataTypes::IRule>&& rule) override;
                void RemoveRule(size_t index) override;
                void RemoveRule(const AZStd::shared_ptr<AZ::SceneAPI::DataTypes::IRule>& rule) override;

                static void Reflect(AZ::ReflectContext* context);

            protected:
                AZStd::vector<AZStd::shared_ptr<DataTypes::IRule>> m_rules;
                SceneNodeSelectionList m_nodeSelectionList;
            };

        } // SceneData
    } // SceneAPI
} // AZ