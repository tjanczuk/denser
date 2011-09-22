@echo off
if "%1" equ "" echo Usage: genconfig.bat ^<number_of_applications^> & exit /b -1
echo {^"programs^":[
set COUNT=%1
:genone
echo {^"file^":^"chat2.js^"},
set /a COUNT=%COUNT%-1
if "%COUNT%" neq "1" goto genone
echo {^"file^":^"chat2.js^"}]}