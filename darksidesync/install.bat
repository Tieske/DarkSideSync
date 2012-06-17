@echo off
echo off
REM ===================================================
REM This batch files copies the build output to the Lua 
REM for Windows directory, set the path below correct
REM ===================================================
SET T_LUAPATH=C:\Users\Public\lua\5.1

echo Copying file 'darksidesync.dll'
copy "..\debug\darksidesync.dll" "%T_LUAPATH%\clibs"

echo Copying file 'dss.lua'
copy "dss.lua" "%T_LUAPATH%\lua"
