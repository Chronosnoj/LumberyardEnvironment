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
#include "CameraTargetComponentAcquirer.h"
#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Math/Transform.h>
#include <StartingPointCamera/StartingPointCameraConstants.h>

namespace Camera
{
    void CameraTargetComponentAcquirer::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<CameraTargetComponentAcquirer>()
                ->Version(1)
                ->Field("Tag of Specific Target", &CameraTargetComponentAcquirer::m_tagOfSpecificTarget)
                ->Field("Use Target Rotation", &CameraTargetComponentAcquirer::m_shouldUseTargetRotation)
                ->Field("Use Target Position", &CameraTargetComponentAcquirer::m_shouldUseTargetPosition);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<CameraTargetComponentAcquirer>("CameraTargetComponentAcquirer", "Acquires a target from a CameraTargetComponent")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/Camera.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/Camera.png")
                    ->DataElement(0, &CameraTargetComponentAcquirer::m_tagOfSpecificTarget, "Tag of Specific Target", "Specific Tag to look for in a Camera Target. Ex. 'Player'")
                    ->DataElement(0, &CameraTargetComponentAcquirer::m_shouldUseTargetRotation, "Use Target Rotation", "Set to false to not have the camera orient itself with the target")
                    ->DataElement(0, &CameraTargetComponentAcquirer::m_shouldUseTargetPosition, "Use Target Position", "Set to false to not have the camera position itself with the target");
            }
        }
    }

    bool CameraTargetComponentAcquirer::AcquireTarget(AZ::Transform& outTransformInformation)
    {
        if (m_currentEntityTarget.IsValid())
        {
            AZ::Transform targetsTransform = AZ::Transform::Identity();
            EBUS_EVENT_ID_RESULT(targetsTransform, m_currentEntityTarget, AZ::TransformBus, GetWorldTM);
            if (m_shouldUseTargetPosition)
            {
                outTransformInformation.SetPosition(targetsTransform.GetPosition());
            }
            if (m_shouldUseTargetRotation)
            {
                outTransformInformation.SetColumns(targetsTransform.GetColumn(0), targetsTransform.GetColumn(1), targetsTransform.GetColumn(2), outTransformInformation.GetColumn(3));
            }
            return true;
        }
        return false;
    }

    void CameraTargetComponentAcquirer::OnCameraTargetAdded(AZ::EntityId entityTargetAdded, AZ::Crc32 tagCrc)
    {
        // if this is the entity we are told to look for or if we don't currently have a valid target
        if (!m_currentEntityTarget.IsValid() && (!m_tagCrc || tagCrc == m_tagCrc))
        {
            m_currentEntityTarget = entityTargetAdded;
        }
    }

    void CameraTargetComponentAcquirer::OnCameraTargetRemoved(AZ::EntityId entityTargetAdded, AZ::Crc32 /*tagCrc*/)
    {
        if (m_currentEntityTarget == entityTargetAdded)
        {
            m_currentEntityTarget.SetInvalid();
        }
    }

    void CameraTargetComponentAcquirer::Activate(AZ::EntityId)
    {
        m_currentEntityTarget.SetInvalid();
        m_tagCrc = AZ::Crc32(m_tagOfSpecificTarget.c_str(), m_tagOfSpecificTarget.length(), s_forceCrcLowerCase);
        // We need to collect all of the camera targets that have been added before our activation and then
        // subscribe for notifications when new ones are added to catch them all
        CameraTargetList previouslyAddedTargets;
        EBUS_EVENT(CameraTargetRequestBus, RequestAllCameraTargets, previouslyAddedTargets);
        CameraTargetNotificationBus::Handler::BusConnect();
        for (const CameraTargetEntry& previousTarget : previouslyAddedTargets)
        {
            OnCameraTargetAdded(previousTarget.m_entityId, previousTarget.m_tagCrc);
        }
    }

    void CameraTargetComponentAcquirer::Deactivate()
    {
        CameraTargetNotificationBus::Handler::BusDisconnect();
    }
} // namespace Camera