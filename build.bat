@echo off
setlocal enabledelayedexpansion

rem ------------------------------ How to use ------------------------------
rem   build.bat              build unsandboxed
rem   build.bat sandboxed    build stock lib_base (no bypass)

rem ------------------------------ constants ------------------------------
rem same version as openmw 0.51:
set "LUAJIT_COMMIT=707c12bf00dafdfd3899b1a6c36435dbbf6c7022"

rem relative paths:
set "ROOT=%~dp0"
set "CLONE=%ROOT%luajit"
set "SRC=%CLONE%\src"

rem ARGUMENT: sandboxed?
set "SANDBOX=unsandboxed"
if /i "%~1"=="sandboxed" set "SANDBOX=sandboxed"
if /i "%~1"=="unsandboxed" set "SANDBOX=unsandboxed"
echo [build] sandbox mode: %SANDBOX%

rem ------------------------------ locate msvc ------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
	echo [error] vswhere not found - is visual studio installed?
	exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
	echo [error] no msvc x64 toolset found
	exit /b 1
)
echo [build] msvc: %VSPATH%

rem ------------------------------ clone ------------------------------
if exist "%CLONE%" (
	echo [build] removing existing clone
	rmdir /s /q "%CLONE%"
)
echo [build] cloning luajit
git clone --branch v2.1 https://github.com/LuaJIT/LuaJIT.git "%CLONE%"
if errorlevel 1 (
	echo [error] clone failed
	exit /b 1
)
git -C "%CLONE%" checkout "%LUAJIT_COMMIT%"
if errorlevel 1 (
	echo [error] checkout failed
	exit /b 1
)

rem ------------------------------ patch ------------------------------
if /i "%SANDBOX%"=="unsandboxed" (
	echo [build] applying unsandbox patch
	copy /Y "%ROOT%patch\lib_base.c" "%SRC%\lib_base.c" >nul
)
echo [build] applying gc stage source
copy /Y "%ROOT%patch\gc\*.c" "%SRC%\" >nul
copy /Y "%ROOT%patch\gc\*.h" "%SRC%\" >nul

rem ------------------------------ msvc environment ------------------------------
set "VSCMD_START_DIR=%SRC%"
call "%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 (
	echo [error] vcvarsall failed
	exit /b 1
)
set "NoDefaultCurrentDirectoryInExePath="

rem ------------------------------ build ------------------------------
echo [build] compiling in %CD%
cd /d "%SRC%"
call msvcbuild.bat
if errorlevel 1 (
	echo [error] build failed
	exit /b 1
)

rem ------------------------------ verification ------------------------------
if /i "%SANDBOX%"=="unsandboxed" (
	luajit.exe -e "print(_VERSION); local S = select('sandbox.bypass'); print(type(S.ffi), type(S.shared))"
) else (
	luajit.exe -e "print(_VERSION)"
)

echo.
echo [build] done in %SRC%
endlocal
