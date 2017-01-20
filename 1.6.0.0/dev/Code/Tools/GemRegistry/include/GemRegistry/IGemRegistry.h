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

#include <AzCore/Math/Uuid.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/containers/unordered_map.h>

#include "GemRegistry/IDependency.h"
#include "GemRegistry/GemVersion.h"

namespace Gems
{
    /**
     * Describes an instance of a Gem.
     */
    class IGemDescription
    {
    public:
        /// Describes how other Gems (and the final executable) will link against this Gem.
        enum class LinkType
        {
            /// Do not link against this Gem, it is loaded as a Dynamic Library at runtime.
            Dynamic,
            /// Link against this Gem, it is also loaded as a Dynamic Library at runtime.
            DynamicStatic,
            /// Gem has no code, there is nothing to link against.
            NoCode,
        };

        /// The ID of the Gem
        virtual const AZ::Uuid& GetID() const = 0;
        /// The name of the Gem
        virtual const AZStd::string& GetName() const = 0;
        /// The UI-friendly name of the Gem
        virtual const AZStd::string& GetDisplayName() const = 0;
        /// The version of the Gem
        virtual const GemVersion& GetVersion() const = 0;
        /// Path to the folder of this Gem
        virtual const AZStd::string& GetPath() const = 0;
        /// Summary description of the Gem
        virtual const AZStd::string& GetSummary() const = 0;
        /// Icon path of the gem to be displayed by Lyzard
        virtual const AZStd::string& GetIconPath() const = 0;
        /// Tags to associated with the Gem
        virtual const AZStd::vector<AZStd::string>& GetTags() const = 0;
        /// How to link against this Gem
        virtual LinkType GetLinkType() const = 0;
        /// Whether this Gem compiles to a dll
        bool HasDll() const
        {
            return GetLinkType() == LinkType::Dynamic
                   || GetLinkType() == LinkType::DynamicStatic;
        }
        /// The name of the compiled dll
        virtual const AZStd::string& GetDllFileName() const = 0;
        /// The name of the editor dll file. Returns same as GetDllFileName() if there isn't an editor specific module.
        virtual const AZStd::string& GetEditorDllFileName() const = 0;
        /// The name of the engine module class to initialize
        virtual const AZStd::string& GetEngineModuleClass() const = 0;
        /// Get the Gem's dependencies
        virtual const AZStd::vector<AZStd::shared_ptr<IDependency> >& GetDependencies() const = 0;

        virtual ~IGemDescription() = default;
    };
    using IGemDescriptionConstPtr = AZStd::shared_ptr<const IGemDescription>;

    /// A specific Gem known to a Project.
    /// The Gem is not used unless it is enabled.
    struct ProjectGemSpecifier
        : public Specifier
    {
        /// Folder in which this specific Gem can be found.
        AZStd::string m_path;

        ProjectGemSpecifier(const AZ::Uuid& id, const GemVersion& version, const AZStd::string& path)
            : Specifier(id, version)
            , m_path(path)
        {}
        ~ProjectGemSpecifier() override = default;
    };
    using ProjectGemSpecifierMap = AZStd::unordered_map<AZ::Uuid, ProjectGemSpecifier>;

    /**
     * Stores project-specific settings, such as which Gems are enabled and which versions are required.
     */
    class IProjectSettings
    {
    public:
        /**
         * Initializes the ProjectSettings with a project name to load the settings from.
         *
         * \param[in] projectFolder   The folder in which the project's assets reside (and the configuration file)
         *
         * \returns                 True on success, false on failure.
         */
        virtual bool Initialize(const AZStd::string& projectFolder) = 0;

        /**
         * Enables the specified instance of a Gem.
         *
         * \param[in] spec      The specific Gem to enable.
         *
         * \returns             True on success, False on failure.
         */
        virtual bool EnableGem(const ProjectGemSpecifier& spec) = 0;

        /**
         * Disables the specified instance of a Gem.
         *
         * \param[in] spec      The specific Gem to disable.
         *
         * \returns             True on success, False on failure.
         */
        virtual bool DisableGem(const Specifier& spec) = 0;

        /**
         * Checks if a Gem of the specified description is enabled.
         *
         * \param[in] spec      The specific Gem to check.
         *
         * \returns             True if the Gem is enabled, False if it is disabled.
         */
        virtual bool IsGemEnabled(const Specifier& spec) const = 0;

        /**
         * Checks if a Gem of the specified ID and version constraints is enabled.
         *
         * \param[in] id                     The ID of the Gem to check.
         * \param[in] versionConstraints     An array of strings, each of which is a condition using the gem dependency syntax
         *
         * \returns             True if the Gem is enabled and passes every version constraint condition, False if it is disabled or does not match the version constraints.
         */
        virtual bool IsGemEnabled(const AZ::Uuid& id, const AZStd::vector<AZStd::string>& versionConstraints) const = 0;

