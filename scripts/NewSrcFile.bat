@echo off
set currdir=%~dp0
set code_rel_dir=..\code\

set header_file=%~1
set src_file=%~2

set header_path=%currdir%%code_rel_dir%%header_file%
set src_path=%currdir%%code_rel_dir%%src_file%

cat header_template.h.tpl > %header_path%
cat src_template.cpp.tpl > %src_path%

echo Running cmake...
cd ../build
cmake ../
cd %currdir%
echo ------------------------
echo New files:
echo  * Header: %header_path%
echo  * Source: %src_path%
echo ------------------------
