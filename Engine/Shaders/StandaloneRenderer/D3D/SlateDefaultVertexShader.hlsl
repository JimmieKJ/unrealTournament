// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.



cbuffer PerElementVSConstants
{
	matrix WorldViewProjection;
}

struct VertexOut
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR0;
	float4 TextureCoordinates : TEXCOORD0;
	float4 ClipOriginAndPos : TEXCOORD1;
	float4 ClipExtents : TEXCOORD2;
};

VertexOut Main(
	in int2 InPosition : POSITION,
	in float4 InTextureCoordinates : TEXCOORD0,
	in half2 InClipOrigin : TEXCOORD1,
	in half4 InClipExtents : TEXCOORD2,
	in float4 InColor : COLOR0
	) 
{
	VertexOut Out;

	Out.Position = mul( WorldViewProjection,float4( InPosition.xy, 0, 1 ) );

	Out.TextureCoordinates = InTextureCoordinates;
	Out.ClipOriginAndPos = float4(InClipOrigin, InPosition.xy);
	Out.ClipExtents = InClipExtents;
	
	Out.Color = InColor;

	return Out;
}
