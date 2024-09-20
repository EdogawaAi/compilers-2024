@echo off

@REM 下载 Docker 镜像
set URL=https://ipads.se.sjtu.edu.cn/courses/compilers/labs/tigerlabs_env_v2.tar
set IMAGE_FILE=tigerlabs_env_v2.tar
if exist %IMAGE_FILE% (
    del %IMAGE_FILE%
)
curl -o %IMAGE_FILE% %URL%
echo Download completed.

@REM 载入 Docker 镜像
docker load -i %IMAGE_FILE% 
echo Load completed.

@REM 清理 Docker 镜像
del %IMAGE_FILE%

pause