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

#include <AzCore/std/string/string.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/RTTI/RTTI.h>
#include <SceneAPI/SceneCore/DataTypes/IManifestObject.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace DataTypes
        {
            class IRule;
            class IGroup
                : public IManifestObject
            {
            public:
                AZ_RTTI(IGroup, "{DE008E67-790D-4672-A73A-5CA0F31EDD2D}", IManifestObject);

                virtual ~IGroup() override = default;

                virtual size_t GetRuleCount() const = 0;
                virtual AZStd::shared_ptr<IRule> GetRule(size_t index) const = 0;
                virtual void AddRule(const AZStd::shared_ptr<IRule>& rule) = 0;
                virtual void AddRule(AZStd::shared_ptr<IRule>&& rule) = 0;
                virtual void RemoveRule(size_t index) = 0;
                virtual void RemoveRule(const AZStd::shared_ptr<IRule>& rule) = 0;
            };
        }  // DataTypes
    }  // SceneAPI
}  // AZ
