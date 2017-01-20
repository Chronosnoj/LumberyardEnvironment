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
#ifndef GUIAPPLICATIONMANAGER_H
#define GUIAPPLICATIONMANAGER_H

#include "native/utilities/BatchApplicationManager.h"
#include "native/utilities/assetUtilEBusHelper.h"
#include <QFileSystemWatcher>
#include <QMap>
#include <QAtomicInt>

class ConnectionManager;
class IniConfiguration;
class ApplicationServer;
class FileServer;
class ShaderCompilerManager;
class ShaderCompilerModel;

namespace AssetProcessor
{
    class AssetRequestHandler;
}

//! This class is the Application manager for the GUI Mode


class GUIApplicationManager
    : public BatchApplicationManager
    , public AssetProcessor::MessageInfoBus::Handler
{
    Q_OBJECT
public:
    explicit GUIApplicationManager(int argc, char** argv, QObject* parent = 0);
    virtual ~GUIApplicationManager();

    ApplicationManager::BeforeRunStatus BeforeRun() override;
    ConnectionManager* GetConnectionManager() const;
    IniConfiguration* GetIniConfiguration() const;
    ApplicationServer* GetApplicationServer() const;
    FileServer* GetFileServer() const;
    ShaderCompilerManager* GetShaderCompilerManager() const;
    ShaderCompilerModel* GetShaderCompilerModel() const;

    bool Run() override;
	////////////////////////////////////////////////////
    ///MessageInfoBus::Listener interface///////////////
    void NegotiationFailed() override;
    void ProxyConnectFailed() override;
	///////////////////////////////////////////////////
protected:
    bool Activate() override;
    void CreateQtApplication() override;
    void InitConnectionManager();
    void DestroyConnectionManager();
    void InitIniConfiguration();
    void DestroyIniConfiguration();
    bool InitApplicationServer();
    void DestroyApplicationServer();
    void InitFileServer();
    void DestroyFileServer();
    void InitShaderCompilerManager();
    void DestroyShaderCompilerManager();
    void InitShaderCompilerModel();
    void DestroyShaderCompilerModel();
    void InitAssetRequestHandler();
    void Destroy() override;

Q_SIGNALS:
    void ToggleDisplay();

protected Q_SLOTS:
    void FileChanged(QString path);
    void ShowMessageBox(QString title, QString msg);

private:
    bool RestartRequired();
    ConnectionManager* m_connectionManager = nullptr;
    IniConfiguration* m_iniConfiguration = nullptr;
    ApplicationServer* m_applicationServer = nullptr;
    FileServer* m_fileServer = nullptr;
    ShaderCompilerManager* m_shaderCompilerManager = nullptr;
    ShaderCompilerModel* m_shaderCompilerModel = nullptr;
    AssetProcessor::AssetRequestHandler* m_assetRequestHandler = nullptr;
    QFileSystemWatcher m_fileWatcher;
    bool m_messageBoxIsVisible = false;

    QMap<int, qintptr> m_queuedConnections;
    QAtomicInt m_connectionsAwaitingAssetCatalogSave = 0;
};


#endif//GUIAPPLICATIONMANAGER_H
