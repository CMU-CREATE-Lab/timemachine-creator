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

===================

Development directory structure:

<gitroot>/tmc/(tmc.app/Contents/MacOS/tmc|(debug|release)/tmc[.exe])
<gitroot>/tmclib/index.html
<gitroot>/tilestacktool/tilestacktool[.exe]
<gitroot>/ffmpeg/(osx|windows|linux)/ffmpeg[.exe]

Windows and Linux deploy directory structure:
<root>/tmc|tmc[.exe]
<root>/tmclib/index.html
<root>/tilestacktool[.exe]
<root>/ffmpeg[.exe]
<root>/time-machine-explorer
<root>/ct.rb
<root>/ctlib/[*.rb etc]

Mac deploy directory structure:
tmc.app/Contents/MacOS/tmc
tmc.app/Contents/tmc/index.html
tmc.app/Contents/tilestacktool
tmc.app/Contents/ffmpeg
tmc.app/Contents/time-machine-explorer






OS X, development.  root = git root;  binary is <root>/tmc/tmc.app/Contents/MacOS/tmc
OS X, deployed.  root = .app directory;  binary is <root>/MacOS/tmc
Windows, development.  root = git root; binary is <root>/tmc/[debug or release]/tmc.exe
Windows, deployed.  root = install directory;  binary is <root>/tmc.exe
Linux, development.  root = git root; binary is <root>/tmc/[debug or release]/tmc


