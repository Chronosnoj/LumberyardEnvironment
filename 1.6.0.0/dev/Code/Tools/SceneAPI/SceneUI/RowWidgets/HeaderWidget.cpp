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
#include <RowWidgets/ui_HeaderWidget.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>
#include <AzToolsFramework/Debug/TraceContext.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/SceneManifest.h>
#include <SceneAPI/SceneCore/Utilities/Reporting.h>
#include <SceneAPI/SceneCore/Events/ManifestMetaInfoBus.h>
#include <SceneAPI/SceneUI/RowWidgets/HeaderWidget.h>
#include <SceneAPI/SceneUI/RowWidgets/ManifestVectorWidget.h>
#include <SceneAPI/SceneUI/SceneWidgets/ManifestWidget.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace UI
        {
            AZ_CLASS_ALLOCATOR_IMPL(HeaderWidget, SystemAllocator, 0)

            HeaderWidget::HeaderWidget(QWidget* parent)
                : QWidget(parent)
                , ui(new Ui::HeaderWidget())
                , m_target(nullptr)
                , m_nameIsEditable(false)
                , m_sceneManifest(nullptr)
            {
                ui->setupUi(this);

                ui->m_icon->hide();
                
                ui->m_nameLabel->installEventFilter(this);
                ui->m_nameEdit->installEventFilter(this);
                connect(ui->m_nameEdit, &QLineEdit::returnPressed, this, &HeaderWidget::NameChangeAccepted);
                ui->m_nameStack->setCurrentIndex(NameStack::Label);
                
                ui->m_deleteButton->setIcon(QIcon(":/PropertyEditor/Resources/cross-small.png"));
                connect(ui->m_deleteButton, &QToolButton::clicked, this, &HeaderWidget::DeleteObject);
                ui->m_deleteButton->hide();

                ManifestWidget* root = ManifestWidget::FindRoot(this);
                AZ_Assert(root, "HeaderWidget is not a child of the ManifestWidget");
                if (root)
                {
                    m_sceneManifest = &root->GetScene()->GetManifest();
                }

            }

            void HeaderWidget::SetManifestObject(const DataTypes::IManifestObject* target)
            {
                m_target = target;

                UpdateNameEditable();
                UpdateDeletable();

                SetIcon(target);
            }

            const DataTypes::IManifestObject* HeaderWidget::GetManifestObject() const
            {
                return m_target;
            }

            bool HeaderWidget::eventFilter(QObject* object, QEvent* event)
            {
                if (m_nameIsEditable && object == ui->m_nameEdit && event->type() == QEvent::FocusOut)
                {
                    NameChangeAccepted();
                }
                return QWidget::eventFilter(object, event);
            }

            void HeaderWidget::NameChangeAccepted()
            {
                if (m_nameIsEditable)
                {
                    AZ_TraceContext("Old name", m_objectName);
                    AZStd::string newName = ui->m_nameEdit->text().toStdString().c_str();
                    AZ_TraceContext("New name", newName);
                    if (newName == m_objectName)
                    {
                        return;
                    }

                    if (m_sceneManifest && !m_sceneManifest->HasEntry(newName))
                    {
                        m_sceneManifest->RenameEntry(m_objectName, newName);
                        m_objectName = AZStd::move(newName);
                    }
                    else
                    {
                        ui->m_nameEdit->setText(m_objectName.c_str());
                        AZ_TracePrintf(Utilities::ErrorWindow, "New name for manifest object is invalid, please choose another name.");
                    }
                }
            }

            void HeaderWidget::DeleteObject()
            {
                AZ_TraceContext("Delete target", m_objectName);

                if (m_sceneManifest)
                {
                    Containers::SceneManifest::Index index = m_sceneManifest->FindIndex(m_objectName);
                    if (index != Containers::SceneManifest::s_invalidIndex)
                    {
                        AZ_TraceContext("Manifest index", index);
                        ManifestWidget* root = ManifestWidget::FindRoot(this);
                        // The manifest object could be a root element at the manifest page level so it needs to be
                        //      removed from there as well in that case.
                        if (root->RemoveObject(m_sceneManifest->GetValue(index)) && m_sceneManifest->RemoveEntry(m_objectName))
                        {
                            m_target = nullptr;
                            return;
                        }
                        else
                        {
                            AZ_TracePrintf(Utilities::LogWindow, "Unable to delete manifest object from manifest.");
                        }
                    }
                }

                QObject* widget = this->parent();
                while (widget != nullptr)
                {
                    ManifestVectorWidget* manifestVectorWidget = qobject_cast<ManifestVectorWidget*>(widget);
                    if (manifestVectorWidget)
                    {
                        if (manifestVectorWidget->RemoveManifestObject(m_target))
                        {
                            m_target = nullptr;
                        }
                        else
                        {
                            AZ_TracePrintf(Utilities::WarningWindow, "Parent collection did not contain this ManifestObject");
                        }

                        return;
                    }
                    widget = widget->parent();
                }

                AZ_TracePrintf(Utilities::ErrorWindow, "No parent valid parent collection found.");
            }

            void HeaderWidget::UpdateNameEditable()
            {
                if (m_sceneManifest)
                {
                    Containers::SceneManifest::Index index = m_sceneManifest->FindIndex(m_target);
                    if (index != Containers::SceneManifest::s_invalidIndex)
                    {
                        m_objectName = m_sceneManifest->GetName(index);

                        ui->m_nameEdit->setText(m_objectName.c_str());
                        ui->m_nameStack->setCurrentIndex(NameStack::EditField);
                        m_nameIsEditable = true;
                        return;
                    }
                }

                m_objectName = GetSerializedName(m_target);
                ui->m_nameLabel->setText(m_objectName.c_str());
                ui->m_nameStack->setCurrentIndex(NameStack::Label);
                m_nameIsEditable = false;
            }

            void HeaderWidget::UpdateDeletable()
            {
                ui->m_deleteButton->hide();

                if (m_sceneManifest)
                {
                    Containers::SceneManifest::Index index = m_sceneManifest->FindIndex(m_target);
                    if (index != Containers::SceneManifest::s_invalidIndex)
                    {
                        ui->m_deleteButton->show();
                        return;
                    }
                }

                QObject* widget = this->parent();
                while(widget != nullptr)
                {
                    ManifestVectorWidget* manifestVectorWidget = qobject_cast<ManifestVectorWidget*>(widget);
                    if (manifestVectorWidget && manifestVectorWidget->ContainsManifestObject(m_target))
                    {
                        ui->m_deleteButton->show();
                        break;
                    }
                    widget = widget->parent();
                }
            }

            const char* HeaderWidget::GetSerializedName(const DataTypes::IManifestObject* target) const
            {
                SerializeContext* context = nullptr;
                EBUS_EVENT_RESULT(context, ComponentApplicationBus, GetSerializeContext);
                if (context)
                {
                    const SerializeContext::ClassData* classData = context->FindClassData(target->RTTI_GetType());
                    if (classData)
                    {
                        if (classData->m_editData)
                        {
                            return classData->m_editData->m_name;
                        }
                        return classData->m_name;
                    }
                }
                return "<type not registered>";
            }

            void HeaderWidget::SetIcon(const DataTypes::IManifestObject* target)
            {
                AZStd::string iconPath;
                EBUS_EVENT(Events::ManifestMetaInfoBus, GetIconPath, iconPath, target);
                if (iconPath.empty())
                {
                    ui->m_icon->hide();
                }
                else
                {
                    ui->m_icon->setPixmap(QPixmap(iconPath.c_str()));
                    ui->m_icon->show();
                }
            }
        } // UI
    } // SceneAPI
} // AZ

#include <RowWidgets/HeaderWidget.moc>