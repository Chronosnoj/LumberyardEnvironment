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

#include <SceneAPI/SceneData/GraphData/MeshVertexUVData.h>

namespace AZ
{
    namespace SceneData
    {
        namespace GraphData
        {
            size_t MeshVertexUVData::GetCount() const
            {
                return m_uvs.size();
            }

            const AZ::Vector2& MeshVertexUVData::GetUV(size_t index) const
            {
                AZ_Assert(index < m_uvs.size(), "Invalid index %i for mesh vertex UVs.", index);
                return m_uvs[index];
            }

            void MeshVertexUVData::ReserveContainerSpace(size_t size)
            {
                m_uvs.reserve(size);
            }

            void MeshVertexUVData::AppendUV(const AZ::Vector2& uv)
            {
                m_uvs.push_back(uv);
            }
        } // GraphData
    } // SceneData
} // AZ