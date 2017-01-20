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
#include "GemDescription.h"
#include "GemRegistry.h"
#include "Dependency.h"

#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/JSON/error/en.h>
#include <AzFramework/StringFunc/StringFunc.h>

// For LinkTypeFromString
#include <unordered_map>
#include <string>

namespace Gems
{
    GemDescription::GemDescription()
        : m_id(AZ::Uuid::CreateNull())
        , m_name()
        , m_displayName()
        , m_version()
        , m_path()
        , m_summary()
        , m_iconPath()
        , m_tags()
        , m_linkType(LinkType::Dynamic)
        , m_dllFileName()
        , m_editorDllFileName()
        , m_engineModuleClass()
        , m_dependencies()
    {
    }

    GemDescription::GemDescription(const GemDescription& rhs)
        : m_id(rhs.m_id)
        , m_name(rhs.m_name)
        , m_displayName(rhs.m_displayName)
        , m_version(rhs.m_version)
        , m_path(rhs.m_path)
        , m_summary(rhs.m_summary)
        , m_iconPath(rhs.m_iconPath)
        , m_tags(rhs.m_tags)
        , m_linkType(rhs.m_linkType)
        , m_dllFileName(rhs.m_dllFileName)
        , m_editorDllFileName(rhs.m_editorDllFileName)
        , m_engineModuleClass(rhs.m_engineModuleClass)
        , m_dependencies(rhs.m_dependencies)
    {
    }


    GemDescription::GemDescription(GemDescription&& rhs)
        : m_id(rhs.m_id)
        , m_name(AZStd::move(rhs.m_name))
        , m_displayName(AZStd::move(rhs.m_displayName))
        , m_path(AZStd::move(rhs.m_path))
        , m_summary(AZStd::move(rhs.m_summary))
        , m_iconPath(AZStd::move(rhs.m_iconPath))
        , m_tags(AZStd::move(rhs.m_tags))
        , m_version(rhs.m_version)
        , m_linkType(rhs.m_linkType)
        , m_dllFileName(AZStd::move(rhs.m_dllFileName))
        , m_editorDllFileName(AZStd::move(rhs.m_editorDllFileName))
        , m_engineModuleClass(AZStd::move(rhs.m_engineModuleClass))
        , m_dependencies(AZStd::move(rhs.m_dependencies))
    {
        rhs.m_id = AZ::Uuid::CreateNull();
        rhs.m_version = GemVersion { 0, 0, 0 };
        rhs.m_linkType = LinkType::Dynamic;
    }

    // returns whether conversion was successful
    static bool LinkTypeFromString(const char* value, IGemDescription::LinkType& linkTypeOut)
    {
        // static map for lookups
        static const std::unordered_map<std::string, IGemDescription::LinkType> linkNameToType = {
            { GPF_TAG_LINK_TYPE_DYNAMIC,        IGemDescription::LinkType::Dynamic },
            { GPF_TAG_LINK_TYPE_DYNAMIC_STATIC, IGemDescription::LinkType::DynamicStatic },
            { GPF_TAG_LINK_TYPE_NO_CODE,        IGemDescription::LinkType::NoCode },
        };

        auto found = linkNameToType.find(value);
        if (found != linkNameToType.end())
        {
            linkTypeOut = found->second;
            return true;
        }
        else
        {
            return false;
        }
    }

    // Bring contents of file up to current version.
    AZ::Outcome<void, AZStd::string> UpgradeGemDescriptionJson(rapidjson::Document& descNode)
    {
        // get format version
        if (!JSON_IS_VALID_MEMBER(descNode, GPF_TAG_FORMAT_VERSION, IsInt))
        {
            return AZ::Failure(AZStd::string(GPF_TAG_FORMAT_VERSION " int is required."));
        }

        int gemFormatVersion = descNode[GPF_TAG_FORMAT_VERSION].GetInt();

        // decline ancient and future versions
        if (gemFormatVersion < 2 || gemFormatVersion > GEM_DEF_FILE_VERSION)
        {
            return AZ::Failure(AZStd::string::format(GPF_TAG_FORMAT_VERSION " is version %d, but %d is expected.",
                gemFormatVersion, GEM_DEF_FILE_VERSION));
        }

        // upgrade v2 -> v3
        if (gemFormatVersion < 3)
        {
            // beginning in v3 Gems contain an AZ::Module, in the past they contained an IGem
            descNode.AddMember("IsLegacyIGem", true, descNode.GetAllocator());
        }

        // file is now up to date
        descNode[GPF_TAG_FORMAT_VERSION] = GEM_DEF_FILE_VERSION;
        return AZ::Success();
    }

