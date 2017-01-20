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
#include "MainWindow.h"
#include <native/ui/ui_mainwindow.h>
#include "../utilities/GUIApplicationManager.h"
#include "../utilities/ApplicationServer.h"
#include "../connection/connectionManager.h"
#include "../resourcecompiler/rccontroller.h"
#include "../resourcecompiler/RCJobSortFilterProxyModel.h"
#include "../shadercompiler/shadercompilerModel.h"
#include <AzToolsFramework/UI/Logging/GenericLogPanel.h>
#include <AzToolsFramework/Asset/AssetProcessorMessages.h>
#include <QDialog>
#include <qtreeview.h>
#include <qheaderview.h>
#include <qaction.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qlistwidget.h>
#include <qstackedwidget.h>
#include <qpushbutton.h>
#include <QDesktopServices.h>
#include <qurl.h>
#include "native/utilities/IniConfiguration.h"


MainWindow::MainWindow(GUIApplicationManager* guiApplicationManager, QWidget* parent)
    : QMainWindow(parent)
    , m_guiApplicationManager(guiApplicationManager)
    , m_sortFilterProxy(new RCJobSortFilterProxyModel(this))
    , ui(new Ui::MainWindow)
{
    using namespace AssetProcessor;

    ui->setupUi(this);
    int listeningPort = guiApplicationManager->GetApplicationServer()->getServerListeningPort();
    ui->port->setText(QString::number(listeningPort));
    ui->proxyIP->setPlaceholderText(QString("localhost:%1").arg(listeningPort));
    ui->proxyIP->setText(guiApplicationManager->GetIniConfiguration()->proxyInformation());
    ui->proxyEnable->setChecked(guiApplicationManager->GetConnectionManager()->ProxyConnect());

    ui->gameProject->setText(guiApplicationManager->GetGameName());
    ui->gameRoot->setText(guiApplicationManager->GetSystemRoot().absolutePath());

    ui->buttonList->setCurrentRow(0);

    connect(ui->proxyIP, &QLineEdit::editingFinished, this, &MainWindow::OnProxyIPEditingFinished);
    connect(ui->proxyEnable, &QCheckBox::stateChanged, this, &MainWindow::OnProxyConnectChanged);
    connect(ui->buttonList, &QListWidget::currentItemChanged, this, &MainWindow::OnPaneChanged);
    connect(ui->supportButton, &QPushButton::clicked, this, &MainWindow::OnSupportClicked);

    connect(guiApplicationManager->GetConnectionManager(), &ConnectionManager::ProxyConnectChanged, this, 
    [this] (bool proxyMode)
    {
        if (static_cast<bool>(ui->proxyEnable->checkState()) != proxyMode)
        {
            ui->proxyEnable->setChecked(proxyMode);
        }
    });


    //Connection view
    ui->connectionTreeView->setModel(guiApplicationManager->GetConnectionManager());
    ui->connectionTreeView->setEditTriggers(QAbstractItemView::CurrentChanged);
    ui->connectionTreeView->header()->resizeSection(ConnectionManager::StatusColumn, 100);
    ui->connectionTreeView->header()->resizeSection(ConnectionManager::IdColumn, 100);
    ui->connectionTreeView->header()->resizeSection(ConnectionManager::IpColumn, 75);
    ui->connectionTreeView->header()->resizeSection(ConnectionManager::PortColumn, 50);
    ui->connectionTreeView->header()->resizeSection(ConnectionManager::PlatformColumn, 60);
    ui->connectionTreeView->header()->resizeSection(ConnectionManager::AutoConnectColumn, 20);
    ui->connectionTreeView->header()->setSectionResizeMode(ConnectionManager::PlatformColumn, QHeaderView::Stretch);
    ui->connectionTreeView->header()->setStretchLastSection(false);

    connect(ui->addConnectionButton, &QPushButton::clicked, this, &MainWindow::OnAddConnection);
    connect(ui->removeConnectionButton, &QPushButton::clicked, this, &MainWindow::OnRemoveConnection);

#if !defined(FORCE_PROXY_MODE)
    //Job view
    m_sortFilterProxy->setSourceModel(guiApplicationManager->GetRCController()->getQueueModel());
    m_sortFilterProxy->setDynamicSortFilter(true);
    m_sortFilterProxy->setFilterKeyColumn(2);

    ui->jobTreeView->setModel(m_sortFilterProxy);
    ui->jobTreeView->setSortingEnabled(true);
    ui->jobTreeView->header()->resizeSection(RCJobListModel::ColumnState, 80);
    ui->jobTreeView->header()->resizeSection(RCJobListModel::ColumnJobId, 40);
    ui->jobTreeView->header()->resizeSection(RCJobListModel::ColumnCommand, 220);
    ui->jobTreeView->header()->resizeSection(RCJobListModel::ColumnCompleted, 80);
    ui->jobTreeView->header()->resizeSection(RCJobListModel::ColumnPlatform, 60);
    ui->jobTreeView->header()->setSectionResizeMode(RCJobListModel::ColumnCommand, QHeaderView::Stretch);
    ui->jobTreeView->header()->setStretchLastSection(false);

    connect(ui->jobTreeView->header(), &QHeaderView::sortIndicatorChanged, m_sortFilterProxy, &RCJobSortFilterProxyModel::sort);
    connect(ui->jobTreeView, &QAbstractItemView::doubleClicked, [this, guiApplicationManager](const QModelIndex &index)
    {
        
        // we have to deliver this using a safe cross-thread approach.
        // the Asset Processor Manager always runs on a seperate thread to us.
        using AssetJobLogRequest = AzToolsFramework::AssetSystem::AssetJobLogRequest;
        using AssetJobLogResponse = AzToolsFramework::AssetSystem::AssetJobLogResponse;

        AssetJobLogRequest request;
        AssetJobLogResponse response;
        request.m_jobId = m_sortFilterProxy->data(index, RCJobListModel::jobIDRole).toLongLong();

        QMetaObject::invokeMethod(guiApplicationManager->GetAssetProcessorManager(), "ProcessGetAssetJobLogRequest",
            Qt::BlockingQueuedConnection,
            Q_ARG(const AssetJobLogRequest&, request),
            Q_ARG(AssetJobLogResponse&, response));

        // read the log file and show it to the user
        
        QDialog logDialog;
        logDialog.setMinimumSize(1024, 400);
        QHBoxLayout* pLayout = new QHBoxLayout(&logDialog);
        logDialog.setLayout(pLayout);
        AzToolsFramework::LogPanel::GenericLogPanel* logPanel = new AzToolsFramework::LogPanel::GenericLogPanel(&logDialog);
        logDialog.layout()->addWidget(logPanel);
        logPanel->ParseData(response.m_jobLog.c_str(), response.m_jobLog.size());

        auto tabsResetFunction = [logPanel]() -> void
        {
            logPanel->AddLogTab(AzToolsFramework::LogPanel::TabSettings("All output", "", ""));
            logPanel->AddLogTab(AzToolsFramework::LogPanel::TabSettings("Warnings/Errors Only", "", "", false, true, true, false));
        };

        tabsResetFunction();

        connect(logPanel, &AzToolsFramework::LogPanel::BaseLogPanel::TabsReset, this, tabsResetFunction);
        logDialog.adjustSize();
        logDialog.exec();
       
    });
    connect(ui->jobFilterLineEdit, &QLineEdit::textChanged, this, &MainWindow::OnJobFilterRegExpChanged);
    connect(ui->jobFilterClearButton, &QPushButton::clicked, this, &MainWindow::OnJobFilterClear);

    //Shader view
    ui->shaderTreeView->setModel(guiApplicationManager->GetShaderCompilerModel());
    ui->shaderTreeView->header()->resizeSection(ShaderCompilerModel::ColumnTimeStamp, 80);
    ui->shaderTreeView->header()->resizeSection(ShaderCompilerModel::ColumnServer, 40);
    ui->shaderTreeView->header()->resizeSection(ShaderCompilerModel::ColumnError, 220);
    ui->shaderTreeView->header()->setSectionResizeMode(ShaderCompilerModel::ColumnError, QHeaderView::Stretch);
    ui->shaderTreeView->header()->setStretchLastSection(false);
#endif //FORCE_PROXY_MODE
}

