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

#include <AzCore/std/bind/bind.h>
#include <RowWidgets/ui_NodeTreeSelectionWidget.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/DataTypes/IGraphObject.h>
#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ISceneNodeSelectionList.h>
#include <SceneAPI/SceneUI/RowWidgets/NodeTreeSelectionWidget.h>
#include <SceneAPI/SceneUI/SceneWidgets/ManifestWidget.h>
#include <SceneAPI/SceneUI/CommonWidgets/OverlayWidget.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace UI
        {
            AZ_CLASS_ALLOCATOR_IMPL(NodeTreeSelectionWidget, SystemAllocator, 0)

            NodeTreeSelectionWidget::NodeTreeSelectionWidget(QWidget* parent)
                : QWidget(parent)
                , ui(new Ui::NodeTreeSelectionWidget())
            {
                ui->setupUi(this);

                ui->m_selectButton->setIcon(QIcon(":/SceneUI/Manifest/TreeIcon.png"));
                connect(ui->m_selectButton, &QPushButton::clicked, this, &NodeTreeSelectionWidget::SelectButtonClicked);

                SetFilterName("nodes");
            }

            void NodeTreeSelectionWidget::SetList(const DataTypes::ISceneNodeSelectionList& list)
            {
                m_list = list.Copy();
                UpdateSelectionLabel();
            }

            void NodeTreeSelectionWidget::CopyListTo(DataTypes::ISceneNodeSelectionList& target)
            {
                if (m_list)
                {
                    m_list->CopyTo(target);
                }
            }
            
            void NodeTreeSelectionWidget::SetFilterName(const AZStd::string& name)
            {
                ui->m_selectButton->setToolTip(QString::asprintf("Select %s", name.c_str()));
                m_filterName = name;
                
                UpdateSelectionLabel();
            }
            
            void NodeTreeSelectionWidget::SetFilterName(AZStd::string&& name)
            {
                ui->m_selectButton->setToolTip(QString::asprintf("Select %s", name.c_str()));
                m_filterName = AZStd::move(name);
                
                UpdateSelectionLabel();
            }

            void NodeTreeSelectionWidget::AddFilterType(const Uuid& id)
            {
                m_filterTypes.push_back(id);
                UpdateSelectionLabel();
            }

            void NodeTreeSelectionWidget::SelectButtonClicked()
            {
                AZ_Assert(!m_treeWidget, "Node tree already active, NodeTreeSelectionWidget button pressed multiple times.");
                AZ_Assert(m_list, "Requested updating of selection list before it was set.");
                ManifestWidget* root = ManifestWidget::FindRoot(this);
                AZ_Assert(root, "NodeTreeSelectionWidget is not a child of a ManifestWidget.");
                if (!m_list || !root)
                {
                    return;
                }

                OverlayWidgetButtonList buttons;
                
                OverlayWidgetButton acceptButton;
                acceptButton.m_text = "Accept";
                acceptButton.m_callback = AZStd::bind(&NodeTreeSelectionWidget::ListChangesAccepted, this);
                acceptButton.m_triggersPop = true;

                OverlayWidgetButton cancelButton;
                cancelButton.m_text = "Cancel";
                cancelButton.m_callback = AZStd::bind(&NodeTreeSelectionWidget::ListChangesCanceled, this);
                cancelButton.m_triggersPop = true;

                buttons.push_back(&acceptButton);
                buttons.push_back(&cancelButton);

                m_treeWidget = aznew SceneGraphWidget(root->GetScene()->GetGraph(), *m_list, false);
                OverlayWidget::PushLayerToContainingOverlay(this, m_treeWidget.get(), buttons);
            }

            void NodeTreeSelectionWidget::ListChangesAccepted()
            {
                m_list = m_treeWidget->ClaimTargetList();
                UpdateSelectionLabel();
                m_treeWidget.reset();
                
                emit valueChanged();
            }

            void NodeTreeSelectionWidget::ListChangesCanceled()
            {
                m_treeWidget.reset();
            }

            void NodeTreeSelectionWidget::UpdateSelectionLabel()
            {
                if (m_list)
                {
                    size_t selected = CalculateSelectedCount();
                    size_t total = CalculateTotalCount();
                    
                    if (total == 0)
                    {
                        ui->m_statusLabel->setText("Default selection.");
                    }
                    else if (selected == total)
                    {
                        ui->m_statusLabel->setText(QString::asprintf("All %s selected.", m_filterName.c_str()));
                    }
                    else
                    {
                        ui->m_statusLabel->setText(
                            QString::asprintf("%i of %i %s selected.", selected, total, m_filterName.c_str()));
                    }
                }
                else
                {
                    ui->m_statusLabel->setText("No list assigned.");
                }
            }

            size_t NodeTreeSelectionWidget::CalculateSelectedCount() const
            {
                if (!m_list)
                {
                    return 0;
                }

                if (m_filterTypes.empty())
                {
                    return m_list->GetSelectedNodeCount();
                }
                else
                {
                    const ManifestWidget* root = ManifestWidget::FindRoot(this);
                    AZ_Assert(root, "NodeTreeSelectionWidget is not a child of a ManifestWidget.");
                    if (!m_list || !root)
                    {
                        return 0;
                    }
                    const Containers::SceneGraph& graph = root->GetScene()->GetGraph();

                    size_t result = 0;
                    size_t selectedCount = m_list->GetSelectedNodeCount();
                    for (size_t i = 0; i < selectedCount; ++i)
                    {
                        Containers::SceneGraph::NodeIndex index = graph.Find(m_list->GetSelectedNode(i).c_str());
                        if (!index.IsValid())
                        {
                            continue;
                        }

                        AZStd::shared_ptr<const DataTypes::IGraphObject> object = graph.GetNodeContent(index);
                        if (!object)
                        {
                            continue;
                        }
                        for (const Uuid& type : m_filterTypes)
                        {
                            if (object->RTTI_IsTypeOf(type))
                            {
                                result++;
                                break;
                            }
                        }
                    }
                    return result;
                }
            }

            size_t NodeTreeSelectionWidget::CalculateTotalCount() const
            {
                const ManifestWidget* root = ManifestWidget::FindRoot(this);
                AZ_Assert(root, "NodeTreeSelectionWidget is not a child of a ManifestWidget.");
                if (!m_list || !root)
                {
                    return 0;
                }
                const Containers::SceneGraph& graph = root->GetScene()->GetGraph();

                size_t total = 0;
                if (m_filterTypes.empty())
                {
                    Containers::SceneGraph::HierarchyStorageConstData view = graph.GetHierarchyStorage();
                    if (!graph.GetNodeContent(graph.GetRoot()) && graph.GetNodeName(graph.GetRoot()).empty())
                    {
                        view = Containers::SceneGraph::HierarchyStorageConstData(view.begin() + 1, view.end());
                    }

                    for (auto& it : view)
                    {
                        if (!it.IsEndPoint())
                        {
                            total++;
                        }
                    }
                }
                else
                {
                    for (auto& data : graph.GetContentStorage())
                    {
                        if (!data)
                        {
                            continue;
                        }
                        for (const Uuid& type : m_filterTypes)
                        {
                            if (data->RTTI_IsTypeOf(type))
                            {
                                total++;
                                break;
                            }
                        }
                    }
                }
                return total;
            }
        } // UI
    } // SceneAPI
} // AZ

#include <RowWidgets/NodeTreeSelectionWidget.moc>
