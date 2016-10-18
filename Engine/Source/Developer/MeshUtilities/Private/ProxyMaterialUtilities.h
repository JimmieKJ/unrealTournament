// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace ProxyMaterialUtilities
{
#define TEXTURE_MACRO_BASE(a, b, c) \
	bool b##a##Texture = FlattenMaterial.a##Samples.Num() > 1; \
	if ( b##a##Texture) \
	{ \
		UTexture2D* a##Texture = b##a##Texture ? FMaterialUtilities::CreateTexture(InOuter, AssetBasePath + TEXT("T_") + AssetBaseName + TEXT("_" #a), FlattenMaterial.a##Size, FlattenMaterial.a##Samples, b, TEXTUREGROUP_HierarchicalLOD, RF_Public | RF_Standalone, c) : nullptr; \
		OutMaterial->SetTextureParameterValueEditorOnly(#a "Texture", a##Texture); \
		FStaticSwitchParameter SwitchParameter; \
		SwitchParameter.ParameterName = "Use" #a; \
		SwitchParameter.Value = true; \
		SwitchParameter.bOverride = true; \
		NewStaticParameterSet.StaticSwitchParameters.Add(SwitchParameter); \
		a##Texture->PostEditChange(); \
	} 

#define TEXTURE_MACRO_VECTOR(a, b, c) TEXTURE_MACRO_BASE(a, b, c)\
	else\
	{ \
		OutMaterial->SetVectorParameterValueEditorOnly(#a "Const", FlattenMaterial.a##Samples[0]); \
	} 

#define TEXTURE_MACRO_VECTOR_LINEAR(a, b, c) TEXTURE_MACRO_BASE(a, b, c)\
	else\
	{ \
		OutMaterial->SetVectorParameterValueEditorOnly(#a "Const", FlattenMaterial.a##Samples[0].ReinterpretAsLinear()); \
	} 

#define TEXTURE_MACRO_SCALAR(a, b, c) TEXTURE_MACRO_BASE(a, b, c)\
	else \
	{ \
		FLinearColor Colour = ( FlattenMaterial.a##Samples.Num() == 1 ) ? FLinearColor::FromSRGBColor( FlattenMaterial.a##Samples[0]) : FLinearColor( InMaterialProxySettings.a##Constant, 0, 0, 0 ); \
		OutMaterial->SetScalarParameterValueEditorOnly(#a "Const", Colour.R ); \
	}

	static const bool CalculatePackedTextureData(const FFlattenMaterial& InMaterial, bool& bOutPackMetallic, bool& bOutPackSpecular, bool& bOutPackRoughness, int32& OutNumSamples, FIntPoint& OutSize)
	{
		// Whether or not a material property is baked down
		const bool bHasMetallic = (InMaterial.MetallicSamples.Num() > 1);
		const bool bHasRoughness = (InMaterial.RoughnessSamples.Num() > 1);
		const bool bHasSpecular = (InMaterial.SpecularSamples.Num() > 1);

		// Check for same texture sizes
		bool bSameTextureSize = false;

		// Determine whether or not the properties sizes match
		bSameTextureSize = (InMaterial.MetallicSize == InMaterial.RoughnessSize) || (InMaterial.MetallicSize == InMaterial.SpecularSize);
		if (bSameTextureSize)
		{
			OutSize = InMaterial.MetallicSize;
			OutNumSamples = InMaterial.MetallicSamples.Num();
		}
		else
		{
			bSameTextureSize = (InMaterial.RoughnessSize == InMaterial.SpecularSize);
			if (bSameTextureSize)
			{
				OutSize = InMaterial.RoughnessSize;
				OutNumSamples = InMaterial.RoughnessSamples.Num();
			}
		}

		// Now that we know if the data matches determine whether or not we should pack the properties
		int32 NumPacked = 0;
		if (OutNumSamples != 0)
		{
			bOutPackMetallic = bHasMetallic ? (OutNumSamples == InMaterial.MetallicSamples.Num()) : false;
			NumPacked += (bOutPackMetallic) ? 1 : 0;
			bOutPackRoughness = bHasRoughness ? (OutNumSamples == InMaterial.RoughnessSamples.Num()) : false;
			NumPacked += (bOutPackRoughness) ? 1 : 0;
			bOutPackSpecular = bHasSpecular ? (OutNumSamples == InMaterial.SpecularSamples.Num()) : false;
			NumPacked += (bOutPackSpecular) ? 1 : 0;
		}
		else
		{
			bOutPackMetallic = bOutPackRoughness = bOutPackSpecular = false;
		}

		// Need atleast two properties to pack
		return NumPacked >= 2;
	}

	static UMaterialInstanceConstant* CreateProxyMaterialInstance(UPackage* InOuter, const FMaterialProxySettings& InMaterialProxySettings, FFlattenMaterial& FlattenMaterial, const FString& AssetBasePath, const FString& AssetBaseName)
	{
		UMaterial* BaseMaterial = LoadObject<UMaterial>(NULL, TEXT("/Engine/EngineMaterials/BaseFlattenMaterial.BaseFlattenMaterial"), NULL, LOAD_None, NULL);
		check(BaseMaterial);

		UMaterialInstanceConstant* OutMaterial = FMaterialUtilities::CreateInstancedMaterial(BaseMaterial, InOuter, AssetBasePath + AssetBaseName, RF_Public | RF_Standalone);

		OutMaterial->BasePropertyOverrides.TwoSided = false;
		OutMaterial->BasePropertyOverrides.bOverride_TwoSided = true;
		OutMaterial->BasePropertyOverrides.DitheredLODTransition = FlattenMaterial.bDitheredLODTransition;
		OutMaterial->BasePropertyOverrides.bOverride_DitheredLODTransition = true;

		bool bPackMetallic, bPackSpecular, bPackRoughness;
		int32 NumSamples = 0;
		FIntPoint PackedSize;
		const bool bPackTextures = CalculatePackedTextureData(FlattenMaterial, bPackMetallic, bPackSpecular, bPackRoughness, NumSamples, PackedSize);

		const bool bSRGB = true;
		const bool bRGB = false;

		FStaticParameterSet NewStaticParameterSet;

		// Load textures and set switches accordingly
		if (FlattenMaterial.DiffuseSamples.Num() > 0 && !(FlattenMaterial.DiffuseSamples.Num() == 1 && FlattenMaterial.DiffuseSamples[0] == FColor::Black))
		{
			TEXTURE_MACRO_VECTOR(Diffuse, TC_Default, bSRGB);
		}

		if (FlattenMaterial.NormalSamples.Num() > 1)
		{
			TEXTURE_MACRO_BASE(Normal, TC_Normalmap, bRGB)
		}

		// Determine whether or not specific material properties are packed together into one texture (requires at least two to match (number of samples and texture size) in order to be packed
		if (!bPackMetallic && (FlattenMaterial.MetallicSamples.Num() > 0 || !InMaterialProxySettings.bMetallicMap))
		{
			TEXTURE_MACRO_SCALAR(Metallic, TC_Default, bSRGB);
		}

		if (!bPackRoughness && (FlattenMaterial.RoughnessSamples.Num() > 0 || !InMaterialProxySettings.bRoughnessMap))
		{
			TEXTURE_MACRO_SCALAR(Roughness, TC_Default, bSRGB);
		}

		if (!bPackSpecular && (FlattenMaterial.SpecularSamples.Num() > 0 || !InMaterialProxySettings.bSpecularMap))
		{
			TEXTURE_MACRO_SCALAR(Specular, TC_Default, bSRGB);
		}

		// Handle the packed texture if applicable
		if (bPackTextures)
		{
			TArray<FColor> MergedTexture;
			MergedTexture.AddZeroed(NumSamples);

			// Merge properties into one texture using the separate colour channels
			if (bPackMetallic) for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex) { MergedTexture[SampleIndex].R = FlattenMaterial.MetallicSamples[SampleIndex].R; }

			if (bPackRoughness) for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex) { MergedTexture[SampleIndex].G = FlattenMaterial.RoughnessSamples[SampleIndex].R; }

			if (bPackSpecular) for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex) { MergedTexture[SampleIndex].B = FlattenMaterial.SpecularSamples[SampleIndex].R; }

			// Create texture using the merged property data
			UTexture2D* PackedTexture = FMaterialUtilities::CreateTexture(InOuter, AssetBasePath + TEXT("T_") + AssetBaseName + TEXT("_MRS"), PackedSize, MergedTexture, TC_Default, TEXTUREGROUP_HierarchicalLOD, RF_Public | RF_Standalone, bSRGB);
			checkf(PackedTexture, TEXT("Failed to create texture"));

			// Setup switches for whether or not properties will be packed into one texture
			FStaticSwitchParameter SwitchParameter;
			SwitchParameter.ParameterName = "PackMetallic";
			SwitchParameter.Value = bPackMetallic;
			SwitchParameter.bOverride = true;
			NewStaticParameterSet.StaticSwitchParameters.Add(SwitchParameter);

			SwitchParameter.ParameterName = "PackSpecular";
			SwitchParameter.Value = bPackSpecular;
			SwitchParameter.bOverride = true;
			NewStaticParameterSet.StaticSwitchParameters.Add(SwitchParameter);

			SwitchParameter.ParameterName = "PackRoughness";
			SwitchParameter.Value = bPackRoughness;
			SwitchParameter.bOverride = true;
			NewStaticParameterSet.StaticSwitchParameters.Add(SwitchParameter);

			// Set up switch and texture values
			OutMaterial->SetTextureParameterValueEditorOnly("PackedTexture", PackedTexture);
		}

		// Emissive is a special case due to the scaling variable
		if (FlattenMaterial.EmissiveSamples.Num() > 0 && !(FlattenMaterial.EmissiveSamples.Num() == 1 && FlattenMaterial.EmissiveSamples[0] == FColor::Black))
		{
			TEXTURE_MACRO_VECTOR_LINEAR(Emissive, TC_Default, bRGB);

			if (FlattenMaterial.EmissiveScale != 1.0f)
			{
				OutMaterial->SetScalarParameterValueEditorOnly("EmissiveScale", FlattenMaterial.EmissiveScale);
			}
		}

		// Force initializing the static permutations according to the switches we have set
		OutMaterial->UpdateStaticPermutation(NewStaticParameterSet);
		OutMaterial->InitStaticPermutation();

		OutMaterial->PostEditChange();

		return OutMaterial;
	}
};