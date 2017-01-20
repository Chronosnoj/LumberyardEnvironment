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

#include <SceneAPI/SceneData/GraphData/MeshData.h>

namespace AZ
{
    namespace SceneData
    {
        namespace GraphData
        {
            namespace DataTypes = AZ::SceneAPI::DataTypes;


            MeshData::~MeshData() = default;

            void MeshData::AddPosition(const AZ::Vector3& position)
            {
                m_positions.push_back(position);
            }

            void MeshData::AddNormal(const AZ::Vector3& normal)
            {
                m_normals.push_back(normal);
            }

            //assume consistent winding - no stripping or fanning expected (3 index per face)
            void MeshData::AddFace(unsigned int index1, unsigned int index2, unsigned int index3, unsigned int faceMaterialId)
            {
                IMeshData::Face face;
                face.vertexIndex[0] = index1;
                face.vertexIndex[1] = index2;
                face.vertexIndex[2] = index3;
                m_faceList.push_back(face);
                m_faceMaterialIds.push_back(faceMaterialId);
            }

            void MeshData::AddFace(const DataTypes::IMeshData::Face& face, unsigned int faceMaterialId)
            {
                m_faceList.push_back(face);
                m_faceMaterialIds.push_back(faceMaterialId);
            }

            unsigned int MeshData::GetVertexCount() const
            {
                return static_cast<unsigned int>(m_positions.size());
            }

            bool MeshData::HasNormalData() const
            {
                return m_normals.size() > 0;
            }

            const AZ::Vector3& MeshData::GetPosition(unsigned int index) const
            {
                AZ_Assert(index < m_positions.size(), "GetPosition index not in range");
                return m_positions[index];
            }

            const AZ::Vector3& MeshData::GetNormal(unsigned int index) const
            {
                AZ_Assert(index < m_normals.size(), "GetNormal index not in range");
                return m_normals[index];
            }

            unsigned int MeshData::GetFaceCount() const
            {
                return static_cast<unsigned int>(m_faceList.size());
            }

            const  DataTypes::IMeshData::Face& MeshData::GetFaceInfo(unsigned int index) const
            {
                AZ_Assert(index < m_faceList.size(), "GetFaceInfo index not in range");
                return m_faceList[index];
            }

            unsigned int MeshData::GetFaceMaterialId(unsigned int index) const
            {
                AZ_Assert(index < m_faceMaterialIds.size(), "GetFaceMaterialIds index not in range");
                return m_faceMaterialIds[index];
            }
        }
    }
}
