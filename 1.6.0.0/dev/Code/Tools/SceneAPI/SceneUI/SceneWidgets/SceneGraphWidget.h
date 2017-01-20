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

#include <string>
#include <QWidget>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/Memory/SystemAllocator.h>

class QStandardItem; 
class QStandardItemModel;

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            class SceneGraph;
        }
        namespace DataTypes
        {
            class IGraphObject;
            class ISceneNodeSelectionList;
        }

        namespace UI
        {
            // QT space
            namespace Ui
            {
                class SceneGraphWidget;
            }
            class SceneGraphWidget
                : public QWidget
            {
            public:
                Q_OBJECT
            public:
                AZ_CLASS_ALLOCATOR_DECL

                SceneGraphWidget(const Containers::SceneGraph& graph, QWidget* parent = nullptr);
                SceneGraphWidget(const Containers::SceneGraph& graph, bool includeEndPoints, QWidget* parent = nullptr);
                SceneGraphWidget(const Containers::SceneGraph& graph, const DataTypes::ISceneNodeSelectionList& targetList, 
                    bool includeEndPoints, QWidget* parent = nullptr);

                AZStd::unique_ptr<DataTypes::ISceneNodeSelectionList>&& ClaimTargetList();
                
            protected:
                virtual void SetupUI(bool isCheckable);
                virtual void BuildTree(bool isCheckable, bool includeEndPoints);
                virtual QStandardItem* BuildTreeItem(const AZStd::shared_ptr<const DataTypes::IGraphObject>& object, 
                    const std::string& fullName, bool isCheckable, bool isEndPoint) const;

                virtual void OnSelectAllCheckboxStateChanged();
                virtual void OnTreeItemStateChanged(QStandardItem* item);

                virtual void UpdateSelectAllStatus();

                virtual bool IsSelected(const AZStd::string& fullName) const;
                virtual bool AddSelection(const QStandardItem* item);
                virtual bool RemoveSelection(const QStandardItem* item);

                AZStd::vector<QStandardItem*> m_treeItems;
                QScopedPointer<Ui::SceneGraphWidget> ui;
                QScopedPointer<QStandardItemModel> m_treeModel;
                AZStd::unique_ptr<DataTypes::ISceneNodeSelectionList> m_targetList;
                const Containers::SceneGraph& m_graph;

                size_t m_selectedCount;
                size_t m_totalCount;
            };
        } // UI
    } // SceneAPI
} // AZ
