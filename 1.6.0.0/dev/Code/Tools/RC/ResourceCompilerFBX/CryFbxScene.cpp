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

#pragma warning(disable: 4266) // no override available for virtual member function
#pragma warning(disable: 4264) // no override available for virtual member function

#include "Cry_Geo.h"
#include "Cry_Math.h"
#include "VertexFormats.h"
#include "IIndexedMesh.h"

#include "FileUtil.h"
#include "IRCLog.h"
#include "Export/MeshUtils.h"
#include "StringHelpers.h"
#include "Util.h"

#include "CryFbxScene.h"
#include "FBXConverter.h"


#define FBXSDK_NEW_API
#include "fbxsdk.h"


static Matrix34 GetMatrix34FromFbxAMatrix(const FbxAMatrix& m)
{
    // Note that FBX stores 4x4 matrices in non-mathematical form
    // (so translation is stored in fourth row instead of fourth column).
    const FbxVector4 x = m.GetRow(0);
    const FbxVector4 y = m.GetRow(1);
    const FbxVector4 z = m.GetRow(2);
    const FbxVector4 p = m.GetRow(3);

    Matrix34 result;

    result.m00 = (float)x[0];
    result.m10 = (float)x[1];
    result.m20 = (float)x[2];

    result.m01 = (float)y[0];
    result.m11 = (float)y[1];
    result.m21 = (float)y[2];

    result.m02 = (float)z[0];
    result.m12 = (float)z[1];
    result.m22 = (float)z[2];

    result.m03 = (float)p[0];
    result.m13 = (float)p[1];
    result.m23 = (float)p[2];

    return result;
}


template <typename ValueType, typename ElementArrayType>
static void GetGeometryElement(ValueType& value, const ElementArrayType* pElementArray, int polygonIndex, int polygonVertexIndex, int controlPointIndex)
{
    if (!pElementArray)
    {
        return;
    }

    int index;
    switch (pElementArray->GetMappingMode())
    {
    case FbxGeometryElement::eByControlPoint:
        // One mapping coordinate for each surface control point/vertex.
        index = controlPointIndex;
        break;
    case FbxGeometryElement::eByPolygonVertex:
        // One mapping coordinate for each vertex, for every polygon of which it is a part.
        // This means that a vertex will have as many mapping coordinates as polygons of which it is a part.
        index = polygonVertexIndex;
        break;
    case FbxGeometryElement::eByPolygon:
        // One mapping coordinate for the whole polygon.
        index = polygonIndex;
        break;
    default:
        return;
    }

    if (pElementArray->GetReferenceMode() == FbxGeometryElement::eIndexToDirect)
    {
        // Convert index from "index of value's index" to "index of value".
        const FbxLayerElementArrayTemplate<int>& indices = pElementArray->GetIndexArray();
        index = indices.GetAt(index);
    }

    const FbxLayerElementArrayTemplate<ValueType>& elements = pElementArray->GetDirectArray();
    value = elements.GetAt(index);
}


