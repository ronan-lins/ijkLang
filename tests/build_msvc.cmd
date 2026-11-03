@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b 1

if not exist bin\Debug mkdir bin\Debug
if not exist build\compiler mkdir build\compiler
if not exist build\vm mkdir build\vm
if not exist build\common mkdir build\common

echo Compiling ijkl.exe with MSVC C11...
cl /nologo /W4 /WX- /Od /D_DEBUG /D_CONSOLE /std:c11 /Iinclude /MDd /Febin\Debug\ijkl.exe ^
   src\compiler\main.c src\compiler\config.c src\compiler\lexer.c src\compiler\token.c ^
   src\compiler\vocabulary.c src\compiler\ast.c src\compiler\parser.c src\compiler\symtab.c ^
   src\compiler\semantic.c src\compiler\codegen.c src\compiler\module_loader.c ^
   src\common\memory.c src\common\utf8.c /link /INCREMENTAL:NO
if errorlevel 1 exit /b 1

echo Compiling ijkl-m.exe with MSVC C11...
cl /nologo /W4 /WX- /Od /D_DEBUG /D_CONSOLE /std:c11 /Iinclude /MDd /Febin\Debug\ijkl-m.exe ^
   src\vm\main.c src\vm\ci_instruction.c src\vm\ci_loader.c src\vm\vm_core.c ^
   src\vm\vm_debugger.c src\vm\vm_io.c src\vm\vm_memory.c src\vm\vm_plugins.c ^
   src\common\memory.c src\common\utf8.c /link /INCREMENTAL:NO
if errorlevel 1 exit /b 1

echo MSVC Build completed successfully!
