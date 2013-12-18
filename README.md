# DRMDecrypt

## Synopsis

This is a UNIX(c) / Windows / OS X Port of the DRMdecrypt i found somewhere on the net. It is capable of extracting the encryption key from the .mdb file and decrypts the .srf to a standard transport stream format.

## Platforms
+ [X] OS X Mavericks (10.9)
+ [X] Windows 7 x64

## TV
+ [x] Samsung PS50C7700

## Building

There is no Makefile at the moment. You will need a C++-Compiler. Apple Xcode project and MS Visual Studio solution are included.

+ `g++ -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -c aes.c -o aes.o`
+ `g++ -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -c DRMDecrypt.cpp -o DRMDecrypt.o`
+ `g++ -o drmdecrypt aes.o DRMDecrypt.o -lstdc++`

## ToDo

* [x] Fancy output (progressbar, etc.)
* [x] Speedup of decryption (ASM, CPU assisted)
* [x] Read the recorded filename / timestamp from the .inf file
* [x] Change to C++
* [x] Create Visual Studio Solution and test it
* [x] Create Xcode project and test it
* [ ] QT GUI
