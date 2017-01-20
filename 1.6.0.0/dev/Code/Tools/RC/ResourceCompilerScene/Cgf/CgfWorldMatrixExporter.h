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

#include <Cry_Matrix34.h>
#include <AzCore/Math/Transform.h>
#include <SceneAPI/SceneCore/Containers/SceneGraph.h>
#include <SceneAPI/SceneCore/Events/CallProcessorBinder.h>

namespace AZ
{
    class Transform;

    namespace SceneAPI
    {
        namespace DataTypes
        {
            class IOriginRule;
            class IMeshGroup;
        }
    }

    namespace RC
    {
        struct CgfMeshGroupExportContext;
        struct CgfNodeExportContext;

        class CgfWorldMatrixExporter
            : public SceneAPI::Events::CallProcessorBinder
        {
        public:
            CgfWorldMatrixExporter();
            ~CgfWorldMatrixExporter() override = default;

            SceneAPI::Events::ProcessingResult ProcessMeshGroup(CgfMeshGroupExportContext& context);
            SceneAPI::Events::ProcessingResult ProcessNode(CgfNodeExportContext& context);

        protected:
            using HierarchyStorageIterator = SceneAPI::Containers::SceneGraph::HierarchyStorageConstIterator;

            bool ConcatenateMatricesUpwards(Transform& transform, const HierarchyStorageIterator& nodeIterator, const SceneAPI::Containers::SceneGraph& graph) const;
            bool MultiplyEndPointTransforms(Transform& transform, const HierarchyStorageIterator& nodeIterator, const SceneAPI::Containers::SceneGraph& graph) const;
            void TransformToMatrix34(Matrix34& out, const Transform& in) const;

            Transform m_cachedRootMatrix;
            const SceneAPI::DataTypes::IMeshGroup* m_cachedMeshGroup;
            bool m_cachedRootMatrixIsSet;
        };
    } // RC
} // AZ