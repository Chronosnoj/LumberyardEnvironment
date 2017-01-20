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

#ifndef CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERIMAGE_PIXELFORMATS_H
#define CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERIMAGE_PIXELFORMATS_H
#pragma once


#include <d3d9.h>                    // WORD (used in d3d9types)
#include <d3d9types.h>               // D3DFORMAT
#include <dxgiformat.h>              // DX10+ formats

#include "platform.h"                // uint32
#include <ITexture.h>                // ETEX_Format
#include <ImageExtensionHelper.h>


enum EPixelFormat
{
    // NOTES:
    // The ARGB format has a memory layout of [B][G][R][A], on modern DX10+ architectures
    // this format became disfavored over ABGR with a memory layout of [R][G][B][A].
    ePixelFormat_A8R8G8B8 = 0,
    ePixelFormat_X8R8G8B8,
    ePixelFormat_R8G8B8,
    ePixelFormat_A8,
    ePixelFormat_L8,
    ePixelFormat_A8L8,

    ePixelFormat_DXT1,
    ePixelFormat_DXT1a,
    ePixelFormat_DXT3,
    ePixelFormat_DXT3t,
    ePixelFormat_DXT5,
    ePixelFormat_DXT5t,
    ePixelFormat_3DC,
    ePixelFormat_3DCp,
    ePixelFormat_CTX1,
    //  Confetti BEGIN: Igor Lobanchikov
    ePixelFormat_ASTC_4x4,
    ePixelFormat_ASTC_5x4,
    ePixelFormat_ASTC_5x5,
    ePixelFormat_ASTC_6x5,
    ePixelFormat_ASTC_6x6,
    ePixelFormat_ASTC_8x5,
    ePixelFormat_ASTC_8x6,
    ePixelFormat_ASTC_8x8,
    ePixelFormat_ASTC_10x5,
    ePixelFormat_ASTC_10x6,
    ePixelFormat_ASTC_10x8,
    ePixelFormat_ASTC_10x10,
    ePixelFormat_ASTC_12x10,
    ePixelFormat_ASTC_12x12,
    //  Confetti End: Igor Lobanchikov
    ePixelFormat_PVRTC2,
    ePixelFormat_PVRTC4,
    ePixelFormat_EAC_R11,
    ePixelFormat_EAC_RG11,
    ePixelFormat_ETC2,
    ePixelFormat_ETC2a,

    ePixelFormat_BC1,
    ePixelFormat_BC1a,
    ePixelFormat_BC2,
    ePixelFormat_BC2t,
    ePixelFormat_BC3,
    ePixelFormat_BC3t,
    ePixelFormat_BC4,
    ePixelFormat_BC4s,
    ePixelFormat_BC5,
    ePixelFormat_BC5s,
    ePixelFormat_BC6UH,
    ePixelFormat_BC7,
    ePixelFormat_BC7t,

    ePixelFormat_E5B9G9R9,

    ePixelFormat_A32B32G32R32F,
    ePixelFormat_G32R32F,
    ePixelFormat_R32F,
    ePixelFormat_A16B16G16R16F,
    ePixelFormat_G16R16F,
    ePixelFormat_R16F,

    ePixelFormat_A16B16G16R16,
    ePixelFormat_G16R16,
    ePixelFormat_R16,

    //  ePixelFormat_A8B8G8R8,
    //  ePixelFormat_X8B8G8R8,
    ePixelFormat_G8R8,
    ePixelFormat_R8,

    ePixelFormat_Count
};

enum ESampleType
{
    eSampleType_Uint8,
    eSampleType_Uint16,
    eSampleType_Half,
    eSampleType_Float,
    eSampleType_Compressed,
};

struct PixelFormatInfo
{
    int         nChannels;
    bool        bHasAlpha;
    const char* szAlpha;
    uint32      minWidth; // Confetti: fixing signed unsigned mismatches
    uint32      minHeight;
    int         blockWidth;
    int         blockHeight;
    int         bitsPerBlock;   //  Igor: bits per pixel for uncompressed
    bool        bSquarePow2;
    D3DFORMAT   d3d9Format;
    DXGI_FORMAT d3d10Format;
    ESampleType eSampleType;
    const char* szName;
    const char* szDescription;
    bool        bCompressed;
    bool        bSelectable;     // shows up in the list of usable destination pixel formats in the dialog window
    ETEX_Format etexFormat;

