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

#include <QWidget.h>
#include <QStackedLayout.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/functional.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <SceneAPI/SceneUI/SceneUIConfiguration.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace UI
        {
            class OverlayWidgetLayer;

            struct OverlayWidgetButton
            {
                using Callback = AZStd::function<void()>;

                AZStd::string m_text;
                Callback m_callback;
                bool m_triggersPop = { false };
            };

            using OverlayWidgetButtonList = AZStd::vector<const OverlayWidgetButton*>;
            
            class OverlayWidget : public QWidget
            {
                friend class OverlayWidgetLayer;

                Q_OBJECT
            public:
                AZ_CLASS_ALLOCATOR(OverlayWidget, SystemAllocator, 0)

                static const int s_invalidOverlayIndex = -1;

                SCENE_UI_API explicit OverlayWidget(QWidget* parent);
                
                // Sets the widget that will be shown when there are no layers active.
                SCENE_UI_API void SetRoot(QWidget* root);

                SCENE_UI_API int PushLayer(QWidget* centerWidget, const OverlayWidgetButtonList& buttons);
                SCENE_UI_API void PopLayer();
                SCENE_UI_API void RemoveLayer(int index);

                SCENE_UI_API static int PushLayerToOverlay(OverlayWidget* overlay, QWidget* centerWidget, const OverlayWidgetButtonList& buttons);
                SCENE_UI_API static int PushLayerToContainingOverlay(QWidget* overlayChild, QWidget* centerWidget, 
                    const OverlayWidgetButtonList& buttons);

            protected:
                int PushLayer(OverlayWidgetLayer* layer);
                void PopLayer(OverlayWidgetLayer* layer);
                
                static OverlayWidget* GetContainingOverlay(QWidget* overlayChild);
                static int PushLayer(OverlayWidget* targetOverlay, OverlayWidgetLayer* layer);

                QStackedLayout* m_stack;
                int m_rootIndex;
            };
        } // UI
    } // SceneAPI
} // AZ