    AZ::Outcome<GemDescription, AZStd::string> GemDescription::CreateFromJson(
        rapidjson::Document& descNode,
        const AZStd::string& gemFolderPath)
    {
        // gem to build
        GemDescription gem;

        gem.m_path = gemFolderPath;

        if (!descNode.IsObject())
        {
            return AZ::Failure(AZStd::string("Json root element must be an object."));
        }

        // upgrade contents to current version
        auto upgradeOutcome = UpgradeGemDescriptionJson(descNode);
        if (!upgradeOutcome.IsSuccess())
        {
            return AZ::Failure(upgradeOutcome.TakeError());
        }

        // read name
        if (JSON_IS_VALID_MEMBER(descNode, GPF_TAG_NAME, IsString))
        {
            gem.m_name = descNode[GPF_TAG_NAME].GetString();
        }
        else
        {
            return AZ::Failure(AZStd::string(GPF_TAG_NAME " string must not be empty."));
        }

        // read display name
        if (JSON_IS_VALID_MEMBER(descNode, GPF_TAG_DISPLAY_NAME, IsString))
        {
            gem.m_displayName = descNode[GPF_TAG_DISPLAY_NAME].GetString();
        }

        // read id
        if (!JSON_IS_VALID_MEMBER(descNode, GPF_TAG_UUID, IsString))
        {
            return AZ::Failure(AZStd::string(GPF_TAG_UUID " string is required."));
        }
        gem.m_id = AZ::Uuid::CreateString(descNode[GPF_TAG_UUID].GetString());
        if (gem.m_id.IsNull())
        {
            return AZ::Failure(AZStd::string::format(GPF_TAG_UUID " string \"%s\" is invalid.", descNode[GPF_TAG_UUID].GetString()));
        }

        // read version
        if (JSON_IS_VALID_MEMBER(descNode, GPF_TAG_VERSION, IsString))
        {
            auto versionOutcome = GemVersion::ParseFromString(descNode[GPF_TAG_VERSION].GetString());

            if (versionOutcome)
            {
                gem.m_version = versionOutcome.GetValue();
            }
            else
            {
                return AZ::Failure(AZStd::string(versionOutcome.GetError()));
            }
        }
        else
        {
            return AZ::Failure(AZStd::string(GPF_TAG_VERSION " string is required."));
        }

        // read link type
        if (!JSON_IS_VALID_MEMBER(descNode, GPF_TAG_LINK_TYPE, IsString))
        {
            return AZ::Failure(AZStd::string(GPF_TAG_LINK_TYPE " string must not be empty."));
        }
        if (!LinkTypeFromString(descNode[GPF_TAG_LINK_TYPE].GetString(), gem.m_linkType))
        {
            return AZ::Failure(AZStd::string(GPF_TAG_LINK_TYPE " string is invalid."));
        }

        // dependencies
        if (descNode.HasMember(GPF_TAG_DEPENDENCIES))
        {
            if (!descNode[GPF_TAG_DEPENDENCIES].IsArray())
            {
                return AZ::Failure(AZStd::string(GPF_TAG_DEPENDENCIES " must be an array."));
            }

            // List of descriptions of Gems we depend upon
            const rapidjson::Value& depsNode = descNode[GPF_TAG_DEPENDENCIES];
            const auto& end = depsNode.End();
            for (auto it = depsNode.Begin(); it != end; ++it)
            {
                const auto& depNode = *it;
                if (!depNode.IsObject())
                {
                    return AZ::Failure(AZStd::string(GPF_TAG_DEPENDENCIES " must contain objects."));
                }

                // read id
                if (!JSON_IS_VALID_MEMBER(depNode, GPF_TAG_UUID, IsString))
                {
                    return AZ::Failure(AZStd::string(GPF_TAG_UUID " string is required for dependency."));
                }

                const char* idStr = depNode[GPF_TAG_UUID].GetString();
                AZ::Uuid id(idStr);
                if (id.IsNull())
                {
                    return AZ::Failure(AZStd::string::format(GPF_TAG_UUID " in dependency is invalid: %s.", idStr));
                }

                // read version constraints
                if (!JSON_IS_VALID_MEMBER(depNode, GPF_TAG_VERSION_CONSTRAINTS, IsArray))
                {
                    return AZ::Failure(AZStd::string(GPF_TAG_VERSION_CONSTRAINTS " array is required for dependency."));
                }

                // Make sure versions are specified
                if (depNode[GPF_TAG_VERSION_CONSTRAINTS].Size() < 1)
                {
                    return AZ::Failure(AZStd::string(GPF_TAG_VERSION_CONSTRAINTS " must have at least 1 entry for dependency."));
                }

                AZStd::vector<AZStd::string> versionConstraints;
                const auto& constraints = depNode[GPF_TAG_VERSION_CONSTRAINTS];
                const auto& end = constraints.End();
                for (auto it = constraints.Begin(); it != end; ++it)
                {
                    const auto& constraint = *it;
                    if (!constraint.IsString())
                    {
                        return AZ::Failure(AZStd::string(GPF_TAG_VERSION_CONSTRAINTS " array for dependency must contain strings."));
                    }
                    versionConstraints.push_back(constraint.GetString());
                }

                // create Dependency
                Dependency dep;
                dep.SetID(id);
                if (!dep.ParseVersions(versionConstraints))
                {
                    return AZ::Failure(AZStd::string(GPF_TAG_VERSION_CONSTRAINTS " for dependency is invalid"));
                }
                gem.m_dependencies.push_back(AZStd::make_shared<Dependency>(dep));
            }
        }

        // optional metadata
        if (JSON_IS_VALID_MEMBER(descNode, GPF_TAG_SUMMARY, IsString))
        {
            gem.m_summary = descNode[GPF_TAG_SUMMARY].GetString();
        }

        if (JSON_IS_VALID_MEMBER(descNode, GPF_TAG_ICON_PATH, IsString))
        {
            gem.m_iconPath = descNode[GPF_TAG_ICON_PATH].GetString();
        }

        if (descNode.HasMember(GPF_TAG_TAGS))
        {
            const rapidjson::Value& tags = descNode[GPF_TAG_TAGS];
            if (tags.IsArray())
            {
                const auto& end = tags.End();
                for (auto it = tags.Begin(); it != end; ++it)
                {
                    const auto& tag = *it;
                    gem.m_tags.push_back(tag.GetString());
                }
            }
            else
            {
                return AZ::Failure(AZStd::string("Value for key " GPF_TAG_TAGS " must be an array."));
            }
        }

        // engine module class
        gem.m_engineModuleClass = JSON_IS_VALID_MEMBER(descNode, GPF_TAG_MODULE_CLASS, IsString)
            ? descNode[GPF_TAG_MODULE_CLASS].GetString()
            : gem.GetName() + AZStd::string("Gem");

        // Cache constants
        char idStr[UUID_STR_BUF_LEN];
        gem.GetID().ToString(idStr, UUID_STR_BUF_LEN, false, false);
        AZStd::to_lower(idStr, idStr + strlen(idStr));

        // Dll file name is Gem.[NAME].[ID].v[VERSION]
        gem.m_dllFileName = AZStd::string::format("Gem.%s.%s.v%s", gem.GetName().c_str(), idStr, gem.GetVersion().ToString().c_str());

        // If EditorModule is specified and is true, generate dll file name. Otherwise, fallback on default file name.
        if (JSON_IS_VALID_MEMBER(descNode, GPF_TAG_EDITOR_MODULE, IsBool) && descNode[GPF_TAG_EDITOR_MODULE].GetBool())
        {
            // Dll file name is Gem.[NAME].Editor.[ID].v[VERSION]
            gem.m_editorDllFileName = AZStd::string::format("Gem.%s.Editor.%s.v%s", gem.GetName().c_str(), idStr, gem.GetVersion().ToString().c_str());
        }
        else
        {
            gem.m_editorDllFileName = gem.m_dllFileName;
        }

        return AZ::Success(AZStd::move(gem));
    }
} // namespace Gems
