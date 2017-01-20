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

#include <QStandardItemModel>
#include <SceneWidgets/ui_SceneGraphWidget.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/containers/stack.h>
#include <AzToolsFramework/Debug/TraceContext.h>
#include <SceneAPI/SceneCore/Containers/SceneGraph.h>
#include <SceneAPI/SceneCore/Containers/Views/PairIterator.h>
#include <SceneAPI/SceneCore/Containers/Views/SceneGraphDownwardsIterator.h>
#include <SceneAPI/SceneCore/DataTypes/IGraphObject.h>
#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ISceneNodeSelectionList.h>
#include <SceneAPI/SceneCore/Events/GraphMetaInfoBus.h>
#include <SceneAPI/SceneCore/Utilities/SceneGraphSelector.h>
#include <SceneAPI/SceneUI/SceneWidgets/SceneGraphWidget.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace UI
        {
            AZ_CLASS_ALLOCATOR_IMPL(SceneGraphWidget, SystemAllocator, 0)

            SceneGraphWidget::SceneGraphWidget(const Containers::SceneGraph& graph, QWidget* parent)
                : QWidget(parent)
                , ui(new Ui::SceneGraphWidget())
                , m_treeModel(new QStandardItemModel())
                , m_graph(graph)
                , m_targetList(nullptr)
                , m_selectedCount(0)
                , m_totalCount(0)
            {
                SetupUI(false);
                BuildTree(false, true);
            }

            SceneGraphWidget::SceneGraphWidget(const Containers::SceneGraph& graph, bool includeEndPoints, QWidget* parent)
                : QWidget(parent)
                , ui(new Ui::SceneGraphWidget())
                , m_treeModel(new QStandardItemModel())
                , m_graph(graph)
                , m_targetList(nullptr)
                , m_selectedCount(0)
                , m_totalCount(0)
            {
                SetupUI(false);
                BuildTree(false, includeEndPoints);
            }

            SceneGraphWidget::SceneGraphWidget(const Containers::SceneGraph& graph, const DataTypes::ISceneNodeSelectionList& targetList,
                bool includeEndPoints, QWidget* parent)
                : QWidget(parent)
                , ui(new Ui::SceneGraphWidget())
                , m_treeModel(new QStandardItemModel())
                , m_graph(graph)
                , m_targetList(targetList.Copy())
                , m_selectedCount(0)
                , m_totalCount(0)
            {
                AZ_Assert(m_targetList, "Target list failed to copy.");
                if (m_targetList)
                {
                    Utilities::SceneGraphSelector::UpdateNodeSelection(graph, *m_targetList);
                }
                SetupUI(true);
                BuildTree(true, includeEndPoints);
            }

            AZStd::unique_ptr<DataTypes::ISceneNodeSelectionList>&& SceneGraphWidget::ClaimTargetList()
            {
                return AZStd::move(m_targetList);
            }

            void SceneGraphWidget::SetupUI(bool isCheckable)
            {
                ui->setupUi(this);
                ui->m_selectionTree->setHeaderHidden(true);
                ui->m_selectionTree->setModel(m_treeModel.data());

                if (!isCheckable)
                {
                    ui->m_selectAllCheckBox->hide();
                }

                connect(ui->m_selectAllCheckBox, &QCheckBox::stateChanged, this, &SceneGraphWidget::OnSelectAllCheckboxStateChanged);
                connect(m_treeModel.data(), &QStandardItemModel::itemChanged, this, &SceneGraphWidget::OnTreeItemStateChanged);
            }

            void SceneGraphWidget::BuildTree(bool isCheckable, bool includeEndPoints)
            {
                setUpdatesEnabled(false);
                QSignalBlocker blocker(m_treeModel.data());

                m_treeItems = AZStd::vector<QStandardItem*>(m_graph.GetNodeCount(), nullptr);
                
                auto sceneGraphView = Containers::Views::MakePairView(m_graph.GetNameStorage(), m_graph.GetContentStorage());
                auto sceneGraphDownardsIteratorView = Containers::Views::MakeSceneGraphDownwardsView<Containers::Views::BreadthFirst>(
                    m_graph, m_graph.GetRoot(), sceneGraphView.begin(), true);

                // Some importer implementations may write an empty node to force collection all items under a common root
                //  If that is the case, we're going to skip it so we don't show the user an empty node root
                auto iterator = sceneGraphDownardsIteratorView.begin();
                if (iterator->first.empty() && !iterator->second)
                {
                    ++iterator;
                }

                for (; iterator != sceneGraphDownardsIteratorView.end(); ++iterator)
                {
                    Containers::SceneGraph::HierarchyStorageConstIterator hierarchy = iterator.GetHierarchyIterator();
                    Containers::SceneGraph::NodeIndex currIndex = m_graph.ConvertToNodeIndex(hierarchy);
                    AZ_Assert(currIndex.IsValid(), "While iterating through the Scene Graph an unexpected invalid entry was found.");
                    Containers::SceneGraph::NodeIndex parentIndex = m_graph.GetNodeParent(currIndex);
                    AZStd::shared_ptr<const DataTypes::IGraphObject> currItem = iterator->second;

                    if (hierarchy->IsEndPoint() && !includeEndPoints)
                    {
                        continue;
                    }

                    QStandardItem* treeItem = BuildTreeItem(currItem, iterator->first, isCheckable, hierarchy->IsEndPoint());
                    if (IsSelected(iterator->first.c_str()))
                    {
                        treeItem->setCheckState(Qt::CheckState::Checked);
                        m_selectedCount++;
                    }
                    m_treeItems[currIndex.AsNumber()] = treeItem;
                    if (parentIndex.IsValid() && m_treeItems[parentIndex.AsNumber()])
                    {
                        m_treeItems[parentIndex.AsNumber()]->appendRow(treeItem);
                    }
                    else
                    {
                        m_treeModel->appendRow(treeItem);
                    }

                    m_totalCount++;
                }

                ui->m_selectionTree->expandAll();
                UpdateSelectAllStatus();

                setUpdatesEnabled(true);
            }

            QStandardItem* SceneGraphWidget::BuildTreeItem(const AZStd::shared_ptr<const DataTypes::IGraphObject>& object, 
                const std::string& fullName, bool isCheckable, bool isEndPoint) const
            {
                std::string shortName = Containers::SceneGraph::GetShortName(fullName);

                QStandardItem* treeItem = new QStandardItem(shortName.c_str());
                treeItem->setData(QString(fullName.c_str()));
                treeItem->setEditable(false);
                treeItem->setCheckable(isCheckable);
                if (object)
                {
                    AZStd::string toolTip;
                    EBUS_EVENT(Events::GraphMetaInfoBus, GetToolTip, toolTip, object.get());
                    if (toolTip.empty())
                    {
                        treeItem->setToolTip(QString::asprintf("%s\n<%s>", fullName.c_str(), object->RTTI_GetTypeName()));
                    }
                    else
                    {
                        treeItem->setToolTip(QString::asprintf("%s\n\n%s", fullName.c_str(), toolTip.c_str()));
                    }
                    
                    AZStd::string iconPath;
                    EBUS_EVENT(Events::GraphMetaInfoBus, GetIconPath, iconPath, object.get());
                    if (!iconPath.empty())
                    {
                        treeItem->setIcon(QIcon(iconPath.c_str()));
                    }
                    else if (!isEndPoint)
                    {
                        treeItem->setIcon(QIcon(":/SceneUI/Graph/NodeIcon.png"));
                    }
                }
                else
                {
                    treeItem->setToolTip(fullName.c_str());
                    if (!isEndPoint)
                    {
                        treeItem->setIcon(QIcon(":/SceneUI/Graph/NodeIcon.png"));
                    }
                }

                return treeItem;
            }

            void SceneGraphWidget::OnSelectAllCheckboxStateChanged()
            {
                setUpdatesEnabled(false);
                QSignalBlocker blocker(m_treeModel.data());
                Qt::CheckState state = ui->m_selectAllCheckBox->checkState();

                if (m_targetList)
                {
                    m_targetList->ClearSelectedNodes();
                    m_targetList->ClearUnselectedNodes();

                    for (QStandardItem* item : m_treeItems)
                    {
                        if (!item)
                        {
                            continue;
                        }
                        
                        item->setCheckState(state);

                        QVariant itemData = item->data();
                        if (itemData.isValid())
                        {
                            AZStd::string fullName = itemData.toString().toLatin1();
                            if (state == Qt::CheckState::Unchecked)
                            {
                                m_targetList->RemoveSelectedNode(fullName);
                            }
                            else
                            {
                                m_targetList->AddSelectedNode(AZStd::move(fullName));
                            }
                        }
                    }
                }
                else
                {
                    for (QStandardItem* item : m_treeItems)
                    {
                        if (item)
                        {
                            item->setCheckState(state);
                        }
                    }
                }

                m_selectedCount = (state == Qt::CheckState::Unchecked) ? 0 : m_totalCount;
                UpdateSelectAllStatus();

                setUpdatesEnabled(true);
            }

            void SceneGraphWidget::OnTreeItemStateChanged(QStandardItem* item)
            {
                setUpdatesEnabled(false);
                QSignalBlocker blocker(m_treeModel.data());

                Qt::CheckState state = item->checkState();
                bool decrement = (state == Qt::CheckState::Unchecked);
                if (decrement)
                {
                    if (!RemoveSelection(item))
                    {
                        item->setCheckState(Qt::CheckState::Checked);
                        return;
                    }
                }
                else
                {
                    if (!AddSelection(item))
                    {
                        item->setCheckState(Qt::CheckState::Unchecked);
                        return;
                    }
                }

                AZStd::stack<QStandardItem*> children;
                int rowCount = item->rowCount();
                for (int index = 0; index < rowCount; ++index)
                {
                    children.push(item->child(index));
                }

                while (!children.empty())
                {
                    QStandardItem* current = children.top();
                    children.pop();

                    if (decrement)
                    {
                        if (current->checkState() != Qt::CheckState::Unchecked && RemoveSelection(current))
                        {
                            current->setCheckState(state);
                        }
                    }
                    else
                    {
                        if (current->checkState() == Qt::CheckState::Unchecked && AddSelection(current))
                        {
                            current->setCheckState(state);
                        }
                    }

                    int rowCount = current->rowCount();
                    for (int index = 0; index < rowCount; ++index)
                    {
                        children.push(current->child(index));
                    }
                }

                UpdateSelectAllStatus();
                setUpdatesEnabled(true);
            }

            void SceneGraphWidget::UpdateSelectAllStatus()
            {
                QSignalBlocker blocker(ui->m_selectAllCheckBox);
                if (m_selectedCount == m_totalCount)
                {
                    ui->m_selectAllCheckBox->setText("Unselect all");
                    ui->m_selectAllCheckBox->setCheckState(Qt::CheckState::Checked);
                }
                else
                {
                    ui->m_selectAllCheckBox->setText("Select all");
                    ui->m_selectAllCheckBox->setCheckState(Qt::CheckState::Unchecked);
                }
            }

            bool SceneGraphWidget::IsSelected(const AZStd::string& fullName) const
            {
                if (!m_targetList)
                {
                    return false;
                }

                size_t count = m_targetList->GetSelectedNodeCount();
                for (size_t i = 0; i < count; ++i)
                {
                    if (m_targetList->GetSelectedNode(i) == fullName)
                    {
                        return true;
                    }
                }

                return false;
            }

            bool SceneGraphWidget::AddSelection(const QStandardItem* item)
            {
                if (!m_targetList)
                {
                    return true;
                }

                QVariant itemData = item->data();
                if (!itemData.isValid() || itemData.type() != QVariant::Type::String)
                {
                    return false;
                }

                AZStd::string fullName = itemData.toString().toLatin1();
                AZ_TraceContext("Item for addition", fullName);
                Containers::SceneGraph::NodeIndex nodeIndex = m_graph.Find(fullName.c_str());
                AZ_Assert(nodeIndex.IsValid(), "Invalid node added to tree.");
                if (!nodeIndex.IsValid())
                {
                    return false;
                }

                m_targetList->AddSelectedNode(AZStd::move(fullName));
                m_selectedCount++;
                AZ_Assert(m_selectedCount <= m_totalCount, "Selected node count exceeds available node count.");
                return true;
            }

            bool SceneGraphWidget::RemoveSelection(const QStandardItem* item)
            {
                if (!m_targetList)
                {
                    return true;
                }

                QVariant itemData = item->data();
                if (!itemData.isValid() || itemData.type() != QVariant::Type::String)
                {
                    return false;
                }

                AZStd::string fullName = itemData.toString().toLatin1();
                AZ_TraceContext("Item for removal", fullName);
                Containers::SceneGraph::NodeIndex nodeIndex = m_graph.Find(fullName.c_str());
                AZ_Assert(nodeIndex.IsValid(), "Invalid node added to tree.");
                if (!nodeIndex.IsValid())
                {
                    return false;
                }

                m_targetList->RemoveSelectedNode(fullName);
                AZ_Assert(m_selectedCount > 0, "Selected node count can not be decremented below zero.");
                m_selectedCount--;
                return true;
            }
        } // UI
    } // SceneAPI
} // AZ

#include <SceneWidgets/SceneGraphWidget.moc>
