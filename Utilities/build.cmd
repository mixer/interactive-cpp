@echo off

if NOT DEFINED BEAMROOT (
@echo.
@echo Please set up an environment, e.g. run github-beam-env.cmd
@echo.
@echo If this is your first time setting up the Xbox Live Beam
@echo repo, please edit the scripts as appropriate for your
@echo local machine.
@echo.
@echo Refer to utilHelp.cmd or the readme for more information.
@echo.
)

if "%1" EQU "help" (
GOTO help
)

if "%1" EQU "xbox" (
GOTO xbox
)

@echo.
@echo This script provides assistance building the Xbox
@echo Live Beam SDK. Feel free to modify and adapt as needed.
@echo.
@echo For a specific example, try running "build help xbox"
@echo to learn about building for the Xbox platform.
@echo.

goto exit

:help

if "%2" EQU "xbox" (
@echo.
@echo To build the release flavor for the Xbox platform, run
@echo build xbox
@echo.
@echo To build the debug flavor for the Xbox platform, run
@echo build xbox debug
@echo.
goto exit
)

if "%2" EQU "uwp" (
@echo.
@echo To build the release flavor for the UWP platform, run
@echo build uwp
@echo.
@echo To build the debug flavor for the UWP platform, run
@echo build uwp debug
@echo.
goto exit
)

if "%2" EQU "full" (
goto :full
)

@echo This script simplifies building from the command line.
@echo You can build for a specific platform and/or flavor by
@echo specifying targets. For example:
@echo    build uwp debug
@echo will build the debug flavor of project for the UWP platform.
@echo.
@echo For a full list of options, run "build help full"

goto exit

:full

@echo.
@echo The full list of options:
@echo.
@echo     This is not yet complete. Check back soon!
@echo.


:xbox

@echo.
@echo This is not yet complete. Check back soon!
@echo.

REM .\GenSDKBuildCppFile\GenSDKBuildCppFile.exe %1 %1\Build\Microsoft.Xbox.Services.110.XDK.Cpp\build.cpp xbox
goto exit

:uwp

@echo.
@echo This is not yet complete. Check back soon!
@echo.

REM .\GenSDKBuildCppFile\GenSDKBuildCppFile.exe %1 %1\Build\Microsoft.Xbox.Services.140.UWP.Cpp\build.cpp uwp
goto exit

:exit