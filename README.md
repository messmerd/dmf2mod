# dmf2mod
A Windows command-line utility for converting Deflemask's GameBoy .dmf files to ProTracker's .mod tracker files. 
 
Because of the limitations of the .mod format, there are some restrictions on the .dmf files that can be converted. 
For example, .dmf files must use the GameBoy system, patterns must have 64 rows, only one effect column is allowed per channel, etc. 
 
Other programmers may find the dmf.c/dmf.h source files helpful for writing programs that utilize .dmf files, since those source files contain everything needed to import a .dmf file. You have my permission to do whatever you want with any of dmf2mod's code (except the zlib code which isn't mine).

## Build    
Download and extract the repository then run make.bat to get the executable file. 
 
Note that `gcc` must be installed and added to the PATH in order to build. 

If you are having issues with the zlib dll, try recompiling it from the source code at https://zlib.net.

## Usage 
```
.\dmf2mod.exe output_file.mod deflemask_gameboy_file.dmf [options]
``` 
Options:
```
--help                   Displays the help message.
--noeffects              Ignore Deflemask effects during conversion (except for set duty cycle effect).
```
 
Created by Dalton Messmer <messmer.dalton(at)gmail(dot)com>.
