// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialShared.cpp: Shared material implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialInstanceBasePropertyOverrides.h"
#include "PixelFormat.h"
#include "ShaderCompiler.h"
#include "MaterialCompiler.h"
#include "MaterialShaderType.h"
#include "MeshMaterialShaderType.h"
#include "HLSLMaterialTranslator.h"
#include "MaterialUniformExpressions.h"
#include "Developer/TargetPlatform/Public/TargetPlatform.h"
#include "ComponentReregisterContext.h"
#include "ComponentRecreateRenderStateContext.h"
#include "EngineModule.h"
#include "Engine/Font.h"

#include "LocalVertexFactory.h"

#include "VertexFactory.h"
#include "RendererInterface.h"
DEFINE_LOG_CATEGORY(LogMaterial);

FName MaterialQualityLevelNames[] = 
{
	FName(TEXT("Low")),
	FName(TEXT("High")),
	FName(TEXT("Num"))
};

static_assert(ARRAY_COUNT(MaterialQualityLevelNames) == EMaterialQualityLevel::Num + 1, "Missing entry from material quality level names.");

void GetMaterialQualityLevelName(EMaterialQualityLevel::Type InQualityLevel, FString& OutName)
{
	check(InQualityLevel < ARRAY_COUNT(MaterialQualityLevelNames));
	MaterialQualityLevelNames[(int32)InQualityLevel].ToString(OutName);
}

int32 FMaterialCompiler::Errorf(const TCHAR* Format,...)
{
	TCHAR	ErrorText[2048];
	GET_VARARGS( ErrorText, ARRAY_COUNT(ErrorText), ARRAY_COUNT(ErrorText)-1, Format, Format );
	return Error(ErrorText);
}

//
//	FExpressionInput::Compile
//

int32 FExpressionInput::Compile(class FMaterialCompiler* Compiler, int32 MultiplexIndex)
{
	if(Expression)
	{
		Expression->ValidateState();
		
		int32 ExpressionResult = Compiler->CallExpression(FMaterialExpressionKey(Expression, OutputIndex, MultiplexIndex),Compiler);

		if(Mask && ExpressionResult != INDEX_NONE)
		{
			return Compiler->ComponentMask(
				ExpressionResult,
				!!MaskR,!!MaskG,!!MaskB,!!MaskA
				);
		}
		else
		{
			return ExpressionResult;
		}
	}
	else
		return INDEX_NONE;
}

void FExpressionInput::Connect( int32 InOutputIndex, class UMaterialExpression* InExpression )
{
	OutputIndex = InOutputIndex;
	Expression = InExpression;

	TArray<FExpressionOutput> Outputs;
	Outputs = Expression->GetOutputs();
	FExpressionOutput* Output = &Outputs[ OutputIndex ];
	Mask = Output->Mask;
	MaskR = Output->MaskR;
	MaskG = Output->MaskG;
	MaskB = Output->MaskB;
	MaskA = Output->MaskA;
}

int32 FColorMaterialInput::CompileWithDefault(class FMaterialCompiler* Compiler, EMaterialProperty Property)
{
	if (UseConstant)
	{
		FLinearColor LinearColor(Constant);
		return Compiler->Constant3(LinearColor.R, LinearColor.G, LinearColor.B);
	}
	else if (Expression)
	{
		int32 ResultIndex = FExpressionInput::Compile(Compiler);
		if (ResultIndex != INDEX_NONE)
		{
			return ResultIndex;
		}
	}

	return Compiler->ForceCast(GetDefaultExpressionForMaterialProperty(Compiler, Property), MCT_Float3);
}

int32 FScalarMaterialInput::CompileWithDefault(class FMaterialCompiler* Compiler, EMaterialProperty Property)
{
	if (UseConstant)
	{
		return Compiler->Constant(Constant);
	}
	else if (Expression)
	{
		int32 ResultIndex = FExpressionInput::Compile(Compiler);
		if (ResultIndex != INDEX_NONE)
		{
			return ResultIndex;
		}
	}

	return Compiler->ForceCast(GetDefaultExpressionForMaterialProperty(Compiler, Property), MCT_Float1);
}

int32 FVectorMaterialInput::CompileWithDefault(class FMaterialCompiler* Compiler, EMaterialProperty Property)
{
	if (UseConstant)
	{
		return Compiler->Constant3(Constant.X, Constant.Y, Constant.Z);
	}
	else if(Expression)
	{
		int32 ResultIndex = FExpressionInput::Compile(Compiler);
		if (ResultIndex != INDEX_NONE)
		{
			return ResultIndex;
		}
	}
	return Compiler->ForceCast(GetDefaultExpressionForMaterialProperty(Compiler, Property), MCT_Float3);
}

int32 FVector2MaterialInput::CompileWithDefault(class FMaterialCompiler* Compiler, EMaterialProperty Property)
{
	if (UseConstant)
	{
		return Compiler->Constant2(Constant.X, Constant.Y);
	}
	else if (Expression)
	{
		int32 ResultIndex = FExpressionInput::Compile(Compiler);
		if (ResultIndex != INDEX_NONE)
		{
			return ResultIndex;
		}
	}

	return Compiler->ForceCast(GetDefaultExpressionForMaterialProperty(Compiler, Property), MCT_Float2);
}

int32 FMaterialAttributesInput::CompileWithDefault(class FMaterialCompiler* Compiler, EMaterialProperty Property)
{
	int32 Ret = INDEX_NONE;
	if(Expression)
	{
		int32 MultiplexIndex = GetInputOutputIndexFromMaterialProperty(Property);

		if( MultiplexIndex >= 0 )
		{
			Ret = FExpressionInput::Compile(Compiler,MultiplexIndex);

			if( Ret != INDEX_NONE && !Expression->IsResultMaterialAttributes(OutputIndex) )
			{
				Compiler->Error(TEXT("Cannot connect a non MaterialAttributes node to a MaterialAttributes pin."));
			}
		}
	}

	SetConnectedProperty(Property, Ret != INDEX_NONE);

	if( Ret == INDEX_NONE )
	{
		Ret = GetDefaultExpressionForMaterialProperty(Compiler, Property);
	}

	return Ret;
}

EMaterialValueType GetMaterialPropertyType(EMaterialProperty Property)
{
	switch(Property)
	{
	case MP_EmissiveColor: return MCT_Float3;
	case MP_Opacity: return MCT_Float;
	case MP_OpacityMask: return MCT_Float;
	case MP_DiffuseColor: return MCT_Float3;
	case MP_SpecularColor: return MCT_Float3;
	case MP_BaseColor: return MCT_Float3;
	case MP_Metallic: return MCT_Float;
	case MP_Specular: return MCT_Float;
	case MP_Roughness: return MCT_Float;
	case MP_Normal: return MCT_Float3;
	case MP_WorldPositionOffset: return MCT_Float3;
	case MP_WorldDisplacement : return MCT_Float3;
	case MP_TessellationMultiplier: return MCT_Float;
	case MP_SubsurfaceColor: return MCT_Float3;
	case MP_ClearCoat: return MCT_Float;
	case MP_ClearCoatRoughness: return MCT_Float;
	case MP_AmbientOcclusion: return MCT_Float;
	case MP_Refraction: return MCT_Float;
	case MP_MaterialAttributes: return MCT_MaterialAttributes;
	case MP_PixelDepthOffset: return MCT_Float;
	};

	if (Property >= MP_CustomizedUVs0 && Property <= MP_CustomizedUVs7)
	{
		return MCT_Float2;
	}

	return MCT_Unknown;
}

/** Returns the shader frequency corresponding to the given material input. */
EShaderFrequency GetMaterialPropertyShaderFrequency(EMaterialProperty Property)
{
	if (Property == MP_WorldPositionOffset
		|| (Property >= MP_CustomizedUVs0 && Property <= MP_CustomizedUVs7))
	{
		return SF_Vertex;
	}
	else if(Property == MP_WorldDisplacement)
	{
		return SF_Domain;
	}
	else if(Property == MP_TessellationMultiplier)
	{
		return SF_Hull;
	}
	return SF_Pixel;
}

void FMaterialCompilationOutput::Serialize(FArchive& Ar)
{
	UniformExpressionSet.Serialize(Ar);

	if (Ar.UE4Ver() >= VER_UE4_PURGED_FMATERIAL_COMPILE_OUTPUTS)
	{
		Ar << bRequiresSceneColorCopy;
	}

	Ar << bNeedsSceneTextures;
	Ar << bUsesEyeAdaptation;
	Ar << bModifiesMeshPosition;
	Ar << bNeedsGBuffer;
}

void FMaterial::GetShaderMapId(EShaderPlatform Platform, FMaterialShaderMapId& OutId) const
{ 
	TArray<FShaderType*> ShaderTypes;
	TArray<FVertexFactoryType*> VFTypes;

	GetDependentShaderAndVFTypes(Platform, ShaderTypes, VFTypes);

	OutId.Usage = GetShaderMapUsage();
	OutId.BaseMaterialId = GetMaterialId();
	OutId.QualityLevel = GetQualityLevelForShaderMapId();
	OutId.FeatureLevel = GetFeatureLevel();
	OutId.SetShaderDependencies(ShaderTypes, VFTypes);
	GetReferencedTexturesHash(OutId.TextureReferencesHash);
}

EMaterialTessellationMode FMaterial::GetTessellationMode() const 
{ 
	return MTM_NoTessellation; 
};

void FMaterial::GetShaderMapIDsWithUnfinishedCompilation(TArray<int32>& ShaderMapIds)
{
	// Build an array of the shader map Id's are not finished compiling.
	if (GameThreadShaderMap && !GameThreadShaderMap->IsCompilationFinalized())
	{
		ShaderMapIds.Add(GameThreadShaderMap->GetCompilingId());
	}
	else if (OutstandingCompileShaderMapIds.Num() != 0 )
	{
		ShaderMapIds.Append(OutstandingCompileShaderMapIds);
	}
}

bool FMaterial::IsCompilationFinished()
{
	// Build an array of the shader map Id's are not finished compiling.
	if (GameThreadShaderMap && !GameThreadShaderMap->IsCompilationFinalized())
	{
		return false;
	}
	else if (OutstandingCompileShaderMapIds.Num() != 0 )
	{
		return false;
	}
	return true;
}

bool FMaterial::HasValidGameThreadShaderMap()
{
	if(!GameThreadShaderMap || !GameThreadShaderMap->IsCompilationFinalized())
	{
		return false;
	}
	return true;
}

void FMaterial::CancelCompilation()
{
	TArray<int32> ShaderMapIdsToCancel;
	GetShaderMapIDsWithUnfinishedCompilation(ShaderMapIdsToCancel);

	if (ShaderMapIdsToCancel.Num() > 0)
	{
		// Cancel all compile jobs for these shader maps.
		GShaderCompilingManager->CancelCompilation(*GetFriendlyName(), ShaderMapIdsToCancel);
	}
}

void FMaterial::FinishCompilation()
{
	TArray<int32> ShaderMapIdsToFinish;
	GetShaderMapIDsWithUnfinishedCompilation(ShaderMapIdsToFinish);

	if (ShaderMapIdsToFinish.Num() > 0)
	{
		// Block until the shader maps that we will save have finished being compiled
		GShaderCompilingManager->FinishCompilation(*GetFriendlyName(), ShaderMapIdsToFinish);
	}
}

