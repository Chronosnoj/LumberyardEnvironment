
#include "Precompiled.h"
#include <AzTest/AzTest.h>

#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/MathUtils.h>

#include <ISystem.h>

#include "../Animation/MotionExtraction.h"

using ::testing::NiceMock;

TEST(AnimationMotionParameterExtraction, BasicExtractionTest)
{
    AZ::Quaternion entityOrientation = AZ::Quaternion::CreateRotationZ(AZ::Constants::Pi);
    AZ::Vector3 entityPosition(10.f, 20.f, 30.f);

    AZ::Quaternion frameRotation = AZ::Quaternion::CreateRotationZ(-AZ::Constants::HalfPi);
    AZ::Vector3 frameTranslation(-1.f, 0.f, 0.f);

    // Ground is oriented downhill with respect to our movement direction.
    const float groundSlope = -0.2;
    AZ::Vector3 groundNormal = AZ::Quaternion::CreateRotationZ(-AZ::Constants::HalfPi) * AZ::Quaternion::CreateRotationX(-groundSlope) * AZ::Vector3::CreateAxisZ();

    const float timeStep = 0.016f;

    LmbrCentral::Animation::MotionParameters params = LmbrCentral::Animation::ExtractMotionParameters(
        timeStep, 
        AZ::Transform::CreateFromQuaternionAndTranslation(entityOrientation, entityPosition),
        AZ::Transform::CreateFromQuaternionAndTranslation(frameRotation, frameTranslation),
        groundNormal);

    EXPECT_TRUE(AZ::IsClose(params.m_groundAngleSigned, groundSlope, 0.01f));                       // -0.2 rads ground angle (slightly downhill).
    EXPECT_TRUE(AZ::IsClose(params.m_travelAngle, -AZ::Constants::HalfPi, 0.01f));                  // Traveling to our left (positive in blend-space land).
    EXPECT_TRUE(AZ::IsClose(params.m_travelDistance, 1.f / cosf(groundSlope), 0.01f));              // 1 m travel distance on slight downward slope.
    EXPECT_TRUE(AZ::IsClose(params.m_travelSpeed, (1.f / cosf(groundSlope)) / timeStep, 0.01f));    // 1 m / timeStep.
    EXPECT_TRUE(AZ::IsClose(params.m_turnAngle, -AZ::Constants::HalfPi, 0.01f));                    // Turn angle is -HalfPi.
    EXPECT_TRUE(AZ::IsClose(params.m_turnSpeed, -AZ::Constants::HalfPi / timeStep, 0.01f));         // Turn speed is -HalfPi / timeStep.
}
