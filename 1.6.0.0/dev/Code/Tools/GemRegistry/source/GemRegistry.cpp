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
#include "ProjectSettings.h"
#include "GemRegistry.h"
#include "Dependency.h"

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/JSON/document.h>
#include <AzCore/JSON/error/en.h>
#include <AzFramework/StringFunc/StringFunc.h>

#if defined(AZ_PLATFORM_WINDOWS)
#   include <shlwapi.h>
#elif defined(AZ_PLATFORM_APPLE_IOS) || defined(AZ_PLATFORM_APPLE_TV)
#   include <CoreFoundation/CFBundle.h>
#   include <CoreFoundation/CFURL.h>
#endif//AZ_PLATFORM

namespace Gems
{
    AZ_CLASS_ALLOCATOR_IMPL(GemRegistry, AZ::SystemAllocator, 0)

    bool GemRegistry::Initialize(const AZStd::string& engineRootPath)
    {
#if defined(AZ_PLATFORM_WINDOWS)
        // if its an alias like @root@, don't do anything to it:
        if ((!engineRootPath.empty()) && (engineRootPath[0] != '@') && (PathIsRelative(engineRootPath.c_str())))
        {
            // Get current directory
            char curDir[MAX_PATH];
            if (GetCurrentDirectory(MAX_PATH, curDir) == 0)
            {
                SetErrorMessage("Unable to get current working directory.");
                return false;
            }

            char absDir[MAX_PATH];
            if (PathCombine(absDir, curDir, engineRootPath.c_str()) == NULL)
            {
                SetErrorMessage("Unable to get current absolute working directory.");
                return false;
            }

            if (!PathCanonicalize(curDir, absDir))
            {
                SetErrorMessage("Unable to normalize current working directory.");
                return false;
            }

            m_engineRoot = AZStd::string {
                curDir
            };
        }
        else
        {
            m_engineRoot = engineRootPath;
        }
#elif defined(AZ_PLATFORM_APPLE_IOS) || defined(AZ_PLATFORM_APPLE_TV)
        m_engineRoot = "@root@";
#else
        m_engineRoot = engineRootPath;
#endif//AZ_PLATFORM

        return true;
    }

    bool GemRegistry::LoadAllGemsFromDisk()
    {
        // Were all Gems parsed successfully?
        bool success = true;
        // Stores the folder currently being recursed
        AZStd::string currentPath = m_engineRoot;

        // Local functions to be used
        AZ::IO::SystemFile::FindFileCB fileFinderCb;
        AZStd::function<void( const char* subDir )> recurser;

        // Handles each file and directory found
        fileFinderCb = [this, &success, &currentPath, &recurser](const char* itemName, bool isFile) -> bool
            {
                if (0 == strcmp(itemName, ".") ||
                    0 == strcmp(itemName, ".."))
                {
                    // If dir is ./ or ../, ignore but keep searching.
                    return true;
                }

                if (isFile)
                {
                    if (0 == strcmp(itemName, GEM_DEF_FILE))
                    {
                        // need relative path to gem folder
                        AZStd::string gemFolderRelPath = currentPath.substr(m_engineRoot.length());
                        AzFramework::StringFunc::RelativePath::Normalize(gemFolderRelPath);

                        success &= LoadGemDescription(gemFolderRelPath);
                    }
                }
                else
                {
                    // recurse into subdirectory
                    recurser(itemName);
                }

                return true; // keep searching
            };

        // Scans subdirectories
        recurser = [this, &currentPath, &fileFinderCb](const char* subDir)
            {
                AZStd::string oldPath = currentPath;
                AzFramework::StringFunc::Path::Join(currentPath.c_str(), subDir, currentPath, false, true, true);

                AZStd::string searchPath;
#if defined(AZ_PLATFORM_WINDOWS)
                AzFramework::StringFunc::Path::Join(currentPath.c_str(), "*", searchPath, false, true, false); // bNormalize kills '*'
#else
                searchPath = currentPath;
#endif // Windows
                AZ::IO::SystemFile::FindFiles(searchPath.c_str(), fileFinderCb);

                currentPath = oldPath;
            };

        recurser(GEMS_FOLDER);

        return success;
    }

    bool GemRegistry::LoadProject(const IProjectSettings& settings)
    {
        for (const auto& pair : settings.GetGems())
        {
            if (!LoadGemDescription(pair.second.m_path))
            {
                return false;
            }
        }

        return true;
    }

    IGemDescriptionConstPtr GemRegistry::GetGemDescription(const Specifier& spec) const
    {
        IGemDescriptionConstPtr result;

        auto idIt = m_gemDescs.find(spec.m_id);
        if (idIt != m_gemDescs.end())
        {
            auto versionIt = idIt->second.find(spec.m_version);
            if (versionIt != idIt->second.end())
            {
                result = AZStd::static_pointer_cast<IGemDescriptionConstPtr::element_type>(versionIt->second);
            }
        }

        return result;
    }

    IGemDescriptionConstPtr GemRegistry::GetLatestGem(const AZ::Uuid& uuid) const
    {
        IGemDescriptionConstPtr result;
        auto idIt = m_gemDescs.find(uuid);
        if (idIt != m_gemDescs.end())
        {
            GemVersion latestVersion;
            GemDescriptionPtr desc;

            for (const auto& pair : idIt->second)
            {
                if (pair.first > latestVersion)
                {
                    latestVersion = pair.first;
                    desc = pair.second;
                }
            }

            if (!latestVersion.IsZero())
            {
                result = AZStd::static_pointer_cast<IGemDescriptionConstPtr::element_type>(desc);
            }
        }

        return result;
    }

