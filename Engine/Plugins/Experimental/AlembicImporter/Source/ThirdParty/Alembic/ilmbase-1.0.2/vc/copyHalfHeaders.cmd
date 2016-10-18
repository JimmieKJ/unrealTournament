@echo off
set src=..\..\..\..\Half

cd %src%
set instpath=..\..\Deploy\include
if not exist %instpath% mkdir %instpath%

copy half.h %instpath%
copy halfFunction.h %instpath%
copy halfLimits.h %instpath%

