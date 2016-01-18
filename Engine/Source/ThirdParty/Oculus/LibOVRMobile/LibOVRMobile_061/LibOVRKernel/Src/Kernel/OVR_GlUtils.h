/************************************************************************************

Filename    :   OVR_GlUtils.h
Content     :   Policy-free OpenGL convenience functions
Created     :   August 24, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/
#ifndef OVR_GlUtils_h
#define OVR_GlUtils_h

// Everyone else should include OVR_GlUtils.h to get the GL headers instead of
// individually including them, so if we have to change what we include,
// we can do it in one place.

#if defined( ANDROID )	// FIXME: Use OVR_Types defines when used consistently

#define OVR_USE_OPENGL_LOADER

#if defined(OVR_USE_OPENGL_LOADER)
	#define __gl2_h_
	#include <EGL/egl.h>
	#include <EGL/eglext.h>
	#include <GLES3/gl3_loader.h>
	#include <GLES2/gl2ext.h>
	static const int GL_ES_VERSION = 3;	// This will be passed to EglSetup() by App.cpp
	using namespace GLES3;
#else
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
#endif

// We need to detect the API level because Google tweaked some of the GL headers in version 21+
#include <android/api-level.h>
#if __ANDROID_API__ < 21
typedef khronos_int64_t GLint64;
typedef khronos_uint64_t GLuint64;
#endif

#if !defined( GL_EXT_multisampled_render_to_texture )
typedef void (GL_APIENTRY* PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GL_APIENTRY* PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples);
#endif

extern bool OES_vertex_array_object;
extern bool QCOM_tiled_rendering;
extern bool EXT_texture_filter_anisotropic;
extern bool	EXT_disjoint_timer_query;
extern bool HasEXT_sRGB_texture_decode;

// extensions

// IMG_multisampled_render_to_texture
// If GL_EXT_multisampled_render_to_texture is found instead,
// they will be assigned here as well.
extern PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC   glRenderbufferStorageMultisampleEXT_;
extern PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC  glFramebufferTexture2DMultisampleEXT_;

// EGL_KHR_reusable_sync
extern PFNEGLCREATESYNCKHRPROC			eglCreateSyncKHR_;
extern PFNEGLDESTROYSYNCKHRPROC			eglDestroySyncKHR_;
extern PFNEGLCLIENTWAITSYNCKHRPROC		eglClientWaitSyncKHR_;
extern PFNEGLSIGNALSYNCKHRPROC			eglSignalSyncKHR_;
extern PFNEGLGETSYNCATTRIBKHRPROC		eglGetSyncAttribKHR_;

// GL_OES_vertex_array_object
extern PFNGLBINDVERTEXARRAYOESPROC		glBindVertexArrayOES_;
extern PFNGLDELETEVERTEXARRAYSOESPROC	glDeleteVertexArraysOES_;
extern PFNGLGENVERTEXARRAYSOESPROC		glGenVertexArraysOES_;
extern PFNGLISVERTEXARRAYOESPROC		glIsVertexArrayOES_;

// QCOM_tiled_rendering
extern PFNGLSTARTTILINGQCOMPROC			glStartTilingQCOM_;
extern PFNGLENDTILINGQCOMPROC			glEndTilingQCOM_;

// QCOM_binning_control
#define GL_BINNING_CONTROL_HINT_QCOM           0x8FB0

#define GL_CPU_OPTIMIZED_QCOM                  0x8FB1
#define GL_GPU_OPTIMIZED_QCOM                  0x8FB2
#define GL_RENDER_DIRECT_TO_FRAMEBUFFER_QCOM   0x8FB3
#define GL_DONT_CARE                           0x1100

// EXT_disjoint_timer_query
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

extern PFNGLGENQUERIESEXTPROC				glGenQueriesEXT_;
extern PFNGLDELETEQUERIESEXTPROC			glDeleteQueriesEXT_;
extern PFNGLISQUERYEXTPROC					glIsQueryEXT_;
extern PFNGLBEGINQUERYEXTPROC				glBeginQueryEXT_;
extern PFNGLENDQUERYEXTPROC					glEndQueryEXT_;
extern PFNGLQUERYCOUNTEREXTPROC				glQueryCounterEXT_;
extern PFNGLGETQUERYIVEXTPROC				glGetQueryivEXT_;
extern PFNGLGETQUERYOBJECTIVEXTPROC			glGetQueryObjectivEXT_;
extern PFNGLGETQUERYOBJECTUIVEXTPROC		glGetQueryObjectuivEXT_;
extern PFNGLGETQUERYOBJECTI64VEXTPROC		glGetQueryObjecti64vEXT_;
extern PFNGLGETQUERYOBJECTUI64VEXTPROC		glGetQueryObjectui64vEXT_;
extern PFNGLGETINTEGER64VPROC				glGetInteger64v_;

// EGL_KHR_gl_colorspace
static const int EGL_GL_COLORSPACE_KHR			= 0x309D;
static const int EGL_GL_COLORSPACE_SRGB_KHR		= 0x3089;
static const int EGL_GL_COLORSPACE_LINEAR_KHR	= 0x308A;

// EXT_sRGB_write_control
#if !defined(GL_EXT_sRGB_write_control)
static const int GL_FRAMEBUFFER_SRGB_EXT		= 0x8DB9;
#endif

// EXT_sRGB_decode
#if !defined(GL_EXT_texture_sRGB_decode)
static const int GL_TEXTURE_SRGB_DECODE_EXT		= 0x8A48;
static const int GL_DECODE_EXT             		= 0x8A49;
static const int GL_SKIP_DECODE_EXT        		= 0x8A4A;
#endif

// To link against the ES2 library for UE4, we need to make our own versions of these
typedef void (GL_APIENTRYP PFNGLBLITFRAMEBUFFER_) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef void (GL_APIENTRYP PFNGLRENDERBUFFERSTORAGEMULTISAMPLE_) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GL_APIENTRYP PFNGLINVALIDATEFRAMEBUFFER_) (GLenum target, GLsizei numAttachments, const GLenum* attachments);
typedef GLvoid* (GL_APIENTRYP PFNGLMAPBUFFERRANGE_) (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef GLboolean (GL_APIENTRYP PFNGLUNMAPBUFFEROESPROC_) (GLenum target);

extern PFNGLBLITFRAMEBUFFER_				glBlitFramebuffer_;
extern PFNGLRENDERBUFFERSTORAGEMULTISAMPLE_	glRenderbufferStorageMultisample_;
extern PFNGLINVALIDATEFRAMEBUFFER_			glInvalidateFramebuffer_;
extern PFNGLMAPBUFFERRANGE_					glMapBufferRange_;
extern PFNGLUNMAPBUFFEROESPROC_				glUnmapBuffer_;

// GPU Trust Zone support
#ifndef EGL_PROTECTED_CONTENT_EXT
static const int EGL_PROTECTED_CONTENT_EXT = 0x32c0;
#endif

#elif defined( WIN32 ) || defined( WIN64 ) || defined( _WIN32 ) || defined( _WIN64 )

#include <windows.h>
#include <GL/gl.h>
#define GL_EXT_color_subtable
#include <GL/glext.h>
#include <GL/wglext.h>

#if !defined( GL_EXT_multisampled_render_to_texture )
typedef void (APIENTRY* PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRY* PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples);
#endif

// Used to aide porting process
typedef void *EGLDisplay;
typedef void *EGLSurface;
struct ANativeWindow;
typedef struct ANativeWindow ANativeWindow;
#define EGL_NO_SURFACE (EGLSurface)0

extern PFNGLBINDFRAMEBUFFERPROC				glBindFramebuffer;
extern PFNGLGENFRAMEBUFFERSPROC				glGenFramebuffers;
extern PFNGLDELETEFRAMEBUFFERSPROC			glDeleteFramebuffers;
extern PFNGLBLITFRAMEBUFFERPROC				glBlitFramebuffer;
extern PFNGLGENRENDERBUFFERSPROC			glGenRenderbuffers;
extern PFNGLDELETERENDERBUFFERSPROC			glDeleteRenderbuffers;
extern PFNGLBINDRENDERBUFFERPROC			glBindRenderbuffer;
extern PFNGLISRENDERBUFFERPROC				glIsRenderbuffer;
extern PFNGLRENDERBUFFERSTORAGEPROC			glRenderbufferStorage;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC		glFramebufferRenderbuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC		glFramebufferTexture2D;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC		glCheckFramebufferStatus;
extern PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glRenderbufferStorageMultisample;
extern PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC   glRenderbufferStorageMultisampleEXT_;
extern PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC  glFramebufferTexture2DMultisampleEXT_;

extern PFNGLGENBUFFERSPROC					glGenBuffers;
extern PFNGLDELETEBUFFERSPROC				glDeleteBuffers;
extern PFNGLBINDBUFFERPROC					glBindBuffer;
extern PFNGLBUFFERDATAPROC					glBufferData;
extern PFNGLBUFFERSUBDATAPROC				glBufferSubData;

extern PFNGLMAPBUFFERRANGEPROC				glMapBufferRange;
extern PFNGLUNMAPBUFFERPROC					glUnmapBuffer;

extern PFNGLGENVERTEXARRAYSPROC				glGenVertexArrays;
extern PFNGLDELETEVERTEXARRAYSPROC			glDeleteVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC				glBindVertexArray;
extern PFNGLVERTEXATTRIBPOINTERPROC			glVertexAttribPointer;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC	glDisableVertexAttribArray;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC		glEnableVertexAttribArray;

extern PFNGLACTIVETEXTUREPROC				glActiveTexture;
extern PFNGLTEXIMAGE3DPROC					glTexImage3D;
extern PFNGLCOMPRESSEDTEXIMAGE2DPROC		glCompressedTexImage2D;
extern PFNGLCOMPRESSEDTEXIMAGE3DPROC		glCompressedTexImage3D;
extern PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC		glCompressedTexSubImage2D;
extern PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC		glCompressedTexSubImage3D;

extern PFNGLTEXSTORAGE2DPROC				glTexStorage2D;
extern PFNGLTEXSTORAGE3DPROC				glTexStorage3D;

extern PFNGLGENERATEMIPMAPPROC				glGenerateMipmap;

extern PFNGLCREATEPROGRAMPROC				glCreateProgram;
extern PFNGLCREATESHADERPROC				glCreateShader;
extern PFNGLSHADERSOURCEPROC				glShaderSource;
extern PFNGLCOMPILESHADERPROC				glCompileShader;
extern PFNGLGETSHADERIVPROC					glGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC			glGetShaderInfoLog;
extern PFNGLDELETESHADERPROC				glDeleteShader;
extern PFNGLDELETEPROGRAMPROC				glDeleteProgram;
extern PFNGLUSEPROGRAMPROC					glUseProgram;
extern PFNGLATTACHSHADERPROC				glAttachShader;
extern PFNGLLINKPROGRAMPROC					glLinkProgram;
extern PFNGLGETPROGRAMIVPROC				glGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC			glGetProgramInfoLog;
extern PFNGLBINDATTRIBLOCATIONPROC			glBindAttribLocation;
extern PFNGLGETUNIFORMLOCATIONPROC			glGetUniformLocation;
extern PFNGLUNIFORM2FVPROC					glUniform2fv;
extern PFNGLUNIFORM3FVPROC					glUniform3fv;
extern PFNGLUNIFORM4FPROC					glUniform4f;
extern PFNGLUNIFORM4FVPROC					glUniform4fv;
extern PFNGLUNIFORMMATRIX4FVPROC			glUniformMatrix4fv;
extern PFNGLUNIFORM1IPROC					glUniform1i;

extern PFNGLBLENDEQUATIONPROC				glBlendEquation;
extern PFNGLBLENDEQUATIONSEPARATEPROC		glBlendEquationSeparate;
extern PFNGLBLENDFUNCSEPARATEPROC			glBlendFuncSeparate;

extern PFNGLGENQUERIESPROC					glGenQueries;
extern PFNGLDELETEQUERIESPROC				glDeleteQueries;
extern PFNGLISQUERYPROC						glIsQuery;
extern PFNGLBEGINQUERYPROC					glBeginQuery;
extern PFNGLENDQUERYPROC					glEndQuery;
extern PFNGLQUERYCOUNTERPROC				glQueryCounter;
extern PFNGLGETQUERYIVPROC					glGetQueryiv;
extern PFNGLGETQUERYOBJECTIVPROC			glGetQueryObjectiv;
extern PFNGLGETQUERYOBJECTUIVPROC			glGetQueryObjectuiv;
extern PFNGLGETQUERYOBJECTI64VPROC			glGetQueryObjecti64v;
extern PFNGLGETQUERYOBJECTUI64VPROC			glGetQueryObjectui64v;

extern PFNGLFENCESYNCPROC					glFenceSync;
extern PFNGLCLIENTWAITSYNCPROC				glClientWaitSync;
extern PFNGLDELETESYNCPROC					glDeleteSync;
extern PFNGLISSYNCPROC						glIsSync;

extern PFNWGLCREATECONTEXTATTRIBSARBPROC	wglCreateContextAttribsARB;
extern PFNWGLSWAPINTERVALEXTPROC			wglSwapIntervalEXT;
extern PFNWGLDELAYBEFORESWAPNVPROC			wglDelayBeforeSwapNV;

#elif defined( __APPLE__ )
	#error "not implemented"
#else
	#error "unknown platform"
#endif

// TODO: unify the naming

namespace OVR
{
#if defined( ANDROID )
// For TimeWarp to make a compatible context with the main app, which might be a Unity app
// that didn't use ChooseColorConfig, we need to query the configID, then find the
// EGLConfig with the matching configID.
EGLConfig EglConfigForConfigID( const EGLDisplay display, const GLint configID );
#endif

// Calls eglGetError() and returns a string for the enum, "EGL_NOT_INITIALIZED", etc.
const char * GL_GetErrorString();

// Calls glGetError() and logs messages until GL_NO_ERROR is returned.
// LOG( "%s GL Error: %s", logTitle, GL_ErrorForEnum( err ) );
// Returns false if no errors were found.
bool GL_CheckErrors( const char * logTitle );

// Looks up all the extensions available.
void GL_InitExtensions();

// Logs a Found / Not found message after checking for the extension.
bool GL_ExtensionStringPresent( const char * extension, const char * allExtensions );

// These use a KHR_Sync object if available, so drivers can't "optimze" the finish/flush away.
void GL_Finish();
void GL_Flush();

// Use EXT_discard_framebuffer or ES 3.0's glInvalidateFramebuffer as available
// This requires the isFBO parameter because GL ES 3.0's glInvalidateFramebuffer() uses
// different attachment values for FBO vs default framebuffer, unlike glDiscardFramebufferEXT()
enum invalidateTarget_t {
	INV_DEFAULT,
	INV_FBO
};
void GL_InvalidateFramebuffer( const invalidateTarget_t isFBO, const bool colorBuffer, const bool depthBuffer );

}	// namespace OVR

#endif	// OVR_GlUtils_h
