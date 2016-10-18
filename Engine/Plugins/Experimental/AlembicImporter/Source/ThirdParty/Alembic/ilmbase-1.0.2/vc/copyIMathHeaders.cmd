@echo off
set src=..\..\..\..\Imath

cd %src%
set instpath=..\..\Deploy\include
if not exist %instpath% mkdir %instpath%
copy *.h %instpath%

