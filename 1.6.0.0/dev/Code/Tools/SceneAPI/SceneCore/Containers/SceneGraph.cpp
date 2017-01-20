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

#include <AzCore/Casting/numeric_cast.h>
#include <SceneAPI/SceneCore/Containers/SceneGraph.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            const SceneGraph::NodeIndex::IndexType SceneGraph::NodeIndex::INVALID_INDEX;

            static_assert(sizeof(SceneGraph::NodeIndex::IndexType) >= ((SceneGraph::NodeHeader::INDEX_BIT_COUNT / 8) + 1),
                "SceneGraph::NodeIndex is not big enough to store the parent index of a SceneGraph::NodeHeader");

            //
            // SceneGraph
            //
            SceneGraph::SceneGraph()
            {
                AddDefaultRoot();
            }

            SceneGraph::NodeIndex SceneGraph::Find(const char* path) const
            {
                return Find(std::string(path));
            }

            SceneGraph::NodeIndex SceneGraph::Find(const std::string& path) const
            {
                auto location = FindNameLookupIterator(path);
                return NodeIndex(location != m_nameLookup.end() ? (*location).second : NodeIndex::INVALID_INDEX);
            }

            SceneGraph::NodeIndex SceneGraph::Find(NodeIndex root, const char* relativePath) const
            {
                return Find(root, std::string(relativePath));
            }

            SceneGraph::NodeIndex SceneGraph::Find(NodeIndex root, const std::string& relativePath) const
            {
                if (root.m_value < m_names.size())
                {
                    std::string fullname = CombineName(m_names[root.m_value], relativePath.c_str());
                    return Find(fullname);
                }
                return NodeIndex();
            }

            const std::string& SceneGraph::GetNodeName(NodeIndex node) const
            {
                if (node.m_value < m_names.size())
                {
                    return m_names[node.m_value];
                }
                else
                {
                    static std::string invalidNodeName = "<Invalid>";
                    return invalidNodeName;
                }
            }

            SceneGraph::NodeIndex SceneGraph::AddChild(NodeIndex parent, const char* name)
            {
                return AddChild(parent, name, AZStd::shared_ptr<DataTypes::IGraphObject>(nullptr));
            }

            SceneGraph::NodeIndex SceneGraph::AddChild(NodeIndex parent, const char* name, const AZStd::shared_ptr<DataTypes::IGraphObject>& content)
            {
                return AddChild(parent, name, AZStd::shared_ptr<DataTypes::IGraphObject>(content));
            }

            SceneGraph::NodeIndex SceneGraph::AddChild(NodeIndex parent, const char* name, AZStd::shared_ptr<DataTypes::IGraphObject>&& content)
            {
                if (parent.m_value < m_hierarchy.size())
                {
                    NodeHeader& parentNode = m_hierarchy[parent.m_value];
                    if (parentNode.HasChild())
                    {
                        return AddSibling(NodeIndex(parentNode.m_childIndex), name, AZStd::move(content));
                    }
                    else
                    {
                        return NodeIndex(AppendChild(parent.m_value, name, AZStd::move(content)));
                    }
                }

                return NodeIndex();
            }

            SceneGraph::NodeIndex SceneGraph::AddSibling(NodeIndex sibling, const char* name)
            {
                return AddSibling(sibling, name, AZStd::shared_ptr<DataTypes::IGraphObject>(nullptr));
            }

            SceneGraph::NodeIndex SceneGraph::AddSibling(NodeIndex sibling, const char* name, const AZStd::shared_ptr<DataTypes::IGraphObject>& content)
            {
                return AddSibling(sibling, name, AZStd::shared_ptr<DataTypes::IGraphObject>(content));
            }

            SceneGraph::NodeIndex SceneGraph::AddSibling(NodeIndex sibling, const char* name, AZStd::shared_ptr<DataTypes::IGraphObject>&& content)
            {
                if (sibling.m_value < m_hierarchy.size())
                {
                    NodeIndex::IndexType siblingIndex = sibling.m_value;
                    while (m_hierarchy[siblingIndex].HasSibling())
                    {
                        siblingIndex = m_hierarchy[siblingIndex].m_siblingIndex;
                    }
                    return NodeIndex(AppendSibling(siblingIndex, name, AZStd::move(content)));
                }

                return NodeIndex();
            }

            bool SceneGraph::SetContent(NodeIndex node, const AZStd::shared_ptr<DataTypes::IGraphObject>& content)
            {
                if (node.m_value < m_content.size())
                {
                    m_content[node.m_value] = content;
                    return true;
                }
                return false;
            }

            bool SceneGraph::SetContent(NodeIndex node, AZStd::shared_ptr<DataTypes::IGraphObject>&& content)
            {
                if (node.m_value < m_content.size())
                {
                    m_content[node.m_value] = AZStd::move(content);
                    return true;
                }
                return false;
            }

            bool SceneGraph::MakeEndPoint(NodeIndex node)
            {
                if (node.m_value < m_hierarchy.size())
                {
                    m_hierarchy[node.m_value].m_isEndPoint = 1;
                    return true;
                }
                return false;
            }

            void SceneGraph::Clear()
            {
                m_nameLookup.clear();
                m_hierarchy.clear();
                m_names.clear();
                m_content.clear();

                AddDefaultRoot();
            }

            bool SceneGraph::IsValidName(const char* name)
            {
                if (!name)
                {
                    return false;
                }

                if (name[0] == 0)
                {
                    return false;
                }

                const char* current = name;
                while (*current)
                {
                    if (*current++ == s_nodeSeperationCharacter)
                    {
                        return false;
                    }
                }
                return true;
            }

            bool SceneGraph::IsValidName(const std::string& name)
            {
                if (name.size() > 0)
                {
                    return IsValidName(name.c_str());
                }
                else
                {
                    return false;
                }
            }

            std::string SceneGraph::GetShortName(const char* name)
            {
                if (!name)
                {
                    return "";
                }

                int seperatorPosition = -1;
                int position = 0;

                while (name[position] != 0)
                {
                    if (name[position] == s_nodeSeperationCharacter)
                    {
                        seperatorPosition = position;
                    }
                    position++;
                }
                return std::string(name + seperatorPosition + 1);
            }

            std::string SceneGraph::GetShortName(const std::string& name)
            {
                return name.substr(name.find_last_of(s_nodeSeperationCharacter) + 1);
            }

            char SceneGraph::GetNodeSeperationCharacter()
            {
                return s_nodeSeperationCharacter;
            }

            SceneGraph::NodeIndex::IndexType SceneGraph::AppendChild(NodeIndex::IndexType parent, const char* name, AZStd::shared_ptr<DataTypes::IGraphObject>&& content)
            {
                if (parent < m_hierarchy.size())
                {
                    NodeHeader parentNode = m_hierarchy[parent];
                    AZ_Assert(!parentNode.HasChild(), "Child '%s' couldn't be added as the target parent already contains a child.", name);
                    AZ_Assert(!parentNode.IsEndPoint(), "Attempting to add a child '%s' to node which is marked as an end point.", name);
                    if (!parentNode.HasChild() && !parentNode.IsEndPoint())
                    {
                        NodeIndex::IndexType nodeIndex = AppendNode(parent, name, AZStd::move(content));
                        m_hierarchy[parent].m_childIndex = nodeIndex;
                        return nodeIndex;
                    }
                }

                return NodeIndex::INVALID_INDEX;
            }

            SceneGraph::NodeIndex::IndexType SceneGraph::AppendSibling(NodeIndex::IndexType sibling, const char* name, AZStd::shared_ptr<DataTypes::IGraphObject>&& content)
            {
                if (sibling < m_hierarchy.size())
                {
                    NodeHeader siblingNode = m_hierarchy[sibling];
                    AZ_Assert(!siblingNode.HasSibling(), "Sibling '%s' couldn't be added as the target node already contains a sibling.", name);
                    if (!siblingNode.HasSibling())
                    {
                        NodeIndex::IndexType nodeIndex = AppendNode(siblingNode.m_parentIndex, name, AZStd::move(content));
                        m_hierarchy[sibling].m_siblingIndex = nodeIndex;
                        return nodeIndex;
                    }
                }

                return NodeIndex::INVALID_INDEX;
            }

            SceneGraph::NodeIndex::IndexType SceneGraph::AppendNode(NodeIndex::IndexType parentIndex, const char* name, AZStd::shared_ptr<DataTypes::IGraphObject>&& content)
            {
                NodeIndex::IndexType nodeIndex = aznumeric_caster(m_hierarchy.size());
                NodeHeader node;
                node.m_parentIndex = parentIndex;
                m_hierarchy.push_back(node);

                AZ_Assert(IsValidName(name), "Name '%s' for SceneGraph sibling contains invalid characters", name);
                std::string fullName = parentIndex != NodeHeader::INVALID_INDEX ? CombineName(m_names[parentIndex], name) : name;
                StringHash fullNameHash = StringHasher()(fullName);
                AZ_Assert(FindNameLookupIterator(fullNameHash, fullName) == m_nameLookup.end(), "Duplicate name found in SceneGraph: %s", fullName.c_str());
                m_nameLookup.insert(NameLookup::value_type(fullNameHash, nodeIndex));
                m_names.push_back(std::move(fullName));
                AZ_Assert(m_hierarchy.size() == m_names.size(), 
                    "Hierarchy and name lists in SceneGraph have gone out of sync. (%i vs. %i)", m_hierarchy.size(), m_names.size());

                m_content.push_back(AZStd::move(content));
                AZ_Assert(m_hierarchy.size() == m_content.size(), 
                    "Hierarchy and data lists in SceneGraph have gone out of sync. (%i vs. %i)", m_hierarchy.size(), m_content.size());

                return nodeIndex;
            }

            SceneGraph::NameLookup::const_iterator SceneGraph::FindNameLookupIterator(const std::string& name) const
            {
                StringHash hash = StringHasher()(name);
                return FindNameLookupIterator(hash, name);
            }

            SceneGraph::NameLookup::const_iterator SceneGraph::FindNameLookupIterator(StringHash hash, const std::string& name) const
            {
                auto range = m_nameLookup.equal_range(hash);
                // Always check the name, even if there's only one entry as the hash can be a clash with
                //      the single entry.
                for (auto it = range.first; it != range.second; ++it)
                {
                    if (m_names[it->second] == name)
                    {
                        return it;
                    }
                }

                return m_nameLookup.end();
            }

            std::string SceneGraph::CombineName(const std::string& path, const char* name) const
            {
                std::string result = path;
                if (result.length() > 0)
                {
                    result += s_nodeSeperationCharacter;
                }
                result += name;
                return result;
            }

            void SceneGraph::AddDefaultRoot()
            {
                AZ_Assert(m_hierarchy.size() == 0, "Adding a default root node to a SceneGraph with content.");

                m_hierarchy.push_back(NodeHeader());
                m_nameLookup.insert(NameLookup::value_type(StringHasher()(""), 0));
                m_names.emplace_back("");
                m_content.emplace_back(nullptr);
            }
        } // Containers
    } // SceneAPI
} // AZ
