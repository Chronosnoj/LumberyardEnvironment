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

#include <AzCore/Component/Component.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>

namespace Camera
{
    class CameraEditorSystemComponent
        : public AZ::Component
        , private AzToolsFramework::EditorEvents::Bus::Handler
    {
    public:
        AZ_COMPONENT(CameraEditorSystemComponent, "{769802EB-722A-4F89-A475-DA396DA1FDCC}");
        static void Reflect(AZ::ReflectContext* context);

        CameraEditorSystemComponent() = default;
        ~CameraEditorSystemComponent() override = default;

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component
        void Activate() override;
        void Deactivate() override;
        //////////////////////////////////////////////////////////////////////////

    private:
        //////////////////////////////////////////////////////////////////////////
        // AzToolsFramework::EditorEvents
        void PopulateEditorGlobalContextMenu(QMenu *menu, const AZ::Vector2& point, int flags) override;
        //////////////////////////////////////////////////////////////////////////
    };
}
