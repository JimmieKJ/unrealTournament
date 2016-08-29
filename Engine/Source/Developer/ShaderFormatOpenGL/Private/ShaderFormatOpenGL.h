// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

enum GLSLVersion 
{
	GLSL_150,
	GLSL_430,
	GLSL_ES2,
	GLSL_ES2_WEBGL,
	GLSL_150_ES2,	// ES2 Emulation
	GLSL_150_ES2_NOUB,	// ES2 Emulation with NoUBs
	GLSL_150_ES3_1,	// ES3.1 Emulation
	GLSL_ES2_IOS,
	GLSL_150_MAC, // Apple only
	GLSL_310_ES_EXT,
	GLSL_ES3_1_ANDROID,
};

extern void CompileShader_Windows_OGL(const struct FShaderCompilerInput& Input,struct FShaderCompilerOutput& Output,const class FString& WorkingDirectory, GLSLVersion Version);
