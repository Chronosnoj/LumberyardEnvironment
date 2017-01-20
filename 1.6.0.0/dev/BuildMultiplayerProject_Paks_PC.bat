@echo off

setlocal enabledelayedexpansion 

set ORIGINALDIRECTORY=%cd%
set MYBATCHFILEDIRECTORY=%~dp0
cd "%MYBATCHFILEDIRECTORY%"

echo ----- Processing Assets Using Asset Processor Batch ----
.\bin64\AssetProcessorBatch.exe /gamefolder=MultiplayerProject /platforms=pc
IF ERRORLEVEL 1 GOTO AssetProcessingFailed

echo ----- Creating Packages ----
rem lowercase is intentional, since cache folders are lowercase (for ps4)
.\bin64\rc\rc.exe /job=bin64\rc\RCJob_Generic_MakePaks.xml /p=pc /game=multiplayerproject
IF ERRORLEVEL 1 GOTO RCFailed

echo ----- Done -----
cd "%ORIGINALDIRECTORY%"
exit /b 0

:RCFailed
echo ---- RC PAK failed ----
cd "%ORIGINALDIRECTORY%"
exit /b 1

:AssetProcessingFailed
echo ---- ASSET PROCESSING FAILED ----
cd "%ORIGINALDIRECTORY%"
exit /b 1





