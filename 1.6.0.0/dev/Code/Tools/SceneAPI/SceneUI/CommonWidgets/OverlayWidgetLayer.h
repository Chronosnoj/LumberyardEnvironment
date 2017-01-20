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
#include <AzCore/Memory/SystemAllocator.h>
#include <SceneAPI/SceneUI/SceneUIConfiguration.h>
#include <SceneAPI/SceneUI/CommonWidgets/OverlayWidget.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace UI
        {
            class OverlayWidget;
            // QT space
            namespace Ui
            {
                class OverlayWidgetLayer;
            }

            class OverlayWidgetLayer : public QWidget
            {
                Q_OBJECT
            public:
                AZ_CLASS_ALLOCATOR(OverlayWidgetLayer, SystemAllocator, 0)

                OverlayWidgetLayer(OverlayWidget* parent, QWidget* centerWidget, const OverlayWidgetButtonList& buttons);

            protected:
                virtual void PopLayer();
                bool eventFilter(QObject* object, QEvent* event) override;

                QScopedPointer<Ui::OverlayWidgetLayer> ui;
                OverlayWidget* m_parent;
            };
        } // UI
    } // SceneAPI
} // AZ