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
#include "StdAfx.h"
#include "ExportSettings.h"

#include <QFile>
#include <QTextStream>
#include "ICrySourceControl.h"

namespace
{
    QString getExportSettingsFileName(const char* filename)
    {
        QString exportSettingsFilename = filename;
        exportSettingsFilename.append(".exportsettings");
        return exportSettingsFilename;
    }
}

bool ImageExportSettings::LoadSettings(const char* filename, string& settings)
{
    QFile exportSettingsFile(getExportSettingsFileName(filename));
    if (!exportSettingsFile.open(QFile::ReadOnly | QFile::Text))
    {
        return false;
    }

    QTextStream inStream(&exportSettingsFile);
    QString inSettings = inStream.readAll();
    settings = inSettings.toStdString().c_str();

    exportSettingsFile.close();
    return true;
}

bool ImageExportSettings::SaveSettings(const char* filename, const string& settings)
{
    // If no old settings and new settings are empty, don't write them out
    string oldSettings;
    if (!LoadSettings(filename, oldSettings) && settings.empty())
    {
        return true;
    }

    //check if the settings changed
    bool bSettingsChanged = oldSettings.compare(settings) != 0;

    QString exportSettingFileName = getExportSettingsFileName(filename);

    //////////////////////////////////////////////////////////////////////////
    //if the source file is not in source control do not do anything
    //if the source file is in source control and the export file is not add it
    //if they are both in source control make the export settings file editable
    //be sure not to make it writable if the contents have not changed but still
    //add it if the source is in source control and it isn't
    if (CrySourceControl())
    {
        //see if the source and export files are in source control already
        bool bSourceFileinSourceControl = CrySourceControl()->IsUnderSourceControl(filename);
        bool bExportFileinSourceControl = CrySourceControl()->IsUnderSourceControl(exportSettingFileName.toUtf8().data());

        //if the source file is in source control and the export settings file is not add it
        if (bSourceFileinSourceControl && !bExportFileinSourceControl)
        {
            //add it
            CrySourceControl()->Add(exportSettingFileName.toUtf8().data());

            //if the settings changed if not return
            if (!bSettingsChanged)
            {
                return true;
            }
        }//else if both are in source control Make it writable only if changed
        else if (bSourceFileinSourceControl && bExportFileinSourceControl)
        {
            //if the settings changed if not return
            if (!bSettingsChanged)
            {
                return true;
            }

            //make it writable
            CrySourceControl()->MakeWritable(exportSettingFileName.toUtf8().data());
        }
    }
    //////////////////////////////////////////////////////////////////////////

    //if the settings changed if not return
    if (!bSettingsChanged)
    {
        return true;
    }

    //write the settings to the export settings file
    QFile exportSettingsFile(exportSettingFileName);
    if (!exportSettingsFile.open(QFile::WriteOnly | QFile::Text))
    {
        // Unable to open for write
        return false;
    }

    QTextStream outStream(&exportSettingsFile);
    outStream << settings.c_str();

    exportSettingsFile.close();
    return true;
}
