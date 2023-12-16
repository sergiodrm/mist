@echo off
set currdir=%~dp0
set code_rel_dir=..\code\

set header_file=%~1
set src_file=%~2

set header_path=%currdir%%code_rel_dir%%header_file%
set src_path=%currdir%%code_rel_dir%%src_file%

echo // header file for vkmmc project > %header_path%
echo // src file for vkmmc project > %src_path%

echo Running cmake...
cd ../build
cmake ../
cd %currdir%
echo ------------------------
echo New files:
echo  * Header: %header_path%
echo  * Source: %src_path%
echo ------------------------
