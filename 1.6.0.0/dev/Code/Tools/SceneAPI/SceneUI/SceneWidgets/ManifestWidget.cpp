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

#include <QLabel.h>
#include <QPushButton.h>
#include <SceneWidgets/ui_ManifestWidget.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/SceneManifest.h>
#include <SceneAPI/SceneCore/Events/ManifestMetaInfoBus.h>
#include <SceneAPI/SceneUI/SceneWidgets/ManifestWidget.h>
#include <SceneAPI/SceneUI/SceneWidgets/ManifestWidgetPage.h>
#include <SceneAPI/SceneUI/CommonWidgets/OverlayWidget.h>
#include <SceneAPI/SceneUI/SceneWidgets/SceneGraphWidget.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace UI
        {
            ManifestWidget::ManifestWidget(SerializeContext* serializeContext, QWidget* parent)
                : QWidget(parent)
                , ui(new Ui::ManifestWidget())
                , m_serializeContext(serializeContext)
            {
                ui->setupUi(this);
                BuildPages();

#if defined(_DEBUG)
                QPushButton* graphView = new QPushButton();
                graphView->setFixedSize(20, 20);
                graphView->setFlat(true);
                graphView->setIcon(QIcon(":/SceneUI/Manifest/TreeIcon.png"));
                graphView->setToolTip("Displays the full unfiltered graph.");
                connect(graphView, &QPushButton::clicked, this, &ManifestWidget::ShowGraph);
                ui->m_tabs->setCornerWidget(graphView);
#endif
            }

            void ManifestWidget::BuildFromScene(const AZStd::shared_ptr<Containers::Scene>& scene)
            {
                // First clear
                for (ManifestWidgetPage* page : m_pages)
                {
                    page->Clear();
                }

                // Then add all the values
                m_scene = scene;
                Containers::SceneManifest& manifest = scene->GetManifest();
                for (auto& value : manifest.GetValueStorage())
                {
                    AddObject(value);
                }
            }

            bool ManifestWidget::AddObject(const AZStd::shared_ptr<DataTypes::IManifestObject>& object)
            {
                for (ManifestWidgetPage* page : m_pages)
                {
                    if (page->SupportsType(object))
                    {
                        return page->AddObject(object);
                    }
                }
                return false;
            }

            bool ManifestWidget::RemoveObject(const AZStd::shared_ptr<DataTypes::IManifestObject>& object)
            {
                for (ManifestWidgetPage* page : m_pages)
                {
                    if (page->SupportsType(object))
                    {
                        return page->RemoveObject(object);
                    }
                }
                return false;
            }

            AZStd::shared_ptr<Containers::Scene> ManifestWidget::GetScene()
            {
                return m_scene;
            }

            AZStd::shared_ptr<const Containers::Scene> ManifestWidget::GetScene() const
            {
                return m_scene;
            }

            ManifestWidget* ManifestWidget::FindRoot(QWidget* child)
            {
                while (child != nullptr)
                {
                    ManifestWidget* manifestWidget = qobject_cast<ManifestWidget*>(child);
                    if (manifestWidget)
                    {
                        return manifestWidget;
                    }
                    else
                    {
                        child = child->parentWidget();
                    }
                }
                return nullptr;
            }

            const ManifestWidget* ManifestWidget::FindRoot(const QWidget* child)
            {
                while (child != nullptr)
                {
                    const ManifestWidget* manifestWidget = qobject_cast<const ManifestWidget*>(child);
                    if (manifestWidget)
                    {
                        return manifestWidget;
                    }
                    else
                    {
                        child = child->parentWidget();
                    }
                }
                return nullptr;
            }

            void ManifestWidget::BuildPages()
            {
                Events::ManifestMetaInfo::CategoryMap categories;
                EBUS_EVENT(Events::ManifestMetaInfoBus, GetCategoryAssignments, categories);

                AZStd::string currentCategory;
                AZStd::vector<AZ::Uuid> types;
                for (auto& category : categories)
                {
                    if (category.first != currentCategory)
                    {
                        // Skip first occurrence.
                        if (!currentCategory.empty())
                        {
                            ManifestWidgetPage* page = new ManifestWidgetPage(m_serializeContext, AZStd::move(types));
                            m_pages.push_back(page);
                            ui->m_tabs->addTab(page, currentCategory.c_str());
                        }
                        currentCategory = category.first;
                        AZ_Assert(types.empty(), "Expecting vectors to be empty after being moved.");
                    }
                    types.push_back(category.second);
                }
                // Add final page
                if (!currentCategory.empty())
                {
                    ManifestWidgetPage* page = new ManifestWidgetPage(m_serializeContext, AZStd::move(types));
                    m_pages.push_back(page);
                    ui->m_tabs->addTab(page, currentCategory.c_str());
                }
            }

            void ManifestWidget::ShowGraph()
            {
                if (!m_scene)
                {
                    return;
                }
                
                OverlayWidgetButtonList buttons;
                OverlayWidgetButton closeButton;
                closeButton.m_text = "Close";
                closeButton.m_triggersPop = true;
                buttons.push_back(&closeButton);

                SceneGraphWidget* graph = aznew SceneGraphWidget(m_scene->GetGraph());
                OverlayWidget::PushLayerToContainingOverlay(this, graph, buttons);
            }
        } // UI
    } // SceneAPI
} // AZ

#include <SceneWidgets/ManifestWidget.moc>