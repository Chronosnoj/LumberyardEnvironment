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
#include "gui/LabGUIApplication.h"
#include "gui/QtComponent.h"

#include <QtWidgets/QApplication>

#include <AzCore/Module/Module.h>

void LabGUIApplication::StartCommon(AZ::Entity* systemEntity)
{
    LabApplication::StartCommon(systemEntity);

    QApplication::setApplicationName("Prefab Lab");
    QApplication::setApplicationVersion("1.0");

    m_mainWindow = new MainWindow();
    m_mainWindow->setWindowTitle("Prefab Lab");
}

void LabGUIApplication::Stop()
{
    delete m_mainWindow;
    m_mainWindow = nullptr;

    LabApplication::Stop();
}

void LabGUIApplication::RegisterCoreComponents()
{
    LabApplication::RegisterCoreComponents();

    RegisterComponentDescriptor(AzToolsFramework::Components::QtComponent::CreateDescriptor());
}

void LabGUIApplication::AddSystemComponents(AZ::Entity* systemEntity)
{
    LabApplication::AddSystemComponents(systemEntity);

    EnsureComponentAdded<AzToolsFramework::Components::QtComponent>(systemEntity);

    auto qt = systemEntity->FindComponent<AzToolsFramework::Components::QtComponent>();
    qt->SetArgs(m_argc, m_argv);
}

int LabGUIApplication::Execute()
{
    m_mainWindow->show();

    int returnCode = -1;
    EBUS_EVENT_RESULT(returnCode, AzToolsFramework::Components::QtRequestBus, Exec);

    m_mainWindow->hide();

    return returnCode;
}
