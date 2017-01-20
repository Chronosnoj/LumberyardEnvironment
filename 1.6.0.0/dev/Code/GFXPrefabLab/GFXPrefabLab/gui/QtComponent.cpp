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
#include "gui/QtComponent.h"

#include <QtCore/QDir>
#include <QtGui/QPalette>
#include <QtWidgets/QApplication>

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/Serialization/SerializeContext.h>

#define STYLE_SHEET_VARIABLES_PATH_DARK "/Editor/Styles/EditorStylesheetVariables_Dark.json"
#define STYLE_SHEET_VARIABLES_PATH_LIGHT "/Editor/Styles/EditorStylesheetVariables_Light.json"
#define STYLE_SHEET_PATH "/Editor/Styles/EditorStylesheet.qss"
#define STYLE_SHEET_LINK_COLOR_VAR "LinkColor"

namespace AzToolsFramework
{
    namespace Components
    {
        void QtComponent::Reflect(AZ::ReflectContext* context)
        {
            if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serializeContext
                    ->Class<QtComponent, AZ::Component>()
                        ->Version(1)
                        ->SerializerForEmptyClass()
                    ;
            }
        }

        void QtComponent::SetArgs(int argc, char** argv)
        {
            m_argc = argc;
            m_argv = argv;
        }

        void QtComponent::Activate()
        {
            QApplication::addLibraryPath(AppendToCWD("qtlibs/plugins"));

            m_qApp = new QApplication(m_argc, m_argv);

            RefreshStylesheet();

            QtRequestBus::Handler::BusConnect();
        }

        void QtComponent::Deactivate()
        {
            QtRequestBus::Handler::BusDisconnect();

            delete m_qApp;
            m_qApp = nullptr;
        }

        int QtComponent::Exec()
        {
            AZ_Assert(m_qApp, "QtComponent is unactivated.");

            return m_qApp->exec();
        }

        QString QtComponent::AppendToCWD(const char subpath[])
        {
            QString root = "";
            EBUS_EVENT_RESULT(root, AZ::ComponentApplicationBus, GetAppRoot);
            QDir folder = root + QDir::separator() + "Bin64" + QDir::separator() + subpath;
            return folder.absolutePath();
        }

        AZ::Outcome<void, AZStd::string> QtComponent::RefreshStylesheet()
        {
            AZ_Assert(m_qApp, "QApp not initialized! Activate the component!");

            QString appRoot = nullptr;
            EBUS_EVENT_RESULT(appRoot, AZ::ComponentApplicationBus, GetAppRoot);

            QFile styleSheetVariablesFile;

            styleSheetVariablesFile.setFileName(appRoot + STYLE_SHEET_VARIABLES_PATH_DARK);

            if (styleSheetVariablesFile.open(QFile::ReadOnly))
            {
                m_stylesheetPreprocessor.ReadVariables(styleSheetVariablesFile.readAll());
            }
            else
            {
                return AZ::Failure(AZStd::string::format("Failed to open file %s.", STYLE_SHEET_VARIABLES_PATH_DARK));
            }

            QFile styleSheetFile(appRoot + STYLE_SHEET_PATH);
            if (styleSheetFile.open(QFile::ReadOnly))
            {
                QString processedStyle = m_stylesheetPreprocessor.ProcessStyleSheet(styleSheetFile.readAll());
                m_qApp->setStyleSheet(processedStyle);
            }
            else
            {
                return AZ::Failure(AZStd::string::format("Failed to open file %s.", STYLE_SHEET_PATH));
            }

            QPalette newPal = m_qApp->palette();
            newPal.setColor(QPalette::Link, m_stylesheetPreprocessor.GetColorByName(STYLE_SHEET_LINK_COLOR_VAR));
            m_qApp->setPalette(newPal);

            return AZ::Success();
        }
    }
}
