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
#include <AzCore/std/containers/vector.h>
#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>

namespace AZStd
{
    template<class T>
    class shared_ptr;
}

namespace AzToolsFramework
{
    class ReflectedPropertyEditor;
    class InstanceDataNode;
}

namespace AZ
{
    class SerializeContext;
    class SerializeContext::IObjectFactory;

    namespace SceneAPI
    {
        namespace DataTypes
        {
            class IManifestObject;
            class IGroup;
        }

        namespace SceneData
        {
            template<class T>
            class VectorWrapper;
        }

        namespace UI
        {
            // QT space
            namespace Ui
            {
                class ManifestVectorWidget;
            }
            class ManifestVectorWidget
                : public QWidget
                , public AzToolsFramework::IPropertyEditorNotify
            {
                Q_OBJECT
            public:
                AZ_CLASS_ALLOCATOR_DECL

                using ManifestVectorType = AZStd::vector<AZStd::shared_ptr<DataTypes::IManifestObject> >;

                ManifestVectorWidget(SerializeContext* serializeContext, QWidget* parent);

                void SetManifestVector(const ManifestVectorType& manifestVector, DataTypes::IManifestObject* ownerObject);
                ManifestVectorType GetManifestVector();

                void SetCollectionTypeName(const AZStd::string& typeName);

                bool ContainsManifestObject(const DataTypes::IManifestObject* object) const;
                bool RemoveManifestObject(const DataTypes::IManifestObject* object);

            signals:
                void valueChanged();

            protected:
                void DisplayAddPrompt();
                void AddNewObject(SerializeContext::IObjectFactory* factory, const AZStd::string& typeName);
                void UpdatePropertyGrid();
                void UpdatePropertyGridSize();

                // IPropertyEditorNotify
                void AfterPropertyModified(AzToolsFramework::InstanceDataNode* /*node*/) override;
                void RequestPropertyContextMenu(AzToolsFramework::InstanceDataNode* /*node*/, const QPoint& /*point*/) override;
                void BeforePropertyModified(AzToolsFramework::InstanceDataNode* /*node*/) override;
                void SetPropertyEditingActive(AzToolsFramework::InstanceDataNode* /*node*/) override;
                void SetPropertyEditingComplete(AzToolsFramework::InstanceDataNode* /*node*/) override;
                void SealUndoStack() override;

                SerializeContext* m_serializeContext;
                AzToolsFramework::ReflectedPropertyEditor* m_propertyEditor;
                QScopedPointer<Ui::ManifestVectorWidget> m_ui;
                DataTypes::IManifestObject* m_ownerObject;
                ManifestVectorType m_manifestVector;

            private slots:
                void OnPropertyGridContraction();
            };
        } // UI
    } // SceneAPI
} // AZ