// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

// #version 120 at the beginning is added in FSlateOpenGLShader::CompileShader()

uniform mat4 ViewProjectionMatrix;

// Per vertex
attribute vec4 InTexCoords;
attribute vec2 InPosition;
attribute vec2 InClipOrigin;
attribute vec4 InClipExtents;
attribute vec4 InColor;

// Between vertex and pixel shader
varying vec4 TexCoords;
varying vec4 ClipOriginAndPos;
varying vec4 ClipExtents;
varying vec4 Color;

void main()
{
	TexCoords = InTexCoords;
	ClipOriginAndPos = vec4(InClipOrigin, InPosition);
	ClipExtents = InClipExtents;
	Color = InColor;

	gl_Position = ViewProjectionMatrix * vec4( InPosition, 0, 1 );
}
