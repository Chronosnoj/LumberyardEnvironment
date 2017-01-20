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

#include "FbxTimeWrapper.h"

namespace AZ
{
    namespace FbxSDKWrapper
    {
        FbxTimeWrapper::FbxTimeWrapper() : m_fbxTime(FBXSDK_TIME_INFINITE)
        {
        }

        FbxTimeWrapper::FbxTimeWrapper(FbxTime& fbxTime) : m_fbxTime(fbxTime)
        {
        }

        FbxTimeWrapper::~FbxTimeWrapper()
        {
        }

        void FbxTimeWrapper::SetFrame(int64_t frames, TimeMode timeMode)
        {
            m_fbxTime.SetFrame(static_cast<FbxLongLong>(frames), GetFbxTimeMode(timeMode));
        }

        double FbxTimeWrapper::GetFrameRate() const
        {
            return m_fbxTime.GetFrameRate(m_fbxTime.GetGlobalTimeMode());
        }

        int64_t FbxTimeWrapper::GetFrameCount() const
        {
            return static_cast<int64_t>(m_fbxTime.GetFrameCount());
        }

        FbxTime::EMode FbxTimeWrapper::GetFbxTimeMode(TimeMode timeMode) const
        {
            switch (timeMode)
            {
            case frames60:
                return FbxTime::eFrames60;
            case frames30:
                return FbxTime::eFrames30;
            case frames24:
                return FbxTime::eFrames24;
            default:
                return FbxTime::eDefaultMode;
            }
        }
    }
}