const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& FMaterial::GetUniform2DTextureExpressions() const 
{ 
	const FMaterialShaderMap* ShaderMapToUse = NULL;

	if (IsInGameThread())
	{
		// If we are accessing uniform texture expressions on the game thread, use results from a shader map whose compile is in flight that matches this material
		// This allows querying what textures a material uses even when it is being asynchronously compiled
		ShaderMapToUse = GetGameThreadShaderMap() ? GetGameThreadShaderMap() : FMaterialShaderMap::GetShaderMapBeingCompiled(this);

		checkf(!ShaderMapToUse || ShaderMapToUse->GetNumRefs() > 0, TEXT("NumRefs %i, GameThreadShaderMap 0x%08x"), ShaderMapToUse->GetNumRefs(), GetGameThreadShaderMap());
	}
	else 
	{
		check(IsInRenderingThread());
		ShaderMapToUse = GetRenderingThreadShaderMap();
	}

	if (ShaderMapToUse)
	{
		return ShaderMapToUse->GetUniformExpressionSet().Uniform2DTextureExpressions; 
	}
	
	static const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> > EmptyExpressions;
	return EmptyExpressions;
}

const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& FMaterial::GetUniformCubeTextureExpressions() const 
{ 
	const FMaterialShaderMap* ShaderMapToUse = NULL;

	if (IsInGameThread())
	{
		// If we are accessing uniform texture expressions on the game thread, use results from a shader map whose compile is in flight that matches this material
		// This allows querying what textures a material uses even when it is being asynchronously compiled
		ShaderMapToUse = GetGameThreadShaderMap() ? GetGameThreadShaderMap() : FMaterialShaderMap::GetShaderMapBeingCompiled(this);
	}
	else 
	{
		check(IsInRenderingThread());
		ShaderMapToUse = GetRenderingThreadShaderMap();
	}

	if (ShaderMapToUse)
	{
		return ShaderMapToUse->GetUniformExpressionSet().UniformCubeTextureExpressions; 
	}

	static const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> > EmptyExpressions;
	return EmptyExpressions;
}

const TArray<TRefCountPtr<FMaterialUniformExpression> >& FMaterial::GetUniformVectorParameterExpressions() const 
{ 
	const FMaterialShaderMap* ShaderMapToUse = NULL;

	if (IsInGameThread())
	{
		// If we are accessing uniform texture expressions on the game thread, use results from a shader map whose compile is in flight that matches this material
		// This allows querying what textures a material uses even when it is being asynchronously compiled
		ShaderMapToUse = GetGameThreadShaderMap() ? GetGameThreadShaderMap() : FMaterialShaderMap::GetShaderMapBeingCompiled(this);
	}
	else 
	{
		check(IsInRenderingThread());
		ShaderMapToUse = GetRenderingThreadShaderMap();
	}

	if (ShaderMapToUse)
	{
		return ShaderMapToUse->GetUniformExpressionSet().UniformVectorExpressions; 
	}

	static const TArray<TRefCountPtr<FMaterialUniformExpression> > EmptyExpressions;
	return EmptyExpressions;
}

const TArray<TRefCountPtr<FMaterialUniformExpression> >& FMaterial::GetUniformScalarParameterExpressions() const 
{ 
	const FMaterialShaderMap* ShaderMapToUse = NULL;

	if (IsInGameThread())
	{
		// If we are accessing uniform texture expressions on the game thread, use results from a shader map whose compile is in flight that matches this material
		// This allows querying what textures a material uses even when it is being asynchronously compiled
		ShaderMapToUse = GetGameThreadShaderMap() ? GetGameThreadShaderMap() : FMaterialShaderMap::GetShaderMapBeingCompiled(this);
	}
	else 
	{
		check(IsInRenderingThread());
		ShaderMapToUse = GetRenderingThreadShaderMap();
	}

	if (ShaderMapToUse)
	{
		return ShaderMapToUse->GetUniformExpressionSet().UniformScalarExpressions; 
	}

	static const TArray<TRefCountPtr<FMaterialUniformExpression> > EmptyExpressions;
	return EmptyExpressions;
}

bool FMaterial::RequiresSceneColorCopy_GameThread() const
{
	check(IsInGameThread());
	if (GameThreadShaderMap)
	{
		return GameThreadShaderMap->RequiresSceneColorCopy();
	}
	return false;
}

bool FMaterial::RequiresSceneColorCopy_RenderThread() const
{
	check(IsInParallelRenderingThread());
	if (RenderingThreadShaderMap)
	{
		return RenderingThreadShaderMap->RequiresSceneColorCopy();
	}
	return false;
}

bool FMaterial::NeedsSceneTextures() const 
{
	check(IsInParallelRenderingThread());

	if (RenderingThreadShaderMap)
	{
		return RenderingThreadShaderMap->NeedsSceneTextures();
	}
	
	return false;
}

bool FMaterial::NeedsGBuffer() const
{
	check(IsInParallelRenderingThread());

	if (IsOpenGLPlatform(GMaxRHIShaderPlatform)) // @todo: TTP #341211
	{
		return true;
	}

	if (RenderingThreadShaderMap)
	{
		return RenderingThreadShaderMap->NeedsGBuffer();
	}

	return false;
}


bool FMaterial::UsesEyeAdaptation() const 
{
	check(IsInParallelRenderingThread());

	if (RenderingThreadShaderMap)
	{
		return RenderingThreadShaderMap->UsesEyeAdaptation();
	}

	return false;
}

bool FMaterial::MaterialModifiesMeshPosition_RenderThread() const
{ 
	check(IsInParallelRenderingThread());
	bool bUsesWPO = RenderingThreadShaderMap ? RenderingThreadShaderMap->ModifiesMeshPosition() : false;

	return bUsesWPO || GetTessellationMode() != MTM_NoTessellation;
}

bool FMaterial::MaterialModifiesMeshPosition_GameThread() const
{
	check(IsInGameThread());
	FMaterialShaderMap* ShaderMap = GameThreadShaderMap.GetReference();
	bool bUsesWPO = ShaderMap ? ShaderMap->ModifiesMeshPosition() : false;

	return bUsesWPO || GetTessellationMode() != MTM_NoTessellation;
}

bool FMaterial::MaterialMayModifyMeshPosition() const
{
	// Conservative estimate when called before material translation has occurred. 
	// This function is only intended for use in deciding whether or not shader permutations are required.
	return HasVertexPositionOffsetConnected() || HasPixelDepthOffsetConnected() || HasMaterialAttributesConnected() || GetTessellationMode() != MTM_NoTessellation;
}

FMaterialShaderMap* FMaterial::GetRenderingThreadShaderMap() const 
{ 
	check(IsInParallelRenderingThread());
	return RenderingThreadShaderMap; 
}

void FMaterial::SetRenderingThreadShaderMap(FMaterialShaderMap* InMaterialShaderMap)
{
	check(IsInRenderingThread());
	RenderingThreadShaderMap = InMaterialShaderMap;
}

void FMaterial::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (int32 ExpressionIndex = 0; ExpressionIndex < ErrorExpressions.Num(); ExpressionIndex++)
	{
		Collector.AddReferencedObject(ErrorExpressions[ExpressionIndex]);
	}
}

struct FLegacyTextureLookup
{
	void Serialize(FArchive& Ar)
	{
		Ar << TexCoordIndex;
		Ar << TextureIndex;
		Ar << UScale;
		Ar << VScale;
	}

	int32 TexCoordIndex;
	int32 TextureIndex;	

	float UScale;
	float VScale;
};

FArchive& operator<<(FArchive& Ar, FLegacyTextureLookup& Ref)
{
	Ref.Serialize( Ar );
	return Ar;
}

void FMaterial::LegacySerialize(FArchive& Ar)
{
	if (Ar.UE4Ver() < VER_UE4_PURGED_FMATERIAL_COMPILE_OUTPUTS)
	{
		TArray<FString> LegacyStrings;
		Ar << LegacyStrings;

		TMap<UMaterialExpression*,int32> LegacyMap;
		Ar << LegacyMap;
		int32 LegacyInt;
		Ar << LegacyInt;

		FeatureLevel = ERHIFeatureLevel::SM4;
		QualityLevel = EMaterialQualityLevel::High;
		Ar << Id_DEPRECATED;

		TArray<UTexture*> LegacyTextures;
		Ar << LegacyTextures;

		bool bTemp2;
		Ar << bTemp2;

		bool bTemp;
		Ar << bTemp;

		TArray<FLegacyTextureLookup> LegacyLookups;
		Ar << LegacyLookups;

		uint32 DummyDroppedFallbackComponents = 0;
		Ar << DummyDroppedFallbackComponents;
	}

	SerializeInlineShaderMap(Ar);
}

void FMaterial::SerializeInlineShaderMap(FArchive& Ar)
{
	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

	if (FPlatformProperties::RequiresCookedData() && !bCooked && Ar.IsLoading())
	{
		UE_LOG(LogShaders, Fatal, TEXT("This platform requires cooked packages, and shaders were not cooked into this material %s."), *GetFriendlyName());
	}

	if (bCooked)
	{
		if (Ar.IsCooking())
		{
			FinishCompilation();

			bool bValid = GameThreadShaderMap != NULL && GameThreadShaderMap->CompiledSuccessfully();
			
			Ar << bValid;

			if (bValid)
			{
				GameThreadShaderMap->Serialize(Ar);
			}
		}
		else
		{
			bool bValid = false;
			Ar << bValid;

			if (bValid)
			{
				TRefCountPtr<FMaterialShaderMap> LoadedShaderMap = new FMaterialShaderMap();
				LoadedShaderMap->Serialize(Ar);

				// Toss the loaded shader data if this is a server only instance
				//@todo - don't cook it in the first place
				if (FApp::CanEverRender())
				{
					GameThreadShaderMap = RenderingThreadShaderMap = LoadedShaderMap;
				}
			}
		}
	}
}

void FMaterialResource::LegacySerialize(FArchive& Ar)
{
	FMaterial::LegacySerialize(Ar);

	if (Ar.UE4Ver() < VER_UE4_PURGED_FMATERIAL_COMPILE_OUTPUTS)
	{
		int32 BlendModeOverrideValueTemp = 0;
		Ar << BlendModeOverrideValueTemp;
		bool bDummyBool = false;
		Ar << bDummyBool;
		Ar << bDummyBool;
	}
}

const TArray<UTexture*>& FMaterialResource::GetReferencedTextures() const
{
	return Material->ExpressionTextureReferences;
}

bool FMaterialResource::GetAllowDevelopmentShaderCompile()const
{
	return Material->bAllowDevelopmentShaderCompile;
}

void FMaterial::ReleaseShaderMap()
{
	if (GameThreadShaderMap)
	{
		GameThreadShaderMap = NULL;
		
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			ReleaseShaderMap,
			FMaterial*,Material,this,
		{
			Material->SetRenderingThreadShaderMap(NULL);
		});
	}
}

bool IsTranslucentBlendMode(EBlendMode BlendMode)
{
	return BlendMode != BLEND_Opaque && BlendMode != BLEND_Masked;
}

