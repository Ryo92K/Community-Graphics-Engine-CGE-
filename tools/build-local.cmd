@echo off
where cl.exe >nul 2>nul
if not errorlevel 1 goto configured
set "CGE_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%CGE_VSWHERE%" exit /b 1
for /f "usebackq delims=" %%i in (`"%CGE_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "CGE_VS=%%i"
if not defined CGE_VS exit /b 1
call "%CGE_VS%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b 1
:configured
cmake -S "%~dp0.." -B "%~dp0..\build" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
if errorlevel 1 exit /b 1
cmake --build "%~dp0..\build"
if errorlevel 1 exit /b 1
ctest --test-dir "%~dp0..\build" --output-on-failure
if errorlevel 1 exit /b 1
cmake --install "%~dp0..\build" --prefix "%~dp0..\dist-skse"
if errorlevel 1 exit /b 1
pwsh -NoProfile -File "%~dp0Prepare-Test.ps1"

