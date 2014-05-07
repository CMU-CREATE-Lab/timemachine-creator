@echo off
rd /Q /S installer
xcopy assets installer\tmc\assets /E /C /R /I /K /Y
for %%I in (index.html test.html) do copy %%I installer\tmc
xcopy ..\ct\backports installer\ct\backports /E /C /R /I /K /Y
xcopy ..\ct\json installer\ct\json /E /C /R /I /K /Y
xcopy ..\ct\exifr installer\ct\exifr /E /C /R /I /K /Y
xcopy ..\datasets\patp10_1x1 installer\datasets\patp10_1x1 /E /C /R /I /K /Y
xcopy ..\datasets\carnival4_2x2 installer\datasets\carnival4_2x2 /E /C /R /I /K /Y
xcopy ..\ct-examples\local-examples installer\ct-examples\local-examples /E /C /R /I /K /Y
cp ..\ct\g10.response installer\ct\
for %%I in (..\ct\*.rb) do copy %%I installer\ct
echo Making tilestacktool.exe
cd ..\tilestacktool
make clean
make
cd ..\tmc
xcopy ..\tilestacktool\tilestacktool.exe installer\tilestacktool\ /I
xcopy ..\tilestacktool\ffmpeg\windows\ffmpeg.exe installer\tilestacktool\ffmpeg\windows\ /I
xcopy ..\ruby installer\ruby /E /C /R /I /K /Y
xcopy ..\time-machine-explorer installer\time-machine-explorer /E /C /R /I /K /Y
echo Making tmc.exe
qmake
nmake clean
nmake release
copy release\tmc.exe installer
echo.
echo Now copy the following dll files to the main installer directory:
echo   Drive:\WINDOWS\system32\msvcp100.dll
echo   Drive:\WINDOWS\system32\msvcr100.dll
echo   Drive:\QtSDK\Desktop\Qt\4.8.x\bin\QtCore4.dll
echo   Drive:\QtSDK\Desktop\Qt\4.8.x\bin\QtGui4.dll
echo   Drive:\QtSDK\Desktop\Qt\4.8.x\bin\QtNetwork4.dll
echo   Drive:\QtSDK\Desktop\Qt\4.8.x\bin\QtWebKit4.dll
echo   Drive:\QtSDK\Desktop\Qt\4.8.x\plugins\imageformats\*.dll