static MeshUtils::Mesh* CreateMesh(
    const FbxMesh* pFbxMesh,
    const FbxNode* pFbxNode,
    const std::map<const FbxSurfaceMaterial*, int>& mapFbxMatToGlobalIndex,
    double scale,
    const FbxAMatrix* pTransform)
{
    if (!pFbxMesh || !pFbxNode)
    {
        assert(0);
        return 0;
    }

    // Prepare materials

    // Node-local material index -> global index
    std::vector<int> mapMaterial;

    // global material index to use in case of ab invalid node-local material
    const int badMaterial = mapFbxMatToGlobalIndex.find(0)->second;

    {
        const int materialCount = pFbxNode->GetMaterialCount();
        mapMaterial.resize(materialCount, -1);

        for (int j = 0; j < materialCount; ++j)
        {
            const FbxSurfaceMaterial* const pFbxMaterial = pFbxNode->GetMaterial(j);

            std::map<const FbxSurfaceMaterial*, int>::const_iterator it = mapFbxMatToGlobalIndex.find(pFbxMaterial);

            if (it == mapFbxMatToGlobalIndex.end())
            {
                mapMaterial[j] = badMaterial;
            }
            else
            {
                mapMaterial[j] = it->second;
            }
        }
    }

    // Fill geometry

    std::unique_ptr<MeshUtils::Mesh> pMesh(new MeshUtils::Mesh());
    {
        MeshUtils::Mesh& mesh = *pMesh.get();

        const FbxVector4* const pControlPoints = pFbxMesh->GetControlPoints();
        const int* const pControlPointIndices = pFbxMesh->GetPolygonVertices();

        const FbxGeometryElementUV* const pElementUV = pFbxMesh->GetElementUV();
        const FbxGeometryElementVertexColor* const pElementVertexColor = pFbxMesh->GetElementVertexColor();

        FbxLayerElementArrayTemplate<int>* pMaterialIndices = 0;
        pFbxMesh->GetMaterialIndices(&pMaterialIndices); // per polygon

        const int numPolygons = pFbxMesh->GetPolygonCount();
        for (int polygonIndex = 0; polygonIndex < numPolygons; ++polygonIndex)
        {
            const int numPolygonVertices = pFbxMesh->GetPolygonSize(polygonIndex);
            if (numPolygonVertices <= 2)
            {
                continue;
            }

            int materialIndex = pMaterialIndices ? (*pMaterialIndices)[polygonIndex] : -1;
            if (materialIndex < 0 || materialIndex >= (int)mapMaterial.size())
            {
                materialIndex = badMaterial;
            }
            else
            {
                materialIndex = mapMaterial[materialIndex];
            }

            const int startIdx = pFbxMesh->GetPolygonVertexIndex(polygonIndex);

            // Triangulate the polygon as a fan and remember resulting vertices and faces

            int firstIndex = -1;
            int prevIndex = -1;

            MeshUtils::Face face;
            int verticesInFace = 0;

            for (int i = 0; i < numPolygonVertices; ++i)
            {
                const int newIndex = (int)mesh.m_positions.size();

                const int polygonVertexIndex = startIdx + i;
                const int controlPointIndex = pControlPointIndices[polygonVertexIndex];

                FbxVector4 pos = pControlPoints[controlPointIndex];

                FbxVector4 nor;
                pFbxMesh->GetPolygonVertexNormal(polygonIndex, i, nor);

                FbxVector2 texCoord(0, 0);
                if (pElementUV)
                {
                    GetGeometryElement(texCoord, pElementUV, polygonIndex, polygonVertexIndex, controlPointIndex);
                }

                FbxColor color(1, 1, 1, 1);
                if (pElementVertexColor)
                {
                    GetGeometryElement(color, pElementVertexColor, polygonIndex, polygonVertexIndex, controlPointIndex);
                }

                // Position
                pos[3] = 1;
                if (pTransform)
                {
                    pos = pTransform->MultT(pos);
                }
                pos[0] *= scale;
                pos[1] *= scale;
                pos[2] *= scale;
                mesh.m_positions.push_back(Vec3(float(pos[0]), float(pos[1]), float(pos[2])));

                // Normal
                nor[3] = 0;
                if (pTransform)
                {
                    nor = pTransform->MultT(nor);
                }
                nor[3] = 0;
                nor.Normalize();
                mesh.m_normals.push_back(Vec3(float(nor[0]), float(nor[1]), float(nor[2])));

                // Texture coordinates
                mesh.m_texCoords.push_back(
                    Vec2(float(texCoord[0]), 1.0f - float(texCoord[1])));

                // Color & alpha
                if (pElementVertexColor)
                {
                    MeshUtils::Color c;
                    c.r = (uint8)Util::getClamped(256 * color.mRed,   0.0, 255.0);
                    c.g = (uint8)Util::getClamped(256 * color.mGreen, 0.0, 255.0);
                    c.b = (uint8)Util::getClamped(256 * color.mBlue,  0.0, 255.0);
                    mesh.m_colors.push_back(c);

                    const uint8 a = (uint8)Util::getClamped(256 * color.mAlpha, 0.0, 255.0);
                    mesh.m_alphas.push_back(a);
                }

                // Topology ID
                mesh.m_topologyIds.push_back(controlPointIndex);

                // Add face
                {
                    if (i == 0)
                    {
                        firstIndex = newIndex;
                    }

                    int v[3];
                    int n = 0;

                    v[n++] = newIndex;

                    if (i >= 3)
                    {
                        v[n++] = firstIndex;
                        v[n++] = prevIndex;
                    }

                    for (int j = 0; j < n; ++j)
                    {
                        face.vertexIndex[verticesInFace++] = v[j];
                        if (verticesInFace == 3)
                        {
                            verticesInFace = 0;
                            mesh.m_faces.push_back(face);
                            mesh.m_faceMatIds.push_back(materialIndex);
                        }
                    }

                    prevIndex = newIndex;
                }
            }

            if (verticesInFace != 0)
            {
                assert(0);
                RCLogError("Internal error in mesh filler");
                return 0;
            }
        }

        if (mesh.GetVertexCount() <= 0 || mesh.GetFaceCount() <= 0)
        {
            RCLogError("Missing geometry data in mesh node");
            return 0;
        }
    }

    return pMesh.release();
}

