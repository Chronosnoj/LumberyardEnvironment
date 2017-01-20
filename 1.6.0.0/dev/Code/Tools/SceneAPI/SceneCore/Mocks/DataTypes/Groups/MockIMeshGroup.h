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
#pragma once

#include <AzTest/AzTest.h>

#include <SceneAPI/SceneCore/DataTypes/Groups/IMeshGroup.h>
#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ISceneNodeSelectionList.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace DataTypes
        {
            class MockIMeshGroup
                : public IMeshGroup
            {
            public:
                AZ_RTTI(MockIMeshGroup, "{AB119B84-E6D8-4CF3-9456-E44B23EDCDFE}", IMeshGroup);

                // IMeshGroup
                MOCK_METHOD0(GetSceneNodeSelectionList,
                    ISceneNodeSelectionList&());
                MOCK_CONST_METHOD0(GetSceneNodeSelectionList,
                    const ISceneNodeSelectionList&());

                // IGroup
                MOCK_CONST_METHOD0(GetRuleCount,
                    size_t());
                MOCK_CONST_METHOD1(GetRule,
                    AZStd::shared_ptr<IRule>(size_t index));
                MOCK_METHOD1(AddRule,
                    void(const AZStd::shared_ptr<IRule>&rule));
                MOCK_METHOD1(RemoveRule,
                    void(size_t index));
                MOCK_METHOD1(RemoveRule,
                    void(const AZStd::shared_ptr<IRule>&rule));

                void AddRule(AZStd::shared_ptr<IRule>&& rule) override
                {
                    AddRule(rule);
                }
            };
        }  // namespace DataTypes
    }  // namespace SceneAPI
}  // namespace AZ
