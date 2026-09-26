@echo off
setlocal enabledelayedexpansion
set "ROOT_DIR=%~dp0"
set "ROOT_DIR=%ROOT_DIR:~0,-1%"
set "NO_PAUSE="
if /I "%~1"=="--no-pause" set "NO_PAUSE=1"

echo ============================================
echo  ETS2 Chat Translator - Build
echo ============================================
echo.

set ELECTRON_RUN_AS_NODE=

set "FOUND_VS="
set "VS_BAT="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        if exist "%%I\VC\Auxiliary\Build\vcvars64.bat" (
            set "VS_BAT=%%I\VC\Auxiliary\Build\vcvars64.bat"
            set "FOUND_VS=1"
        )
    )
)

if not defined FOUND_VS if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    set "VS_BAT=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    set "FOUND_VS=1"
)

if not defined FOUND_VS if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VS_BAT=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    set "FOUND_VS=1"
)

if not defined FOUND_VS if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    set "VS_BAT=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    set "FOUND_VS=1"
)

if not defined FOUND_VS if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    set "VS_BAT=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    set "FOUND_VS=1"
)

if not defined FOUND_VS if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VS_BAT=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
    set "FOUND_VS=1"
)

if not defined FOUND_VS (
    echo [ERROR] Visual Studio C++ x64 build tools were not found.
    echo.
    echo Install one of these first:
    echo   - Visual Studio 2022 Build Tools
    echo   - Workload: Desktop development with C++
    echo   - Include Windows 10/11 SDK and MSVC v143 x64/x86 build tools
    echo.
    echo Download: https://visualstudio.microsoft.com/visual-cpp-build-tools/
    if not defined NO_PAUSE pause
    exit /b 1
)

echo [INFO] Using Visual Studio environment:
echo        %VS_BAT%
call "%VS_BAT%" >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to initialize Visual Studio build environment.
    if not defined NO_PAUSE pause
    exit /b 1
)

where cl.exe >nul 2>&1
if not "%ERRORLEVEL%"=="0" (
    echo [ERROR] cl.exe was not found after loading vcvars64.bat.
    echo        Reinstall Visual Studio Build Tools with C++ x64 tools.
    if not defined NO_PAUSE pause
    exit /b 1
)

where npm >nul 2>&1
if not "%ERRORLEVEL%"=="0" (
    echo [ERROR] Node.js/npm was not found.
    echo.
    echo Install Node.js LTS first, then rerun:
    echo   https://nodejs.org/
    if not defined NO_PAUSE pause
    exit /b 1
)

pushd "%ROOT_DIR%"
if not exist build mkdir build

echo [INFO] Building plugin DLL...
cl.exe /nologo /EHsc /O2 /std:c++17 /utf-8 /W3 ^
    /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_WIN64" ^
    /I "include" /I "src" ^
    /Fe"build\ets2_chat_translator.dll" ^
    /Fo"build\\" ^
    src\dllmain.cpp src\app_runtime.cpp src\chat_panel.cpp src\chat_tailer.cpp ^
    src\translate_engine.cpp src\http_agent.cpp src\settings_store.cpp ^
    src\text_codec.cpp src\win_paths.cpp src\truckersmp_bridge.cpp ^
    src\sdk_overlay.cpp src\d3d_overlay.cpp src\game_window.cpp src\overlay_link.cpp ^
    /link /DLL /DEF:exports.def /MACHINE:X64 ^
    user32.lib gdi32.lib winhttp.lib shell32.lib advapi32.lib shlwapi.lib crypt32.lib ^
    d3d11.lib dxgi.lib d3dcompiler.lib

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Plugin build failed.
    popd
    if not defined NO_PAUSE pause
    exit /b 1
)

echo [INFO] Building overlay preview tool...
cl.exe /nologo /EHsc /O2 /std:c++17 /utf-8 /W3 ^
    /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_WIN64" ^
    /I "include" /I "src" ^
    /Fe"build\ets2_chat_translator_preview.exe" ^
    /Fo"build\\" ^
    src\overlay_preview.cpp src\chat_panel.cpp src\text_codec.cpp src\game_window.cpp ^
    /link /SUBSYSTEM:WINDOWS /MACHINE:X64 user32.lib gdi32.lib shell32.lib

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Overlay preview build failed.
    popd
    if not defined NO_PAUSE pause
    exit /b 1
)

