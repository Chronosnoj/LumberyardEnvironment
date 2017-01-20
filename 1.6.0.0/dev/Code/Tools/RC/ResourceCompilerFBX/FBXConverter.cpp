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

#include "stdafx.h"

#include "Cry_Geo.h"
#include "Cry_Math.h"
#include "VertexFormats.h"
#include "CGFContent.h"

#include "IRCLog.h"
#include "FileUtil.h"
#include "Export/MeshUtils.h"
#include "Export/TransformHelpers.h"
#include "StringHelpers.h"
#include "Util.h"

#include "CryFbxScene.h"
#include "FBXConverter.h"
#include "Scene.h"

#include "XMLWriter.h"

//////////////////////////////////////////////////////////////////////////

namespace
{
    struct ExportMaterial
    {
    public:
        CMaterialCGF* m_pCgfMaterial;
        bool m_bOwnMaterial;

    public:
        ExportMaterial()
            : m_pCgfMaterial(0)
            , m_bOwnMaterial(false)
        {
        }

        ~ExportMaterial()
        {
            if (m_pCgfMaterial && m_bOwnMaterial)
            {
                delete m_pCgfMaterial;
            }
            // FIXME: It's not clear who is expected to delete m_pCgfMaterial->subMaterials[i]
        }

        int addMaterial(const char* name)
        {
            if (!name)
            {
                name = "";
            }

            if (!m_pCgfMaterial)
            {
                m_pCgfMaterial = new CMaterialCGF();
                m_bOwnMaterial = true;
                cry_strcpy(m_pCgfMaterial->name, "default");
                m_pCgfMaterial->nPhysicalizeType = PHYS_GEOM_TYPE_NONE;
            }

            for (int i = 0; i < (int)m_pCgfMaterial->subMaterials.size(); ++i)
            {
                if (stricmp(m_pCgfMaterial->subMaterials[i]->name, name) == 0)
                {
                    return i;
                }
            }

            CMaterialCGF* const p = new CMaterialCGF();
            cry_strcpy(p->name, name);
            p->nPhysicalizeType = PHYS_GEOM_TYPE_NONE; // TODO: we should also let the user choose PHYS_GEOM_TYPE_DEFAULT and/or PHYS_GEOM_TYPE_DEFAULT_PROXY
            m_pCgfMaterial->subMaterials.push_back(p);

            return int(m_pCgfMaterial->subMaterials.size()) - 1;
        }
    };


    struct ExportNode
    {
    public:
        ExportNode* m_pParent;
        std::vector<ExportNode*> m_children;
        int m_nodeIndexInScene; // used in Scene::IScene::GetNode() calls
        Matrix34 m_nodeToWorldTM; // node -> world (non-orthonormal)
        Matrix34 m_originToWorldTM; // origin -> world (orthonormal)
        Matrix34 m_nodeToOriginTM; // node -> origin (orthonormal)
        Matrix34 m_finalNodeToOriginTM; // node -> origin (orthonormal) in cgf file
        int m_indexInCgf;

    public:
        ExportNode()
            : m_pParent(0)
            , m_nodeIndexInScene(-1)
            , m_nodeToWorldTM(IDENTITY)
            , m_originToWorldTM(IDENTITY)
            , m_nodeToOriginTM(IDENTITY)
            , m_finalNodeToOriginTM(IDENTITY)
            , m_indexInCgf(-1)
        {
        }

        static void DeleteNodesRecursively(ExportNode* pNode)
        {
            if (pNode)
            {
                for (size_t i = 0; i < pNode->m_children.size(); ++i)
                {
                    DeleteNodesRecursively(pNode->m_children[i]);
                }
                delete pNode;
            }
        }

        template<typename Visitor>
        static void TraverseDepthFirstPreOrder(ExportNode* p, const Visitor& visitor)
        {
            if (!p)
            {
                return;
            }
            visitor(p);
            for (int i = 0, n = (int)p->m_children.size(); i < n; ++i)
            {
                TraverseDepthFirstPreOrder(p->m_children[i], visitor);
            }
        }
    };
} // namespace


static bool SceneNodeHasMesh(
    const Scene::IScene* pScene,
    int nodeIndex)
{
    if (pScene &&
        pScene->GetNode(nodeIndex) &&
        pScene->GetNode(nodeIndex)->FindFirstMatchingAttribute(Scene::SNode::eAttributeType_Mesh) >= 0)
    {
        return true;
    }
    return false;
}


static bool SceneNodeNameIsMatching(
    const Scene::IScene* pScene,
    int nodeIndex,
    const std::vector<string>& names)
{
    if (!pScene || names.empty())
    {
        return false;
    }

    const string name = Scene::GetEncodedFullName(pScene, nodeIndex);

    for (size_t i = 0; i < names.size(); ++i)
    {
        if (names[i].empty() || StringHelpers::EqualsIgnoreCase(name, names[i]))
        {
            return true;
        }
    }

    return false;
}


static void CollectSceneNodesWithMatchingNames(
    Scene::IScene* pScene,
    int nodeIndex,
    const std::vector<string>& names,
    std::vector<int>& collected)
{
    if (!pScene || nodeIndex < 0)
    {
        return;
    }
    if (nodeIndex == 0 || (!names.empty() && !SceneNodeNameIsMatching(pScene, nodeIndex, names)))
    {
        for (size_t i = 0; i < pScene->GetNode(nodeIndex)->children.size(); ++i)
        {
            CollectSceneNodesWithMatchingNames(pScene, pScene->GetNode(nodeIndex)->children[i], names, collected);
        }
    }
    else
    {
        collected.push_back(nodeIndex);
    }
}


