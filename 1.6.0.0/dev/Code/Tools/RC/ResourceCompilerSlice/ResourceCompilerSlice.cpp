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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#include "stdafx.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Must be included only once in DLL module.
#include <platform_implRC.h>

#include <IConvertor.h>

#include <AzCore/Asset/AssetDatabase.h>
#include <AzCore/Asset/AssetDatabaseComponent.h>
#include <AzCore/std/string/osstring.h>
#include <AzCore/Prefab/PrefabAssetHandler.h>
#include <AzCore/Prefab/PrefabComponent.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Memory/PoolAllocator.h>
#include <AzCore/Memory/MemoryComponent.h>
#include <AzCore/Memory/AllocationRecords.h>
#include <AzCore/Memory/AllocatorManager.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Script/ScriptSystemComponent.h>
#include <AzCore/Debug/TraceMessageBus.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzFramework/Asset/AssetCatalogComponent.h>
#include <AzFramework/Asset/AssetCatalogBus.h>
#include <AzToolsFramework/Application/ToolsApplication.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

static HMODULE g_hInst;

/**
 * Trace hook for asserts/errors/warning.
 * This allows us to report to the RC log, as well as to capture non-fatal errors and exit gracefully.
 */
class TraceDrillerHook
    : public AZ::Debug::TraceMessageBus::Handler
{
public:
    TraceDrillerHook()
    {
        BusConnect();
    }
    virtual ~TraceDrillerHook()
    {
    }
    bool OnAssert(const char* message) override
    {
        RCLogWarning("Assert failed: %s", message);
        ++s_errorsOccurred;
        return true;
    }
    bool OnError(const char* /*window*/, const char* message) override
    {
        RCLogWarning("Error: %s", message);
        ++s_errorsOccurred;
        return true;
    }
    bool OnWarning(const char* /*window*/, const char* message) override
    {
        RCLogWarning("Warning: %s", message);
        return true;
    }

    static size_t s_errorsOccurred;
};

size_t TraceDrillerHook::s_errorsOccurred = 0;

/**
 * Compiler for slice assets.
 */
class SliceCompiler
    : public ICompiler
{
public:

    /**
     * AZToolsFramework application override for RC.
     * Adds the minimal set of system components required for slice loading & conversion.
     */
    class RCToolApplication
        : public AzToolsFramework::ToolsApplication
    {
    public:
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            AZ::ComponentTypeList components = AZ::ComponentApplication::GetRequiredSystemComponents();

            components.insert(components.end(), {
                azrtti_typeid<AZ::MemoryComponent>(),
                azrtti_typeid<AZ::AssetDatabaseComponent>(),
                azrtti_typeid<AzFramework::AssetCatalogComponent>(),
                azrtti_typeid<AZ::ScriptSystemComponent>(),
            });

            return components;
        }

        void LoadDynamicModules() override
        {
            // Add .. to the path, so that modules are read out of BinX rather than BinX/rc
            azstrncat(m_exeDirectory, "../", 4);

            ToolsApplication::LoadDynamicModules();

            // Put a 0 after rc/ (removing ../)
            m_exeDirectory[strlen(m_exeDirectory) - 3] = 0;
        }
    };

public:
    SliceCompiler() = default;
    virtual ~SliceCompiler() = default;

    void Release() override                                         { delete this; }
    void BeginProcessing(const IConfig* config) override            {}
    void EndProcessing() override                                   {}

    virtual bool Process() override;

    IConvertContext* GetConvertContext() override { return &m_CC; }

private:

    bool ProcessAsset(RCToolApplication& app);

    ConvertContext m_CC;
};

/**
 * Registered converter for slice assets.
 */
class SliceConverter
    : public IConvertor
{
public:
    SliceConverter() = default;
    virtual ~SliceConverter() = default;

    void Release() override                         { delete this; }
    void DeInit() override                          {}
    ICompiler* CreateCompiler() override            { return new SliceCompiler(); }
    bool SupportsMultithreading() const override    { return false; }
    const char* GetExt(int index) const override;

private:
};

