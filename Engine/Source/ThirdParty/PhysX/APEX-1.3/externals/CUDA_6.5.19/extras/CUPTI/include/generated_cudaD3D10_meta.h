// This file is generated.  Any changes you make will be lost during the next clean build.

// Dependent includes
#include <rpcsal.h> // XP build fails in D3D10 header unless you include this first
#include <D3D10_1.h>

// CUDA public interface, for type definitions and cu* function prototypes
#include "cudaD3D10.h"


// *************************************************************************
//      Definitions of structs to hold parameters for each function
// *************************************************************************

typedef struct cuD3D10GetDevice_params_st {
    CUdevice *pCudaDevice;
    IDXGIAdapter *pAdapter;
} cuD3D10GetDevice_params;

typedef struct cuD3D10GetDevices_params_st {
    unsigned int *pCudaDeviceCount;
    CUdevice *pCudaDevices;
    unsigned int cudaDeviceCount;
    ID3D10Device *pD3D10Device;
    CUd3d10DeviceList deviceList;
} cuD3D10GetDevices_params;

typedef struct cuGraphicsD3D10RegisterResource_params_st {
    CUgraphicsResource *pCudaResource;
    ID3D10Resource *pD3DResource;
    unsigned int Flags;
} cuGraphicsD3D10RegisterResource_params;

typedef struct cuD3D10CtxCreate_v2_params_st {
    CUcontext *pCtx;
    CUdevice *pCudaDevice;
    unsigned int Flags;
    ID3D10Device *pD3DDevice;
} cuD3D10CtxCreate_v2_params;

typedef struct cuD3D10CtxCreateOnDevice_params_st {
    CUcontext *pCtx;
    unsigned int flags;
    ID3D10Device *pD3DDevice;
    CUdevice cudaDevice;
} cuD3D10CtxCreateOnDevice_params;

typedef struct cuD3D10GetDirect3DDevice_params_st {
    ID3D10Device **ppD3DDevice;
} cuD3D10GetDirect3DDevice_params;

typedef struct cuD3D10RegisterResource_params_st {
    ID3D10Resource *pResource;
    unsigned int Flags;
} cuD3D10RegisterResource_params;

typedef struct cuD3D10UnregisterResource_params_st {
    ID3D10Resource *pResource;
} cuD3D10UnregisterResource_params;

typedef struct cuD3D10MapResources_params_st {
    unsigned int count;
    ID3D10Resource **ppResources;
} cuD3D10MapResources_params;

typedef struct cuD3D10UnmapResources_params_st {
    unsigned int count;
    ID3D10Resource **ppResources;
} cuD3D10UnmapResources_params;

typedef struct cuD3D10ResourceSetMapFlags_params_st {
    ID3D10Resource *pResource;
    unsigned int Flags;
} cuD3D10ResourceSetMapFlags_params;

typedef struct cuD3D10ResourceGetMappedArray_params_st {
    CUarray *pArray;
    ID3D10Resource *pResource;
    unsigned int SubResource;
} cuD3D10ResourceGetMappedArray_params;

typedef struct cuD3D10ResourceGetMappedPointer_v2_params_st {
    CUdeviceptr *pDevPtr;
    ID3D10Resource *pResource;
    unsigned int SubResource;
} cuD3D10ResourceGetMappedPointer_v2_params;

typedef struct cuD3D10ResourceGetMappedSize_v2_params_st {
    size_t *pSize;
    ID3D10Resource *pResource;
    unsigned int SubResource;
} cuD3D10ResourceGetMappedSize_v2_params;

typedef struct cuD3D10ResourceGetMappedPitch_v2_params_st {
    size_t *pPitch;
    size_t *pPitchSlice;
    ID3D10Resource *pResource;
    unsigned int SubResource;
} cuD3D10ResourceGetMappedPitch_v2_params;

typedef struct cuD3D10ResourceGetSurfaceDimensions_v2_params_st {
    size_t *pWidth;
    size_t *pHeight;
    size_t *pDepth;
    ID3D10Resource *pResource;
    unsigned int SubResource;
} cuD3D10ResourceGetSurfaceDimensions_v2_params;

typedef struct cuD3D10CtxCreate_params_st {
    CUcontext *pCtx;
    CUdevice *pCudaDevice;
    unsigned int Flags;
    ID3D10Device *pD3DDevice;
} cuD3D10CtxCreate_params;

typedef struct cuD3D10ResourceGetMappedPitch_params_st {
    unsigned int *pPitch;
    unsigned int *pPitchSlice;
    ID3D10Resource *pResource;
    unsigned int SubResource;
} cuD3D10ResourceGetMappedPitch_params;

typedef struct cuD3D10ResourceGetMappedPointer_params_st {
    CUdeviceptr_v1 *pDevPtr;
    ID3D10Resource *pResource;
    unsigned int SubResource;
} cuD3D10ResourceGetMappedPointer_params;

typedef struct cuD3D10ResourceGetMappedSize_params_st {
    unsigned int *pSize;
    ID3D10Resource *pResource;
    unsigned int SubResource;
} cuD3D10ResourceGetMappedSize_params;

typedef struct cuD3D10ResourceGetSurfaceDimensions_params_st {
    unsigned int *pWidth;
    unsigned int *pHeight;
    unsigned int *pDepth;
    ID3D10Resource *pResource;
    unsigned int SubResource;
} cuD3D10ResourceGetSurfaceDimensions_params;