//////////////////////////////////////////////////////////////////////////

namespace
{
    class CryFbxMesh
    {
    public:
        FbxMesh* m_pFbxMesh;
        FbxNode* m_pFbxNode; // needed to be stored here to access per-node materials
        bool m_bMeshReady;
        MeshUtils::Mesh* m_pMesh;

    public:
        CryFbxMesh()
            : m_pFbxMesh(0)
            , m_pFbxNode(0)
            , m_bMeshReady(false)
            , m_pMesh(0)
        {
        }

        ~CryFbxMesh()
        {
            delete m_pMesh;
        }

        const MeshUtils::Mesh* GetMesh(const std::map<const FbxSurfaceMaterial*, int>& mapFbxMatToGlobalIndex)
        {
            if (m_bMeshReady)
            {
                return m_pMesh;
            }

            assert(!m_pMesh);
            assert(m_pFbxNode);

            m_bMeshReady = true;

            if (m_pFbxMesh)
            {
                const double scale = 1.0;
                m_pMesh = CreateMesh(m_pFbxMesh, m_pFbxNode, mapFbxMatToGlobalIndex, scale, 0);
                if (m_pMesh && (m_pMesh->GetVertexCount() <= 0 || m_pMesh->GetFaceCount() <= 0))
                {
                    delete m_pMesh;
                    m_pMesh = 0;
                }
            }

            return m_pMesh;
        }
    };


    class CryFbxNode
    {
    public:
        Scene::SNode m_node;
        FbxNode* m_pFbxNode;

    public:
        CryFbxNode()
            : m_pFbxNode(0)
        {
        }

        ~CryFbxNode()
        {
        }
    };


    class CryFbxMaterial
    {
    public:
        Scene::SMaterial m_material;

    public:
        CryFbxMaterial()
        {
        }

        ~CryFbxMaterial()
        {
        }
    };
} // namespace

//////////////////////////////////////////////////////////////////////////

namespace
{
    class CryFbxManager
    {
    public:
        FbxManager* m_pFbxManager;
        FbxScene* m_pFbxScene;

    public:
        CryFbxManager()
            : m_pFbxManager(0)
            , m_pFbxScene(0)
        {
        }

        ~CryFbxManager()
        {
            Reset();
        }

        void Reset()
        {
            if (m_pFbxManager)
            {
                m_pFbxManager->Destroy();
                m_pFbxManager = 0;
            }
            m_pFbxScene = 0;
        }

        bool LoadScene(const char* fbxFilename)
        {
            Reset();

            m_pFbxManager = FbxManager::Create();

            if (!m_pFbxManager)
            {
                RCLogError("Failed to create FBX manager");
                return false;
            }

            FbxIOSettings* const pSettings = FbxIOSettings::Create(m_pFbxManager, IOSROOT);

            if (!pSettings)
            {
                RCLogError("Failed to create FBX I/O object");
                Reset();
                return false;
            }

            m_pFbxManager->SetIOSettings(pSettings);

            FbxImporter* const pImporter = FbxImporter::Create(m_pFbxManager, "");

            if (!pImporter)
            {
                RCLogError("Failed to create FBX importer");
                Reset();
                return false;
            }

            if (!pImporter->Initialize(fbxFilename, -1, m_pFbxManager->GetIOSettings()))
            {
                RCLogError("Call to FbxImporter::Initialize('%s') failed: '%s'", fbxFilename, pImporter->GetStatus().GetErrorString());
                Reset();
                return false;
            }

            // Create a new FBX scene so it can be populated by the imported file.
            m_pFbxScene = FbxScene::Create(m_pFbxManager, "myScene");

            if (!m_pFbxScene)
            {
                RCLogError("Failed to create FBX scene");
                Reset();
                return false;
            }

            if (!pImporter->Import(m_pFbxScene))
            {
                RCLogError("Failed to import FBX data from '%s'", fbxFilename);
                Reset();
                return false;
            }

            pImporter->Destroy();

            return true;
        }
    };
} // namespace

