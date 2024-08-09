# LLVM-Pass-to-Instrument-Function-Pointers

## Overview
These are two LLVM passes written to detect when function pointers are used. StaticFP detects pointers statically at compile time, and outputs a CSV containing the function in which the function pointer was used, the address of the IR instruction that called it, and the address of the function it points to. DynamicFP instruments code to printf the function in which a function pointer is called, as well as the address the function pointer points to.

## Building and Installation
These passes should be built and run out of tree for LLVM-16.04, using the architecture of https://github.com/banach-space/llvm-tutor. You may have to edit certain configs to lower the LLVM version.

## Usage
After following the instructions at https://github.com/banach-space/llvm-tutor to build each pass into a library (.so or .dll if you're on Windows):
clang -fpass-plugin=/your/library/dir/lib<Static/DynamicFP>.so fp.c -o fp.elf
./fp.elf

If using StaticFP, the CSV should be created in the same directory, and if using DynamicFP, output will be printed to the terminal.

## Instrumentation Details
StaticFP simply detects function pointers, and writes details about them to a CSV it creates on compile. DynamicFP instruments the code with printf statements that output information about the function pointers at runtime.

## License
MIT License
