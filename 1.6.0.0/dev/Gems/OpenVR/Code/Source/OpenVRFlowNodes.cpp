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

#include "StdAfx.h"
#include <FlowSystem/Nodes/FlowBaseNode.h>
#include "HMDBus.h"
#include <IViewSystem.h>
#include <OpenVR/OpenVRBus.h>

namespace OpenVR
{

class OpenVRPlayspace : public CFlowBaseNode<eNCT_Singleton>
{
        enum INPUTS
        {
            Activate = 0,
        };

        enum OUTPUTS
        {
            Corner0,
            Corner1,
            Corner2,
            Corner3,
            Center,
            Dimensions,
            Valid
        };

    public:

        OpenVRPlayspace(SActivationInfo* actInfo)
        {
        }

        void GetMemoryUsage(ICrySizer* s) const override
        {
            s->Add(*this);
        }

        void GetConfiguration(SFlowNodeConfig& config) override
        {
            static const SInputPortConfig inConfig[] =
            {
                InputPortConfig<bool>("Activate", true, _HELP("Get information about the HMD's playspace")),
                { 0 }
            };
            static const SOutputPortConfig outConfig[] =
            {
                OutputPortConfig<AZ::Vector3>("Corner0", _HELP("The world-space position of Corner 0")),
                OutputPortConfig<AZ::Vector3>("Corner1", _HELP("The world-space position of Corner 1")),
                OutputPortConfig<AZ::Vector3>("Corner2", _HELP("The world-space position of Corner 2")),
                OutputPortConfig<AZ::Vector3>("Corner3", _HELP("The world-space position of Corner 3")),
                OutputPortConfig<AZ::Vector3>("Center", _HELP("The world-space center of the playspace. Note that the center is on the floor.")),
                OutputPortConfig<AZ::Vector3>("Dimensions", _HELP("The width (x) and height (y) of the playspace in meters.")),
                OutputPortConfig<bool>("IsValid", _HELP("If true, the playspace data is valid and configured correctly.")),
                { 0 }
            };

            config.sDescription = _HELP("This node provides information about HMD's playspace");
            config.pInputPorts = inConfig;
            config.pOutputPorts = outConfig;
            config.SetCategory(EFLN_APPROVED);
        }

        void ProcessEvent(EFlowEvent event, SActivationInfo* actInfo) override
        {
            switch (event)
            {
                case eFE_Activate:
                {
                    OpenVRRequests::Playspace playspace;
                    EBUS_EVENT(OpenVRRequestBus, GetPlayspace, playspace);
                    ActivateOutput(actInfo, Valid, playspace.isValid);

                    if (playspace.isValid)
                    {
                        IViewSystem* viewSystem = gEnv->pGame->GetIGameFramework()->GetIViewSystem();
                        IView* mainView = viewSystem->GetActiveView();
                        const SViewParams* viewParams = mainView->GetCurrentParams();
                        
                        AZ::Quaternion rotation = LYQuaternionToAZQuaternion(viewParams->rotation);
                        AZ::Vector3 translation = LYVec3ToAZVec3(viewParams->targetPos);
                        AZ::Transform transform = AZ::Transform::CreateFromQuaternionAndTranslation(rotation, translation);

                        const AZ::Vector3 corners[] =
                        {
                            transform * playspace.corners[0],
                            transform * playspace.corners[1],
                            transform * playspace.corners[2],
                            transform * playspace.corners[3]
                        };

                        ActivateOutput(actInfo, Corner0, corners[0]);
                        ActivateOutput(actInfo, Corner1, corners[1]);
                        ActivateOutput(actInfo, Corner2, corners[2]);
                        ActivateOutput(actInfo, Corner3, corners[3]);

                        // Camera is centered at the playspace's 0 position.
                        Vec3 center = viewParams->targetPos;
                        ActivateOutput(actInfo, Center, center);

                        Vec3 dimensions;
                        dimensions.x = fabsf(corners[2].GetX() - corners[0].GetX());
                        dimensions.y = fabsf(corners[2].GetY() - corners[0].GetY());
                        dimensions.z = 0.0f;
                        ActivateOutput(actInfo, Dimensions, dimensions);
                    }
                }
                break;
            }
        }
};

REGISTER_FLOW_NODE("VR:OpenVR:Playspace", OpenVRPlayspace);
} // namespace OpenVR
