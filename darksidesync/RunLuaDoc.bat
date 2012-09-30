@echo off
echo off
cls
rem =================================================================
rem The following variables should be defined;
rem   LUA_DEV         - path to the Lua environment
rem                     usually; c:\program files\lua\5.1
rem   LUA_EDITOR      - path including executable of lua editor
rem                     usually; %LUA_DEV%\scite\scite.exe
rem   LUA_SOURCEPATH  - path to Lua source code
rem                     usually; %LUA_DEV%\lua
rem =================================================================
rem

rem go to source directory and start LuaDoc
"%LUA_SOURCEPATH%\luadoc_start.lua" --nofiles -d .\doc dss.lua darksidesync.luadoc 
start .\doc\index.html
pause



