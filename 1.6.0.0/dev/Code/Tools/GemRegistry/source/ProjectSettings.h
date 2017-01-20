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

#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/JSON/document.h>

#include "GemRegistry.h"

namespace Gems
{
    class ProjectSettings
        : public IProjectSettings
    {
    public:
        AZ_CLASS_ALLOCATOR_DECL

        ProjectSettings(GemRegistry* registry);
        ~ProjectSettings() override = default;

        // IProjectSettings
        bool Initialize(const AZStd::string& projectFolder) override;

        bool EnableGem(const ProjectGemSpecifier& spec) override;
        bool DisableGem(const Specifier& spec) override;
        bool IsGemEnabled(const Specifier& spec) const override;
        bool IsGemEnabled(const AZ::Uuid& id, const AZStd::vector<AZStd::string>& versionConstraints) const override;
        bool IsDependencyMet(const AZStd::shared_ptr<IDependency> dep) const override;

        const ProjectGemSpecifierMap& GetGems() const override { return m_gems; }
        void SetGems(const ProjectGemSpecifierMap& newGemMap) override { m_gems = newGemMap; }
        bool ValidateDependencies() const override;

        AZ::Outcome<void, AZStd::string> Save() const override;
        // ~IProjectSettings

        // Internal methods
        /// Loads settings from the path provided by m_settingsFilePath
        bool LoadSettings();
        /// Converts the ProjectGemSpecifierMap (m_gems) into it's Json representation for saving
        rapidjson::Document GetJsonRepresentation() const;
        /// Converts Json into the ProjectGemSpecifierMap (m_gems)
        bool ParseJson(const rapidjson::Document& json);

    private:
        ProjectGemSpecifierMap m_gems;
        GemRegistry* m_registry;
        AZStd::string m_settingsFilePath;
        bool m_initialized;
    };
} // namespace Gems
