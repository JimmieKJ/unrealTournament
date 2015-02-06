/************************************************************************************

Filename    :   GlUtils.h
Content     :   Policy-free OpenGL convenience functions
Created     :   August 24, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/
#ifndef OVR_GlUtils_h
#define OVR_GlUtils_h

// Everyone else should include GlUtils.h to get the GL headers instead of
// individually including them, so if we have to change what we include,
// we can do it in one place.

#define __gl2_h_


#include <EGL/egl.h>
#include <EGL/eglext.h>
#ifdef __gl2_h_
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
static const int GL_ES_VERSION = 3;	// This will be passed to EglSetup() by App.cpp
#else
#include <GLES2/gl2.h>
static const int GL_ES_VERSION = 2;	// This will be passed to EglSetup() by App.cpp
#endif
#include <GLES2/gl2ext.h>



// TODO: unify the naming

namespace OVR
{

enum GpuType
{
	GPU_TYPE_ADRENO			= 0x1000,
	GPU_TYPE_ADRENO_330		= 0x1001,
	GPU_TYPE_ADRENO_420		= 0x1002,
	GPU_TYPE_MALI			= 0x2000,
	GPU_TYPE_MALI_T760		= 0x2001,
	GPU_TYPE_UNKNOWN		= 0xFFFF
};

struct eglSetup_t
{
	int			glEsVersion;		// 2 or 3
	GpuType		gpuType;
	EGLDisplay	display;
	EGLSurface	pbufferSurface;		// use to make context current when we don't have window surfaces
	EGLConfig	config;
	EGLContext	context;
};

// Create an appropriate config, a tiny pbuffer surface, and a context,
// then make it current.  This combination can be used before and after
// the actual window surfaces are available.
// egl.context will be EGL_NO_CONTEXT if there was an error.
// requestedGlEsVersion can be 2 or 3.  If 3 is requested, 2 might still
// be returned in eglSetup_t.glEsVersion.
//
// If contextPriority == EGL_CONTEXT_PRIORITY_MID_IMG, then no priority
// attribute will be set, otherwise EGL_CONTEXT_PRIORITY_LEVEL_IMG will
// be set with contextPriority.
eglSetup_t	EglSetup( const EGLContext shareContext,
		const int requestedGlEsVersion,
		const int redBits, const int greenBits, const int blueBits,
		const int depthBits, const int multisamples,
		const GLuint contextPriority );

void	EglShutdown( eglSetup_t & eglr );

// Uses a KHR_Sync object if available, so drivers don't "optimze" it away.
void GL_Finish();

// Prints the time the finish took to the log.
void GL_TimedFinish( const char * title );
void GL_TimedFlush( const char * title );

// Uses a KHR_Sync object if available, so drivers don't "optimze" it away.
void GL_Flush();

// Dumps information on all the available configs for the display to the log.
void DumpEglConfigs( const EGLDisplay display );

// We do NOT want to use eglChooseConfig, because the Android EGL code pushes in
// multisample flags behind our back if the user has selected the "force 4x MSAA"
// option in settings, and that is completely wasted for our warp target.
// Returns 0 if no matching config was found.
EGLConfig ChooseColorConfig( const EGLDisplay display, const int redBits,
		const int greeBits, const int blueBits, const int depthBits, const int samples, const bool pbuffer );

// For TimeWarp to make a compatible context with the main app, which might be a Unity app
// that didn't use ChooseColorConfig, we need to query the configID, then find the
// EGLConfig with the matching configID.
EGLConfig EglConfigForConfigID( const EGLDisplay display, const GLint configID );

// Calls eglGetError() and returns a string for the enum, "EGL_NOT_INITIALIZED", etc.
const char * EglErrorString();

// Returns a string for the given enum, "GL_INVALID_VALUE", etc.
const char * GL_ErrorForEnum( const GLenum e );

// Calls glGetError() and logs messages until GL_NO_ERROR is returned.
// LOG( "%s GL Error: %s", logTitle, GL_ErrorForEnum( err ) );
// Returns false if no errors were found.
bool GL_CheckErrors( const char * logTitle );

// Logs a NOT FOUND message if the name is not found.
void *GetExtensionProc( const char * name );

// Logs a Found / Not found message after checking for the extension.
bool ExtensionStringPresent( const char * extension, const char * allExtensions );

// Split the space separated OpenGL extension string into one extension per line.
// Returns a string allocated with new char[] that the caller can delete[].
const char * ExtensionsStringInColumns( const char * allExtensions );

// Looks up all the extensions available.
void GL_FindExtensions();

// Looks up and returns the GPU Type.
GpuType EglGetGpuType();

// Use EXT_discard_framebuffer or ES 3.0's glInvalidateFramebuffer as available
// This requires the isFBO parameter because GL ES 3.0's glInvalidateFramebuffer() uses
// different attachment values for FBO vs default framebuffer, unlike glDiscardFramebufferEXT()
enum invalidateTarget_t {
	INV_DEFAULT,
	INV_FBO
};
void GL_InvalidateFramebuffer( const invalidateTarget_t isFBO, const bool colorBuffer, const bool depthBuffer );

}	// namespace OVR

// extensions

// IMG_multisampled_render_to_texture
// If GL_EXT_multisampled_render_to_texture is found instead,
// they will be assigned here as well.
extern bool	IMG_multisampled_render_to_texture;
extern PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMG glRenderbufferStorageMultisampleIMG_;
extern PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMG glFramebufferTexture2DMultisampleIMG_;

extern PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR_;
extern PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR_;
extern PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR_;
extern PFNEGLSIGNALSYNCKHRPROC eglSignalSyncKHR_;
extern PFNEGLGETSYNCATTRIBKHRPROC eglGetSyncAttribKHR_;

// EGL_KHR_image_base
extern PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR_;
extern PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR_;

extern PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES_;

// GL_OES_vertex_array_object
extern bool OES_vertex_array_object;
extern PFNGLBINDVERTEXARRAYOESPROC	glBindVertexArrayOES_;
extern PFNGLDELETEVERTEXARRAYSOESPROC	glDeleteVertexArraysOES_;
extern PFNGLGENVERTEXARRAYSOESPROC	glGenVertexArraysOES_;
extern PFNGLISVERTEXARRAYOESPROC	glIsVertexArrayOES_;

// QCOM_tiled_rendering
extern bool QCOM_tiled_rendering;
extern PFNGLSTARTTILINGQCOMPROC		glStartTilingQCOM_;
extern PFNGLENDTILINGQCOMPROC		glEndTilingQCOM_;

// QCOM_binning_control
#define GL_BINNING_CONTROL_HINT_QCOM           0x8FB0

#define GL_CPU_OPTIMIZED_QCOM                  0x8FB1
#define GL_GPU_OPTIMIZED_QCOM                  0x8FB2
#define GL_RENDER_DIRECT_TO_FRAMEBUFFER_QCOM   0x8FB3
#define GL_DONT_CARE                           0x1100

// EXT_disjoint_timer_query
extern bool	EXT_disjoint_timer_query;

typedef long long GLint64;
typedef unsigned long long GLuint64;

#define GL_QUERY_COUNTER_BITS_EXT         0x8864
#define GL_CURRENT_QUERY_EXT              0x8865
#define GL_QUERY_RESULT_EXT               0x8866
#define GL_QUERY_RESULT_AVAILABLE_EXT     0x8867
#define GL_TIME_ELAPSED_EXT               0x88BF
#define GL_TIMESTAMP_EXT                  0x8E28
#define GL_GPU_DISJOINT_EXT               0x8FBB
typedef void (GL_APIENTRYP PFNGLGENQUERIESEXTPROC) (GLsizei n, GLuint *ids);
typedef void (GL_APIENTRYP PFNGLDELETEQUERIESEXTPROC) (GLsizei n, const GLuint *ids);
typedef GLboolean (GL_APIENTRYP PFNGLISQUERYEXTPROC) (GLuint id);
typedef void (GL_APIENTRYP PFNGLBEGINQUERYEXTPROC) (GLenum target, GLuint id);
typedef void (GL_APIENTRYP PFNGLENDQUERYEXTPROC) (GLenum target);
typedef void (GL_APIENTRYP PFNGLQUERYCOUNTEREXTPROC) (GLuint id, GLenum target);
typedef void (GL_APIENTRYP PFNGLGETQUERYIVEXTPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP PFNGLGETQUERYOBJECTIVEXTPROC) (GLuint id, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP PFNGLGETQUERYOBJECTUIVEXTPROC) (GLuint id, GLenum pname, GLuint *params);
typedef void (GL_APIENTRYP PFNGLGETQUERYOBJECTI64VEXTPROC) (GLuint id, GLenum pname, GLint64 *params);
typedef void (GL_APIENTRYP PFNGLGETQUERYOBJECTUI64VEXTPROC) (GLuint id, GLenum pname, GLuint64 *params);
typedef void (GL_APIENTRYP PFNGLGETINTEGER64VPROC) (GLenum pname, GLint64 *params);


extern PFNGLGENQUERIESEXTPROC glGenQueriesEXT_;
extern PFNGLDELETEQUERIESEXTPROC glDeleteQueriesEXT_;
extern PFNGLISQUERYEXTPROC glIsQueryEXT_;
extern PFNGLBEGINQUERYEXTPROC glBeginQueryEXT_;
extern PFNGLENDQUERYEXTPROC glEndQueryEXT_;
extern PFNGLQUERYCOUNTEREXTPROC glQueryCounterEXT_;
extern PFNGLGETQUERYIVEXTPROC glGetQueryivEXT_;
extern PFNGLGETQUERYOBJECTIVEXTPROC glGetQueryObjectivEXT_;
extern PFNGLGETQUERYOBJECTUIVEXTPROC glGetQueryObjectuivEXT_;
extern PFNGLGETQUERYOBJECTI64VEXTPROC glGetQueryObjecti64vEXT_;
extern PFNGLGETQUERYOBJECTUI64VEXTPROC glGetQueryObjectui64vEXT_;
extern PFNGLGETINTEGER64VPROC glGetInteger64v_;

// EGL_KHR_gl_colorspace
static const int EGL_GL_COLORSPACE_KHR			= 0x309D;
static const int EGL_GL_COLORSPACE_SRGB_KHR		= 0x3089;
static const int EGL_GL_COLORSPACE_LINEAR_KHR	= 0x308A;

// EXT_sRGB_write_control
static const int GL_FRAMEBUFFER_SRGB_EXT		= 0x8DB9;

// EXT_sRGB_decode
extern bool HasEXT_sRGB_texture_decode;
static const int GL_TEXTURE_SRGB_DECODE_EXT		= 0x8A48;
static const int GL_DECODE_EXT             		= 0x8A49;
static const int GL_SKIP_DECODE_EXT        		= 0x8A4A;

// To link against the ES2 library for UE4, we need to make our own versions of these
typedef void (GL_APIENTRYP PFNGLBLITFRAMEBUFFER_) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef void (GL_APIENTRYP PFNGLRENDERBUFFERSTORAGEMULTISAMPLE_) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GL_APIENTRYP PFNGLINVALIDATEFRAMEBUFFER_) (GLenum target, GLsizei numAttachments, const GLenum* attachments);
typedef GLvoid* (GL_APIENTRYP PFNGLMAPBUFFERRANGE_) (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef GLboolean (GL_APIENTRYP PFNGLUNMAPBUFFEROESPROC_) (GLenum target);

extern PFNGLBLITFRAMEBUFFER_	glBlitFramebuffer_;
extern PFNGLRENDERBUFFERSTORAGEMULTISAMPLE_ glRenderbufferStorageMultisample_;
extern PFNGLINVALIDATEFRAMEBUFFER_	glInvalidateFramebuffer_;
extern PFNGLMAPBUFFERRANGE_		glMapBufferRange_;
extern PFNGLUNMAPBUFFEROESPROC_ glUnmapBuffer_;


#endif	// OVR_GlUtils_h
