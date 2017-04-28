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
goto exit
)


if "%1" EQU "test" (
GOTO test
)

@echo Main body

:test

if "%2" EQU "xbox" (
@echo Testing Xbox
goto exit
)

@echo Second section

:xbox


REM Fill out once unit tests, samples, etc are stood up
@echo.
@echo Not yet supported. Check back soon!
@echo.

:exit