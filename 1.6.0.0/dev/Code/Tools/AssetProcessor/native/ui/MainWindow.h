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
#pragma once

#include <QMainWindow>

namespace Ui {
    class MainWindow;
}
class GUIApplicationManager;
class QListWidgetItem;
class RCJobSortFilterProxyModel;

class MainWindow
    : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(GUIApplicationManager* guiApplicationManager, QWidget* parent = 0);
    ~MainWindow();

public Q_SLOTS:
    void ToggleWindow();

private:
    Ui::MainWindow* ui;
    GUIApplicationManager* m_guiApplicationManager;
    RCJobSortFilterProxyModel* m_sortFilterProxy;

    void OnProxyIPEditingFinished();
    void OnProxyConnectChanged(int state);
    void OnPaneChanged(QListWidgetItem* current, QListWidgetItem* previous);
    void OnAddConnection(bool checked);
    void OnRemoveConnection(bool checked);
    void OnJobFilterClear(bool checked);
    void OnJobFilterRegExpChanged();
    void OnSupportClicked(bool checked);
};