    AZStd::vector<IGemDescriptionConstPtr> GemRegistry::GetAllGemDescriptions() const
    {
        AZStd::vector<IGemDescriptionConstPtr> results;

        for (auto && idIt : m_gemDescs)
        {
            for (auto && versionIt : idIt.second)
            {
                results.push_back(AZStd::static_pointer_cast<IGemDescriptionConstPtr::element_type>(versionIt.second));
            }
        }

        return results;
    }

    IProjectSettings* GemRegistry::CreateProjectSettings()
    {
        return aznew ProjectSettings(this);
    }

    void GemRegistry::DestroyProjectSettings(IProjectSettings* settings)
    {
        delete settings;
    }

    bool GemRegistry::LoadGemDescription(const AZStd::string& gemFolderPath)
    {
        auto descOutcome = ParseToGemDescription(gemFolderPath);
        if (!descOutcome)
        {
            SetErrorMessage(descOutcome.GetError());
            return false;
        }

        auto desc = AZStd::make_shared<GemDescription>(descOutcome.TakeValue());

        // If the Gem hasn't been loaded yet, add it's id to the root map
        auto idIt = m_gemDescs.find(desc->GetID());
        if (idIt == m_gemDescs.end())
        {
            m_gemDescs.insert(AZStd::make_pair(desc->GetID(), AZStd::unordered_map<GemVersion, GemDescriptionPtr>()));
            idIt = m_gemDescs.find(desc->GetID());
        }

        // If the Gem's version doesn't exist, add it, otherwise update it
        auto versionIt = idIt->second.find(desc->GetVersion());
        if (versionIt == idIt->second.end())
        {
            idIt->second.insert(AZStd::make_pair(desc->GetVersion(), AZStd::move(desc)));
        }
        else
        {
            versionIt->second = desc;
        }

        return true;
    }

    AZ::Outcome<GemDescription, AZStd::string> GemRegistry::ParseToGemDescription(const AZStd::string& gemFolderRelPath)
    {
        // build absolute path to gem file
        AZStd::string filePath;
        AzFramework::StringFunc::Path::Join(m_engineRoot.c_str(), gemFolderRelPath.c_str(), filePath);
        AzFramework::StringFunc::Path::Join(filePath.c_str(), GEM_DEF_FILE, filePath);
        AZStd::to_lower(filePath.begin(), filePath.end());

        // read json
        AZStd::string fileBuf;

        // do we have a pluggable, engine-compatable fileIO?  For things like tools, we may not be plugged
        // into a game engine, and thus, we may need to use raw file io.
        AZ::IO::FileIOBase* fileReader = AZ::IO::FileIOBase::GetInstance();
        if (fileReader)
        {
            // an engine compatible file reader has been attached, so use that.
            AZ::IO::HandleType fileHandle = AZ::IO::InvalidHandle;
            AZ::u64 fileSize = 0;
            if (!fileReader->Open(filePath.c_str(), AZ::IO::OpenMode::ModeRead | AZ::IO::OpenMode::ModeBinary, fileHandle))
            {
                return AZ::Failure(AZStd::string::format("Failed to read %s - file read failed.", filePath.c_str()));
            }

            if ((!fileReader->Size(fileHandle, fileSize)) || (fileSize == 0))
            {
                fileReader->Close(fileHandle);
                return AZ::Failure(AZStd::string::format("Failed to read %s - file read failed.", filePath.c_str()));
            }

            fileBuf.resize(fileSize);
            if (!fileReader->Read(fileHandle, fileBuf.data(), fileSize, true))
            {
                fileReader->Close(fileHandle);
                return AZ::Failure(AZStd::string::format("Failed to read %s - file read failed.", filePath.c_str()));
            }
            fileReader->Close(fileHandle);
        }
        else
        {
            // we don't have an engine file io, use raw file IO.
            AZ::IO::SystemFile rawFile;
            if (!rawFile.Open(filePath.c_str(), AZ::IO::SystemFile::OpenMode::SF_OPEN_READ_ONLY))
            {
                return AZ::Failure(AZStd::string::format("Failed to read %s - file read failed.", filePath.c_str()));
            }
            fileBuf.resize(rawFile.Length());
            if (fileBuf.size() == 0)
            {
                return AZ::Failure(AZStd::string::format("Failed to read %s - file read failed.", filePath.c_str()));
            }
            if (rawFile.Read(fileBuf.size(), fileBuf.data()) != fileBuf.size())
            {
                return AZ::Failure(AZStd::string::format("Failed to read %s - file read failed.", filePath.c_str()));
            }
        }

        rapidjson::Document document;
        document.Parse(fileBuf.data());
        if (document.HasParseError())
        {
            const char* errorStr = rapidjson::GetParseError_En(document.GetParseError());
            return AZ::Failure(AZStd::string::format("Failed to parse %s: %s", filePath.c_str(), errorStr));
        }

        return GemDescription::CreateFromJson(document, gemFolderRelPath);
    }
} // namespace Gems

//////////////////////////////////////////////////////////////////////////
// DLL Exported Functions
//////////////////////////////////////////////////////////////////////////

#ifndef AZ_MONOLITHIC_BUILD // Module init functions, only required when building as a DLL.
AZ_DECLARE_MODULE_INITIALIZATION
#endif//AZ_MONOLITHIC_BUILD

extern "C" AZ_DLL_EXPORT Gems::IGemRegistry * CreateGemRegistry()
{
    return aznew Gems::GemRegistry();
}

extern "C" AZ_DLL_EXPORT void DestroyGemRegistry(Gems::IGemRegistry* reg)
{
    delete reg;
}
