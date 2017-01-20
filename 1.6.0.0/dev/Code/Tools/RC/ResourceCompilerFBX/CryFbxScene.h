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

#ifndef CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERFBX_CRYFBXSCENE_H
#define CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERFBX_CRYFBXSCENE_H
#pragma once


#include "Scene.h"

struct CryFbxSceneImpl;

class CryFbxScene
    : public Scene::IScene
{
public:
    CryFbxScene();

    virtual ~CryFbxScene();

    // interface Scene::IScene -----------------------------------------------
    virtual const char* GetForwardUpAxes() const override;

    virtual float GetUnitSizeInCentimeters() const override;

    virtual int GetNodeCount() const override;
    virtual const Scene::SNode* GetNode(int idx) const override;

    virtual int GetMeshCount() const override;
    virtual const MeshUtils::Mesh* GetMesh(int idx) const override;

    virtual int GetMaterialCount() const override;
    virtual const Scene::SMaterial* GetMaterial(int idx) const override;
    // -----------------------------------------------------------------------

    void Reset();

    void LoadScene_StaticMesh(const char* fbxFilename);

private:
    CryFbxSceneImpl* m_pImpl;
};

#endif // CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERFBX_CRYFBXSCENE_H
