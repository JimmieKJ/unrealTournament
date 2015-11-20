// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * A resource for rendering a UMaterial in Slate
 */
class FSlateMaterialResource : public FSlateShaderResource
{
public:
	FSlateMaterialResource(const UMaterialInterface& InMaterialResource, const FVector2D& InImageSize, FSlateShaderResource* InTextureMask = nullptr );
	~FSlateMaterialResource();

	virtual uint32 GetWidth() const override { return Width; }
	virtual uint32 GetHeight() const override { return Height; }
	virtual ESlateShaderResource::Type GetType() const override { return ESlateShaderResource::Material; }

	void UpdateMaterial(const UMaterialInterface& InMaterialResource, const FVector2D& InImageSize, FSlateShaderResource* InTextureMask );

	/** Updates the rendering resource */
	void UpdateRenderResource(FMaterialRenderProxy* Proxy) { RenderProxy = Proxy; }

	/** @return The material render proxy */
	FMaterialRenderProxy* GetRenderProxy() const { return MaterialObject->GetRenderProxy(false, false); }

	/** @return the material object */
	const UMaterialInterface* GetMaterialObject() const { return MaterialObject; }

	FSlateShaderResource* GetTextureMaskResource() const { return TextureMaskResource; }
public:
	const class UMaterialInterface* MaterialObject;
	/** Rendering thread version of the material object */
	FMaterialRenderProxy* RenderProxy;
	/** Slate proxy used for batching the material */
	FSlateShaderResourceProxy* SlateProxy;

	FSlateShaderResource* TextureMaskResource;
	uint32 Width;
	uint32 Height;
};