static void CreateExportNodesForSceneSubtree(ExportNode* pParent, const Scene::IScene* pScene, int nodeIndex)
{
    if (!pParent || !pScene || nodeIndex < 0)
    {
        return;
    }

    ExportNode* const pNode = new ExportNode();
    pNode->m_pParent = pParent;
    pNode->m_nodeIndexInScene = nodeIndex;
    pParent->m_children.push_back(pNode);

    for (size_t i = 0; i < pScene->GetNode(nodeIndex)->children.size(); ++i)
    {
        CreateExportNodesForSceneSubtree(pNode, pScene, pScene->GetNode(nodeIndex)->children[i]);
    }
}


static bool ExportNodeHasMeshInSubtree(const ExportNode* p, const Scene::IScene* pScene)
{
    if (!p || !pScene)
    {
        return false;
    }
    if (SceneNodeHasMesh(pScene, p->m_nodeIndexInScene))
    {
        return true;
    }
    for (int i = 0, n = (int)p->m_children.size(); i < n; ++i)
    {
        if (ExportNodeHasMeshInSubtree(p->m_children[i], pScene))
        {
            return true;
        }
    }
    return false;
}


namespace
{
    struct ImportRequest
    {
    public:
        string missingAttribute;
        string sourceAssetFilename;
        string sourceUnitSizeText;
        float scale;
        string originName;
        std::vector<string> nodeNames;

    public:
        ImportRequest()
        {
            Clear();
        }

        void Clear()
        {
            missingAttribute = "\x01"; // an impossible text
            sourceAssetFilename = missingAttribute;
            sourceUnitSizeText = missingAttribute;
            scale = -1;
            originName = missingAttribute;
            nodeNames.clear();
        }
    };
}


static bool LoadImportRequest(
    IResourceCompiler* pRc,
    const char* xmlFilename,
    ImportRequest& ir)
{
    XmlNodeRef root = pRc->LoadXml(xmlFilename);
    if (!root)
    {
        RCLogError("Asset converter: failed to load asset xml file '%s'", xmlFilename);
        return false;
    }

    const char* assetType = 0;
    if (!root->getAttr("assettype", &assetType))
    {
        RCLogError("Asset converter: missing 'asset_type' attribute in asset xml file '%s'", xmlFilename);
        return false;
    }

    if (!StringHelpers::EqualsIgnoreCase(assetType, "static_mesh"))
    {
        RCLogError("Asset converter: non-supported value '%s' of attribute 'assettype' in asset xml file '%s'", assetType, xmlFilename);
        return false;
    }

    const char* fileType = 0;
    if (!root->getAttr("filetype", &fileType))
    {
        RCLogError("Asset converter: missing 'filetype' attribute in asset xml file '%s'", xmlFilename);
        return false;
    }

    if (!StringHelpers::EqualsIgnoreCase(fileType, "cryfbx"))
    {
        RCLogError("Asset converter: non-supported value '%s' of attribute 'filetype' in asset xml file '%s'", fileType, xmlFilename);
        return false;
    }

    const char* fileName = 0;
    if (!root->getAttr("filename", &fileName) || !fileName)
    {
        RCLogError("Asset converter: missing 'filename' attribute in asset xml file '%s'", xmlFilename);
        return false;
    }

    ir.sourceAssetFilename = fileName;

    {
        const char* p = 0;
        if (root->getAttr("sourceunitsize", &p) && p)
        {
            ir.sourceUnitSizeText = p;
        }
    }

    {
        float tmpScale = 0;
        if (root->getAttr("scale", tmpScale))
        {
            ir.scale = tmpScale;
        }
    }

    {
        const char* p = 0;
        if (root->getAttr("origin", &p) && p)
        {
            ir.originName = p;
        }
    }

    for (int i = 0; i < root->getChildCount(); ++i)
    {
        XmlNodeRef item = root->getChild(i);

        if (!StringHelpers::EqualsIgnoreCase(item->getTag(), "item"))
        {
            continue;
        }

        const char* nodeName = 0;
        if (!item->getAttr("node", &nodeName))
        {
            continue;
        }

        ir.nodeNames.push_back(nodeName);
    }

    return true;
}