echo [INFO] Building JavaScript/Electron manager app...
rem Electron runtime and packaging tools download from GitHub by default, which often
rem times out on restricted networks; point them at mirrors unless the user set one.
if not defined ELECTRON_MIRROR set "ELECTRON_MIRROR=https://npmmirror.com/mirrors/electron/"
if not defined ELECTRON_BUILDER_BINARIES_MIRROR set "ELECTRON_BUILDER_BINARIES_MIRROR=https://npmmirror.com/mirrors/electron-builder-binaries/"
pushd manager
rem Reuse node_modules only when both the packager entry and the Electron binary exist.
if exist "node_modules\.bin\electron-builder.cmd" if exist "node_modules\electron\dist\electron.exe" (
    echo [INFO] Reusing existing manager\node_modules.
) else (
    if exist "package-lock.json" (
        call npm ci
        if not "!ERRORLEVEL!"=="0" (
            echo [WARN] npm install failed with the current registry. Retrying with https://registry.npmjs.org/ ...
            call npm ci --registry=https://registry.npmjs.org/
        )
    ) else (
        call npm install
        if not "!ERRORLEVEL!"=="0" (
            echo [WARN] npm install failed with the current registry. Retrying with https://registry.npmjs.org/ ...
            call npm install --registry=https://registry.npmjs.org/
        )
    )
)
if not "!ERRORLEVEL!"=="0" (
    popd
    popd
    echo [ERROR] npm dependency install failed.
    echo.
    echo Try these commands, then rerun build.bat:
    echo   npm config get registry
    echo   npm config set registry https://registry.npmjs.org/
    echo   npm cache clean --force
    if not defined NO_PAUSE pause
    exit /b 1
)
del /q "dist\ETS2-Chat-Translator-Manager-Setup-*.exe" "dist\ETS2-Chat-Translator-Manager-Setup-*.exe.blockmap" >nul 2>&1
call npm run dist
if not "!ERRORLEVEL!"=="0" (
    popd
    popd
    echo [ERROR] Electron manager build failed.
    if not defined NO_PAUSE pause
    exit /b 1
)
popd
if exist "%ROOT_DIR%\build\ets2_chat_translator_app" rmdir /S /Q "%ROOT_DIR%\build\ets2_chat_translator_app"
xcopy "%ROOT_DIR%\manager\dist\win-unpacked" "%ROOT_DIR%\build\ets2_chat_translator_app" /E /I /Y >nul
if not "!ERRORLEVEL!"=="0" (
    echo [ERROR] Failed to copy Electron manager app folder.
    popd
    if not defined NO_PAUSE pause
    exit /b 1
)
if not exist "%ROOT_DIR%\build\installer" mkdir "%ROOT_DIR%\build\installer"
del /q "%ROOT_DIR%\build\installer\*.exe" >nul 2>&1
for %%F in ("%ROOT_DIR%\manager\dist\ETS2-Chat-Translator-Manager-Setup-*.exe") do copy /Y "%%~fF" "%ROOT_DIR%\build\installer\" >nul
if not exist "%ROOT_DIR%\build\installer\ETS2-Chat-Translator-Manager-Setup-*.exe" (
    echo [ERROR] Failed to copy manager installer.
    popd
    if not defined NO_PAUSE pause
    exit /b 1
)

del /q build\*.obj build\*.exp 2>nul

echo.
echo Build successful:
echo   build\ets2_chat_translator.dll
if exist "build\ets2_chat_translator_preview.exe" echo   build\ets2_chat_translator_preview.exe
if exist "build\ets2_chat_translator_app\ETS2 Chat Translator Manager.exe" echo   build\ets2_chat_translator_app\ETS2 Chat Translator Manager.exe
for %%F in ("build\installer\ETS2-Chat-Translator-Manager-Setup-*.exe") do if exist "%%~fF" echo   %%~fF
echo.
popd
if not defined NO_PAUSE pause
