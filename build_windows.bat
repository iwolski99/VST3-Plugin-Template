@echo off
REM ===========================================================================
REM  Foundry - one-click Windows build
REM
REM  Double-click this file. It will:
REM    1. check you have CMake, Git and Visual Studio (with C++)
REM    2. download JUCE the first time (a few minutes)
REM    3. build Foundry.vst3 (64-bit, Release)
REM    4. install it to C:\Program Files\Common Files\VST3
REM       (Windows asks for permission for this one step)
REM
REM  No need to "Run as Administrator".
REM  Logs:  build-logs\configure-log.txt, build-logs\build-log.txt
REM  If something fails, build-logs\errors.txt has the errors and this window
REM  explains what went wrong.
REM
REM  Options:  build_windows.bat noinstall   build only, don't install
REM            build_windows.bat clean       wipe the build folder first
REM            build_windows.bat nopause     don't wait for a key at the end
REM ===========================================================================
setlocal EnableExtensions EnableDelayedExpansion

REM Always work from the folder this script is in, wherever it was started from.
cd /d "%~dp0"

set "DO_INSTALL=1"
set "DO_CLEAN=0"
set "DO_PAUSE=1"
for %%A in (%*) do (
    if /i "%%~A"=="noinstall" set "DO_INSTALL=0"
    if /i "%%~A"=="clean"     set "DO_CLEAN=1"
    if /i "%%~A"=="nopause"   set "DO_PAUSE=0"
)

set "LOGDIR=%CD%\build-logs"
if not exist "%LOGDIR%" mkdir "%LOGDIR%"
set "CONFIG_LOG=%LOGDIR%\configure-log.txt"
set "BUILD_LOG=%LOGDIR%\build-log.txt"
set "ERRORS=%LOGDIR%\errors.txt"
del /q "%CONFIG_LOG%" "%BUILD_LOG%" "%ERRORS%" 2>nul

set "EXITCODE=0"
set "STAGE=checks"

echo.
echo ==============================================================
echo   Building Foundry (VST3)
echo ==============================================================

REM ---------------------------------------------------------------- checks
where cmake >nul 2>nul
if errorlevel 1 (
    set "WHY=CMake is not installed, or it is not on your PATH."
    set "FIX=Install CMake from https://cmake.org/download/ and choose 'Add CMake to the system PATH'. Then close this window and run the script again."
    goto :failed
)

if not exist "build\_deps\juce-src\CMakeLists.txt" (
    where git >nul 2>nul
    if errorlevel 1 (
        set "WHY=Git is not installed. The first build needs it to download JUCE."
        set "FIX=Install Git from https://git-scm.com/download/win (the default options are fine), then run the script again."
        goto :failed
    )
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    set "WHY=Visual Studio was not found."
    set "FIX=Install Visual Studio 2022 Community (free) from https://visualstudio.microsoft.com/ and tick the 'Desktop development with C++' workload."
    goto :failed
)

