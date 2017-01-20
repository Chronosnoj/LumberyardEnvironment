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

#include "IGestureRecognizer.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace Gestures
{
    class RecognizerRotate;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    struct IRotateListener
    {
        virtual ~IRotateListener() {}
        virtual void OnRotateInitiated(const RecognizerRotate& recognizer) {}
        virtual void OnRotateUpdated(const RecognizerRotate& recognizer) {}
        virtual void OnRotateEnded(const RecognizerRotate& recognizer) {}
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////
    class RecognizerRotate
        : public IRecognizer
    {
    public:
        inline static float GetDefaultMaxPixelsMoved() { return 50.0f; }
        inline static float GetDefaultMinAngleDegrees() { return 15.0f; }
        inline static int32_t GetDefaultPriority() { return 0; }

        struct Config
        {
            inline Config()
                : maxPixelsMoved(GetDefaultMaxPixelsMoved())
                , minAngleDegrees(GetDefaultMinAngleDegrees())
                , priority(GetDefaultPriority())
            {}

            float maxPixelsMoved;
            float minAngleDegrees;
            int32_t priority;
        };
        inline static const Config& GetDefaultConfig() { static Config s_cfg; return s_cfg; }

        inline explicit RecognizerRotate(IRotateListener& listener,
            const Config& config = GetDefaultConfig());
        inline ~RecognizerRotate() override;

        inline int32_t GetPriority() const override { return m_config.priority; }
        inline bool OnPressedEvent(const Vec2& screenPosition, uint32_t pointerIndex) override;
        inline bool OnDownEvent(const Vec2& screenPosition, uint32_t pointerIndex) override;
        inline bool OnReleasedEvent(const Vec2& screenPosition, uint32_t pointerIndex) override;

        inline Config& GetConfig() { return m_config; }
        inline const Config& GetConfig() const { return m_config; }

        inline Vec2 GetStartPosition0() const { return m_startPositions[0]; }
        inline Vec2 GetStartPosition1() const { return m_startPositions[1]; }

        inline Vec2 GetCurrentPosition0() const { return m_currentPositions[0]; }
        inline Vec2 GetCurrentPosition1() const { return m_currentPositions[1]; }

        inline Vec2 GetStartMidpoint() const { return Lerp(GetStartPosition0(), GetStartPosition1(), 0.5f); }
        inline Vec2 GetCurrentMidpoint() const { return Lerp(GetCurrentPosition0(), GetCurrentPosition1(), 0.5f); }

        inline float GetStartDistance() const { return GetStartPosition1().GetDistance(GetStartPosition0()); }
        inline float GetCurrentDistance() const { return GetCurrentPosition1().GetDistance(GetCurrentPosition0()); }
        inline float GetSignedRotationInDegrees() const;

    private:
        enum class State
        {
            Idle = -1,
            Pressed0 = 0,
            Pressed1 = 1,
            PressedBoth,
            Rotating
        };

        static const uint32_t s_maxRotatePointerIndex = 1;

        IRotateListener& m_listener;
        Config m_config;

        ScreenPosition m_startPositions[2];
        ScreenPosition m_currentPositions[2];

        int64_t m_lastUpdateTimes[2];

        State m_currentState;
    };
}

// Include the implementation inline so the class can be instantiated outside of the gem.
#include <Gestures/GestureRecognizerRotate.inl>
