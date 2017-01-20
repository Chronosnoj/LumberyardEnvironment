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
REM BuildShaderPak_ES3 shaderlist.txt gameProjectName

if [%1] == [] GOTO MissingRequiredParam
if [%2] == [] GOTO MissingRequiredParam
if [%3] == [] GOTO MissingRequiredParam
GOTO SetParams

:MissingRequiredParam
echo Missing one or more required arguments. Example: BuildShaderPak_ES3.bat GLES3_1 shaderlist.txt gameProjectName
GOTO Failed

:SetParams
set SHADERFLAVOR=%1
set SOURCESHADERLIST=%2
set GAMENAME=%3

xcopy /y %SOURCESHADERLIST% Cache\%GAMENAME%\es3\user\Cache\Shaders\shaderlist.txt*
call .\Tools\PakShaders.bat %SHADERFLAVOR% %GAMENAME% es3

GOTO Success

:Failed
echo ---- process failed ----
exit /b 1

:Success
echo ---- Process succeeded ----
exit /b 0
