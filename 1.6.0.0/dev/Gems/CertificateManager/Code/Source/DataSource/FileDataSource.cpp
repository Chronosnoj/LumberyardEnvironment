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

#include <AzCore/IO/SystemFile.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/containers/vector.h>

#include "Source/DataSource/FileDataSource.h"

namespace CertificateManager
{

    static bool ReadFileIntoString(const char* filename, AZStd::vector<char>& outBuffer)
    {
        char resolvedPath[MAX_PATH] = { 0 };
        const CryStringT<char> filePath = PathUtil::Make("@root@", PathUtil::GetFile(filename));

        if (!gEnv->pFileIO->ResolvePath(filePath.c_str(), resolvedPath, MAX_PATH))
        {
            return false;
        }

        if (!AZ::IO::SystemFile::Exists(resolvedPath))
        {
            return false;
        }

        uint64_t fileSize = AZ::IO::SystemFile::Length(resolvedPath);
        if (fileSize == 0)
        {
            return false;
        }

        outBuffer.resize(fileSize + 1);
        if (!AZ::IO::SystemFile::Read(resolvedPath, outBuffer.data()))
        {
            return false;
        }

        return true;
    }

    FileDataSource::FileDataSource() 
        : m_privateKeyPEM(nullptr)
        , m_certificatePEM(nullptr)
        , m_certificateAuthorityCertPEM(nullptr)
    {
        FileDataSourceConfigurationBus::Handler::BusConnect();
    }

    FileDataSource::~FileDataSource()
    {
        FileDataSourceConfigurationBus::Handler::BusDisconnect();

        azfree(m_privateKeyPEM);
        azfree(m_certificatePEM);
        azfree(m_certificateAuthorityCertPEM);
    }    

    void FileDataSource::ConfigureDataSource(const char* keyPath, const char* certPath, const char* caPath)
    {
        ConfigurePrivateKey(keyPath);
        ConfigureCertificate(certPath);
        ConfigureCertificateAuthority(caPath);        
    }

    void FileDataSource::ConfigurePrivateKey(const char* path)
    {
        if (m_privateKeyPEM != nullptr)
        {
            azfree(m_privateKeyPEM);
            m_privateKeyPEM = nullptr;
        }

        if (path != nullptr)
        {
            LoadGenericFile(path,m_privateKeyPEM);
        }
    }

    void FileDataSource::ConfigureCertificate(const char* path)
    {
        if (m_certificatePEM != nullptr)
        {
            azfree(m_certificatePEM);
            m_certificatePEM = nullptr;
        }

        if (path != nullptr)
        {
            LoadGenericFile(path,m_certificatePEM);
        }        
    }

    void FileDataSource::ConfigureCertificateAuthority(const char* path)
    {
        if (m_certificateAuthorityCertPEM != nullptr)
        {
            azfree(m_certificateAuthorityCertPEM);
            m_certificateAuthorityCertPEM = nullptr;
        }

        if (path != nullptr)
        {
            LoadGenericFile(path,m_certificateAuthorityCertPEM);
        }
    }

    bool FileDataSource::HasCertificateAuthority() const
    {
        return m_certificateAuthorityCertPEM != nullptr;
    }

    char* FileDataSource::RetrieveCertificateAuthority()
    {
        return m_certificateAuthorityCertPEM;
    }
    
    bool FileDataSource::HasPublicKey() const
    {
        return m_certificatePEM != nullptr;
    }

    char* FileDataSource::RetrievePublicKey()
    {
        return m_certificatePEM;
    }
    
    bool FileDataSource::HasPrivateKey() const
    {
        return m_privateKeyPEM != nullptr;
    }

    char* FileDataSource::RetrievePrivateKey()
    {
        return m_privateKeyPEM;
    }

    void FileDataSource::LoadGenericFile(const char* filename, char* &destination)
    {
        if (filename)
        {
            AZStd::vector<char> contents;
            if (ReadFileIntoString(filename, contents))
            {
                if (destination != nullptr)
                {
                    azfree(destination);
                    destination = nullptr;
                }
                destination = reinterpret_cast<char*>(azmalloc(contents.size()));
                if (destination == nullptr)
                {
                    AZ_Error("CertificateManager", false, "Invalid destination for file input");
                    return;
                }
                memcpy(destination, contents.data(), contents.size());
            }
            else
            {
                AZ_Warning("CertificateManager", false, "Failed to read authentication file '%s'.", filename);
            }
        }
    }
} //namespace CertificateManager
