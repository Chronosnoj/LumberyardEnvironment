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

#include <QPushButton.h>
#include <CommonWidgets/ui_OverlayWidgetLayer.h>
#include <SceneAPI/SceneUI/CommonWidgets/OverlayWidgetLayer.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace UI
        {
            OverlayWidgetLayer::OverlayWidgetLayer(OverlayWidget* parent, QWidget* centerWidget, const OverlayWidgetButtonList& buttons)
                : QWidget(parent)
                , m_parent(parent)
                , ui(new Ui::OverlayWidgetLayer())
            {
                ui->setupUi(this);

                ui->m_centerLayout->addWidget(centerWidget);
                bool hasPopper = false;

                if (buttons.empty())
                {
                    ui->m_controls->setVisible(false);
                }
                else
                {
                    for (const OverlayWidgetButton* info : buttons)
                    {
                        QPushButton* button = new QPushButton(info->m_text.c_str());

                        if (info->m_triggersPop)
                        {
                            if (info->m_callback)
                            {
                                const OverlayWidgetButton::Callback& callback = info->m_callback;
                                connect(button, &QPushButton::clicked,
                                    [this, callback]()
                                {
                                    callback();
                                    PopLayer();
                                });
                            }
                            else
                            {
                                connect(button, &QPushButton::clicked, this, &OverlayWidgetLayer::PopLayer);
                            }
                            hasPopper = true;
                        }
                        else
                        {
                            if (info->m_callback)
                            {
                                connect(button, &QPushButton::clicked, info->m_callback);
                            }
                        }

                        ui->m_controlsLayout->addWidget(button);
                    }
                }

                if (!parent)
                {
                    if (hasPopper)
                    {
                        setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
                    }
                    else
                    {
                        setWindowFlags(Qt::WindowStaysOnTopHint);
                    }
                }
                centerWidget->installEventFilter(this);
            }

            void OverlayWidgetLayer::PopLayer()
            {
                if (m_parent)
                {
                    m_parent->PopLayer(this);
                }
                else
                {
                    close();
                }
            }

            bool OverlayWidgetLayer::eventFilter(QObject* object, QEvent* event)
            {
                if (event->type() == QEvent::Close)
                {
                    PopLayer();
                }
                return QWidget::eventFilter(object, event);
            }
        } // UI
    } // SceneAPI
} // AZ

#include <CommonWidgets/OverlayWidgetLayer.moc>