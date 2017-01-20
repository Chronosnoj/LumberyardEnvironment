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

#include <RC/ResourceCompilerScene/Cgf/CgfExportContexts.h>

namespace AZ
{
    namespace RC
    {
        CgfMeshGroupExportContext::CgfMeshGroupExportContext(SceneAPI::Events::ExportEventContext& parent, const AZStd::string& groupName,
            const SceneAPI::DataTypes::IMeshGroup& group, Phase phase)
            : m_scene(parent.GetScene())
            , m_outputDirectory(parent.GetOutputDirectory())
            , m_groupName(groupName)
            , m_group(group)
            , m_phase(phase)
        {
        }

        CgfMeshGroupExportContext::CgfMeshGroupExportContext(const SceneAPI::Containers::Scene& scene, const AZStd::string& outputDirectory,
            const AZStd::string& groupName, const SceneAPI::DataTypes::IMeshGroup& group, Phase phase)
            : m_scene(scene)
            , m_outputDirectory(outputDirectory)
            , m_groupName(groupName)
            , m_group(group)
            , m_phase(phase)
        {
        }

        CgfMeshGroupExportContext::CgfMeshGroupExportContext(const CgfMeshGroupExportContext& copyContext, Phase phase)
            : m_scene(copyContext.m_scene)
            , m_outputDirectory(copyContext.m_outputDirectory)
            , m_groupName(copyContext.m_groupName)
            , m_group(copyContext.m_group)
            , m_phase(phase)
        {
        }

        CgfContainerExportContext::CgfContainerExportContext(CgfMeshGroupExportContext& parent, CContentCGF& container, Phase phase)
            : CgfMeshGroupExportContext(parent, phase)
            , m_container(container)
        {
        }

        CgfContainerExportContext::CgfContainerExportContext(const SceneAPI::Containers::Scene& scene, const AZStd::string& outputDirectory,
            const AZStd::string& groupName, const SceneAPI::DataTypes::IMeshGroup& group, CContentCGF& container,
            Phase phase)
            : CgfMeshGroupExportContext(scene, outputDirectory, groupName, group, phase)
            , m_container(container)
        {
        }

        CgfContainerExportContext::CgfContainerExportContext(const CgfContainerExportContext& copyContext, Phase phase)
            : CgfMeshGroupExportContext(copyContext, phase)
            , m_container(copyContext.m_container)
        {
        }

        CgfNodeExportContext::CgfNodeExportContext(CgfContainerExportContext& parent, CNodeCGF& node, const AZStd::string& nodeName,
            SceneAPI::Containers::SceneGraph::NodeIndex nodeIndex, EPhysicsGeomType physicalizeType, Phase phase)
            : CgfContainerExportContext(parent, phase) 
            , m_node(node)
            , m_nodeName(nodeName)
            , m_nodeIndex(nodeIndex)
            , m_physicalizeType(physicalizeType)
        {
        }

        CgfNodeExportContext::CgfNodeExportContext(const SceneAPI::Containers::Scene& scene, const AZStd::string& outputDirectory,
            const AZStd::string& groupName, const SceneAPI::DataTypes::IMeshGroup& group, CContentCGF& container, CNodeCGF& node, 
            const AZStd::string& nodeName, SceneAPI::Containers::SceneGraph::NodeIndex nodeIndex, EPhysicsGeomType physicalizeType,
            Phase phase)
            : CgfContainerExportContext(scene, outputDirectory, groupName, group, container, phase)
            , m_node(node)
            , m_nodeName(nodeName)
            , m_nodeIndex(nodeIndex)
            , m_physicalizeType(physicalizeType)
        {
        }

        CgfNodeExportContext::CgfNodeExportContext(const CgfNodeExportContext& copyContext, Phase phase)
            : CgfContainerExportContext(copyContext, phase)
            , m_node(copyContext.m_node)
            , m_nodeName(copyContext.m_nodeName)
            , m_nodeIndex(copyContext.m_nodeIndex)
            , m_physicalizeType(copyContext.m_physicalizeType)
        {
        }

        CgfMeshNodeExportContext::CgfMeshNodeExportContext(CgfNodeExportContext& parent, CMesh& mesh, Phase phase)
            : CgfNodeExportContext(parent, phase)
            , m_mesh(mesh)
        {
        }

        CgfMeshNodeExportContext::CgfMeshNodeExportContext(const SceneAPI::Containers::Scene& scene, const AZStd::string& outputDirectory,
            const AZStd::string& groupName, const SceneAPI::DataTypes::IMeshGroup& group, CContentCGF& container, CNodeCGF& node, 
            const AZStd::string& nodeName, SceneAPI::Containers::SceneGraph::NodeIndex nodeIndex, EPhysicsGeomType physicalizeType, 
            CMesh& mesh, Phase phase)
            : CgfNodeExportContext(scene, outputDirectory, groupName, group, container, node, nodeName, nodeIndex, physicalizeType, phase)
            , m_mesh(mesh)
        {
        }

        CgfMeshNodeExportContext::CgfMeshNodeExportContext(const CgfMeshNodeExportContext& copyContext, Phase phase)
            : CgfNodeExportContext(copyContext, phase)
            , m_mesh(copyContext.m_mesh)
        {
        }

    } // RC
} // AZ