// Based on CLoaderCGF::LoadGeomChunk() and ColladaLoader.cpp's CreateCMeshFromMeshUtilsMesh()
static CMesh* CreateCMesh(const MeshUtils::Mesh& a_mesh, const std::vector<int>& remapMatIds, const Matrix34& transform)
{
    MeshUtils::Mesh mesh;
    mesh = a_mesh;

    // Remap per-face material ids as requested
    for (int i = 0, numFaces = mesh.GetFaceCount(); i < numFaces; ++i)
    {
        const int matId = mesh.m_faceMatIds[i];
        if (matId < 0 || matId >= (int)remapMatIds.size())
        {
            RCLogError("Internal error: incorrect material data detected in %s", __FUNCTION__);
            return 0;
        }
        const int newMatId = remapMatIds[matId];
        if (newMatId < 0)
        {
            RCLogError("Internal error: incorrect remapping data detected in %s", __FUNCTION__);
            return 0;
        }
        mesh.m_faceMatIds[i] = newMatId;
    }

    // Cleaning source mesh
    {
        if (mesh.m_links.empty())
        {
            const int maxBoneLinkCount = 4;  // TODO: pass it in parameters

            for (size_t i = 0, n = mesh.m_links.size(); i < n; ++i)
            {
                const char* const err = mesh.m_links[i].Normalize(MeshUtils::VertexLinks::eSort_ByWeight, 0.0f, maxBoneLinkCount);
                if (err)
                {
                    RCLogWarning("%s: Failed to process vertex bone linking: %s", __FUNCTION__, err);
                    return 0;
                }
            }
        }

        mesh.RemoveDegradedFaces();

        // Prevent sharing materials by vertices (this call might create new vertices)
        mesh.SetVertexMaterialIdsFromFaceMaterialIds();

        mesh.ComputeVertexRemapping();

        // Delete duplicate vertices (note that mesh.m_vertexNewToOld[] and mesh.m_vertexOldToNew[] are not modified)
        mesh.RemoveVerticesByUsingComputedRemapping();

        mesh.RemoveDegenerateFaces();
    }

    // Validation
    {
        const char* const err = mesh.Validate();
        if (err)
        {
            RCLogWarning("%s: Failed: %s", __FUNCTION__, err);
            return 0;
        }
    }

    // Creating and filling CMesh

    const int numFaces = mesh.GetFaceCount();
    const int numVertices = mesh.GetVertexCount();
    const bool hasTexCoords = !mesh.m_texCoords.empty();
    const bool hasColor = !mesh.m_colors.empty();
    const bool hasBoneMaps = !mesh.m_links.empty();

    CMesh* const pMesh = new CMesh();

    pMesh->SetFaceCount(numFaces);
    pMesh->SetVertexCount(numVertices);
    pMesh->ReallocStream(CMesh::TOPOLOGY_IDS, numVertices);
    pMesh->ReallocStream(CMesh::TEXCOORDS, hasTexCoords ? numVertices : 0);

    if (hasColor)
    {
        pMesh->ReallocStream(CMesh::COLORS_0, numVertices);
    }

    bool hasExtraWeights = false;
    if (hasBoneMaps)
    {
        for (int i = 0; i < numVertices; ++i)
        {
            if (mesh.m_links[i].links.size() > 4)
            {
                hasExtraWeights = true;
                break;
            }
        }

        pMesh->ReallocStream(CMesh::BONEMAPPING, numVertices);
        if (hasExtraWeights)
        {
            pMesh->ReallocStream(CMesh::EXTRABONEMAPPING, numVertices);
        }
    }

    const Matrix33 transformNormals = Matrix33(transform).GetInverted().GetTransposed();

    pMesh->m_bbox.Reset();

    for (int i = 0; i < numVertices; ++i)
    {
        pMesh->m_pTopologyIds[i] = mesh.m_topologyIds[i];

        pMesh->m_pPositions[i] = transform.TransformPoint(mesh.m_positions[i]);
        pMesh->m_bbox.Add(mesh.m_positions[i]);

        pMesh->m_pNorms[i] = SMeshNormal(transformNormals.TransformVector(mesh.m_normals[i]).normalize());

        if (hasTexCoords)
        {
            pMesh->m_pTexCoord[i] = SMeshTexCoord(mesh.m_texCoords[i].x, mesh.m_texCoords[i].y);
        }
        else
        {
            pMesh->m_pTexCoord[i] = SMeshTexCoord(0.0f, 0.0f);
        }

        if (hasColor)
        {
            pMesh->m_pColor0[i] = SMeshColor(
                    mesh.m_colors[i].r,
                    mesh.m_colors[i].g,
                    mesh.m_colors[i].b,
                    mesh.m_alphas[i]);
        }

        if (hasBoneMaps)
        {
            const MeshUtils::VertexLinks& inLinks = mesh.m_links[i];

            for (int b = 0; b < 4; ++b)
            {
                if (b < inLinks.links.size())
                {
                    pMesh->m_pBoneMapping[i].weights[b] = (int)Util::getClamped(255.0f * inLinks.links[b].weight, 0.0f, 255.0f);
                    pMesh->m_pBoneMapping[i].boneIds[b] = inLinks.links[b].boneId;
                }
            }

            if (hasExtraWeights)
            {
                for (int b = 4; b < 8; ++b)
                {
                    if (b < inLinks.links.size())
                    {
                        pMesh->m_pExtraBoneMapping[i].weights[b - 4] = (int)Util::getClamped(255.0f * inLinks.links[b].weight, 0.0f, 255.0f);
                        pMesh->m_pExtraBoneMapping[i].boneIds[b - 4] = inLinks.links[b].boneId;
                    }
                }
            }
        }
    }

    // Setting faces and subsets.
    // Note that in a subset we set nMatID only, other members are not set - it's
    // enough for MeshCompiler to work properly (MeshCompiler is expected to be
    // called later).
    {
        int minMatId = INT_MAX;
        int maxMatId = INT_MIN;

        for (int i = 0; i < numFaces; ++i)
        {
            const int matId = mesh.m_faceMatIds[i];
            minMatId = Util::getMin(minMatId, matId);
            maxMatId = Util::getMax(maxMatId, matId);
        }

        if (minMatId > maxMatId)
        {
            RCLogWarning("%s: Empty mesh", __FUNCTION__);
            delete pMesh;
            return 0;
        }

        if (minMatId < 0)
        {
            RCLogWarning("%s: Internal error", __FUNCTION__);
            delete pMesh;
            return 0;
        }

        std::vector<int> mapMatIdToSubset;

        mapMatIdToSubset.resize(maxMatId - minMatId + 1, -1);

        for (int i = 0; i < numFaces; ++i)
        {
            mapMatIdToSubset[mesh.m_faceMatIds[i] - minMatId] = 0;
        }

        pMesh->m_subsets.clear();

        for (int i = 0; i < (int)mapMatIdToSubset.size(); ++i)
        {
            if (mapMatIdToSubset[i] >= 0)
            {
                mapMatIdToSubset[i] = (int)pMesh->m_subsets.size();
                pMesh->m_subsets.push_back();
                pMesh->m_subsets[pMesh->m_subsets.size() - 1].nMatID = minMatId + i;
            }
        }

        for (int i = 0; i < numFaces; ++i)
        {
            pMesh->m_pFaces[i].nSubset = mapMatIdToSubset[mesh.m_faceMatIds[i] - minMatId];

            for (int j = 0; j < 3; ++j)
            {
                const int vIdx = mesh.m_faces[i].vertexIndex[j];
                assert(vIdx >= 0 && vIdx < numVertices);
                pMesh->m_pFaces[i].v[j] = vIdx;
            }
        }
    }

    return pMesh;
}

