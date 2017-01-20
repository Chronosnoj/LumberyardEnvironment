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

#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Plane.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Component/TransformBus.h>

#include "MotionExtraction.h"

namespace LmbrCentral
{
    namespace Animation
    {
        MotionParameters ExtractMotionParameters(float deltaTime, const AZ::Transform& entityTransform, const AZ::Transform& frameMotionDelta, const AZ::Vector3& worldGroundNormal)
        {
            MotionParameters params;

            const float kMovementEpsilon = 0.001f;
            const float kSlightlyLessThanHalfPi = AZ::Constants::HalfPi - 0.01f;

            const AZ::Quaternion entityOrientation = AZ::Quaternion::CreateFromTransform(entityTransform);
            const AZ::Vector3 entityPosition = entityTransform.GetTranslation();

            // Ground angle information is movement-relative, not character-relative.
            AZ::Vector3 worldTranslation = frameMotionDelta.GetTranslation();
            worldTranslation.SetZ(0.f);
            const AZ::Vector3 movementDirection = (worldTranslation.IsZero(kMovementEpsilon)) ? entityTransform.GetColumn(1) : worldTranslation;
            const AZ::Vector3 movementLocalGroundNormal = AZ::Quaternion::CreateRotationZ(-atan2f(-movementDirection.GetX(), movementDirection.GetY())) * worldGroundNormal;
            const float groundAngle = AZ::Constants::HalfPi - acosf(fabsf(movementLocalGroundNormal.GetY()));

            AZ::Vector3 relativeTranslation = entityOrientation.GetInverseFast() * worldTranslation;
            relativeTranslation.SetZ(0.f);
            if (!relativeTranslation.IsZero())
            {
                // Scale XY translation per the ground angle.
                relativeTranslation /= cosf(groundAngle);
            }

            // Ground angle is signed. Negative for downhill.
            params.m_groundAngleSigned = groundAngle * ((movementLocalGroundNormal.GetY() > 0.f) ? -1.f : 1.f);

            // Compute travel angle (facing-relative movement angle in rads). Signed, ranges from -Pi..Pi, following animation system's motion extraction.
            // A left angle is positive.
            float travelAngle = 0.f;
            if (!relativeTranslation.IsZero())
            {
                travelAngle = atan2f(-relativeTranslation.GetX(), relativeTranslation.GetY());
            }
            params.m_travelAngle = travelAngle;

            // Compute ground-relative travel distance (this frame displacement, m/frame).
            params.m_travelDistance = relativeTranslation.GetLength();

            // Compute ground-relative travel speed (m/s).
            params.m_travelSpeed = deltaTime > 0.f ? (params.m_travelDistance / deltaTime) : 0.f;

            // Compute single frame orientation change (rads/frame).
            const AZ::Vector3 directionChange = frameMotionDelta.GetColumn(1);
            params.m_turnAngle = atan2f(-directionChange.GetX(), directionChange.GetY());

            // Compute turn rate (rads/sec);
            params.m_turnSpeed = deltaTime > 0.f ? (params.m_turnAngle / deltaTime) : 0.f;

            return params;
        }

    } // namespace Animation

} // namespace LmbrCentral
