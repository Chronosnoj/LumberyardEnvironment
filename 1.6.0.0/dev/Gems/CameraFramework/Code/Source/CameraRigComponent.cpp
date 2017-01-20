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
#include "StdAfx.h"
#include "CameraRigComponent.h"
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Component/Entity.h>
#include "CameraFramework/ICameraTargetAcquirer.h"
#include "CameraFramework/ICameraLookAtBehavior.h"
#include "CameraFramework/ICameraTransformBehavior.h"

namespace Camera
{
    void CameraRigComponent::Init()
    {
        for (ICameraTargetAcquirer* targetAcquirer : m_targetAcquirers)
        {
            targetAcquirer->Init();
        }
        for (ICameraLookAtBehavior* lookAtBehavior : m_lookAtBehaviors)
        {
            lookAtBehavior->Init();
        }
        for (ICameraTransformBehavior* transformBehavior : m_transformBehaviors)
        {
            transformBehavior->Init();
        }
    }

    void CameraRigComponent::Activate()
    {
        for (ICameraTargetAcquirer* targetAcquirer : m_targetAcquirers)
        {
            targetAcquirer->Activate(GetEntityId());
        }
        for (ICameraLookAtBehavior* lookAtBehavior : m_lookAtBehaviors)
        {
            lookAtBehavior->Activate(GetEntityId());
        }
        for (ICameraTransformBehavior* transformBehavior : m_transformBehaviors)
        {
            transformBehavior->Activate(GetEntityId());
        }

        if (GetEntity()->GetTransform())
        {
            m_initialTransform = GetEntity()->GetTransform()->GetWorldTM();
        }

        AZ::TickBus::Handler::BusConnect();
    }

    void CameraRigComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();

        for (ICameraTargetAcquirer* targetAcquirer : m_targetAcquirers)
        {
            targetAcquirer->Deactivate();
        }
        for (ICameraLookAtBehavior* lookAtBehavior : m_lookAtBehaviors)
        {
            lookAtBehavior->Deactivate();
        }
        for (ICameraTransformBehavior* transformBehavior : m_transformBehaviors)
        {
            transformBehavior->Deactivate();
        }
    }

    void CameraRigComponent::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<CameraRigComponent, AZ::Component>()
                ->Version(1)
                ->Field("Target Acquirers", &CameraRigComponent::m_targetAcquirers)
                ->Field("Look-at Behaviors", &CameraRigComponent::m_lookAtBehaviors)
                ->Field("Camera Transform Behaviors", &CameraRigComponent::m_transformBehaviors);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<CameraRigComponent>("Camera Rig", "The Camera Rig drives the Camera and defines its behavior")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Game")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/CameraRig.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/CameraRig.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                    ->DataElement(0, &CameraRigComponent::m_targetAcquirers, "Target acquirers",
                    "A list of behaviors that define how a camera will select a target.  They are executed in order until one succeeds")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(0, &CameraRigComponent::m_lookAtBehaviors, "Look-at behaviors",
                    "A list of look-at behaviors.  They are run in order, each having the chance to sequentially modify the look-at target transform")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(0, &CameraRigComponent::m_transformBehaviors, "Transform behaviors",
                    "A list of behaviors that run in order, each having the chance to sequentially modify the camera's transform based on the look-at transform")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true);
            }
        }
    }

    void CameraRigComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("CameraRigService"));
    }

    void CameraRigComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC("TransformService"));
    }

    void CameraRigComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        dependent.push_back(AZ_CRC("CameraService"));
    }

    void CameraRigComponent::OnTick(float deltaTime, AZ::ScriptTimePoint time)
    {
        // Step 1 Acquire a target
        AZ::Transform initialCameraTransform = AZ::Transform::Identity();
        EBUS_EVENT_ID_RESULT(initialCameraTransform, GetEntityId(), AZ::TransformBus, GetWorldTM);
        AZ::Transform targetTransform(m_initialTransform);
        for (ICameraTargetAcquirer* targetAcquirer : m_targetAcquirers)
        {
            if (targetAcquirer->AcquireTarget(targetTransform))
            {
                break;
            }
        }

        // Step 2 modify the target look at position
        AZ::Transform lookAtTargetTransform(targetTransform);
        for (ICameraLookAtBehavior* CameraLookAtBehavior : m_lookAtBehaviors)
        {
            CameraLookAtBehavior->AdjustLookAtTarget(deltaTime, targetTransform, lookAtTargetTransform);
        }

        // Step 3 modify the camera's position
        AZ::Transform finalTransform(initialCameraTransform);
        for (ICameraTransformBehavior* cameraTransformBehavior : m_transformBehaviors)
        {
            cameraTransformBehavior->AdjustCameraTransform(deltaTime, initialCameraTransform, lookAtTargetTransform, finalTransform);
        }

        // Step 4 Alert the camera component of the new desired transform
        EBUS_EVENT_ID(GetEntityId(), AZ::TransformBus, SetWorldTM, finalTransform);
    }
} //namespace Camera