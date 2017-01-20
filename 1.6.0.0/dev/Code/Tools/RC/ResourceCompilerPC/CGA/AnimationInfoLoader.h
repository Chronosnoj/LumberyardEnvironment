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

#ifndef CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERPC_CGA_ANIMATIONINFOLOADER_H
#define CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERPC_CGA_ANIMATIONINFOLOADER_H
#pragma once

#include "PathHelpers.h"

class ICryXML;

inline void UnifyPath(string& str)
{
    str.MakeLower();
    str = PathHelpers::ToUnixPath(str);
}


// Final values used for compressing/processing a bone.
struct SBoneCompressionValues
{
    enum EDelete
    {
        eDelete_No,
        eDelete_Auto,
        eDelete_Yes
    };

    EDelete m_eAutodeletePos;
    float m_autodeletePosEps;   // value interpretation depends on SAnimationDesc::m_bNewFormat
    EDelete m_eAutodeleteRot;
    float m_autodeleteRotEps;   // value interpretation depends on SAnimationDesc::m_bNewFormat

    float m_compressPosTolerance;
    float m_compressRotToleranceInDegrees;
};


// Processing & compressing parameters for single bone.
// "const" data (never changed during processing of an animation)
struct SBoneCompressionDesc
{
    string m_namePattern;

    SBoneCompressionValues::EDelete m_eDeletePos;
    SBoneCompressionValues::EDelete m_eDeleteRot;

    union
    {
        // used in new format only. see SAnimationDesc::m_bNewFormat
        struct NewFormat
        {
            float m_compressPosEps;
            float m_compressRotEps;
        } newFmt;

        // used in old format only. see SAnimationDesc::m_bNewFormat
        struct OldFormat
        {
            float m_mult;
        } oldFmt;
    };
};


// Processing & compressing parameters for single animation.
// "const" data (never changed during processing of an animation)
struct SAnimationDesc
{
public:
    bool m_bSkipSaveToDatabase;
    bool m_bAdditiveAnimation;

    bool m_bNewFormat;

    string m_skeletonName;

    union
    {
        struct NewFormat
        {
            float m_autodeletePosEps; // the bigger value the higher chance that the channel will be deleted
            float m_autodeleteRotEps; // the bigger value the higher chance that the channel will be deleted
            float m_compressPosMul;   // the bigger value the more keys will be deleted
            float m_compressRotMul;   // the bigger value the more keys will be deleted
        } newFmt;
        struct OldFormat
        {
            int   m_CompressionQuality;
            float m_fPOS_EPSILON;
            float m_fROT_EPSILON;
        } oldFmt;
    };

    std::vector<SBoneCompressionDesc> m_perBoneCompressionDesc;
    std::vector<string> m_tags;

private:
    string m_AnimString;

public:
    SAnimationDesc()
    {
        m_bSkipSaveToDatabase = false;
        m_bAdditiveAnimation = false;
        m_bNewFormat = false;
        oldFmt.m_CompressionQuality = 0;
        oldFmt.m_fPOS_EPSILON = 0.02f;
        oldFmt.m_fROT_EPSILON = 0.005f;
    }

    void SetAnimationName(const string& name)
    {
        m_AnimString = name;
        UnifyPath(m_AnimString);
    }

    const string& GetAnimString() const
    {
        return m_AnimString;
    }

    void LoadBoneSettingsFromXML(const XmlNodeRef& root);

    SBoneCompressionValues GetBoneCompressionValues(const char* boneName, float platformMultiplier) const;
};


// Container of parameters for animations for reference model
struct SAnimationDefinition
{
    string m_Model;
    string m_DBName;


    SAnimationDesc m_MainDesc;

    std::vector<SAnimationDesc> m_OverrideAnimations;

    const SAnimationDesc& GetAnimationDesc(const string& name) const;

    void SetAnimationPath(const string& path, const string& unifiedPath)
    {
        m_AnimationPath = PathHelpers::AddSeparator(path);
        UnifyPath(m_AnimationPath);

        m_AnimationPathWithoutSlash = PathHelpers::RemoveSeparator(m_AnimationPath);

        m_UnifiedAnimationPath = unifiedPath;
    }

    bool FindIdentical(const string& name, bool checkLen = false);
    const string& GetAnimationPath() const { return m_AnimationPath; };
    const string& GetAnimationPathWithoutSlash() const { return m_AnimationPathWithoutSlash; };
    const string& GetUnifiedAnimationPath() const { return m_UnifiedAnimationPath; };


    static void SetOverrideAnimationSettingsFilename(const string& animationSettingsFilename);

    enum EPreferredAnimationSettingsFile
    {
        ePASF_OVERRIDE_FILE,
        ePASF_DEFAULT_FILE,
        ePASF_UPTODATECHECK_FILE
    };
    static string GetAnimationSettingsFilename(const string& animationFilename, EPreferredAnimationSettingsFile flag);
    static bool GetDescFromAnimationSettingsFile(SAnimationDesc* pDesc, bool* errorReported, bool* usesNameContains, IPakSystem* pPakSystem, ICryXML* pXmlParser, const string& animationFilename, const std::vector<string>& jointNames);

private:

    string m_AnimationPath;
    string m_AnimationPathWithoutSlash;
    string m_UnifiedAnimationPath;

    static string s_OverrideAnimationSettingsFilename;
};


struct SPlatformAnimationSetup
{
    string m_platform;
    float m_compressionMultiplier;

    SPlatformAnimationSetup()
        : m_compressionMultiplier(1.0f)
    {
    }
};


// Helper class for loading & parsing parameters from .cba file
class CAnimationInfoLoader
{
public:
    CAnimationInfoLoader(ICryXML* pXML);
    ~CAnimationInfoLoader(void);

    bool LoadDescription(const string& name, IPakSystem* pPakSystem);
    const SAnimationDefinition* GetAnimationDefinition(const string& name) const;
    const SPlatformAnimationSetup* GetPlatformSetup(const char* pName) const;

    std::vector<SAnimationDefinition> m_ADefinitions;
    std::vector<SPlatformAnimationSetup> m_platformSetups;
private:

    ICryXML* m_pXML;
};

#endif // CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERPC_CGA_ANIMATIONINFOLOADER_H