int32 FMaterialResource::GetMaterialDomain() const { return Material->MaterialDomain; }
bool FMaterialResource::IsTangentSpaceNormal() const { return Material->bTangentSpaceNormal || Material->Normal.Expression == NULL; }
bool FMaterialResource::ShouldInjectEmissiveIntoLPV() const { return Material->bUseEmissiveForDynamicAreaLighting; }
bool FMaterialResource::ShouldGenerateSphericalParticleNormals() const { return Material->bGenerateSphericalParticleNormals; }
bool FMaterialResource::ShouldDisableDepthTest() const { return Material->bDisableDepthTest; }
bool FMaterialResource::ShouldEnableResponsiveAA() const { return Material->bEnableResponsiveAA; }
bool FMaterialResource::ShouldDoSSR() const { return Material->bScreenSpaceReflections; }
bool FMaterialResource::IsWireframe() const { return Material->Wireframe; }
bool FMaterialResource::IsLightFunction() const { return Material->MaterialDomain == MD_LightFunction; }
bool FMaterialResource::IsUsedWithEditorCompositing() const { return Material->bUsedWithEditorCompositing; }
bool FMaterialResource::IsUsedWithDeferredDecal() const { return Material->MaterialDomain == MD_DeferredDecal; }
bool FMaterialResource::IsSpecialEngineMaterial() const { return Material->bUsedAsSpecialEngineMaterial; }
bool FMaterialResource::HasVertexPositionOffsetConnected() const { return !Material->bUseMaterialAttributes && Material->WorldPositionOffset.Expression != NULL; }
bool FMaterialResource::HasPixelDepthOffsetConnected() const { return !Material->bUseMaterialAttributes && Material->PixelDepthOffset.Expression != NULL; }
bool FMaterialResource::HasMaterialAttributesConnected() const { return Material->bUseMaterialAttributes && Material->MaterialAttributes.Expression != NULL; }
FString FMaterialResource::GetBaseMaterialPathName() const { return Material->GetPathName(); }

bool FMaterialResource::IsUsedWithSkeletalMesh() const
{
	return Material->bUsedWithSkeletalMesh;
}

bool FMaterialResource::IsUsedWithLandscape() const
{
	return Material->bUsedWithLandscape;
}

bool FMaterialResource::IsUsedWithParticleSystem() const
{
	return Material->bUsedWithParticleSprites || Material->bUsedWithBeamTrails;
}

bool FMaterialResource::IsUsedWithParticleSprites() const
{
	return Material->bUsedWithParticleSprites;
}

bool FMaterialResource::IsUsedWithBeamTrails() const
{
	return Material->bUsedWithBeamTrails;
}

bool FMaterialResource::IsUsedWithMeshParticles() const
{
	return Material->bUsedWithMeshParticles;
}

bool FMaterialResource::IsUsedWithStaticLighting() const
{
	return Material->bUsedWithStaticLighting;
}

bool FMaterialResource::IsUsedWithMorphTargets() const
{
	return Material->bUsedWithMorphTargets;
}

bool FMaterialResource::IsUsedWithSplineMeshes() const
{
	return Material->bUsedWithSplineMeshes;
}

bool FMaterialResource::IsUsedWithInstancedStaticMeshes() const
{
	return Material->bUsedWithInstancedStaticMeshes;
}

bool FMaterialResource::IsUsedWithAPEXCloth() const
{
	return Material->bUsedWithClothing;
}

bool FMaterialResource::IsUsedWithUI() const
{
	return Material->bUsedWithUI;
}

EMaterialTessellationMode FMaterialResource::GetTessellationMode() const 
{ 
	return (EMaterialTessellationMode)Material->D3D11TessellationMode; 
}

bool FMaterialResource::IsCrackFreeDisplacementEnabled() const 
{ 
	return Material->bEnableCrackFreeDisplacement;
}

bool FMaterialResource::IsSeparateTranslucencyEnabled() const 
{ 
	return Material->bEnableSeparateTranslucency;
}

bool FMaterialResource::IsAdaptiveTessellationEnabled() const
{
	return Material->bEnableAdaptiveTessellation;
}

bool FMaterialResource::IsFullyRough() const
{
	return Material->bFullyRough;
}

bool FMaterialResource::OutputsVelocityOnBasePass() const
{
	return Material->bOutputVelocityOnBasePass;
}

bool FMaterialResource::IsNonmetal() const
{
	return !Material->bUseMaterialAttributes ?
			(!Material->Metallic.IsConnected() && !Material->Specular.IsConnected()) :
			!(Material->MaterialAttributes.IsConnected(MP_Specular) || Material->MaterialAttributes.IsConnected(MP_Metallic));
}

bool FMaterialResource::UseLmDirectionality() const
{
	return Material->bUseLightmapDirectionality;
}

/**
 * Should shaders compiled for this material be saved to disk?
 */
bool FMaterialResource::IsPersistent() const { return true; }

FGuid FMaterialResource::GetMaterialId() const
{
	return Material->StateId;
}

ETranslucencyLightingMode FMaterialResource::GetTranslucencyLightingMode() const { return (ETranslucencyLightingMode)Material->TranslucencyLightingMode; }

float FMaterialResource::GetOpacityMaskClipValue() const 
{
	return MaterialInstance ? MaterialInstance->GetOpacityMaskClipValue() : Material->GetOpacityMaskClipValue();
}

EBlendMode FMaterialResource::GetBlendMode() const 
{
	return MaterialInstance ? MaterialInstance->GetBlendMode() : Material->GetBlendMode();
}

EMaterialShadingModel FMaterialResource::GetShadingModel() const 
{
	return MaterialInstance ? MaterialInstance->GetShadingModel() : Material->GetShadingModel();
}

bool FMaterialResource::IsTwoSided() const 
{
	return MaterialInstance ? MaterialInstance->IsTwoSided() : Material->IsTwoSided();
}

bool FMaterialResource::IsMasked() const 
{
	return MaterialInstance ? MaterialInstance->IsMasked() : Material->IsMasked();
}

bool FMaterialResource::IsDistorted() const { return Material->bUsesDistortion; }
float FMaterialResource::GetTranslucencyDirectionalLightingIntensity() const { return Material->TranslucencyDirectionalLightingIntensity; }
float FMaterialResource::GetTranslucentShadowDensityScale() const { return Material->TranslucentShadowDensityScale; }
float FMaterialResource::GetTranslucentSelfShadowDensityScale() const { return Material->TranslucentSelfShadowDensityScale; }
float FMaterialResource::GetTranslucentSelfShadowSecondDensityScale() const { return Material->TranslucentSelfShadowSecondDensityScale; }
float FMaterialResource::GetTranslucentSelfShadowSecondOpacity() const { return Material->TranslucentSelfShadowSecondOpacity; }
float FMaterialResource::GetTranslucentBackscatteringExponent() const { return Material->TranslucentBackscatteringExponent; }
FLinearColor FMaterialResource::GetTranslucentMultipleScatteringExtinction() const { return Material->TranslucentMultipleScatteringExtinction; }
float FMaterialResource::GetTranslucentShadowStartOffset() const { return Material->TranslucentShadowStartOffset; }
float FMaterialResource::GetRefractionDepthBiasValue() const { return Material->RefractionDepthBias; }
float FMaterialResource::GetMaxDisplacement() const { return Material->MaxDisplacement; }
bool FMaterialResource::UseTranslucencyVertexFog() const {return Material->bUseTranslucencyVertexFog;}
FString FMaterialResource::GetFriendlyName() const { return *GetNameSafe(Material); }

uint32 FMaterialResource::GetDecalBlendMode() const
{
	return Material->GetDecalBlendMode();
}

uint32 FMaterialResource::GetMaterialDecalResponse() const
{
	return Material->GetMaterialDecalResponse();
}

bool FMaterialResource::HasNormalConnected() const
{
	return Material->HasNormalConnected();
}

bool FMaterialResource::RequiresSynchronousCompilation() const
{
	return Material->IsDefaultMaterial();
}

bool FMaterialResource::IsDefaultMaterial() const
{
	return Material->IsDefaultMaterial();
}

void FMaterialResource::NotifyCompilationFinished()
{
	Material->NotifyCompilationFinished(this);
}

/**
 * Gets instruction counts that best represent the likely usage of this material based on shading model and other factors.
 * @param Descriptions - an array of descriptions to be populated
 * @param InstructionCounts - an array of instruction counts matching the Descriptions.  
 *		The dimensions of these arrays are guaranteed to be identical and all values are valid.
 */
void FMaterialResource::GetRepresentativeInstructionCounts(TArray<FString> &Descriptions, TArray<int32> &InstructionCounts) const
{
	TArray<FString> ShaderTypeNames;
	TArray<FString> ShaderTypeDescriptions;

	//when adding a shader type here be sure to update FPreviewMaterial::ShouldCache()
	//so the shader type will get compiled with preview materials
	const FMaterialShaderMap* MaterialShaderMap = GetGameThreadShaderMap();
	if (MaterialShaderMap && MaterialShaderMap->IsCompilationFinalized())
	{
		GetRepresentativeShaderTypesAndDescriptions(ShaderTypeNames, ShaderTypeDescriptions);

		const FMeshMaterialShaderMap* MeshShaderMap = MaterialShaderMap->GetMeshShaderMap(&FLocalVertexFactory::StaticType);
		if (MeshShaderMap)
		{
			Descriptions.Empty();
			InstructionCounts.Empty();

			for (int32 InstructionIndex = 0; InstructionIndex < ShaderTypeNames.Num(); InstructionIndex++)
			{
				FShaderType* ShaderType = FindShaderTypeByName(*ShaderTypeNames[InstructionIndex]);
				if (ShaderType)
				{
					const FShader* Shader = MeshShaderMap->GetShader(ShaderType);
					if (Shader && Shader->GetNumInstructions() > 0)
					{
						//if the shader was found, add it to the output arrays
						InstructionCounts.Push(Shader->GetNumInstructions());
						Descriptions.Push(ShaderTypeDescriptions[InstructionIndex]);
					}
				}
			}
		}
	}

	check(Descriptions.Num() == InstructionCounts.Num());
}

void FMaterialResource::GetRepresentativeShaderTypesAndDescriptions(TArray<FString> &ShaderTypeNames, TArray<FString> &ShaderTypeDescriptions) const
{
	static auto* MobileHDR = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
	bool bMobileHDR = MobileHDR && MobileHDR->GetValueOnAnyThread() == 1;

	if (GetFeatureLevel() >= ERHIFeatureLevel::SM4)
	{
		if (GetShadingModel() == MSM_Unlit)
		{
			//unlit materials are never lightmapped
			new (ShaderTypeNames)FString(TEXT("TBasePassPSFNoLightMapPolicy"));
			new (ShaderTypeDescriptions)FString(TEXT("Base pass shader without light map"));
		}
		else
		{
			if (IsUsedWithStaticLighting())
			{
				//lit materials are usually lightmapped
				new (ShaderTypeNames)FString(TEXT("TBasePassPSTDistanceFieldShadowsAndLightMapPolicyHQ"));
				new (ShaderTypeDescriptions)FString(TEXT("Base pass shader with static lighting"));
			}

			//also show a dynamically lit shader
			new (ShaderTypeNames)FString(TEXT("TBasePassPSFNoLightMapPolicy"));
			new (ShaderTypeDescriptions)FString(TEXT("Base pass shader with only dynamic lighting"));

			if (IsTranslucentBlendMode(GetBlendMode()))
			{
				new (ShaderTypeNames)FString(TEXT("TBasePassPSFSelfShadowedTranslucencyPolicy"));
				new (ShaderTypeDescriptions)FString(TEXT("Base pass shader for self shadowed translucency"));
			}
		}

		new (ShaderTypeNames)FString(TEXT("TBasePassVSFNoLightMapPolicy"));
		new (ShaderTypeDescriptions)FString(TEXT("Vertex shader"));
	}
	else
	{
		const TCHAR* ShaderSuffix = bMobileHDR ? TEXT("HDRLinear64") : TEXT("LDRGamma32");
		const TCHAR* DescSuffix = bMobileHDR ? TEXT(" (HDR)") : TEXT(" (LDR)");

		if (GetShadingModel() == MSM_Unlit)
		{
			//unlit materials are never lightmapped
			new (ShaderTypeNames)FString(FString::Printf(TEXT("TBasePassForForwardShadingPSFNoLightMapPolicy%s"), ShaderSuffix));
			new (ShaderTypeDescriptions)FString(FString::Printf(TEXT("Mobile base pass shader without light map%s"), DescSuffix));
		}
		else
		{
			if (IsUsedWithStaticLighting())
			{
				//lit materials are usually lightmapped
				new (ShaderTypeNames)FString(FString::Printf(TEXT("TBasePassForForwardShadingPSTLightMapPolicyLQ%s"), ShaderSuffix));
				new (ShaderTypeDescriptions)FString(FString::Printf(TEXT("Mobile base pass shader with static lighting%s"), DescSuffix));

				// + distance field shadows
				new (ShaderTypeNames)FString(FString::Printf(TEXT("TBasePassForForwardShadingPSTDistanceFieldShadowsAndLightMapPolicyLQ%s"), ShaderSuffix));
				new (ShaderTypeDescriptions)FString(FString::Printf(TEXT("Mobile base pass shader with distance field shadows%s"), DescSuffix));
			}

			//also show a dynamically lit shader
			new (ShaderTypeNames)FString(FString::Printf(TEXT("TBasePassForForwardShadingPSFSimpleDirectionalLightAndSHIndirectPolicy%s"), ShaderSuffix));
			new (ShaderTypeDescriptions)FString(FString::Printf(TEXT("Mobile base pass shader with only dynamic lighting%s"), DescSuffix));
		}

		new (ShaderTypeNames)FString(FString::Printf(TEXT("TBasePassForForwardShadingVSFNoLightMapPolicy%s"), ShaderSuffix));
		new (ShaderTypeDescriptions)FString(FString::Printf(TEXT("Mobile base pass vertex shader%s"), DescSuffix));
	}
}

