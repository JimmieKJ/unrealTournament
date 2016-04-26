/********************************************************************************//**
\file      OVR_CAPI_GL.h
\brief     OpenGL-specific structures used by the CAPI interface.
\copyright Copyright 2013 Oculus VR, LLC. All Rights reserved.
************************************************************************************/

#ifndef OVR_CAPI_GL_h
#define OVR_CAPI_GL_h

#include "OVR_CAPI.h"

// We avoid gl.h #includes here which interferes with some users' use of alternatives and typedef GLuint manually.
typedef unsigned int GLuint;


#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4324) // structure was padded due to __declspec(align())
#endif

/// Used to pass GL eye texture data to ovr_EndFrame.
typedef struct ovrGLTextureData_s
{
    ovrTextureHeader Header;    ///< General device settings.
    GLuint           TexId;     ///< The OpenGL name for this texture.
} ovrGLTextureData;

OVR_STATIC_ASSERT(sizeof(ovrTexture) >= sizeof(ovrGLTextureData), "Insufficient size.");
OVR_STATIC_ASSERT(sizeof(ovrGLTextureData) == sizeof(ovrTextureHeader) + 4, "size mismatch");

/// Contains OpenGL-specific texture information.
typedef union ovrGLTexture_s
{
    ovrTexture       Texture;   ///< General device settings.
    ovrGLTextureData OGL;       ///< OpenGL-specific settings.
} ovrGLTexture;


#if defined(_MSC_VER)
    #pragma warning(pop)
#endif



/// Creates a Texture Set suitable for use with OpenGL.
///
/// Multiple calls to ovr_CreateSwapTextureSetD3D11 for the same ovrHmd are supported, but applications
/// cannot rely on switching between ovrSwapTextureSets at runtime without a performance penalty.
///
/// \param[in]  session Specifies an ovrSession previously returned by ovr_Create.
/// \param[in]  format Specifies the texture format.
/// \param[in]  width Specifies the requested texture width.
/// \param[in]  height Specifies the requested texture height.
/// \param[out] outTextureSet Specifies the created ovrSwapTextureSet, which will be valid upon a successful return value, else it will be NULL.
///             This texture set must be eventually destroyed via ovr_DestroySwapTextureSet before destroying the HMD with ovr_Destroy.
///
/// \return Returns an ovrResult indicating success or failure. In the case of failure, use 
///         ovr_GetLastErrorInfo to get more information.
///
/// \note The \a format provided should be thought of as the format the distortion compositor will use when reading the contents of the
/// texture. To that end, it is highly recommended that the application requests swap-texture-set formats that are in sRGB-space (e.g. GL_SRGB_ALPHA8)
/// as the distortion compositor does sRGB-correct rendering. Furthermore, the app should then make sure "glEnable(GL_FRAMEBUFFER_SRGB);"
/// is called before rendering into these textures. Even though it is not recommended, if the application would like to treat the
/// texture as a linear format and do linear-to-gamma conversion in GLSL, then the application can avoid calling "glEnable(GL_FRAMEBUFFER_SRGB);",
/// but should still pass in GL_SRGB_ALPHA8 (not GL_RGBA) for the \a format. Failure to do so will cause the distortion compositor
/// to apply incorrect gamma conversions leading to gamma-curve artifacts.
///
/// \see ovr_DestroySwapTextureSet
///
OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateSwapTextureSetGL(ovrSession session, GLuint format,
                                                             int width, int height,
                                                             ovrSwapTextureSet** outTextureSet);


/// Creates a Mirror Texture which is auto-refreshed to mirror Rift contents produced by this application.
///
/// A second call to ovr_CreateMirrorTextureGL for a given ovrHmd before destroying the first one
/// is not supported and will result in an error return.
///
/// \param[in]  session Specifies an ovrSession previously returned by ovr_Create.
/// \param[in]  format Specifies the texture format.
/// \param[in]  width Specifies the requested texture width.
/// \param[in]  height Specifies the requested texture height.
/// \param[out] outMirrorTexture Specifies the created ovrSwapTexture, which will be valid upon a successful return value, else it will be NULL.
///             This texture must be eventually destroyed via ovr_DestroyMirrorTexture before destroying the HMD with ovr_Destroy.
///
/// \return Returns an ovrResult indicating success or failure. In the case of failure, use 
///         ovr_GetLastErrorInfo to get more information.
///
/// \note The \a format provided should be thought of as the format the distortion compositor will use when writing into the mirror
/// texture. It is highly recommended that mirror textures are requested as GL_SRGB_ALPHA8 because the distortion compositor
/// does sRGB-correct rendering. If the application requests a non-sRGB format (e.g. GL_RGBA) as the mirror texture,
/// then the application might have to apply a manual linear-to-gamma conversion when reading from the mirror texture.
/// Failure to do so can result in incorrect gamma conversions leading to gamma-curve artifacts and color banding.
///
/// \see ovr_DestroyMirrorTexture
///
OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateMirrorTextureGL(ovrSession session, GLuint format,
                                                            int width, int height,
                                                            ovrTexture** outMirrorTexture);


#endif    // OVR_CAPI_GL_h
