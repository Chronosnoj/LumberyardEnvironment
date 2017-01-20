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

#ifndef CRYINCLUDE_TOOLS_SPHERICALHARMONICS_PRT_DEFAULTMATERIAL_H
#define CRYINCLUDE_TOOLS_SPHERICALHARMONICS_PRT_DEFAULTMATERIAL_H
#pragma once

#if defined(OFFLINE_COMPUTATION)


#include <PRT/ISHMaterial.h>

class CSimpleIndexedMesh;

namespace NSH
{
    namespace NMaterial
    {
        //!< default sh-related material without subsurface transfer, diffuse model based on incident angle (cos)
        class CDefaultSHMaterial
            : public ISHMaterial
        {
        public:
            INSTALL_CLASS_NEW(CDefaultSHMaterial)

            CDefaultSHMaterial(CSimpleIndexedMesh* pMesh);
            //!< sets diffuse material components
            virtual void SetDiffuseIntensity(const float cRedIntensity, const float cGreenIntensity, const float cBlueIntensity, const float cAlphaIntensity = 0.f);
            //!< returns diffuse intensity at a given angle, coloured incident intensity and surface orientation
            virtual const TRGBCoeffD DiffuseIntensity(const TRGBCoeffD& crIncidentIntensity, const uint32 cTriangleIndex, const TVec& rBaryCoord, const TCartesianCoord& rIncidentDir, const bool cApplyCosTerm = true, const bool cApplyExitanceTerm = false, const bool cAbsCosTerm = false, const bool cUseTransparency = false) const;
            virtual const TRGBCoeffD DiffuseIntensity(const TVec&, const TVec& crVertexNormal, const Vec2&, const TRGBCoeffD& crIncidentIntensity, const TCartesianCoord& rIncidentDir, const bool cApplyCosTerm = true, const bool cApplyExitanceTerm = false, const bool cAbsCosTerm = false, const bool cUseTransparency = false) const;
            virtual const EMaterialType Type() const{return MATERIAL_TYPE_DEFAULT; }

        private:
            CSimpleIndexedMesh* m_pMesh;        //!< indexed triangle mesh where surface data get retrieves from, indices are not shared among triangles
            float m_RedIntensity;                       //!< diffuse reflectance material property for red
            float m_GreenIntensity;                 //!< diffuse reflectance material property for green
            float m_BlueIntensity;                  //!< diffuse reflectance material property for blue
        };
    }
}

#endif
#endif // CRYINCLUDE_TOOLS_SPHERICALHARMONICS_PRT_DEFAULTMATERIAL_H
