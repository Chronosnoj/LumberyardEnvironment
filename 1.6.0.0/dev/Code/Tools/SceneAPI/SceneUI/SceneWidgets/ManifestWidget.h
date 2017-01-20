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
#include <QTabWidget.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/containers/unordered_map.h>
#include <SceneAPI/SceneUI/SceneUIConfiguration.h>

namespace AZ
{
    class SerializeContext;

    namespace SceneAPI
    {
        namespace Containers
        {
            class Scene;
        }
        namespace DataTypes
        {
            class IManifestObject;
        }

        namespace UI
        {
            // QT space
            namespace Ui
            {
                class ManifestWidget;
            }

            class ManifestWidgetPage;

            class ManifestWidget : public QWidget
            {
                Q_OBJECT
            public:
                using PageList = AZStd::vector<ManifestWidgetPage*>;

                SCENE_UI_API explicit ManifestWidget(SerializeContext* serializeContext, QWidget* parent = nullptr);

                SCENE_UI_API void BuildFromScene(const AZStd::shared_ptr<Containers::Scene>& scene);
                SCENE_UI_API bool AddObject(const AZStd::shared_ptr<DataTypes::IManifestObject>& object);
                SCENE_UI_API bool RemoveObject(const AZStd::shared_ptr<DataTypes::IManifestObject>& object);
                SCENE_UI_API AZStd::shared_ptr<Containers::Scene> GetScene();
                SCENE_UI_API AZStd::shared_ptr<const Containers::Scene> GetScene() const;

                // Finds this ManifestWidget if the given widget is it's child, otherwise returns null.
                SCENE_UI_API static ManifestWidget* FindRoot(QWidget* child);
                // Finds this ManifestWidget if the given widget is it's child, otherwise returns null.
                SCENE_UI_API static const ManifestWidget* FindRoot(const QWidget* child);

            protected:
                SCENE_UI_API void BuildPages();
                SCENE_UI_API void ShowGraph();

                PageList m_pages;
                QScopedPointer<Ui::ManifestWidget> ui;
                AZStd::shared_ptr<Containers::Scene> m_scene;
                SerializeContext* m_serializeContext;
            };
        } // UI
    } // SceneAPI
} // AZ
