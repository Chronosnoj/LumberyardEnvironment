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
#pragma once

#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

#include <AzFramework/Entity/EntityDebugDisplayBus.h>

#include "TriggerAreaComponent.h"

namespace LmbrCentral
{
    class EditorTriggerAreaComponent
        : public AzToolsFramework::Components::EditorComponentBase
    {
    public:
        AZ_COMPONENT(EditorTriggerAreaComponent, "{7F983509-09FF-4990-93D0-9D52BA1C60B2}", AzToolsFramework::Components::EditorComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        ~EditorTriggerAreaComponent() override = default;

        void Activate() override;
        void Deactivate() override;

        //////////////////////////////////////////////////////////////////////////
        // EditorComponentBase
        void BuildGameEntity(AZ::Entity* gameEntity) override;
        void FinishedBuildingGameEntity(AZ::Entity* gameEntity) override;
        //////////////////////////////////////////////////////////////////////////

    private:

        TriggerAreaComponent m_gameComponent;

        //! The tags that are required to trigger this Area
        AZStd::vector<AZStd::string> m_requiredTags;

        //! The tags that exclude an entity from triggering this Area
        AZStd::vector<AZStd::string> m_excludedTags;
    };
} // namespace LmbrCentral
