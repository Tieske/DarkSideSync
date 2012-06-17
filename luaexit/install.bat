@echo off
echo off
REM ===================================================
REM This batch files copies the build output to the Lua 
REM for Windows directory, set the path below correct
REM ===================================================
SET T_LUAPATH=C:\Users\Public\lua\5.1

echo Copying file 'luaexit.dll'
copy "..\debug\luaexit.dll" "%T_LUAPATH%\clibs"

echo Copying file 'luaexit_test.lua'
copy "luaexit_test.lua" "%T_LUAPATH%\lua"