SIZE_T FMaterialResource::GetResourceSizeInclusive()
{
	SIZE_T ResourceSize = 0;
	TSet<const FMaterialShaderMap*> UniqueShaderMaps;
	TMap<FShaderId,FShader*> UniqueShaders;
	TSet<FShaderResourceId> UniqueShaderResourceIds;

	ResourceSize += sizeof(FMaterialResource);
	UniqueShaderMaps.Add(GetGameThreadShaderMap());

	for (TSet<const FMaterialShaderMap*>::TConstIterator It(UniqueShaderMaps); It; ++It)
	{
		const FMaterialShaderMap* MaterialShaderMap = *It;
		if (MaterialShaderMap)
		{
			ResourceSize += MaterialShaderMap->GetSizeBytes();
			MaterialShaderMap->GetShaderList(UniqueShaders);
		}
	}

	for (TMap<FShaderId,FShader*>::TConstIterator It(UniqueShaders); It; ++It)
	{
		FShader* Shader = It.Value();
		if (Shader)
		{
			FShaderResourceId ResourceId = Shader->GetResourceId();
			bool bCountedResource = false;
			UniqueShaderResourceIds.Add(ResourceId,&bCountedResource);
			if (!bCountedResource)
			{
				ResourceSize += Shader->GetResourceSizeBytes();
			}
			ResourceSize += Shader->GetSizeBytes();
		}
	}

	return ResourceSize;
}

/**
 * Destructor
 */
FMaterial::~FMaterial()
{
	if (GIsEditor)
	{
		const FSetElementId FoundId = EditorLoadedMaterialResources.FindId(this);
		if (FoundId.IsValidId())
		{
			// Remove the material from EditorLoadedMaterialResources if found
			EditorLoadedMaterialResources.Remove(FoundId);
		}
	}

	FMaterialShaderMap::RemovePendingMaterial(this);
}

// could be more to a more central DBuffer file
// @return e.g. 1+2+4 means DBufferA(1) + DBufferB(2) + DBufferC(4) is used
static uint8 ComputeDBufferMRTMask(EDecalBlendMode DecalBlendMode)
{
	switch(DecalBlendMode)
	{
		case DBM_DBuffer_ColorNormalRoughness:
			return 1 + 2 + 4;
		case DBM_DBuffer_Color:
			return 1;
		case DBM_DBuffer_ColorNormal:
			return 1 + 2;
		case DBM_DBuffer_ColorRoughness:
			return 1 + 4;
		case DBM_DBuffer_Normal:
			return 2;
		case DBM_DBuffer_NormalRoughness:
			return 2 + 4;
		case DBM_DBuffer_Roughness:
			return 4;
	}

	return 0;
}

/** Populates OutEnvironment with defines needed to compile shaders for this material. */
void FMaterial::SetupMaterialEnvironment(
	EShaderPlatform Platform,
	const FUniformExpressionSet& InUniformExpressionSet,
	FShaderCompilerEnvironment& OutEnvironment
	) const
{
	// Add the material uniform buffer definition.
	FShaderUniformBufferParameter::ModifyCompilationEnvironment(TEXT("Material"),InUniformExpressionSet.GetUniformBufferStruct(),Platform,OutEnvironment);

	if ((RHISupportsTessellation(Platform) == false) || (GetTessellationMode() == MTM_NoTessellation))
	{
		OutEnvironment.SetDefine(TEXT("USING_TESSELLATION"),TEXT("0"));
	}
	else
	{
		OutEnvironment.SetDefine(TEXT("USING_TESSELLATION"),TEXT("1"));
		if (GetTessellationMode() == MTM_FlatTessellation)
		{
			OutEnvironment.SetDefine(TEXT("TESSELLATION_TYPE_FLAT"),TEXT("1"));
		}
		else if (GetTessellationMode() == MTM_PNTriangles)
		{
			OutEnvironment.SetDefine(TEXT("TESSELLATION_TYPE_PNTRIANGLES"),TEXT("1"));
		}

		// This is dominant vertex/edge information.  Note, mesh must have preprocessed neighbors IB of material will fallback to default.
		//  PN triangles needs preprocessed buffers regardless of c
		if (IsCrackFreeDisplacementEnabled())
		{
			OutEnvironment.SetDefine(TEXT("DISPLACEMENT_ANTICRACK"),TEXT("1"));
		}
		else
		{
			OutEnvironment.SetDefine(TEXT("DISPLACEMENT_ANTICRACK"),TEXT("0"));
		}

		// Set whether to enable the adaptive tessellation, which tries to maintain a uniform number of pixels per triangle.
		if (IsAdaptiveTessellationEnabled())
		{
			OutEnvironment.SetDefine(TEXT("USE_ADAPTIVE_TESSELLATION_FACTOR"),TEXT("1"));
		}
		else
		{
			OutEnvironment.SetDefine(TEXT("USE_ADAPTIVE_TESSELLATION_FACTOR"),TEXT("0"));
		}
		
	}

	switch(GetBlendMode())
	{
	case BLEND_Opaque: OutEnvironment.SetDefine(TEXT("MATERIALBLENDING_SOLID"),TEXT("1")); break;
	case BLEND_Masked:
		{
			// Only set MATERIALBLENDING_MASKED if the material is truly masked
			//@todo - this may cause mismatches with what the shader compiles and what the renderer thinks the shader needs
			// For example IsTranslucentBlendMode doesn't check IsMasked
			if(IsMasked())
			{
				OutEnvironment.SetDefine(TEXT("MATERIALBLENDING_MASKED"),TEXT("1"));
			}
			else
			{
				OutEnvironment.SetDefine(TEXT("MATERIALBLENDING_SOLID"),TEXT("1"));
			}
			break;
		}
	case BLEND_Translucent: OutEnvironment.SetDefine(TEXT("MATERIALBLENDING_TRANSLUCENT"),TEXT("1")); break;
	case BLEND_Additive: OutEnvironment.SetDefine(TEXT("MATERIALBLENDING_ADDITIVE"),TEXT("1")); break;
	case BLEND_Modulate: OutEnvironment.SetDefine(TEXT("MATERIALBLENDING_MODULATE"),TEXT("1")); break;
	default: 
		UE_LOG(LogMaterial, Warning, TEXT("Unknown material blend mode: %u  Setting to BLEND_Opaque"),(int32)GetBlendMode());
		OutEnvironment.SetDefine(TEXT("MATERIALBLENDING_SOLID"),TEXT("1"));
	}

	{
		EMaterialDecalResponse MaterialDecalResponse = (EMaterialDecalResponse)GetMaterialDecalResponse();

		// bit 0:color/1:normal/2:roughness to enable/disable parts of the DBuffer decal effect
		int32 MaterialDecalResponseMask = 0;

		switch(MaterialDecalResponse)
		{
			case MDR_None:					MaterialDecalResponseMask = 0; break;
			case MDR_ColorNormalRoughness:	MaterialDecalResponseMask = 1 + 2 + 4; break;
			case MDR_Color:					MaterialDecalResponseMask = 1; break;
			case MDR_ColorNormal:			MaterialDecalResponseMask = 1 + 2; break;
			case MDR_ColorRoughness:		MaterialDecalResponseMask = 1 + 4; break;
			case MDR_Normal:				MaterialDecalResponseMask = 2; break;
			case MDR_NormalRoughness:		MaterialDecalResponseMask = 2 + 4; break;
			case MDR_Roughness:				MaterialDecalResponseMask = 4; break;
			default:
				check(0);
		}

		OutEnvironment.SetDefine(TEXT("MATERIALDECALRESPONSEMASK"), MaterialDecalResponseMask);
	}

	OutEnvironment.SetDefine(TEXT("MATERIAL_TWOSIDED"), IsTwoSided() ? TEXT("1") : TEXT("0"));
	OutEnvironment.SetDefine(TEXT("MATERIAL_TANGENTSPACENORMAL"), IsTangentSpaceNormal() ? TEXT("1") : TEXT("0"));
	OutEnvironment.SetDefine(TEXT("GENERATE_SPHERICAL_PARTICLE_NORMALS"),ShouldGenerateSphericalParticleNormals() ? TEXT("1") : TEXT("0"));
	OutEnvironment.SetDefine(TEXT("MATERIAL_USES_SCENE_COLOR_COPY"), RequiresSceneColorCopy_GameThread() ? TEXT("1") : TEXT("0"));
	OutEnvironment.SetDefine(TEXT("MATERIAL_FULLY_ROUGH"), IsFullyRough() ? TEXT("1") : TEXT("0"));
	OutEnvironment.SetDefine(TEXT("MATERIAL_NONMETAL"), IsNonmetal() ? TEXT("1") : TEXT("0"));
	OutEnvironment.SetDefine(TEXT("MATERIAL_USE_LM_DIRECTIONALITY"), UseLmDirectionality() ? TEXT("1") : TEXT("0"));
	OutEnvironment.SetDefine(TEXT("MATERIAL_INJECT_EMISSIVE_INTO_LPV"), ShouldInjectEmissiveIntoLPV() ? TEXT("1") : TEXT("0"));
	OutEnvironment.SetDefine(TEXT("MATERIAL_SSR"), ShouldDoSSR() ? TEXT("1") : TEXT("0"));

	{
		auto DecalBlendMode = (EDecalBlendMode)GetDecalBlendMode();

		uint8 bDBufferMask = ComputeDBufferMRTMask(DecalBlendMode);

		OutEnvironment.SetDefine(TEXT("MATERIAL_DBUFFERA"), (bDBufferMask & 0x1) != 0);
		OutEnvironment.SetDefine(TEXT("MATERIAL_DBUFFERB"), (bDBufferMask & 0x2) != 0);
		OutEnvironment.SetDefine(TEXT("MATERIAL_DBUFFERC"), (bDBufferMask & 0x4) != 0);
	}

	switch(GetShadingModel())
	{
		case MSM_Unlit:				OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_UNLIT"),				TEXT("1")); break;
		case MSM_DefaultLit:		OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_DEFAULT_LIT"),			TEXT("1")); break;
		case MSM_Subsurface:		OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_SUBSURFACE"),			TEXT("1")); break;
		case MSM_PreintegratedSkin: OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_PREINTEGRATED_SKIN"),	TEXT("1")); break;
		case MSM_SubsurfaceProfile: OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_SUBSURFACE_PROFILE"),	TEXT("1")); break;
		case MSM_ClearCoat:			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_CLEAR_COAT"),			TEXT("1")); break;
		case MSM_TwoSidedFoliage:	OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_TWOSIDED_FOLIAGE"),	TEXT("1")); break;
		default: 
			UE_LOG(LogMaterial, Warning, TEXT("Unknown material shading model: %u  Setting to MSM_DefaultLit"),(int32)GetShadingModel());
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_DEFAULT_LIT"),TEXT("1"));
	};

	if (IsTranslucentBlendMode(GetBlendMode()))
	{
		switch(GetTranslucencyLightingMode())
		{
		case TLM_VolumetricNonDirectional: OutEnvironment.SetDefine(TEXT("TRANSLUCENCY_LIGHTING_VOLUMETRIC_NONDIRECTIONAL"),TEXT("1")); break;
		case TLM_VolumetricDirectional: OutEnvironment.SetDefine(TEXT("TRANSLUCENCY_LIGHTING_VOLUMETRIC_DIRECTIONAL"),TEXT("1")); break;
		case TLM_Surface: OutEnvironment.SetDefine(TEXT("TRANSLUCENCY_LIGHTING_SURFACE"),TEXT("1")); break;
		case TLM_SurfacePerPixelLighting: OutEnvironment.SetDefine(TEXT("TRANSLUCENCY_LIGHTING_SURFACE_PERPIXEL"),TEXT("1")); break;

		default: 
			UE_LOG(LogMaterial, Warning, TEXT("Unknown lighting mode: %u"),(int32)GetTranslucencyLightingMode());
			OutEnvironment.SetDefine(TEXT("TRANSLUCENCY_LIGHTING_VOLUMETRIC_NONDIRECTIONAL"),TEXT("1")); break;
		};
	}

	if( IsUsedWithEditorCompositing() )
	{
		OutEnvironment.SetDefine(TEXT("EDITOR_PRIMITIVE_MATERIAL"),TEXT("1"));
	}
}

