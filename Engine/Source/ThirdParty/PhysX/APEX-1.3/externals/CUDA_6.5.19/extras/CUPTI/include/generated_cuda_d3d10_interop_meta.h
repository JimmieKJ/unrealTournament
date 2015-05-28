// This file is generated.  Any changes you make will be lost during the next clean build.

// CUDA public interface, for type definitions and api function prototypes
#include "cuda_d3d10_interop.h"

// *************************************************************************
//      Definitions of structs to hold parameters for each function
// *************************************************************************

// Currently used parameter trace structures 
typedef struct cudaGraphicsD3D10RegisterResource_v3020_params_st {
    struct cudaGraphicsResource **resource;
    ID3D10Resource *pD3DResource;
    unsigned int flags;
} cudaGraphicsD3D10RegisterResource_v3020_params;

typedef struct cudaD3D10GetDevice_v3020_params_st {
    int *device;
    IDXGIAdapter *pAdapter;
} cudaD3D10GetDevice_v3020_params;

typedef struct cudaD3D10GetDevices_v3020_params_st {
    unsigned int *pCudaDeviceCount;
    int *pCudaDevices;
    unsigned int cudaDeviceCount;
    ID3D10Device *pD3D10Device;
    enum cudaD3D10DeviceList deviceList;
} cudaD3D10GetDevices_v3020_params;

typedef struct cudaD3D10GetDirect3DDevice_v3020_params_st {
    ID3D10Device **ppD3D10Device;
} cudaD3D10GetDirect3DDevice_v3020_params;

typedef struct cudaD3D10SetDirect3DDevice_v3020_params_st {
    ID3D10Device *pD3D10Device;
    int device;
} cudaD3D10SetDirect3DDevice_v3020_params;

typedef struct cudaD3D10RegisterResource_v3020_params_st {
    ID3D10Resource *pResource;
    unsigned int flags;
} cudaD3D10RegisterResource_v3020_params;

typedef struct cudaD3D10UnregisterResource_v3020_params_st {
    ID3D10Resource *pResource;
} cudaD3D10UnregisterResource_v3020_params;

typedef struct cudaD3D10MapResources_v3020_params_st {
    int count;
    ID3D10Resource **ppResources;
} cudaD3D10MapResources_v3020_params;

typedef struct cudaD3D10UnmapResources_v3020_params_st {
    int count;
    ID3D10Resource **ppResources;
} cudaD3D10UnmapResources_v3020_params;

typedef struct cudaD3D10ResourceGetMappedArray_v3020_params_st {
    cudaArray **ppArray;
    ID3D10Resource *pResource;
    unsigned int subResource;
} cudaD3D10ResourceGetMappedArray_v3020_params;

typedef struct cudaD3D10ResourceSetMapFlags_v3020_params_st {
    ID3D10Resource *pResource;
    unsigned int flags;
} cudaD3D10ResourceSetMapFlags_v3020_params;

typedef struct cudaD3D10ResourceGetSurfaceDimensions_v3020_params_st {
    size_t *pWidth;
    size_t *pHeight;
    size_t *pDepth;
    ID3D10Resource *pResource;
    unsigned int subResource;
} cudaD3D10ResourceGetSurfaceDimensions_v3020_params;

typedef struct cudaD3D10ResourceGetMappedPointer_v3020_params_st {
    void **pPointer;
    ID3D10Resource *pResource;
    unsigned int subResource;
} cudaD3D10ResourceGetMappedPointer_v3020_params;

typedef struct cudaD3D10ResourceGetMappedSize_v3020_params_st {
    size_t *pSize;
    ID3D10Resource *pResource;
    unsigned int subResource;
} cudaD3D10ResourceGetMappedSize_v3020_params;

typedef struct cudaD3D10ResourceGetMappedPitch_v3020_params_st {
    size_t *pPitch;
    size_t *pPitchSlice;
    ID3D10Resource *pResource;
    unsigned int subResource;
} cudaD3D10ResourceGetMappedPitch_v3020_params;

// Parameter trace structures for removed functions 


// End of parameter trace structures
