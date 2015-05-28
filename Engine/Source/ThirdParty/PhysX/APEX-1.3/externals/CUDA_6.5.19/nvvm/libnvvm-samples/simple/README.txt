simple.c is a simple C program that illustrates the use of libnvvm.
simple-gpu.ll is a text file with a simple program written in NVVM
IR. simple.c reads in simple-gpu.ll, uses libnvvm API to generate
PTX code, and uses CUDA driver API to execute the PTX code on a GPU.