//=========================================================================
// SliceConverter::GetExt
//=========================================================================
const char* SliceConverter::GetExt(int index) const
{
    return (index == 0) ? "slice" : nullptr;
}

//=========================================================================
// SliceCompiler::Process
//=========================================================================
bool SliceCompiler::Process()
{
    bool result = false;

    TraceDrillerHook traceHook;

    AZ::IO::FileIOBase* prevIO = AZ::IO::FileIOBase::GetInstance();
    AZ::IO::FileIOBase::SetInstance(nullptr);
    AZ::IO::FileIOBase::SetInstance(m_CC.pRC->GetSystemEnvironment()->pFileIO);

    RCToolApplication app;
    AZ::ComponentApplication::Descriptor appDescriptor;
    appDescriptor.m_recordingMode = AZ::Debug::AllocationRecords::Mode::RECORD_FULL;

    const char* gameFolder = m_CC.pRC->GetSystemEnvironment()->pFileIO->GetAlias("@devassets@");
    char configPath[2048];
    sprintf_s(configPath, "%s/config/editor.xml", gameFolder);
    app.Start(configPath);

    // Allow RC jobs to leak, should not fail the job
    AZ::AllocatorManager::Instance().SetAllocatorLeaking(true);

    EBUS_EVENT(AZ::Data::AssetCatalogRequestBus, LoadCatalog, AZStd::string::format("@assets@/AssetCatalog.xml").c_str());

    result = ProcessAsset(app);

    app.Stop();

    AZ::IO::FileIOBase::SetInstance(nullptr);
    AZ::IO::FileIOBase::SetInstance(prevIO);

    return result;
}