/**
 * Caches the material shaders for this material with no static parameters on the given platform.
 * This is used by material resources of UMaterials.
 */
bool FMaterial::CacheShaders(EShaderPlatform Platform, bool bApplyCompletedShaderMapForRendering)
{
	FMaterialShaderMapId NoStaticParametersId;
	GetShaderMapId(Platform, NoStaticParametersId);
	return CacheShaders(NoStaticParametersId, Platform, bApplyCompletedShaderMapForRendering);
}

/**
 * Caches the material shaders for the given static parameter set and platform.
 * This is used by material resources of UMaterialInstances.
 */
bool FMaterial::CacheShaders(const FMaterialShaderMapId& ShaderMapId, EShaderPlatform Platform, bool bApplyCompletedShaderMapForRendering)
{
	bool bSucceeded = false;

	check(ShaderMapId.BaseMaterialId.IsValid());

	// If we loaded this material with inline shaders, use what was loaded (GameThreadShaderMap) instead of looking in the DDC
	if (bContainsInlineShaders)
	{
		FMaterialShaderMap* ExistingShaderMap = NULL;
		
		if (GameThreadShaderMap)
		{
			// Note: in the case of an inlined shader map, the shadermap Id will not be valid because we stripped some editor-only data needed to create it
			// Get the shadermap Id from the shadermap that was inlined into the package, if it exists
			ExistingShaderMap = FMaterialShaderMap::FindId(GameThreadShaderMap->GetShaderMapId(), Platform);
		}

		// Re-use an identical shader map in memory if possible, removing the reference to the inlined shader map
		if (ExistingShaderMap)
		{
			GameThreadShaderMap = ExistingShaderMap;
		}
		else if (GameThreadShaderMap)
		{
			// We are going to use the inlined shader map, register it so it can be re-used by other materials
			GameThreadShaderMap->Register(Platform);
		}
	}
	else
	{
		// Find the material's cached shader map.
		GameThreadShaderMap = FMaterialShaderMap::FindId(ShaderMapId, Platform);

		// Attempt to load from the derived data cache if we are uncooked
		if ((!GameThreadShaderMap || !GameThreadShaderMap->IsComplete(this, true)) && !FPlatformProperties::RequiresCookedData())
		{
			FMaterialShaderMap::LoadFromDerivedDataCache(this, ShaderMapId, Platform, GameThreadShaderMap);
		}
	}

	if (GameThreadShaderMap && GameThreadShaderMap->TryToAddToExistingCompilationTask(this))
	{
		OutstandingCompileShaderMapIds.Add(GameThreadShaderMap->GetCompilingId());
		// Reset the shader map so the default material will be used until the compile finishes.
		GameThreadShaderMap = NULL;
		bSucceeded = true;
	}
	else
	if (!GameThreadShaderMap || !GameThreadShaderMap->IsComplete(this, true))
	{
		if (bContainsInlineShaders || FPlatformProperties::RequiresCookedData())
		{
			if (IsSpecialEngineMaterial())
			{
				//assert if the default material's shader map was not found, since it will cause problems later
				UE_LOG(LogMaterial, Fatal,TEXT("Failed to find shader map for default material %s!  Please make sure cooking was successful."), *GetFriendlyName());
			}
			else
			{
				UE_LOG(LogMaterial, Log, TEXT("Can't compile %s with cooked content, will use default material instead"), *GetFriendlyName());
			}

			// Reset the shader map so the default material will be used.
			GameThreadShaderMap = NULL;
		}
		else
		{
			const TCHAR* ShaderMapCondition;
			if (GameThreadShaderMap)
			{
				ShaderMapCondition = TEXT("Incomplete");
			}
			else
			{
				ShaderMapCondition = TEXT("Missing");
			}
			UE_LOG(LogMaterial, Log, TEXT("%s cached shader map for material %s, compiling. %s"),ShaderMapCondition,*GetFriendlyName(), IsSpecialEngineMaterial() ? TEXT("Is special engine material.") : TEXT("") );

			// If there's no cached shader map for this material, compile a new one.
			// This is just kicking off the async compile, GameThreadShaderMap will not be complete yet
			bSucceeded = BeginCompileShaderMap(ShaderMapId, Platform, GameThreadShaderMap, bApplyCompletedShaderMapForRendering);

			if (!bSucceeded)
			{
				// If it failed to compile the material, reset the shader map so the material isn't used.
				GameThreadShaderMap = NULL;

				if (IsDefaultMaterial())
				{
					for (int32 ErrorIndex = 0; ErrorIndex < CompileErrors.Num(); ErrorIndex++)
					{
						// Always log material errors in an unsuppressed category
						UE_LOG(LogMaterial, Warning, TEXT("	%s"), *CompileErrors[ErrorIndex]);
					}

					// Assert if the default material could not be compiled, since there will be nothing for other failed materials to fall back on.
					UE_LOG(LogMaterial, Fatal,TEXT("Failed to compile default material %s!"), *GetFriendlyName());
				}
			}
		}
	}
	else
	{
		bSucceeded = true;
	}

	RenderingThreadShaderMap = GameThreadShaderMap;

	return bSucceeded;
}

/**
* Compiles this material for Platform, storing the result in OutShaderMap
*
* @param ShaderMapId - the set of static parameters to compile
* @param Platform - the platform to compile for
* @param OutShaderMap - the shader map to compile
* @return - true if compile succeeded or was not necessary (shader map for ShaderMapId was found and was complete)
*/
bool FMaterial::BeginCompileShaderMap(
	const FMaterialShaderMapId& ShaderMapId, 
	EShaderPlatform Platform, 
	TRefCountPtr<FMaterialShaderMap>& OutShaderMap, 
	bool bApplyCompletedShaderMapForRendering)
{
#if WITH_EDITORONLY_DATA
	bool bSuccess = false;

	STAT(double MaterialCompileTime = 0);

	TRefCountPtr<FMaterialShaderMap> NewShaderMap = new FMaterialShaderMap();

	SCOPE_SECONDS_COUNTER(MaterialCompileTime);

	// Generate the material shader code.
	FMaterialCompilationOutput NewCompilationOutput;
	FHLSLMaterialTranslator MaterialTranslator(this,NewCompilationOutput,ShaderMapId.ParameterSet,Platform,GetQualityLevel(),ShaderMapId.FeatureLevel);
	bSuccess = MaterialTranslator.Translate();

	if(bSuccess)
	{
		// Create a shader compiler environment for the material that will be shared by all jobs from this material
		TRefCountPtr<FShaderCompilerEnvironment> MaterialEnvironment = new FShaderCompilerEnvironment();

		MaterialTranslator.GetMaterialEnvironment(Platform, *MaterialEnvironment);
		const FString MaterialShaderCode = MaterialTranslator.GetMaterialShaderCode();
		const bool bSynchronousCompile = RequiresSynchronousCompilation() || !GShaderCompilingManager->AllowAsynchronousShaderCompiling();

		MaterialEnvironment->IncludeFileNameToContentsMap.Add(TEXT("Material.usf"), MaterialShaderCode);

		// Compile the shaders for the material.
		NewShaderMap->Compile(this, ShaderMapId, MaterialEnvironment, NewCompilationOutput, Platform, bSynchronousCompile, bApplyCompletedShaderMapForRendering);

		if (bSynchronousCompile)
		{
			// If this is a synchronous compile, assign the compile result to the output
			OutShaderMap = NewShaderMap->CompiledSuccessfully() ? NewShaderMap : NULL;
		}
		else
		{
			OutstandingCompileShaderMapIds.Add( NewShaderMap->GetCompilingId() );
			// Async compile, use NULL so that rendering will fall back to the default material.
			OutShaderMap = NULL;
		}
	}

	INC_FLOAT_STAT_BY(STAT_ShaderCompiling_MaterialCompiling,(float)MaterialCompileTime);
	INC_FLOAT_STAT_BY(STAT_ShaderCompiling_MaterialShaders,(float)MaterialCompileTime);

	return bSuccess;
#else
	UE_LOG(LogMaterial, Fatal,TEXT("Not supported."));
	return false;
#endif
}

/**
 * Should the shader for this material with the given platform, shader type and vertex 
 * factory type combination be compiled
 *
 * @param Platform		The platform currently being compiled for
 * @param ShaderType	Which shader is being compiled
 * @param VertexFactory	Which vertex factory is being compiled (can be NULL)
 *
 * @return true if the shader should be compiled
 */
bool FMaterial::ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const
{
	return true;
}

//
// FColoredMaterialRenderProxy implementation.
//

const FMaterial* FColoredMaterialRenderProxy::GetMaterial(ERHIFeatureLevel::Type InFeatureLevel) const
{
	return Parent->GetMaterial(InFeatureLevel);
}

/**
 * Finds the shader matching the template type and the passed in vertex factory, asserts if not found.
 */
