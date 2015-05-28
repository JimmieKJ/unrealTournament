// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MeshPaintPrivatePCH.h"
#include "MeshPaintEdMode.h"
#include "MeshPaintAdapterFactory.h"
#include "MeshPaintStaticMeshAdapter.h"

#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"

//////////////////////////////////////////////////////////////////////////
// IMeshPaintGeometryAdapter

void IMeshPaintGeometryAdapter::DefaultApplyOrRemoveTextureOverride(UMeshComponent* InMeshComponent, UTexture* SourceTexture, UTexture* OverrideTexture)
{
	const ERHIFeatureLevel::Type FeatureLevel = InMeshComponent->GetWorld()->FeatureLevel;

	// Check all the materials on the mesh to see if the user texture is there
	int32 MaterialIndex = 0;
	UMaterialInterface* MaterialToCheck = InMeshComponent->GetMaterial(MaterialIndex);
	while (MaterialToCheck != nullptr)
	{
		const bool bIsTextureUsed = DoesMaterialUseTexture(MaterialToCheck, SourceTexture);
		if (bIsTextureUsed)
		{
			MaterialToCheck->OverrideTexture(SourceTexture, OverrideTexture, FeatureLevel);
		}

		++MaterialIndex;
		MaterialToCheck = InMeshComponent->GetMaterial(MaterialIndex);
	}
}

void IMeshPaintGeometryAdapter::DefaultQueryPaintableTextures(int32 MaterialIndex, UMeshComponent* MeshComponent, int32& OutDefaultIndex, TArray<struct FPaintableTexture>& InOutTextureList)
{
	OutDefaultIndex = INDEX_NONE;

	// We already know the material we are painting on, take it off the static mesh component
	UMaterialInterface* Material = MeshComponent->GetMaterial(MaterialIndex);

	if (Material != NULL)
	{
		FPaintableTexture PaintableTexture;
		// Find all the unique textures used in the top material level of the selected actor materials

		const TArray<UMaterialExpression*>& Expressions = Material->GetMaterial()->Expressions;

		// Only grab the textures from the top level of samples
		for (auto ItExpressions = Expressions.CreateConstIterator(); ItExpressions; ItExpressions++)
		{
			UMaterialExpressionTextureBase* TextureBase = Cast<UMaterialExpressionTextureBase>(*ItExpressions);
			if (TextureBase != NULL &&
				TextureBase->Texture != NULL &&
				!TextureBase->Texture->IsNormalMap())
			{
				// Default UV channel to index 0. 
				PaintableTexture = FPaintableTexture(TextureBase->Texture, 0);

				// Texture Samples can have UV's specified, check the first node for whether it has a custom UV channel set. 
				// We only check the first as the Mesh paint mode does not support painting with UV's modified in the shader.
				UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>(*ItExpressions);
				if (TextureSample != NULL)
				{
					UMaterialExpressionTextureCoordinate* TextureCoords = Cast<UMaterialExpressionTextureCoordinate>(TextureSample->Coordinates.Expression);
					if (TextureCoords != NULL)
					{
						// Store the uv channel, this is set when the texture is selected. 
						PaintableTexture.UVChannelIndex = TextureCoords->CoordinateIndex;
					}

					// Handle texture parameter expressions
					UMaterialExpressionTextureSampleParameter* TextureSampleParameter = Cast<UMaterialExpressionTextureSampleParameter>(TextureSample);
					if (TextureSampleParameter != NULL)
					{
						// Grab the overridden texture if it exists.  
						Material->GetTextureParameterValue(TextureSampleParameter->ParameterName, PaintableTexture.Texture);
					}
				}

				// note that the same texture will be added again if its UV channel differs. 
				int32 TextureIndex = InOutTextureList.AddUnique(PaintableTexture);

				// cache the first default index, if there is no previous info this will be used as the selected texture
				if ((OutDefaultIndex == INDEX_NONE) && TextureBase->IsDefaultMeshpaintTexture)
				{
					OutDefaultIndex = TextureIndex;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FMeshPaintModule

class FMeshPaintModule : public IMeshPaintModule
{
public:
	
	/**
	 * Called right after the module's DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule() override
	{
		FEditorModeRegistry::Get().RegisterMode<FEdModeMeshPaint>(
			FBuiltinEditorModes::EM_MeshPaint,
			NSLOCTEXT("MeshPaint_Mode", "MeshPaint_ModeName", "Paint"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.MeshPaintMode", "LevelEditor.MeshPaintMode.Small"),
			true, 200
			);


		RegisterGeometryAdapterFactory(MakeShareable(new FMeshPaintGeometryAdapterForStaticMeshesFactory));
	}

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void ShutdownModule() override
	{
		FEditorModeRegistry::Get().UnregisterMode(FBuiltinEditorModes::EM_MeshPaint);
	}

	virtual void RegisterGeometryAdapterFactory(TSharedRef<IMeshPaintGeometryAdapterFactory> Factory) override
	{
		FMeshPaintAdapterFactory::FactoryList.Add(Factory);
	}

	virtual void UnregisterGeometryAdapterFactory(TSharedRef<IMeshPaintGeometryAdapterFactory> Factory) override
	{
		FMeshPaintAdapterFactory::FactoryList.Remove(Factory);
	}
};

IMPLEMENT_MODULE( FMeshPaintModule, MeshPaint );