//////////////////////////////////////////////////////////////////////////

struct CryFbxSceneImpl
{
    CryFbxManager m_fbxMan;

    char m_forwardUpAxes[5];  // "<signOfForwardAxis><forwardAxis><signOfUpAxis><upAxis>"

    float m_unitSizeInCentimeters;

    std::vector<CryFbxNode> m_nodes;
    std::vector<CryFbxMesh> m_meshes;
    std::vector<CryFbxMaterial> m_materials;
    std::map<const FbxSurfaceMaterial*, int> m_mapFbxMatToGlobalIndex;

    CryFbxSceneImpl()
    {
        Reset();
    }

    ~CryFbxSceneImpl()
    {
        Reset();
    }

    void Reset()
    {
        m_nodes.clear();
        m_meshes.clear();
        m_materials.clear();
        m_mapFbxMatToGlobalIndex.clear();
        m_fbxMan.Reset();
        m_forwardUpAxes[0] = 0;
        m_unitSizeInCentimeters = 1.0f;
    }

    void PrepareMaterials()
    {
        m_materials.clear();
        m_mapFbxMatToGlobalIndex.clear();

        // Create a special material - all invalid materials will be mapped to it
        {
            CryFbxMaterial m;
            m.m_material.name = "no material";

            m_materials.push_back(m);
            m_mapFbxMatToGlobalIndex[0] = 0;
        }

        // Collect materials from all modes, except the root node (it's an auxiliary node)
        for (size_t i = 1; i < m_nodes.size(); ++i)
        {
            CryFbxNode& node = m_nodes[i];

            if (!node.m_pFbxNode)
            {
                continue;
            }

            const int materialCount = node.m_pFbxNode->GetMaterialCount();

            for (int j = 0; j < materialCount; ++j)
            {
                const FbxSurfaceMaterial* const pFbxMaterial = node.m_pFbxNode->GetMaterial(j);

                std::map<const FbxSurfaceMaterial*, int>::iterator it = m_mapFbxMatToGlobalIndex.find(pFbxMaterial);

                if (it == m_mapFbxMatToGlobalIndex.end())
                {
                    m_mapFbxMatToGlobalIndex[pFbxMaterial] = (int)m_materials.size();

                    CryFbxMaterial m;
                    if (pFbxMaterial)
                    {
                        m.m_material.name = pFbxMaterial->GetName();
                    }
                    else
                    {
                        m.m_material.name = "no name";
                    }
                    m_materials.push_back(m);
                }
            }
        }
    }
};

//////////////////////////////////////////////////////////////////////////

CryFbxScene::CryFbxScene()
    : m_pImpl(new CryFbxSceneImpl())
{
    assert(m_pImpl);
}

CryFbxScene::~CryFbxScene()
{
    Reset();
    delete m_pImpl;
}

void CryFbxScene::Reset()
{
    m_pImpl->Reset();
}

//////////////////////////////////////////////////////////////////////////


static bool HasFbxNodeAttributeType(const FbxNode* pFbxNode, FbxNodeAttribute::EType eType)
{
    if (!pFbxNode)
    {
        return false;
    }

    for (int i = 0; i < pFbxNode->GetNodeAttributeCount(); ++i)
    {
        const FbxNodeAttribute* const pAttribute = pFbxNode->GetNodeAttributeByIndex(i);

        if (pAttribute->GetAttributeType() == eType)
        {
            return true;
        }
    }

    return false;
}