    PixelFormatInfo()
        : szAlpha(0)
        , bitsPerBlock(-1)
        , d3d9Format(D3DFMT_UNKNOWN)
        , d3d10Format(DXGI_FORMAT_UNKNOWN)
        , szName(0)
        , szDescription(0)
        , etexFormat(eTF_Unknown)
    {
    }

    PixelFormatInfo(
        int         a_bitsPerPixel,
        int         a_Channels,
        bool        a_Alpha,
        const char* a_szAlpha,
        uint32      a_minWidth, // Confetti: fixing signed unsigned mismatches
        uint32      a_minHeight,
        int         a_blockWidth,
        int         a_blockHeight,
        int         a_bitsPerBlock, //  Igor: bits per pixel for uncompressed
        bool        a_bSquarePow2,
        D3DFORMAT   a_d3d9Format,
        DXGI_FORMAT a_d3d10Format,
        ESampleType a_eSampleType,
        const char* a_szName,
        const char* a_szDescription,
        bool        a_bCompressed,
        bool        a_bSelectable,
        ETEX_Format a_etexFormat)
        : nChannels(a_Channels)
        , bHasAlpha(a_Alpha)
        , minWidth(a_minWidth)
        , minHeight(a_minHeight)
        , blockWidth(a_blockWidth)
        , blockHeight(a_blockHeight)
        , bitsPerBlock(a_bitsPerBlock)  //  Igor: bits per pixel for uncompressed
        , bSquarePow2(a_bSquarePow2)
        , szAlpha(a_szAlpha)
        , d3d9Format(a_d3d9Format)
        , d3d10Format(a_d3d10Format)
        , eSampleType(a_eSampleType)
        , szName(a_szName)
        , szDescription(a_szDescription)
        , bCompressed(a_bCompressed)
        , bSelectable(a_bSelectable)
        , etexFormat(a_etexFormat)
    {
        //  Confetti BEGIN: Igor Lobanchikov
        if (a_bitsPerPixel)
        {
            assert(a_bitsPerPixel * blockWidth * blockHeight == bitsPerBlock);
        }
        //  Confetti End: Igor Lobanchikov
    }
};


class ImageObject;

class CPixelFormats
{
public:
    static inline bool IsFormatWithoutAlpha(enum EPixelFormat fmt)
    {
        // all these formats have no alpha-channel at all
        return (fmt == ePixelFormat_DXT1 || fmt == ePixelFormat_BC1);
    }

    static inline bool IsFormatWithThresholdAlpha(enum EPixelFormat fmt)
    {
        // all these formats have a 1bit alpha-channel
        return (fmt == ePixelFormat_DXT1a || fmt == ePixelFormat_BC1a);
    }

    static inline bool IsFormatWithWeightingAlpha(enum EPixelFormat fmt)
    {
        // all these formats use alpha to weight the primary channels
        return (fmt == ePixelFormat_DXT1a || fmt == ePixelFormat_DXT3t || fmt == ePixelFormat_DXT5t || fmt == ePixelFormat_BC1a || fmt == ePixelFormat_BC2t || fmt == ePixelFormat_BC3t || fmt == ePixelFormat_BC7t);
    }

    static inline bool IsFormatSingleChannel(enum EPixelFormat fmt)
    {
        // all these formats have a single channel
        return (fmt == ePixelFormat_A8 || fmt == ePixelFormat_R8 || fmt == ePixelFormat_R16 || fmt == ePixelFormat_R16F || fmt == ePixelFormat_R32F || fmt == ePixelFormat_BC4 || fmt == ePixelFormat_BC4s);
    }

    static inline bool IsFormatSigned(enum EPixelFormat fmt)
    {
        // all these formats contain signed data, the FP-formats contain scale & biased unsigned data
        return (fmt == ePixelFormat_BC4s || fmt == ePixelFormat_BC5s /*|| fmt == ePixelFormat_BC6SH*/);
    }

