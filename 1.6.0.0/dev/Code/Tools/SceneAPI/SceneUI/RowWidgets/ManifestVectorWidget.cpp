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

#include <QEvent.h>
#include <QMenu.h>
#include <QTimer.h>
#include <RowWidgets/ui_ManifestVectorWidget.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>
#include <AzToolsFramework/UI/PropertyEditor/PropertyRowWidget.hxx>
#include <AzToolsFramework/UI/PropertyEditor/ReflectedPropertyEditor.hxx>
#include <AzToolsFramework/Debug/TraceContext.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/SceneManifest.h>
#include <SceneAPI/SceneCore/DataTypes/IManifestObject.h>
#include <SceneAPI/SceneCore/Utilities/Reporting.h>
#include <SceneAPI/SceneCore/Events/ManifestMetaInfoBus.h>
#include <SceneAPI/SceneUI/RowWidgets/ManifestVectorWidget.h>
#include <SceneAPI/SceneUI/SceneWidgets/ManifestWidget.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace UI
        {
            AZ_CLASS_ALLOCATOR_IMPL(ManifestVectorWidget, SystemAllocator, 0)

            ManifestVectorWidget::ManifestVectorWidget(SerializeContext* serializeContext, QWidget* parent)
                : QWidget(parent)
                , m_serializeContext(serializeContext)
                , m_propertyEditor(nullptr)
                , m_ui(new Ui::ManifestVectorWidget())
            {
                m_ui->setupUi(this);

                m_propertyEditor = new AzToolsFramework::ReflectedPropertyEditor(this);
                m_propertyEditor->Setup(m_serializeContext, this, false, 175);
                m_propertyEditor->show();
                connect(m_propertyEditor, &AzToolsFramework::ReflectedPropertyEditor::OnExpansionContractionDone,
                    this, &ManifestVectorWidget::OnPropertyGridContraction);
                m_ui->m_mainLayout->insertWidget(1, m_propertyEditor);

                connect(m_ui->m_addObjectButton, &QPushButton::pressed, this, &ManifestVectorWidget::DisplayAddPrompt);

                // Add empty menu for visual consistency.
                m_ui->m_addObjectButton->setMenu(new QMenu(this));
            }

            void ManifestVectorWidget::SetManifestVector(const ManifestVectorType& manifestVector, DataTypes::IManifestObject* ownerObject)
            {
                AZ_Assert(ownerObject, "ManifestVectorWidgets must be initialized with a non-null owner object.");
                m_manifestVector = manifestVector;
                m_ownerObject = ownerObject;
                UpdatePropertyGrid();
            }

            ManifestVectorWidget::ManifestVectorType ManifestVectorWidget::GetManifestVector()
            {
                return m_manifestVector;
            }

            void ManifestVectorWidget::SetCollectionTypeName(const AZStd::string& typeName)
            {
                QString addString = QString("Add ") + typeName.c_str();
                m_ui->m_addObjectButton->setText(addString);
            }

            bool ManifestVectorWidget::ContainsManifestObject(const DataTypes::IManifestObject* object) const
            {
                for (auto& containedObject : m_manifestVector)
                {
                    if (containedObject.get() == object)
                    {
                        return true;
                    }
                }

                return false;
            }

            bool ManifestVectorWidget::RemoveManifestObject(const DataTypes::IManifestObject* object)
            {
                AZ_TraceContext("Remove Object Type", object->RTTI_GetTypeName());

                for (auto it = m_manifestVector.begin(); it < m_manifestVector.end(); ++it)
                {
                    if ((*it).get() == object)
                    {
                        m_manifestVector.erase(it);
                        QTimer::singleShot(0, this, 
                            [this]()
                            {
                                UpdatePropertyGrid();
                                emit valueChanged();
                            });
                        return true;
                    }
                }

                AZ_TracePrintf(Utilities::WarningWindow, "Tried to remove an object that was not contained in the vector.");

                return false;
            }

            void ManifestVectorWidget::DisplayAddPrompt()
            {
                Events::ManifestMetaInfo::ModifiersList availableManifestUUIDs;
                ManifestWidget* root = ManifestWidget::FindRoot(this);
                AZ_Assert(root, "ManifestVectorWidget is not a child of a ManifestWidget.");
                if (!root)
                {
                    return;
                }
                AZStd::shared_ptr<Containers::Scene> scene = root->GetScene();
                EBUS_EVENT(Events::ManifestMetaInfoBus, GetAvailableModifiers, availableManifestUUIDs, scene, static_cast<const DataTypes::IManifestObject*>(m_ownerObject));

                AZ_TraceContext("Parent ManifestObject type", m_ownerObject->RTTI_GetTypeName());

                QMenu* objectMenu = m_ui->m_addObjectButton->menu();
                objectMenu->clear();
                
                for (auto& manifestUUID : availableManifestUUIDs)
                {
                    const AZ::SerializeContext::ClassData* manifestClassData = m_serializeContext->FindClassData(manifestUUID);

                    AZ_TraceContext("Child ManifestObject UUID", manifestUUID.ToString<AZStd::string>());

                    if (!manifestClassData)
                    {
                        AZ_TracePrintf(Utilities::WarningWindow, "Class data was not registered for class, it will not be available as an option");
                        continue;
                    }

                    AZStd::string displayName;
                    if (manifestClassData->m_editData && manifestClassData->m_editData->m_name[0] != '\0')
                    {
                        displayName = manifestClassData->m_editData->m_name;
                    }
                    else if(manifestClassData->m_name[0] != '\0')
                    {
                        displayName = manifestClassData->m_name;
                    }
                    else
                    {
                        AZ_TracePrintf(Utilities::WarningWindow, "Class data did not contain a human readable name for class, it will not be available as an option");
                        continue;
                    }

                    QAction* objectCreateAction = new QAction(displayName.c_str(), m_ui->m_addObjectButton);
                    connect(objectCreateAction, &QAction::triggered, 
                        [this, manifestClassData, displayName]() 
                        {
                            this->AddNewObject(manifestClassData->m_factory, displayName); 
                        });
                    objectMenu->addAction(objectCreateAction);
                }
            }

            void ManifestVectorWidget::AddNewObject(SerializeContext::IObjectFactory* factory, const AZStd::string& objectName)
            {
                AZStd::shared_ptr<DataTypes::IManifestObject> newObject(static_cast<DataTypes::IManifestObject*>(factory->Create(objectName.c_str())));
                
                ManifestWidget* root = ManifestWidget::FindRoot(this);
                AZ_Assert(root, "ManifestVectorWidget is not a child of a ManifestWidget.");
                if (!root)
                {
                    return;
                }
                AZStd::shared_ptr<Containers::Scene> scene = root->GetScene();
                EBUS_EVENT(Events::ManifestMetaInfoBus, InitializeObject, *scene, newObject.get());
                
                m_manifestVector.push_back(newObject);
                UpdatePropertyGrid();
                emit valueChanged();
            }

            void ManifestVectorWidget::UpdatePropertyGrid()
            {
                QSignalBlocker(this);
                m_propertyEditor->ClearInstances();
                for (auto &object : m_manifestVector)
                {
                    if(object)
                    {
                        m_propertyEditor->AddInstance(object.get(), object->RTTI_GetType());
                    }
                }
                m_propertyEditor->InvalidateAll();
                m_propertyEditor->ExpandAll();

                UpdatePropertyGridSize();
            }

            void ManifestVectorWidget::UpdatePropertyGridSize()
            {
                if (m_propertyEditor)
                {
                    int height = m_propertyEditor->GetContentHeight();
                    m_propertyEditor->setMinimumHeight(height);
                    m_propertyEditor->setMaximumHeight(height);
                }
            }

            void ManifestVectorWidget::AfterPropertyModified(AzToolsFramework::InstanceDataNode* /*node*/)
            {
                emit valueChanged();
            }

            void ManifestVectorWidget::RequestPropertyContextMenu(AzToolsFramework::InstanceDataNode* /*node*/, const QPoint& /*point*/)
            {
            }

            void ManifestVectorWidget::BeforePropertyModified(AzToolsFramework::InstanceDataNode* /*node*/)
            {
            }

            void ManifestVectorWidget::SetPropertyEditingActive(AzToolsFramework::InstanceDataNode* /*node*/)
            {
            }

            void ManifestVectorWidget::SetPropertyEditingComplete(AzToolsFramework::InstanceDataNode* /*node*/)
            {
            }

            void ManifestVectorWidget::SealUndoStack()
            {
            }

            void ManifestVectorWidget::OnPropertyGridContraction()
            {
                UpdatePropertyGridSize();
            }
        } // UI
    } // SceneAPI
} // AZ

#include <RowWidgets/ManifestVectorWidget.moc>