static FbxAMatrix GetFbxNodeWorldTransform(FbxNode* pFbxNode)
{
    if (!pFbxNode)
    {
        FbxAMatrix m;
        m.SetIdentity();
        return m;
    }

    const FbxAMatrix globalTM = pFbxNode->EvaluateGlobalTransform();
    const FbxVector4 t = pFbxNode->GetGeometricTranslation(FbxNode::eSourcePivot);
    const FbxVector4 s = pFbxNode->GetGeometricScaling(FbxNode::eSourcePivot);
    const FbxVector4 r = pFbxNode->GetGeometricRotation(FbxNode::eSourcePivot);
    const FbxAMatrix offsetTM(t, r, s);

    return globalTM * offsetTM;
}


const char* CryFbxScene::GetForwardUpAxes() const
{
    return &m_pImpl->m_forwardUpAxes[0];
}


float CryFbxScene::GetUnitSizeInCentimeters() const
{
    return m_pImpl->m_unitSizeInCentimeters;
}


int CryFbxScene::GetNodeCount() const
{
    return (int)m_pImpl->m_nodes.size();
}

const Scene::SNode* CryFbxScene::GetNode(int idx) const
{
    if (idx < 0 || idx >= GetNodeCount())
    {
        return 0;
    }
    return &m_pImpl->m_nodes[idx].m_node;
}


int CryFbxScene::GetMeshCount() const
{
    return (int)m_pImpl->m_meshes.size();
}

const MeshUtils::Mesh* CryFbxScene::GetMesh(int idx) const
{
    if (idx < 0 || idx >= GetMeshCount())
    {
        return 0;
    }
    return m_pImpl->m_meshes[idx].GetMesh(m_pImpl->m_mapFbxMatToGlobalIndex);
}


int CryFbxScene::GetMaterialCount() const
{
    return (int)m_pImpl->m_materials.size();
}

const Scene::SMaterial* CryFbxScene::GetMaterial(int idx) const
{
    if (idx < 0 || idx >= GetMaterialCount())
    {
        return 0;
    }
    return &m_pImpl->m_materials[idx].m_material;
}


