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

#include <ctime>
#include <QTableWidget.h>
#include <CommonWidgets/ui_ReportCard.h>
#include <AzToolsFramework/Debug/TraceContextStackInterface.h>
#include <SceneAPI/SceneUI/CommonWidgets/ReportCard.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace UI
        {
            ReportCard::ReportCard(Type type)
                : QWidget(nullptr)
                , ui(new Ui::ReportCard())
            {
                ui->setupUi(this);

                ui->m_messageLabel->hide();
                ui->m_timeStamp->hide();
                ui->m_contextArea->hide();

                const char* imageName = nullptr;
                switch (type)
                {
                case Type::Log:
                    ui->m_severity->hide();
                    break;
                case Type::Warning:
                    ui->m_severityLabel->setText("Warning");
                    ui->m_severityLabel->setStyleSheet("color : orange;");
                    imageName = ":/SceneUI/Common/WarningIcon.png";
                    break;
                case Type::Error:
                    ui->m_severityLabel->setText("Error");
                    ui->m_severityLabel->setStyleSheet("color : red;");
                    imageName = ":/SceneUI/Common/ErrorIcon.png";
                    break;
                case Type::Assert:
                    ui->m_severityLabel->setText("Assert");
                    ui->m_severityLabel->setStyleSheet("color : red;");
                    imageName = ":/SceneUI/Common/AssertIcon.png";
                    break;
                default:
                    ui->m_severityLabel->setText("<Unsupported type>");
                    ui->m_severityLabel->setStyleSheet("color : red;");
                    imageName = ":/SceneUI/Common/ErrorIcon.png";
                    break;
                }

                if (imageName)
                {
                    int width = ui->m_icon->width();
                    int height = ui->m_icon->height();
                    ui->m_icon->setPixmap(QPixmap(imageName).scaled(width, height, Qt::KeepAspectRatio));
                }
            }

            void ReportCard::SetMessage(const AZStd::string& message)
            {
                ui->m_messageLabel->setText(message.c_str());
                ui->m_messageLabel->show();
            }

            void ReportCard::SetTimeStamp(const AZStd::string& timeStamp)
            {
                ui->m_timeStamp->setText(timeStamp.c_str());
                ui->m_timeStamp->show();
            }

            void ReportCard::SetTimeStamp(const time_t& timeStamp)
            {
                struct tm timeInfo;
                localtime_s(&timeInfo, &timeStamp);

                char buffer[128];
                std::strftime(buffer, sizeof(buffer), "%A, %B %d, %Y %H:%M:%S", &timeInfo);

                ui->m_timeStamp->setText(buffer);
                ui->m_timeStamp->show();
            }

            void ReportCard::SetContext(const AzToolsFramework::Debug::TraceContextStackInterface& stack, bool includeUuids)
            {
                int index = 0;
                size_t count = stack.GetStackCount();
                for (int i = 0; i < count; ++i)
                {
                    QString valueText;
                    AzToolsFramework::Debug::TraceContextStackInterface::ContentType type = stack.GetType(i);
                    switch (type)
                    {
                    case AzToolsFramework::Debug::TraceContextStackInterface::ContentType::StringType:
                        valueText = stack.GetStringValue(i);
                        break;
                    case AzToolsFramework::Debug::TraceContextStackInterface::ContentType::BoolType:
                        valueText = stack.GetBoolValue(i) ? "True" : "False";
                        break;
                    case AzToolsFramework::Debug::TraceContextStackInterface::ContentType::IntType:
                        valueText = QString("%1").arg(stack.GetIntValue(i));
                        break;
                    case AzToolsFramework::Debug::TraceContextStackInterface::ContentType::UintType:
                        valueText = QString("%1").arg(stack.GetUIntValue(i));
                        break;
                    case AzToolsFramework::Debug::TraceContextStackInterface::ContentType::FloatType:
                        valueText = QString("%1").arg(stack.GetFloatValue(i));
                        break;
                    case AzToolsFramework::Debug::TraceContextStackInterface::ContentType::DoubleType:
                        valueText = QString("%1").arg(stack.GetDoubleValue(i));
                        break;
                    case AzToolsFramework::Debug::TraceContextStackInterface::ContentType::UuidType:
                        if (includeUuids)
                        {
                            valueText = stack.GetUuidValue(i).ToString<AZStd::string>().c_str();
                            break;
                        }
                        else
                        {
                            continue;
                        }
                    default:
                        valueText = QString("Unsupported type %1").arg(static_cast<int>(type));
                        break;
                    }

                    // Only configure and show the context area if there's an entry for it.
                    if (index == 0)
                    {
                        ui->m_context->setColumnStretch(0, 1);
                        ui->m_context->setColumnStretch(1, 4);

                        ui->m_contextArea->show();
                    }

                    QLabel* keyLabel = new QLabel(stack.GetKey(i));
                    keyLabel->setWordWrap(true);
                    ui->m_context->addWidget(keyLabel, index, 0);
                    
                    QLabel* valueLabel = new QLabel(valueText);
                    valueLabel->setWordWrap(true);
                    ui->m_context->addWidget(valueLabel, index, 1);
                    index++;
                }
            }

            void ReportCard::SetTopLineVisible(bool visible)
            {
                if (visible)
                {
                    ui->m_topLine->show();
                }
                else
                {
                    ui->m_topLine->hide();
                }
            }
        } // UI
    } // SceneAPI
} // AZ

#include <CommonWidgets/ReportCard.moc>
