@echo off
REM ---------------------------------------------------------------------------
REM  Builds Foundry.vst3 (64-bit, Release) with Visual Studio 2022 and copies
REM  it into the default Windows VST3 folder:
REM      C:\Program Files\Common Files\VST3\
REM
REM  First configure downloads JUCE automatically.
REM  Copying into Program Files usually needs this script run as Administrator.
REM ---------------------------------------------------------------------------
setlocal EnableExtensions

where cmake >nul 2>nul
if errorlevel 1 (
    echo.
    echo CMake was not found on PATH.
    echo Install it from https://cmake.org/download/ and tick "Add to PATH".
    echo.
    exit /b 1
)

echo.
echo === Configuring Foundry ===
cmake -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :failed

echo.
echo === Building Foundry VST3 (Release x64) ===
cmake --build build --config Release --target Foundry_VST3
if errorlevel 1 goto :failed

set "SRC=%CD%\build\Foundry_artefacts\Release\VST3\Foundry.vst3"
set "DEST=%COMMONPROGRAMFILES%\VST3\Foundry.vst3"

if not exist "%SRC%" (
    echo.
    echo Build reported success but Foundry.vst3 was not found at:
    echo   %SRC%
    echo.
    exit /b 1
)

echo.
echo === Installing to default VST3 folder ===
echo Source:      %SRC%
echo Destination: %DEST%

if not exist "%COMMONPROGRAMFILES%\VST3" mkdir "%COMMONPROGRAMFILES%\VST3"
if errorlevel 1 (
    echo.
    echo Could not create "%COMMONPROGRAMFILES%\VST3".
    echo Re-run build_windows.bat as Administrator.
    echo The built plugin is still at:
    echo   %SRC%
    echo.
    exit /b 1
)

if exist "%DEST%" rmdir /S /Q "%DEST%"
xcopy /E /I /Y "%SRC%" "%DEST%" >nul
if errorlevel 1 (
    echo.
    echo Copy into the VST3 folder failed. Re-run this script as Administrator,
    echo or copy this folder yourself:
    echo   %SRC%
    echo into:
    echo   %COMMONPROGRAMFILES%\VST3\
    echo then rescan plugins in your DAW.
    echo.
    exit /b 1
)

echo.
echo Done. Foundry.vst3 is installed at:
echo   %DEST%
echo.
echo Rescan plugins in FL Studio / your DAW if it was already open.
echo.
exit /b 0

:failed
echo.
echo Build failed - see the output above.
exit /b 1
