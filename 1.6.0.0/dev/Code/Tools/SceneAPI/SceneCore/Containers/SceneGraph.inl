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

#include <AzCore/std/iterator.h>
#include <AzCore/Casting/numeric_cast.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            //
            // SceneGraph NodeHeader
            //
            SceneGraph::NodeHeader::NodeHeader()
                : m_isEndPoint(0)
                , m_parentIndex(NodeHeader::INVALID_INDEX)
                , m_siblingIndex(NodeHeader::INVALID_INDEX)
                , m_childIndex(NodeHeader::INVALID_INDEX)
            {
            }

            bool SceneGraph::NodeHeader::HasParent() const
            {
                return m_parentIndex != NodeHeader::INVALID_INDEX;
            }

            bool SceneGraph::NodeHeader::HasSibling() const
            {
                return m_siblingIndex != NodeHeader::INVALID_INDEX;
            }

            bool SceneGraph::NodeHeader::HasChild() const
            {
                return m_childIndex != NodeHeader::INVALID_INDEX;
            }

            bool SceneGraph::NodeHeader::IsEndPoint() const
            {
                return m_isEndPoint;
            }

            //
            // NodeIndex
            //
            bool SceneGraph::NodeIndex::IsValid() const
            {
                return m_value != NodeIndex::INVALID_INDEX;
            }

            bool SceneGraph::NodeIndex::operator==(NodeIndex rhs) const
            {
                return m_value == rhs.m_value;
            }

            bool SceneGraph::NodeIndex::operator!=(NodeIndex rhs) const
            {
                return m_value != rhs.m_value;
            }

            SceneGraph::NodeIndex::IndexType SceneGraph::NodeIndex::AsNumber() const
            {
                return m_value;
            }

            SceneGraph::NodeIndex::NodeIndex()
                : m_value(NodeIndex::INVALID_INDEX)
            {
            }

            SceneGraph::NodeIndex::NodeIndex(IndexType value)
                : m_value(value)
            {
            }

            //
            // SceneGraph
            //
            AZStd::shared_ptr<const DataTypes::IGraphObject> SceneGraph::ConstDataConverter(const AZStd::shared_ptr<DataTypes::IGraphObject>& value)
            {
                return AZStd::shared_ptr<const DataTypes::IGraphObject>(value);
            }

            SceneGraph::NodeIndex SceneGraph::GetRoot() const
            {
                return NodeIndex(0);
            }

            bool SceneGraph::HasNodeContent(NodeIndex node) const
            {
                return node.m_value < m_content.size() ? m_content[node.m_value] != nullptr : false;
            }

            bool SceneGraph::HasNodeSibling(NodeIndex node) const
            {
                return node.m_value < m_hierarchy.size() ? m_hierarchy[node.m_value].HasSibling() : false;
            }

            bool SceneGraph::HasNodeChild(NodeIndex node) const
            {
                return node.m_value < m_hierarchy.size() ? m_hierarchy[node.m_value].HasChild() : false;
            }

            bool SceneGraph::HasNodeParent(NodeIndex node) const
            {
                return node.m_value < m_hierarchy.size() ? m_hierarchy[node.m_value].HasParent() : false;
            }

            bool SceneGraph::IsNodeEndPoint(NodeIndex node) const
            {
                return node.m_value < m_hierarchy.size() ? m_hierarchy[node.m_value].IsEndPoint() : true;
            }

            AZStd::shared_ptr<DataTypes::IGraphObject> SceneGraph::GetNodeContent(NodeIndex node)
            {
                return node.m_value < m_content.size() ? m_content[node.m_value] : nullptr;
            }

            AZStd::shared_ptr<const DataTypes::IGraphObject> SceneGraph::GetNodeContent(NodeIndex node) const
            {
                return node.m_value < m_content.size() ? m_content[node.m_value] : nullptr;
            }

            SceneGraph::NodeIndex SceneGraph::GetNodeParent(NodeIndex node) const
            {
                return node.m_value < m_hierarchy.size() ? GetNodeParent(m_hierarchy[node.m_value]) : NodeIndex();
            }

            SceneGraph::NodeIndex SceneGraph::GetNodeParent(NodeHeader node) const
            {
                return NodeIndex(node.m_parentIndex != NodeHeader::INVALID_INDEX ? static_cast<uint32_t>(node.m_parentIndex) : NodeIndex::INVALID_INDEX);
            }

            SceneGraph::NodeIndex SceneGraph::GetNodeSibling(NodeIndex node) const
            {
                return node.m_value < m_hierarchy.size() ? GetNodeSibling(m_hierarchy[node.m_value]) : NodeIndex();
            }

            SceneGraph::NodeIndex SceneGraph::GetNodeSibling(NodeHeader node) const
            {
                return NodeIndex(node.m_siblingIndex != NodeHeader::INVALID_INDEX ? static_cast<uint32_t>(node.m_siblingIndex) : NodeIndex::INVALID_INDEX);
            }

            SceneGraph::NodeIndex SceneGraph::GetNodeChild(NodeIndex node) const
            {
                return node.m_value < m_hierarchy.size() ? GetNodeChild(m_hierarchy[node.m_value]) : NodeIndex();
            }

            SceneGraph::NodeIndex SceneGraph::GetNodeChild(NodeHeader node) const
            {
                return NodeIndex(node.m_childIndex != NodeHeader::INVALID_INDEX ? static_cast<uint32_t>(node.m_childIndex) : NodeIndex::INVALID_INDEX);
            }

            size_t SceneGraph::GetNodeCount() const
            {
                return m_hierarchy.size();
            }

            SceneGraph::HierarchyStorageConstData::iterator SceneGraph::ConvertToHierarchyIterator(NodeIndex node) const
            {
                return node.m_value < m_hierarchy.size() ? m_hierarchy.cbegin() + node.m_value : m_hierarchy.cend();
            }

            SceneGraph::NodeIndex SceneGraph::ConvertToNodeIndex(HierarchyStorageConstData::iterator iterator) const
            {
                return NodeIndex(iterator != m_hierarchy.cend() ? static_cast<uint32_t>(std::distance(m_hierarchy.cbegin(), iterator)) : NodeIndex::INVALID_INDEX);
            }

            SceneGraph::NodeIndex SceneGraph::ConvertToNodeIndex(NameStorageConstData::iterator iterator) const
            {
                return NodeIndex(iterator != m_names.cend() ? static_cast<uint32_t>(std::distance(m_names.cbegin(), iterator)) : NodeIndex::INVALID_INDEX);
            }

            SceneGraph::NodeIndex SceneGraph::ConvertToNodeIndex(ContentStorageData::iterator iterator) const
            {
                return NodeIndex(iterator != m_content.end() ? static_cast<uint32_t>(std::distance(m_content.begin(), iterator)) : NodeIndex::INVALID_INDEX);
            }

            SceneGraph::NodeIndex SceneGraph::ConvertToNodeIndex(ContentStorageConstData::iterator iterator) const
            {
                return NodeIndex(
                    iterator.GetBaseIterator() != m_content.cend() ?
                    aznumeric_caster(AZStd::distance(m_content.cbegin(), iterator.GetBaseIterator())) :
                    NodeIndex::INVALID_INDEX);
            }

            SceneGraph::HierarchyStorageConstData SceneGraph::GetHierarchyStorage() const
            {
                return HierarchyStorageConstData(m_hierarchy.begin(), m_hierarchy.end());
            }

            SceneGraph::NameStorageConstData SceneGraph::GetNameStorage() const
            {
                return NameStorageConstData(m_names.begin(), m_names.end());
            }

            SceneGraph::ContentStorageData SceneGraph::GetContentStorage()
            {
                return ContentStorageData(m_content.begin(), m_content.end());
            }

            SceneGraph::ContentStorageConstData SceneGraph::GetContentStorage() const
            {
                return ContentStorageConstData(
                    Views::MakeConvertIterator(m_content.cbegin(), ConstDataConverter),
                    Views::MakeConvertIterator(m_content.cend(), ConstDataConverter));
            }
        } // Containers
    } // SceneAPI
} // AZ
