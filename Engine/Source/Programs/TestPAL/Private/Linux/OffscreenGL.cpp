// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "TestPALLog.h"
#include <GL/glcorearb.h>
#include <GL/glext.h>
#include "SDL.h"

// subset of GL functions
/** List all OpenGL entry points used by Unreal. */
#define ENUM_GL_ENTRYPOINTS(EnumMacro) \
	EnumMacro(PFNGLENABLEPROC, glEnable) \
	EnumMacro(PFNGLDISABLEPROC, glDisable) \
	EnumMacro(PFNGLCLEARPROC, glClear) \
	EnumMacro(PFNGLCLEARCOLORPROC, glClearColor) \
	EnumMacro(PFNGLDRAWARRAYSPROC, glDrawArrays) \
	EnumMacro(PFNGLGENBUFFERSARBPROC, glGenBuffers) \
	EnumMacro(PFNGLBINDBUFFERARBPROC, glBindBuffer) \
	EnumMacro(PFNGLBUFFERDATAARBPROC, glBufferData) \
	EnumMacro(PFNGLDELETEBUFFERSARBPROC, glDeleteBuffers) \
	EnumMacro(PFNGLMAPBUFFERARBPROC, glMapBuffer) \
	EnumMacro(PFNGLUNMAPBUFFERARBPROC, glDrawRangeElements) \
	EnumMacro(PFNGLBINDTEXTUREPROC, glBindTexture) \
	EnumMacro(PFNGLACTIVETEXTUREARBPROC, glActiveTexture) \
	EnumMacro(PFNGLTEXIMAGE2DPROC, glTexImage2D) \
	EnumMacro(PFNGLGENTEXTURESPROC, glGenTextures) \
	EnumMacro(PFNGLCREATESHADERPROC, glCreateShader) \
	EnumMacro(PFNGLSHADERSOURCEPROC, glShaderSource) \
	EnumMacro(PFNGLCOMPILESHADERPROC, glCompileShader) \
	EnumMacro(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
	EnumMacro(PFNGLATTACHSHADERPROC, glAttachShader) \
	EnumMacro(PFNGLDETACHSHADERPROC, glDetachShader) \
	EnumMacro(PFNGLLINKPROGRAMPROC, glLinkProgram) \
	EnumMacro(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) \
	EnumMacro(PFNGLUSEPROGRAMPROC, glUseProgram) \
	EnumMacro(PFNGLDELETESHADERPROC, glDeleteShader) \
	EnumMacro(PFNGLCREATEPROGRAMPROC,glCreateProgram) \
	EnumMacro(PFNGLDELETEPROGRAMPROC, glDeleteProgram) \
	EnumMacro(PFNGLGETSHADERIVPROC, glGetShaderiv) \
	EnumMacro(PFNGLGETPROGRAMIVPROC, glGetProgramiv) \
	EnumMacro(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) \
	EnumMacro(PFNGLUNIFORM1FPROC, glUniform1f) \
	EnumMacro(PFNGLUNIFORM2FPROC, glUniform2f) \
	EnumMacro(PFNGLUNIFORM3FPROC, glUniform3f) \
	EnumMacro(PFNGLUNIFORM4FPROC, glUniform4f) \
	EnumMacro(PFNGLUNIFORM1IPROC, glUniform1i) \
	EnumMacro(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv) \
	EnumMacro(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
	EnumMacro(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation) \
	EnumMacro(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
	EnumMacro(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray) \
	EnumMacro(PFNGLGETATTRIBLOCATIONPROC, glGetAttribLocation) \
	EnumMacro(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
	EnumMacro(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays) \
	EnumMacro(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
	EnumMacro(PFNGLISVERTEXARRAYPROC, glIsVertexArray) \
	EnumMacro(PFNGLTEXPARAMETERIPROC, glTexParameteri)

#define DEFINE_GL_ENTRYPOINTS(Type,Func) Type Func = NULL;
namespace GLFuncPointers	// see explanation in OpenGLLinux.h why we need the namespace
{
	ENUM_GL_ENTRYPOINTS(DEFINE_GL_ENTRYPOINTS);
};

using namespace GLFuncPointers;

void DrawOffscreenGL()
{
	// init
	if (!FPlatformMisc::PlatformInitMultimedia())
	{
		UE_LOG(LogTestPAL, Fatal, TEXT("Could not init multimedia."));
	}

	const int kWidth = 256;
	const int kHeight = 256;
	SDL_Window* Window = SDL_CreateWindow(nullptr,
									SDL_WINDOWPOS_CENTERED,
									SDL_WINDOWPOS_CENTERED,
									kWidth,
									kHeight,
									SDL_WINDOW_BORDERLESS | SDL_WINDOW_OPENGL
									);
	checkf(Window != nullptr, TEXT("Unable to create a GL window!"));

	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 2 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute( SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0 );

	SDL_GLContext Context = SDL_GL_CreateContext(Window);
	checkf(Context != nullptr, TEXT("Unable to create a GL context!"));

#define GET_GL_ENTRYPOINTS(Type,Func) { GLFuncPointers::Func = reinterpret_cast<Type>(SDL_GL_GetProcAddress(#Func)); check(GLFuncPointers::Func != nullptr); }
	ENUM_GL_ENTRYPOINTS(GET_GL_ENTRYPOINTS);

	bool bMadeCurrent = SDL_GL_MakeCurrent(Window, Context) == 0;
	checkf(bMadeCurrent, TEXT("Unable to set GL context!"));

	UE_LOG(LogTestPAL, Display, TEXT("Everything initialized!"));

	// destroy
	if (Context)
	{
		SDL_GL_DeleteContext(Context);
		Context = nullptr;
	}

	if (Window)
	{
		SDL_DestroyWindow(Window);
		Window = nullptr;
	}

	UE_LOG(LogTestPAL, Display, TEXT("All Ok!"));
}

