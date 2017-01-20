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

#include <SceneAPI/SceneData/ReflectionRegistrar.h>
#include <SceneAPI/SceneData/Groups/MeshGroup.h>
#include <SceneAPI/SceneData/Groups/SkeletonGroup.h>
#include <SceneAPI/SceneData/Groups/SkinGroup.h>
#include <SceneAPI/SceneData/Rules/CommentRule.h>
#include <SceneAPI/SceneData/Rules/MeshAdvancedRule.h>
#include <SceneAPI/SceneData/Rules/OriginRule.h>
#include <SceneAPI/SceneData/Rules/MaterialRule.h>
#include <SceneAPI/SceneData/Rules/PhysicsRule.h>
#include <SceneAPI/SceneData/ManifestBase/SceneNodeSelectionList.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace AZ
{
    namespace SceneAPI
    {
        void RegisterDataTypeReflection(AZ::SerializeContext* context)
        {
            // Groups
            SceneData::MeshGroup::Reflect(context);
            SceneData::SkeletonGroup::Reflect(context);
            SceneData::SkinGroup::Reflect(context);

            // Rules
            SceneData::CommentRule::Reflect(context);
            SceneData::MeshAdvancedRule::Reflect(context);
            SceneData::OriginRule::Reflect(context);
            SceneData::MaterialRule::Reflect(context);
            SceneData::PhysicsRule::Reflect(context);

            // Utility
            SceneData::SceneNodeSelectionList::Reflect(context);
        }
    }
}
