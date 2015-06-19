// This file is generated.  Any changes you make will be lost during the next clean build.

// Dependent includes
#include <rpcsal.h> // XP build fails in D3D10 header unless you include this first
#include <D3D11.h>

// CUDA public interface, for type definitions and cu* function prototypes
#include "cudaD3D11.h"


// *************************************************************************
//      Definitions of structs to hold parameters for each function
// *************************************************************************

typedef struct cuD3D11GetDevice_params_st {
    CUdevice *pCudaDevice;
    IDXGIAdapter *pAdapter;
} cuD3D11GetDevice_params;

typedef struct cuD3D11GetDevices_params_st {
    unsigned int *pCudaDeviceCount;
    CUdevice *pCudaDevices;
    unsigned int cudaDeviceCount;
    ID3D11Device *pD3D11Device;
    CUd3d11DeviceList deviceList;
} cuD3D11GetDevices_params;

typedef struct cuGraphicsD3D11RegisterResource_params_st {
    CUgraphicsResource *pCudaResource;
    ID3D11Resource *pD3DResource;
    unsigned int Flags;
} cuGraphicsD3D11RegisterResource_params;

typedef struct cuD3D11CtxCreate_v2_params_st {
    CUcontext *pCtx;
    CUdevice *pCudaDevice;
    unsigned int Flags;
    ID3D11Device *pD3DDevice;
} cuD3D11CtxCreate_v2_params;

typedef struct cuD3D11CtxCreateOnDevice_params_st {
    CUcontext *pCtx;
    unsigned int flags;
    ID3D11Device *pD3DDevice;
    CUdevice cudaDevice;
} cuD3D11CtxCreateOnDevice_params;

typedef struct cuD3D11GetDirect3DDevice_params_st {
    ID3D11Device **ppD3DDevice;
} cuD3D11GetDirect3DDevice_params;

typedef struct cuD3D11CtxCreate_params_st {
    CUcontext *pCtx;
    CUdevice *pCudaDevice;
    unsigned int Flags;
    ID3D11Device *pD3DDevice;
} cuD3D11CtxCreate_params;
