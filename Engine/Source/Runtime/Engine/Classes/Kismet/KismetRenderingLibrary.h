// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KismetRenderingLibrary.generated.h"

USTRUCT(BlueprintType)
struct FDrawToRenderTargetContext
{
	GENERATED_USTRUCT_BODY()

	FDrawToRenderTargetContext() :
		RenderTarget(NULL),
		DrawEvent(NULL)
	{}

	UPROPERTY()
	UTextureRenderTarget2D* RenderTarget;

	TDrawEvent<FRHICommandList>* DrawEvent;
};

UCLASS(MinimalAPI)
class UKismetRenderingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()
	

	/** 
	 * Clears the specified render target with the given ClearColor.
	 */
	UFUNCTION(BlueprintCallable, Category="Rendering", meta=(Keywords="ClearRenderTarget", WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
	static ENGINE_API void ClearRenderTarget2D(UObject* WorldContextObject, UTextureRenderTarget2D* TextureRenderTarget, FLinearColor ClearColor);

	/**
	 * Creates a new render target and initializes it to the specified dimensions
	 */
	UFUNCTION(BlueprintCallable, Category="Rendering", meta=(WorldContext="WorldContextObject"))
	static ENGINE_API UTextureRenderTarget2D* CreateRenderTarget2D(UObject* WorldContextObject, int32 Width = 256, int32 Height = 256);

	/** 
	 * Renders a quad with the material applied to the specified render target.   
	 * This sets the render target even if it is already set, which is an expensive operation. 
	 * Use BeginDrawCanvasToRenderTarget / EndDrawCanvasToRenderTarget instead if rendering multiple primitives to the same render target.
	 */
	UFUNCTION(BlueprintCallable, Category="Rendering", meta=(Keywords="DrawMaterialToRenderTarget", WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
	static ENGINE_API void DrawMaterialToRenderTarget(UObject* WorldContextObject, UTextureRenderTarget2D* TextureRenderTarget, UMaterialInterface* Material);

	/**
	* Exports a render target as a HDR image onto the disk.
	*/
	UFUNCTION(BlueprintCallable, Category = "Rendering", meta = (Keywords = "ExportRenderTarget", WorldContext = "WorldContextObject"))
	static ENGINE_API void ExportRenderTarget(UObject* WorldContextObject, UTextureRenderTarget2D* TextureRenderTarget, const FString& FilePath, const FString& FileName);

	/**
	* Exports a Texture2D as a HDR image onto the disk.
	*/
	UFUNCTION(BlueprintCallable, Category = "Rendering", meta = (Keywords = "ExportTexture2D", WorldContext = "WorldContextObject"))
	static ENGINE_API void ExportTexture2D(UObject* WorldContextObject, UTexture2D* Texture, const FString& FilePath, const FString& FileName);

	/** 
	 * Returns a Canvas object that can be used to draw to the specified render target.
	 * Canvas has functions like DrawMaterial with size parameters that can be used to draw to a specific area of a render target.
	 * Be sure to call EndDrawCanvasToRenderTarget to complete the rendering!
	 */
	UFUNCTION(BlueprintCallable, Category="Rendering", meta=(Keywords="BeginDrawCanvasToRenderTarget", WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
	static ENGINE_API void BeginDrawCanvasToRenderTarget(UObject* WorldContextObject, UTextureRenderTarget2D* TextureRenderTarget, UCanvas*& Canvas, FVector2D& Size, FDrawToRenderTargetContext& Context);

	/**  
	 * Must be paired with a BeginDrawCanvasToRenderTarget to complete rendering to a render target.
	 */
	UFUNCTION(BlueprintCallable, Category="Rendering", meta=(Keywords="EndDrawCanvasToRenderTarget", WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
	static ENGINE_API void EndDrawCanvasToRenderTarget(UObject* WorldContextObject, const FDrawToRenderTargetContext& Context);
};
