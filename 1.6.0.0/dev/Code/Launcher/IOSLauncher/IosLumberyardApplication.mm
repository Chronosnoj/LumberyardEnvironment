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

#include <AzInput/TouchEvents.h>

#import <UIKit/UIKit.h>

namespace
{
    // The maximum number of active touches tracked by iOS is actually device dependent, and at this
    // time appears to be 5 for iPhone/iPodTouch and 11 for iPad. There is no API to query or set this,
    // so we'll give ourselves a buffer in case new devices are released that are able to track more.
    static const uint32_t MAX_ACTIVE_TOUCHES = 20;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    AzInput::TouchEvent InitTouchEvent(const UITouch* touch,
                                       uint32_t index,
                                       AzInput::TouchEvent::State state)
    {
        CGPoint touchLocation = [touch locationInView: touch.view];
        CGSize viewSize = [touch.view bounds].size;

        const float normalizedLocationX = touchLocation.x / viewSize.width;
        const float normalizedLocationY = touchLocation.y / viewSize.height;
        const float pressure = touch.force / touch.maximumPossibleForce;

        return AzInput::TouchEvent(normalizedLocationX,
                                   normalizedLocationY,
                                   pressure,
                                   index,
                                   state);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
@interface IosLumberyardApplication : UIApplication
{
    // iOS does not provide us with the index of a touch, but it does persist UITouch objects
    // throughout a multi-touch sequence, so we can keep track of the touch indices ourselves.
    UITouch* m_activeTouches[MAX_ACTIVE_TOUCHES];
}
@end    // IosLumberyardApplication Interface

////////////////////////////////////////////////////////////////////////////////////////////////////
@implementation IosLumberyardApplication

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)touchesBegan: (NSSet<UITouch*>*)touches withEvent: (UIEvent*)event
{
    for (UITouch* touch in touches)
    {
        for (uint32_t i = 0; i < MAX_ACTIVE_TOUCHES; ++i)
        {
            // Use the first available index.
            if (m_activeTouches[i] == nullptr)
            {
                m_activeTouches[i] = touch;

                const AzInput::TouchEvent::State state = AzInput::TouchEvent::State::Began;
                const AzInput::TouchEvent event = InitTouchEvent(touch, i, state);
                EBUS_EVENT(AzInput::TouchNotificationsBus, OnTouchBegan, event);

                break;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)touchesMoved: (NSSet<UITouch*>*)touches withEvent: (UIEvent*)event
{
    for (UITouch* touch in touches)
    {
        for (uint32_t i = 0; i < MAX_ACTIVE_TOUCHES; ++i)
        {
            if (m_activeTouches[i] == touch)
            {
                const AzInput::TouchEvent::State state = AzInput::TouchEvent::State::Moved;
                const AzInput::TouchEvent event = InitTouchEvent(touch, i, state);
                EBUS_EVENT(AzInput::TouchNotificationsBus, OnTouchMoved, event);

                break;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)touchesEnded: (NSSet<UITouch*>*)touches withEvent: (UIEvent*)event
{
    for (UITouch* touch in touches)
    {
        for (uint32_t i = 0; i < MAX_ACTIVE_TOUCHES; ++i)
        {
            if (m_activeTouches[i] == touch)
            {
                const AzInput::TouchEvent::State state = AzInput::TouchEvent::State::Ended;
                const AzInput::TouchEvent event = InitTouchEvent(touch, i, state);
                EBUS_EVENT(AzInput::TouchNotificationsBus, OnTouchEnded, event);

                m_activeTouches[i] = nullptr;

                break;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent: (UIEvent*)event
{
    // Active touches can be cancelled (as opposed to ended) for a variety of reasons, including:
    // - The active view being rotated to match the device orientation.
    // - The application resigning it's active status (eg. when receiving a message or phone call).
    // - Exceeding the max number of active touches tracked by the system (which as explained above
    //   is device dependent). For some reason this causes all active touches to be cancelled.
    // In any case, for the purposes of a game (or really any application that I can think of),
    // there really isn't any reason to distinguish between a touch ending or being cancelled.
    // They are mutually exclusive events, and both result in the touch being discarded by the
    // system.
    [self touchesEnded: touches withEvent: event];
}

@end // IosLumberyardApplication Implementation
