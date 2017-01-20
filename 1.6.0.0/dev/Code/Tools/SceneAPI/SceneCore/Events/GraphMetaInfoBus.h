#pragma once

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

#include <AzCore/ebus/ebus.h>
#include <AzCore/std/string/string.h>
#include <SceneAPI/SceneCore/SceneCoreConfiguration.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace DataTypes
        {
            class IGraphObject;
        }
        namespace Events
        {
            class GraphMetaInfo
                : public AZ::EBusTraits
            {
            public:
                static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;

                virtual ~GraphMetaInfo() = 0;

                // Gets the path to the icon associated with the given object.
                SCENE_CORE_API virtual void GetIconPath(AZStd::string& iconPath, const DataTypes::IGraphObject* target);

                // Provides a short description of the type.
                SCENE_CORE_API virtual void GetToolTip(AZStd::string& toolTip, const DataTypes::IGraphObject* target);
            };

            inline GraphMetaInfo::~GraphMetaInfo() = default;

            using GraphMetaInfoBus = AZ::EBus<GraphMetaInfo>;
        } // Events
    } // SceneAPI
} // AZ
