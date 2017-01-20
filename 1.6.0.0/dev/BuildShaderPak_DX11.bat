@ECHO OFF
REM 
REM All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
REM its licensors.
REM
REM For complete copyright and license terms please see the LICENSE at the root of this
REM distribution (the "License"). All use of this software is governed by the License,
REM or, if provided, by the license below or the license accompanying this file. Do not
REM remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
REM WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
REM
REM Original file Copyright Crytek GMBH or its affiliates, used under license.

REM expected usage:
REM BuildShaderPak_DX11 shaderlist.txt

set SOURCESHADERLIST=%1
set GAMENAME=SamplesProject
set DESTSHADERFOLDER=Cache\%GAMENAME%\PC\user\Cache\Shaders

set SHADERPLATFORM=PC
rem other available platforms are GL4 GLES3 ORBIS DURANGO METAL
rem if changing the above platform, also change the below folder name (D3D11, ORBIS, DURANGO, METAL, GL4, GLES3)
set SHADERFLAVOR=D3D11

if [%1] == [] GOTO MissingShaderParam
GOTO DoCopy

:MissingShaderParam
echo source Shader List not specified, using %DESTSHADERFOLDER%\shaderlist.txt by default 
GOTO DoCompile

:DoCopy
echo Copying "%SOURCESHADERLIST%" to %DESTSHADERFOLDER%\shaderlist.txt
xcopy /y "%SOURCESHADERLIST%" %DESTSHADERFOLDER%\shaderlist.txt*
IF ERRORLEVEL 1 GOTO CopyFailed
GOTO DoCompile

:CopyFailed
echo copy failed.
GOTO Failed

:DoCompile
echo Compiling shaders...
Bin64\ShaderCacheGen.exe /BuildGlobalCache /ShadersPlatform=%SHADERPLATFORM%

echo Packing Shaders...
call .\Tools\PakShaders.bat %SHADERFLAVOR% %GAMENAME%

GOTO Success

:Failed
echo ---- process failed ----
exit /b 1

:Success
echo ---- Process succeeded ----
exit /b 0