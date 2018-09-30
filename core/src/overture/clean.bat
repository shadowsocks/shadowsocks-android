@ECHO OFF

SET DIR=%~dp0
SET DEPS=%DIR%\.deps

RD /S /Q %DEPS%
RD /S /Q %DIR%\go\bin
RD /S /Q %DIR%\bin

ECHO "Successfully clean overture"
