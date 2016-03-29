/********************************************************************************//**
\file      OVR_CAPI_D3D.h
\brief     D3D specific structures used by the CAPI interface.
\copyright Copyright 2014 Oculus VR, LLC All Rights reserved.
************************************************************************************/

#ifndef OVR_CAPI_D3D_h
#define OVR_CAPI_D3D_h

#include "OVR_CAPI.h"

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4324) // structure was padded due to __declspec(align())
#endif

//-----------------------------------------------------------------------------------
// ***** D3D11 Specific
#if defined(_MSC_VER)
    #pragma warning(push, 0)
#endif
#include <d3d11.h>
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

/// Used to pass D3D11 eye texture data to ovr_EndFrame.
typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) ovrD3D11TextureData_
{
    ovrTextureHeader          Header;           ///< General device settings.
    OVR_ON64(OVR_UNUSED_STRUCT_PAD(pad0, 4))    ///< \internal struct padding.
    ID3D11Texture2D*          pTexture;         ///< The D3D11 texture containing the undistorted eye image.
    ID3D11ShaderResourceView* pSRView;          ///< The D3D11 shader resource view for this texture.
} ovrD3D11TextureData;

OVR_STATIC_ASSERT(sizeof(ovrTexture) >= sizeof(ovrD3D11TextureData), "Insufficient size.");
OVR_STATIC_ASSERT(sizeof(ovrD3D11TextureData) == sizeof(ovrTextureHeader) OVR_ON64(+4) + 2 * OVR_PTR_SIZE, "size mismatch");


/// Contains D3D11-specific texture information.
union ovrD3D11Texture
{
    ovrTexture          Texture;    ///< General device settings.
    ovrD3D11TextureData D3D11;      ///< D3D11-specific settings.
};


#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

/// Flags used when creating a swap texture set for a D3D11 renderer
typedef enum ovrSwapTextureSetD3D11Flags_
{
    ovrSwapTextureSetD3D11_Typeless = 0x0001,       ///< Forces creation of a DXGI_*_TYPELESS texture. ShaderResourceView still uses specified format.

    ovrSwapTextureSetD3D11_EnumSize = 0x7fffffff    ///< \internal Force type int32_t.
} ovrSwapTextureSetD3D11Flags;

/// Create Texture Set suitable for use with D3D11.
///
/// Multiple calls to ovr_CreateSwapTextureSetD3D11 for the same ovrHmd are supported, but applications
/// cannot rely on switching between ovrSwapTextureSets at runtime without a performance penalty.
///
/// \param[in]  session Specifies an ovrSession previously returned by ovr_Create.
/// \param[in]  device Specifies the associated ID3D11Device, which must be the one that the textures will be used with in the application's process.
/// \param[in]  desc Specifies requested texture properties. See notes for more info about texture format.
/// \param[in]  miscFlags Specifies misc bit flags of type \a ovrSwapTextureSetD3D11Flags used when creating the swap textures
/// \param[out] outTextureSet Specifies the created ovrSwapTextureSet, which will be valid upon a successful return value, else it will be NULL.
///             This texture set must be eventually destroyed via ovr_DestroySwapTextureSet before destroying the HMD with ovr_Destroy.
///
/// \return Returns an ovrResult indicating success or failure. In the case of failure, use 
///         ovr_GetLastErrorInfo to get more information.
///
/// \note The texture format provided in \a desc should be thought of as the format the distortion-compositor will use for the
/// ShaderResourceView when reading the contents of the texture. To that end, it is highly recommended that the application
/// requests swap-texture-set formats that are in sRGB-space (e.g. DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) as the compositor
/// does sRGB-correct rendering. As such, the compositor relies on the GPU's hardware sampler to do the sRGB-to-linear
/// conversion. If the application still prefers to render to a linear format (e.g. DXGI_FORMAT_R8G8B8A8_UNORM) while handling the
/// linear-to-gamma conversion via HLSL code, then the application must still request the corresponding sRGB format and also use
/// the \a ovrSwapTextureSetD3D11_Typeless flag. This will allow the application to create a RenderTargetView that is the desired
/// linear format while the compositor continues to treat it as sRGB. Failure to do so will cause the compositor to apply
/// unexpected gamma conversions leading to gamma-curve artifacts. The \a ovrSwapTextureSetD3D11_Typeless flag for depth buffer
/// formats (e.g. DXGI_FORMAT_D32) are ignored as they are always converted to be typeless.
///
/// \see ovr_DestroySwapTextureSet
///
OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateSwapTextureSetD3D11(ovrSession session,
                                                                ID3D11Device* device,
                                                                const D3D11_TEXTURE2D_DESC* desc,
                                                                unsigned int miscFlags,
                                                                ovrSwapTextureSet** outTextureSet);

/// Create Mirror Texture which is auto-refreshed to mirror Rift contents produced by this application.
///
/// A second call to ovr_CreateMirrorTextureD3D11 for a given ovrHmd before destroying the first one
/// is not supported and will result in an error return.
///
/// \param[in]  session Specifies an ovrSession previously returned by ovr_Create.
/// \param[in]  device Specifies the associated ID3D11Device, which must be the one that the textures will be used with in the application's process.
/// \param[in]  desc Specifies requested texture properties. See notes for info about texture format.
/// \param[in]  miscFlags Specifies misc bit flags of type \a ovrSwapTextureSetD3D11Flags used when creating the swap textures
/// \param[out] outMirrorTexture Specifies the created ovrTexture, which will be valid upon a successful return value, else it will be NULL.
///             This texture must be eventually destroyed via ovr_DestroyMirrorTexture before destroying the HMD with ovr_Destroy.
///
/// \return Returns an ovrResult indicating success or failure. In the case of failure, use 
///         ovr_GetLastErrorInfo to get more information.
///
/// \note The texture format provided in \a desc should be thought of as the format the compositor will use for the RenderTargetView when
/// writing into mirror texture. To that end, it is highly recommended that the application requests a mirror texture format that is
/// in sRGB-space (e.g. DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) as the compositor does sRGB-correct rendering. If however the application wants
/// to still read the mirror texture as a linear format (e.g. DXGI_FORMAT_R8G8B8A8_UNORM) and handle the sRGB-to-linear conversion in
/// HLSL code, then it is recommended the application still requests an sRGB format and also use the \a ovrSwapTextureSetD3D11_Typeless
/// flag. This will allow the application to bind a ShaderResourceView that is a linear format while the compositor continues
/// to treat is as sRGB. Failure to do so will cause the compositor to apply unexpected gamma conversions leading to 
/// gamma-curve artifacts.
///
/// \see ovr_DestroyMirrorTexture
///
OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateMirrorTextureD3D11(ovrSession session,
                                                               ID3D11Device* device,
                                                               const D3D11_TEXTURE2D_DESC* desc,
                                                               unsigned int miscFlags,
                                                               ovrTexture** outMirrorTexture);



#endif    // OVR_CAPI_D3D_h
