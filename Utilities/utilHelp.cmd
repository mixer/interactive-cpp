@echo off

setlocal
set ME=%~n0

if NOT DEFINED BEAMROOT (
@echo.
@echo Please set up an environment, e.g. run github-beam-env.cmd
@echo.
@echo If this is your first time setting up the Xbox Live Beam
@echo repo, please edit the scripts as appropriate for your
@echo local machine.
goto exit
)

REM direct help to specific scripts
if "%1" EQU "env" (
GOTO env
)

if "%1" EQU "build" (
GOTO build
)

if "%1" EQU "test" (
GOTO build
)

REM catch all other cases
if "%1" NEQ "" (
GOTO help
)

REM if nothing defined, default description
@echo.
@echo This script provides reference and help using
@echo supplied utilities and helper scripts for managing
@echo git, environment and general dev fun. Feel free to
@echo modify and adapt as needed.
@echo.
@echo For a specific example, try running "utilHelp env"
@echo to learn about the default environment variables
@echo and setup.
@echo.

goto exit

:help
@echo.
@echo For help with setting up your environment, run "utilHelp env"
@echo For help with building source, run "utilHelp build"
@echo For help with running tests, run "utilHelp test"

goto exit

:env
%ENVUTIL%\%ENVSCRIPT% help
goto exit

:build
%ENVUTIL%\%BUILDSCRIPT% help
goto exit

:test
%ENVUTIL%\%TESTSCRIPT% help
goto exit

:exit