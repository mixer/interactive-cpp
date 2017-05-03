if "%1" == "local" goto testlocal
goto start
:testlocal
set TFS_DropLocation=c:\test
mkdir %TFS_DropLocation%
rmdir /s /q %TFS_DropLocation%\SDK
mkdir %TFS_DropLocation%
set TFS_VersionNumber=1701.10000
set TFS_SourcesDirectory=%CD%\..\..
goto serializeForPostbuild

:start
if "%XES_SERIALPOSTBUILDREADY%" == "True" goto serializeForPostbuild
goto done
:serializeForPostbuild

echo Running postBuildScript.cmd
echo on

setlocal
call %TFS_SourcesDirectory%\Utilities\VSOBuildScripts\setBuildVersion.cmd

rem format release numbers
for /f "tokens=2 delims==" %%G in ('wmic os get localdatetime /value') do set datetime=%%G
set DATETIME_YEAR=%datetime:~0,4%
set DATETIME_MONTH=%datetime:~4,2%
set DATETIME_DAY=%datetime:~6,2%

FOR /F "TOKENS=1 eol=/ DELIMS=. " %%A IN ("%TFS_VersionNumber%") DO SET SDK_POINT_NAME_YEARMONTH=%%A
FOR /F "TOKENS=2 eol=/ DELIMS=. " %%A IN ("%TFS_VersionNumber%") DO SET SDK_POINT_NAME_DAYVER=%%A
set SDK_POINT_NAME_YEAR=%DATETIME_YEAR%
set SDK_POINT_NAME_MONTH=%DATETIME_MONTH%
set SDK_POINT_NAME_DAY=%DATETIME_DAY%
set SDK_POINT_NAME_VER=%SDK_POINT_NAME_DAYVER:~2,9%
set SDK_RELEASE_NAME=%SDK_RELEASE_YEAR:~2,2%%SDK_RELEASE_MONTH%
set LONG_SDK_RELEASE_NAME=%SDK_RELEASE_NAME%-%SDK_POINT_NAME_YEAR%%SDK_POINT_NAME_MONTH%%SDK_POINT_NAME_DAY%-%SDK_POINT_NAME_VER%
set NUGET_VERSION_NUMBER=%SDK_RELEASE_YEAR%.%SDK_RELEASE_MONTH%.%SDK_POINT_NAME_YEAR%%SDK_POINT_NAME_MONTH%%SDK_POINT_NAME_DAY%.%SDK_POINT_NAME_VER%

set

xcopy %TFS_SourcesDirectory%\Utilities\Nuget\* %TFS_DropLocation%\Nuget\ /s /e /q /y
robocopy /NJS /NJH /MT:16 /S /NP %TFS_SourcesDirectory%\Include %TFS_DropLocation%\Include
robocopy /NJS /NJH /MT:16 /S /NP %TFS_SourcesDirectory%\External\cpprestsdk\Release\include %TFS_DropLocation%\include

rem copy NuGetPackages to build output folder
mkdir %TFS_DropLocation%\SDK\Binaries

\\scratch2\scratch\jasonsa\tools\nuget pack %TFS_DropLocation%\Nuget\Microsoft.Xbox.Services.Beam.Cpp.UWP.nuspec -BasePath %TFS_DropLocation% -OutputDirectory %TFS_DropLocation% -Verbosity normal -version %NUGET_VERSION_NUMBER%

mkdir %TFS_DropLocation%\NuGetBinaries
move %TFS_DropLocation%\*.nupkg %TFS_DropLocation%\NuGetBinaries
copy %XDK_BUILD_SHARE%\NuGetBinaries\*.nupkg %TFS_DropLocation%\SDK\Binaries

:finalize
if "%1" == "local" goto skipEmail
set MSGTITLE="BUILD: %BUILD_SOURCEVERSIONAUTHOR% %BUILD_DEFINITIONNAME% %BUILD_SOURCEBRANCH% = %agent.jobstatus%"
set MSGBODY="%TFS_DROPLOCATION%    https://microsoft.visualstudio.com/OS/_build/index?buildId=%BUILD_BUILDID%&_a=summary"
call \\scratch2\scratch\jasonsa\tools\send-build-email.cmd %MSGTITLE% %MSGBODY% 
:skipEmail

echo.
echo Done postBuildScript.cmd
echo.
endlocal

:done
