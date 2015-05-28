/*
 * Copyright 1993-2012 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

#include <math.h>
#include <cuda.h>
#include <builtin_types.h>
#include <drvapi_error_string.h>
#include "nvvm.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

// This will output the proper CUDA error strings in the event that a CUDA host call returns an error
#define checkCudaErrors(err)  __checkCudaErrors (err, __FILE__, __LINE__)

// These are the inline versions for all of the SDK helper functions
void __checkCudaErrors( CUresult err, const char *file, const int line )
{
    if( CUDA_SUCCESS != err) {
        fprintf(stderr, "checkCudaErrors() Driver API error = %04d \"%s\" from file <%s>, line %i.\n",
                err, getCudaDrvErrorString(err), file, line );
        exit(-1);
    }
}

CUdevice cudaDeviceInit()
{
    CUdevice cuDevice = 0;
    int deviceCount = 0;
    CUresult err = cuInit(0);
    char name[100];
    int major=0, minor=0;

    if (CUDA_SUCCESS == err)
        checkCudaErrors(cuDeviceGetCount(&deviceCount));
    if (deviceCount == 0) {
        fprintf(stderr, "cudaDeviceInit error: no devices supporting CUDA\n");
        exit(-1);
    }
    checkCudaErrors(cuDeviceGet(&cuDevice, 0));
    cuDeviceGetName(name, 100, cuDevice);
    printf("Using CUDA Device [0]: %s\n", name);

    checkCudaErrors( cuDeviceComputeCapability(&major, &minor, cuDevice) );
    if (major < 2) {
        fprintf(stderr, "Device 0 is not sm_20 or later\n");
        exit(-1);
    }
    return cuDevice;
}


CUresult initCUDA(CUcontext *phContext,
                  CUdevice *phDevice,
                  CUmodule *phModule,
                  CUfunction *phKernel,
                  const char *ptx)
{
    // Initialize 
    *phDevice = cudaDeviceInit();

    // Create context on the device
    checkCudaErrors(cuCtxCreate(phContext, 0, *phDevice));

    // Load the PTX 
    checkCudaErrors(cuModuleLoadDataEx(phModule, ptx, 0, 0, 0));

    // Locate the kernel entry poin
    checkCudaErrors(cuModuleGetFunction(phKernel, *phModule, "simple"));

    return CUDA_SUCCESS;
}

char *loadProgramSource(const char *filename, size_t *size) 
{
    struct stat statbuf;
    FILE *fh;
    char *source = NULL;
    *size = 0;
    fh = fopen(filename, "rb");
    if (fh) {
        stat(filename, &statbuf);
        source = (char *) malloc(statbuf.st_size+1);
        if (source) {
            fread(source, statbuf.st_size, 1, fh);
            source[statbuf.st_size] = 0;
            *size = statbuf.st_size+1;
        }
    }
    else {
        fprintf(stderr, "Error reading file %s\n", filename);
        exit(-1);
    }
    return source;
}

char *generatePTX(const char *ll, size_t size, const char *filename)
{
    nvvmResult result;
    nvvmProgram program;
    size_t PTXSize;
    char *PTX = NULL;

    result = nvvmCreateProgram(&program);
    if (result != NVVM_SUCCESS) {
        fprintf(stderr, "nvvmCreateProgram: Failed\n");
        exit(-1); 
    }

    result = nvvmAddModuleToProgram(program, ll, size, filename);
    if (result != NVVM_SUCCESS) {
        fprintf(stderr, "nvvmAddModuleToProgram: Failed\n");
        exit(-1);
    }
 
    result = nvvmCompileProgram(program,  0, NULL);
    if (result != NVVM_SUCCESS) {
        char *Msg = NULL;
        size_t LogSize;
        fprintf(stderr, "nvvmCompileProgram: Failed\n");
        nvvmGetProgramLogSize(program, &LogSize);
        Msg = (char*)malloc(LogSize);
        nvvmGetProgramLog(program, Msg);
        fprintf(stderr, "%s\n", Msg);
        free(Msg);
        exit(-1);
    }
    
    result = nvvmGetCompiledResultSize(program, &PTXSize);
    if (result != NVVM_SUCCESS) {
        fprintf(stderr, "nvvmGetCompiledResultSize: Failed\n");
        exit(-1);
    }
    
    PTX = (char*)malloc(PTXSize);
    result = nvvmGetCompiledResult(program, PTX);
    if (result != NVVM_SUCCESS) {
        fprintf(stderr, "nvvmGetCompiledResult: Failed\n");
        free(PTX);
        exit(-1);
    }
    
    result = nvvmDestroyProgram(&program);
    if (result != NVVM_SUCCESS) {
      fprintf(stderr, "nvvmDestroyProgram: Failed\n");
      free(PTX);
      exit(-1);
    }
    
    return PTX;
}

int main(int argc, char **argv)
{
    const unsigned int nThreads = 32;
    const unsigned int nBlocks  = 1;
    const size_t memSize = nThreads * nBlocks * sizeof(int);

    CUcontext    hContext = 0;
    CUdevice     hDevice  = 0;
    CUmodule     hModule  = 0;
    CUfunction   hKernel  = 0;
    CUdeviceptr  d_data   = 0;
    int         *h_data   = 0;
    char        *ptx      = NULL;
    unsigned int i;

    // Get the ll from file
    size_t size = 0;
    // Kernel parameters
    void *params[] = { &d_data };
#if BUILD_64_BIT
    const char *filename = "simple-gpu64.ll";
#else
    const char *filename = "simple-gpu.ll";
#endif
    char *ll = loadProgramSource(filename, &size);
    fprintf(stdout, "NVVM IR ll file loaded\n");

    // Use libnvvm to generte PTX
    ptx = generatePTX(ll, size, filename);
    fprintf(stdout, "PTX generated:\n");
    fprintf(stdout, "%s\n", ptx);
    
    // Initialize the device and get a handle to the kernel
    checkCudaErrors(initCUDA(&hContext, &hDevice, &hModule, &hKernel, ptx));

    // Allocate memory on host and device
    if ((h_data = (int *)malloc(memSize)) == NULL) {
        fprintf(stderr, "Could not allocate host memory\n");
        exit(-1);
    }
    checkCudaErrors(cuMemAlloc(&d_data, memSize));

    // Launch the kernel
    checkCudaErrors(cuLaunchKernel(hKernel, nBlocks, 1, 1, nThreads, 1, 1,
                                   0, NULL, params, NULL));
    fprintf(stdout, "CUDA kernel launched\n");
    
    // Copy the result back to the host
    checkCudaErrors(cuMemcpyDtoH(h_data, d_data, memSize));

    // Print the result
    for (i = 0 ; i < nBlocks * nThreads ; i++) {
        fprintf(stdout, "%d ", h_data[i]);
    }

    fprintf(stdout, "\n");
    
    // Cleanup
    if (d_data) {
        checkCudaErrors(cuMemFree(d_data));
        d_data = 0;
    }
    if (h_data) {
        free(h_data);
        h_data = 0;
    }
    if (hModule) {
        checkCudaErrors(cuModuleUnload(hModule));
        hModule = 0;
    }
    if (hContext) {
        checkCudaErrors(cuCtxDestroy(hContext));
        hContext = 0;
    }

    free(ll);
    free(ptx);
    
    return 0;
}