void MainWindow::OnSupportClicked(bool checked)
{
    QDesktopServices::openUrl(QString("https://docs.aws.amazon.com/lumberyard/latest/userguide/asset-pipeline-processor.html"));
}

void MainWindow::OnJobFilterClear(bool checked)
{
    ui->jobFilterLineEdit->setText(QString());
}

void MainWindow::OnJobFilterRegExpChanged()
{
    QRegExp regExp(ui->jobFilterLineEdit->text(), Qt::CaseInsensitive, QRegExp::PatternSyntax::RegExp);
    m_sortFilterProxy->setFilterRegExp(regExp);
}

void MainWindow::OnAddConnection(bool checked)
{
    m_guiApplicationManager->GetConnectionManager()->addConnection();
}


void MainWindow::OnRemoveConnection(bool checked)
{
    ConnectionManager* manager = m_guiApplicationManager->GetConnectionManager();

    QModelIndexList list = ui->connectionTreeView->selectionModel()->selectedIndexes();
    for (QModelIndex index: list)
    {
        manager->removeConnection(index);
    }
}



void MainWindow::OnPaneChanged(QListWidgetItem* current, QListWidgetItem* previous)
{
    if (!current)
    {
        current = previous;
    }

    ui->dialogStack->setCurrentIndex(ui->buttonList->row(current));
}


MainWindow::~MainWindow()
{
    m_guiApplicationManager = nullptr;
    delete ui;
}


void MainWindow::OnProxyIPEditingFinished()
{
    if (m_guiApplicationManager && m_guiApplicationManager->GetIniConfiguration())
    {
        m_guiApplicationManager->GetIniConfiguration()->SetProxyInformation(ui->proxyIP->text());
    }
}


void MainWindow::OnProxyConnectChanged(int state)
{
    if (m_guiApplicationManager)
    {
        m_guiApplicationManager->GetConnectionManager()->SetProxyConnect(state == 2);
    }
}

void MainWindow::ToggleWindow()
{
    if (isVisible())
    {
        hide();
    }
    else
    {
        show();
    }
}


#include <native/ui/mainwindow.moc>
