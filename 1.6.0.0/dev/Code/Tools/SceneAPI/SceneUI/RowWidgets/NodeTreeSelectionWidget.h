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
#include <AzCore/Math/Uuid.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <SceneAPI/SceneUI/SceneUIConfiguration.h>
#include <SceneAPI/SceneUI/SceneWidgets/SceneGraphWidget.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace DataTypes
        {
            class ISceneNodeSelectionList;
        }

        namespace UI
        {
            // QT space
            namespace Ui
            {
                class NodeTreeSelectionWidget;
            }
            class NodeTreeSelectionWidget : public QWidget
            {
                Q_OBJECT
            public:
                AZ_CLASS_ALLOCATOR_DECL

                explicit NodeTreeSelectionWidget(QWidget* parent);

                void SetList(const DataTypes::ISceneNodeSelectionList& list);
                void CopyListTo(DataTypes::ISceneNodeSelectionList& target);
                
                void SetFilterName(const AZStd::string& name);
                void SetFilterName(AZStd::string&& name);

                void AddFilterType(const Uuid& id);

            signals:
                void valueChanged();

            protected:
                void SelectButtonClicked();
                void ListChangesAccepted();
                void ListChangesCanceled();
                void UpdateSelectionLabel();

                size_t CalculateSelectedCount() const;
                size_t CalculateTotalCount() const;

                AZStd::vector<Uuid> m_filterTypes;
                AZStd::string m_filterName;
                QScopedPointer<Ui::NodeTreeSelectionWidget> ui;
                AZStd::unique_ptr<SceneGraphWidget> m_treeWidget;
                AZStd::unique_ptr<DataTypes::ISceneNodeSelectionList> m_list;
            };
        } // UI
    } // SceneAPI
} // AZ