FShader* FMaterial::GetShader(FMeshMaterialShaderType* ShaderType, FVertexFactoryType* VertexFactoryType) const
{
	const FMeshMaterialShaderMap* MeshShaderMap = RenderingThreadShaderMap->GetMeshShaderMap(VertexFactoryType);
	FShader* Shader = MeshShaderMap ? MeshShaderMap->GetShader(ShaderType) : NULL;
	if (!Shader)
	{
		// Get the ShouldCache results that determine whether the shader should be compiled
		auto ShaderPlatform = GShaderPlatformForFeatureLevel[GetFeatureLevel()];
		bool bMaterialShouldCache = ShouldCache(ShaderPlatform, ShaderType, VertexFactoryType);
		bool bVFShouldCache = VertexFactoryType->ShouldCache(ShaderPlatform, this, ShaderType);
		bool bShaderShouldCache = ShaderType->ShouldCache(ShaderPlatform, this, VertexFactoryType);
		FString MaterialUsage = GetMaterialUsageDescription();

		int BreakPoint = 0;

		// Assert with detailed information if the shader wasn't found for rendering.  
		// This is usually the result of an incorrect ShouldCache function.
		UE_LOG(LogMaterial, Fatal,
			TEXT("Couldn't find Shader %s for Material Resource %s!\n")
			TEXT("		With VF=%s, Platform=%s \n")
			TEXT("		ShouldCache: Mat=%u, VF=%u, Shader=%u \n")
			TEXT("		MaterialUsageDesc: %s"),
			ShaderType->GetName(), 
			*GetFriendlyName(),
			VertexFactoryType->GetName(),
			*LegacyShaderPlatformToShaderFormat(ShaderPlatform).ToString(),
			bMaterialShouldCache,
			bVFShouldCache,
			bShaderShouldCache,
			*MaterialUsage
			);
	}

	return Shader;
}

/** Returns the index to the Expression in the Expressions array, or -1 if not found. */
int32 FMaterial::FindExpression( const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >&Expressions, const FMaterialUniformExpressionTexture &Expression )
{
	for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ++ExpressionIndex)
	{
		if ( Expressions[ExpressionIndex]->IsIdentical(&Expression) )
		{
			return ExpressionIndex;
		}
	}
	return -1;
}

TSet<FMaterial*> FMaterial::EditorLoadedMaterialResources;

/*-----------------------------------------------------------------------------
	FMaterialRenderContext
-----------------------------------------------------------------------------*/

/** 
 * Constructor
 */
FMaterialRenderContext::FMaterialRenderContext(
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterial,
	const FSceneView* InView)
		: MaterialRenderProxy(InMaterialRenderProxy)
		, Material(InMaterial)
		, Time(0.0f)
		, RealTime(0.0f)
{
	bShowSelection = GIsEditor && InView && InView->Family->EngineShowFlags.Selection;
}

/*-----------------------------------------------------------------------------
	FMaterialRenderProxy
-----------------------------------------------------------------------------*/

void FMaterialRenderProxy::EvaluateUniformExpressions(FUniformExpressionCache& OutUniformExpressionCache, const FMaterialRenderContext& Context, FRHICommandList* CommandListIfLocalMode) const
{
	check(IsInParallelRenderingThread());

	SCOPE_CYCLE_COUNTER(STAT_CacheUniformExpressions);
	
	// Retrieve the material's uniform expression set.
	const FUniformExpressionSet& UniformExpressionSet = Context.Material.GetRenderingThreadShaderMap()->GetUniformExpressionSet();

	OutUniformExpressionCache.CachedUniformExpressionShaderMap = Context.Material.GetRenderingThreadShaderMap();

	// Create and cache the material's uniform buffer.
	OutUniformExpressionCache.UniformBuffer = UniformExpressionSet.CreateUniformBuffer(Context, CommandListIfLocalMode, &OutUniformExpressionCache.LocalUniformBuffer);

	OutUniformExpressionCache.ParameterCollections = UniformExpressionSet.ParameterCollections;

	OutUniformExpressionCache.bUpToDate = true;
}

void FMaterialRenderProxy::CacheUniformExpressions()
{
	// Register the render proxy's as a render resource so it can receive notifications to free the uniform buffer.
	InitResource();

	check(UMaterial::GetDefaultMaterial(MD_Surface));

	TArray<FMaterialResource*> ResourcesToCache;

	UMaterialInterface::IterateOverActiveFeatureLevels([&](ERHIFeatureLevel::Type FeatureLevel)
	{
		const FMaterial* MaterialNoFallback = GetMaterialNoFallback(FeatureLevel);

		if (MaterialNoFallback && MaterialNoFallback->GetRenderingThreadShaderMap())
		{
			const FMaterial* Material = GetMaterial(FeatureLevel);

			// Do not cache uniform expressions for fallback materials. This step could
			// be skipped where we don't allow for asynchronous shader compiling.
			bool bIsFallbackMaterial = (Material != GetMaterialNoFallback(FeatureLevel));

			if (!bIsFallbackMaterial)
			{
				FMaterialRenderContext MaterialRenderContext(this, *Material, NULL);
				MaterialRenderContext.bShowSelection = GIsEditor;
				EvaluateUniformExpressions(UniformExpressionCache[(int32)FeatureLevel], MaterialRenderContext);
			}
			else
			{
				InvalidateUniformExpressionCache();
				return;
			}
		}
		else
		{
			InvalidateUniformExpressionCache();
			return;
		}
	});
}

void FMaterialRenderProxy::CacheUniformExpressions_GameThread()
{
	if (FApp::CanEverRender())
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FCacheUniformExpressionsCommand,
			FMaterialRenderProxy*,RenderProxy,this,
		{
			RenderProxy->CacheUniformExpressions();
		});
	}
}

void FMaterialRenderProxy::InvalidateUniformExpressionCache()
{
	check(IsInRenderingThread());
	for (int32 i = 0; i < ERHIFeatureLevel::Num; ++i)
	{
		UniformExpressionCache[i].bUpToDate = false;
		UniformExpressionCache[i].UniformBuffer.SafeRelease();
		UniformExpressionCache[i].CachedUniformExpressionShaderMap = NULL;
	}
}

FMaterialRenderProxy::FMaterialRenderProxy()
	: bSelected(false)
	, bHovered(false)
	, SubsurfaceProfileRT(0)
{
}

FMaterialRenderProxy::FMaterialRenderProxy(bool bInSelected, bool bInHovered)
	: bSelected(bInSelected)
	, bHovered(bInHovered)
	, SubsurfaceProfileRT(0)
{
}

FMaterialRenderProxy::~FMaterialRenderProxy()
{
	if(IsInitialized())
	{
		check(IsInRenderingThread());
		ReleaseResource();
	}
}

void FMaterialRenderProxy::InitDynamicRHI()
{
	FMaterialRenderProxy::MaterialRenderProxyMap.Add(this);
}

void FMaterialRenderProxy::ReleaseDynamicRHI()
{
	FMaterialRenderProxy::MaterialRenderProxyMap.Remove(this);
	InvalidateUniformExpressionCache();
}

TSet<FMaterialRenderProxy*> FMaterialRenderProxy::MaterialRenderProxyMap;

/*-----------------------------------------------------------------------------
	FColoredMaterialRenderProxy
-----------------------------------------------------------------------------*/

bool FColoredMaterialRenderProxy::GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
{
	if(ParameterName == ColorParamName)
	{
		*OutValue = Color;
		return true;
	}
	else
	{
		return Parent->GetVectorValue(ParameterName, OutValue, Context);
	}
}

bool FColoredMaterialRenderProxy::GetScalarValue(const FName ParameterName, float* OutValue, const FMaterialRenderContext& Context) const
{
	return Parent->GetScalarValue(ParameterName, OutValue, Context);
}

bool FColoredMaterialRenderProxy::GetTextureValue(const FName ParameterName,const UTexture** OutValue, const FMaterialRenderContext& Context) const
{
	return Parent->GetTextureValue(ParameterName,OutValue,Context);
}

/*-----------------------------------------------------------------------------
	FLightingDensityMaterialRenderProxy
-----------------------------------------------------------------------------*/
static FName NAME_LightmapRes = FName(TEXT("LightmapRes"));

bool FLightingDensityMaterialRenderProxy::GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
{
	if (ParameterName == NAME_LightmapRes)
	{
		*OutValue = FLinearColor(LightmapResolution.X, LightmapResolution.Y, 0.0f, 0.0f);
		return true;
	}
	return FColoredMaterialRenderProxy::GetVectorValue(ParameterName, OutValue, Context);
}

/*-----------------------------------------------------------------------------
	FOverrideSelectionColorMaterialRenderProxy
-----------------------------------------------------------------------------*/

const FMaterial* FOverrideSelectionColorMaterialRenderProxy::GetMaterial(ERHIFeatureLevel::Type InFeatureLevel) const
{
	return Parent->GetMaterial(InFeatureLevel);
}

bool FOverrideSelectionColorMaterialRenderProxy::GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
{
	if( ParameterName == NAME_SelectionColor )
	{
		*OutValue = SelectionColor;
		return true;
	}
	else
	{
		return Parent->GetVectorValue(ParameterName, OutValue, Context);
	}
}

bool FOverrideSelectionColorMaterialRenderProxy::GetScalarValue(const FName ParameterName, float* OutValue, const FMaterialRenderContext& Context) const
{
	return Parent->GetScalarValue(ParameterName,OutValue,Context);
}

bool FOverrideSelectionColorMaterialRenderProxy::GetTextureValue(const FName ParameterName,const UTexture** OutValue, const FMaterialRenderContext& Context) const
{
	return Parent->GetTextureValue(ParameterName,OutValue,Context);
}

/** Returns the number of samplers used in this material, or -1 if the material does not have a valid shader map (compile error or still compiling). */
int32 FMaterialResource::GetSamplerUsage() const
{
	if (GetGameThreadShaderMap())
	{
		return GetGameThreadShaderMap()->GetMaxTextureSamplers();
	}

	return -1;
}

FString FMaterialResource::GetMaterialUsageDescription() const
{
	check(Material);
	FString BaseDescription = FString::Printf(
		TEXT("LightingModel=%s, BlendMode=%s, "),
		*GetShadingModelString(GetShadingModel()), *GetBlendModeString(GetBlendMode()));

	// this changed from ",SpecialEngine, TwoSided" to ",SpecialEngine=1, TwoSided=1, TSNormal=0, ..." to be more readable
	BaseDescription += FString::Printf(
		TEXT("SpecialEngine=%d, TwoSided=%d, TSNormal=%d, InjectEmissiveIntoLPV=%d, Masked=%d, Distorted=%d, Usage={"),
		(int32)IsSpecialEngineMaterial(), (int32)IsTwoSided(), (int32)IsTangentSpaceNormal(), (int32)ShouldInjectEmissiveIntoLPV(), (int32)IsMasked(), (int32)IsDistorted());

	bool bFirst = true;
	for (int32 MaterialUsageIndex = 0; MaterialUsageIndex < MATUSAGE_MAX; MaterialUsageIndex++)
	{
		if (Material->GetUsageByFlag((EMaterialUsage)MaterialUsageIndex))
		{
			if (!bFirst)
			{
				BaseDescription += FString(TEXT(","));
			}
			BaseDescription += Material->GetUsageName((EMaterialUsage)MaterialUsageIndex);
			bFirst = false;
		}
	}
	BaseDescription += FString(TEXT("}"));

	return BaseDescription;
}

