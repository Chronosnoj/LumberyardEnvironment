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

#include "GemRegistry/IGemRegistry.h"

#include <AzCore/std/functional.h>
#include <AzCore/JSON/document.h>

namespace Gems
{
    class GemDescription
        : public IGemDescription
    {
    public:
        GemDescription(const GemDescription& rhs);
        GemDescription(GemDescription&& rhs);
        ~GemDescription() override = default;

        // IGemDescription
        const AZ::Uuid& GetID() const override { return m_id; }
        const AZStd::string& GetName() const override { return m_name; }
        const AZStd::string& GetDisplayName() const override { return m_displayName.empty() ? m_name : m_displayName; }
        const GemVersion& GetVersion() const override { return m_version; }
        const AZStd::string& GetPath() const override { return m_path; }
        const AZStd::string& GetSummary() const override { return m_summary; }
        const AZStd::string& GetIconPath() const override { return m_iconPath; }
        const AZStd::vector<AZStd::string>& GetTags() const override { return m_tags; }
        LinkType GetLinkType() const override { return m_linkType; }
        const AZStd::string& GetDllFileName() const override { return m_dllFileName; }
        const AZStd::string& GetEditorDllFileName() const override { return m_editorDllFileName; }
        const AZStd::string& GetEngineModuleClass() const override { return m_engineModuleClass; }
        const AZStd::vector<AZStd::shared_ptr<IDependency> >& GetDependencies() const override { return m_dependencies; }
        // ~IGemDescription

        // Internal methods

        /// Create GemDescription from Json.
        ///
        /// \param[in]  json    Json object to parse. json may be modified during parse.
        /// \param[in]  gemFolderPath Relative path from engine root to Gem folder.
        ///
        /// \returns    If successful, the GemDescription.
        ///             If unsuccessful, an explanation why parsing failed.
        static AZ::Outcome<GemDescription, AZStd::string> CreateFromJson(
            rapidjson::Document& json,
            const AZStd::string& gemFolderPath);

    private:
        // Outsiders may not create an empty GemDescription
        GemDescription();

        /// The ID of the Gem
        AZ::Uuid m_id;
        /// The name of the Gem
        AZStd::string m_name;
        /// The UI-friendly name of the Gem
        AZStd::string m_displayName;
        /// The version of the Gem
        GemVersion m_version;
        /// Path to Gem folder
        AZStd::string m_path;
        /// Summary description of the Gem
        AZStd::string m_summary;
        /// Icon path of the gem to be displayed by Lyzard
        AZStd::string m_iconPath;
        /// Tags to associate with the Gem
        AZStd::vector<AZStd::string> m_tags;
        /// How to link against this Gem
        LinkType m_linkType;
        /// The name of the compiled dll
        AZStd::string m_dllFileName;
        /// The name of the compiled dll (for the editor). Same as m_dllFileName if no Editor specific module.
        AZStd::string m_editorDllFileName;
        /// The name of the engine module class to initialize
        AZStd::string m_engineModuleClass;
        /// A Gem's dependencies
        AZStd::vector<AZStd::shared_ptr<IDependency> > m_dependencies;
    };
} // namespace Gems
