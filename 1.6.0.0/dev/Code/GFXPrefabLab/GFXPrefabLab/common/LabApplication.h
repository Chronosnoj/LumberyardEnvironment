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

#include <AzCore/IO/FileIOEventBus.h>
#include <AzToolsFramework/Application/ToolsApplication.h>

class LabApplication
    : public AzToolsFramework::ToolsApplication
    , public AZ::IO::FileIOEventBus::Handler
{
public:
    using AzToolsFramework::ToolsApplication::Descriptor;

    LabApplication(int argc, char** argv)
        : m_argc(argc)
        , m_argv(argv)
    { }

    //////////////////////////////////////////////////////////////////////////
    // AzFramework::Application
    void StartCommon(AZ::Entity* systemEntity) override;
    void Stop();
    void ReflectSerialize() override;
    void RegisterCoreComponents() override;
    void AddSystemComponents(AZ::Entity* systemEntity) override;

    //////////////////////////////////////////////////////////////////////////
    // AZ::IO::FileEventBus
    void OnError(const AZ::IO::SystemFile* file, const char* fileName, int errorCode) override;
    //////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////
    // GPL Application
    virtual int Execute() = 0;

protected:
    int m_argc = 0;
    char** m_argv = nullptr;
};
