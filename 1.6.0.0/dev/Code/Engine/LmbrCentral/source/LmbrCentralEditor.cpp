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

#include "Precompiled.h"

#include "LmbrCentral.h"

#include "Animation/EditorAttachmentComponent.h"
#include "Physics/EditorConstraintComponent.h"
#include "Rendering/EditorDecalComponent.h"
#include "Scripting/EditorFlowGraphComponent.h"
#include "Rendering/EditorLensFlareComponent.h"
#include "Rendering/EditorLightComponent.h"
#include "Rendering/EditorStaticMeshComponent.h"
#include "Rendering/EditorSkinnedMeshComponent.h"
#include "Rendering/EditorParticleComponent.h"
#include "Animation/EditorSimpleAnimationComponent.h"
#include "Animation/EditorMannequinScopeComponent.h"
#include "Animation/EditorMannequinComponent.h"
#include "Scripting/EditorLookAtComponent.h"
#include "Scripting/EditorTagComponent.h"
#include "Scripting/EditorTriggerAreaComponent.h"

#include "Shape/EditorBoxShapeComponent.h"
#include "Shape/EditorSphereShapeComponent.h"
#include "Shape/EditorCylinderShapeComponent.h"
#include "Shape/EditorCapsuleShapeComponent.h"

namespace LmbrCentral
{
    /**
     * The LmbrCentralEditor module class extends the \ref LmbrCentralModule
     * by defining editor versions of many components.
     *
     * This is the module used when working in the Editor.
     * The \ref LmbrCentralModule is used by the standalone game.
     */
    class LmbrCentralEditorModule
        : public LmbrCentralModule
    {
    public:
        LmbrCentralEditorModule()
            : LmbrCentralModule()
        {
            m_descriptors.insert(m_descriptors.end(), {
                EditorAttachmentComponent::CreateDescriptor(),
                EditorConstraintComponent::CreateDescriptor(),
                EditorDecalComponent::CreateDescriptor(),
                EditorFlowGraphComponent::CreateDescriptor(),
                EditorLensFlareComponent::CreateDescriptor(),
                EditorLightComponent::CreateDescriptor(),
                EditorStaticMeshComponent::CreateDescriptor(),
                EditorSkinnedMeshComponent::CreateDescriptor(),
                EditorParticleComponent::CreateDescriptor(),
                EditorSimpleAnimationComponent::CreateDescriptor(),
                EditorTagComponent::CreateDescriptor(),
                EditorTriggerAreaComponent::CreateDescriptor(),
                EditorMannequinScopeComponent::CreateDescriptor(),
                EditorMannequinComponent::CreateDescriptor(),
                EditorSphereShapeComponent::CreateDescriptor(),
                EditorBoxShapeComponent::CreateDescriptor(),
                EditorLookAtComponent::CreateDescriptor(),
                EditorCylinderShapeComponent::CreateDescriptor(),
                EditorCapsuleShapeComponent::CreateDescriptor()
            });
        }
    };
} // namespace LmbrCentral

AZ_DECLARE_MODULE_CLASS(LmbrCentralEditor, LmbrCentral::LmbrCentralEditorModule)