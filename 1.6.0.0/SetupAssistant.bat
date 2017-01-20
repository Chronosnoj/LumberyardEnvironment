SET launchOverrideCommand=%1
SET launchOverrideValue=%2
SET workingDir=".\dev\Bin64\"
IF NOT "%1"=="" (
    IF "%1"=="-workingDir" (
        SET workingDir=%2
    )
)
REM Some tooling, like the installer, may pass in extra slashes.
SET workingDirStrippedBack=%workingDir:\\=\%
SET workingDirStripped=%workingDirStrippedBack://=/%
start /d %workingDirStripped% SetupAssistant.exe