        /**
         * Checks if a Gem of the specified dependency is enabled.
         *
         * \param[in] dep       The dependency to validate.
         *
         * \returns             True if the Gem is enabled, False if it is disabled.
         */
        virtual bool IsDependencyMet(const AZStd::shared_ptr<IDependency> dep) const = 0;

        /**
         * Gets the Gems known to this project.
         * Only enabled Gems are actually used at runtime.
         * A project can only reference one version of a Gem.
         *
         * \returns             The vector of enabled Gems.
         */
        virtual const ProjectGemSpecifierMap& GetGems() const = 0;

        /**
        * Sets the Gem map to the passed in list. Used when resetting after a failed save.
        */
        virtual void SetGems(const ProjectGemSpecifierMap& newGemMap) = 0;

        /**
         * Checks that all installed Gems have their dependencies met.
         * Any unmet dependencies can be found via IGemRegistry::GetErrorMessage();
         *
         * \returns             True if all dependencies are met, false otherwise.
         */
        virtual bool ValidateDependencies() const = 0;

        /**
         * Saves the current state of the project settings to it's project configuration file.
         *
         * \returns             void on success, error message on failure.
         */
        virtual AZ::Outcome<void, AZStd::string> Save() const = 0;

        virtual ~IProjectSettings() = default;
    };

    /**
     * Manages installed Gems.
     */
    class IGemRegistry
    {
    public:
        /**
         * Initialize the Registry with the given engine root.
         *
         * \param[in] engineRoot    The path to the engine root (relative to the current execution directory)
         *
         * \returns                 True if the Gem is enabled, False if it is disabled.
         */
        virtual bool Initialize(const AZStd::string& engineRootPath) = 0;

        /**
         * Scans the Gems/ folder for all installed Gems.
         *
         * In the event of an error loading a Gem, the error will be recorded,
         * the Gem will not be loaded, and false will be returned.
         *
         * Loaded Gems can be accessed via GetGemDescription() or GetAllGemDescriptions().
         *
         * \returns                 True if the search succeeded, False if any errors occurred.
         */
        virtual bool LoadAllGemsFromDisk() = 0;

        /**
         * Loads Gems for the specified project.
         * May be called for multiple projects.
         *
         * \param[in] settings      The project to load Gems for.
         *
         * \returns                 True if the Gem is enabled, False if it is disabled.
         */
        virtual bool LoadProject(const IProjectSettings& settings) = 0;

        /**
         * Gets the description for a Gem.
         *
         * \param[in] spec      The specific Gem to search for.
         *
         * \returns             A pointer to the Gem's description if it exists, otherwise nullptr.
         */
        virtual IGemDescriptionConstPtr GetGemDescription(const Specifier& spec) const = 0;

        /**
         * Gets the description for the latest version of a Gem.
         *
         * \param[in] uuid      The specific uuid of the Gem to search for.
         *
         * \returns             A pointer to the Gem's description highest version if it exists, otherwise nullptr.
         */
        virtual IGemDescriptionConstPtr GetLatestGem(const AZ::Uuid& uuid) const = 0;

        /**
         * Gets a list of all loaded Gem descriptions.
         */
        virtual AZStd::vector<IGemDescriptionConstPtr> GetAllGemDescriptions() const = 0;

        /**
         * Creates a new instance of IProjectSettings.
         *
         * \returns                 A new project settings object.
         */
        virtual IProjectSettings* CreateProjectSettings() = 0;

        /**
         * Destroys an instance of IProjectSettings.
         *
         * \params[in] settings     The settings instance to destroy.
         */
        virtual void DestroyProjectSettings(IProjectSettings* settings) = 0;

        /**
         * Gets the most recent error message.
         * Any method that returns false for failure will post
         * a message that can be retrieved here.
         *
         * \returns                 The most recent error message.
         */
        virtual const AZStd::string& GetErrorMessage() const = 0;

        virtual ~IGemRegistry() = default;
    };

    /**
     * The type of function exported for creating a new GemRegistry.
     */
    using RegistryCreatorFunction = IGemRegistry * (*)();
    #define GEMS_REGISTRY_CREATOR_FUNCTION_NAME "CreateGemRegistry"

    /**
     * The type of function exported for destroying a GemRegistry.
     */
    using RegistryDestroyerFunction = void(*)(IGemRegistry*);
    #define GEMS_REGISTRY_DESTROYER_FUNCTION_NAME "DestroyGemRegistry"
} // namespace Gems
