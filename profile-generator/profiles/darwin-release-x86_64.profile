cc=clang
cxx=llvm-g++
lib_cflags=-O3 -DNDEBUG -pipe -Wall -arch x86_64
bin_cflags=-O0 -pipe -Wall -arch x86_64
ldflags=-arch x86_64 -L.
lto=0
context=x86_64
context_asm=1
as=gcc -arch x86_64