set "VSVER="
for /f "usebackq delims=" %%V in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion`) do set "VSVER=%%V"
if not defined VSVER (
    set "WHY=Visual Studio is installed, but without the C++ tools."
    set "FIX=Open the Visual Studio Installer, click Modify, tick 'Desktop development with C++' and install it."
    goto :failed
)

for /f "tokens=1 delims=." %%M in ("%VSVER%") do set "VSMAJOR=%%M"
set "GEN="
if "%VSMAJOR%"=="16" set "GEN=Visual Studio 16 2019"
if "%VSMAJOR%"=="17" set "GEN=Visual Studio 17 2022"
if "%VSMAJOR%"=="18" set "GEN=Visual Studio 18 2026"
if not defined GEN (
    set "WHY=Unsupported Visual Studio version %VSVER%."
    set "FIX=Install Visual Studio 2022 with 'Desktop development with C++'."
    goto :failed
)
echo   Visual Studio: %GEN%  ^(%VSVER%^)

REM -------------------------------------------------- stale / clean build dir
if "%DO_CLEAN%"=="1" (
    echo   Cleaning the build folder...
    if exist build rmdir /s /q build
)

REM A build folder made by a different Visual Studio (or on another machine)
REM can't be reused; drop its settings but keep the downloaded JUCE.
if exist "build\CMakeCache.txt" (
    findstr /c:"CMAKE_GENERATOR:INTERNAL=%GEN%" "build\CMakeCache.txt" >nul 2>nul
    if errorlevel 1 (
        echo   The build folder was set up with different tools - resetting it.
        del /q "build\CMakeCache.txt" 2>nul
        if exist "build\CMakeFiles" rmdir /s /q "build\CMakeFiles"
    )
)

REM ------------------------------------------------------------- configure
set "STAGE=configure"
echo.
echo [1/3] Configuring  ^(the first run downloads JUCE - this can take a few minutes^)
cmake -B build -G "%GEN%" -A x64 > "%CONFIG_LOG%" 2>&1
if errorlevel 1 goto :diagnose

REM ----------------------------------------------------------------- build
set "STAGE=build"
echo.
echo [2/3] Building Foundry.vst3  ^(5-25 minutes the first time, much faster after^)
echo.
cmake --build build --config Release --target Foundry_VST3 -- /m /nologo /v:minimal /fl "/flp:logfile=%BUILD_LOG%;verbosity=normal"
if errorlevel 1 goto :diagnose

set "SRC=%CD%\build\Foundry_artefacts\Release\VST3\Foundry.vst3"
if not exist "%SRC%" (
    set "WHY=The build said it succeeded, but Foundry.vst3 is missing."
    set "FIX=Run 'build_windows.bat clean' to rebuild from scratch."
    goto :failed
)

REM --------------------------------------------------------------- install
if "%DO_INSTALL%"=="0" (
    echo.
    echo [3/3] Skipping install ^(noinstall^). The plugin is at:
    echo       !SRC!
    goto :success
)

set "STAGE=install"
set "DESTDIR=%CommonProgramFiles%\VST3"
set "DEST=%DESTDIR%\Foundry.vst3"
echo.
echo [3/3] Installing to %DESTDIR%

tasklist /fi "imagename eq FL64.exe" 2>nul | find /i "FL64.exe" >nul
if not errorlevel 1 echo   Note: FL Studio is open. If it has Foundry loaded, the install can't replace it - close FL first.

REM Try without admin rights first (works if you already have write access).
robocopy "%SRC%" "%DEST%" /MIR /R:1 /W:1 /NFL /NDL /NJH /NJS /NP >nul 2>nul
if %errorlevel% lss 8 goto :installed

echo   Windows needs your permission to write to Program Files - accept the prompt.
set "RC_SRC=%SRC%"
set "RC_DEST=%DEST%"
powershell -NoProfile -ExecutionPolicy Bypass -Command "try { $p = Start-Process robocopy -ArgumentList ('\"' + $env:RC_SRC + '\" \"' + $env:RC_DEST + '\" /MIR /R:1 /W:1 /NFL /NDL /NJH /NJS /NP') -Verb RunAs -Wait -PassThru -WindowStyle Hidden; exit $p.ExitCode } catch { exit 99 }"
set "RC=%errorlevel%"
if "%RC%"=="99" (
    set "WHY=The install was cancelled (the permission prompt was declined)."
    set "FIX=Run the script again and click Yes, or copy this folder yourself into !DESTDIR!:  !SRC!"
    goto :failed
)
if %RC% geq 8 (
    set "WHY=Could not copy Foundry.vst3 into !DESTDIR!."
    set "FIX=Close FL Studio (or any DAW using Foundry) and run the script again. Or copy this folder yourself into !DESTDIR!:  !SRC!"
    goto :failed
)

:installed
echo   Installed: %DEST%

:success
echo.
echo ==============================================================
echo   SUCCESS - Foundry built.
if "%DO_INSTALL%"=="1" echo   In FL Studio: Options ^> Manage plugins ^> Find installed plugins,
if "%DO_INSTALL%"=="1" echo   then load Foundry on an instrument channel.
echo ==============================================================
goto :end

REM ===========================================================================
REM  Failure analysis: look for known problems in the logs and say what to do.
REM ===========================================================================
:diagnose
set "LOG=%CONFIG_LOG%"
if "%STAGE%"=="build" set "LOG=%BUILD_LOG%"
if not exist "%LOG%" set "LOG=%CONFIG_LOG%"

REM Collect the actual error lines into errors.txt.
findstr /i /r /c:"error [A-Z][A-Z]*[0-9][0-9]*:" /c:"fatal error" /c:"CMake Error" /c:"error:" "%CONFIG_LOG%" "%BUILD_LOG%" > "%ERRORS%" 2>nul

set "WHY=The %STAGE% step failed."
set "FIX=See the errors below and the full log in %LOG%."

call :match "Could not create named generator"       "Your CMake is too old for %GEN%."  "Update CMake from https://cmake.org/download/ and run the script again."
call :match "No CMAKE_CXX_COMPILER could be found"     "Visual Studio's C++ compiler could not be found."  "Open the Visual Studio Installer, Modify, and tick 'Desktop development with C++'."
call :match "could not find any instance of Visual Studio" "CMake could not find Visual Studio."  "Install Visual Studio 2022 with 'Desktop development with C++', then run again."
call :match "Failed to clone repository"               "Downloading JUCE failed."  "Check your internet connection (or firewall / antivirus blocking git), then run again."
call :match "Could not resolve host"                   "No internet connection while downloading JUCE."  "Connect to the internet and run the script again (only the first build needs it)."
call :match "does not match the generator used previously" "The build folder belongs to a different Visual Studio."  "Run 'build_windows.bat clean'."
call :match "LNK1104"                                  "A file the build writes is locked - usually Foundry.vst3 loaded in FL Studio."  "Close FL Studio (and any other DAW), then run the script again."
call :match "being used by another process"            "A file is locked by another program - usually FL Studio with Foundry loaded."  "Close FL Studio (and any other DAW), then run the script again."
call :match "out of heap space"                        "The compiler ran out of memory."  "Close other programs and run the script again (it resumes where it stopped)."
call :match "C1060"                                    "The compiler ran out of memory."  "Close other programs and run the script again (it resumes where it stopped)."
call :match "not enough space on the disk"             "The disk is full."  "Free up a few GB (the build needs about 3 GB) and run again."
call :match "error C"                                  "Foundry's code failed to compile."  "Send the errors below (or build-logs\errors.txt) so the code can be fixed."

:failed
set "EXITCODE=1"
echo.
echo ==============================================================
echo   BUILD FAILED  ^(during: %STAGE%^)
echo ==============================================================
echo.
echo   What went wrong:  !WHY!
echo   How to fix it:    !FIX!
echo.
if exist "%ERRORS%" (
    for %%S in ("%ERRORS%") do if %%~zS gtr 0 (
        echo   First errors ^(all of them are in build-logs\errors.txt^):
        echo   --------------------------------------------------------
        set /a SHOWN=0
        for /f "usebackq delims=" %%L in ("%ERRORS%") do (
            if !SHOWN! lss 12 echo   %%L
            set /a SHOWN+=1
        )
        echo   --------------------------------------------------------
    )
)
echo   Logs are in: %LOGDIR%
echo.

:end
if "%DO_PAUSE%"=="1" (
    echo Press any key to close this window.
    pause >nul
)
exit /b %EXITCODE%

REM ---------------------------------------------------------------------------
REM :match "text to look for" "what went wrong" "how to fix it"
REM The first match wins (later, more generic patterns don't overwrite it).
REM ---------------------------------------------------------------------------
:match
if defined MATCHED exit /b 0
findstr /i /c:%1 "%CONFIG_LOG%" "%BUILD_LOG%" >nul 2>nul
if errorlevel 1 exit /b 0
set "MATCHED=1"
set "WHY=%~2"
set "FIX=%~3"
exit /b 0
