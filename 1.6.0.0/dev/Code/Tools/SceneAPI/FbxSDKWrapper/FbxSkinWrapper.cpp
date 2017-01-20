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

#include "FbxSkinWrapper.h"
#include "FbxNodeWrapper.h"
#include <AzCore/std/smart_ptr/make_shared.h>

namespace AZ
{
    namespace FbxSDKWrapper
    {
        FbxSkinWrapper::FbxSkinWrapper(FbxSkin* fbxSkin)
            : m_fbxSkin(fbxSkin)
        {
            assert(fbxSkin);
        }

        FbxSkinWrapper::~FbxSkinWrapper()
        {
            m_fbxSkin = nullptr;
        }

        const char* FbxSkinWrapper::GetName() const
        {
            return m_fbxSkin->GetName();
        }

        int FbxSkinWrapper::GetClusterCount() const
        {
            return m_fbxSkin->GetClusterCount();
        }

        int FbxSkinWrapper::GetClusterControlPointIndicesCount(int index) const
        {
            return m_fbxSkin->GetCluster(index)->GetControlPointIndicesCount();
        }

        int FbxSkinWrapper::GetClusterControlPointIndex(int clusterIndex, int pointIndex) const
        {
            return m_fbxSkin->GetCluster(clusterIndex)->GetControlPointIndices()[pointIndex];
        }

        double FbxSkinWrapper::GetClusterControlPointWeight(int clusterIndex, int pointIndex) const
        {
            return m_fbxSkin->GetCluster(clusterIndex)->GetControlPointWeights()[pointIndex];
        }

        AZStd::shared_ptr<const FbxNodeWrapper> FbxSkinWrapper::GetClusterLink(int index) const
        {
            return AZStd::make_shared<const FbxNodeWrapper>(m_fbxSkin->GetCluster(index)->GetLink());
        }
    }
}