    static inline bool IsFormatFloatingPoint(enum EPixelFormat fmt, bool bFullPrecision)
    {
        // all these formats contain floating point data
        if (!bFullPrecision)
        {
            return ((fmt == ePixelFormat_R16F || fmt == ePixelFormat_G16R16F || fmt == ePixelFormat_A16B16G16R16F) || (fmt == ePixelFormat_BC6UH || fmt == ePixelFormat_E5B9G9R9));
        }
        else
        {
            return ((fmt == ePixelFormat_R32F || fmt == ePixelFormat_G32R32F || fmt == ePixelFormat_A32B32G32R32F));
        }
    }

    static const PixelFormatInfo* GetPixelFormatInfo(EPixelFormat format);
    static bool GetPixelFormatInfo(EPixelFormat format, ESampleType* pSampleType, int* pChannelCount, bool* pHasAlpha);

    static bool IsPixelFormatWithoutAlpha(EPixelFormat format);
    static bool IsPixelFormatUncompressed(EPixelFormat format);
    static bool IsPixelFormatAnyRGB(EPixelFormat format);
    static bool IsPixelFormatAnyRG(EPixelFormat format);
    static bool IsPixelFormatForExtendedDDSOnly(EPixelFormat format);

    // Note: case is ignored
    // Returns (EPixelFormat)-1 if the name was not recognized
    static EPixelFormat FindPixelFormatByName(const char* name);
    static EPixelFormat FindFinalTextureFormat(EPixelFormat format, bool bAlphaChannelUsed);

    static uint32 ComputeMaxMipCount(EPixelFormat format, uint32 width, uint32 height, bool bCubemap);

    static bool BuildSurfaceHeader(const ImageObject* pImage, uint32 maxMipCount, CImageExtensionHelper::DDS_HEADER& header, bool bForceDX10);
    static bool BuildSurfaceExtendedHeader(const ImageObject* pImage, uint32 sliceCount, CImageExtensionHelper::DDS_HEADER_DXT10& exthead);

    static bool ParseSurfaceHeader(ImageObject* pImage, uint32& maxMipCount, const CImageExtensionHelper::DDS_HEADER& header, bool bForceDX10);
    static bool ParseSurfaceExtendedHeader(ImageObject* pImage, uint32& sliceCount, const CImageExtensionHelper::DDS_HEADER_DXT10& exthead);
};


