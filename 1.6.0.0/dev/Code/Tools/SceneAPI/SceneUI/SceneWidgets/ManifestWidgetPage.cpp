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

#include <QMenu.h>
#include <QTimer.h>
#include <AzToolsFramework/Debug/TraceContext.h>
#include <SceneWidgets/ui_ManifestWidgetPage.h>
#include <SceneAPI/SceneCore/DataTypes/IManifestObject.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/SceneManifest.h>
#include <SceneAPI/SceneCore/Containers/Utilities/RuleNameUnicity.h>
#include <SceneAPI/SceneCore/Containers/Views/PairIterator.h>
#include <SceneAPI/SceneCore/DataTypes/Groups/IMeshGroup.h>
#include <SceneAPI/SceneCore/Events/ManifestMetaInfoBus.h>
#include <SceneAPI/SceneUI/SceneWidgets/ManifestWidget.h>
#include <SceneAPI/SceneUI/SceneWidgets/ManifestWidgetPage.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace UI
        {
            ManifestWidgetPage::ManifestWidgetPage(SerializeContext* context, AZStd::vector<AZ::Uuid>&& classTypeIds)
                : m_classTypeIds(AZStd::move(classTypeIds))
                , ui(new Ui::ManifestWidgetPage())
                , m_propertyEditor(nullptr)
                , m_context(context)
            {
                ui->setupUi(this);

                m_propertyEditor = new AzToolsFramework::ReflectedPropertyEditor(nullptr);
                m_propertyEditor->Setup(context, nullptr, true, 175);
                m_propertyEditor->show();
                ui->m_mainLayout->insertWidget(0, m_propertyEditor);

                BuildAndConnectAddButton();
            }

            bool ManifestWidgetPage::SupportsType(const AZStd::shared_ptr<DataTypes::IManifestObject>& object)
            {
                for (Uuid& id : m_classTypeIds)
                {
                    if (object->RTTI_IsTypeOf(id))
                    {
                        return true;
                    }
                }
                return false;
            }

            bool ManifestWidgetPage::AddObject(const AZStd::shared_ptr<DataTypes::IManifestObject>& object)
            {
                if (SupportsType(object))
                {
                    if (!m_propertyEditor->AddInstance(object.get(), object->RTTI_GetType()))
                    {
                        AZ_Assert(false, "Failed to add manifest object to Reflected Property Editor.");
                        return false;
                    }
                    m_propertyEditor->InvalidateAll();
                    m_propertyEditor->ExpandAll();

                    m_objects.push_back(object);

                    return true;
                }
                else
                {
                    return false;
                }
            }

            bool ManifestWidgetPage::RemoveObject(const AZStd::shared_ptr<DataTypes::IManifestObject>& object)
            {
                if (SupportsType(object))
                {
                    // Explicitly keep a copy of the shared pointer to guarantee that the manifest object isn't
                    //      deleted before it can be queued for the delete deletion.
                    AZStd::shared_ptr<DataTypes::IManifestObject> temp = object;
                    (void)temp;

                    auto it = AZStd::find(m_objects.begin(), m_objects.end(), object);
                    if (it == m_objects.end())
                    {
                        AZ_Assert(false, "Manifest object not part of manifest page.");
                        return false;
                    }

                    m_objects.erase(it);

                    // If the property editor is immediately updated here QT will do some processing in an unexpected order,
                    //      leading to heap corruption. To avoid this, keep a cached version of the deleted object and
                    //      delay the rebuilding of the property editor to the end of the update cycle.
                    QTimer::singleShot(0, this,
                        [this, object]()
                        {
                            // Explicitly keep a copy of the shared pointer to guarantee that the manifest object isn't
                            //      deleted between updates of QT.
                            (void)object;

                            m_propertyEditor->ClearInstances();
                            for (auto& instance : m_objects)
                            {
                                if (!m_propertyEditor->AddInstance(instance.get(), instance->RTTI_GetType()))
                                {
                                    AZ_Assert(false, "Failed to add manifest object to Reflected Property Editor.");
                                }
                            }
                            m_propertyEditor->InvalidateAll();
                            m_propertyEditor->ExpandAll();
                        });
                    return true;
                }
                else
                {
                    return false;
                }
            }

            void ManifestWidgetPage::Clear()
            {
                m_objects.clear();
                m_propertyEditor->ClearInstances();
            }

            void ManifestWidgetPage::OnAdd()
            {
                if (m_classTypeIds.size() > 0)
                {
                    AddNewObject(m_classTypeIds[0]);
                }
            }

            void ManifestWidgetPage::OnAddTriggered(const Uuid& id)
            {
                AddNewObject(id);
            }

            void ManifestWidgetPage::BuildAndConnectAddButton()
            {
                if (m_classTypeIds.size() == 0)
                {
                    ui->m_addButton->setText("No types for this group");
                }
                else if (m_classTypeIds.size() == 1)
                {
                    ui->m_addButton->setText(QString::fromLatin1("Add another %1").arg(ClassIdToName(m_classTypeIds[0])));
                    connect(ui->m_addButton, &QPushButton::clicked, this, &ManifestWidgetPage::OnAdd);
                }
                else
                {
                    ui->m_addButton->setText("Add another...");
                    QMenu* menu = new QMenu();
                    for (Uuid& id : m_classTypeIds)
                    {
                        menu->addAction(ClassIdToName(id), 
                            [this, id]()
                            {
                                OnAddTriggered(id); 
                            });
                    }
                    connect(menu, &QMenu::aboutToShow, 
                        [this, menu]() 
                        {
                            menu->setFixedWidth(ui->m_addButton->width());
                        });
                    ui->m_addButton->setMenu(menu);
                }
            }

            const char* ManifestWidgetPage::ClassIdToName(const Uuid& id) const
            {
                const SerializeContext::ClassData* classData = m_context->FindClassData(id);
                if (classData)
                {
                    if (classData->m_editData)
                    {
                        return classData->m_editData->m_name;
                    }
                    return classData->m_name;
                }
                return "<type not registered>";
            }

            void ManifestWidgetPage::AddNewObject(const Uuid& id)
            {
                AZ_TraceContext("Instance id", id);
                
                const SerializeContext::ClassData* classData = m_context->FindClassData(id);
                AZ_Assert(classData, "Type not registered.");
                if (classData)
                {
                    AZ_Assert(classData->m_factory, "Registered type has no factory to create a new instance with.");
                    if (classData->m_factory)
                    {
                        ManifestWidget* parent = ManifestWidget::FindRoot(this);
                        AZ_Assert(parent, "ManifestWidgetPage isn't docked in a ManifestWidget.");
                        if (!parent)
                        {
                            return;
                        }

                        AZStd::shared_ptr<Containers::Scene> scene = parent->GetScene();
                        Containers::SceneManifest& manifest = scene->GetManifest();

                        // TODO: GenerateUniqueRuleKey is a misleading name as it can be applied to any type of entry.
                        AZStd::string name = GenerateUniqueRuleKey(manifest.GetNameStorage(), scene->GetName());
                        AZ_TraceContext("New name", name);

                        void* rawInstance = classData->m_factory->Create(name.c_str());
                        AZ_Assert(rawInstance, "Serialization factory failed to construct new instance.");
                        if (!rawInstance)
                        {
                            return;
                        }

                        AZStd::shared_ptr<DataTypes::IManifestObject> instance(reinterpret_cast<DataTypes::IManifestObject*>(rawInstance));
                        EBUS_EVENT(Events::ManifestMetaInfoBus, InitializeObject, *scene, instance.get());
                        
                        if (manifest.AddEntry(AZStd::move(name), instance))
                        {
                            AddObject(instance);
                        }
                        else
                        {
                            AZ_Assert(false, "Unable to add new object to manifest.");
                        }

                        return;
                    }
                }
            }
        } // UI
    } // SceneAPI
} // AZ

#include <SceneWidgets/ManifestWidgetPage.moc>