//=========================================================================
// SliceCompiler::ProcessAsset
//=========================================================================
bool SliceCompiler::ProcessAsset(RCToolApplication& app)
{
    // Source slice is copied directly to the cache as-is, for use in-editor.
    // If slice is tagged as dynamic, we also conduct a runtime export, which converts editor->runtime components and generates a new .dynamicslice file.

    AZStd::string sourcePath;
    AZStd::string outputPath;
    AzFramework::StringFunc::Path::Join(m_CC.sourceFolder.c_str(), m_CC.sourceFileNameOnly.c_str(), sourcePath, true, true, true);
    AzFramework::StringFunc::Path::Join(m_CC.GetOutputFolder().c_str(), m_CC.sourceFileNameOnly.c_str(), outputPath, true, true, true);

    RCLog("Processing slice \"%s\" with output path \"%s\".", sourcePath.c_str(), outputPath.c_str());

    // Serialize in the source slice to determine if we need to generate a .dynamicslice.
    AZStd::unique_ptr<AZ::Entity> sourceSliceEntity(
        AZ::Utils::LoadObjectFromFile<AZ::Entity>(sourcePath, app.GetSerializeContext(),
            AZ::ObjectStream::FilterDescriptor(AZ::ObjectStream::AssetFilterSlicesOnly)));

    // If the slice is designated as dynamic, generate the dynamic slice (.dynamicslice).
    AZ::PrefabComponent* sourcePrefab = (sourceSliceEntity) ? sourceSliceEntity->FindComponent<AZ::PrefabComponent>() : nullptr;
    if (sourcePrefab && sourcePrefab->IsDynamic())
    {
        // Fail gracefully if any errors occurred while serializing in the editor slice.
        // i.e. missing assets or serialization errors.
        if (TraceDrillerHook::s_errorsOccurred > 0)
        {
            RCLogError("Exporting of .dynamicslice for \"%s\" failed due to errors loading editor slice. See ap_gui.log for details.", sourcePath.c_str());
            return true;
        }

        AZStd::string dynamicSliceOutputPath = outputPath;
        AzFramework::StringFunc::Path::ReplaceExtension(dynamicSliceOutputPath, "dynamicslice");

        AZ::PrefabComponent::EntityList sourceEntities;
        sourcePrefab->GetEntities(sourceEntities);
        AZ::Entity exportPrefabEntity;
        AZ::PrefabComponent* exportPrefab = exportPrefabEntity.CreateComponent<AZ::PrefabComponent>();

        // For export, components can assume they're initialized, but not activated.
        for (AZ::Entity* sourceEntity : sourceEntities)
        {
            if (sourceEntity->GetState() == AZ::Entity::ES_CONSTRUCTED)
            {
                sourceEntity->Init();
            }
        }

        // Prepare entities for export. This involves invoking BuildGameEntity on source
        // entity's components, targeting a separate entity for export.
        bool entityExportSuccessful = true;
        for (AZ::Entity* sourceEntity : sourceEntities)
        {
            AZ::Entity* exportEntity = aznew AZ::Entity(sourceEntity->GetName().c_str());
            exportEntity->SetId(sourceEntity->GetId());

            const AZ::Entity::ComponentArrayType& editorComponents = sourceEntity->GetComponents();
            for (AZ::Component* component : editorComponents)
            {
                auto* asEditorComponent =
                    azrtti_cast<AzToolsFramework::Components::EditorComponentBase*>(component);

                if (asEditorComponent)
                {
                    asEditorComponent->BuildGameEntity(exportEntity);
                }
            }

            // Pre-sort prior to exporting so it isn't required at instantiation time.
            const AZ::Entity::DependencySortResult sortResult = exportEntity->EvaluateDependencies();
            if (AZ::Entity::DSR_OK != sortResult)
            {
                const char* sortResultError = "";
                switch (sortResult)
                {
                case AZ::Entity::DSR_CYCLIC_DEPENDENCY:
                    sortResultError = "Cyclic dependency found";
                    break;
                case AZ::Entity::DSR_MISSING_REQUIRED:
                    sortResultError = "Required services missing";
                    break;
                }

                RCLogError("For slice \"%s\", Entity \"%s\" [0x%llx] dependency evaluation failed: %s. Dynamic slice cannot be generated.",
                    sourcePath.c_str(), exportEntity->GetName().c_str(), static_cast<AZ::u64>(exportEntity->GetId()),
                    sortResultError);
                return false;
            }

            exportPrefab->AddEntity(exportEntity);
        }

        AZ::PrefabComponent::EntityList exportEntities;
        exportPrefab->GetEntities(exportEntities);

        if (exportEntities.size() != sourceEntities.size())
        {
            RCLogError("Entity export list size must match that of the import list.");
            return false;
        }

        // Save runtime slice to disk.
        AZ::IO::FileIOStream outputStream(dynamicSliceOutputPath.c_str(), AZ::IO::OpenMode::ModeWrite);
        if (outputStream.IsOpen())
        {
            AZ::Utils::SaveObjectToStream<AZ::Entity>(outputStream, AZ::DataStream::ST_XML, &exportPrefabEntity);
            outputStream.Close();
        }

        // Finalize entities for export. This will remove any export components temporarily
        // assigned by the source entity's components.
        auto sourceIter = sourceEntities.begin();
        auto exportIter = exportEntities.begin();
        for (; sourceIter != sourceEntities.end(); ++sourceIter, ++exportIter)
        {
            AZ::Entity* sourceEntity = *sourceIter;
            AZ::Entity* exportEntity = *exportIter;

            const AZ::Entity::ComponentArrayType& editorComponents = sourceEntity->GetComponents();
            for (AZ::Component* component : editorComponents)
            {
                auto* asEditorComponent =
                    azrtti_cast<AzToolsFramework::Components::EditorComponentBase*>(component);

                if (asEditorComponent)
                {
                    asEditorComponent->FinishedBuildingGameEntity(exportEntity);
                }
            }
        }

        m_CC.pRC->AddInputOutputFilePair(sourcePath.c_str(), dynamicSliceOutputPath.c_str());
    }

    return true;
}

//=========================================================================
// RegisterConvertors
//=========================================================================
void __stdcall RegisterConvertors(IResourceCompiler* pRC)
{
    gEnv = pRC->GetSystemEnvironment();

    SetRCLog(pRC->GetIRCLog());

    pRC->RegisterConvertor("SliceCompiler", new SliceConverter());
}

//=========================================================================
// DllMain
//=========================================================================
BOOL APIENTRY DllMain(
    HANDLE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_hInst = (HMODULE)hModule;
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
