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



Remote: (setup for Ubuntu 12.04)
--------------------------------

Setup:
0. sudo apt-get install autoconf libboost1.48-all-dev libcurl4-openssl-dev libexif-dev libglu-dev libjpeg-turbo8-dev liblapack-dev libpng12-dev libqt4-dev libtiff4-dev xvfb -y
1. Install Ruby >= 1.9.3, Python >= 2.7.3
2. Copy the stitcher folder (gigapan-*-linux) to the main tmca folder. (download from https://drive.google.com/uc?id=0B2n3EeJJWXTBUWU1SnMxak5VSjg&export=download)
3. Install stitcher dependecies:
 3.a sudo apt-get install autoconf libboost1.48-all-dev libcurl4-openssl-dev libexif-dev libglu-dev libjpeg-turbo8-dev liblapack-dev libpng12-dev libqt4-dev libtiff4-dev xvfb
4. Install tmca

Setup Torque (task scheduler):
1. Setup host names on the server and clients. For example, if the server is hal.ece.cmu.edu and your node is hal2.ece.cmu.edu do this:
  1.a sudo -s
  1.b echo hal >> /etc/hostname
  1.c echo hal2 >> /etc/hostname
  1.d echo 127.0.0.1 localhost.localdomain localhost >> /etc/hosts
  1.e echo SERVER.IP.ADDRESS hal.ece.cmu.edu hal >> /etc/hosts
  1.d echo NODE.IP.ADDRESS hal2.ece.cmu.edu hal2 >> /etc/hosts
  1.f reboot
2. On server:
 2.a sudo -s
 2.b apt-get install torque-server torque-scheduler -y
 2.c killall pbs_server pbs_sched
 2.d cd /var/spool/torque
 2.e echo hal.ece.cmu.edu | tee server_name
 2.f pbs_server -t create
 2.g qmgr
  create queue default
  set queue default queue_type = Execution
  set queue default resources_default.nodes = 1
  set queue default resources_default.neednodes = 1
  set queue default enabled = True
  set queue default started = True
  set server scheduling = True
  set server acl_host_enable = False
  set server acl_hosts = hal.ece.cmu.edu
  set server default_queue = default
  set server query_other_jobs = True
  exit
 2.h qterm
 2.i echo 'hal2 np=8' | tee server_priv/nodes
3. On client:
 3.a sudo -s
 3.b apt-get install torque-client torque-mom -y
 3.c killall pbs_mom
 3.d cd /var/spool/torque
 3.e echo hal.ece.cmu.edu | tee server_name
 3.f echo 'pbs_server = hal.ece.cmu.edu' | tee mom_priv/config
4. On server:
 4.a pbs_server
 4.b pbs_sched
 4.c exit
5. On client:
 5.a pbs_mom
 5.b exit
6. Test it on server:
 6.a pbsnodes -a
 6.b echo sleep 30 | qsub
 6.c qstat

Setup GlusterFS:
This is the shared file system, creating a shared folder for your server (e.g. hal) and clients (e.g. hal2) to access to the same files. You should put all your tmca data (pictures, etc.) on this shared folder. In this example, we assume each computer (server/nodes) has a local hard disk mounted at /mnt/disk. We connect all of them together through GlusterFS and create one giant shared file system and mount that on ~/jobs.
1. First, install GlusterFS on both server and clients:
 1.a sudo apt-get install glusterfs-server glusterfs-client -y
2. On server:
 2.a sudo gluster peer probe hal2
 2.b sudo gluster volume create hal-shared-disk hal2:/mnt/disk
 2.c sudo gluster volume start hal-shared-disk
3. On both server and clients:
 3.a mkdir ~/jobs
 3.b sudo mount -t glusterfs hal:/hal-shared-disk ~/jobs
4. On server, give write permissions to everybody
 4.a sudo chmod -R a+rwx ~/jobs
Now put the tmca files and datasets inside ~/jobs.

