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

namespace AZ
{
    class Transform;
    class Vector3;
}

namespace LmbrCentral
{
    namespace Animation
    {
        struct MotionParameters
        {
            MotionParameters()
                : m_groundAngleSigned(0.f)
                , m_travelAngle(0.f)
                , m_travelDistance(0.f)
                , m_travelSpeed(0.f)
                , m_turnAngle(0.f)
                , m_turnSpeed(0.f)
            {}

            float m_groundAngleSigned;
            float m_travelAngle;
            float m_travelDistance;
            float m_travelSpeed;
            float m_turnAngle;
            float m_turnSpeed;
        };

        MotionParameters ExtractMotionParameters(float deltaTime, const AZ::Transform& entityTransform, const AZ::Transform& frameMotionDelta, const AZ::Vector3& worldGroundNormal);

    } // namespace Animation

} // namespace LmbrCentral
