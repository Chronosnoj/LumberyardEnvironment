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
#ifndef ASSETUTILS_H
#define ASSETUTILS_H

#include <AzCore/std/string/regex.h>

#include <cstdlib> // for size_t
#include <QString>
#include <AssetBuilderSDk/AssetBuilderSDK.h>
#include <AssetBuilderSDk/AssetBuilderBusses.h>
#include "native/assetprocessor.h"

namespace AzToolsFramework
{
    namespace AssetSystem
    {
        struct JobInfo;
    }
}

class QStringList;
class QDir;

namespace AssetProcessor
{
    class PlatformConfiguration;
    struct AssetRecognizer;
}

namespace AssetUtilities
{
    //! Compute the current branch token
    //! This token will be used during negotiation with the game/editor to ensure that we are communicating with the assetprocessor in the correct branch
    QString GetBranchToken();

    //! Compute the root folder by scanning for marker files such as root.ini
    //! By Default, this searches the applications root and walks upwards, but you are allowed to instead
    //! supply a different starting root.  in that case, it will start from there instead, and walk upwards.
    bool ComputeEngineRoot(QDir& root, const QDir* optionalStartingRoot = nullptr);

    //! Reset the engine root to not be cached anymore.  Generally only useful for tests
    void ResetEngineRoot();

    //! Copy all files from  the source directory to the destination directory, returns true if successfull, else return false
    bool CopyDirectory(QDir source, QDir destination);

    //! Computes and returns the application directory and filename
    void ComputeApplicationInformation(QString& dir, QString& filename);

    //! Check the extension of all the products
    //! return true if any one of the product extension matches the input extension, else return false
    bool CheckProductsExtension(QStringList productList, QString ext);

    //! makes the file writable
    //! return true if operation is successful, otherwise return false
    bool MakeFileWritable(QString filename);

    //! Check to see if we can Lock the file
    bool CheckCanLock(QString filename);

    bool InitializeQtLibraries();
    //! Check the extension of all the products
    //! return true if any one of the product extension matches the input extension, else return false
    bool CheckProductsExtension(QStringList productList, QString ext);

    //! Updates the branch token in the bootstrap file
    bool UpdateBranchToken();

    //! Determine the name of the current game - for example, SamplesProject
    QString ComputeGameName(QString initialFolder = QString("."), bool force = false);

    //! Computes the platformname from the platform flag, returns an empty qstring if an invalid flag is inputted
    QString ComputePlatformName(int platform);

    //! Computes the platformflag from the platform name, returns 0 if an invalid platformname is inputted
    int ComputePlatformFlag(QString platform);

    //! Reads the white list directly from the bootstrap file
    QString ReadWhitelistFromBootstrap(QString initialFolder = QString( "." ));

    //! Reads the game name directly from the bootstrap file
    QString ReadGameNameFromBootstrap(QString initialFolder = QString("."));

    //! Reads a pattern from the bootstrap file
    QString ReadPatternFromBootstrap(QRegExp regExp, QString initialFolder);

    //! Reads the listening port from the bootstrap file
    //! By default the listening port is 45643
    quint16 ReadListeningPortFromBootstrap(QString initialFolder = QString("."));

    //! Reads platforms from command line
    QStringList ReadPlatformsFromCommandLine();

    //! Copies the sourceFile to the outputFile,returns true if the copy operation succeeds otherwise return false
    //! This function will try deleting the outputFile first,if it exists, before doing the copy operation
    bool CopyFileWithTimeout(QString sourceFile, QString outputFile, unsigned int waitTimeinSeconds = 0);
    //! Moves the sourceFile to the outputFile,returns true if the move operation succeeds otherwise return false
    //! This function will try deleting the outputFile first,if it exists, before doing the move operation
    bool MoveFileWithTimeout(QString sourceFile, QString outputFile, unsigned int waitTimeinSeconds = 0);

    //! Normalize and removes any alias from the path
    QString NormalizeAndRemoveAlias(QString path);

    //! Determine the Job Description for a job, for now it is the name of the recognizer
    QString ComputeJobDescription(const AssetProcessor::AssetRecognizer* recognizer);

    //!This  function generates a key based on file name
    QString GenerateKeyForSourceFile(QString file, AssetProcessor::PlatformConfiguration* platformConfig);

    //! Compute the root of the cache for the current project.
    //! This is generally the "cache" folder, subfolder gamedir.
    bool ComputeProjectCacheRoot(QDir& projectCacheRoot);

    //! Compute the folder that will be used for fence files.
    bool ComputeFenceDirectory(QDir& fenceDir);

    //! Given a file path, normalize it into a format that will succeed in case-insensitive compares to other files.
    //! Even if the data file is copied to other operating systems
    //! for example, switch all slashes to forward slashes.
    //! Note:  does not convert into absolute path or canonicalize the path to remove ".." and such.
    QString NormalizeFilePath(const QString& filePath);
    void NormalizeFilePaths(QStringList& filePaths);

