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
#include "CameraTargetComponent.h"
#include "StartingPointCamera/CameraTargetBus.h"
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <StartingPointCamera/StartingPointCameraConstants.h>

// for reflect methods
#include "CameraTargetAcquirers/CameraTargetComponentAcquirer.h"
#include "CameraTransformBehaviors/FollowTargetFromDistance.h"
#include "CameraLookAtBehaviors/OffsetPosition.h"
#include "CameraTransformBehaviors/FollowTargetFromAngle.h"
#include "CameraTransformBehaviors/Rotate.h"
#include "CameraTransformBehaviors/OffsetCameraPosition.h"
#include "CameraLookAtBehaviors/SlideAlongAxisBasedOnAngle.h"
#include "CameraLookAtBehaviors/RotateCameraLookAt.h"
#include "CameraTransformBehaviors/FaceTarget.h"

namespace Camera
{
    void CameraTargetComponent::Reflect(AZ::ReflectContext* reflection)
    {
        Camera::CameraTargetComponentAcquirer::Reflect(reflection);
        Camera::FollowTargetFromDistance::Reflect(reflection);
        Camera::OffsetPosition::Reflect(reflection);
        Camera::FollowTargetFromAngle::Reflect(reflection);
        Camera::Rotate::Reflect(reflection);
        Camera::OffsetCameraPosition::Reflect(reflection);
        Camera::SlideAlongAxisBasedOnAngle::Reflect(reflection);
        Camera::RotateCameraLookAt::Reflect(reflection);
        Camera::FaceTarget::Reflect(reflection);

        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<CameraTargetComponent>()
                ->Version(1)
                ->Field("Tag", &CameraTargetComponent::m_tag);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<CameraTargetComponent>("Camera Target", "Registers attached entity as a valid camera target")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Game")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/CameraTarget.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/CameraTarget.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                    ->DataElement(0, &CameraTargetComponent::m_tag, "Tag", "Tag to help acquirers. Ex. 'Player'");
            }
        }
    }

    void CameraTargetComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("CameraTargetProvider"));
    }

    void CameraTargetComponent::Activate()
    {
        CameraTargetRequestBus::Handler::BusConnect();
        EBUS_EVENT(CameraTargetNotificationBus, OnCameraTargetAdded, GetEntityId(), AZ::Crc32(m_tag.c_str(), m_tag.size(), s_forceCrcLowerCase));
    }

    void CameraTargetComponent::Deactivate()
    {
        CameraTargetRequestBus::Handler::BusDisconnect();
        EBUS_EVENT(CameraTargetNotificationBus, OnCameraTargetRemoved, GetEntityId(), AZ::Crc32(m_tag.c_str(), m_tag.size(), s_forceCrcLowerCase));
    }

    void CameraTargetComponent::RequestAllCameraTargets(CameraTargetList& targetList)
    {
        targetList.push_back(CameraTargetEntry(GetEntityId(), AZ::Crc32(m_tag.c_str(), m_tag.size(), s_forceCrcLowerCase)));
    }
} // namespace Camera
