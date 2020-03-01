REM Build script for Windows

cd ./zlib
make --makefile="./win32/Makefile.gcc"
xcopy "zlib.h" "../zlib.h" /y
xcopy "zconf.h" "../zconf.h" /y
xcopy "zlib1.dll" "../zlib1.dll" /y
cd ../
make 
