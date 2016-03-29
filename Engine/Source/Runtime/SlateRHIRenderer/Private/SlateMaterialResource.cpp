// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateRHIRendererPrivatePCH.h"
#include "SlateMaterialResource.h"


FSlateMaterialResource::FSlateMaterialResource(const UMaterialInterface& InMaterial, const FVector2D& InImageSize, FSlateShaderResource* InTextureMask )
	: MaterialObject( &InMaterial )
	, SlateProxy( new FSlateShaderResourceProxy )
	, TextureMaskResource( InTextureMask )
	, Width(FMath::RoundToInt(InImageSize.X))
	, Height(FMath::RoundToInt(InImageSize.Y))
{
	SlateProxy->ActualSize = InImageSize.IntPoint();
	SlateProxy->Resource = this;
}

FSlateMaterialResource::~FSlateMaterialResource()
{
	if(SlateProxy)
	{
		delete SlateProxy;
	}
}

void FSlateMaterialResource::UpdateMaterial(const UMaterialInterface& InMaterialResource, const FVector2D& InImageSize, FSlateShaderResource* InTextureMask )
{
	MaterialObject = &InMaterialResource;
	if( !SlateProxy )
	{
		SlateProxy = new FSlateShaderResourceProxy;
	}

	TextureMaskResource = InTextureMask;

	SlateProxy->ActualSize = InImageSize.IntPoint();
	SlateProxy->Resource = this;

	Width = FMath::RoundToInt(InImageSize.X);
	Height = FMath::RoundToInt(InImageSize.Y);
}

void FSlateMaterialResource::ResetMaterial()
{
	MaterialObject = nullptr;
	TextureMaskResource = nullptr;
	if (SlateProxy)
	{
		delete SlateProxy;
	}
	SlateProxy = nullptr;
	Width = 0;
	Height = 0;
}