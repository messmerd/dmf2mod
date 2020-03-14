REM Build script for Windows

cd ./zlib
make --makefile="./win32/Makefile.gcc"
cd ../
make 
