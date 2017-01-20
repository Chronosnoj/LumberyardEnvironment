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

#include "FbxNodeWrapper.h"
#include "FbxMaterialWrapper.h"
#include "FbxTypeConverter.h"

namespace AZ
{
    namespace FbxSDKWrapper
    {
        FbxNodeWrapper::FbxNodeWrapper(FbxNode* fbxNode)
            : m_fbxNode(fbxNode)
        {
            assert(fbxNode);
        }

        FbxNodeWrapper::~FbxNodeWrapper()
        {
            m_fbxNode = nullptr;
        }

        int FbxNodeWrapper::GetMaterialCount() const
        {
            return m_fbxNode->GetMaterialCount();
        }

        const char* FbxNodeWrapper::GetMaterialName(int index) const
        {
            if (index < GetMaterialCount())
            {
                return m_fbxNode->GetMaterial(index)->GetName();
            }
            return nullptr;
        }

        const std::shared_ptr<FbxMeshWrapper> FbxNodeWrapper::GetMesh() const
        {
            FbxMesh* mesh = m_fbxNode->GetMesh();
            return mesh ? std::shared_ptr<FbxMeshWrapper>(new FbxMeshWrapper(mesh)) : nullptr;
        }

        const std::shared_ptr<FbxPropertyWrapper> FbxNodeWrapper::FindProperty(const char* name) const
        {
            return std::shared_ptr<FbxPropertyWrapper>(new FbxPropertyWrapper(&m_fbxNode->FindProperty(name)));
        }

        bool FbxNodeWrapper::IsBone() const
        {
            return (m_fbxNode->GetSkeleton() != nullptr);
        }

        const char* FbxNodeWrapper::GetName() const
        {
            return m_fbxNode->GetName();
        }

        Transform FbxNodeWrapper::EvaluateGlobalTransform()
        {
            return FbxTypeConverter::ToTransform(m_fbxNode->EvaluateGlobalTransform());
        }

        Vector3 FbxNodeWrapper::EvaluateLocalTranslation()
        {
            return FbxTypeConverter::ToVector3(m_fbxNode->EvaluateLocalTranslation());
        }

        Vector3 FbxNodeWrapper::EvaluateLocalTranslation(FbxTimeWrapper& time)
        {
            return FbxTypeConverter::ToVector3(m_fbxNode->EvaluateLocalTranslation(time.m_fbxTime));
        }

        Transform FbxNodeWrapper::EvaluateLocalTransform()
        {
            return FbxTypeConverter::ToTransform(m_fbxNode->EvaluateLocalTransform());
        }

        Transform FbxNodeWrapper::EvaluateLocalTransform(FbxTimeWrapper& time)
        {
            return FbxTypeConverter::ToTransform(m_fbxNode->EvaluateLocalTransform(time.m_fbxTime));
        }

        Vector3 FbxNodeWrapper::GetGeometricTranslation() const
        {
            return FbxTypeConverter::ToVector3(m_fbxNode->GetGeometricTranslation(FbxNode::eSourcePivot));
        }

        Vector3 FbxNodeWrapper::GetGeometricScaling() const
        {
            return FbxTypeConverter::ToVector3(m_fbxNode->GetGeometricScaling(FbxNode::eSourcePivot));
        }

        Vector3 FbxNodeWrapper::GetGeometricRotation() const
        {
            return FbxTypeConverter::ToVector3(m_fbxNode->GetGeometricRotation(FbxNode::eSourcePivot));
        }

        Transform FbxNodeWrapper::GetGeometricTransform() const
        {
            FbxAMatrix geoTransform(m_fbxNode->GetGeometricTranslation(FbxNode::eSourcePivot),
                m_fbxNode->GetGeometricRotation(FbxNode::eSourcePivot), m_fbxNode->GetGeometricScaling(FbxNode::eSourcePivot));
            return FbxTypeConverter::ToTransform(geoTransform);
        }

        bool FbxNodeWrapper::IsAnimated() const
        {
            return FbxAnimUtilities::IsAnimated(m_fbxNode);
        }

        int FbxNodeWrapper::GetChildCount() const
        {
            return m_fbxNode->GetChildCount();
        }

        const std::shared_ptr<FbxNodeWrapper> FbxNodeWrapper::GetChild(int childIndex) const
        {
            FbxNode* child = m_fbxNode->GetChild(childIndex);
            return child ? std::shared_ptr<FbxNodeWrapper>(new FbxNodeWrapper(child)) : nullptr;
        }

        const std::shared_ptr<FbxMaterialWrapper> FbxNodeWrapper::GetMaterial(int index) const
        {
            if (index < GetMaterialCount())
            {
                return std::shared_ptr<FbxMaterialWrapper>(new FbxMaterialWrapper(m_fbxNode->GetMaterial(index)));
            }
            return nullptr;
        }
    }
}