// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * A resource for rendering a UMaterial in Slate
 */
class FSlateMaterialResource : public FSlateShaderResource
{
public:
	FSlateMaterialResource(UMaterialInterface& InMaterialResource, const FVector2D& InImageSize);
	~FSlateMaterialResource();

	virtual uint32 GetWidth() const override { return Width; }
	virtual uint32 GetHeight() const override { return Height; }
	virtual ESlateShaderResource::Type GetType() const override { return ESlateShaderResource::Material; }

	void UpdateMaterial(UMaterialInterface& InMaterialResource, const FVector2D& InImageSize);

	/** Updates the rendering resource */
	void UpdateRenderResource(FMaterialRenderProxy* Proxy) { RenderProxy = Proxy; }

	/** @return The material render proxy */
	FMaterialRenderProxy* GetRenderProxy() const { return RenderProxy; }

	/** @return the material object */
	UMaterialInterface* GetMaterialObject() const { return MaterialObject; }

public:
	class UMaterialInterface* MaterialObject;
	/** Rendering thread version of the material object */
	FMaterialRenderProxy* RenderProxy;
	/** Slate proxy used for batching the material */
	FSlateShaderResourceProxy* SlateProxy;

	uint32 Width;
	uint32 Height;
};

