@ECHO OFF
SETLOCAL
SET SPATH=%~dp0
@ECHO ON
rd /S /Q "%SPATH%Debug"
rd /S /Q "%SPATH%ipch"
rd /S /Q "%SPATH%Release"
rd /S /Q "%SPATH%luaexit\Debug"
rd /S /Q "%SPATH%luaexit\Release"
rd /S /Q "%SPATH%darksidesync\Debug"
rd /S /Q "%SPATH%darksidesync\Release"
del /S /Q "%SPATH%darksidesync\*.o"
@ECHO OFF
pause



