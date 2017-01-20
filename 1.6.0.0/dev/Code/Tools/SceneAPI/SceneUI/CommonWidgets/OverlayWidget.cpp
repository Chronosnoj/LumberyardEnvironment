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

#include <QLayout.h>
#include <SceneAPI/SceneUI/CommonWidgets/OverlayWidget.h>
#include <SceneAPI/SceneUI/CommonWidgets/OverlayWidgetLayer.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace UI
        {
            const int OverlayWidget::s_invalidOverlayIndex;

            OverlayWidget::OverlayWidget(QWidget* parent)
                : QWidget(parent)
                , m_rootIndex(s_invalidOverlayIndex)
            {
                m_stack = new QStackedLayout();
                m_stack->setStackingMode(QStackedLayout::StackOne);
                setLayout(m_stack);

                // Guarantees there's always a root element.
                m_rootIndex = m_stack->addWidget(new QWidget());
            }

            void OverlayWidget::SetRoot(QWidget* root)
            {
                m_stack->removeItem(m_stack->itemAt(m_rootIndex));
                m_rootIndex = m_stack->insertWidget(m_rootIndex, root);
            }

            int OverlayWidget::PushLayer(QWidget* centerWidget, const OverlayWidgetButtonList& buttons)
            {
                return PushLayer(aznew OverlayWidgetLayer(this, centerWidget, buttons));
            }

            void OverlayWidget::PopLayer()
            {
                if (m_stack->currentIndex() != m_rootIndex)
                {
                    m_stack->removeWidget(m_stack->currentWidget());
                    m_stack->setCurrentIndex(m_stack->count() - 1);
                }
            }

            void OverlayWidget::RemoveLayer(int index)
            {
                AZ_Assert(index != m_rootIndex, "Attempting to remove the root layer from the overlay which is not allowed.");
                AZ_Assert(index >= 0 && index < m_stack->count(), "Index is not to a valid entry.");
                if (index != m_rootIndex && index >= 0 && index < m_stack->count())
                {
                    m_stack->removeItem(m_stack->itemAt(index));
                    m_stack->setCurrentIndex(m_stack->count() - 1);
                }
            }

            int OverlayWidget::PushLayer(OverlayWidgetLayer* layer)
            {
                int index = m_stack->addWidget(layer);
                m_stack->setCurrentIndex(index);
                return index;
            }

            void OverlayWidget::PopLayer(OverlayWidgetLayer* layer)
            {
                m_stack->removeWidget(layer);
                m_stack->setCurrentIndex(m_stack->count() - 1);
            }

            int OverlayWidget::PushLayerToOverlay(OverlayWidget* overlay, QWidget* centerWidget, const OverlayWidgetButtonList& buttons)
            {
                return PushLayer(overlay, aznew OverlayWidgetLayer(overlay, centerWidget, buttons));
            }

            int OverlayWidget::PushLayerToContainingOverlay(QWidget* overlayChild, QWidget* centerWidget, const OverlayWidgetButtonList& buttons)
            {
                OverlayWidget* overlay = GetContainingOverlay(overlayChild);
                return PushLayer(overlay, aznew OverlayWidgetLayer(overlay, centerWidget, buttons));
            }

            OverlayWidget* OverlayWidget::GetContainingOverlay(QWidget* overlayChild)
            {
                while (overlayChild != nullptr)
                {
                    OverlayWidget* overlayWidget = qobject_cast<OverlayWidget*>(overlayChild);
                    if (overlayWidget)
                    {
                        return overlayWidget;
                    }
                    else
                    {
                        overlayChild = overlayChild->parentWidget();
                    }
                }
                return nullptr;
            }

            int OverlayWidget::PushLayer(OverlayWidget* targetOverlay, OverlayWidgetLayer* layer)
            {
                if (targetOverlay)
                {
                    return targetOverlay->PushLayer(layer);
                }
                else
                {
                    layer->setMinimumSize(640, 480);
                    layer->show();
                    return s_invalidOverlayIndex;
                }
            }
        } // UI
    } // SceneAPI
} // AZ

#include <CommonWidgets/OverlayWidget.moc>