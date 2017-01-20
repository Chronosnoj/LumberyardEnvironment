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

#include "StdAfx.h"
#include "FileUtil.h"
#include "PathHelpers.h"
#include "StringHelpers.h"

#include <io.h>

//////////////////////////////////////////////////////////////////////////
// returns true if 'dir' is a subdirectory of 'baseDir' or same directory as 'baseDir'
// note: returns false in case of wrong names passed
static bool IsSubdirOrSameDir(const char* dir, const char* baseDir)
{
    char szFullPathDir[2 * 1024];
    if (!_fullpath(szFullPathDir, dir, sizeof(szFullPathDir)))
    {
        return false;
    }

    char szFullPathBaseDir[2 * 1024];
    if (!_fullpath(szFullPathBaseDir, baseDir, sizeof(szFullPathBaseDir)))
    {
        return false;
    }

    const char* p = szFullPathDir;
    const char* q = szFullPathBaseDir;
    for (;; ++p, ++q)
    {
        if (tolower(*p) == tolower(*q))
        {
            if (*p == 0)
            {
                // dir is exactly same as baseDir
                return true;
            }
            continue;
        }

        if ((*p == '/' || *p == '\\') && (*q == '/' || *q == '\\'))
        {
            continue;
        }

        if (*p == 0)
        {
            // dir length is shorter than baseDir length. so it's not a subdir
            return false;
        }

        if (*q == 0)
        {
            // baseDir is shorter than dir. so may be it's a subdir.
            const bool isSubdir = (*p == '/' || *p == '\\');
            return isSubdir;
        }

        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
// the paths must have trailing slash
static bool ScanDirectoryRecursive(const string& root, const string& path, const string& file, std::vector<string>& files, bool recursive, const string& dirToIgnore)
{
    __finddata64_t c_file;
    intptr_t hFile;

    bool anyFound = false;

    string fullPath = PathHelpers::Join(root, path);

    if (!dirToIgnore.empty())
    {
        if (IsSubdirOrSameDir(fullPath.c_str(), dirToIgnore.c_str()))
        {
            return anyFound;
        }
    }

    fullPath = PathHelpers::Join(fullPath, file);

    if ((hFile = _findfirst64(fullPath.c_str(), &c_file)) != -1L)
    {
        // Find the rest of the files.
        do
        {
            if ((!(c_file.attrib & _A_SUBDIR)) && StringHelpers::MatchesWildcardsIgnoreCase(c_file.name, file))
            {
                anyFound = true;
                files.push_back(PathHelpers::Join(path, c_file.name));
            }
        }   while (_findnext64(hFile, &c_file) == 0);
        _findclose(hFile);
    }

    if (recursive)
    {
        fullPath = PathHelpers::Join(PathHelpers::Join(root, path), "*.*");
        if ((hFile = _findfirst64(fullPath.c_str(), &c_file)) != -1L)
        {
            // Find directories.
            do
            {
                if (c_file.attrib & _A_SUBDIR)
                {
                    // If recursive.
                    if (strcmp(c_file.name, ".") && strcmp(c_file.name, ".."))
                    {
                        if (ScanDirectoryRecursive(root, PathHelpers::Join(path, c_file.name), file, files, recursive, dirToIgnore))
                        {
                            anyFound = true;
                        }
                    }
                }
            }   while (_findnext64(hFile, &c_file) == 0);
            _findclose(hFile);
        }
    }

    return anyFound;
}

//////////////////////////////////////////////////////////////////////////

bool FileUtil::ScanDirectory(const string& path, const string& file, std::vector<string>& files, bool recursive, const string& dirToIgnore)
{
    return ScanDirectoryRecursive(path, "", file, files, recursive, dirToIgnore);
}


bool FileUtil::EnsureDirectoryExists(const char* szPathIn)
{
    if (!szPathIn || !szPathIn[0])
    {
        return true;
    }

    if (DirectoryExists(szPathIn))
    {
        return true;
    }

    std::vector<char> path(szPathIn, szPathIn + strlen(szPathIn) + 1);
    char* p = &path[0];

    // Skip '/' and '//' in the beginning
    while (*p == '/' || *p == '\\')
    {
        ++p;
    }

    for (;; )
    {
        while (*p != '/' && *p != '\\' && *p)
        {
            ++p;
        }
        const char saved = *p;
        *p = 0;
        _mkdir(&path[0]);
        *p++ = saved;
        if (saved == 0)
        {
            break;
        }
    }

    return DirectoryExists(szPathIn);
}