void FMaterial::GetDependentShaderAndVFTypes(EShaderPlatform Platform, TArray<FShaderType*>& OutShaderTypes, TArray<FVertexFactoryType*>& OutVFTypes) const
{
	// Iterate over all vertex factory types.
	for (TLinkedList<FVertexFactoryType*>::TIterator VertexFactoryTypeIt(FVertexFactoryType::GetTypeList()); VertexFactoryTypeIt; VertexFactoryTypeIt.Next())
	{
		FVertexFactoryType* VertexFactoryType = *VertexFactoryTypeIt;
		check(VertexFactoryType);

		if (VertexFactoryType->IsUsedWithMaterials())
		{
			bool bAddedTypeFromThisVF = false;

			// Iterate over all mesh material shader types.
			for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
			{
				FMeshMaterialShaderType* ShaderType = ShaderTypeIt->GetMeshMaterialShaderType();

				if (ShaderType && 
					VertexFactoryType && 
					ShaderType->ShouldCache(Platform, this, VertexFactoryType) && 
					ShouldCache(Platform, ShaderType, VertexFactoryType) &&
					VertexFactoryType->ShouldCache(Platform, this, ShaderType)
					)
				{
					bAddedTypeFromThisVF = true;
					OutShaderTypes.AddUnique(ShaderType);
				}
			}

			if (bAddedTypeFromThisVF)
			{
				OutVFTypes.Add(VertexFactoryType);
			}
		}
	}

	// Iterate over all material shader types.
	for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList()); ShaderTypeIt; ShaderTypeIt.Next())
	{
		FMaterialShaderType* ShaderType = ShaderTypeIt->GetMaterialShaderType();

		if (ShaderType && 
			ShaderType->ShouldCache(Platform, this) && 
			ShouldCache(Platform, ShaderType, NULL)
			)
		{
			OutShaderTypes.Add(ShaderType);
		}
	}

	// Sort by name so that we get deterministic keys
	OutShaderTypes.Sort(FCompareShaderTypes());
	OutVFTypes.Sort(FCompareVertexFactoryTypes());
}

void FMaterial::GetReferencedTexturesHash(FSHAHash& OutHash) const
{
	FSHA1 HashState;

	const TArray<UTexture*>& ReferencedTextures = GetReferencedTextures();
	// Hash the names of the uniform expression textures to capture changes in their order or values resulting from material compiler code changes
	for (int32 TextureIndex = 0; TextureIndex < ReferencedTextures.Num(); TextureIndex++)
	{
		FString TextureName;

		if (ReferencedTextures[TextureIndex])
		{
			TextureName = ReferencedTextures[TextureIndex]->GetName();
		}

		HashState.UpdateWithString(*TextureName, TextureName.Len());
	}

	HashState.Final();
	HashState.GetHash(&OutHash.Hash[0]);
}

/**
 * Get user source code for the material, with a list of code snippets to highlight representing the code for each MaterialExpression
 * @param OutSource - generated source code
 * @param OutHighlightMap - source code highlight list
 * @return - true on Success
 */
bool FMaterial::GetMaterialExpressionSource( FString& OutSource )
{
#if WITH_EDITORONLY_DATA
	class FViewSourceMaterialTranslator : public FHLSLMaterialTranslator
	{
	public:
		FViewSourceMaterialTranslator(FMaterial* InMaterial,FMaterialCompilationOutput& InMaterialCompilationOutput,const FStaticParameterSet& StaticParameters,EShaderPlatform InPlatform,EMaterialQualityLevel::Type InQualityLevel,ERHIFeatureLevel::Type InFeatureLevel)
		:	FHLSLMaterialTranslator(InMaterial,InMaterialCompilationOutput,StaticParameters,InPlatform,InQualityLevel,InFeatureLevel)
		{}
	};

	FMaterialCompilationOutput TempOutput;
	FViewSourceMaterialTranslator MaterialTranslator(this, TempOutput, FStaticParameterSet(), GMaxRHIShaderPlatform, GetQualityLevel(), GetFeatureLevel());
	bool bSuccess = MaterialTranslator.Translate();

	if( bSuccess )
	{
		// Generate the HLSL
		OutSource = MaterialTranslator.GetMaterialShaderCode();
	}
	return bSuccess;
#else
	UE_LOG(LogMaterial, Fatal,TEXT("Not supported."));
	return false;
#endif
}

/** Recompiles any materials in the EditorLoadedMaterialResources list if they are not complete. */
void FMaterial::UpdateEditorLoadedMaterialResources(EShaderPlatform InShaderPlatform)
{
	for (TSet<FMaterial*>::TIterator It(EditorLoadedMaterialResources); It; ++It)
	{
		FMaterial* CurrentMaterial = *It;
		if (!CurrentMaterial->GetGameThreadShaderMap() || !CurrentMaterial->GetGameThreadShaderMap()->IsComplete(CurrentMaterial, true))
		{
			CurrentMaterial->CacheShaders(InShaderPlatform, true);
		}
	}
}

void FMaterial::BackupEditorLoadedMaterialShadersToMemory(TMap<FMaterialShaderMap*, TScopedPointer<TArray<uint8> > >& ShaderMapToSerializedShaderData)
{
	for (TSet<FMaterial*>::TIterator It(EditorLoadedMaterialResources); It; ++It)
	{
		FMaterial* CurrentMaterial = *It;
		FMaterialShaderMap* ShaderMap = CurrentMaterial->GetGameThreadShaderMap();

		if (ShaderMap && !ShaderMapToSerializedShaderData.Contains(ShaderMap))
		{
			TArray<uint8>* ShaderData = ShaderMap->BackupShadersToMemory();
			ShaderMapToSerializedShaderData.Emplace(ShaderMap, ShaderData);
		}
	}
}

void FMaterial::RestoreEditorLoadedMaterialShadersFromMemory(const TMap<FMaterialShaderMap*, TScopedPointer<TArray<uint8> > >& ShaderMapToSerializedShaderData)
{
	for (TSet<FMaterial*>::TIterator It(EditorLoadedMaterialResources); It; ++It)
	{
		FMaterial* CurrentMaterial = *It;
		FMaterialShaderMap* ShaderMap = CurrentMaterial->GetGameThreadShaderMap();

		if (ShaderMap)
		{
			const TScopedPointer<TArray<uint8> >* ShaderData = ShaderMapToSerializedShaderData.Find(ShaderMap);

			if (ShaderData)
			{
				ShaderMap->RestoreShadersFromMemory(**ShaderData);
			}
		}
	}
}

FMaterialUpdateContext::FMaterialUpdateContext(uint32 Options, EShaderPlatform InShaderPlatform)
{
	bool bReregisterComponents = (Options & EOptions::ReregisterComponents) != 0;
	bool bRecreateRenderStates = (Options & EOptions::RecreateRenderStates) != 0;

	bSyncWithRenderingThread = (Options & EOptions::SyncWithRenderingThread) != 0;
	if (bReregisterComponents)
	{
		ComponentReregisterContext = new FGlobalComponentReregisterContext();
	}
	else if (bRecreateRenderStates)
	{
		ComponentRecreateRenderStateContext = new FGlobalComponentRecreateRenderStateContext();
	}
	if (bSyncWithRenderingThread)
	{
		FlushRenderingCommands();
	}
	ShaderPlatform = InShaderPlatform;
}

void FMaterialUpdateContext::AddMaterial(UMaterial* Material)
{
	UpdatedMaterials.Add(Material);
	UpdatedMaterialInterfaces.Add(Material);
}

void FMaterialUpdateContext::AddMaterialInstance(UMaterialInstance* Instance)
{
	UpdatedMaterials.Add(Instance->GetMaterial());
	UpdatedMaterialInterfaces.Add(Instance);
}

FMaterialUpdateContext::~FMaterialUpdateContext()
{
	double StartTime = FPlatformTime::Seconds();
	bool bProcess = false;

	// if the shader platform that was processed is not the currently rendering shader platform, 
	// there's no reason to update all of the runtime components
	UMaterialInterface::IterateOverActiveFeatureLevels([&](ERHIFeatureLevel::Type InFeatureLevel)
	{
		if (ShaderPlatform == GShaderPlatformForFeatureLevel[InFeatureLevel])
		{
			bProcess = true;
		}
	});

	if (!bProcess)
	{
		return;
	}

	// Flush rendering commands even though we already did so in the constructor.
	// Anything may have happened since the constructor has run. The flush is
	// done once here to avoid calling it once per static permutation we update.
	if (bSyncWithRenderingThread)
	{
		FlushRenderingCommands();
	}

	TArray<const FMaterial*> MaterialResourcesToUpdate;
	TArray<UMaterialInstance*> InstancesToUpdate;

	bool bUpdateStaticDrawLists = !ComponentReregisterContext.IsValid() && !ComponentRecreateRenderStateContext.IsValid();

	// If static draw lists must be updated, gather material resources from all updated materials.
	if (bUpdateStaticDrawLists)
	{
		for (TSet<UMaterial*>::TConstIterator It(UpdatedMaterials); It; ++It)
		{
			UMaterial* Material = *It;

			for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
			{
				for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
				{
					FMaterialResource* CurrentResource = Material->MaterialResources[QualityLevelIndex][FeatureLevelIndex];
					MaterialResourcesToUpdate.Add(CurrentResource);
				}
			}
		}
	}

	// Go through all loaded material instances and recompile their static permutation resources if needed
	// This is necessary since the parent UMaterial stores information about how it should be rendered, (eg bUsesDistortion)
	// but the child can have its own shader map which may not contain all the shaders that the parent's settings indicate that it should.
	for (TObjectIterator<UMaterialInstance> It; It; ++It)
	{
		UMaterialInstance* CurrentMaterialInstance = *It;
		UMaterial* BaseMaterial = CurrentMaterialInstance->GetMaterial();

		if (UpdatedMaterials.Contains(BaseMaterial))
		{
			// Check to see if this instance is dependent on any of the material interfaces we directly updated.
			for (auto InterfaceIt = UpdatedMaterialInterfaces.CreateConstIterator(); InterfaceIt; ++InterfaceIt)
			{
				if (CurrentMaterialInstance->IsDependent(*InterfaceIt))
				{
					InstancesToUpdate.Add(CurrentMaterialInstance);
					break;
				}
			}
		}
	}

	// Material instances that use this base material must have their uniform expressions recached 
	// However, some material instances that use this base material may also depend on another MI with static parameters
	// So we must update the MI's with static parameters first, and do other MI's in a second pass
	int32 NumInstancesWithStaticPermutations = 0;
	for (int32 MIIndex = 0; MIIndex < InstancesToUpdate.Num(); MIIndex++)
	{
		UMaterialInstance* CurrentMaterialInstance = InstancesToUpdate[MIIndex];

		if (CurrentMaterialInstance->bHasStaticPermutationResource)
		{
			NumInstancesWithStaticPermutations++;
			CurrentMaterialInstance->InitStaticPermutation();

			// Collect FMaterial's that have been recompiled
			if (bUpdateStaticDrawLists)
			{
				for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
				{
					for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
					{
						FMaterialResource* CurrentResource = CurrentMaterialInstance->StaticPermutationMaterialResources[QualityLevelIndex][FeatureLevelIndex];
						MaterialResourcesToUpdate.Add(CurrentResource);
					}
				}
			}
		}
	}

	// Recache uniform expressions on dependent MI's without static parameters
	for (int32 MIIndex = 0; MIIndex < InstancesToUpdate.Num(); MIIndex++)
	{
		UMaterialInstance* CurrentMaterialInstance = InstancesToUpdate[MIIndex];

		if (!CurrentMaterialInstance->bHasStaticPermutationResource)
		{
			CurrentMaterialInstance->InitStaticPermutation();
		}
	}

	if (bUpdateStaticDrawLists)
	{
		// Update static draw lists affected by any FMaterials that were recompiled
		// This is only needed if we aren't reregistering components which is not always
		// safe, e.g. while a component is being registered.
		GetRendererModule().UpdateStaticDrawListsForMaterials(MaterialResourcesToUpdate);
	}
	else if (ComponentReregisterContext.IsValid())
	{
		ComponentReregisterContext.Reset();
	}
	else if (ComponentRecreateRenderStateContext.IsValid())
	{
		ComponentRecreateRenderStateContext.Reset();
	}

	double EndTime = FPlatformTime::Seconds();
	UE_LOG(LogMaterial, Log,
		TEXT("%f seconds spent updating %d materials, %d interfaces, %d instances, %d with static permutations."),
		(float)(EndTime - StartTime),
		UpdatedMaterials.Num(),
		UpdatedMaterialInterfaces.Num(),
		InstancesToUpdate.Num(),
		NumInstancesWithStaticPermutations
		);
}

