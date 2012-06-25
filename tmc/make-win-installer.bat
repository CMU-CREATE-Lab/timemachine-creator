@echo off
rd /Q /S installer
xcopy assets installer\tmc\assets /E /C /R /I /K /Y
for %%I in (index.html test.html) do copy %%I installer\tmc
xcopy ..\ct\backports installer\ct\backports /E /C /R /I /K /Y
xcopy ..\ct\json installer\ct\json /E /C /R /I /K /Y
xcopy ..\ct\exifr installer\ct\exifr /E /C /R /I /K /Y
for %%I in (..\ct\*.rb) do copy %%I installer\ct
xcopy ..\tilestacktool\tilestacktool.exe installer\tilestacktool\ /I
xcopy ..\tilestacktool\ffmpeg\windows\ffmpeg.exe installer\tilestacktool\ffmpeg\windows\ /I
xcopy ..\ruby installer\ruby /E /C /R /I /K /Y
xcopy ..\time-machine-explorer installer\time-machine-explorer /E /C /R /I /K /Y
nmake release
copy release\tmc.exe installer
echo.
echo Now copy the following dll files to the main installer directory:
echo   msvcp100.dll
echo   msvcr100.dll
echo   QtCore4.dll
echo   QtGui4.dll
echo   QtNetwork4.dll
echo   QtWebKit4.dll
echo   imageformats\qjpeg4.dll
