#pragma once

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
#include "FbxMeshWrapper.h"
#include "FbxPropertyWrapper.h"
#include <AzCore/Math/Transform.h>

namespace AZ
{
    namespace FbxSDKWrapper
    {
        class FbxMaterialWrapper;

        class FbxNodeWrapper
        {
        public:
            FbxNodeWrapper(FbxNode* fbxNode);
            virtual ~FbxNodeWrapper();

            virtual int GetMaterialCount() const;
            virtual const char* GetMaterialName(int index) const;
            virtual const std::shared_ptr<FbxMaterialWrapper> GetMaterial(int index) const;
            virtual const std::shared_ptr<FbxMeshWrapper> GetMesh() const;
            virtual const std::shared_ptr<FbxPropertyWrapper> FindProperty(const char* name) const;
            virtual bool IsBone() const;
            virtual const char* GetName() const;

            virtual Transform EvaluateGlobalTransform();
            virtual Vector3 EvaluateLocalTranslation();
            virtual Vector3 EvaluateLocalTranslation(FbxTimeWrapper& time);
            virtual Transform EvaluateLocalTransform();
            virtual Transform EvaluateLocalTransform(FbxTimeWrapper& time);
            virtual Vector3 GetGeometricTranslation() const;
            virtual Vector3 GetGeometricScaling() const;
            virtual Vector3 GetGeometricRotation() const;
            virtual Transform GetGeometricTransform() const;

            virtual int GetChildCount() const;
            virtual const std::shared_ptr<FbxNodeWrapper> GetChild(int childIndex) const;

            virtual bool IsAnimated() const;

        protected:
            FbxNodeWrapper() = default;
            FbxNode* m_fbxNode;
        };
    }
}