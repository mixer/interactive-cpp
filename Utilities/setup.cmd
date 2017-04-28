@echo off

REM This script is meant to be run on a freshly cloned repo.
REM It pulls in and builds dependencies to set up the state
REM of the repo for development.


REM if no BEAMROOT, run the dev env command
REM make sure we're up to date
REM make sure submodules are up to date and initialized
REM make sure submodules are built
REM make sure beam source is built
REM make sure beam tests are built
REM print out a nice message!