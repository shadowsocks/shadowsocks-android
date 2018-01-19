@ECHO OFF

SET DIR=%CD%
SET DEPS=%DIR%\.deps

RD /SQ %DEPS%
RD /SQ %DIR%\go\bin
RD /SQ %DIR%\bin

ECHO "Successfully clean overture"