void CryFbxScene::LoadScene_StaticMesh(const char* fbxFilename)
{
    Reset();

    if (!FileUtil::FileExists(fbxFilename))
    {
        RCLogError("Can't find file '%s'", fbxFilename);
        return;
    }

    if (!m_pImpl->m_fbxMan.LoadScene(fbxFilename))
    {
        Reset();
        return;
    }

    // Coordinate axes Forward and Up
    {
        FbxAxisSystem axisSystem = m_pImpl->m_fbxMan.m_pFbxScene->GetGlobalSettings().GetAxisSystem();
        if (axisSystem.GetCoorSystem() != FbxAxisSystem::eRightHanded)
        {
            RCLogError("Scene coordinate system in %s is left-handed. We support only right-handed scenes.", fbxFilename);
            Reset();
            return;
        }

        int sign = 0;

        const char* possibleForwardAxes;

        switch (axisSystem.GetUpVector(sign))
        {
        case FbxAxisSystem::eXAxis:
            m_pImpl->m_forwardUpAxes[3] = 'X';
            possibleForwardAxes = "YZ";
            break;
        case FbxAxisSystem::eYAxis:
            m_pImpl->m_forwardUpAxes[3] = 'Y';
            possibleForwardAxes = "XZ";
            break;
        case FbxAxisSystem::eZAxis:
            m_pImpl->m_forwardUpAxes[3] = 'Z';
            possibleForwardAxes = "XY";
            break;
        default:
            RCLogError("Scene coordinate system info in %s is damaged: unknown value of axis 'up'.", fbxFilename);
            Reset();
            return;
        }
        m_pImpl->m_forwardUpAxes[2] = sign < 0 ? '-' : '+';

        switch (axisSystem.GetFrontVector(sign))
        {
        case FbxAxisSystem::eParityEven:
            m_pImpl->m_forwardUpAxes[1] = possibleForwardAxes[0];
            break;
        case FbxAxisSystem::eParityOdd:
            m_pImpl->m_forwardUpAxes[1] = possibleForwardAxes[1];
            break;
        default:
            RCLogError("Scene coordinate system info in %s is damaged: unknown value of axis 'forward'.", fbxFilename);
            Reset();
            return;
        }
        m_pImpl->m_forwardUpAxes[0] = sign < 0 ? '-' : '+';

        m_pImpl->m_forwardUpAxes[4] = 0;
    }

    // Unit size
    {
        double unitSizeInCm = m_pImpl->m_fbxMan.m_pFbxScene->GetGlobalSettings().GetSystemUnit().GetScaleFactor();
        if (unitSizeInCm <= 0)
        {
            unitSizeInCm = 1.0;
        }

        struct Unit
        {
            const char* name;
            double sizeInCm;
        } units[] =
        {
            { "millimeters", 0.1 },
            { "centimeters", 1.0 },
            { "inches", 2.54 },
            { "decimeters", 10.0 },
            { "feet", 30.48 },
            { "yards", 91.44 },
            { "meters", 100.0 },
            { "kilometers", 100000.0 }
        };

        const char* unitName = 0;
        for (int i = 0; i < sizeof(units) / sizeof(units[0]); ++i)
        {
            if (unitSizeInCm * 0.9999 <= units[i].sizeInCm && units[i].sizeInCm <= unitSizeInCm * 1.0001)
            {
                unitName = units[i].name;
                break;
            }
        }

        if (!unitName)
        {
            unitName = "custom";
        }

        m_pImpl->m_unitSizeInCentimeters = (float)unitSizeInCm;

        RCLog("FBX unit: %s, %fcm", unitName, unitSizeInCm);
    }

    FbxNode* const pRootFbxNode = m_pImpl->m_fbxMan.m_pFbxScene->GetRootNode();
    if (!pRootFbxNode)
    {
        RCLogError("The scene in FBX file is empty");
        Reset();
        return;
    }

    // Collect nodes worth exporting by traversing node subtrees

    assert(m_pImpl->m_nodes.empty());

    CryFbxNode node;

    node.m_pFbxNode = pRootFbxNode;
    node.m_node.name = "<root>";
    node.m_node.parent = -1;
    node.m_node.worldTransform = Matrix34(IDENTITY);  // TODO: maybe it's better to set zeros only, to prevent callers from using it

    m_pImpl->m_nodes.push_back(node);

    for (int parent = 0; parent < (int)m_pImpl->m_nodes.size(); ++parent)
    {
        for (int i = 0; i < m_pImpl->m_nodes[parent].m_pFbxNode->GetChildCount(); ++i)
        {
            FbxNode* const pFbxNode = m_pImpl->m_nodes[parent].m_pFbxNode->GetChild(i);
            assert(pFbxNode);

            const bool bMesh = HasFbxNodeAttributeType(pFbxNode, FbxNodeAttribute::eMesh);
            const bool bNull = HasFbxNodeAttributeType(pFbxNode, FbxNodeAttribute::eNull);

            // We skip nodes (with subtrees!) that are neither meshes, nor helpers.
            if (!bMesh && !bNull)
            {
                continue;
            }

            m_pImpl->m_nodes[parent].m_node.children.push_back((int)m_pImpl->m_nodes.size());

            node.m_pFbxNode = pFbxNode;
            node.m_node.name = pFbxNode->GetName();
            node.m_node.parent = parent;
            node.m_node.worldTransform = GetMatrix34FromFbxAMatrix(GetFbxNodeWorldTransform(pFbxNode));
            node.m_node.attributes.clear();

            for (int j = 0; j < pFbxNode->GetNodeAttributeCount(); ++j)
            {
                FbxNodeAttribute* const pAttribute = pFbxNode->GetNodeAttributeByIndex(j);

                switch (pAttribute->GetAttributeType())
                {
                case FbxNodeAttribute::eMesh:
                {
                    const Scene::SNode::Attribute attr(
                        Scene::SNode::eAttributeType_Mesh,
                        (int)m_pImpl->m_meshes.size());
                    node.m_node.attributes.push_back(attr);

                    CryFbxMesh mesh;
                    mesh.m_pFbxMesh = static_cast<FbxMesh*>(pAttribute);
                    mesh.m_pFbxNode = pFbxNode;
                    m_pImpl->m_meshes.push_back(mesh);
                }
                break;
                default:
                    break;
                }
            }

            m_pImpl->m_nodes.push_back(node);
        }
    }

    m_pImpl->PrepareMaterials();
}

// eof