//////////////////////////////////////////////////////////////////////////

// note: filename passed is used in error messages only
static bool ValidateSceneHierarchy(const char* filename, Scene::IScene* pScene)
{
    if (!filename)
    {
        filename = "";
    }

    if (!pScene)
    {
        RCLogError("%s: Internal error. Missing scene pointer for '%s'.", __FUNCTION__, filename);
        return false;
    }

    if (pScene->GetNodeCount() <= 0)
    {
        RCLogError("Failed to load '%s'.", filename);
        return false;
    }

    if (pScene->GetNodeCount() == 1)
    {
        RCLogError("Scene in '%s' is empty (no nodes).", filename);
        return false;
    }

    // Validating links to parents

    if (pScene->GetNode(0)->parent >= 0)
    {
        RCLogError("Scene's root node in '%s' has a parent.", filename);
        return false;
    }

    for (int i = 1; i < pScene->GetNodeCount(); ++i)
    {
        if (pScene->GetNode(i)->parent < 0)
        {
            RCLogError("Scene in '%s' has multiple roots.", filename);
            return false;
        }

        if (pScene->GetNode(i)->parent == i)
        {
            RCLogError("Node %d in scene in '%s' has bad link to parent.", i, filename);
            return false;
        }

        if (pScene->GetNode(i)->parent > i)
        {
            RCLogError("Node %d in scene in '%s' has a forward-referenced parent.", i, filename);
            return false;
        }
    }

    // Validating children

    std::set<int> tmpIntSet;

    for (int i = 0; i < pScene->GetNodeCount(); ++i)
    {
        const std::vector<int>& children = pScene->GetNode(i)->children;

        tmpIntSet.clear();

        for (int j = 0; j < (int)children.size(); ++j)
        {
            if (children[j] < 0 || children[j] >= pScene->GetNodeCount())
            {
                RCLogError("Node %d in scene in '%s' has a invalid link (%d) to a child.", i, filename);
                return false;
            }

            if (children[j] == i)
            {
                RCLogError("Node %d in scene in '%s' has bad link to a child.", i, filename);
                return false;
            }

            if (children[j] < i)
            {
                RCLogError("Node %d in scene in '%s' has a back-referenced child.", i, filename);
                return false;
            }

            const Scene::SNode* const pChild = pScene->GetNode(children[j]);

            if (pChild->parent != i)
            {
                RCLogError("Child of node %d in scene in '%s' has wrong parent link (%d).", i, filename, pChild->parent);
                return false;
            }

            const size_t n = tmpIntSet.size();
            tmpIntSet.insert(children[j]);
            if (n == tmpIntSet.size())
            {
                RCLogError("Child of node %d in scene in '%s' has duplicate child link (%d).", i, filename, children[j]);
                return false;
            }
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////

static string MakeEscapedXmlString(const string& str)
{
    string s = str;

    s.replace("&", "&amp;");
    s.replace("\"", "&quot;");
    s.replace("\'", "&apos;");
    s.replace("<", "&lt;");
    s.replace(">", "&gt;");

    return s;
}


static void CreateManifestFile(const string& filename, Scene::IScene* pScene)
{
    XMLFileSink sink(filename);
    XMLWriter writer(&sink);

    XMLWriter::Element elemRoot(writer, "scene");

    {
        char str[30];
        _snprintf_s(str, sizeof(str), _TRUNCATE, "%g", pScene->GetUnitSizeInCentimeters());
        elemRoot.Attribute("unitSizeInCm", str);
    }

    {
        XMLWriter::Element elemNodes(writer, "nodes");

        for (int i = 0; i < pScene->GetNodeCount(); ++i)
        {
            XMLWriter::Element elemNode(writer, "node");

            const Scene::SNode* const pSceneNode = pScene->GetNode(i);

            elemNode.Attribute("index", i);  // just for reference/debug

            elemNode.Attribute("name", MakeEscapedXmlString(pSceneNode->name));

            elemNode.Attribute("parentNodeIndex", pSceneNode->parent);

            for (int j = 0; j < (int)pSceneNode->attributes.size(); ++j)
            {
                XMLWriter::Element el(writer, "attribute");

                const Scene::SNode::Attribute& attrib = pSceneNode->attributes[j];

                if (attrib.eType == Scene::SNode::eAttributeType_Mesh)
                {
                    el.Attribute("type", "mesh");
                    el.Attribute("meshIndex", attrib.index);
                }
                else
                {
                    el.Attribute("type", "unknown");
                }
            }
        }
    }

    {
        XMLWriter::Element elemNodes(writer, "meshes");

        for (int i = 0; i < pScene->GetMeshCount(); ++i)
        {
            XMLWriter::Element elemMesh(writer, "mesh");

            elemMesh.Attribute("index", i);  // just for reference/debug

            const MeshUtils::Mesh* const pMesh = pScene->GetMesh(i);
            if (pMesh)
            {
                elemMesh.Attribute("triFaceCount", pMesh->GetFaceCount());
            }
        }
    }
}


static bool ValidateSceneAndCreateManifestFile(Scene::IScene* pScene, const string& sSceneFile, const string& sManifestFile)
{
    if (!ValidateSceneHierarchy(sSceneFile.c_str(), pScene))
    {
        return false;
    }

    RCLog("Creating manifest file '%s'", sManifestFile.c_str());

    bool bSuccess = true;
    try
    {
        CreateManifestFile(sManifestFile, pScene);
    }
    catch (...)
    {
        bSuccess = false;
    }

    if (!bSuccess || !FileUtil::FileExists(sManifestFile.c_str()))
    {
        RCLogError("Failed to create manifest file '%s' for '%s'", sManifestFile.c_str(), sSceneFile.c_str());
        ::remove(sManifestFile.c_str());
        return false;
    }

    return true;
}


static bool ValidateAndExportScene(
    Scene::IScene* pScene,
    const ImportRequest& ir,
    const string& sImportRequestFile,
    const string& sSceneFile,
    IResourceCompiler* pRC)
{
    if (!ValidateSceneHierarchy(ir.sourceAssetFilename.c_str(), pScene))
    {
        return false;
    }

    // Compute coordinate system transform to convert everything to CryEngine's CS
    Matrix34 axesTM;
    {
        const char* const sceneForwardUp = pScene->GetForwardUpAxes();
        const char* const cryEngineForwardUp = "-Y+Z";

        RCLog("Converting scene's forward & up coordinate axes '%s' to Lumberyard's '%s'", sceneForwardUp, cryEngineForwardUp);

        const char* const err = TransformHelpers::ComputeForwardUpAxesTransform(axesTM, sceneForwardUp, cryEngineForwardUp);

        if (err)
        {
            RCLogError("Failed to compute scene transform: %s", err);
            return false;
        }
    }

    // Scale
    float scale;
    {
        scale = ir.scale;
        if (scale <= 0)
        {
            scale = 1.0f;
        }

        RCLog("User scale: %f", ir.scale);
    }

    // Unit conversion & final scale
    {
        double sourceUnitSizeInCm;

        // Missing, empty, or "file" unit means that the user wants to use unit
        // settings from the file.
        // "mm", "cm", "ft" etc means that the user wants to assume that unit
        // size is "cm", "m" etc (ignoring unit settings from the file).

        if (ir.sourceUnitSizeText.empty() ||
            StringHelpers::EqualsIgnoreCase(ir.sourceUnitSizeText, ir.missingAttribute) ||
            StringHelpers::EqualsIgnoreCase(ir.sourceUnitSizeText, "file"))
        {
            sourceUnitSizeInCm = pScene->GetUnitSizeInCentimeters();
        }
        else
        {
            struct Unit
            {
                const char* name;
                const char* fullName;
                double sizeInCm;
            } units[] =
            {
                { "mm", "millimeters", 0.1 },
                { "cm", "centimeters", 1.0 },
                { "in", "inches", 2.54 },
                { "dm", "decimeters", 10.0 },
                { "ft", "feet", 30.48 },
                { "yd", "yards", 91.44 },
                { "m", "meters", 100.0 },
                { "km", "kilometers", 100000.0 }
            };

            sourceUnitSizeInCm = -1;

            for (int i = 0; i < sizeof(units) / sizeof(units[0]); ++i)
            {
                if (StringHelpers::EqualsIgnoreCase(ir.sourceUnitSizeText, units[i].name) ||
                    StringHelpers::EqualsIgnoreCase(ir.sourceUnitSizeText, units[i].fullName))
                {
                    sourceUnitSizeInCm = units[i].sizeInCm;
                    break;
                }
            }

            if (sourceUnitSizeInCm < 0)
            {
                RCLog("Unknown source unit size specified: '%s'. Using unit settings from the asset file.", ir.sourceUnitSizeText.c_str());
                sourceUnitSizeInCm = pScene->GetUnitSizeInCentimeters();
            }
        }

        RCLog("Source unit size: %fcm", sourceUnitSizeInCm);

        // Modify final scale by applying unit conversion to meters
        const double cmToM = 0.01;
        scale *= float(sourceUnitSizeInCm * cmToM);

        RCLog("Final scale (includes user scale and conversion to meters): %f", scale);
    }

    ExportNode* pRootExportNode = 0;

    {
        std::vector<int> matchingNodes;
        CollectSceneNodesWithMatchingNames(pScene, 0, ir.nodeNames, matchingNodes);

        RCLog("Matching nodes/hierarchies: %d", (int)matchingNodes.size());

        // Create export tree

        pRootExportNode = new ExportNode();
        pRootExportNode->m_nodeIndexInScene = 0;

        for (size_t i = 0; i < matchingNodes.size(); ++i)
        {
            CreateExportNodesForSceneSubtree(pRootExportNode, pScene, matchingNodes[i]);
        }

        if (!ExportNodeHasMeshInSubtree(pRootExportNode, pScene))
        {
            RCLogError("Cannot find mesh nodes to export");
            ExportNode::DeleteNodesRecursively(pRootExportNode);
            return false;
        }
    }

    std::vector<ExportNode*> nodes;

    ExportNode::TraverseDepthFirstPreOrder(pRootExportNode, [&nodes](ExportNode* p)
    {
        nodes.push_back(p);
    });

    if (nodes.size() < 2)
    {
        RCLogError("Internal error in %s", __FUNCTION__);
        ExportNode::DeleteNodesRecursively(pRootExportNode);
        return false;
    }

    for (size_t i = 0; i < nodes.size(); ++i)
    {
        nodes[i]->m_indexInCgf = (int)i - 1;  // '- 1' because we don't export root node
    }

    // CryEngine's file formats require root nodes to have *identity*
    // transform matrices. The code below computes transforms
    // depending on the requested 'origin' mode (see 'origin' attribute
    // in the import request file).
    //
    // Note: CryEngine simply ignores transforms of parent-less CGF nodes,
    // so we must provide transforms that are compatible with this approach.
    {
        enum EOrigin
        {
            eOrigin_Local,  // origins are at the pivots of the topmost imported nodes
            eOrigin_World,  // origin is the world's origin
            eOrigin_Node,   // origin is at the pivot of a node
        };

        EOrigin eOrigin;
        int originNodeIndexInScene = -1;  // used only for eOrigin_Node

        if (StringHelpers::EqualsIgnoreCase(ir.originName, "/world") ||
            ir.originName.empty())
        {
            eOrigin = eOrigin_World;
            RCLog("Origin: world");
        }
        else if (StringHelpers::EqualsIgnoreCase(ir.originName, "/local") ||
                 StringHelpers::EqualsIgnoreCase(ir.originName, ir.missingAttribute))
        {
            eOrigin = eOrigin_Local;
            RCLog("Origin: local");
        }
        else
        {
            std::vector<string> arrayWithPivotName;
            arrayWithPivotName.push_back(ir.originName);
            for (int i = 1, n = pScene->GetNodeCount(); i < n; ++i)
            {
                if (SceneNodeNameIsMatching(pScene, i, arrayWithPivotName))
                {
                    originNodeIndexInScene = i;
                    break;
                }
            }

            if (originNodeIndexInScene >= 0)
            {
                eOrigin = eOrigin_Node;
                RCLog("Origin: node '%s'", ir.originName.c_str());
            }
            else
            {
                RCLogWarning("Cannot find pivot node '%s', using world instead.", ir.originName.c_str());
                eOrigin = eOrigin_World;
                RCLog("Origin: world");
            }
        }

        if (eOrigin == eOrigin_World)
        {
            ExportNode::TraverseDepthFirstPreOrder(nodes[0], [](ExportNode* pNode)
            {
                pNode->m_originToWorldTM.SetIdentity();
            });
        }
        else if (eOrigin == eOrigin_Node)
        {
            Vec3 basePos;
            {
                const Matrix34 nodeToWorldTM =
                    Matrix34::CreateScale(Vec3(scale, scale, scale)) *
                    axesTM *
                    pScene->GetNode(originNodeIndexInScene)->worldTransform;

                basePos.x = nodeToWorldTM.m03;
                basePos.y = nodeToWorldTM.m13;
                basePos.z = nodeToWorldTM.m23;
            }

            ExportNode::TraverseDepthFirstPreOrder(nodes[0], [&basePos](ExportNode* pNode)
            {
                pNode->m_originToWorldTM.SetTranslationMat(basePos);
            });
        }
        else
        {
            assert(eOrigin == eOrigin_Local);

            Vec3 basePos(ZERO);

            ExportNode::TraverseDepthFirstPreOrder(nodes[0], [&](ExportNode* pNode)
            {
                const ExportNode* const pParentNode = pNode->m_pParent;

                if (!pParentNode || pParentNode == nodes[0])
                {
                    const Matrix34 nodeToWorldTM =
                        Matrix34::CreateScale(Vec3(scale, scale, scale)) *
                        axesTM *
                        pScene->GetNode(pNode->m_nodeIndexInScene)->worldTransform;

                    basePos.x = nodeToWorldTM.m03;
                    basePos.y = nodeToWorldTM.m13;
                    basePos.z = nodeToWorldTM.m23;
                }

                pNode->m_originToWorldTM.SetTranslationMat(basePos);
            });
        }

        // Compute 'original node'->world transforms (with applied forward/up axes transform and scaling)
        ExportNode::TraverseDepthFirstPreOrder(nodes[0], [&](ExportNode* pNode)
        {
            pNode->m_nodeToWorldTM =
                Matrix34::CreateScale(Vec3(scale, scale, scale)) *
                axesTM *
                pScene->GetNode(pNode->m_nodeIndexInScene)->worldTransform;
        });

        // Compute node->origin transforms (orthonormal)
        ExportNode::TraverseDepthFirstPreOrder(nodes[0], [&](ExportNode* pNode)
        {
            pNode->m_nodeToOriginTM = TransformHelpers::ComputeOrthonormalMatrix(pNode->m_originToWorldTM.GetInvertedFast() * pNode->m_nodeToWorldTM);
        });
    }

    // Debug printing of exported nodes
    if (pRC->GetVerbosityLevel() > 0)
    {
        RCLog("Nodes in scene to be exported:");
        for (size_t i = 1; i < nodes.size(); ++i)
        {
            RCLog("[%i]: %s '%s'",
                (int)i,
                (SceneNodeHasMesh(pScene, nodes[i]->m_nodeIndexInScene) ? "mesh  " : "helper"),
                pScene->GetNode(nodes[i]->m_nodeIndexInScene)->name.c_str());
            if (pRC->GetVerbosityLevel() > 1)
            {
                Matrix34 m;
                m = pScene->GetNode(nodes[i]->m_nodeIndexInScene)->worldTransform;
                RCLog("  original scene tmWorld: X(%g %g %g) Y(%g %g %g) Z(%g %g %g) T(%g %g %g)",
                    m.m00, m.m10, m.m20,
                    m.m01, m.m11, m.m21,
                    m.m02, m.m12, m.m22,
                    m.m03, m.m13, m.m23);
                m = nodes[i]->m_nodeToOriginTM;
                RCLog("  nodeToOriginTM: X(%g %g %g) Y(%g %g %g) Z(%g %g %g) T(%g %g %g)",
                    m.m00, m.m10, m.m20,
                    m.m01, m.m11, m.m21,
                    m.m02, m.m12, m.m22,
                    m.m03, m.m13, m.m23);
            }
        }
    }

    // Create and fill resulting asset

    IAssetWriter* const pAssetWriter = pRC->GetAssetWriter();
    if (!pAssetWriter)
    {
        ExportNode::DeleteNodesRecursively(pRootExportNode);
        return false;
    }

    CContentCGF cgf(PathHelpers::ReplaceExtension(sImportRequestFile, "cgf").c_str());

    {
        CExportInfoCGF* const pExportInfo = cgf.GetExportInfo();

        pExportInfo->bCompiledCGF = false;
        pExportInfo->bMergeAllNodes = true;
        pExportInfo->bHavePhysicsProxy = true;
        cry_strcpy(pExportInfo->rc_version_string, "scene import");
    }

    // Prepare materials

    ExportMaterial exportMaterial;
    std::vector<int> exportMatIds;  // for every IScene's material stores either -1 (for unused materials) or final/exported mat id (for used materials)

    {
        // Init materials as "unused"
        exportMatIds.resize(pScene->GetMaterialCount(), -1);

        // Mark used materials
        for (size_t i = 1; i < nodes.size(); ++i)
        {
            if (SceneNodeHasMesh(pScene, nodes[i]->m_nodeIndexInScene))
            {
                const int attrIdx = pScene->GetNode(nodes[i]->m_nodeIndexInScene)->FindFirstMatchingAttribute(Scene::SNode::eAttributeType_Mesh);
                const int meshIndex = pScene->GetNode(nodes[i]->m_nodeIndexInScene)->attributes[attrIdx].index;

                const MeshUtils::Mesh* const pMesh = pScene->GetMesh(meshIndex);
                if (pMesh)
                {
                    const int numFaces = pMesh->GetFaceCount();

                    for (int j = 0; j < numFaces; ++j)
                    {
                        const int matId = pMesh->m_faceMatIds[j];
                        if (matId < 0 || matId >= (int)exportMatIds.size())
                        {
                            RCLogError("Internal error in material handling code");
                            return false;
                        }
                        exportMatIds[matId] = 1;
                    }
                }
            }
        }

        // Compute final (exported) mat id for every used material
        for (size_t i = 0; i < exportMatIds.size(); ++i)
        {
            if (exportMatIds[i] < 0)
            {
                continue;
            }

            const Scene::SMaterial* const pMat = pScene->GetMaterial(i);
            if (!pMat)
            {
                RCLogError("Internal error in material handling code");
                return false;
            }

            exportMatIds[i] = exportMaterial.addMaterial(pMat->name.c_str());
        }

        // Debug printing of used materials
        if (pRC->GetVerbosityLevel() > 1)
        {
            RCLog("Used materials (%d):", (int)exportMaterial.m_pCgfMaterial->subMaterials.size());
            for (size_t i = 0; i < exportMatIds.size(); ++i)
            {
                if (exportMatIds[i] >= 0)
                {
                    RCLog("\t'%s'", pScene->GetMaterial(i)->name.c_str());
                }
            }
        }
    }

    for (size_t i = 1; i < nodes.size(); ++i)
    {
        // Fill CGF node params

        CNodeCGF* const pCGFNode = new CNodeCGF(); // will be auto deleted by CContentCGF cgf

        cry_strcpy(pCGFNode->name, pScene->GetNode(nodes[i]->m_nodeIndexInScene)->name.c_str());

        pCGFNode->pos_cont_id = 0;
        pCGFNode->rot_cont_id = 0;
        pCGFNode->scl_cont_id = 0;

        {
            const ExportNode* const p = nodes[i]->m_pParent;
            if (!p || p == nodes[0])
            {
                nodes[i]->m_finalNodeToOriginTM = Matrix34(IDENTITY);
                pCGFNode->localTM = Matrix34(IDENTITY);
                pCGFNode->pParent = 0;
            }
            else
            {
                nodes[i]->m_finalNodeToOriginTM = nodes[i]->m_nodeToOriginTM;
                pCGFNode->localTM = p->m_finalNodeToOriginTM.GetInvertedFast() * nodes[i]->m_finalNodeToOriginTM;
                pCGFNode->pParent = cgf.GetNode(p->m_indexInCgf);
            }
            pCGFNode->worldTM = nodes[i]->m_finalNodeToOriginTM;
        }

        pCGFNode->bIdentityMatrix = pCGFNode->worldTM.IsIdentity();

        pCGFNode->nPhysicalizeFlags = 0;

        CMesh* pCMesh = 0;

        if (SceneNodeHasMesh(pScene, nodes[i]->m_nodeIndexInScene))
        {
            const int attrIdx = pScene->GetNode(nodes[i]->m_nodeIndexInScene)->FindFirstMatchingAttribute(Scene::SNode::eAttributeType_Mesh);
            const int meshIndex = pScene->GetNode(nodes[i]->m_nodeIndexInScene)->attributes[attrIdx].index;

            const MeshUtils::Mesh* const pMesh = pScene->GetMesh(meshIndex);
            if (pMesh)
            {
                const Matrix34 originalLocalToFinalOrthonormalLocalTM =
                    nodes[i]->m_finalNodeToOriginTM.GetInvertedFast() *
                    nodes[i]->m_originToWorldTM.GetInvertedFast() *
                    nodes[i]->m_nodeToWorldTM;

                pCMesh = CreateCMesh(*pMesh, exportMatIds, originalLocalToFinalOrthonormalLocalTM);
            }
        }

        if (pCMesh)
        {
            pCGFNode->type = CNodeCGF::NODE_MESH;

            pCGFNode->pMesh = pCMesh;

            assert(exportMaterial.m_pCgfMaterial);
            pCGFNode->pMaterial = exportMaterial.m_pCgfMaterial;
            exportMaterial.m_bOwnMaterial = false;  // CContentCGF deletes materials automatically
        }
        else
        {
            // It's either a helper or we have failed to create CMesh
            pCGFNode->type = CNodeCGF::NODE_HELPER;
        }

        cgf.AddNode(pCGFNode);
    }

    cgf.SetCommonMaterial(exportMaterial.m_pCgfMaterial);

    pAssetWriter->WriteCGF(&cgf);

    ExportNode::DeleteNodesRecursively(pRootExportNode);

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool CFbxConverter::Process()
{
    const string sInputFile = m_CC.GetSourcePath();

    // Manifest creation (if requested)
    {
        const string sManifestFile = m_CC.config->GetAsString("manifest", "", "");
        if (!sManifestFile.empty())
        {
            RCLog("Loading scene file '%s'", sInputFile.c_str());

            CryFbxScene scene;

            scene.LoadScene_StaticMesh(sInputFile);

            return ValidateSceneAndCreateManifestFile(&scene, sInputFile, sManifestFile);
        }
    }

    // Importing scene from a geometry file

    ImportRequest ir;

    if (StringHelpers::EndsWithIgnoreCase(sInputFile.c_str(), ".cryfbx"))
    {
        ir.sourceAssetFilename = sInputFile;
        RCLog("Getting all nodes");
        // Note that we keep ir.nodeNames[] empty. It's fine because
        // CollectSceneNodesWithMatchingNames() treats it as "all nodes".
    }
    else
    {
        if (!LoadImportRequest(m_CC.pRC, sInputFile.c_str(), ir))
        {
            return false;
        }
        RCLog("Import request file contains %d node name(s)", (int)ir.nodeNames.size());
    }

    if (!FileUtil::FileExists(ir.sourceAssetFilename.c_str()))
    {
        RCLogError("Can't find file '%s'", ir.sourceAssetFilename.c_str());
        return false;
    }

    RCLog("Importing scene from file '%s'", ir.sourceAssetFilename.c_str());

    CryFbxScene scene;

    scene.LoadScene_StaticMesh(ir.sourceAssetFilename);

    return ValidateAndExportScene(&scene, ir, sInputFile, ir.sourceAssetFilename, m_CC.pRC);
}


CFbxConverter::CFbxConverter()
    : m_refCount(1)
{
}


CFbxConverter::~CFbxConverter()
{
}


void CFbxConverter::Release()
{
    if (--m_refCount <= 0)
    {
        delete this;
    }
}


void CFbxConverter::BeginProcessing(const IConfig* config)
{
}


void CFbxConverter::Init(const ConvertorInitContext& context)
{
}


ICompiler* CFbxConverter::CreateCompiler()
{
    // Only ever return one compiler, since we don't support multithreading. Since
    // the compiler is just this object, we can tell whether we have already returned
    // a compiler by checking the ref count.
    if (m_refCount >= 2)
    {
        return 0;
    }

    // Until we support multithreading for this convertor, the compiler and the
    // convertor may as well just be the same object.
    ++m_refCount;
    return this;
}


bool CFbxConverter::SupportsMultithreading() const
{
    return false;
}


const char* CFbxConverter::GetExt(int index) const
{
    switch (index)
    {
    case 0:
        return "cryfbx";
    default:
        return 0;
    }
}
