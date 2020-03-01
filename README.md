

dmf2mod [![Build Status](https://travis-ci.org/messmerd/dmf2mod.svg?branch=master)](https://travis-ci.org/messmerd/dmf2mod)
======

A cross-platform command-line utility for converting Deflemask's Game Boy .dmf files to ProTracker's .mod tracker files.

Because of the limitations of the .mod format, there are some restrictions on the .dmf files that can be converted. 
For example, .dmf files must use the Game Boy system, patterns must have 64 rows, only one effect column is allowed per channel, etc. 
 
Other programmers may find the dmf.c/dmf.h source files helpful for writing programs that utilize .dmf files, since those source files contain everything needed to import a .dmf file. You have my permission to do whatever you want with any of dmf2mod's code (except the zlib code which isn't mine).

## Build    
Clone the repository using: 

```git clone --recursive https://github.com/messmerd/dmf2mod.git``` 
(This ensures that the zlib repository is cloned as well.)

Go to the dmf2mod directory: 

```cd ./dmf2mod ```

Next, run the build script. 

In Windows, use:

```./build.bat``` 

In Linux or Mac, use:

```
sudo chmod +x ./build
sudo ./build 
```

Note that `gcc` and `make` must be installed and added to the PATH in order to build. 

If you can't get it to work, please send me an email at the address at the bottom. 

## Usage 
```
.\dmf2mod.exe output_file.mod deflemask_gameboy_file.dmf [options]
``` 
Options:
```
--help                   Displays the help message.
--noeffects              Ignore Deflemask effects (except for Set Duty Cycle).
```
 
Created by Dalton Messmer <messmer.dalton(at)gmail(dot)com>. 
Zlib created by Jean-loup Gailly and Mark Adler (https://zlib.net/).
