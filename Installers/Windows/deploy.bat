:::
::: Build Release and Installer subdirectories of the current shadow build directory.
::: The Release directory contains everything needed to run OSCAR.
::: The Installer directory contains a single executable -- the OSCAR installer.
:::
::: DEPLOY.BAT should be run as the last step of a QT Build Release kit.
::: QT Shadow build should be specified (this is the QT default).
::: A command line parameter is optional.  Any text on the command line will be appended
::: to the file name of the installer.
:::
::: Requirements:
:::     Inno Setup - http://www.jrsoftware.org/isinfo.php, installed to default Program Files (x86) location
:::     gawk - somewhere in the PATH or in Git for Windows installed in its default lolcation
:::
::: Deploy.bat resides in .../OSCAR-code/Installers/Windows, along with
:::     buildinstall.iss -- script for Inno Setup to create installer
:::     getBuildInfo.awk -- gawk script for extracting version fields from various files
:::     setup.ico -- Icon to be used for the installer.    
:::
::: When building a release version in QT, QT will start this batch file which will prepare
::: additional files, run WinDeployQt, and create an installer.
:::
@echo off
setlocal

::: toolDir is where the Windows install/deploy tools are located
set toolDir=%~dp0
:::echo tooldir is %toolDir%

::: Set shadowBuildDir and sourceDir for oscar_qt.pro vs oscar\oscar.pro projects
if exist ..\oscar-code\oscar\oscar.pro (
    ::: oscar_QT.pro
    set sourceDir=..\oscar-code\oscar
    set shadowBuildDir=%cd%\oscar
) else (
    ::: oscar\oscar.pro
    set sourceDir=..\oscar
    set shadowBuildDir=%cd%
)    
echo sourceDir is %sourceDir%
echo shadowBuildDir is %shadowBuildDir%

::: Now copy the base installation control file

copy %toolDir%buildInstall.iss %shadowBuildDir% || exit 45
copy %toolDir%setup.ico %shadowBuildDir% || exit 46

:::
::: If gawk.exe is in the PATH, use it.  If not, add Git mingw tools (awk) to path. They cannot
::: be added to global path as they break CMD.exe (find etc.)
where /q gawk.exe || set PATH=%PATH%;%ProgramW6432%\Git\usr\bin

::: Create and copy installer control files
::: Create file with all version numbers etc. ready for use by buildinstall.iss
::: Extract the version info from various header files using gawk and
::: #define fields for each data item in buildinfo.iss which will be 
::: used by the installer script.
where /q gawk.exe || set PATH=%PATH%;%ProgramW6432%\Git\usr\bin
echo ; This script auto-generated by DEPLOY.BAT >%shadowBuildDir%\buildinfo.iss
gawk -f %toolDir%getBuildInfo.awk %sourcedir%\version.h >>%shadowBuildDir%\buildInfo.iss || exit 60
gawk -f %toolDir%getBuildInfo.awk %sourcedir%\build_number.h >>%shadowBuildDir%\buildInfo.iss || exit 61
gawk -f %toolDir%getBuildInfo.awk %sourcedir%\git_info.h >>%shadowBuildDir%\buildInfo.iss || exit 62
echo %shadowBuildDir% | gawk -f %toolDir%getBuildInfo.awk >>%shadowBuildDir%\buildInfo.iss || exit 63
echo #define MySuffix "%1" >>%shadowBuildDir%\buildinfo.iss || exit 64
:::echo #define MySourceDir "%sourcedir%" >>%shadowBuildDir%\buildinfo.iss

::: Create Release directory and subdirectories
if exist %shadowBuildDir%\Release\*.* rmdir /s /q %shadowBuildDir%\Release
mkdir %shadowBuildDir%\Release
cd %shadowBuildDir%\Release
copy %shadowBuildDir%\oscar.exe .  || exit 71

::: Now in Release subdirectory
::: If QT created a help directory, copy it.  But it might not have if "helpless" option was set
if exist Help\*.* rmdir /s /q Help
mkdir Help
if exist ..\help\*.qch copy ..\help Help

if exist Html\*.* rmdir /s /q Html
mkdir Html
copy ..\html html  || exit 80

if exist Translations\*.* rmdir /s /q Translations
mkdir Translations
copy ..\translations Translations || exit 84

::: Run deployment tool
windeployqt.exe --release --force --compiler-runtime OSCAR.exe || exit 87

::: Clean up unwanted translation files
:::For unknown reasons, Win64 build copies the .ts files into the release directory, while Win32 does not
if exist Translations\*.ts del /q Translations\*.ts

::: Create installer
cd ..
:::"%ProgramFiles(x86)%\Inno Setup 5\compil32" /cc BuildInstall.iss
"%ProgramFiles(x86)%\Inno Setup 5\iscc" /Q BuildInstall.iss