#define D3DFMT_3DC       ((D3DFORMAT)(MAKEFOURCC('A', 'T', 'I', '2')))   // two channel compressed normal maps 8bit -> 8 bits per pixel
#define D3DFMT_3DCp      ((D3DFORMAT)(MAKEFOURCC('A', 'T', 'I', '1')))   // one channel compressed maps 8bit -> 4 bits per pixel
#define D3DFMT_CTX1      ((D3DFORMAT)(MAKEFOURCC('C', 'T', 'X', '1')))   // two channel compressed normal maps 4bit -> 4 bits per pixel
//  Confetti BEGIN: Igor Lobanchikov
// Igor: ASTC codes aren't official
#define D3DFMT_ASTC4x4   ((D3DFORMAT)(MAKEFOURCC('A', 'S', '4', '4')))   // ASTC texture compression, 8 bits per pixel, block is 4x4 pixels, 128 bits
#define D3DFMT_ASTC5x4   ((D3DFORMAT)(MAKEFOURCC('A', 'S', '5', '4')))   // ASTC texture compression, 6.4 bits per pixel, block is 5x4 pixels, 128 bits
#define D3DFMT_ASTC5x5   ((D3DFORMAT)(MAKEFOURCC('A', 'S', '5', '5')))   // ASTC texture compression, 5.12 bits per pixel, block is 5x5 pixels, 128 bits
#define D3DFMT_ASTC6x5   ((D3DFORMAT)(MAKEFOURCC('A', 'S', '6', '5')))   // ASTC texture compression, 4.27 bits per pixel, block is 6x5 pixels, 128 bits
#define D3DFMT_ASTC6x6   ((D3DFORMAT)(MAKEFOURCC('A', 'S', '6', '6')))   // ASTC texture compression, 3.56 bits per pixel, block is 6x6 pixels, 128 bits
#define D3DFMT_ASTC8x5   ((D3DFORMAT)(MAKEFOURCC('A', 'S', '8', '5')))   // ASTC texture compression, 3.2 bits per pixel, block is 8x5 pixels, 128 bits
#define D3DFMT_ASTC8x6   ((D3DFORMAT)(MAKEFOURCC('A', 'S', '8', '6')))   // ASTC texture compression, 2.67 bits per pixel, block is 8x6 pixels, 128 bits
#define D3DFMT_ASTC10x5  ((D3DFORMAT)(MAKEFOURCC('A', 'S', 'A', '5')))   // ASTC texture compression, 2.56 bits per pixel, block is 10x5 pixels, 128 bits
#define D3DFMT_ASTC10x6  ((D3DFORMAT)(MAKEFOURCC('A', 'S', 'A', '6')))   // ASTC texture compression, 2.13 bits per pixel, block is 10x6 pixels, 128 bits
#define D3DFMT_ASTC8x8   ((D3DFORMAT)(MAKEFOURCC('A', 'S', '8', '8')))   // ASTC texture compression, 2.00 bits per pixel, block is 8x8 pixels, 128 bits
#define D3DFMT_ASTC10x8  ((D3DFORMAT)(MAKEFOURCC('A', 'S', 'A', '8')))   // ASTC texture compression, 1.6 bits per pixel, block is 10x8 pixels, 128 bits
#define D3DFMT_ASTC10x10 ((D3DFORMAT)(MAKEFOURCC('A', 'S', 'A', 'A')))   // ASTC texture compression, 1.28 bits per pixel, block is 10x10 pixels, 128 bits
#define D3DFMT_ASTC12x10 ((D3DFORMAT)(MAKEFOURCC('A', 'S', 'C', 'A')))   // ASTC texture compression, 1.07 bits per pixel, block is 12x10 pixels, 128 bits
#define D3DFMT_ASTC12x12 ((D3DFORMAT)(MAKEFOURCC('A', 'S', 'C', 'C')))   // ASTC texture compression, 0.89 bits per pixel, block is 12x10 pixels, 128 bits
#define D3DFMT_PVRTC2    ((D3DFORMAT)(MAKEFOURCC('P', 'V', 'R', '2')))   // POWERVR texture compression, 2 bits per pixel, block is 8x4 pixels, 64 bits
#define D3DFMT_PVRTC4    ((D3DFORMAT)(MAKEFOURCC('P', 'V', 'R', '4')))   // POWERVR texture compression, 4 bits per pixel, block is 4x4 pixels, 64 bits
//  Confetti End: Igor Lobanchikov
// Sokov: ETC2/EAC fourcc codes below are not official, I made them by myself. Feel free to replace them by better codes.
#define D3DFMT_ETC2      ((D3DFORMAT)(MAKEFOURCC('E', 'T', '2', ' ')))   // ETC2 RGB texture compression, 4 bits per pixel, block is 4x4 pixels, 64 bits
#define D3DFMT_ETC2a     ((D3DFORMAT)(MAKEFOURCC('E', 'T', '2', 'A')))   // ETC2 RGBA texture compression, 8 bits per pixel, block is 4x4 pixels, 128 bits
#define D3DFMT_EAC_R11   ((D3DFORMAT)(MAKEFOURCC('E', 'A', 'R', ' ')))   // EAC one channel texture compression, 4 bits per pixel, block is 4x4 pixels, 64 bits
#define D3DFMT_EAC_RG11  ((D3DFORMAT)(MAKEFOURCC('E', 'A', 'R', 'G')))   // EAC two channel texture compression, 4 bits per pixel, block is 4x4 pixels, 128 bits
#define D3DFMT_DX10      ((D3DFORMAT)(MAKEFOURCC('D', 'X', '1', '0')))   // DirectX 10+ header

#endif // CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERIMAGE_PIXELFORMATS_H
