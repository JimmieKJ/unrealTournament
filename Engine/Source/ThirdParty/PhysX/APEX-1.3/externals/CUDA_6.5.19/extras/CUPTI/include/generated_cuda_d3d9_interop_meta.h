// This file is generated.  Any changes you make will be lost during the next clean build.

// CUDA public interface, for type definitions and api function prototypes
#include "cuda_d3d9_interop.h"

// *************************************************************************
//      Definitions of structs to hold parameters for each function
// *************************************************************************

// Currently used parameter trace structures 
typedef struct cudaD3D9GetDirect3DDevice_v3020_params_st {
    IDirect3DDevice9 **ppD3D9Device;
} cudaD3D9GetDirect3DDevice_v3020_params;

typedef struct cudaGraphicsD3D9RegisterResource_v3020_params_st {
    struct cudaGraphicsResource **resource;
    IDirect3DResource9 *pD3DResource;
    unsigned int flags;
} cudaGraphicsD3D9RegisterResource_v3020_params;

typedef struct cudaD3D9GetDevice_v3020_params_st {
    int *device;
    const char *pszAdapterName;
} cudaD3D9GetDevice_v3020_params;

typedef struct cudaD3D9GetDevices_v3020_params_st {
    unsigned int *pCudaDeviceCount;
    int *pCudaDevices;
    unsigned int cudaDeviceCount;
    IDirect3DDevice9 *pD3D9Device;
    enum cudaD3D9DeviceList deviceList;
} cudaD3D9GetDevices_v3020_params;

typedef struct cudaD3D9SetDirect3DDevice_v3020_params_st {
    IDirect3DDevice9 *pD3D9Device;
    int device;
} cudaD3D9SetDirect3DDevice_v3020_params;

typedef struct cudaD3D9RegisterResource_v3020_params_st {
    IDirect3DResource9 *pResource;
    unsigned int flags;
} cudaD3D9RegisterResource_v3020_params;

typedef struct cudaD3D9UnregisterResource_v3020_params_st {
    IDirect3DResource9 *pResource;
} cudaD3D9UnregisterResource_v3020_params;

typedef struct cudaD3D9MapResources_v3020_params_st {
    int count;
    IDirect3DResource9 **ppResources;
} cudaD3D9MapResources_v3020_params;

typedef struct cudaD3D9UnmapResources_v3020_params_st {
    int count;
    IDirect3DResource9 **ppResources;
} cudaD3D9UnmapResources_v3020_params;

typedef struct cudaD3D9ResourceSetMapFlags_v3020_params_st {
    IDirect3DResource9 *pResource;
    unsigned int flags;
} cudaD3D9ResourceSetMapFlags_v3020_params;

typedef struct cudaD3D9ResourceGetSurfaceDimensions_v3020_params_st {
    size_t *pWidth;
    size_t *pHeight;
    size_t *pDepth;
    IDirect3DResource9 *pResource;
    unsigned int face;
    unsigned int level;
} cudaD3D9ResourceGetSurfaceDimensions_v3020_params;

typedef struct cudaD3D9ResourceGetMappedArray_v3020_params_st {
    cudaArray **ppArray;
    IDirect3DResource9 *pResource;
    unsigned int face;
    unsigned int level;
} cudaD3D9ResourceGetMappedArray_v3020_params;

typedef struct cudaD3D9ResourceGetMappedPointer_v3020_params_st {
    void **pPointer;
    IDirect3DResource9 *pResource;
    unsigned int face;
    unsigned int level;
} cudaD3D9ResourceGetMappedPointer_v3020_params;

typedef struct cudaD3D9ResourceGetMappedSize_v3020_params_st {
    size_t *pSize;
    IDirect3DResource9 *pResource;
    unsigned int face;
    unsigned int level;
} cudaD3D9ResourceGetMappedSize_v3020_params;

typedef struct cudaD3D9ResourceGetMappedPitch_v3020_params_st {
    size_t *pPitch;
    size_t *pPitchSlice;
    IDirect3DResource9 *pResource;
    unsigned int face;
    unsigned int level;
} cudaD3D9ResourceGetMappedPitch_v3020_params;

typedef struct cudaD3D9Begin_v3020_params_st {
    IDirect3DDevice9 *pDevice;
} cudaD3D9Begin_v3020_params;

typedef struct cudaD3D9RegisterVertexBuffer_v3020_params_st {
    IDirect3DVertexBuffer9 *pVB;
} cudaD3D9RegisterVertexBuffer_v3020_params;

typedef struct cudaD3D9UnregisterVertexBuffer_v3020_params_st {
    IDirect3DVertexBuffer9 *pVB;
} cudaD3D9UnregisterVertexBuffer_v3020_params;

typedef struct cudaD3D9MapVertexBuffer_v3020_params_st {
    void **dptr;
    IDirect3DVertexBuffer9 *pVB;
} cudaD3D9MapVertexBuffer_v3020_params;

typedef struct cudaD3D9UnmapVertexBuffer_v3020_params_st {
    IDirect3DVertexBuffer9 *pVB;
} cudaD3D9UnmapVertexBuffer_v3020_params;

// Parameter trace structures for removed functions 


// End of parameter trace structures
