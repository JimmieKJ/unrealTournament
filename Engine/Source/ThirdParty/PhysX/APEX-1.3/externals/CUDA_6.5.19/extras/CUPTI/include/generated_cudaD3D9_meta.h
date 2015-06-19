// This file is generated.  Any changes you make will be lost during the next clean build.

// Dependent includes
#include <d3d9.h>

// CUDA public interface, for type definitions and cu* function prototypes
#include "cudaD3D9.h"


// *************************************************************************
//      Definitions of structs to hold parameters for each function
// *************************************************************************

typedef struct cuD3D9GetDevice_params_st {
    CUdevice *pCudaDevice;
    const char *pszAdapterName;
} cuD3D9GetDevice_params;

typedef struct cuD3D9GetDevices_params_st {
    unsigned int *pCudaDeviceCount;
    CUdevice *pCudaDevices;
    unsigned int cudaDeviceCount;
    IDirect3DDevice9 *pD3D9Device;
    CUd3d9DeviceList deviceList;
} cuD3D9GetDevices_params;

typedef struct cuD3D9CtxCreate_v2_params_st {
    CUcontext *pCtx;
    CUdevice *pCudaDevice;
    unsigned int Flags;
    IDirect3DDevice9 *pD3DDevice;
} cuD3D9CtxCreate_v2_params;

typedef struct cuD3D9CtxCreateOnDevice_params_st {
    CUcontext *pCtx;
    unsigned int flags;
    IDirect3DDevice9 *pD3DDevice;
    CUdevice cudaDevice;
} cuD3D9CtxCreateOnDevice_params;

typedef struct cuD3D9GetDirect3DDevice_params_st {
    IDirect3DDevice9 **ppD3DDevice;
} cuD3D9GetDirect3DDevice_params;

typedef struct cuGraphicsD3D9RegisterResource_params_st {
    CUgraphicsResource *pCudaResource;
    IDirect3DResource9 *pD3DResource;
    unsigned int Flags;
} cuGraphicsD3D9RegisterResource_params;

typedef struct cuD3D9RegisterResource_params_st {
    IDirect3DResource9 *pResource;
    unsigned int Flags;
} cuD3D9RegisterResource_params;

typedef struct cuD3D9UnregisterResource_params_st {
    IDirect3DResource9 *pResource;
} cuD3D9UnregisterResource_params;

typedef struct cuD3D9MapResources_params_st {
    unsigned int count;
    IDirect3DResource9 **ppResource;
} cuD3D9MapResources_params;

typedef struct cuD3D9UnmapResources_params_st {
    unsigned int count;
    IDirect3DResource9 **ppResource;
} cuD3D9UnmapResources_params;

typedef struct cuD3D9ResourceSetMapFlags_params_st {
    IDirect3DResource9 *pResource;
    unsigned int Flags;
} cuD3D9ResourceSetMapFlags_params;

typedef struct cuD3D9ResourceGetSurfaceDimensions_v2_params_st {
    size_t *pWidth;
    size_t *pHeight;
    size_t *pDepth;
    IDirect3DResource9 *pResource;
    unsigned int Face;
    unsigned int Level;
} cuD3D9ResourceGetSurfaceDimensions_v2_params;

typedef struct cuD3D9ResourceGetMappedArray_params_st {
    CUarray *pArray;
    IDirect3DResource9 *pResource;
    unsigned int Face;
    unsigned int Level;
} cuD3D9ResourceGetMappedArray_params;

typedef struct cuD3D9ResourceGetMappedPointer_v2_params_st {
    CUdeviceptr *pDevPtr;
    IDirect3DResource9 *pResource;
    unsigned int Face;
    unsigned int Level;
} cuD3D9ResourceGetMappedPointer_v2_params;

typedef struct cuD3D9ResourceGetMappedSize_v2_params_st {
    size_t *pSize;
    IDirect3DResource9 *pResource;
    unsigned int Face;
    unsigned int Level;
} cuD3D9ResourceGetMappedSize_v2_params;

typedef struct cuD3D9ResourceGetMappedPitch_v2_params_st {
    size_t *pPitch;
    size_t *pPitchSlice;
    IDirect3DResource9 *pResource;
    unsigned int Face;
    unsigned int Level;
} cuD3D9ResourceGetMappedPitch_v2_params;

typedef struct cuD3D9Begin_params_st {
    IDirect3DDevice9 *pDevice;
} cuD3D9Begin_params;

typedef struct cuD3D9RegisterVertexBuffer_params_st {
    IDirect3DVertexBuffer9 *pVB;
} cuD3D9RegisterVertexBuffer_params;

typedef struct cuD3D9MapVertexBuffer_v2_params_st {
    CUdeviceptr *pDevPtr;
    size_t *pSize;
    IDirect3DVertexBuffer9 *pVB;
} cuD3D9MapVertexBuffer_v2_params;

typedef struct cuD3D9UnmapVertexBuffer_params_st {
    IDirect3DVertexBuffer9 *pVB;
} cuD3D9UnmapVertexBuffer_params;

typedef struct cuD3D9UnregisterVertexBuffer_params_st {
    IDirect3DVertexBuffer9 *pVB;
} cuD3D9UnregisterVertexBuffer_params;

typedef struct cuD3D9CtxCreate_params_st {
    CUcontext *pCtx;
    CUdevice *pCudaDevice;
    unsigned int Flags;
    IDirect3DDevice9 *pD3DDevice;
} cuD3D9CtxCreate_params;

typedef struct cuD3D9ResourceGetSurfaceDimensions_params_st {
    unsigned int *pWidth;
    unsigned int *pHeight;
    unsigned int *pDepth;
    IDirect3DResource9 *pResource;
    unsigned int Face;
    unsigned int Level;
} cuD3D9ResourceGetSurfaceDimensions_params;

typedef struct cuD3D9ResourceGetMappedPointer_params_st {
    CUdeviceptr_v1 *pDevPtr;
    IDirect3DResource9 *pResource;
    unsigned int Face;
    unsigned int Level;
} cuD3D9ResourceGetMappedPointer_params;

typedef struct cuD3D9ResourceGetMappedSize_params_st {
    unsigned int *pSize;
    IDirect3DResource9 *pResource;
    unsigned int Face;
    unsigned int Level;
} cuD3D9ResourceGetMappedSize_params;

typedef struct cuD3D9ResourceGetMappedPitch_params_st {
    unsigned int *pPitch;
    unsigned int *pPitchSlice;
    IDirect3DResource9 *pResource;
    unsigned int Face;
    unsigned int Level;
} cuD3D9ResourceGetMappedPitch_params;

typedef struct cuD3D9MapVertexBuffer_params_st {
    CUdeviceptr_v1 *pDevPtr;
    unsigned int *pSize;
    IDirect3DVertexBuffer9 *pVB;
} cuD3D9MapVertexBuffer_params;
