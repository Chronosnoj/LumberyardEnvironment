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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#ifndef CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERFBX_SCENE_H
#define CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERFBX_SCENE_H
#pragma once



namespace MeshUtils
{
    class Mesh;
};


namespace Scene
{
    struct SNode
    {
        enum EAttributeType
        {
            eAttributeType_Mesh,
            eAttributeType_Unknown,
        };

        struct Attribute
        {
            EAttributeType eType;
            int index; // index in a scene array; array depends on attribute type (see IScene::GetMesh() for example)

            Attribute()
                : eType(eAttributeType_Unknown)
                , index(-1)
            {
            }

            Attribute(EAttributeType a_eType, int a_index)
                : eType(a_eType)
                , index(a_index)
            {
            }
        };

        string name;
        Matrix34 worldTransform; // allowed to be skewed, left- or right-handed, non-uniformly scaled
        std::vector<Attribute> attributes;

        int parent; // <0 if there is no parent; use IScene::GetNode() to access the parent node
        std::vector<int> children; // use IScene::GetNode(children[i]) to access i-th child node


        int FindFirstMatchingAttribute(EAttributeType eType) const
        {
            for (int i = 0; i < (int)attributes.size(); ++i)
            {
                if (attributes[i].eType == eType)
                {
                    return i;
                }
            }
            return -1;
        }
    };


    struct SMaterial
    {
        string name;
    };


    struct IScene
    {
        virtual ~IScene()
        {
        }

        // returns am ASCIIZ string in form "<signOfForwardAxis><forwardAxis><signOfUpAxis><upAxis>",
        // for example: "-Y+Z", "+Z+Y", "+X+Z", etc.
        virtual const char* GetForwardUpAxes() const = 0;

        virtual float GetUnitSizeInCentimeters() const = 0;

        virtual int GetNodeCount() const = 0;
        virtual const SNode* GetNode(int idx) const = 0;

        // use GetMaterial(MeshUtils::Mesh::m_faceMatIds[i]) to access materials referenced by faces
        virtual int GetMeshCount() const = 0;
        virtual const MeshUtils::Mesh* GetMesh(int idx) const = 0;

        virtual int GetMaterialCount() const = 0;
        virtual const SMaterial* GetMaterial(int idx) const = 0;
    };


    // See: http://en.wikipedia.org/wiki/Tree_traversal
    template<typename Visitor>
    inline void TraverseDepthFirstPreOrder(const IScene* pScene, int nodeIdx, const Visitor& visitor)
    {
        if (nodeIdx < 0 || !pScene)
        {
            return;
        }
        visitor(pScene->GetNode(nodeIdx));
        for (size_t i = 0, n = pScene->GetNode(nodeIdx)->children.size(); i < n; ++i)
        {
            TraverseDepthFirstPreOrder(pScene, pScene->GetNode(nodeIdx)->children[i], visitor);
        }
    }


    // See: http://en.wikipedia.org/wiki/Tree_traversal
    template<typename Visitor>
    inline void TraverseDepthFirstPostOrder(const IScene* pScene, int nodeIdx, const Visitor& visitor)
    {
        if (nodeIdx < 0 || !pScene)
        {
            return;
        }
        for (size_t i = 0, n = pScene->GetNode(nodeIdx)->children.size(); i < n; ++i)
        {
            TraverseDepthFirstPreOrder(pScene, pScene->GetNode(nodeIdx)->children[i], visitor);
        }
        visitor(pScene->GetNode(nodeIdx));
    }


    // Visit parent nodes from root to the passed node
    template<typename Visitor>
    inline void TraversePathFromRoot(const IScene* pScene, int nodeIdx, const Visitor& visitor)
    {
        if (nodeIdx < 0 || !pScene)
        {
            return;
        }
        TraversePathFromRoot(pScene, pScene->GetNode(nodeIdx)->parent, visitor);
        visitor(pScene->GetNode(nodeIdx));
    }


    // Visit parent nodes from passed node to the root
    template<typename Visitor>
    inline void TraversePathToRoot(const IScene* pScene, int nodeIdx, const Visitor& visitor)
    {
        if (nodeIdx < 0 || !pScene)
        {
            return;
        }
        visitor(pScene->GetNode(nodeIdx));
        TraversePathToRoot(pScene, pScene->GetNode(nodeIdx)->parent, visitor);
    }


    // Full name contains node names separated by separatorChar,
    // starting from the root note till the passed node.
    // Note: the name of the root node is excluded because the root node
    // assumed to be a helper that is invisible for the user.
    inline string GetEncodedFullName(const IScene* pScene, int nodeIdx)
    {
        if (nodeIdx < 0 || !pScene)
        {
            return string();
        }

        string name;

        TraversePathFromRoot(pScene, nodeIdx, [&name](const SNode* pNode)
        {
            if (pNode->parent >= 0)
            {
                static const char escapeChar = '^';
                static const char separatorChar = '/';
                if (!name.empty())
                {
                    name += separatorChar;
                }
                const char* p = pNode->name.c_str();
                while (*p)
                {
                    if (*p == escapeChar || *p == separatorChar)
                    {
                        name += escapeChar;
                    }
                    name += *p++;
                }
            }
        });

        return name;
    }
} // namespace Scene

#endif // CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERFBX_SCENE_H
