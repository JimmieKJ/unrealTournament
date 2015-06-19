// This file is generated.  Any changes you make will be lost during the next clean build.

// CUDA public interface, for type definitions and api function prototypes
#include "cuda_d3d11_interop.h"

// *************************************************************************
//      Definitions of structs to hold parameters for each function
// *************************************************************************

// Currently used parameter trace structures 
typedef struct cudaGraphicsD3D11RegisterResource_v3020_params_st {
    struct cudaGraphicsResource **resource;
    ID3D11Resource *pD3DResource;
    unsigned int flags;
} cudaGraphicsD3D11RegisterResource_v3020_params;

typedef struct cudaD3D11GetDevice_v3020_params_st {
    int *device;
    IDXGIAdapter *pAdapter;
} cudaD3D11GetDevice_v3020_params;

typedef struct cudaD3D11GetDevices_v3020_params_st {
    unsigned int *pCudaDeviceCount;
    int *pCudaDevices;
    unsigned int cudaDeviceCount;
    ID3D11Device *pD3D11Device;
    enum cudaD3D11DeviceList deviceList;
} cudaD3D11GetDevices_v3020_params;

typedef struct cudaD3D11GetDirect3DDevice_v3020_params_st {
    ID3D11Device **ppD3D11Device;
} cudaD3D11GetDirect3DDevice_v3020_params;

typedef struct cudaD3D11SetDirect3DDevice_v3020_params_st {
    ID3D11Device *pD3D11Device;
    int device;
} cudaD3D11SetDirect3DDevice_v3020_params;

// Parameter trace structures for removed functions 


// End of parameter trace structures
