@echo off


@REM 执行 Docker 命令
set SCRIPT_DIR=%~dp0..
echo "%SCRIPT_DIR%" 
docker run -it --dns=223.5.5.5 --privileged -v "%SCRIPT_DIR%":/home/stu/tiger-compiler ipadsse302/tigerlabs_env:2.0.0 /bin/bash

pause