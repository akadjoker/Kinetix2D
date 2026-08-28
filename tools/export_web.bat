@echo off
setlocal
set "K2D_TOOLS=%~dp0"
if defined EMSDK_DIR if exist "%EMSDK_DIR%\emsdk_env.bat" call "%EMSDK_DIR%\emsdk_env.bat" >nul
where py >nul 2>nul && (py -3 "%K2D_TOOLS%export_web.py" %* & exit /b %errorlevel%)
python "%K2D_TOOLS%export_web.py" %*
