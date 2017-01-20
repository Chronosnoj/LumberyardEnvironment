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

#include <AzCore/Asset/AssetCommon.h>

#include <smartptr.h>
#include <IStatObj.h>
#include <ICryAnimation.h>

namespace LmbrCentral
{
    class StaticMeshAsset
        : public AZ::Data::AssetData
    {
    public:
        using MeshPtr = _smart_ptr<IStatObj>;

        AZ_RTTI(StaticMeshAsset, "{C2869E3B-DDA0-4E01-8FE3-6770D788866B}", AZ::Data::AssetData);
        AZ_CLASS_ALLOCATOR(StaticMeshAsset, AZ::SystemAllocator, 0);

        /// The assigned static mesh instance.
        MeshPtr m_statObj = nullptr;
    };

    class SkinnedMeshAsset
        : public AZ::Data::AssetData
    {
    public:
        using CharacterPtr = _smart_ptr<ICharacterInstance>;

        AZ_RTTI(SkinnedMeshAsset, "{DF036C63-9AE6-4AC3-A6AC-8A1D76126C01}", AZ::Data::AssetData);
        AZ_CLASS_ALLOCATOR(SkinnedMeshAsset, AZ::SystemAllocator, 0);

        /// Character assets are unique per load.
        bool IsRegisterReadonlyAndShareable() override
        {
            return false;
        }

        /// The assigned skinned mesh instance.
        CharacterPtr m_characterInstance = nullptr;

    };
} // namespace LmbrCentral
