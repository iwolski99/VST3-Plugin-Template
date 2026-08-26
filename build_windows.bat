@echo off
REM ---------------------------------------------------------------------------
REM  Builds PluginTemplate.vst3 (64-bit, Release) with Visual Studio 2022.
REM  JUCE is downloaded automatically on the first configure.
REM
REM  TEMPLATE: after renaming the plugin (see scripts/rename_plugin.py), this
REM  file's PluginTemplate references are updated for you automatically.
REM ---------------------------------------------------------------------------
setlocal

where cmake >nul 2>nul
if errorlevel 1 (
    echo.
    echo CMake was not found on PATH.
    echo Install it from https://cmake.org/download/ and tick "Add to PATH".
    echo.
    exit /b 1
)

echo.
echo === Configuring ===
cmake -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :failed

echo.
echo === Building (Release) ===
cmake --build build --config Release --target PluginTemplate_VST3
if errorlevel 1 goto :failed

echo.
echo Done.
echo Plugin: build\PluginTemplate_artefacts\Release\VST3\PluginTemplate.vst3
echo.
echo It should already be installed to C:\Program Files\Common Files\VST3.
echo If not, copy the PluginTemplate.vst3 folder there yourself, then rescan in your DAW.
echo.
exit /b 0

:failed
echo.
echo Build failed - see the output above.
exit /b 1