EMaterialProperty GetMaterialPropertyFromInputOutputIndex(int32 Index)
{
	// Warning: If you change this mapping, you will need to handle backwards compatibility for nodes that use it, otherwise pins will get swapped

	switch(Index)
	{
	case 0: return MP_BaseColor;
	case 1: return MP_Metallic;
	case 2: return MP_Specular;
	case 3: return MP_Roughness;
	case 4: return MP_EmissiveColor;
	case 5: return MP_Opacity;
	case 6: return MP_OpacityMask;
	case 7: return MP_Normal;
	case 8: return MP_WorldPositionOffset;
	case 9: return MP_WorldDisplacement;
	case 10: return MP_TessellationMultiplier;
	case 11: return MP_SubsurfaceColor;
	case 12: return MP_ClearCoat;
	case 13: return MP_ClearCoatRoughness;
	case 14: return MP_AmbientOcclusion;
	case 15: return MP_Refraction;
	};

	const int32 UVStart = 16;
	const int32 UVEnd = UVStart + MP_CustomizedUVs7 - MP_CustomizedUVs0;

	if (Index >= UVStart && Index <= UVEnd)
	{
		return (EMaterialProperty)(MP_CustomizedUVs0 + Index - UVStart);
	}

	if (Index == UVEnd + 1)
	{
		return MP_MaterialAttributes;
	}

	if (Index == UVEnd + 2)
	{
		return MP_PixelDepthOffset;
	}

	return MP_MAX;
}

int32 GetInputOutputIndexFromMaterialProperty(EMaterialProperty Property)
{
	// Warning: If you change this mapping, you will need to handle backwards compatibility for nodes that use it, otherwise pins will get swapped

	switch(Property)
	{
	case MP_BaseColor: return 0;
	case MP_Metallic: return 1;
	case MP_Specular: return 2;
	case MP_Roughness: return 3;
	case MP_EmissiveColor: return 4;
	case MP_Opacity: return 5;
	case MP_OpacityMask: return 6;
	case MP_Normal: return 7;
	case MP_WorldPositionOffset: return 8;
	case MP_WorldDisplacement: return 9;
	case MP_TessellationMultiplier: return 10;
	case MP_SubsurfaceColor: return 11;
	case MP_ClearCoat: return 12;
	case MP_ClearCoatRoughness: return 13;
	case MP_AmbientOcclusion: return 14;
	case MP_Refraction: return 15;
	case MP_MaterialAttributes: UE_LOG(LogMaterial, Fatal, TEXT("We should never need the IO index of the MaterialAttriubtes property."));
	};

	const int32 UVStart = 16;
	const int32 UVEnd = UVStart + MP_CustomizedUVs7 - MP_CustomizedUVs0;

	if (Property >= MP_CustomizedUVs0 && Property <= MP_CustomizedUVs7)
	{
		return UVStart + Property - MP_CustomizedUVs0;
	}

	if (Property == MP_PixelDepthOffset)
	{
		return UVEnd + 2;
	}

	return -1;
}

int32 GetDefaultExpressionForMaterialProperty(FMaterialCompiler* Compiler, EMaterialProperty Property)
{
	switch (Property)
	{
		case MP_Opacity:				return Compiler->Constant(1.0f);
		case MP_OpacityMask:			return Compiler->Constant(1.0f);
		case MP_Metallic:				return Compiler->Constant(0.0f);
		case MP_Specular:				return Compiler->Constant(0.5f);
		case MP_Roughness:				return Compiler->Constant(0.5f);
		case MP_TessellationMultiplier:	return Compiler->Constant(1.0f);
		case MP_ClearCoat:				return Compiler->Constant(1.0f);
		case MP_ClearCoatRoughness:		return Compiler->Constant(0.1f);
		case MP_AmbientOcclusion:		return Compiler->Constant(1.0f);
		case MP_PixelDepthOffset:		return Compiler->Constant(0.0f);

		case MP_EmissiveColor:			return Compiler->Constant3(0, 0, 0);
		case MP_DiffuseColor:			return Compiler->Constant3(0, 0, 0);
		case MP_SpecularColor:			return Compiler->Constant3(0, 0, 0);
		case MP_BaseColor:				return Compiler->Constant3(0, 0, 0);
		case MP_SubsurfaceColor:		return Compiler->Constant3(1, 1, 1);
		case MP_Normal:					return Compiler->Constant3(0, 0, 1);
		case MP_WorldPositionOffset:	return Compiler->Constant3(0, 0, 0);
		case MP_WorldDisplacement:		return Compiler->Constant3(0, 0, 0);
		case MP_Refraction:				return Compiler->Constant3(1, 0, 0);

		case MP_CustomizedUVs0:
		case MP_CustomizedUVs1:
		case MP_CustomizedUVs2:
		case MP_CustomizedUVs3:
		case MP_CustomizedUVs4:
		case MP_CustomizedUVs5:
		case MP_CustomizedUVs6:
		case MP_CustomizedUVs7:
			{
//				return Compiler->Constant2(0.0f, 0.0f);				
				const int32 TextureCoordinateIndex = Property - MP_CustomizedUVs0;
				// The user did not customize this UV, pass through the vertex texture coordinates
				return Compiler->TextureCoordinate(TextureCoordinateIndex, false, false);
			}
		case MP_MaterialAttributes:
			break;
	}

	check(0);
	return INDEX_NONE;
}

FString GetNameOfMaterialProperty(EMaterialProperty Property)
{
	switch(Property)
	{
	case MP_BaseColor:				return TEXT("BaseColor");
	case MP_Metallic:				return TEXT("Metallic");
	case MP_Specular:				return TEXT("Specular");
	case MP_Roughness:				return TEXT("Roughness");
	case MP_Normal:					return TEXT("Normal");
	case MP_EmissiveColor:			return TEXT("EmissiveColor");
	case MP_Opacity:				return TEXT("Opacity");
	case MP_OpacityMask:			return TEXT("OpacityMask"); 
	case MP_WorldPositionOffset:	return TEXT("WorldPositionOffset");
	case MP_WorldDisplacement:		return TEXT("WorldDisplacement");
	case MP_TessellationMultiplier: return TEXT("TessellationMultiplier");
	case MP_SubsurfaceColor:		return TEXT("SubsurfaceColor");
	case MP_ClearCoat:				return TEXT("ClearCoat");
	case MP_ClearCoatRoughness:		return TEXT("ClearCoatRoughness");
	case MP_AmbientOcclusion:		return TEXT("AmbientOcclusion");
	case MP_Refraction:				return TEXT("Refraction");
	case MP_PixelDepthOffset:		return TEXT("PixelDepthOffset");
	};

	if (Property >= MP_CustomizedUVs0 && Property <= MP_CustomizedUVs7)
	{
		return FString::Printf(TEXT("CustomizedUVs%u"), Property - MP_CustomizedUVs0);
	}

	return TEXT("");
}


bool UMaterialInterface::IsPropertyActive(EMaterialProperty InProperty)const
{
	//TODO: Disable properties in instances based on the currently set overrides and other material settings?
	//For now just allow all properties in instances. 
	//This had to be refactored into the instance as some override properties alter the properties that are active.
	return false;
}

int32 UMaterialInterface::CompilePropertyEx( class FMaterialCompiler* Compiler, EMaterialProperty Property )
{
	return INDEX_NONE;
}

int32 UMaterialInterface::CompileProperty(FMaterialCompiler* Compiler, EMaterialProperty Property)
{
	if (IsPropertyActive(Property))
	{
		return CompilePropertyEx(Compiler, Property);
	}
	else
	{
		return GetDefaultExpressionForMaterialProperty(Compiler, Property);
	}
}

void DoMaterialAttributeReorder(FExpressionInput* Input, int32 UE4Ver)
{
	if( Input && Input->Expression && Input->Expression->IsA(UMaterialExpressionBreakMaterialAttributes::StaticClass()) )
	{
		if( UE4Ver < VER_UE4_MATERIAL_ATTRIBUTES_REORDERING )
		{
			switch(Input->OutputIndex)
			{
			case 4: Input->OutputIndex = 7; break;
			case 5: Input->OutputIndex = 4; break;
			case 6: Input->OutputIndex = 5; break;
			case 7: Input->OutputIndex = 6; break;
			}
		}
		
		if( UE4Ver < VER_UE4_FIX_REFRACTION_INPUT_MASKING && Input->OutputIndex == 13 )
		{
			Input->Mask = 1;
			Input->MaskR = 1;
			Input->MaskG = 1;
			Input->MaskB = 1;
			Input->MaskA = 0;
		}

		// closest version to the clear coat change
		if( UE4Ver < VER_UE4_ADD_ROOTCOMPONENT_TO_FOLIAGEACTOR && Input->OutputIndex >= 12 )
		{
			Input->OutputIndex += 2;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

FMaterialInstanceBasePropertyOverrides::FMaterialInstanceBasePropertyOverrides()
	:bOverride_OpacityMaskClipValue(false)
	,bOverride_BlendMode(false)
	,bOverride_ShadingModel(false)
	,bOverride_TwoSided(false)
	,OpacityMaskClipValue(.333333f)
	,BlendMode(BLEND_Opaque)
	,ShadingModel(MSM_DefaultLit)
	,TwoSided(0)
{

}

bool FMaterialInstanceBasePropertyOverrides::operator==(const FMaterialInstanceBasePropertyOverrides& Other)const
{
	return	bOverride_OpacityMaskClipValue == Other.bOverride_OpacityMaskClipValue &&
			bOverride_BlendMode == Other.bOverride_BlendMode &&
			bOverride_ShadingModel == Other.bOverride_ShadingModel &&
			bOverride_TwoSided == Other.bOverride_TwoSided &&
			OpacityMaskClipValue == Other.OpacityMaskClipValue &&
			BlendMode == Other.BlendMode &&
			ShadingModel == Other.ShadingModel &&
			TwoSided == Other.TwoSided;
}

bool FMaterialInstanceBasePropertyOverrides::operator!=(const FMaterialInstanceBasePropertyOverrides& Other)const
{
	return !(*this == Other);
}

//////////////////////////////////////////////////////////////////////////

bool FMaterialShaderMapId::ContainsShaderType(const FShaderType* ShaderType) const
{
	for (int32 TypeIndex = 0; TypeIndex < ShaderTypeDependencies.Num(); TypeIndex++)
	{
		if (ShaderTypeDependencies[TypeIndex].ShaderType == ShaderType)
		{
			return true;
		}
	}

	return false;
}

bool FMaterialShaderMapId::ContainsVertexFactoryType(const FVertexFactoryType* VFType) const
{
	for (int32 TypeIndex = 0; TypeIndex < VertexFactoryTypeDependencies.Num(); TypeIndex++)
	{
		if (VertexFactoryTypeDependencies[TypeIndex].VertexFactoryType == VFType)
		{
			return true;
		}
	}

	return false;
}
