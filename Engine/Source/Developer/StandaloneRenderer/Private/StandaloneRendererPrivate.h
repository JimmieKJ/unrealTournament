// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "SlateCore.h"

DECLARE_LOG_CATEGORY_EXTERN(LogStandaloneRenderer, Log, All);

#include "StandaloneRendererPlatformHeaders.h"

#if PLATFORM_WINDOWS
#include "Windows/D3D/SlateD3DTextures.h"
#include "Windows/D3D/SlateD3DConstantBuffer.h"
#include "Windows/D3D/SlateD3DShaders.h"
#include "Windows/D3D/SlateD3DIndexBuffer.h"
#include "Windows/D3D/SlateD3DVertexBuffer.h"
#endif

#include "OpenGL/SlateOpenGLExtensions.h"
#include "OpenGL/SlateOpenGLTextures.h"
#include "OpenGL/SlateOpenGLShaders.h"
#include "OpenGL/SlateOpenGLIndexBuffer.h"
#include "OpenGL/SlateOpenGLVertexBuffer.h"

#include "StandaloneRenderer.h"
