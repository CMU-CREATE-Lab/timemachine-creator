TO COMPILE:

OS X:
-----
Prereqs: Install xcode
Download and install Qt 4.8.x libraries (using online installer from http://qt.nokia.com/downloads/)
Find the Qt 4.8 "qmake" executable
Run qmake to generate Makefile, e.g.
~/QtSDK/Desktop/Qt/4.8.0/gcc/bin/qmake 
Make:
make



Windows:
--------

Setup:
1. Download and install Visual Studio Express 2010 (using online installer from http://www.microsoft.com/visualstudio/en-us/products/2010-editions/visual-cpp-express)
  1a. Note: this is the last version supported for Windows XP
2. Download and install Qt 4.8.x libraries (using online installer from http://qt.nokia.com/downloads)

Compile:
1. cd to the directory with your source files (.cpp, .pro, etc)
2. From this directory, either run: 'C:\QtSDK\Desktop\Qt\4.8.0\msvc2010\bin\qmake.exe' or add this (without the qmake.exe part) to your PATH and just run 'qmake'. This will generate make files.
3. Next, you need to run 'nmake.exe' You will need to load a command prompt with Visual Studio environment variables setup. Three ways to do this:
 3a. From inside Visual Studio Express: Tools > Visual Studio Command Prompt. Type 'nmake' at the prompt.
 3b. From Start Menu: Microsoft Visual Studio 2010 Express > Visual Studio Command Prompt. Type 'nmake' at the prompt.
 3c. From your shell of choice: Run '"C:\Program Files\Microsoft Visual Studio 10.0\VC\vcvarsall.bat"' (include double quotes to run a command with spaces in it). This will setup the necessary environment variables in the shell you ran this from. Type 'nmake' at the prompt.

Run:
1. cd to /debug (this directory was created by 'qmake' in your source files directory. Double cick the name of your program: {name}.exe



Linux: (tested on Ubuntu 12.04)
-------------------------------

Setup:
1. Install Qt Creator. You can use Ubuntu software center and install Qt creator or go to Qt website and follow the instructions from there. (http://qt.nokia.com/downloads)
2. Download and compile ffmpeg. It currently works with ffmpeg-0.10.4
 2a. sudo apt-get remove ffmpeg x264 libx264-dev
 2b. sudo apt-get update
 2c. git clone git://git.videolan.org/x264
 2d. cd x264/
 2e. ./configure --enable-shared
 2f. make
 2g. sudo make install
 2h. Download ffmpeg 0.10.4 from this address: http://ffmpeg.org/releases/ffmpeg-0.10.4.tar.gz . Then unpack it and cd to its directory.
 2i. ./configure --enable-gpl --enable-libx264
 2j. make
 2k. copy ffmpeg executable to the time machine directory at this location: tmca/tilestacktool/ffmpeg/linux/
3. cd to time machine directory at: tmca/tilestacktool
 3a. make clean
 3b. make
 3c. make test
4. cd to tmca/tmc for compiling Qt GUI:
 4a. qmake
 4b. make

Run:
1. cd to tmca/tmc
2. ./tmc
