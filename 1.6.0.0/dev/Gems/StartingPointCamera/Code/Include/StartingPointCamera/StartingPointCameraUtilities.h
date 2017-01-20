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
#include <AzCore/Math/Vector3.h>
#include "StartingPointCamera/StartingPointCameraConstants.h"
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzFramework/Math/MathUtils.h>

namespace Camera
{
    //////////////////////////////////////////////////////////////////////////
    /// This methods will 0 out a vector component and re-normalize it
    //////////////////////////////////////////////////////////////////////////
    static void MaskComponentFromNormalizedVector(AZ::Vector3& v, VectorComponentType vectorComponentType)
    {
        switch (vectorComponentType)
        {
        case X_Component:
        {
            v.SetX(0.f);
            break;
        }
        case Y_Component:
        {
            v.SetY(0.f);
            break;
        }
        case Z_Component:
        {
            v.SetZ(0.f);
            break;
        }
        default:
            AZ_Assert(false, "MaskComponentFromNormalizedVector: VectorComponentType - unexpected value");
            break;
        }
        v.Normalize();
    }

    //////////////////////////////////////////////////////////////////////////
    /// This will calculate the requested Euler angle from a given AZ::Quaternion
    //////////////////////////////////////////////////////////////////////////
    static float GetEulerAngleFromTransform(const AZ::Transform& rotation, EulerAngleType eulerAngleType)
    {
        AZ::Vector3 angles = AzFramework::ConvertTransformToEulerDegrees(rotation);
        switch (eulerAngleType)
        {
        case Pitch:
            return angles.GetX();
        case Roll:
            return angles.GetY();
        case Yaw:
            return angles.GetZ();
        default:
            AZ_Warning("", false, "GetEulerAngleFromRotation: eulerAngleType - value not supported");
            return 0.f;
        }
    }

    //////////////////////////////////////////////////////////////////////////
    /// This will calculate an AZ::Transform based on an Euler angle
    //////////////////////////////////////////////////////////////////////////
    static AZ::Transform CreateRotationFromEulerAngle(EulerAngleType rotationType, AZ::VectorFloat radians)
    {
        switch (rotationType)
        {
        case Pitch:
            return AZ::Transform::CreateRotationX(radians);
        case Roll:
            return AZ::Transform::CreateRotationY(radians);
        case Yaw:
            return AZ::Transform::CreateRotationZ(radians);
        default:
            AZ_Warning("", false, "CreateRotationFromEulerAngle: rotationType - value not supported");
            return AZ::Transform::Identity();
        }
    }

    //////////////////////////////////////////////////////////////////////////
    /// Creates the Quaternion representing the rotation looking down the vector
    //////////////////////////////////////////////////////////////////////////
    static AZ::Quaternion CreateQuaternionFromViewVector(const AZ::Vector3 lookVector)
    {
        float twoDimensionLength = AZ::Vector2(lookVector.GetX(), lookVector.GetY()).GetLength();

        if (twoDimensionLength > AZ::g_fltEps)
        {
            AZ::Vector3 hv(lookVector.GetX() / twoDimensionLength, lookVector.GetY() / twoDimensionLength + 1.f, twoDimensionLength + 1.f);
            float twoDimensionHVLength = AZ::Vector2(hv.GetX(), hv.GetY()).GetLength();
            float twoDZLength = AZ::Vector2(hv.GetZ(), lookVector.GetZ()).GetLength();

            float halfCosHV = 0.f;
            float halfSinHV = -1.f;
            if (twoDimensionHVLength > AZ::g_fltEps)
            {
                halfCosHV = hv.GetY() / twoDimensionHVLength;
                halfSinHV = -hv.GetX() / twoDimensionHVLength;
            }
            float halfCosZ = hv.GetZ() / twoDZLength;
            float halfSinZ = lookVector.GetZ() / twoDZLength;
            return AZ::Quaternion(halfCosHV * halfSinZ, halfSinHV * halfSinZ, halfSinHV * halfCosZ, halfCosHV * halfCosZ);
        }
        return AZ::Quaternion::CreateIdentity();
    }
} //namespace Camera