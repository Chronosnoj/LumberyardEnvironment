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

#include <AzCore/RTTI/TypeInfo.h>
#include <AzFramework/Asset/SimpleAsset.h>

namespace LmbrCentral
{
    /*!
     * Material asset type configuration.
     * Reflect as: AzFramework::SimpleAssetReference<BehaviorTreeAsset>
     */
    class BehaviorTreeAsset
    {
    public:
        AZ_TYPE_INFO(BehaviorTreeAsset, "{0DB1F34B-EB30-4318-A20B-CF035F419E74}")
        static const char* GetFileFilter() 
        {
            return "*.xml";
        }
    };
} // namespace LmbrCentral

namespace AZ
{
    AZ_TYPE_INFO_SPECIALIZE(AzFramework::SimpleAssetReference<LmbrCentral::BehaviorTreeAsset>, "{3CF544A7-7F15-4B47-BB33-9481C8A03055}")
}