    //! given a directory name, normalize it the same way as the above file path normalizer
    //! does not convert into absolute path - do that yourself before calling this if you want that
    QString NormalizeDirectoryPath(const QString& directoryPath);

    //! Compute a CRC given a null-terminated string
    //! @param[in] priorCRC     If supplied, continues an existing CRC by feeding it more data
    unsigned int ComputeCRC32(const char* inString, unsigned int priorCRC = 0xFFFFFFFF);

    //! Compute a CRC given data and a size
    //! @param[in] priorCRC     If supplied, continues an existing CRC by feeding it more data
    unsigned int ComputeCRC32(const char* data, size_t dataSize, unsigned int priorCRC = 0xFFFFFFFF);

    //! Compute a CRC given data and a size
    //! @param[in] priorCRC     If supplied, continues an existing CRC by feeding it more data
    template <typename T>
    unsigned int ComputeCRC32(const T* data, size_t dataSize, unsigned int priorCRC = 0xFFFFFFFF)
    {
        return ComputeCRC32(reinterpret_cast<const char*>(data), dataSize, priorCRC);
    }

    //! Compute a CRC given a null-terminated string
    //! @param[in] priorCRC     If supplied, continues an existing CRC by feeding it more data
    unsigned int ComputeCRC32Lowercase(const char* inString, unsigned int priorCRC = 0xFFFFFFFF);

    //! Compute a CRC given data and a size
    //! @param[in] priorCRC     If supplied, continues an existing CRC by feeding it more data
    unsigned int ComputeCRC32Lowercase(const char* data, size_t dataSize, unsigned int priorCRC = 0xFFFFFFFF);

    //! Compute a CRC given data and a size
    //! @param[in] priorCRC     If supplied, continues an existing CRC by feeding it more data
    template <typename T>
    unsigned int ComputeCRC32Lowercase(const T* data, size_t dataSize, unsigned int priorCRC = 0xFFFFFFFF)
    {
        return ComputeCRC32Lowercase(reinterpret_cast<const char*>(data), dataSize, priorCRC);
    }

    //! attempt to create a workspace for yourself to use as scratch-space, at that starting root folder.
    //! If it succeeds, it will return the final folder name.  this includes creation of temp folder with numbered/lettered temp characters in it.
    //! Note that its up to you to clean this temp workspace up.  It will not automatically be deleted.
    //! If it fails, it will return an empty string, and you must try a different folder.
    QString CreateTempWorkspace(QString startFolder);

    //! More broad function to create a temp workspace by trying multiple possible locations.
    //! If it fails, it will return an empty string, and the application will shut down.
    QString CreateTempWorkspace();

    AZStd::string ComputeJobLogFolder();
    AZStd::string ComputeJobLogFileName(const AzToolsFramework::AssetSystem::JobInfo& jobInfo);

    //! interrogate a given file, which is specified as a full path name, and generate a fingerprint for it.
    unsigned int GenerateFingerprint(const AssetProcessor::JobDetails& jobDetail);
    // Generates a fingerprint for a file without querying the existence of metadata files.  Helper function for GenerateFingerprint.
    unsigned int GenerateBaseFingerprint(QString fullPathToFile, QString extraInfo = QString());

    //! This class represents a matching pattern that is based on AssetBuilderSDK::AssetBuilderPattern::PatternType, which can either be a regex
    //! pattern or a wildcar (glob) pattern
    class FilePatternMatcher
    {
    public:
        FilePatternMatcher() = default;
        FilePatternMatcher(const AssetBuilderSDK::AssetBuilderPattern& pattern);
        FilePatternMatcher(const AZStd::string& pattern, AssetBuilderSDK::AssetBuilderPattern::PatternType type);
        FilePatternMatcher(const FilePatternMatcher& copy);

        typedef AZStd::regex RegexType;

        FilePatternMatcher& operator=(const FilePatternMatcher& copy);

        bool MatchesPath(const AZStd::string& assetPath) const;
        bool MatchesPath(const QString& assetPath) const;
        bool IsValid() const;
        AZStd::string GetErrorString() const;
        const AssetBuilderSDK::AssetBuilderPattern& GetBuilderPattern() const;

    protected:
        AssetBuilderSDK::AssetBuilderPattern    m_pattern;
        static bool ValidatePatternRegex(const AZStd::string& pattern, AZStd::string& errorString);
        RegexType           m_regex;
        bool                m_isRegex;
        bool                m_isValid;
        AZStd::string       m_errorString;

    };

    class BuilderFilePatternMatcher
        : public FilePatternMatcher
    {
    public:
        BuilderFilePatternMatcher() = default;
        BuilderFilePatternMatcher(const BuilderFilePatternMatcher& copy);
        BuilderFilePatternMatcher(const AssetBuilderSDK::AssetBuilderPattern& pattern, const AZ::Uuid& builderDescID);

        const AZ::Uuid& GetBuilderDescID() const;
    protected:
        AZ::Uuid    m_builderDescID;
    };
} // end namespace AssetUtilities

#endif // ASSETUTILS_H


