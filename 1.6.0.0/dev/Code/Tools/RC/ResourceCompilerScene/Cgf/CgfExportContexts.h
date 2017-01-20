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

#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/string/string.h>
#include <SceneAPI/SceneCore/Events/CallProcessorBus.h>
#include <SceneAPI/SceneCore/Events/ExportEventContext.h>
#include <SceneAPI/SceneCore/Containers/SceneGraph.h>
#include <ExportContextGlobal.h>

enum EPhysicsGeomType;
class CContentCGF;
struct CNodeCGF;
class CMesh;

namespace AZ
{
    namespace SceneAPI
    {
        namespace DataTypes
        {
            class IMeshGroup;
        }
    }
    namespace RC
    {
        // Called to export a specific Mesh Group.
        struct CgfMeshGroupExportContext
            : public SceneAPI::Events::ICallContext
        {
            AZ_RTTI(CgfMeshGroupExportContext, "{974FF54E-9724-4E8B-A2A1-D360C3B3DA80}", SceneAPI::Events::ICallContext);

            CgfMeshGroupExportContext(SceneAPI::Events::ExportEventContext& parent, const AZStd::string& groupName,
                const SceneAPI::DataTypes::IMeshGroup& group, Phase phase);
            CgfMeshGroupExportContext(const SceneAPI::Containers::Scene& scene, const AZStd::string& outputDirectory,
                const AZStd::string& groupName, const SceneAPI::DataTypes::IMeshGroup& group, Phase phase);
            CgfMeshGroupExportContext(const CgfMeshGroupExportContext& copyContext, Phase phase);
            CgfMeshGroupExportContext(const CgfMeshGroupExportContext& copyContext) = delete;
            ~CgfMeshGroupExportContext() override = default;

            CgfMeshGroupExportContext& operator=(const CgfMeshGroupExportContext& other) = delete;

            const SceneAPI::Containers::Scene& m_scene;
            const AZStd::string& m_outputDirectory;
            const AZStd::string& m_groupName;
            const SceneAPI::DataTypes::IMeshGroup& m_group;
            const Phase m_phase; 
        };

        // Called while creating, filling and finalizing a CContentCGF container.
        struct CgfContainerExportContext
            : public CgfMeshGroupExportContext
        {
            AZ_RTTI(CgfContainerExportContext, "{667A9E60-F3AA-45E1-8E66-05B0C971A094}", CgfMeshGroupExportContext);

            CgfContainerExportContext(CgfMeshGroupExportContext& parent, CContentCGF& container, Phase phase);
            CgfContainerExportContext(const SceneAPI::Containers::Scene& scene, const AZStd::string& outputDirectory,
                const AZStd::string& groupName, const SceneAPI::DataTypes::IMeshGroup& group, CContentCGF& container,
                Phase phase);
            CgfContainerExportContext(const CgfContainerExportContext& copyContext, Phase phase);
            CgfContainerExportContext(const CgfContainerExportContext& copyContext) = delete;
            ~CgfContainerExportContext() override = default;

            CgfContainerExportContext& operator=(const CgfContainerExportContext& other) = delete;

            CContentCGF& m_container;
        };

        // Called when a new CNode is added to a CContentCGF container.
        struct CgfNodeExportContext
            : public CgfContainerExportContext
        {
            AZ_RTTI(CgfNodeExportContext, "{A7D130C6-2CB2-47AC-9D9C-969FA473DFDA}", CgfContainerExportContext);

            CgfNodeExportContext(CgfContainerExportContext& parent, CNodeCGF& node, const AZStd::string& nodeName,
                SceneAPI::Containers::SceneGraph::NodeIndex nodeIndex, EPhysicsGeomType physicalizeType, Phase phase);
            CgfNodeExportContext(const SceneAPI::Containers::Scene& scene, const AZStd::string& outputDirectory,
                const AZStd::string& groupName, const SceneAPI::DataTypes::IMeshGroup& group,
                CContentCGF& container, CNodeCGF& node, const AZStd::string& nodeName, SceneAPI::Containers::SceneGraph::NodeIndex nodeIndex,
                EPhysicsGeomType physicalizeType, Phase phase);
            CgfNodeExportContext(const CgfNodeExportContext& copyContext, Phase phase);
            CgfNodeExportContext(const CgfNodeExportContext& copyContext) = delete;
            ~CgfNodeExportContext() override = default;

            CgfNodeExportContext& operator=(const CgfNodeExportContext& other) = delete;

            CNodeCGF& m_node;
            const AZStd::string& m_nodeName;
            SceneAPI::Containers::SceneGraph::NodeIndex m_nodeIndex;
            EPhysicsGeomType m_physicalizeType;
        };

        // Called when new mesh data was added to a CNode in a CContentCGF container.
        struct CgfMeshNodeExportContext
            : public CgfNodeExportContext
        {
            AZ_RTTI(CgfMeshNodeExportContext, "{D39D08D6-8EB5-4058-B9D7-BED4EB460555}", CgfNodeExportContext);

            CgfMeshNodeExportContext(CgfNodeExportContext& parent, CMesh& mesh, Phase phase);
            CgfMeshNodeExportContext(const SceneAPI::Containers::Scene& scene, const AZStd::string& outputDirectory,
                const AZStd::string& groupName, const SceneAPI::DataTypes::IMeshGroup& group, CContentCGF& container, CNodeCGF& node,
                const AZStd::string& nodeName, SceneAPI::Containers::SceneGraph::NodeIndex nodeIndex, EPhysicsGeomType physicalizeType,
                CMesh& mesh, Phase phase);
            CgfMeshNodeExportContext(const CgfMeshNodeExportContext& copyContext, Phase phase);
            CgfMeshNodeExportContext(const CgfMeshNodeExportContext& copyContext) = delete;
            ~CgfMeshNodeExportContext() override = default;
            
            CgfMeshNodeExportContext& operator=(const CgfMeshNodeExportContext& other) = delete;

            CMesh& m_mesh;
        };
    } // RC
} // AZ
