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

#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/SceneGraph.h>
#include <SceneAPI/SceneCore/Containers/Views/PairIterator.h>
#include <SceneAPI/SceneCore/DataTypes/IGraphObject.h>
#include <SceneAPI/SceneUI/SceneWidgets/ManifestWidget.h>
#include <SceneAPI/SceneUI/RowWidgets/NodeListSelectionWidget.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace UI
        {
            AZ_CLASS_ALLOCATOR_IMPL(NodeListSelectionWidget, SystemAllocator, 0)

            NodeListSelectionWidget::NodeListSelectionWidget(QWidget* parent)
                : QComboBox(parent)
                , m_classTypeId(Uuid::CreateNull())
                , m_exactClassTypeMatch(false)
                , m_hasDirtyList(true)
                , m_useShortNames(false)
                , m_excludeEndPoints(false)
                , m_defaultToDisabled(false)
            {
                connect(this, &QComboBox::currentTextChanged, this, &NodeListSelectionWidget::OnTextChange);
            }

            void NodeListSelectionWidget::SetCurrentSelection(const AZStd::string& selection)
            {
                m_currentSelection = selection; 
                if (!m_hasDirtyList)
                {
                    SetSelection();
                }
            }

            AZStd::string NodeListSelectionWidget::GetCurrentSelection() const
            {
                return currentText().toStdString().c_str();
            }

            void NodeListSelectionWidget::AddDisabledOption(const AZStd::string& option)
            {
                m_disabledOption = option;
                m_hasDirtyList = true;
            }

            void NodeListSelectionWidget::AddDisabledOption(AZStd::string&& option)
            {
                m_disabledOption = AZStd::move(option);
                m_hasDirtyList = true;
            }

            const AZStd::string& NodeListSelectionWidget::GetDisabledOption() const
            {
                return m_disabledOption;
            }

            void NodeListSelectionWidget::UseShortNames(bool use)
            {
                m_useShortNames = use;
                m_hasDirtyList = true;
            }

            void NodeListSelectionWidget::ExcludeEndPoints(bool exclude)
            {
                m_excludeEndPoints = exclude;
                m_hasDirtyList = true;
            }

            void NodeListSelectionWidget::DefaultToDisabled(bool value)
            {
                m_defaultToDisabled = value;
                m_hasDirtyList = true;
            }

            void NodeListSelectionWidget::SetClassTypeId(const Uuid& classTypeId)
            {
                m_classTypeId = classTypeId;
                m_hasDirtyList = true;
            }

            void NodeListSelectionWidget::ClearClassTypeId()
            {
                m_classTypeId = Uuid::CreateNull();
                m_hasDirtyList = true;
            }

            void NodeListSelectionWidget::UseExactClassTypeMatch(bool exactMatch)
            {
                m_exactClassTypeMatch = exactMatch;
                m_hasDirtyList = true;
            }

            void NodeListSelectionWidget::OnTextChange(const QString& text)
            {
                if (!m_hasDirtyList)
                {
                    m_currentSelection = text.toStdString().c_str();
                    emit valueChanged(m_currentSelection);
                }
            }

            void NodeListSelectionWidget::showEvent(QShowEvent* event)
            {
                if (m_hasDirtyList)
                {
                    clear();

                    ManifestWidget* mainWidget = ManifestWidget::FindRoot(this);
                    AZ_Assert(mainWidget, "NodeListSelectionWidget is not an (in)direct child of the ManifestWidget.");
                    if (!mainWidget)
                    {
                        return;
                    }
                    const Containers::SceneGraph& graph = mainWidget->GetScene()->GetGraph();

                    BuildList(graph);
                    AddDisabledOption();
                    SetSelection();
                    
                    m_hasDirtyList = false;
                }
                QComboBox::showEvent(event);
            }

            void NodeListSelectionWidget::BuildList(const Containers::SceneGraph& graph)
            {
                EntrySet entries;

                auto view = Containers::Views::MakePairView(graph.GetNameStorage(), graph.GetContentStorage());
                for (auto it = view.begin(); it != view.end(); ++it)
                {
                    if (!it->second || it->first.empty())
                    {
                        continue;
                    }

                    if (!IsCorrectType(*it->second))
                    {
                        continue;
                    }

                    if (m_excludeEndPoints)
                    {
                        Containers::SceneGraph::NodeIndex index = graph.ConvertToNodeIndex(it.GetFirstIterator());
                        if (graph.IsNodeEndPoint(index))
                        {
                            continue;
                        }
                    }

                    AddEntry(entries, it->first);
                }
            }

            bool NodeListSelectionWidget::IsCorrectType(const DataTypes::IGraphObject& object) const
            {
                if (m_classTypeId.IsNull())
                {
                    return true;
                }
                if (m_exactClassTypeMatch)
                {
                    return object.RTTI_GetType() == m_classTypeId;
                }
                else
                {
                    return object.RTTI_IsTypeOf(m_classTypeId);
                }
            }

            void NodeListSelectionWidget::AddEntry(EntrySet& entries, const std::string& name)
            {
                if (m_useShortNames)
                {
                    std::string shortName = Containers::SceneGraph::GetShortName(name);
                    if (entries.find(shortName) == entries.end())
                    {
                        entries.insert(shortName);
                        addItem(shortName.c_str());
                    }
                }
                else
                {
                    if (entries.find(name) == entries.end())
                    {
                        entries.insert(name);
                        addItem(name.c_str());
                    }
                }
            }

            void NodeListSelectionWidget::SetSelection()
            {
                QString entryName = m_useShortNames ? 
                    Containers::SceneGraph::GetShortName(m_currentSelection.c_str()).c_str() : m_currentSelection.c_str();
                int index = findText(entryName);
                if (m_disabledOption.empty())
                {
                    if (index >= 0)
                    {
                        setCurrentIndex(index);
                    }
                    else
                    {
                        // No disabled option available to set to first entry.
                        setCurrentIndex(0);
                    }
                }
                else
                {
                    // Check against index 1 as an empty string will return the separator.
                    if (index >= 0 && index != 1)
                    {
                        setCurrentIndex(index);
                    }
                    else if (!m_defaultToDisabled && count() >= 2)
                    {
                        // Pick third option as the first is the default followed
                        //      by the separator.
                        setCurrentIndex(2);
                    }
                    else
                    {
                        setCurrentIndex(0);
                    }
                }
            }

            void NodeListSelectionWidget::AddDisabledOption()
            {
                if (!m_disabledOption.empty())
                {
                    insertItem(0, m_disabledOption.c_str());
                    // Only add a separator if the disabled option isn't the only entry.
                    if (count() > 1)
                    {
                        insertSeparator(1);
                    }
                }
            }
        } // UI
    } // SceneAPI
} // AZ

#include <RowWidgets/NodeListSelectionWidget.moc>
