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
#include <AzCore/Component/ComponentBus.h>

namespace Camera
{

    class CameraComponentRequests : public AZ::ComponentBus
    {
    public:
        virtual ~CameraComponentRequests() = default;
        /// Gets the camera's field of view in degrees
        virtual float GetFov() const = 0;

        /// Gets the camera's distance from the near clip plane in meters
        virtual float GetNearClipDistance() const = 0;

        /// Gets the camera's distance from the far clip plane in meters
        virtual float GetFarClipDistance() const = 0;

        /// Sets the camera's field of view in degrees between 0 < fov < 180 degrees
        virtual void SetFov(float fov) = 0;

        /// Sets the near clip plane to a given distance from the camera in meters.  Should be small, but greater than 0
        virtual void SetNearClipDistance(float nearClipDistance) = 0;

        /// Sets the far clip plane to a given distance from the camera in meters.
        virtual void SetFarClipDistance(float farClipDistance) = 0;
    };
    using CameraRequestBus = AZ::EBus<CameraComponentRequests>;
} // namespace Camera
