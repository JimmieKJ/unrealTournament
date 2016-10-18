// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
/*=============================================================================
	HLSLMaterialTranslator.h: Translates material expressions into HLSL code.
=============================================================================*/

#pragma once

#if WITH_EDITORONLY_DATA

#include "EnginePrivate.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "Materials/MaterialFunction.h"
#include "MaterialCompiler.h"
#include "MaterialUniformExpressions.h"
#include "ParameterCollection.h"
#include "Materials/MaterialParameterCollection.h"
#include "LazyPrintf.h"

/** @return the number of components in a vector type. */
static inline uint32 GetNumComponents(EMaterialValueType Type)
{
	switch(Type)
	{
		case MCT_Float:
		case MCT_Float1: return 1;
		case MCT_Float2: return 2;
		case MCT_Float3: return 3;
		case MCT_Float4: return 4;
		default: return 0;
	}
}

/** @return the vector type containing a given number of components. */
static inline EMaterialValueType GetVectorType(uint32 NumComponents)
{
	switch(NumComponents)
	{
		case 1: return MCT_Float;
		case 2: return MCT_Float2;
		case 3: return MCT_Float3;
		case 4: return MCT_Float4;
		default: return MCT_Unknown;
	};
}

static inline int32 SwizzleComponentToIndex(TCHAR Component)
{
	switch (Component)
	{
	case TCHAR('x'): case TCHAR('X'): case TCHAR('r'): case TCHAR('R'): return 0;
	case TCHAR('y'): case TCHAR('Y'): case TCHAR('g'): case TCHAR('G'): return 1;
	case TCHAR('z'): case TCHAR('Z'): case TCHAR('b'): case TCHAR('B'): return 2;
	case TCHAR('w'): case TCHAR('W'): case TCHAR('a'): case TCHAR('A'): return 3;
	default:
		return -1;
	}
}

struct FShaderCodeChunk
{
	/** 
	 * Definition string of the code chunk. 
	 * If !bInline && !UniformExpression || UniformExpression->IsConstant(), this is the definition of a local variable named by SymbolName.
	 * Otherwise if bInline || (UniformExpression && UniformExpression->IsConstant()), this is a code expression that needs to be inlined.
	 */
	FString Definition;
	/** 
	 * Name of the local variable used to reference this code chunk. 
	 * If bInline || UniformExpression, there will be no symbol name and Definition should be used directly instead.
	 */
	FString SymbolName;
	/** Reference to a uniform expression, if this code chunk has one. */
	TRefCountPtr<FMaterialUniformExpression> UniformExpression;
	EMaterialValueType Type;
	/** Whether the code chunk should be inlined or not.  If true, SymbolName is empty and Definition contains the code to inline. */
	bool bInline;

	/** Ctor for creating a new code chunk with no associated uniform expression. */
	FShaderCodeChunk(const TCHAR* InDefinition,const FString& InSymbolName,EMaterialValueType InType,bool bInInline):
		Definition(InDefinition),
		SymbolName(InSymbolName),
		UniformExpression(NULL),
		Type(InType),
		bInline(bInInline)
	{}

	/** Ctor for creating a new code chunk with a uniform expression. */
	FShaderCodeChunk(FMaterialUniformExpression* InUniformExpression,const TCHAR* InDefinition,EMaterialValueType InType):
		Definition(InDefinition),
		UniformExpression(InUniformExpression),
		Type(InType),
		bInline(false)
	{}
};

class FHLSLMaterialTranslator : public FMaterialCompiler
{
protected:

	/** The shader frequency of the current material property being compiled. */
	EShaderFrequency ShaderFrequency;
	/** The current material property being compiled.  This affects the behavior of all compiler functions except GetFixedParameterCode. */
	EMaterialProperty MaterialProperty;
	/** The code chunks corresponding to the currently compiled property or custom output. */
	TArray<FShaderCodeChunk>* CurrentScopeChunks;

	// List of Shared pixel properties. Used to share generated code
	bool SharedPixelProperties[CompiledMP_MAX];

	/* Stack that tracks compiler state specific to the function currently being compiled. */
	TArray<FMaterialFunctionCompileState> FunctionStacks[SF_NumFrequencies];
	/** Material being compiled.  Only transient compilation output like error information can be stored on the FMaterial. */
	FMaterial* Material;
	/** Compilation output which will be stored in the DDC. */
	FMaterialCompilationOutput& MaterialCompilationOutput;
	FStaticParameterSet StaticParameters;
	EShaderPlatform Platform;
	/** Quality level being compiled for. */
	EMaterialQualityLevel::Type QualityLevel;

	/** Feature level being compiled for. */
	ERHIFeatureLevel::Type FeatureLevel;

	/** Code chunk definitions corresponding to each of the material inputs, only initialized after Translate has been called. */
	FString TranslatedCodeChunkDefinitions[CompiledMP_MAX];

	/** Code chunks corresponding to each of the material inputs, only initialized after Translate has been called. */
	FString TranslatedCodeChunks[CompiledMP_MAX];

	/** Line number of the #line in MaterialTemplate.usf */
	int32 MaterialTemplateLineNumber;

	/** Stores the resource declarations */
	FString ResourcesString;

	/** Contents of the MaterialTemplate.usf file */
	FString MaterialTemplate;

	// Array of code chunks per material property
	TArray<FShaderCodeChunk> SharedPropertyCodeChunks[SF_NumFrequencies];

	// Uniform expressions used across all material properties
	TArray<FShaderCodeChunk> UniformExpressions;

	/** Parameter collections referenced by this material.  The position in this array is used as an index on the shader parameter. */
	TArray<UMaterialParameterCollection*> ParameterCollections;

	// Index of the next symbol to create
	int32 NextSymbolIndex;

	/** Any custom expression function implementations */
	TArray<FString> CustomExpressionImplementations;

	/** Any custom output function implementations */
	TArray<FString> CustomOutputImplementations;

	/** Whether the translation succeeded. */
	uint32 bSuccess : 1;
	/** Whether the compute shader material inputs were compiled. */
	uint32 bCompileForComputeShader : 1;
	/** Whether the compiled material uses scene depth. */
	uint32 bUsesSceneDepth : 1;
	/** true if the material needs particle position. */
	uint32 bNeedsParticlePosition : 1;
	/** true if the material needs particle velocity. */
	uint32 bNeedsParticleVelocity : 1;
	/** true of the material uses a particle dynamic parameter */
	uint32 bNeedsParticleDynamicParameter : 1;
	/** true if the material needs particle relative time. */
	uint32 bNeedsParticleTime : 1;
	/** true if the material uses particle motion blur. */
	uint32 bUsesParticleMotionBlur : 1;
	/** true if the material needs particle random value. */
	uint32 bNeedsParticleRandom : 1;
	/** true if the material uses spherical particle opacity. */
	uint32 bUsesSphericalParticleOpacity : 1;
	/** true if the material uses particle sub uvs. */
	uint32 bUsesParticleSubUVs : 1;
	/** Boolean indicating using LightmapUvs */
	uint32 bUsesLightmapUVs : 1;
	/** Whether the material uses AO Material Mask */
	uint32 bUsesAOMaterialMask : 1;
	/** true if needs SpeedTree code */
	uint32 bUsesSpeedTree : 1;
	/** Boolean indicating the material uses worldspace position without shader offsets applied */
	uint32 bNeedsWorldPositionExcludingShaderOffsets : 1;
	/** true if the material needs particle size. */
	uint32 bNeedsParticleSize : 1;
	/** true if any scene texture expressions are reading from post process inputs */
	uint32 bNeedsSceneTexturePostProcessInputs : 1;
	/** true if any atmospheric fog expressions are used */
	uint32 bUsesAtmosphericFog : 1;
	/** true if the material reads vertex color in the pixel shader. */
	uint32 bUsesVertexColor : 1;
	/** true if the material reads particle color in the pixel shader. */
	uint32 bUsesParticleColor : 1;
	/** true if the material reads mesh particle transform in the pixel shader. */
	uint32 bUsesParticleTransform : 1;

	uint32 bUsesTransformVector : 1;
	// True if the current property requires last frame's information
	uint32 bCompilingPreviousFrame : 1;
	/** True if material will output accurate velocities during base pass rendering. */
	uint32 bOutputsBasePassVelocities : 1;
	uint32 bUsesPixelDepthOffset : 1;
	/** Tracks the number of texture coordinates used by this material. */
	uint32 NumUserTexCoords;
	/** Tracks the number of texture coordinates used by the vertex shader in this material. */
	uint32 NumUserVertexTexCoords;

public: 

	FHLSLMaterialTranslator(FMaterial* InMaterial,
		FMaterialCompilationOutput& InMaterialCompilationOutput,
		const FStaticParameterSet& InStaticParameters,
		EShaderPlatform InPlatform,
		EMaterialQualityLevel::Type InQualityLevel,
		ERHIFeatureLevel::Type InFeatureLevel)
	:	ShaderFrequency(SF_Pixel)
	,	MaterialProperty(MP_EmissiveColor)
	,	Material(InMaterial)
	,	MaterialCompilationOutput(InMaterialCompilationOutput)
	,	StaticParameters(InStaticParameters)
	,	Platform(InPlatform)
	,	QualityLevel(InQualityLevel)
	,	FeatureLevel(InFeatureLevel)
	,	MaterialTemplateLineNumber(INDEX_NONE)
	,	NextSymbolIndex(INDEX_NONE)
	,	bSuccess(false)
	,	bCompileForComputeShader(false)
	,	bUsesSceneDepth(false)
	,	bNeedsParticlePosition(false)
	,	bNeedsParticleVelocity(false)
	,	bNeedsParticleDynamicParameter(false)
	,	bNeedsParticleTime(false)
	,	bUsesParticleMotionBlur(false)
	,	bNeedsParticleRandom(false)
	,	bUsesSphericalParticleOpacity(false)
	,   bUsesParticleSubUVs(false)
	,	bUsesLightmapUVs(false)
	,	bUsesAOMaterialMask(false)
	,	bUsesSpeedTree(false)
	,	bNeedsWorldPositionExcludingShaderOffsets(false)
	,	bNeedsParticleSize(false)
	,	bNeedsSceneTexturePostProcessInputs(false)
	,	bUsesAtmosphericFog(false)
	,	bUsesVertexColor(false)
	,	bUsesParticleColor(false)
	,	bUsesParticleTransform(false)
	,	bUsesTransformVector(false)
	,	bCompilingPreviousFrame(false)
	,	bOutputsBasePassVelocities(true)
	,	bUsesPixelDepthOffset(false)
	,	NumUserTexCoords(0)
	,	NumUserVertexTexCoords(0)
	{
		FMemory::Memzero(SharedPixelProperties);

		SharedPixelProperties[MP_Normal] = true;
		SharedPixelProperties[MP_EmissiveColor] = true;
		SharedPixelProperties[MP_Opacity] = true;
		SharedPixelProperties[MP_OpacityMask] = true;
		SharedPixelProperties[MP_BaseColor] = true;
		SharedPixelProperties[MP_Metallic] = true;
		SharedPixelProperties[MP_Specular] = true;
		SharedPixelProperties[MP_Roughness] = true;
		SharedPixelProperties[MP_AmbientOcclusion] = true;
		SharedPixelProperties[MP_Refraction] = true;
		SharedPixelProperties[MP_PixelDepthOffset] = true;

		{
			for (int32 Frequency = 0; Frequency < SF_NumFrequencies; ++Frequency)
			{
				FunctionStacks[Frequency].Add(FMaterialFunctionCompileState(nullptr));
			}
		}
	}
 
	bool Translate()
	{
		STAT(double HLSLTranslateTime = 0);
		{
			SCOPE_SECONDS_COUNTER(HLSLTranslateTime);
			bSuccess = true;

			// WARNING: No compile outputs should be stored on the UMaterial / FMaterial / FMaterialResource, unless they are transient editor-only data (like error expressions)
			// Compile outputs that need to be saved must be stored in MaterialCompilationOutput, which will be saved to the DDC.

			Material->CompileErrors.Empty();
			Material->ErrorExpressions.Empty();

			bCompileForComputeShader = Material->IsLightFunction();

			// Generate code:
			// Normally one would expect the generator to emit something like
			//		float Local0 = ...
			//		...
			//		float Local3= ...
			//		...
			//		float Localn= ...
			//		PixelMaterialInputs.EmissiveColor = Local0 + ...
			//		PixelMaterialInputs.Normal = Local3 * ...
			// However because the Normal can be used in the middle of generating other Locals (which happens when using a node like PixelNormalWS)
			// instead we generate this:
			//		float Local0 = ...
			//		...
			//		float Local3= ...
			//		PixelMaterialInputs.Normal = Local3 * ...
			//		...
			//		float Localn= ...
			//		PixelMaterialInputs.EmissiveColor = Local0 + ...
			// in other words, compile Normal first, then emit all the expressions up to the last one Normal requires;
			// assign the normal into the shared struct, then emit the remaining expressions; finally assign the rest of the shared struct inputs.
			// Inputs that are not shared, have false in the SharedPixelProperties array, and those ones will emit the full code.

			int32 NormalCodeChunkEnd = -1;
			int32 Chunk[CompiledMP_MAX];

			memset(Chunk, -1, sizeof(Chunk));

			const EShaderFrequency NormalShaderFrequency = GetMaterialPropertyShaderFrequency(MP_Normal);

			// Normal must always be compiled first; this will ensure its chunk calculations are the first to be added
			{
				// Verify that start chunk is 0
				check(SharedPropertyCodeChunks[NormalShaderFrequency].Num() == 0);
				Chunk[MP_Normal]						= Material->CompilePropertyAndSetMaterialProperty(MP_Normal, this);
				NormalCodeChunkEnd = SharedPropertyCodeChunks[NormalShaderFrequency].Num();
			}

			// Rest of properties
			Chunk[MP_EmissiveColor]					= Material->CompilePropertyAndSetMaterialProperty(MP_EmissiveColor         ,this);
			Chunk[MP_DiffuseColor]					= Material->CompilePropertyAndSetMaterialProperty(MP_DiffuseColor          ,this);
			Chunk[MP_SpecularColor]					= Material->CompilePropertyAndSetMaterialProperty(MP_SpecularColor         ,this);
			Chunk[MP_BaseColor]						= Material->CompilePropertyAndSetMaterialProperty(MP_BaseColor             ,this);
			Chunk[MP_Metallic]						= Material->CompilePropertyAndSetMaterialProperty(MP_Metallic              ,this);
			Chunk[MP_Specular]						= Material->CompilePropertyAndSetMaterialProperty(MP_Specular              ,this);
			Chunk[MP_Roughness]						= Material->CompilePropertyAndSetMaterialProperty(MP_Roughness             ,this);
			Chunk[MP_Opacity]						= Material->CompilePropertyAndSetMaterialProperty(MP_Opacity               ,this);
			Chunk[MP_OpacityMask]					= Material->CompilePropertyAndSetMaterialProperty(MP_OpacityMask           ,this);
			Chunk[MP_WorldPositionOffset]			= Material->CompilePropertyAndSetMaterialProperty(MP_WorldPositionOffset   ,this);
			if (FeatureLevel >= ERHIFeatureLevel::SM5)
			{
				Chunk[MP_WorldDisplacement]			= Material->CompilePropertyAndSetMaterialProperty(MP_WorldDisplacement     ,this);
			}
			else
			{
				// normally called in CompilePropertyAndSetMaterialProperty, needs to be called!!
				SetMaterialProperty(MP_WorldDisplacement);
				Chunk[MP_WorldDisplacement]			= Constant3(0.0f, 0.0f, 0.0f);
			}
			Chunk[MP_TessellationMultiplier]		= Material->CompilePropertyAndSetMaterialProperty(MP_TessellationMultiplier,this);

			EMaterialShadingModel MaterialShadingModel = Material->GetShadingModel();
			const EMaterialDomain Domain = (const EMaterialDomain)Material->GetMaterialDomain();

			if (Domain == MD_Surface
				&& IsSubsurfaceShadingModel(MaterialShadingModel))
			{
				// Note we don't test for the blend mode as you can have a translucent material using the subsurface shading model

				// another ForceCast as CompilePropertyAndSetMaterialProperty() can return MCT_Float which we don't want here
				int32 SubsurfaceColor = ForceCast(Material->CompilePropertyAndSetMaterialProperty(MP_SubsurfaceColor, this), MCT_Float3, true, true);

				static FName NameSubsurfaceProfile(TEXT("__SubsurfaceProfile"));

				// 1.0f is is a not used profile - later this gets replaced with the actual profile
				int32 CodeSubsurfaceProfile = ForceCast(ScalarParameter(NameSubsurfaceProfile, 1.0f), MCT_Float1);

				Chunk[MP_SubsurfaceColor] = AppendVector(SubsurfaceColor, CodeSubsurfaceProfile);		
			}

			Chunk[MP_CustomData0]					= Material->CompilePropertyAndSetMaterialProperty(MP_CustomData0		,this);
			Chunk[MP_CustomData1]					= Material->CompilePropertyAndSetMaterialProperty(MP_CustomData1		,this);
			Chunk[MP_AmbientOcclusion]				= Material->CompilePropertyAndSetMaterialProperty(MP_AmbientOcclusion	,this);

			if(IsTranslucentBlendMode(Material->GetBlendMode()))
			{
				int32 UserRefraction = ForceCast(Material->CompilePropertyAndSetMaterialProperty(MP_Refraction, this), MCT_Float1);
				int32 RefractionDepthBias = ForceCast(ScalarParameter(FName(TEXT("RefractionDepthBias")), Material->GetRefractionDepthBiasValue()), MCT_Float1);

				Chunk[MP_Refraction] = AppendVector(UserRefraction, RefractionDepthBias);
			}

			if (bCompileForComputeShader)
			{
				Chunk[CompiledMP_EmissiveColorCS]		= Material->CompilePropertyAndSetMaterialProperty(MP_EmissiveColor,this, SF_Compute);
			}

			if (Chunk[MP_WorldPositionOffset] != -1)
			{
				// Only calculate previous WPO if there is a current WPO
				Chunk[CompiledMP_PrevWorldPositionOffset] = Material->CompilePropertyAndSetMaterialProperty(MP_WorldPositionOffset, this, SF_Vertex, true);
			}

			Chunk[MP_PixelDepthOffset] = Material->CompilePropertyAndSetMaterialProperty(MP_PixelDepthOffset,this);

			// No more calls to non-vertex shader CompilePropertyAndSetMaterialProperty beyond this point
			const uint32 SavedNumUserTexCoords = NumUserTexCoords;

			for (uint32 CustomUVIndex = MP_CustomizedUVs0; CustomUVIndex <= MP_CustomizedUVs7; CustomUVIndex++)
			{
				// Only compile custom UV inputs for UV channels requested by the pixel shader inputs
				// Any unconnected inputs will have a texcoord generated for them in Material->CompileProperty, which will pass through the vertex (uncustomized) texture coordinates
				// Note: this is using NumUserTexCoords, which is set by translating all the pixel properties above
				if (CustomUVIndex - MP_CustomizedUVs0 < SavedNumUserTexCoords)
				{
					Chunk[CustomUVIndex] = Material->CompilePropertyAndSetMaterialProperty((EMaterialProperty)CustomUVIndex, this);
				}
			}

			bUsesPixelDepthOffset = IsMaterialPropertyUsed(MP_PixelDepthOffset, Chunk[MP_PixelDepthOffset], FLinearColor(0, 0, 0, 0), 1)
				|| (Domain == MD_DeferredDecal && Material->GetDecalBlendMode() == DBM_Volumetric_DistanceFunction);

			const bool bUsesWorldPositionOffset = IsMaterialPropertyUsed(MP_WorldPositionOffset, Chunk[MP_WorldPositionOffset], FLinearColor(0, 0, 0, 0), 3);
			MaterialCompilationOutput.bModifiesMeshPosition = bUsesPixelDepthOffset || bUsesWorldPositionOffset;
			MaterialCompilationOutput.bUsesWorldPositionOffset = bUsesWorldPositionOffset;
			MaterialCompilationOutput.bUsesPixelDepthOffset = bUsesPixelDepthOffset;
			
			if (Material->GetBlendMode() == BLEND_Modulate && MaterialShadingModel != MSM_Unlit && !Material->IsDeferredDecal())
			{
				Errorf(TEXT("Dynamically lit translucency is not supported for BLEND_Modulate materials."));
			}

			if (Domain == MD_Surface)
			{
				if (Material->GetBlendMode() == BLEND_Modulate && Material->IsSeparateTranslucencyEnabled())
				{
					Errorf(TEXT("Separate translucency with BLEND_Modulate is not supported. Consider using BLEND_Translucent with black emissive"));
				}
			}

			// Don't allow opaque and masked materials to scene depth as the results are undefined
			if (bUsesSceneDepth && Domain != MD_PostProcess && !IsTranslucentBlendMode(Material->GetBlendMode()))
			{
				Errorf(TEXT("Only transparent or postprocess materials can read from scene depth."));
			}

			if (MaterialCompilationOutput.bRequiresSceneColorCopy)
			{
				if (Domain != MD_Surface)
				{
					Errorf(TEXT("Only 'surface' material domain can use the scene color node."));
				}
				else if (!IsTranslucentBlendMode(Material->GetBlendMode()))
				{
					Errorf(TEXT("Only translucent materials can use the scene color node."));
				}
			}

			if (Material->IsLightFunction() && Material->GetBlendMode() != BLEND_Opaque)
			{
				Errorf(TEXT("Light function materials must be opaque."));
			}

			if (Material->IsLightFunction() && MaterialShadingModel != MSM_Unlit)
			{
				Errorf(TEXT("Light function materials must use unlit."));
			}

			if (Domain == MD_PostProcess && MaterialShadingModel != MSM_Unlit)
			{
				Errorf(TEXT("Post process materials must use unlit."));
			}

			if (Material->AllowNegativeEmissiveColor() && MaterialShadingModel != MSM_Unlit)
			{
				Errorf(TEXT("Only unlit materials can output negative emissive color."));
			}

			static IConsoleVariable* CDBufferVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DBuffer"));
			bool bDBufferAllowed = CDBufferVar ? CDBufferVar->GetInt() != 0 : false;
			bool bDBufferBlendMode = IsDBufferDecalBlendMode((EDecalBlendMode)Material->GetDecalBlendMode());

			if (bDBufferBlendMode && !bDBufferAllowed)
			{
				// Error feedback for when the decal would not be displayed due to project settings
				Errorf(TEXT("DBuffer decal blend modes are only supported when the 'DBuffer Decals' Rendering Project setting is enabled."));
			}

			if (Domain == MD_DeferredDecal && Material->GetBlendMode() != BLEND_Translucent)
			{
				// We could make the change for the user but it would be confusing when going to DeferredDecal and back
				// or we would have to pay a performance cost to make the change more transparently.
				// The change saves performance as with translucency we don't need to test for MeshDecals in all opaque rendering passes
				Errorf(TEXT("Material using the DeferredDecal domain need to use the BlendModel Translucent (this saves performance)"));
			}

			if (MaterialCompilationOutput.bNeedsSceneTextures)
			{
				if (Domain != MD_DeferredDecal && Domain != MD_PostProcess)
				{
					if (Material->GetBlendMode() == BLEND_Opaque || Material->GetBlendMode() == BLEND_Masked)
					{
						// In opaque pass, none of the textures are available
						Errorf(TEXT("SceneTexture expressions cannot be used in opaque materials"));
					}
					else if (bNeedsSceneTexturePostProcessInputs)
					{
						Errorf(TEXT("SceneTexture expressions cannot use post process inputs or scene color in non post process domain materials"));
					}
				}
			}

			// Catch any modifications to NumUserTexCoords that will not seen by customized UVs
			check(SavedNumUserTexCoords == NumUserTexCoords);

			ResourcesString = TEXT("");

			// Gather the implementation for any custom output expressions
			{
				TArray<UMaterialExpressionCustomOutput*> CustomOutputExpressions;
				Material->GatherCustomOutputExpressions(CustomOutputExpressions);
				TSet<UClass*> SeenCustomOutputExpressionsClases;

				for (UMaterialExpressionCustomOutput* CustomOutput : CustomOutputExpressions)
				{
					if (SeenCustomOutputExpressionsClases.Contains(CustomOutput->GetClass()))
					{
						Errorf(TEXT("The material can contain only one %s node"), *CustomOutput->GetDescription());
					}
					else
					{
						SeenCustomOutputExpressionsClases.Add(CustomOutput->GetClass());

						int32 NumOutputs = CustomOutput->GetNumOutputs();
						ResourcesString += FString::Printf(TEXT("#define NUM_MATERIAL_OUTPUTS_%s %d\r\n"), *CustomOutput->GetFunctionName().ToUpper(), NumOutputs);
						if (NumOutputs > 0)
						{
							for (int32 Index = 0; Index < NumOutputs; Index++)
							{
								{
									FunctionStacks[SF_Pixel].Empty();
									FunctionStacks[SF_Pixel].Add(FMaterialFunctionCompileState(nullptr));
								}
								MaterialProperty = MP_MAX; // Indicates we're not compiling any material property.
								ShaderFrequency = SF_Pixel;
								TArray<FShaderCodeChunk> CustomExpressionChunks;
								CurrentScopeChunks = &CustomExpressionChunks; //-V506
								CustomOutput->Compile(this, Index, 0);
							}
						}
					}
				}
			}

			// Output the implementation for any custom expressions we will call below.
			for(int32 ExpressionIndex = 0;ExpressionIndex < CustomExpressionImplementations.Num();ExpressionIndex++)
			{
				ResourcesString += CustomExpressionImplementations[ExpressionIndex] + "\r\n\r\n";
			}

			// Per frame expressions
			{
				for (int32 Index = 0, Num = MaterialCompilationOutput.UniformExpressionSet.PerFrameUniformScalarExpressions.Num(); Index < Num; ++Index)
				{
					ResourcesString += FString::Printf(TEXT("float UE_Material_PerFrameScalarExpression%u;"), Index) + "\r\n\r\n";
				}

				for (int32 Index = 0, Num = MaterialCompilationOutput.UniformExpressionSet.PerFrameUniformVectorExpressions.Num(); Index < Num; ++Index)
				{
					ResourcesString += FString::Printf(TEXT("float4 UE_Material_PerFrameVectorExpression%u;"), Index) + "\r\n\r\n";
				}

				for (int32 Index = 0, Num = MaterialCompilationOutput.UniformExpressionSet.PerFramePrevUniformScalarExpressions.Num(); Index < Num; ++Index)
				{
					ResourcesString += FString::Printf(TEXT("float UE_Material_PerFramePrevScalarExpression%u;"), Index) + "\r\n\r\n";
				}

				for (int32 Index = 0, Num = MaterialCompilationOutput.UniformExpressionSet.PerFramePrevUniformVectorExpressions.Num(); Index < Num; ++Index)
				{
					ResourcesString += FString::Printf(TEXT("float4 UE_Material_PerFramePrevVectorExpression%u;"), Index) + "\r\n\r\n";
				}
			}

			// Do Normal Chunk first
			{
				GetFixedParameterCode(
					0,
					NormalCodeChunkEnd,
					Chunk[MP_Normal],
					SharedPropertyCodeChunks[NormalShaderFrequency],
					TranslatedCodeChunkDefinitions[MP_Normal],
					TranslatedCodeChunks[MP_Normal]);

				// Always gather MP_Normal definitions as they can be shared by other properties
				if (TranslatedCodeChunkDefinitions[MP_Normal].IsEmpty())
				{
					TranslatedCodeChunkDefinitions[MP_Normal] = GetDefinitions(SharedPropertyCodeChunks[NormalShaderFrequency], 0, NormalCodeChunkEnd);
				}
			}

			// Now the rest, skipping Normal
			for(uint32 PropertyId = 0; PropertyId < MP_MAX; ++PropertyId)
			{
				if (PropertyId == MP_MaterialAttributes || PropertyId == MP_Normal)
				{
					continue;
				}

				const EShaderFrequency PropertyShaderFrequency = GetMaterialPropertyShaderFrequency((EMaterialProperty)PropertyId);

				int32 StartChunk = 0;
				if (PropertyShaderFrequency == NormalShaderFrequency && SharedPixelProperties[PropertyId])
				{
					// When processing shared properties, do not generate the code before the Normal was generated as those are already handled
					StartChunk = NormalCodeChunkEnd;
				}

				GetFixedParameterCode(
					StartChunk,
					SharedPropertyCodeChunks[PropertyShaderFrequency].Num(),
					Chunk[PropertyId],
					SharedPropertyCodeChunks[PropertyShaderFrequency],
					TranslatedCodeChunkDefinitions[PropertyId],
					TranslatedCodeChunks[PropertyId]);
			}

			for(uint32 PropertyId = MP_MAX; PropertyId < CompiledMP_MAX; ++PropertyId)
			{
				switch(PropertyId)
				{
				case CompiledMP_EmissiveColorCS:
			    	if (bCompileForComputeShader)
				    {
						{
							GetFixedParameterCode(Chunk[PropertyId], SharedPropertyCodeChunks[SF_Compute], TranslatedCodeChunkDefinitions[PropertyId], TranslatedCodeChunks[PropertyId]);
						}
				    }
					break;
				case CompiledMP_PrevWorldPositionOffset:
					{
						GetFixedParameterCode(Chunk[PropertyId], SharedPropertyCodeChunks[SF_Vertex], TranslatedCodeChunkDefinitions[PropertyId], TranslatedCodeChunks[PropertyId]);
					}
					break;
				default: check(0);
					break;
				}
			}

			// Output the implementation for any custom output expressions
			for (int32 ExpressionIndex = 0; ExpressionIndex < CustomOutputImplementations.Num(); ExpressionIndex++)
			{
				ResourcesString += CustomOutputImplementations[ExpressionIndex] + "\r\n\r\n";
			}

			LoadShaderSourceFileChecked(TEXT("MaterialTemplate"), MaterialTemplate);

			// Find the string index of the '#line' statement in MaterialTemplate.usf
			const int32 LineIndex = MaterialTemplate.Find(TEXT("#line"), ESearchCase::CaseSensitive);
			check(LineIndex != INDEX_NONE);

			// Count line endings before the '#line' statement
			MaterialTemplateLineNumber = INDEX_NONE;
			int32 StartPosition = LineIndex + 1;
			do 
			{
				MaterialTemplateLineNumber++;
				// Using \n instead of LINE_TERMINATOR as not all of the lines are terminated consistently
				// Subtract one from the last found line ending index to make sure we skip over it
				StartPosition = MaterialTemplate.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, StartPosition - 1);
			} 
			while (StartPosition != INDEX_NONE);
			check(MaterialTemplateLineNumber != INDEX_NONE);
			// At this point MaterialTemplateLineNumber is one less than the line number of the '#line' statement
			// For some reason we have to add 2 more to the #line value to get correct error line numbers from D3DXCompileShader
			MaterialTemplateLineNumber += 3;

			MaterialCompilationOutput.UniformExpressionSet.SetParameterCollections(ParameterCollections);

			// Create the material uniform buffer struct.
			MaterialCompilationOutput.UniformExpressionSet.CreateBufferStruct();
		}
		INC_FLOAT_STAT_BY(STAT_ShaderCompiling_HLSLTranslation,(float)HLSLTranslateTime);

		return bSuccess;
	}

	void GetMaterialEnvironment(EShaderPlatform InPlatform, FShaderCompilerEnvironment& OutEnvironment)
	{
		if (bNeedsParticlePosition || Material->ShouldGenerateSphericalParticleNormals() || bUsesSphericalParticleOpacity)
		{
			OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_POSITION"), 1);
		}

		if (bNeedsParticleVelocity)
		{
			OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_VELOCITY"), 1);
		}

		if (bNeedsParticleDynamicParameter)
		{
			OutEnvironment.SetDefine(TEXT("USE_DYNAMIC_PARAMETERS"), 1);
		}

		if (bNeedsParticleTime)
		{
			OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_TIME"), 1);
		}

		if (bUsesParticleMotionBlur)
		{
			OutEnvironment.SetDefine(TEXT("USES_PARTICLE_MOTION_BLUR"), 1);
		}

		if (bNeedsParticleRandom)
		{
			OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_RANDOM"), 1);
		}

		if (bUsesSphericalParticleOpacity)
		{
			OutEnvironment.SetDefine(TEXT("SPHERICAL_PARTICLE_OPACITY"), TEXT("1"));
		}

		if (bUsesParticleSubUVs)
		{
			OutEnvironment.SetDefine(TEXT("USE_PARTICLE_SUBUVS"), TEXT("1"));
		}

		if (bUsesLightmapUVs)
		{
			OutEnvironment.SetDefine(TEXT("LIGHTMAP_UV_ACCESS"),TEXT("1"));
		}

		if (bUsesAOMaterialMask)
		{
			OutEnvironment.SetDefine(TEXT("USES_AO_MATERIAL_MASK"),TEXT("1"));
		}

		if (bUsesSpeedTree)
		{
			OutEnvironment.SetDefine(TEXT("USES_SPEEDTREE"),TEXT("1"));
		}

		if (bNeedsWorldPositionExcludingShaderOffsets)
		{
			OutEnvironment.SetDefine(TEXT("NEEDS_WORLD_POSITION_EXCLUDING_SHADER_OFFSETS"), TEXT("1"));
		}

		if( bNeedsParticleSize )
		{
			OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_SIZE"), TEXT("1"));
		}

		if( MaterialCompilationOutput.bNeedsSceneTextures )
		{
			OutEnvironment.SetDefine(TEXT("NEEDS_SCENE_TEXTURES"), TEXT("1"));
		}
		if( MaterialCompilationOutput.bUsesEyeAdaptation )
		{
			OutEnvironment.SetDefine(TEXT("USES_EYE_ADAPTATION"), TEXT("1"));
		}
		OutEnvironment.SetDefine(TEXT("MATERIAL_ATMOSPHERIC_FOG"), bUsesAtmosphericFog);
		OutEnvironment.SetDefine(TEXT("INTERPOLATE_VERTEX_COLOR"), bUsesVertexColor); 
		OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_COLOR"), bUsesParticleColor); 
		OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_TRANSFORM"), bUsesParticleTransform);
		OutEnvironment.SetDefine(TEXT("USES_TRANSFORM_VECTOR"), bUsesTransformVector);
		OutEnvironment.SetDefine(TEXT("WANT_PIXEL_DEPTH_OFFSET"), bUsesPixelDepthOffset); 
		// Distortion uses tangent space transform 
		OutEnvironment.SetDefine(TEXT("USES_DISTORTION"), Material->IsDistorted()); 

		OutEnvironment.SetDefine(TEXT("ENABLE_TRANSLUCENCY_VERTEX_FOG"), Material->UseTranslucencyVertexFog());

		for (int32 CollectionIndex = 0; CollectionIndex < ParameterCollections.Num(); CollectionIndex++)
		{
			// Add uniform buffer declarations for any parameter collections referenced
			const FString CollectionName = FString::Printf(TEXT("MaterialCollection%u"), CollectionIndex);
			FShaderUniformBufferParameter::ModifyCompilationEnvironment(*CollectionName, ParameterCollections[CollectionIndex]->GetUniformBufferStruct(), InPlatform, OutEnvironment);
		}
		OutEnvironment.SetDefine(TEXT("IS_MATERIAL_SHADER"), TEXT("1"));
	}

	void GetSharedInputsMaterialCode(FString& PixelMembersDeclaration, FString& NormalAssignment, FString& PixelMembersInitializationEpilog)
	{
		{
			int32 LastProperty = -1;

			FString PixelInputInitializerValues;
			FString NormalInitializerValue;

			for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
			{
				// Skip non-shared properties
				if (!SharedPixelProperties[PropertyIndex])
				{
					continue;
				}

				const EMaterialProperty Property = (EMaterialProperty)PropertyIndex;
				check(GetMaterialPropertyShaderFrequency(Property) == SF_Pixel);
				const FString PropertyName = GetNameOfMaterialProperty(Property);
				check(PropertyName.Len() > 0);
				const EMaterialValueType Type = GetMaterialPropertyType(Property);

				// Normal requires its own separate initializer
				if (Property == MP_Normal)
				{
					NormalInitializerValue = FString::Printf(TEXT("\tPixelMaterialInputs.%s = %s;\n"), *PropertyName, *TranslatedCodeChunks[Property]);
				}
				else
				{
					if (TranslatedCodeChunkDefinitions[Property].Len() > 0)
					{
						if (LastProperty >= 0)
						{
							// Verify that all code chunks have the same contents
							check(TranslatedCodeChunkDefinitions[Property].Len() == TranslatedCodeChunkDefinitions[LastProperty].Len());
						}

						LastProperty = Property;
					}

					PixelInputInitializerValues += FString::Printf(TEXT("\tPixelMaterialInputs.%s = %s;\n"), *PropertyName, *TranslatedCodeChunks[Property]);
				}

				PixelMembersDeclaration += FString::Printf(TEXT("\t%s %s;\n"), HLSLTypeString(Type), *PropertyName);
			}

			NormalAssignment = NormalInitializerValue;
			if (LastProperty != -1)
			{
				PixelMembersInitializationEpilog += TranslatedCodeChunkDefinitions[LastProperty] + TEXT("\n");
			}

			PixelMembersInitializationEpilog += PixelInputInitializerValues;
		}
	}

	FString GetMaterialShaderCode()
	{	
		// use "MaterialTemplate.usf" to create the functions to get data (e.g. material attributes) and code (e.g. material expressions to create specular color) from C++
		FLazyPrintf LazyPrintf(*MaterialTemplate);

		LazyPrintf.PushParam(*FString::Printf(TEXT("%u"),NumUserVertexTexCoords));
		LazyPrintf.PushParam(*FString::Printf(TEXT("%u"),NumUserTexCoords));

		// Stores the shared shader results member declarations
		FString PixelMembersDeclaration;

		FString NormalAssignment;

		// Stores the code to initialize all inputs after MP_Normal
		FString PixelMembersSetupAndAssignments;

		GetSharedInputsMaterialCode(PixelMembersDeclaration, NormalAssignment, PixelMembersSetupAndAssignments);

		LazyPrintf.PushParam(*PixelMembersDeclaration);

		LazyPrintf.PushParam(*ResourcesString);

		if (bCompileForComputeShader)
		{
			LazyPrintf.PushParam(*GenerateFunctionCode(CompiledMP_EmissiveColorCS));
		}
		else
		{
			LazyPrintf.PushParam(TEXT("return 0"));
		}

		LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"),Material->GetTranslucencyDirectionalLightingIntensity()));
		
		LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"),Material->GetTranslucentShadowDensityScale()));
		LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"),Material->GetTranslucentSelfShadowDensityScale()));
		LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"),Material->GetTranslucentSelfShadowSecondDensityScale()));
		LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"),Material->GetTranslucentSelfShadowSecondOpacity()));
		LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"),Material->GetTranslucentBackscatteringExponent()));

		{
			FLinearColor Extinction = Material->GetTranslucentMultipleScatteringExtinction();

			LazyPrintf.PushParam(*FString::Printf(TEXT("return MaterialFloat3(%.5f, %.5f, %.5f)"), Extinction.R, Extinction.G, Extinction.B));
		}

		LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"),Material->GetOpacityMaskClipValue()));

		LazyPrintf.PushParam(*GenerateFunctionCode(MP_WorldPositionOffset));
		LazyPrintf.PushParam(*GenerateFunctionCode(CompiledMP_PrevWorldPositionOffset));
		LazyPrintf.PushParam(*GenerateFunctionCode(MP_WorldDisplacement));
		LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"),Material->GetMaxDisplacement()));
		LazyPrintf.PushParam(*GenerateFunctionCode(MP_TessellationMultiplier));
		LazyPrintf.PushParam(*GenerateFunctionCode(MP_SubsurfaceColor));
		LazyPrintf.PushParam(*GenerateFunctionCode(MP_CustomData0));
		LazyPrintf.PushParam(*GenerateFunctionCode(MP_CustomData1));

		FString CustomUVAssignments;

			int32 LastProperty = -1;
			for (uint32 CustomUVIndex = 0; CustomUVIndex < NumUserTexCoords; CustomUVIndex++)
			{
				if (CustomUVIndex == 0)
				{
					CustomUVAssignments += TranslatedCodeChunkDefinitions[MP_CustomizedUVs0 + CustomUVIndex];
				}

				if (TranslatedCodeChunkDefinitions[MP_CustomizedUVs0 + CustomUVIndex].Len() > 0)
				{
					if (LastProperty >= 0)
					{
						check(TranslatedCodeChunkDefinitions[LastProperty].Len() == TranslatedCodeChunkDefinitions[MP_CustomizedUVs0 + CustomUVIndex].Len());
					}
					LastProperty = MP_CustomizedUVs0 + CustomUVIndex;
				}
				CustomUVAssignments += FString::Printf(TEXT("\tOutTexCoords[%u] = %s;") LINE_TERMINATOR, CustomUVIndex, *TranslatedCodeChunks[MP_CustomizedUVs0 + CustomUVIndex]);
			}

		LazyPrintf.PushParam(*CustomUVAssignments);

		// Initializers required for Normal
		LazyPrintf.PushParam(*TranslatedCodeChunkDefinitions[MP_Normal]);
		LazyPrintf.PushParam(*NormalAssignment);
		// Finally the rest of common code followed by assignment into each input
		LazyPrintf.PushParam(*PixelMembersSetupAndAssignments);

		LazyPrintf.PushParam(*FString::Printf(TEXT("%u"),MaterialTemplateLineNumber));

		return LazyPrintf.GetResultString();
	}

protected:

	bool IsMaterialPropertyUsed(EMaterialProperty Property, int32 PropertyChunkIndex, const FLinearColor& ReferenceValue, int32 NumComponents)
	{
		bool bPropertyUsed = false;

		if (PropertyChunkIndex == -1)
		{
			bPropertyUsed = false;
		}
		else
		{
			int32 Frequency = (int32)GetMaterialPropertyShaderFrequency(Property);
			FShaderCodeChunk& PropertyChunk = SharedPropertyCodeChunks[Frequency][PropertyChunkIndex];

			// Determine whether the property is used. 
			// If the output chunk has a uniform expression, it is constant, and GetNumberValue returns the default property value then property isn't used.
			bPropertyUsed = true;

			if( PropertyChunk.UniformExpression && PropertyChunk.UniformExpression->IsConstant() )
			{
				FLinearColor Value;
				FMaterialRenderContext DummyContext(nullptr, *Material, nullptr);
				PropertyChunk.UniformExpression->GetNumberValue(DummyContext, Value);

				if ((NumComponents < 1 || Value.R == ReferenceValue.R)
					&& (NumComponents < 2 || Value.G == ReferenceValue.G)
					&& (NumComponents < 3 || Value.B == ReferenceValue.B)
					&& (NumComponents < 4 || Value.A == ReferenceValue.A))
				{
					bPropertyUsed = false;
				}
			}
		}

		return bPropertyUsed;
	}

	// only used by GetMaterialShaderCode()
	// @param Index ECompiledMaterialProperty or EMaterialProperty
	FString GenerateFunctionCode(uint32 Index) const
	{
		check(Index < CompiledMP_MAX);
		return TranslatedCodeChunkDefinitions[Index] + TEXT("	return ") + TranslatedCodeChunks[Index] + TEXT(";");
	}

	// GetParameterCode
	virtual FString GetParameterCode(int32 Index, const TCHAR* Default = 0)
	{
		if(Index == INDEX_NONE && Default)
		{
			return Default;
		}

		checkf(Index >= 0 && Index < CurrentScopeChunks->Num(), TEXT("Index %d/%d, Platform=%d"), Index, CurrentScopeChunks->Num(), (int)Platform);
		const FShaderCodeChunk& CodeChunk = (*CurrentScopeChunks)[Index];
		if((CodeChunk.UniformExpression && CodeChunk.UniformExpression->IsConstant()) || CodeChunk.bInline)
		{
			// Constant uniform expressions and code chunks which are marked to be inlined are accessed via Definition
			return CodeChunk.Definition;
		}
		else
		{
			if (CodeChunk.UniformExpression)
			{
				// If the code chunk has a uniform expression, create a new code chunk to access it
				const int32 AccessedIndex = AccessUniformExpression(Index);
				const FShaderCodeChunk& AccessedCodeChunk = (*CurrentScopeChunks)[AccessedIndex];
				if(AccessedCodeChunk.bInline)
				{
					// Handle the accessed code chunk being inlined
					return AccessedCodeChunk.Definition;
				}
				// Return the symbol used to reference this code chunk
				check(AccessedCodeChunk.SymbolName.Len() > 0);
				return AccessedCodeChunk.SymbolName;
			}
			
			// Return the symbol used to reference this code chunk
			check(CodeChunk.SymbolName.Len() > 0);
			return CodeChunk.SymbolName;
		}
	}

	/** Creates a string of all definitions needed for the given material input. */
	FString GetDefinitions(TArray<FShaderCodeChunk>& CodeChunks, int32 StartChunk, int32 EndChunk) const
	{
		FString Definitions;
		for (int32 ChunkIndex = StartChunk; ChunkIndex < EndChunk; ChunkIndex++)
		{
			const FShaderCodeChunk& CodeChunk = CodeChunks[ChunkIndex];
			// Uniform expressions (both constant and variable) and inline expressions don't have definitions.
			if (!CodeChunk.UniformExpression && !CodeChunk.bInline)
			{
				Definitions += CodeChunk.Definition;
			}
		}
		return Definitions;
	}

	// GetFixedParameterCode
	void GetFixedParameterCode(int32 StartChunk, int32 EndChunk, int32 ResultIndex, TArray<FShaderCodeChunk>& CodeChunks, FString& OutDefinitions, FString& OutValue)
	{
		if (ResultIndex != INDEX_NONE)
		{
			checkf(ResultIndex >= 0 && ResultIndex < CodeChunks.Num(), TEXT("Index out of range %d/%d [%s]"), ResultIndex, CodeChunks.Num(), *Material->GetFriendlyName());
			check(!CodeChunks[ResultIndex].UniformExpression || CodeChunks[ResultIndex].UniformExpression->IsConstant());
			if (CodeChunks[ResultIndex].UniformExpression && CodeChunks[ResultIndex].UniformExpression->IsConstant())
			{
				// Handle a constant uniform expression being the only code chunk hooked up to a material input
				const FShaderCodeChunk& ResultChunk = CodeChunks[ResultIndex];
				OutValue = ResultChunk.Definition;
			}
			else
			{
				const FShaderCodeChunk& ResultChunk = CodeChunks[ResultIndex];
				// Combine the definition lines and the return statement
				check(ResultChunk.bInline || ResultChunk.SymbolName.Len() > 0);
				OutDefinitions = GetDefinitions(CodeChunks, StartChunk, EndChunk);
				OutValue = ResultChunk.bInline ? ResultChunk.Definition : ResultChunk.SymbolName;
			}
		}
		else
		{
			OutValue = TEXT("0");
		}
	}

	void GetFixedParameterCode(int32 ResultIndex, TArray<FShaderCodeChunk>& CodeChunks, FString& OutDefinitions, FString& OutValue)
	{
		GetFixedParameterCode(0, CodeChunks.Num(), ResultIndex, CodeChunks, OutDefinitions, OutValue);
	}

	/** Used to get a user friendly type from EMaterialValueType */
	const TCHAR* DescribeType(EMaterialValueType Type) const
	{
		switch(Type)
		{
		case MCT_Float1:		return TEXT("float");
		case MCT_Float2:		return TEXT("float2");
		case MCT_Float3:		return TEXT("float3");
		case MCT_Float4:		return TEXT("float4");
		case MCT_Float:			return TEXT("float");
		case MCT_Texture2D:		return TEXT("texture2D");
		case MCT_TextureCube:	return TEXT("textureCube");
		case MCT_StaticBool:	return TEXT("static bool");
		case MCT_MaterialAttributes:	return TEXT("MaterialAttributes");
		default:				return TEXT("unknown");
		};
	}

	/** Used to get an HLSL type from EMaterialValueType */
	const TCHAR* HLSLTypeString(EMaterialValueType Type) const
	{
		switch(Type)
		{
		case MCT_Float1:		return TEXT("MaterialFloat");
		case MCT_Float2:		return TEXT("MaterialFloat2");
		case MCT_Float3:		return TEXT("MaterialFloat3");
		case MCT_Float4:		return TEXT("MaterialFloat4");
		case MCT_Float:			return TEXT("MaterialFloat");
		case MCT_Texture2D:		return TEXT("texture2D");
		case MCT_TextureCube:	return TEXT("textureCube");
		case MCT_StaticBool:	return TEXT("static bool");
		case MCT_MaterialAttributes:	return TEXT("MaterialAttributes");
		default:				return TEXT("unknown");
		};
	}

	int32 NonPixelShaderExpressionError()
	{
		return Errorf(TEXT("Invalid node used in vertex/hull/domain shader input!"));
	}

	int32 ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::Type RequiredFeatureLevel)
	{
		if (FeatureLevel < RequiredFeatureLevel)
		{
			FString FeatureLevelName;
			GetFeatureLevelName(FeatureLevel, FeatureLevelName);
			return Errorf(TEXT("Node not supported in feature level %s"), *FeatureLevelName);
		}

		return 0;
	}

	int32 NonVertexShaderExpressionError()
	{
		return Errorf(TEXT("Invalid node used in pixel/hull/domain shader input!"));
	}

	int32 NonVertexOrPixelShaderExpressionError()
	{
		return Errorf(TEXT("Invalid node used in hull/domain shader input!"));
	}

	/** Creates a unique symbol name and adds it to the symbol list. */
	FString CreateSymbolName(const TCHAR* SymbolNameHint)
	{
		NextSymbolIndex++;
		return FString(SymbolNameHint) + FString::FromInt(NextSymbolIndex);
	}

	/** Adds an already formatted inline or referenced code chunk */
	int32 AddCodeChunkInner(const TCHAR* FormattedCode,EMaterialValueType Type,bool bInlined)
	{
		if (Type == MCT_Unknown)
		{
			return INDEX_NONE;
		}

		if (bInlined)
		{
			const int32 CodeIndex = CurrentScopeChunks->Num();
			// Adding an inline code chunk, the definition will be the code to inline
			new(*CurrentScopeChunks) FShaderCodeChunk(FormattedCode,TEXT(""),Type,true);
			return CodeIndex;
		}
		// Can only create temporaries for float and material attribute types.
		else if (Type & (MCT_Float))
		{
			const int32 CodeIndex = CurrentScopeChunks->Num();
			// Allocate a local variable name
			const FString SymbolName = CreateSymbolName(TEXT("Local"));
			// Construct the definition string which stores the result in a temporary and adds a newline for readability
			const FString LocalVariableDefinition = FString("	") + HLSLTypeString(Type) + TEXT(" ") + SymbolName + TEXT(" = ") + FormattedCode + TEXT(";") + LINE_TERMINATOR;
			// Adding a code chunk that creates a local variable
			new(*CurrentScopeChunks) FShaderCodeChunk(*LocalVariableDefinition,SymbolName,Type,false);
			return CodeIndex;
		}
		else
		{
			if (Type == MCT_MaterialAttributes)
			{
				return Errorf(TEXT("Operation not supported on Material Attributes"));
			}

			if (Type & MCT_Texture)
			{
				return Errorf(TEXT("Operation not supported on a Texture"));
			}

			if (Type == MCT_StaticBool)
			{
				return Errorf(TEXT("Operation not supported on a Static Bool"));
			}
			
			return INDEX_NONE;
		}
	}

	/** 
	 * Constructs the formatted code chunk and creates a new local variable definition from it. 
	 * This should be used over AddInlinedCodeChunk when the code chunk adds actual instructions, and especially when calling a function.
	 * Creating local variables instead of inlining simplifies the generated code and reduces redundant expression chains,
	 * Making compiles faster and enabling the shader optimizer to do a better job.
	 */
	int32 AddCodeChunk(EMaterialValueType Type,const TCHAR* Format,...)
	{
		int32	BufferSize		= 256;
		TCHAR*	FormattedCode	= NULL;
		int32	Result			= -1;

		while(Result == -1)
		{
			FormattedCode = (TCHAR*) FMemory::Realloc( FormattedCode, BufferSize * sizeof(TCHAR) );
			GET_VARARGS_RESULT( FormattedCode, BufferSize, BufferSize-1, Format, Format, Result );
			BufferSize *= 2;
		};
		FormattedCode[Result] = 0;

		const int32 CodeIndex = AddCodeChunkInner(FormattedCode,Type,false);
		FMemory::Free(FormattedCode);

		return CodeIndex;
	}

	/** 
	 * Constructs the formatted code chunk and creates an inlined code chunk from it. 
	 * This should be used instead of AddCodeChunk when the code chunk does not add any actual shader instructions, for example a component mask.
	 */
	int32 AddInlinedCodeChunk(EMaterialValueType Type,const TCHAR* Format,...)
	{
		int32	BufferSize		= 256;
		TCHAR*	FormattedCode	= NULL;
		int32	Result			= -1;

		while(Result == -1)
		{
			FormattedCode = (TCHAR*) FMemory::Realloc( FormattedCode, BufferSize * sizeof(TCHAR) );
			GET_VARARGS_RESULT( FormattedCode, BufferSize, BufferSize-1, Format, Format, Result );
			BufferSize *= 2;
		};
		FormattedCode[Result] = 0;
		const int32 CodeIndex = AddCodeChunkInner(FormattedCode,Type,true);
		FMemory::Free(FormattedCode);

		return CodeIndex;
	}

	// AddUniformExpression - Adds an input to the Code array and returns its index.
	int32 AddUniformExpression(FMaterialUniformExpression* UniformExpression,EMaterialValueType Type,const TCHAR* Format,...)
	{
		if (Type == MCT_Unknown)
		{
			return INDEX_NONE;
		}

		check(UniformExpression);

		// Only a texture uniform expression can have MCT_Texture type
		if ((Type & MCT_Texture) && !UniformExpression->GetTextureUniformExpression())
		{
			return Errorf(TEXT("Operation not supported on a Texture"));
		}

		if (Type == MCT_StaticBool)
		{
			return Errorf(TEXT("Operation not supported on a Static Bool"));
		}
		
		if (Type == MCT_MaterialAttributes)
		{
			return Errorf(TEXT("Operation not supported on a MaterialAttributes"));
		}

		bool bFoundExistingExpression = false;
		// Search for an existing code chunk with the same uniform expression in the array of all uniform expressions used by this material.
		for (int32 ExpressionIndex = 0; ExpressionIndex < UniformExpressions.Num() && !bFoundExistingExpression; ExpressionIndex++)
		{
			FMaterialUniformExpression* TestExpression = UniformExpressions[ExpressionIndex].UniformExpression;
			check(TestExpression);
			if(TestExpression->IsIdentical(UniformExpression))
			{
				bFoundExistingExpression = true;
				// This code chunk has an identical uniform expression to the new expression, reuse it.
				// This allows multiple material properties to share uniform expressions because AccessUniformExpression uses AddUniqueItem when adding uniform expressions.
				check(Type == UniformExpressions[ExpressionIndex].Type);
				// Search for an existing code chunk with the same uniform expression in the array of code chunks for this material property.
				for(int32 ChunkIndex = 0;ChunkIndex < CurrentScopeChunks->Num();ChunkIndex++)
				{
					FMaterialUniformExpression* OtherExpression = (*CurrentScopeChunks)[ChunkIndex].UniformExpression;
					if(OtherExpression && OtherExpression->IsIdentical(UniformExpression))
					{
						delete UniformExpression;
						// Reuse the entry in CurrentScopeChunks
						return ChunkIndex;
					}
				}
				delete UniformExpression;
				// Use the existing uniform expression from a different material property,
				// And continue so that a code chunk using the uniform expression will be generated for this material property.
				UniformExpression = TestExpression;
				break;
			}
		}

		int32	BufferSize		= 256;
		TCHAR*	FormattedCode	= NULL;
		int32	Result			= -1;

		while(Result == -1)
		{
			FormattedCode = (TCHAR*) FMemory::Realloc( FormattedCode, BufferSize * sizeof(TCHAR) );
			GET_VARARGS_RESULT( FormattedCode, BufferSize, BufferSize-1, Format, Format, Result );
			BufferSize *= 2;
		};
		FormattedCode[Result] = 0;

		const int32 ReturnIndex = CurrentScopeChunks->Num();
		// Create a new code chunk for the uniform expression
		new(*CurrentScopeChunks) FShaderCodeChunk(UniformExpression,FormattedCode,Type);

		if (!bFoundExistingExpression)
		{
			// Add an entry to the material-wide list of uniform expressions
			new(UniformExpressions) FShaderCodeChunk(UniformExpression,FormattedCode,Type);
		}

		FMemory::Free(FormattedCode);
		return ReturnIndex;
	}

	// AccessUniformExpression - Adds code to access the value of a uniform expression to the Code array and returns its index.
	int32 AccessUniformExpression(int32 Index)
	{
		check(Index >= 0 && Index < CurrentScopeChunks->Num());
		const FShaderCodeChunk&	CodeChunk = (*CurrentScopeChunks)[Index];
		check(CodeChunk.UniformExpression && !CodeChunk.UniformExpression->IsConstant());

		FMaterialUniformExpressionTexture* TextureUniformExpression = CodeChunk.UniformExpression->GetTextureUniformExpression();
		// Any code chunk can have a texture uniform expression (eg FMaterialUniformExpressionFlipBookTextureParameter),
		// But a texture code chunk must have a texture uniform expression
		check(!(CodeChunk.Type & MCT_Texture) || TextureUniformExpression);

		TCHAR FormattedCode[MAX_SPRINTF]=TEXT("");
		if(CodeChunk.Type == MCT_Float)
		{
			if (CodeChunk.UniformExpression->IsChangingPerFrame())
			{
				if (bCompilingPreviousFrame)
				{
					const int32 ScalarInputIndex = MaterialCompilationOutput.UniformExpressionSet.PerFramePrevUniformScalarExpressions.AddUnique(CodeChunk.UniformExpression);
					FCString::Sprintf(FormattedCode, TEXT("UE_Material_PerFramePrevScalarExpression%u"), ScalarInputIndex);
				}
				else
				{
				const int32 ScalarInputIndex = MaterialCompilationOutput.UniformExpressionSet.PerFrameUniformScalarExpressions.AddUnique(CodeChunk.UniformExpression);
				FCString::Sprintf(FormattedCode, TEXT("UE_Material_PerFrameScalarExpression%u"), ScalarInputIndex);
			}
			}
			else
			{
				const static TCHAR IndexToMask[] = {'x', 'y', 'z', 'w'};
				const int32 ScalarInputIndex = MaterialCompilationOutput.UniformExpressionSet.UniformScalarExpressions.AddUnique(CodeChunk.UniformExpression);
				// Update the above FMemory::Malloc if this FCString::Sprintf grows in size, e.g. %s, ...
				FCString::Sprintf(FormattedCode, TEXT("Material.ScalarExpressions[%u].%c"), ScalarInputIndex / 4, IndexToMask[ScalarInputIndex % 4]);
			}
		}
		else if(CodeChunk.Type & MCT_Float)
		{
			const TCHAR* Mask;
			switch(CodeChunk.Type)
			{
			case MCT_Float:
			case MCT_Float1: Mask = TEXT(".r"); break;
			case MCT_Float2: Mask = TEXT(".rg"); break;
			case MCT_Float3: Mask = TEXT(".rgb"); break;
			default: Mask = TEXT(""); break;
			};

			if (CodeChunk.UniformExpression->IsChangingPerFrame())
			{
				if (bCompilingPreviousFrame)
				{
					const int32 VectorInputIndex = MaterialCompilationOutput.UniformExpressionSet.PerFramePrevUniformVectorExpressions.AddUnique(CodeChunk.UniformExpression);
					FCString::Sprintf(FormattedCode, TEXT("UE_Material_PerFramePrevVectorExpression%u%s"), VectorInputIndex, Mask);
				}
				else
				{
				const int32 VectorInputIndex = MaterialCompilationOutput.UniformExpressionSet.PerFrameUniformVectorExpressions.AddUnique(CodeChunk.UniformExpression);
				FCString::Sprintf(FormattedCode, TEXT("UE_Material_PerFrameVectorExpression%u%s"), VectorInputIndex, Mask);
			}
			}
			else
			{
				const int32 VectorInputIndex = MaterialCompilationOutput.UniformExpressionSet.UniformVectorExpressions.AddUnique(CodeChunk.UniformExpression);
				FCString::Sprintf(FormattedCode, TEXT("Material.VectorExpressions[%u]%s"), VectorInputIndex, Mask);
			}
		}
		else if(CodeChunk.Type & MCT_Texture)
		{
			check(!CodeChunk.UniformExpression->IsChangingPerFrame());
			int32 TextureInputIndex = INDEX_NONE;
			const TCHAR* BaseName = TEXT("");
			switch(CodeChunk.Type)
			{
			case MCT_Texture2D:
				TextureInputIndex = MaterialCompilationOutput.UniformExpressionSet.Uniform2DTextureExpressions.AddUnique(TextureUniformExpression);
				BaseName = TEXT("Texture2D");
				break;
			case MCT_TextureCube:
				TextureInputIndex = MaterialCompilationOutput.UniformExpressionSet.UniformCubeTextureExpressions.AddUnique(TextureUniformExpression);
				BaseName = TEXT("TextureCube");
				break;
			default: UE_LOG(LogMaterial, Fatal,TEXT("Unrecognized texture material value type: %u"),(int32)CodeChunk.Type);
			};
			FCString::Sprintf(FormattedCode, TEXT("Material.%s_%u"), BaseName, TextureInputIndex);
		}
		else
		{
			UE_LOG(LogMaterial, Fatal,TEXT("User input of unknown type: %s"),DescribeType(CodeChunk.Type));
		}

		return AddInlinedCodeChunk((*CurrentScopeChunks)[Index].Type,FormattedCode);
	}

	// CoerceParameter
	FString CoerceParameter(int32 Index,EMaterialValueType DestType)
	{
		check(Index >= 0 && Index < CurrentScopeChunks->Num());
		const FShaderCodeChunk&	CodeChunk = (*CurrentScopeChunks)[Index];
		if( CodeChunk.Type == DestType )
		{
			return GetParameterCode(Index);
		}
		else
		{
			if( (CodeChunk.Type & DestType) && (CodeChunk.Type & MCT_Float) )
			{
				switch( DestType )
				{
				case MCT_Float1:
					return FString::Printf( TEXT("MaterialFloat(%s)"), *GetParameterCode(Index) );
				case MCT_Float2:
					return FString::Printf( TEXT("MaterialFloat2(%s,%s)"), *GetParameterCode(Index), *GetParameterCode(Index) );
				case MCT_Float3:
					return FString::Printf( TEXT("MaterialFloat3(%s,%s,%s)"), *GetParameterCode(Index), *GetParameterCode(Index), *GetParameterCode(Index) );
				case MCT_Float4:
					return FString::Printf( TEXT("MaterialFloat4(%s,%s,%s,%s)"), *GetParameterCode(Index), *GetParameterCode(Index), *GetParameterCode(Index), *GetParameterCode(Index) );
				default: 
					return FString::Printf( TEXT("%s"), *GetParameterCode(Index) );
				}
			}
			else
			{
				Errorf(TEXT("Coercion failed: %s: %s -> %s"),*CodeChunk.Definition,DescribeType(CodeChunk.Type),DescribeType(DestType));
				return TEXT("");
			}
		}
	}

	// GetParameterType
	EMaterialValueType GetParameterType(int32 Index) const
	{
		check(Index >= 0 && Index < CurrentScopeChunks->Num());
		return (*CurrentScopeChunks)[Index].Type;
	}

	// GetParameterUniformExpression
	FMaterialUniformExpression* GetParameterUniformExpression(int32 Index) const
	{
		check(Index >= 0 && Index < CurrentScopeChunks->Num());

		const FShaderCodeChunk& Chunk = (*CurrentScopeChunks)[Index];

		return Chunk.UniformExpression;
	}

	// GetArithmeticResultType
	EMaterialValueType GetArithmeticResultType(EMaterialValueType TypeA,EMaterialValueType TypeB)
	{
		if(!(TypeA & MCT_Float) || !(TypeB & MCT_Float))
		{
			Errorf(TEXT("Attempting to perform arithmetic on non-numeric types: %s %s"),DescribeType(TypeA),DescribeType(TypeB));
			return MCT_Unknown;
		}

		if(TypeA == TypeB)
		{
			return TypeA;
		}
		else if(TypeA & TypeB)
		{
			if(TypeA == MCT_Float)
			{
				return TypeB;
			}
			else
			{
				check(TypeB == MCT_Float);
				return TypeA;
			}
		}
		else
		{
			Errorf(TEXT("Arithmetic between types %s and %s are undefined"),DescribeType(TypeA),DescribeType(TypeB));
			return MCT_Unknown;
		}
	}

	EMaterialValueType GetArithmeticResultType(int32 A,int32 B)
	{
		check(A >= 0 && A < CurrentScopeChunks->Num());
		check(B >= 0 && B < CurrentScopeChunks->Num());

		EMaterialValueType	TypeA = (*CurrentScopeChunks)[A].Type,
			TypeB = (*CurrentScopeChunks)[B].Type;

		return GetArithmeticResultType(TypeA,TypeB);
	}

	// FMaterialCompiler interface.

	/** 
	 * Sets the current material property being compiled.  
	 * This affects the internal state of the compiler and the results of all functions except GetFixedParameterCode.
	 * @param OverrideShaderFrequency SF_NumFrequencies to not override
	 */
	virtual void SetMaterialProperty(EMaterialProperty InProperty, EShaderFrequency OverrideShaderFrequency = SF_NumFrequencies, bool bUsePreviousFrameTime = false) override
	{
		MaterialProperty = InProperty;

		if(OverrideShaderFrequency != SF_NumFrequencies)
		{
			ShaderFrequency = OverrideShaderFrequency;
		}
		else
		{
			ShaderFrequency = GetMaterialPropertyShaderFrequency(InProperty);
		}

		bCompilingPreviousFrame = bUsePreviousFrameTime;

		CurrentScopeChunks = &SharedPropertyCodeChunks[ShaderFrequency];
	}
	virtual EShaderFrequency GetCurrentShaderFrequency() const override
	{
		return ShaderFrequency;
	}

	virtual EMaterialShadingModel GetMaterialShadingModel() const override
	{
		check(Material);
		return Material->GetShadingModel();
	}

	virtual int32 Error(const TCHAR* Text) override
	{
		FString ErrorString;
		check(ShaderFrequency < SF_NumFrequencies);
		auto& CurrentFunctionStack = FunctionStacks[ShaderFrequency];
		if (CurrentFunctionStack.Num() > 1)
		{
			// If we are inside a function, add that to the error message.  
			// Only add the function call node to ErrorExpressions, since we can't add a reference to the expressions inside the function as they are private objects.
			// Add the first function node on the stack because that's the one visible in the material being compiled, the rest are all nested functions.
			UMaterialExpressionMaterialFunctionCall* ErrorFunction = CurrentFunctionStack[1].FunctionCall;
			Material->ErrorExpressions.Add(ErrorFunction);
			ErrorFunction->LastErrorText = Text;
			ErrorString = FString(TEXT("Function ")) + ErrorFunction->MaterialFunction->GetName() + TEXT(": ");
		}

		if (CurrentFunctionStack.Last().ExpressionStack.Num() > 0)
		{
			UMaterialExpression* ErrorExpression = CurrentFunctionStack.Last().ExpressionStack.Last().Expression;
			check(ErrorExpression);

			if (ErrorExpression->GetClass() != UMaterialExpressionMaterialFunctionCall::StaticClass()
				&& ErrorExpression->GetClass() != UMaterialExpressionFunctionInput::StaticClass()
				&& ErrorExpression->GetClass() != UMaterialExpressionFunctionOutput::StaticClass())
			{
				// Add the expression currently being compiled to ErrorExpressions so we can draw it differently
				Material->ErrorExpressions.Add(ErrorExpression);
				ErrorExpression->LastErrorText = Text;

				const int32 ChopCount = FCString::Strlen(TEXT("MaterialExpression"));
				const FString ErrorClassName = ErrorExpression->GetClass()->GetName();

				// Add the node type to the error message
				ErrorString += FString(TEXT("(Node ")) + ErrorClassName.Right(ErrorClassName.Len() - ChopCount) + TEXT(") ");
			}
		}
			
		ErrorString += Text;

		//add the error string to the material's CompileErrors array
		Material->CompileErrors.AddUnique(ErrorString);
		bSuccess = false;
		
		return INDEX_NONE;
	}

	virtual int32 CallExpression(FMaterialExpressionKey ExpressionKey,FMaterialCompiler* Compiler) override
	{
		// Check if this expression has already been translated.
		check(ShaderFrequency < SF_NumFrequencies);
		auto& CurrentFunctionStack = FunctionStacks[ShaderFrequency];
		int32* ExistingCodeIndex = CurrentFunctionStack.Last().ExpressionCodeMap.Find(ExpressionKey);
		if(ExistingCodeIndex)
		{
			return *ExistingCodeIndex;
		}
		else
		{
			// Disallow reentrance.
			if(CurrentFunctionStack.Last().ExpressionStack.Find(ExpressionKey) != INDEX_NONE)
			{
				return Error(TEXT("Reentrant expression"));
			}

			// The first time this expression is called, translate it.
			CurrentFunctionStack.Last().ExpressionStack.Add(ExpressionKey);
			const int32 FunctionDepth = CurrentFunctionStack.Num();

			int32 Result = ExpressionKey.Expression->Compile(Compiler, ExpressionKey.OutputIndex, ExpressionKey.MultiplexIndex);

			FMaterialExpressionKey PoppedExpressionKey = CurrentFunctionStack.Last().ExpressionStack.Pop();

			// Verify state integrity
			check(PoppedExpressionKey == ExpressionKey);
			check(FunctionDepth == CurrentFunctionStack.Num());

			// Cache the translation.
			CurrentFunctionStack.Last().ExpressionCodeMap.Add(ExpressionKey,Result);

			return Result;
		}
	}

	virtual EMaterialValueType GetType(int32 Code) override
	{
		if(Code != INDEX_NONE)
		{
			return GetParameterType(Code);
		}
		else
		{
			return MCT_Unknown;
		}
	}

	virtual EMaterialQualityLevel::Type GetQualityLevel() override
	{
		return QualityLevel;
	}

	virtual ERHIFeatureLevel::Type GetFeatureLevel() override
	{
		return FeatureLevel;
	}

	/** 
	 * Casts the passed in code to DestType, or generates a compile error if the cast is not valid. 
	 * This will truncate a type (float4 -> float3) but not add components (float2 -> float3), however a float1 can be cast to any float type by replication. 
	 */
	virtual int32 ValidCast(int32 Code,EMaterialValueType DestType) override
	{
		if(Code == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		EMaterialValueType SourceType = GetParameterType(Code);
		int32 CompiledResult = INDEX_NONE;

		if (SourceType & DestType)
		{
			CompiledResult = Code;
		}
		else if(GetParameterUniformExpression(Code) && !GetParameterUniformExpression(Code)->IsConstant())
		{
			return ValidCast(AccessUniformExpression(Code), DestType);
		}
		else if((SourceType & MCT_Float) && (DestType & MCT_Float))
		{
			const uint32 NumSourceComponents = GetNumComponents(SourceType);
			const uint32 NumDestComponents = GetNumComponents(DestType);

			if(NumSourceComponents > NumDestComponents) // Use a mask to select the first NumDestComponents components from the source.
			{
				const TCHAR*	Mask;
				switch(NumDestComponents)
				{
				case 1: Mask = TEXT(".r"); break;
				case 2: Mask = TEXT(".rg"); break;
				case 3: Mask = TEXT(".rgb"); break;
				default: UE_LOG(LogMaterial, Fatal,TEXT("Should never get here!")); return INDEX_NONE;
				};

				return AddInlinedCodeChunk(DestType,TEXT("%s%s"),*GetParameterCode(Code),Mask);
			}
			else if(NumSourceComponents < NumDestComponents) // Pad the source vector up to NumDestComponents.
			{
				// Only allow replication when the Source is a Float1
				if (NumSourceComponents == 1)
				{
					const uint32 NumPadComponents = NumDestComponents - NumSourceComponents;
					FString CommaParameterCodeString = FString::Printf(TEXT(",%s"), *GetParameterCode(Code));

					CompiledResult = AddInlinedCodeChunk(
						DestType,
						TEXT("%s(%s%s%s%s)"),
						HLSLTypeString(DestType),
						*GetParameterCode(Code),
						NumPadComponents >= 1 ? *CommaParameterCodeString : TEXT(""),
						NumPadComponents >= 2 ? *CommaParameterCodeString : TEXT(""),
						NumPadComponents >= 3 ? *CommaParameterCodeString : TEXT("")
						);
				}
				else
				{
					CompiledResult = Errorf(TEXT("Cannot cast from %s to %s."), DescribeType(SourceType), DescribeType(DestType));
				}
			}
			else
			{
				CompiledResult = Code;
			}
		}
		else
		{
			//We can feed any type into a material attributes socket as we're really just passing them through.
			if( DestType == MCT_MaterialAttributes )
			{
				CompiledResult = Code;
			}
			else
			{
				CompiledResult = Errorf(TEXT("Cannot cast from %s to %s."), DescribeType(SourceType), DescribeType(DestType));
			}
		}

		return CompiledResult;
	}

	virtual int32 ForceCast(int32 Code,EMaterialValueType DestType,bool bExactMatch=false,bool bReplicateValue=false) override
	{
		if(Code == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(Code) && !GetParameterUniformExpression(Code)->IsConstant())
		{
			return ForceCast(AccessUniformExpression(Code),DestType,bExactMatch,bReplicateValue);
		}

		EMaterialValueType	SourceType = GetParameterType(Code);

		if (bExactMatch ? (SourceType == DestType) : (SourceType & DestType))
		{
			return Code;
		}
		else if((SourceType & MCT_Float) && (DestType & MCT_Float))
		{
			const uint32 NumSourceComponents = GetNumComponents(SourceType);
			const uint32 NumDestComponents = GetNumComponents(DestType);

			if(NumSourceComponents > NumDestComponents) // Use a mask to select the first NumDestComponents components from the source.
			{
				const TCHAR*	Mask;
				switch(NumDestComponents)
				{
				case 1: Mask = TEXT(".r"); break;
				case 2: Mask = TEXT(".rg"); break;
				case 3: Mask = TEXT(".rgb"); break;
				default: UE_LOG(LogMaterial, Fatal,TEXT("Should never get here!")); return INDEX_NONE;
				};

				return AddInlinedCodeChunk(DestType,TEXT("%s%s"),*GetParameterCode(Code),Mask);
			}
			else if(NumSourceComponents < NumDestComponents) // Pad the source vector up to NumDestComponents.
			{
				// Only allow replication when the Source is a Float1
				if (NumSourceComponents != 1)
				{
					bReplicateValue = false;
				}

				const uint32 NumPadComponents = NumDestComponents - NumSourceComponents;
				FString CommaParameterCodeString = FString::Printf(TEXT(",%s"), *GetParameterCode(Code));

				return AddInlinedCodeChunk(
					DestType,
					TEXT("%s(%s%s%s%s)"),
					HLSLTypeString(DestType),
					*GetParameterCode(Code),
					NumPadComponents >= 1 ? (bReplicateValue ? *CommaParameterCodeString : TEXT(",0")) : TEXT(""),
					NumPadComponents >= 2 ? (bReplicateValue ? *CommaParameterCodeString : TEXT(",0")) : TEXT(""),
					NumPadComponents >= 3 ? (bReplicateValue ? *CommaParameterCodeString : TEXT(",0")) : TEXT("")
					);
			}
			else
			{
				return Code;
			}
		}
		else
		{
			return Errorf(TEXT("Cannot force a cast between non-numeric types."));
		}
	}

	/** Pushes a function onto the compiler's function stack, which indicates that compilation is entering a function. */
	virtual void PushFunction(const FMaterialFunctionCompileState& FunctionState) override
	{
		check(ShaderFrequency < SF_NumFrequencies);
		auto& CurrentFunctionStack = FunctionStacks[ShaderFrequency];
		CurrentFunctionStack.Push(FunctionState);
	}	

	/** Pops a function from the compiler's function stack, which indicates that compilation is leaving a function. */
	virtual FMaterialFunctionCompileState PopFunction() override
	{
		check(ShaderFrequency < SF_NumFrequencies);
		auto& CurrentFunctionStack = FunctionStacks[ShaderFrequency];
		return CurrentFunctionStack.Pop();
	}

	virtual int32 AccessCollectionParameter(UMaterialParameterCollection* ParameterCollection, int32 ParameterIndex, int32 ComponentIndex) override
	{
		if (!ParameterCollection || ParameterIndex == -1)
		{
			return INDEX_NONE;
		}

		int32 CollectionIndex = ParameterCollections.Find(ParameterCollection);

		if (CollectionIndex == INDEX_NONE)
		{
			if (ParameterCollections.Num() >= MaxNumParameterCollectionsPerMaterial)
			{
				return Error(TEXT("Material references too many MaterialParameterCollections!  A material may only reference 2 different collections."));
			}

			ParameterCollections.Add(ParameterCollection);
			CollectionIndex = ParameterCollections.Num() - 1;
		}

		int32 VectorChunk = AddCodeChunk(MCT_Float4,TEXT("MaterialCollection%u.Vectors[%u]"),CollectionIndex,ParameterIndex);

		return ComponentMask(VectorChunk, 
			ComponentIndex == -1 ? true : ComponentIndex % 4 == 0,
			ComponentIndex == -1 ? true : ComponentIndex % 4 == 1,
			ComponentIndex == -1 ? true : ComponentIndex % 4 == 2,
			ComponentIndex == -1 ? true : ComponentIndex % 4 == 3);
	}

	virtual int32 VectorParameter(FName ParameterName,const FLinearColor& DefaultValue) override
	{
		return AddUniformExpression(new FMaterialUniformExpressionVectorParameter(ParameterName,DefaultValue),MCT_Float4,TEXT(""));
	}

	virtual int32 ScalarParameter(FName ParameterName,float DefaultValue) override
	{
		return AddUniformExpression(new FMaterialUniformExpressionScalarParameter(ParameterName,DefaultValue),MCT_Float,TEXT(""));
	}

	virtual int32 Constant(float X) override
	{
		return AddUniformExpression(new FMaterialUniformExpressionConstant(FLinearColor(X,X,X,X),MCT_Float),MCT_Float,TEXT("%0.8f"),X);
	}

	virtual int32 Constant2(float X,float Y) override
	{
		return AddUniformExpression(new FMaterialUniformExpressionConstant(FLinearColor(X,Y,0,0),MCT_Float2),MCT_Float2,TEXT("MaterialFloat2(%0.8f,%0.8f)"),X,Y);
	}

	virtual int32 Constant3(float X,float Y,float Z) override
	{
		return AddUniformExpression(new FMaterialUniformExpressionConstant(FLinearColor(X,Y,Z,0),MCT_Float3),MCT_Float3,TEXT("MaterialFloat3(%0.8f,%0.8f,%0.8f)"),X,Y,Z);
	}

	virtual int32 Constant4(float X,float Y,float Z,float W) override
	{
		return AddUniformExpression(new FMaterialUniformExpressionConstant(FLinearColor(X,Y,Z,W),MCT_Float4),MCT_Float4,TEXT("MaterialFloat4(%0.8f,%0.8f,%0.8f,%0.8f)"),X,Y,Z,W);
	}
	
	virtual int32 ViewProperty(EMaterialExposedViewProperty Property, bool InvProperty) override
	{
		check(Property < MEVP_MAX);

		// Compile time struct storing all EMaterialExposedViewProperty's enumerations' HLSL compilation specific meta information
		struct EMaterialExposedViewPropertyMeta
		{
			EMaterialExposedViewProperty EnumValue;
			EMaterialValueType Type;
			const TCHAR * PropertyCode;
			const TCHAR * InvPropertyCode;
		};

		static const EMaterialExposedViewPropertyMeta ViewPropertyMetaArray[] = {
			{MEVP_BufferSize, MCT_Float2, TEXT("View.BufferSizeAndInvSize.xy"), TEXT("View.BufferSizeAndInvSize.zw")},
			{MEVP_FieldOfView, MCT_Float2, TEXT("View.<PREV>FieldOfViewWideAngles"), nullptr},
			{MEVP_TanHalfFieldOfView, MCT_Float2, TEXT("Get<PREV>TanHalfFieldOfView()"), TEXT("Get<PREV>CotanHalfFieldOfView()")},
			{MEVP_ViewSize, MCT_Float2, TEXT("View.ViewSizeAndInvSize.xy"), TEXT("View.ViewSizeAndInvSize.zw")},
			{MEVP_WorldSpaceViewPosition, MCT_Float3, TEXT("ResolvedView.<PREV>WorldViewOrigin"), nullptr},
			{MEVP_WorldSpaceCameraPosition, MCT_Float3, TEXT("ResolvedView.<PREV>WorldCameraOrigin"), nullptr},
		};
		static_assert((sizeof(ViewPropertyMetaArray) / sizeof(ViewPropertyMetaArray[0])) == MEVP_MAX, "incoherency between EMaterialExposedViewProperty and ViewPropertyMetaArray");

		auto& PropertyMeta = ViewPropertyMetaArray[Property];
		check(Property == PropertyMeta.EnumValue);

		FString Code = PropertyMeta.PropertyCode;

		if (InvProperty && PropertyMeta.InvPropertyCode)
		{
			Code = PropertyMeta.InvPropertyCode;
		}

		// Resolved templated code
		Code.ReplaceInline(TEXT("<PREV>"), bCompilingPreviousFrame ? TEXT("Prev") : TEXT(""));
		
		if (InvProperty && !PropertyMeta.InvPropertyCode)
		{
			// fall back to compute the property's inverse from PropertyCode
			return Div(Constant(1.f), AddInlinedCodeChunk(PropertyMeta.Type, *Code));
		}

		return AddCodeChunk(PropertyMeta.Type, *Code);
	}

	virtual int32 GameTime(bool bPeriodic, float Period) override
	{
		if (!bPeriodic)
		{
			if (bCompilingPreviousFrame)
			{
				return AddInlinedCodeChunk(MCT_Float, TEXT("View.PrevFrameGameTime"));
			}

			return AddInlinedCodeChunk(MCT_Float, TEXT("View.GameTime"));
		}
		else if (Period == 0.0f)
		{
			return Constant(0.0f);
		}

		return AddUniformExpression(
			new FMaterialUniformExpressionFmod(
				new FMaterialUniformExpressionTime(),
				new FMaterialUniformExpressionConstant(FLinearColor(Period, Period, Period, Period), MCT_Float)
				),
			MCT_Float, TEXT("")
			);
	}

	virtual int32 RealTime(bool bPeriodic, float Period) override
	{
		if (!bPeriodic)
		{
			if (bCompilingPreviousFrame)
			{
				return AddInlinedCodeChunk(MCT_Float, TEXT("View.PrevFrameRealTime"));
			}

			return AddInlinedCodeChunk(MCT_Float, TEXT("View.RealTime"));
		}
		else if (Period == 0.0f)
		{
			return Constant(0.0f);
		}

		return AddUniformExpression(
			new FMaterialUniformExpressionFmod(
				new FMaterialUniformExpressionRealTime(),
				new FMaterialUniformExpressionConstant(FLinearColor(Period, Period, Period, Period), MCT_Float)
				),
			MCT_Float, TEXT("")
			);
	}

	virtual int32 PeriodicHint(int32 PeriodicCode) override
	{
		if(PeriodicCode == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(PeriodicCode))
		{
			return AddUniformExpression(new FMaterialUniformExpressionPeriodic(GetParameterUniformExpression(PeriodicCode)),GetParameterType(PeriodicCode),TEXT("%s"),*GetParameterCode(PeriodicCode));
		}
		else
		{
			return PeriodicCode;
		}
	}

	virtual int32 Sine(int32 X) override
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionSine(GetParameterUniformExpression(X),0),MCT_Float,TEXT("sin(%s)"),*CoerceParameter(X,MCT_Float));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),TEXT("sin(%s)"),*GetParameterCode(X));
		}
	}

	virtual int32 Cosine(int32 X) override
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionSine(GetParameterUniformExpression(X),1),MCT_Float,TEXT("cos(%s)"),*CoerceParameter(X,MCT_Float));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),TEXT("cos(%s)"),*GetParameterCode(X));
		}
	}

	virtual int32 Floor(int32 X) override
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFloor(GetParameterUniformExpression(X)),GetParameterType(X),TEXT("floor(%s)"),*GetParameterCode(X));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),TEXT("floor(%s)"),*GetParameterCode(X));
		}
	}

	virtual int32 Ceil(int32 X) override
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionCeil(GetParameterUniformExpression(X)),GetParameterType(X),TEXT("ceil(%s)"),*GetParameterCode(X));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),TEXT("ceil(%s)"),*GetParameterCode(X));
		}
	}

	virtual int32 Frac(int32 X) override
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFrac(GetParameterUniformExpression(X)),GetParameterType(X),TEXT("frac(%s)"),*GetParameterCode(X));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),TEXT("frac(%s)"),*GetParameterCode(X));
		}
	}

	virtual int32 Fmod(int32 A, int32 B) override
	{
		if ((A == INDEX_NONE) || (B == INDEX_NONE))
		{
			return INDEX_NONE;
		}

		if (GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFmod(GetParameterUniformExpression(A),GetParameterUniformExpression(B)),
				GetParameterType(A),TEXT("fmod(%s,%s)"),*GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		}
		else
		{
			return AddCodeChunk(GetParameterType(A),
				TEXT("fmod(%s,%s)"),*GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		}
	}

	/**
	* Creates the new shader code chunk needed for the Abs expression
	*
	* @param	X - Index to the FMaterialCompiler::CodeChunk entry for the input expression
	* @return	Index to the new FMaterialCompiler::CodeChunk entry for this expression
	*/	
	virtual int32 Abs( int32 X ) override
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		// get the user input struct for the input expression
		FMaterialUniformExpression* pInputParam = GetParameterUniformExpression(X);
		if( pInputParam )
		{
			FMaterialUniformExpressionAbs* pUniformExpression = new FMaterialUniformExpressionAbs( pInputParam );
			return AddUniformExpression( pUniformExpression, GetParameterType(X), TEXT("abs(%s)"), *GetParameterCode(X) );
		}
		else
		{
			return AddCodeChunk( GetParameterType(X), TEXT("abs(%s)"), *GetParameterCode(X) );
		}
	}

	virtual int32 ReflectionVector() override
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Domain)
		{
			return NonPixelShaderExpressionError();
		}
		if (ShaderFrequency != SF_Vertex)
		{
			bUsesTransformVector = true;
		}
		return AddInlinedCodeChunk(MCT_Float3,TEXT("Parameters.ReflectionVector"));
	}

	virtual int32 ReflectionAboutCustomWorldNormal(int32 CustomWorldNormal, int32 bNormalizeCustomWorldNormal) override
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Domain)
		{
			return NonPixelShaderExpressionError();
		}

		if (CustomWorldNormal == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (ShaderFrequency != SF_Vertex)
		{
			bUsesTransformVector = true;
		}

		const TCHAR* ShouldNormalize = (!!bNormalizeCustomWorldNormal) ? TEXT("true") : TEXT("false");

		return AddCodeChunk(MCT_Float3,TEXT("ReflectionAboutCustomWorldNormal(Parameters, %s, %s)"), *GetParameterCode(CustomWorldNormal), ShouldNormalize);
	}

	virtual int32 CameraVector() override
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Domain)
		{
			return NonPixelShaderExpressionError();
		}
		if (ShaderFrequency != SF_Vertex)
		{
			bUsesTransformVector = true;
		}
		return AddInlinedCodeChunk(MCT_Float3,TEXT("Parameters.CameraVector"));
	}

	virtual int32 LightVector() override
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonPixelShaderExpressionError();
		}

		if (!Material->IsLightFunction() && !Material->IsDeferredDecal())
		{
			return Errorf(TEXT("LightVector can only be used in LightFunction or DeferredDecal materials"));
		}

		return AddInlinedCodeChunk(MCT_Float3,TEXT("Parameters.LightVector"));
	}

	virtual int32 ScreenPosition(EMaterialExpressionScreenPositionMapping Mapping) override
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Vertex)
		{
			return Errorf(TEXT("Invalid node used in hull/domain shader input!"));
		}

		switch (Mapping)
		{
		case MESP_SceneTextureUV:
			return AddCodeChunk(MCT_Float2, TEXT("ScreenAlignedPosition(GetScreenPosition(Parameters))"));
		case MESP_ViewportUV:
			// Works for pixel and vertex shader.  The following commented line would optimize for pixel shader but doesnt work for vertex shader
			return AddCodeChunk(MCT_Float2, TEXT("BufferUVToViewportUV(ScreenAlignedPosition(GetScreenPosition(Parameters)))"));
			//return AddCodeChunk(MCT_Float2, TEXT("SvPositionToViewportUV(Parameters.SvPosition)"));
		default:
			return Errorf(TEXT("Invalid UV mapping!"));
		}		
	}

	virtual int32 ParticleMacroUV() override 
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonPixelShaderExpressionError();
		}

		return AddCodeChunk(MCT_Float2,TEXT("GetParticleMacroUV(Parameters)"));
	}

	virtual int32 ParticleSubUV(int32 TextureIndex, EMaterialSamplerType SamplerType, bool bBlend) override
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonPixelShaderExpressionError();
		}

		if (TextureIndex == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		int32 ParticleSubUV;
		const FString TexCoordCode = TEXT("Parameters.Particle.SubUVCoords[%u].xy");
		const int32 TexCoord1 = AddCodeChunk(MCT_Float2,*TexCoordCode,0);

		if(bBlend)
		{
			// Out	 = linear interpolate... using 2 sub-images of the texture
			// A	 = RGB sample texture with Parameters.Particle.SubUVCoords[0]
			// B	 = RGB sample texture with Parameters.Particle.SubUVCoords[1]
			// Alpha = Parameters.Particle.SubUVLerp

			const int32 TexCoord2 = AddCodeChunk( MCT_Float2,*TexCoordCode,1);
			const int32 SubImageLerp = AddCodeChunk(MCT_Float, TEXT("Parameters.Particle.SubUVLerp"));

			const int32 TexSampleA = TextureSample(TextureIndex, TexCoord1, SamplerType );
			const int32 TexSampleB = TextureSample(TextureIndex, TexCoord2, SamplerType );
			ParticleSubUV = Lerp( TexSampleA,TexSampleB, SubImageLerp);
		} 
		else
		{
			ParticleSubUV = TextureSample(TextureIndex, TexCoord1, SamplerType );
		}
	
		bUsesParticleSubUVs = true;
		return ParticleSubUV;
	}

	virtual int32 ParticleColor() override
	{
		if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		bUsesParticleColor |= (ShaderFrequency != SF_Vertex);
		return AddInlinedCodeChunk(MCT_Float4,TEXT("Parameters.Particle.Color"));	
	}

	virtual int32 ParticlePosition() override
	{
		if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		bNeedsParticlePosition = true;
		return AddInlinedCodeChunk(MCT_Float3,TEXT("(Parameters.Particle.TranslatedWorldPositionAndSize.xyz - ResolvedView.PreViewTranslation.xyz)"));	
	}

	virtual int32 ParticleRadius() override
	{
		if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		bNeedsParticlePosition = true;
		return AddInlinedCodeChunk(MCT_Float,TEXT("max(Parameters.Particle.TranslatedWorldPositionAndSize.w, .001f)"));	
	}

	virtual int32 SphericalParticleOpacity(int32 Density) override
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonPixelShaderExpressionError();
		}

		if (Density == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		bNeedsParticlePosition = true;
		bUsesSphericalParticleOpacity = true;
		bNeedsWorldPositionExcludingShaderOffsets = true;
		bUsesSceneDepth = true;
		return AddCodeChunk(MCT_Float, TEXT("GetSphericalParticleOpacity(Parameters,%s)"), *GetParameterCode(Density));
	}

	virtual int32 ParticleRelativeTime() override
	{
		if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		bNeedsParticleTime = true;
		return AddInlinedCodeChunk(MCT_Float,TEXT("Parameters.Particle.RelativeTime"));
	}

	virtual int32 ParticleMotionBlurFade() override
	{
		if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		bUsesParticleMotionBlur = true;
		return AddInlinedCodeChunk(MCT_Float,TEXT("Parameters.Particle.MotionBlurFade"));
	}

	virtual int32 ParticleRandom() override
	{
		if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		bNeedsParticleRandom = true;
		return AddInlinedCodeChunk(MCT_Float,TEXT("Parameters.Particle.Random"));
	}


	virtual int32 ParticleDirection() override
	{
		if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		bNeedsParticleVelocity = true;
		return AddInlinedCodeChunk(MCT_Float3,TEXT("Parameters.Particle.Velocity.xyz"));
	}

	virtual int32 ParticleSpeed() override
	{
		if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		bNeedsParticleVelocity = true;
		return AddInlinedCodeChunk(MCT_Float,TEXT("Parameters.Particle.Velocity.w"));
	}

	virtual int32 ParticleSize() override
	{
		if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		bNeedsParticleSize = true;
		return AddInlinedCodeChunk(MCT_Float2,TEXT("Parameters.Particle.Size"));
	}

	virtual int32 WorldPosition(EWorldPositionIncludedOffsets WorldPositionIncludedOffsets) override
	{
		FString FunctionNamePattern;

		// If this material has no expressions for world position offset or world displacement, the non-offset world position will
		// be exactly the same as the offset one, so there is no point bringing in the extra code.
		// Also, we can't access the full offset world position in anything other than the pixel shader, because it won't have
		// been calculated yet
		switch (WorldPositionIncludedOffsets)
		{
		case WPT_Default:
			{
				FunctionNamePattern = TEXT("Get<PREV>WorldPosition");
				break;
			}

		case WPT_ExcludeAllShaderOffsets:
			{
				bNeedsWorldPositionExcludingShaderOffsets = true;
				FunctionNamePattern = TEXT("Get<PREV>WorldPosition<NO_MATERIAL_OFFSETS>");
				break;
			}

		case WPT_CameraRelative:
			{
				FunctionNamePattern = TEXT("Get<PREV>TranslatedWorldPosition");
				break;
			}

		case WPT_CameraRelativeNoOffsets:
			{
				bNeedsWorldPositionExcludingShaderOffsets = true;
				FunctionNamePattern = TEXT("Get<PREV>TranslatedWorldPosition<NO_MATERIAL_OFFSETS>");
				break;
			}

		default:
			{
				Errorf(TEXT("Encountered unknown world position type '%d'"), WorldPositionIncludedOffsets);
				return INDEX_NONE;
			}
		}

		// If compiling for the previous frame in the vertex shader
		FunctionNamePattern.ReplaceInline(TEXT("<PREV>"), bCompilingPreviousFrame && ShaderFrequency == SF_Vertex ? TEXT("Prev") : TEXT(""));
		
		if (ShaderFrequency == SF_Pixel)
		{
			// No material offset only available in the vertex shader.
			// TODO: should also be available in the tesselation shader
			FunctionNamePattern.ReplaceInline(TEXT("<NO_MATERIAL_OFFSETS>"), TEXT("_NoMaterialOffsets"));
		}
		else
		{
			FunctionNamePattern.ReplaceInline(TEXT("<NO_MATERIAL_OFFSETS>"), TEXT(""));
		}

		return AddInlinedCodeChunk(MCT_Float3, TEXT("%s(Parameters)"), *FunctionNamePattern);
	}

	virtual int32 ObjectWorldPosition() override
	{
		return AddInlinedCodeChunk(MCT_Float3,TEXT("GetObjectWorldPosition(Parameters)"));		
	}

	virtual int32 ObjectRadius() override
	{
		return GetPrimitiveProperty(MCT_Float, TEXT("ObjectRadius"), TEXT("ObjectWorldPositionAndRadius.w"));		
	}

	virtual int32 ObjectBounds() override
	{
		return GetPrimitiveProperty(MCT_Float3, TEXT("ObjectBounds"), TEXT("ObjectBounds.xyz"));
	}

	virtual int32 DistanceCullFade() override
	{
		return AddInlinedCodeChunk(MCT_Float,TEXT("GetDistanceCullFade()"));		
	}

	virtual int32 ActorWorldPosition() override
	{
		return AddInlinedCodeChunk(MCT_Float3,TEXT("GetActorWorldPosition()"));		
	}

	virtual int32 If(int32 A,int32 B,int32 AGreaterThanB,int32 AEqualsB,int32 ALessThanB,int32 ThresholdArg) override
	{
		if(A == INDEX_NONE || B == INDEX_NONE || AGreaterThanB == INDEX_NONE || ALessThanB == INDEX_NONE || ThresholdArg == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (AEqualsB != INDEX_NONE)
		{
			EMaterialValueType ResultType = GetArithmeticResultType(GetParameterType(AGreaterThanB),GetArithmeticResultType(AEqualsB,ALessThanB));

			int32 CoercedAGreaterThanB = ForceCast(AGreaterThanB,ResultType);
			int32 CoercedAEqualsB = ForceCast(AEqualsB,ResultType);
			int32 CoercedALessThanB = ForceCast(ALessThanB,ResultType);

			if(CoercedAGreaterThanB == INDEX_NONE || CoercedAEqualsB == INDEX_NONE || CoercedALessThanB == INDEX_NONE)
			{
				return INDEX_NONE;
			}

			return AddCodeChunk(
				ResultType,
				TEXT("((abs(%s - %s) > %s) ? (%s >= %s ? %s : %s) : %s)"),
				*GetParameterCode(A),
				*GetParameterCode(B),
				*GetParameterCode(ThresholdArg),
				*GetParameterCode(A),
				*GetParameterCode(B),
				*GetParameterCode(CoercedAGreaterThanB),
				*GetParameterCode(CoercedALessThanB),
				*GetParameterCode(CoercedAEqualsB)
				);
		}
		else
		{
			EMaterialValueType ResultType = GetArithmeticResultType(AGreaterThanB,ALessThanB);

			int32 CoercedAGreaterThanB = ForceCast(AGreaterThanB,ResultType);
			int32 CoercedALessThanB = ForceCast(ALessThanB,ResultType);

			if(CoercedAGreaterThanB == INDEX_NONE || CoercedALessThanB == INDEX_NONE)
			{
				return INDEX_NONE;
			}

			return AddCodeChunk(
				ResultType,
				TEXT("((%s >= %s) ? %s : %s)"),
				*GetParameterCode(A),
				*GetParameterCode(B),
				*GetParameterCode(CoercedAGreaterThanB),
				*GetParameterCode(CoercedALessThanB)
				);
		}
	}

#if WITH_EDITOR
	virtual int32 MaterialBakingWorldPosition() override
	{
		if (ShaderFrequency == SF_Vertex)
		{
			NumUserVertexTexCoords = FMath::Max((uint32)8, NumUserVertexTexCoords);
		}
		else
		{
			NumUserTexCoords = FMath::Max((uint32)8, NumUserTexCoords);
		}

		// Note: inlining is important so that on ES2 devices, where half precision is used in the pixel shader, 
		// The UV does not get assigned to a half temporary in cases where the texture sample is done directly from interpolated UVs
		return AddInlinedCodeChunk(MCT_Float3, TEXT("float3(Parameters.TexCoords[6].x, Parameters.TexCoords[6].y, Parameters.TexCoords[7].x)"));
	}
#endif
		

	virtual int32 TextureCoordinate(uint32 CoordinateIndex, bool UnMirrorU, bool UnMirrorV) override
	{
		const uint32 MaxNumCoordinates = (FeatureLevel == ERHIFeatureLevel::ES2) ? 3 : 8;

		if (CoordinateIndex >= MaxNumCoordinates)
		{
			return Errorf(TEXT("Only %u texture coordinate sets can be used by this feature level, currently using %u"), MaxNumCoordinates, CoordinateIndex + 1);
		}

		if (ShaderFrequency == SF_Vertex)
		{
			NumUserVertexTexCoords = FMath::Max(CoordinateIndex + 1, NumUserVertexTexCoords);
		}
		else
		{
			NumUserTexCoords = FMath::Max(CoordinateIndex + 1, NumUserTexCoords);
		}

		FString	SampleCode;
		if ( UnMirrorU && UnMirrorV )
		{
			SampleCode = TEXT("UnMirrorUV(Parameters.TexCoords[%u].xy, Parameters)");
		}
		else if ( UnMirrorU )
		{
			SampleCode = TEXT("UnMirrorU(Parameters.TexCoords[%u].xy, Parameters)");
		}
		else if ( UnMirrorV )
		{
			SampleCode = TEXT("UnMirrorV(Parameters.TexCoords[%u].xy, Parameters)");
		}
		else
		{
			SampleCode = TEXT("Parameters.TexCoords[%u].xy");
		}

		// Note: inlining is important so that on ES2 devices, where half precision is used in the pixel shader, 
		// The UV does not get assigned to a half temporary in cases where the texture sample is done directly from interpolated UVs
		return AddInlinedCodeChunk(
				MCT_Float2,
				*SampleCode,
				CoordinateIndex
				);
		}

	virtual int32 TextureSample(
		int32 TextureIndex,
		int32 CoordinateIndex,
		EMaterialSamplerType SamplerType,
		int32 MipValue0Index=INDEX_NONE,
		int32 MipValue1Index=INDEX_NONE,
		ETextureMipValueMode MipValueMode=TMVM_None,
		ESamplerSourceMode SamplerSource=SSM_FromTextureAsset,
		int32 TextureReferenceIndex=INDEX_NONE
		) override
	{
		if(TextureIndex == INDEX_NONE || CoordinateIndex == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (FeatureLevel == ERHIFeatureLevel::ES2 && ShaderFrequency == SF_Vertex)
		{
			if (MipValueMode != TMVM_MipLevel)
			{
				Errorf(TEXT("Sampling from vertex textures requires an absolute mip level on feature level ES2!"));
				return INDEX_NONE;
			}
		}
		else if (ShaderFrequency != SF_Pixel
			&& ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::ES3_1) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		EMaterialValueType TextureType = GetParameterType(TextureIndex);

		if(TextureType != MCT_Texture2D && TextureType != MCT_TextureCube)
		{
			Errorf(TEXT("Sampling unknown texture type: %s"),DescribeType(TextureType));
			return INDEX_NONE;
		}

		if(ShaderFrequency != SF_Pixel && MipValueMode == TMVM_MipBias)
		{
			Errorf(TEXT("MipBias is only supported in the pixel shader"));
			return INDEX_NONE;
		}

		FString MipValue0Code = TEXT("0.0f");
		FString MipValue1Code = TEXT("0.0f");

		if (MipValue0Index != INDEX_NONE && (MipValueMode == TMVM_MipBias || MipValueMode == TMVM_MipLevel))
		{
			MipValue0Code = CoerceParameter(MipValue0Index, MCT_Float1);
		}

		// if we are not in the PS we need a mip level
		if(ShaderFrequency != SF_Pixel)
		{
			MipValueMode = TMVM_MipLevel;
		}

		FString SamplerStateCode;

		if (SamplerSource == SSM_FromTextureAsset)
		{
			SamplerStateCode = TEXT("%sSampler");
		}
		else if (SamplerSource == SSM_Wrap_WorldGroupSettings)
		{
			// Use the shared sampler to save sampler slots
			SamplerStateCode = TEXT("GetMaterialSharedSampler(%sSampler,Material.Wrap_WorldGroupSettings)");
		}
		else if (SamplerSource == SSM_Clamp_WorldGroupSettings)
		{
			// Use the shared sampler to save sampler slots
			SamplerStateCode = TEXT("GetMaterialSharedSampler(%sSampler,Material.Clamp_WorldGroupSettings)");
		}

		FString SampleCode = 
			(TextureType == MCT_TextureCube)
			? TEXT("TextureCubeSample")
			: TEXT("Texture2DSample");
		
		EMaterialValueType UVsType = (TextureType == MCT_TextureCube) ? MCT_Float3 : MCT_Float2;
	
		if(MipValueMode == TMVM_None)
		{
			SampleCode += FString(TEXT("(%s,")) + SamplerStateCode + TEXT(",%s)");
		}
		else if(MipValueMode == TMVM_MipLevel)
		{
			// Mobile: Sampling of a particular level depends on an extension; iOS does have it by default but
			// there's a driver as of 7.0.2 that will cause a GPU hang if used with an Aniso > 1 sampler, so show an error for now
			if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::ES3_1) == INDEX_NONE)
			{
				Errorf(TEXT("Sampling for a specific mip-level is not supported for ES2"));
				return INDEX_NONE;
			}

			SampleCode += TEXT("Level(%s,%sSampler,%s,%s)");
		}
		else if(MipValueMode == TMVM_MipBias)
		{
			SampleCode += TEXT("Bias(%s,%sSampler,%s,%s)");
		}
		else if(MipValueMode == TMVM_Derivative)
		{
			if (MipValue0Index == INDEX_NONE)
			{
				return Errorf(TEXT("Missing DDX(UVs) parameter"));
			}
			else if (MipValue1Index == INDEX_NONE)
			{
				return Errorf(TEXT("Missing DDY(UVs) parameter"));
			}

			SampleCode += TEXT("Grad(%s,%sSampler,%s,%s,%s)");

			MipValue0Code = CoerceParameter(MipValue0Index, UVsType);
			MipValue1Code = CoerceParameter(MipValue1Index, UVsType);
		}
		else
		{
			check(0);
		}

		switch( SamplerType )
		{
			case SAMPLERTYPE_Color:
				SampleCode = FString::Printf( TEXT("ProcessMaterialColorTextureLookup(%s)"), *SampleCode );
				break;

			case SAMPLERTYPE_LinearColor:
				SampleCode = FString::Printf(TEXT("ProcessMaterialLinearColorTextureLookup(%s)"), *SampleCode);
			break;

			case SAMPLERTYPE_Alpha:
			case SAMPLERTYPE_DistanceFieldFont:
				// Sampling a single channel texture in D3D9 gives: (G,G,G)
				// Sampling a single channel texture in D3D11 gives: (G,0,0)
				// This replication reproduces the D3D9 behavior in all cases.
				SampleCode = FString::Printf( TEXT("(%s).rrrr"), *SampleCode );
				break;
			
			case SAMPLERTYPE_Grayscale:
				// Sampling a greyscale texture in D3D9 gives: (G,G,G)
				// Sampling a greyscale texture in D3D11 gives: (G,0,0)
				// This replication reproduces the D3D9 behavior in all cases.
				SampleCode = FString::Printf( TEXT("ProcessMaterialGreyscaleTextureLookup((%s).r).rrrr"), *SampleCode );
				break;

			case SAMPLERTYPE_LinearGrayscale:
				// Sampling a greyscale texture in D3D9 gives: (G,G,G)
				// Sampling a greyscale texture in D3D11 gives: (G,0,0)
				// This replication reproduces the D3D9 behavior in all cases.
				SampleCode = FString::Printf(TEXT("ProcessMaterialLinearGreyscaleTextureLookup((%s).r).rrrr"), *SampleCode);
				break;

			case SAMPLERTYPE_Normal:
				// Normal maps need to be unpacked in the pixel shader.
				SampleCode = FString::Printf( TEXT("UnpackNormalMap(%s)"), *SampleCode );
				break;
			case SAMPLERTYPE_Masks:
				break;
		}

		FString TextureName =
			(TextureType == MCT_TextureCube)
			? CoerceParameter(TextureIndex, MCT_TextureCube)
			: CoerceParameter(TextureIndex, MCT_Texture2D);

		FString UVs = CoerceParameter(CoordinateIndex, UVsType);

		const bool bStoreTexCoordScales = (ShaderFrequency == SF_Pixel && TextureReferenceIndex != INDEX_NONE && Material && Material->GetShaderMapUsage() == EMaterialShaderMapUsage::DebugViewModeTexCoordScale);
		if (bStoreTexCoordScales)
		{
			AddCodeChunk(MCT_Float, TEXT("StoreTexCoordScale(Parameters.TexCoordScalesParams, %s, %d)"), *UVs, (int)TextureReferenceIndex);
		}

		int32 SamplingCodeIndex = AddCodeChunk(
			MCT_Float4,
			*SampleCode,
			*TextureName,
			*TextureName,
			*UVs,
			*MipValue0Code,
			*MipValue1Code
			);

		if (bStoreTexCoordScales)
		{
			FString SamplingCode = CoerceParameter(SamplingCodeIndex, MCT_Float4);
			AddCodeChunk(MCT_Float, TEXT("StoreTexSample(Parameters.TexCoordScalesParams, %s, %d)"), *SamplingCode, (int)TextureReferenceIndex);
		}

		return SamplingCodeIndex;
	}

	virtual int32 TextureProperty(int32 TextureIndex, EMaterialExposedTextureProperty Property) override
	{
		EMaterialValueType TextureType = GetParameterType(TextureIndex);

		if(TextureType != MCT_Texture2D)
		{
			return Errorf(TEXT("Texture size only available for Texture2D, not %s"),DescribeType(TextureType));
		}
		
		auto TextureExpression = (FMaterialUniformExpressionTexture*) (*CurrentScopeChunks)[TextureIndex].UniformExpression.GetReference();

		return AddUniformExpression(new FMaterialUniformExpressionTextureProperty(TextureExpression, Property), MCT_Float2, TEXT(""));
	}

	virtual int32 TextureDecalMipmapLevel(int32 TextureSizeInput) override
	{
		if (Material->GetMaterialDomain() != MD_DeferredDecal)
		{
			return Errorf(TEXT("Decal mipmap level only available in the decal material domain."));
		}

		EMaterialValueType TextureSizeType = GetParameterType(TextureSizeInput);

		if (TextureSizeType != MCT_Float2)
		{
			Errorf(TEXT("Unmatching conversion %s -> float2"), DescribeType(TextureSizeType));
			return INDEX_NONE;
		}

		FString TextureSize = CoerceParameter(TextureSizeInput, MCT_Float2);

		return AddCodeChunk(
			MCT_Float1,
			TEXT("ComputeDecalMipmapLevel(Parameters,%s)"),
			*TextureSize
			);
	}

	virtual int32 TextureDecalDerivative(bool bDDY) override
	{
		if (Material->GetMaterialDomain() != MD_DeferredDecal)
		{
			return Errorf(TEXT("Decal derivatives only available in the decal material domain."));
		}

		return AddCodeChunk(
			MCT_Float2,
			bDDY ? TEXT("ComputeDecalDDY(Parameters)") : TEXT("ComputeDecalDDX(Parameters)")
			);
	}

	virtual int32 DecalLifetimeOpacity() override
	{
		if (Material->GetMaterialDomain() != MD_DeferredDecal)
		{
			return Errorf(TEXT("Decal lifetime fade is only available in the decal material domain."));
		}

		if (ShaderFrequency != SF_Pixel)
		{
			return Errorf(TEXT("Decal lifetime fade is only available in the pixel shader."));
		}

		return AddCodeChunk(
			MCT_Float,
			TEXT("DecalLifetimeOpacity()")
			);
	}

	virtual int32 PixelDepth() override
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Vertex)
		{
			return Errorf(TEXT("Invalid node used in hull/domain shader input!"));
		}
		return AddInlinedCodeChunk(MCT_Float, TEXT("GetScreenPosition(Parameters).w"));		
	}

	/** Calculate screen aligned UV coordinates from an offset fraction or texture coordinate */
	int32 GetScreenAlignedUV(int32 Offset, int32 UV, bool bUseOffset)
	{
		if(bUseOffset)
		{
			return AddCodeChunk(MCT_Float2, TEXT("CalcScreenUVFromOffsetFraction(GetScreenPosition(Parameters), %s)"), *GetParameterCode(Offset));
		}
		else
		{
			FString DefaultScreenAligned(TEXT("ScreenAlignedPosition(GetScreenPosition(Parameters))"));
			FString CodeString = (UV != INDEX_NONE) ? CoerceParameter(UV,MCT_Float2) : DefaultScreenAligned;
			return AddInlinedCodeChunk(MCT_Float2, *CodeString );
		}
	}

	virtual int32 SceneDepth(int32 Offset, int32 UV, bool bUseOffset) override
	{
		if (Offset == INDEX_NONE && bUseOffset)
		{
			return INDEX_NONE;
		}

		bUsesSceneDepth = true;

		FString	UserDepthCode(TEXT("CalcSceneDepth(%s)"));
		int32 TexCoordCode = GetScreenAlignedUV(Offset, UV, bUseOffset);
		// add the code string
		return AddCodeChunk(
			MCT_Float,
			*UserDepthCode,
			*GetParameterCode(TexCoordCode)
			);
	}
	
	// @param SceneTextureId of type ESceneTextureId e.g. PPI_SubsurfaceColor
	virtual int32 SceneTextureLookup(int32 UV, uint32 InSceneTextureId, bool bFiltered) override
	{
		if (InSceneTextureId != PPI_PostProcessInput0 // fetching PostProcessInput0 supported on all platforms
			&& ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM4) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Vertex)
		{
			// we can relax this later if needed
			return NonPixelShaderExpressionError();
		}
		
		if (InSceneTextureId == PPI_DecalMask)
		{
			return Error(TEXT("Decal Mask bit was move out of GBuffer to the stencil buffer for performance optimisation and is therefor no longer available"));
		}

		ESceneTextureId SceneTextureId = (ESceneTextureId)InSceneTextureId;

		UseSceneTextureId(SceneTextureId, true);

		if (FeatureLevel >= ERHIFeatureLevel::SM4)
		{
			FString DefaultScreenAligned(TEXT("ScreenAlignedPosition(GetScreenPosition(Parameters))"));
			FString TexCoordCode((UV != INDEX_NONE) ? CoerceParameter(UV, MCT_Float2) : DefaultScreenAligned);
			
			return AddCodeChunk(
				MCT_Float4,
				TEXT("SceneTextureLookup(%s, %d, %s)"),
				*TexCoordCode, (int)SceneTextureId, bFiltered ? TEXT("true") : TEXT("false")
				);
		}
		else
		{
			if (UV == INDEX_NONE)
			{
				// Avoid UV computation in a pixel shader
				UV = TextureCoordinate(0, false, false);
			}
			FString TexCoordCode = CoerceParameter(UV, MCT_Float2);
			// Only PPI_PostProcessInput0, which holds scene color 
			return AddCodeChunk(MCT_Float4,	TEXT("MobileLookupPostProcessInput0(Parameters, %s)"), *TexCoordCode);
		}
	}

	// @param SceneTextureId of type ESceneTextureId e.g. PPI_SubsurfaceColor
	virtual int32 SceneTextureSize(uint32 InSceneTextureId, bool bInvert) override
	{
		if (ShaderFrequency != SF_Pixel)
		{
			// we can relax this later if needed
			return NonPixelShaderExpressionError();
		}

		ESceneTextureId SceneTextureId = (ESceneTextureId)InSceneTextureId;

		UseSceneTextureId(SceneTextureId, false);

		if(SceneTextureId >= PPI_PostProcessInput0 && SceneTextureId <= PPI_PostProcessInput6)
		{
			int Index = SceneTextureId - PPI_PostProcessInput0;

			return AddCodeChunk(MCT_Float2, TEXT("GetPostProcessInputSize(%d).%s"), Index, bInvert ? TEXT("zw") : TEXT("xy"));
		}
		else
		{
			// BufferSize
			if(bInvert)
			{
				return Div(Constant(1.0f), AddCodeChunk(MCT_Float2, TEXT("View.BufferSizeAndInvSize.xy")));
			}
			else
			{
				return AddCodeChunk(MCT_Float2, TEXT("View.BufferSizeAndInvSize.xy"));
			}
		}
	}

	// @param SceneTextureId of type ESceneTextureId e.g. PPI_SubsurfaceColor
	virtual int32 SceneTextureMin(uint32 InSceneTextureId) override
	{
		if (ShaderFrequency != SF_Pixel)
		{
			// we can relax this later if needed
			return NonPixelShaderExpressionError();
		}

		ESceneTextureId SceneTextureId = (ESceneTextureId)InSceneTextureId;

		UseSceneTextureId(SceneTextureId, false);

		if(SceneTextureId >= PPI_PostProcessInput0 && SceneTextureId <= PPI_PostProcessInput6)
		{
			int Index = SceneTextureId - PPI_PostProcessInput0;

			return AddCodeChunk(MCT_Float2, TEXT("GetPostProcessInputMinMax(%d).xy"), Index);
		}
		else
		{			
			return AddCodeChunk(MCT_Float2,TEXT("View.SceneTextureMinMax.xy"));
		}
	}

	virtual int32 SceneTextureMax(uint32 InSceneTextureId) override
	{
		if (ShaderFrequency != SF_Pixel)
		{
			// we can relax this later if needed
			return NonPixelShaderExpressionError();
		}

		ESceneTextureId SceneTextureId = (ESceneTextureId)InSceneTextureId;

		UseSceneTextureId(SceneTextureId, false);

		if(SceneTextureId >= PPI_PostProcessInput0 && SceneTextureId <= PPI_PostProcessInput6)
		{
			int Index = SceneTextureId - PPI_PostProcessInput0;

			return AddCodeChunk(MCT_Float2, TEXT("GetPostProcessInputMinMax(%d).zw"), Index);
		}
		else
		{			
			return AddCodeChunk(MCT_Float2,TEXT("View.SceneTextureMinMax.zw"));
		}
	}

	// @param bTextureLookup true: texture, false:no texture lookup, usually to get the size
	void UseSceneTextureId(ESceneTextureId SceneTextureId, bool bTextureLookup)
	{
		MaterialCompilationOutput.bNeedsSceneTextures = true;

		if(Material->GetMaterialDomain() == MD_DeferredDecal)
		{
			EDecalBlendMode DecalBlendMode = (EDecalBlendMode)Material->GetDecalBlendMode();
			bool bDBuffer = IsDBufferDecalBlendMode(DecalBlendMode);

			bool bRequiresSM5 = (SceneTextureId == PPI_WorldNormal || SceneTextureId == PPI_CustomDepth || SceneTextureId == PPI_CustomStencil || SceneTextureId == PPI_AmbientOcclusion);

			if(bDBuffer)
			{
				if(!(SceneTextureId == PPI_SceneDepth || SceneTextureId == PPI_CustomDepth || SceneTextureId == PPI_CustomStencil))
				{
					// Note: For DBuffer decals: CustomDepth and CustomStencil are only available if r.CustomDepth.Order = 0
					Errorf(TEXT("DBuffer decals (MaterialDomain=DeferredDecal and DecalBlendMode is using DBuffer) can only access SceneDepth, CustomDepth, CustomStencil"));
				}
			}
			else
			{
				if(!(SceneTextureId == PPI_SceneDepth || SceneTextureId == PPI_CustomDepth || SceneTextureId == PPI_CustomStencil || SceneTextureId == PPI_WorldNormal || SceneTextureId == PPI_AmbientOcclusion))
				{
					Errorf(TEXT("Decals (MaterialDomain=DeferredDecal) can only access WorldNormal, AmbientOcclusion, SceneDepth, CustomDepth, CustomStencil"));
				}

				if (SceneTextureId == PPI_WorldNormal && Material->HasNormalConnected())
				{
					 // GBuffer can only relate to WorldNormal here.
					Errorf(TEXT("Decals that read WorldNormal cannot output to normal at the same time"));
				}
			}

			if (bRequiresSM5)
			{
				ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM4);
			}
		}

		if(SceneTextureId == PPI_SceneColor && Material->GetMaterialDomain() != MD_Surface)
		{
			if(Material->GetMaterialDomain() == MD_PostProcess)
			{
				Errorf(TEXT("SceneColor lookups are only available when MaterialDomain = Surface. PostProcessMaterials should use the SceneTexture PostProcessInput0."));
			}
			else
			{
				Errorf(TEXT("SceneColor lookups are only available when MaterialDomain = Surface."));
			}
		}

		if(bTextureLookup)
		{
			bNeedsSceneTexturePostProcessInputs = bNeedsSceneTexturePostProcessInputs
				|| ((SceneTextureId >= PPI_PostProcessInput0 && SceneTextureId <= PPI_PostProcessInput6)
				|| SceneTextureId == PPI_SceneColor);
		}

		const bool bNeedsGBuffer = SceneTextureId == PPI_DiffuseColor 
			|| SceneTextureId == PPI_SpecularColor
			|| SceneTextureId == PPI_SubsurfaceColor
			|| SceneTextureId == PPI_BaseColor
			|| SceneTextureId == PPI_Specular
			|| SceneTextureId == PPI_Metallic
			|| SceneTextureId == PPI_WorldNormal
			|| SceneTextureId == PPI_Opacity
			|| SceneTextureId == PPI_Roughness
			|| SceneTextureId == PPI_MaterialAO
			|| SceneTextureId == PPI_DecalMask
			|| SceneTextureId == PPI_ShadingModel
			|| SceneTextureId == PPI_StoredBaseColor
			|| SceneTextureId == PPI_StoredSpecular;

		MaterialCompilationOutput.bNeedsGBuffer = MaterialCompilationOutput.bNeedsGBuffer || bNeedsGBuffer;

		if (bNeedsGBuffer && IsForwardShadingEnabled(FeatureLevel))
		{
			Errorf(TEXT("GBuffer scene textures not available with forward shading."));
		}

		// not yet tracked:
		//   PPI_SeparateTranslucency, PPI_CustomDepth, PPI_AmbientOcclusion
	}

	virtual int32 SceneColor(int32 Offset, int32 UV, bool bUseOffset) override
	{
		if (Offset == INDEX_NONE && bUseOffset)
		{
			return INDEX_NONE;
		}

		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}

		if(Material->GetMaterialDomain() != MD_Surface)
		{
			Errorf(TEXT("SceneColor lookups are only available when MaterialDomain = Surface."));
		}

		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM4) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		MaterialCompilationOutput.bRequiresSceneColorCopy = true;

		int32 ScreenUVCode = GetScreenAlignedUV(Offset, UV, bUseOffset);
		return AddCodeChunk(
			MCT_Float3,
			TEXT("DecodeSceneColorForMaterialNode(%s)"),
			*GetParameterCode(ScreenUVCode)
			);
	}

	virtual int32 Texture(UTexture* InTexture,int32& TextureReferenceIndex,ESamplerSourceMode SamplerSource=SSM_FromTextureAsset, ETextureMipValueMode MipValueMode = TMVM_None) override
	{
		if (FeatureLevel == ERHIFeatureLevel::ES2 && ShaderFrequency == SF_Vertex)
		{
			if (MipValueMode != TMVM_MipLevel)
			{
				Errorf(TEXT("Sampling from vertex textures requires an absolute mip level on feature level ES2"));
				return INDEX_NONE;
			}
		}
		else if (ShaderFrequency != SF_Pixel
			&& ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::ES3_1) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		EMaterialValueType ShaderType = InTexture->GetMaterialType();
		TextureReferenceIndex = Material->GetReferencedTextures().Find(InTexture);

#if DO_CHECK
		// UE-3518: Additional pre-assert logging to help determine the cause of this failure.
		if (TextureReferenceIndex == INDEX_NONE)
		{
			const TArray<UTexture*>& ReferencedTextures = Material->GetReferencedTextures();
			UE_LOG(LogMaterial, Error, TEXT("Compiler->Texture() failed to find texture '%s' in referenced list of size '%i':"), *InTexture->GetName(), ReferencedTextures.Num());
			for (int32 i = 0; i < ReferencedTextures.Num(); ++i)
			{
				UE_LOG(LogMaterial, Error, TEXT("%i: '%s'"), i, ReferencedTextures[i] ? *ReferencedTextures[i]->GetName() : TEXT("nullptr"));
			}
		}
#endif
		checkf(TextureReferenceIndex != INDEX_NONE, TEXT("Material expression called Compiler->Texture() without implementing UMaterialExpression::GetReferencedTexture properly"));

		return AddUniformExpression(new FMaterialUniformExpressionTexture(TextureReferenceIndex, SamplerSource),ShaderType,TEXT(""));
	}

	virtual int32 TextureParameter(FName ParameterName,UTexture* DefaultValue,int32& TextureReferenceIndex,ESamplerSourceMode SamplerSource=SSM_FromTextureAsset) override
	{
		if (ShaderFrequency != SF_Pixel
			&& ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM4) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		EMaterialValueType ShaderType = DefaultValue->GetMaterialType();
		TextureReferenceIndex = Material->GetReferencedTextures().Find(DefaultValue);
		checkf(TextureReferenceIndex != INDEX_NONE, TEXT("Material expression called Compiler->TextureParameter() without implementing UMaterialExpression::GetReferencedTexture properly"));
		return AddUniformExpression(new FMaterialUniformExpressionTextureParameter(ParameterName, TextureReferenceIndex, SamplerSource),ShaderType,TEXT(""));
	}

	virtual int32 GetTextureReferenceIndex(UTexture* TextureValue)
	{
		return Material->GetReferencedTextures().Find(TextureValue);
	}

	virtual int32 StaticBool(bool bValue) override
	{
		return AddInlinedCodeChunk(MCT_StaticBool,(bValue ? TEXT("true") : TEXT("false")));
	}

	virtual int32 StaticBoolParameter(FName ParameterName,bool bDefaultValue) override
	{
		// Look up the value we are compiling with for this static parameter.
		bool bValue = bDefaultValue;
		for(int32 ParameterIndex = 0;ParameterIndex < StaticParameters.StaticSwitchParameters.Num();++ParameterIndex)
		{
			const FStaticSwitchParameter& Parameter = StaticParameters.StaticSwitchParameters[ParameterIndex];
			if(Parameter.ParameterName == ParameterName)
			{
				bValue = Parameter.Value;
				break;
			}
		}

		return StaticBool(bValue);
	}
	
	virtual int32 StaticComponentMask(int32 Vector,FName ParameterName,bool bDefaultR,bool bDefaultG,bool bDefaultB,bool bDefaultA) override
	{
		// Look up the value we are compiling with for this static parameter.
		bool bValueR = bDefaultR;
		bool bValueG = bDefaultG;
		bool bValueB = bDefaultB;
		bool bValueA = bDefaultA;
		for(int32 ParameterIndex = 0;ParameterIndex < StaticParameters.StaticComponentMaskParameters.Num();++ParameterIndex)
		{
			const FStaticComponentMaskParameter& Parameter = StaticParameters.StaticComponentMaskParameters[ParameterIndex];
			if(Parameter.ParameterName == ParameterName)
			{
				bValueR = Parameter.R;
				bValueG = Parameter.G;
				bValueB = Parameter.B;
				bValueA = Parameter.A;
				break;
			}
		}

		return ComponentMask(Vector,bValueR,bValueG,bValueB,bValueA);
	}

	virtual bool GetStaticBoolValue(int32 BoolIndex, bool& bSucceeded) override
	{
		bSucceeded = true;
		if (BoolIndex == INDEX_NONE)
		{
			bSucceeded = false;
			return false;
		}

		if (GetParameterType(BoolIndex) != MCT_StaticBool)
		{
			Errorf(TEXT("Failed to cast %s input to static bool type"), DescribeType(GetParameterType(BoolIndex)));
			bSucceeded = false;
			return false;
		}

		if (GetParameterCode(BoolIndex).Contains(TEXT("true")))
		{
			return true;
		}
		return false;
	}

	virtual int32 StaticTerrainLayerWeight(FName ParameterName,int32 Default) override
	{
		// Look up the weight-map index for this static parameter.
		int32 WeightmapIndex = INDEX_NONE;
		bool bFoundParameter = false;
		for(int32 ParameterIndex = 0;ParameterIndex < StaticParameters.TerrainLayerWeightParameters.Num();++ParameterIndex)
		{
			const FStaticTerrainLayerWeightParameter& Parameter = StaticParameters.TerrainLayerWeightParameters[ParameterIndex];
			if(Parameter.ParameterName == ParameterName)
			{
				WeightmapIndex = Parameter.WeightmapIndex;
				bFoundParameter = true;
				break;
			}
		}

		if(!bFoundParameter)
		{
			return Default;
		}
		else if(WeightmapIndex == INDEX_NONE)
		{
			return INDEX_NONE;
		}
		else if (GetFeatureLevel() >= ERHIFeatureLevel::SM4)
		{
			FString WeightmapName = FString::Printf(TEXT("Weightmap%d"),WeightmapIndex);
			int32 TextureReferenceIndex = INDEX_NONE;
			int32 TextureCodeIndex = TextureParameter(FName(*WeightmapName), GEngine->WeightMapPlaceholderTexture, TextureReferenceIndex);
			int32 WeightmapCode = TextureSample(TextureCodeIndex, TextureCoordinate(3, false, false), SAMPLERTYPE_Masks);
			FString LayerMaskName = FString::Printf(TEXT("LayerMask_%s"),*ParameterName.ToString());
			return Dot(WeightmapCode,VectorParameter(FName(*LayerMaskName), FLinearColor(1.f,0.f,0.f,0.f)));
		}
		else
		{
			int32 WeightmapCode = AddInlinedCodeChunk(MCT_Float4, TEXT("Parameters.LayerWeights"));
			FString LayerMaskName = FString::Printf(TEXT("LayerMask_%s"),*ParameterName.ToString());
			return Dot(WeightmapCode,VectorParameter(FName(*LayerMaskName), FLinearColor(1.f,0.f,0.f,0.f)));
		}
	}

	virtual int32 VertexColor() override
	{
		bUsesVertexColor |= (ShaderFrequency != SF_Vertex);
		return AddInlinedCodeChunk(MCT_Float4,TEXT("Parameters.VertexColor"));
	}

	virtual int32 Add(int32 A,int32 B) override
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(GetParameterUniformExpression(A),GetParameterUniformExpression(B),FMO_Add),GetArithmeticResultType(A,B),TEXT("(%s + %s)"),*GetParameterCode(A),*GetParameterCode(B));
		}
		else
		{
			return AddCodeChunk(GetArithmeticResultType(A,B),TEXT("(%s + %s)"),*GetParameterCode(A),*GetParameterCode(B));
		}
	}

	virtual int32 Sub(int32 A,int32 B) override
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(GetParameterUniformExpression(A),GetParameterUniformExpression(B),FMO_Sub),GetArithmeticResultType(A,B),TEXT("(%s - %s)"),*GetParameterCode(A),*GetParameterCode(B));
		}
		else
		{
			return AddCodeChunk(GetArithmeticResultType(A,B),TEXT("(%s - %s)"),*GetParameterCode(A),*GetParameterCode(B));
		}
	}

	virtual int32 Mul(int32 A,int32 B) override
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(GetParameterUniformExpression(A),GetParameterUniformExpression(B),FMO_Mul),GetArithmeticResultType(A,B),TEXT("(%s * %s)"),*GetParameterCode(A),*GetParameterCode(B));
		}
		else
		{
			return AddCodeChunk(GetArithmeticResultType(A,B),TEXT("(%s * %s)"),*GetParameterCode(A),*GetParameterCode(B));
		}
	}

	virtual int32 Div(int32 A,int32 B) override
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(GetParameterUniformExpression(A),GetParameterUniformExpression(B),FMO_Div),GetArithmeticResultType(A,B),TEXT("(%s / %s)"),*GetParameterCode(A),*GetParameterCode(B));
		}
		else
		{
			return AddCodeChunk(GetArithmeticResultType(A,B),TEXT("(%s / %s)"),*GetParameterCode(A),*GetParameterCode(B));
		}
	}

	virtual int32 Dot(int32 A,int32 B) override
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		FMaterialUniformExpression* ExpressionA = GetParameterUniformExpression(A);
		FMaterialUniformExpression* ExpressionB = GetParameterUniformExpression(B);

		EMaterialValueType TypeA = GetParameterType(A);
		EMaterialValueType TypeB = GetParameterType(B);
		if(ExpressionA && ExpressionB)
		{
			if (TypeA == MCT_Float && TypeB == MCT_Float)
			{
				return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(ExpressionA,ExpressionB,FMO_Mul),MCT_Float,TEXT("mul(%s,%s)"),*GetParameterCode(A),*GetParameterCode(B));
			}
			else
			{
				if (TypeA == TypeB)
				{
					return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(ExpressionA,ExpressionB,FMO_Dot),MCT_Float,TEXT("dot(%s,%s)"),*GetParameterCode(A),*GetParameterCode(B));
				}
				else
				{
					// Promote scalar (or truncate the bigger type)
					if (TypeA == MCT_Float || (TypeB != MCT_Float && GetNumComponents(TypeA) > GetNumComponents(TypeB)))
					{
						return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(ExpressionA,ExpressionB,FMO_Dot),MCT_Float,TEXT("dot(%s,%s)"),*CoerceParameter(A, TypeB),*GetParameterCode(B));
					}
					else
					{
						return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(ExpressionA,ExpressionB,FMO_Dot),MCT_Float,TEXT("dot(%s,%s)"),*GetParameterCode(A),*CoerceParameter(B, TypeA));
					}
				}
			}
		}
		else
		{
			// Promote scalar (or truncate the bigger type)
			if (TypeA == MCT_Float || (TypeB != MCT_Float && GetNumComponents(TypeA) > GetNumComponents(TypeB)))
			{
				return AddCodeChunk(MCT_Float,TEXT("dot(%s, %s)"), *CoerceParameter(A, TypeB), *GetParameterCode(B));
			}
			else
			{
				return AddCodeChunk(MCT_Float,TEXT("dot(%s, %s)"), *GetParameterCode(A), *CoerceParameter(B, TypeA));
			}
		}
	}

	virtual int32 Cross(int32 A,int32 B) override
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		return AddCodeChunk(MCT_Float3,TEXT("cross(%s,%s)"),*CoerceParameter(A,MCT_Float3),*CoerceParameter(B,MCT_Float3));
	}

	virtual int32 Power(int32 Base,int32 Exponent) override
	{
		if(Base == INDEX_NONE || Exponent == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		// use ClampedPow so artist are prevented to cause NAN creeping into the math
		return AddCodeChunk(GetParameterType(Base),TEXT("ClampedPow(%s,%s)"),*GetParameterCode(Base),*CoerceParameter(Exponent,MCT_Float));
	}
	
	virtual int32 Logarithm2(int32 X) override
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}
		
		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionLogarithm2(GetParameterUniformExpression(X)),GetParameterType(X),TEXT("log2(%s)"),*GetParameterCode(X));
		}

		return AddCodeChunk(GetParameterType(X),TEXT("log2(%s)"),*GetParameterCode(X));
	}

	virtual int32 SquareRoot(int32 X) override
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionSquareRoot(GetParameterUniformExpression(X)),GetParameterType(X),TEXT("sqrt(%s)"),*GetParameterCode(X));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),TEXT("sqrt(%s)"),*GetParameterCode(X));
		}
	}

	virtual int32 Length(int32 X) override
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionLength(GetParameterUniformExpression(X)),MCT_Float,TEXT("length(%s)"),*GetParameterCode(X));
		}
		else
		{
			return AddCodeChunk(MCT_Float,TEXT("length(%s)"),*GetParameterCode(X));
		}
	}

	virtual int32 Lerp(int32 X,int32 Y,int32 A) override
	{
		if(X == INDEX_NONE || Y == INDEX_NONE || A == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		FMaterialUniformExpression* ExpressionX = GetParameterUniformExpression(X);
		FMaterialUniformExpression* ExpressionY = GetParameterUniformExpression(Y);
		FMaterialUniformExpression* ExpressionA = GetParameterUniformExpression(A);
		bool bExpressionsAreEqual = false;

		// Skip over interpolations where inputs are equal
		if (X == Y)
		{
			bExpressionsAreEqual = true;
		}
		else if (ExpressionX && ExpressionY)
		{
			if (ExpressionX->IsConstant() && ExpressionY->IsConstant() && (*CurrentScopeChunks)[X].Type == (*CurrentScopeChunks)[Y].Type)
			{
				FLinearColor ValueX, ValueY;
				FMaterialRenderContext DummyContext(nullptr, *Material, nullptr);
				ExpressionX->GetNumberValue(DummyContext, ValueX);
				ExpressionY->GetNumberValue(DummyContext, ValueY);

				if (ValueX == ValueY)
				{
					bExpressionsAreEqual = true;
				}
			}
		}

		if (bExpressionsAreEqual)
		{
			return X;
		}

		EMaterialValueType ResultType = GetArithmeticResultType(X,Y);
		EMaterialValueType AlphaType = ResultType == (*CurrentScopeChunks)[A].Type ? ResultType : MCT_Float1;

		if (AlphaType == MCT_Float1 && ExpressionA && ExpressionA->IsConstant())
		{
			// Skip over interpolations that explicitly select an input
			FLinearColor Value;
			FMaterialRenderContext DummyContext(nullptr, *Material, nullptr);
			ExpressionA->GetNumberValue(DummyContext, Value);

			if (Value.R == 0.0f)
			{
				return X;
			}
			else if (Value.R == 1.f)
			{
				return Y;
			}
		}

		return AddCodeChunk(ResultType,TEXT("lerp(%s,%s,%s)"),*CoerceParameter(X,ResultType),*CoerceParameter(Y,ResultType),*CoerceParameter(A,AlphaType));
	}

	virtual int32 Min(int32 A,int32 B) override
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionMin(GetParameterUniformExpression(A),GetParameterUniformExpression(B)),GetParameterType(A),TEXT("min(%s,%s)"),*GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		}
		else
		{
			return AddCodeChunk(GetParameterType(A),TEXT("min(%s,%s)"),*GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		}
	}

	virtual int32 Max(int32 A,int32 B) override
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionMax(GetParameterUniformExpression(A),GetParameterUniformExpression(B)),GetParameterType(A),TEXT("max(%s,%s)"),*GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		}
		else
		{
			return AddCodeChunk(GetParameterType(A),TEXT("max(%s,%s)"),*GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		}
	}

	virtual int32 Clamp(int32 X,int32 A,int32 B) override
	{
		if(X == INDEX_NONE || A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X) && GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionClamp(GetParameterUniformExpression(X),GetParameterUniformExpression(A),GetParameterUniformExpression(B)),GetParameterType(X),TEXT("min(max(%s,%s),%s)"),*GetParameterCode(X),*CoerceParameter(A,GetParameterType(X)),*CoerceParameter(B,GetParameterType(X)));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),TEXT("min(max(%s,%s),%s)"),*GetParameterCode(X),*CoerceParameter(A,GetParameterType(X)),*CoerceParameter(B,GetParameterType(X)));
		}
	}

	virtual int32 Saturate(int32 X) override
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionSaturate(GetParameterUniformExpression(X)),GetParameterType(X),TEXT("saturate(%s)"),*GetParameterCode(X));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),TEXT("saturate(%s)"),*GetParameterCode(X));
		}
	}

	virtual int32 ComponentMask(int32 Vector,bool R,bool G,bool B,bool A) override
	{
		if(Vector == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		EMaterialValueType	VectorType = GetParameterType(Vector);

		if(	(A && (VectorType & MCT_Float) < MCT_Float4) ||
			(B && (VectorType & MCT_Float) < MCT_Float3) ||
			(G && (VectorType & MCT_Float) < MCT_Float2) ||
			(R && (VectorType & MCT_Float) < MCT_Float1))
		{
			return Errorf(TEXT("Not enough components in (%s: %s) for component mask %u%u%u%u"),*GetParameterCode(Vector),DescribeType(GetParameterType(Vector)),R,G,B,A);
		}

		EMaterialValueType	ResultType;
		switch((R ? 1 : 0) + (G ? 1 : 0) + (B ? 1 : 0) + (A ? 1 : 0))
		{
		case 1: ResultType = MCT_Float; break;
		case 2: ResultType = MCT_Float2; break;
		case 3: ResultType = MCT_Float3; break;
		case 4: ResultType = MCT_Float4; break;
		default: 
			return Errorf(TEXT("Couldn't determine result type of component mask %u%u%u%u"),R,G,B,A);
		};

		FString MaskString = FString::Printf(TEXT("%s%s%s%s"),
			R ? TEXT("r") : TEXT(""),
			// If VectorType is set to MCT_Float which means it could be any of the float types, assume it is a float1
			G ? (VectorType == MCT_Float ? TEXT("r") : TEXT("g")) : TEXT(""),
			B ? (VectorType == MCT_Float ? TEXT("r") : TEXT("b")) : TEXT(""),
			A ? (VectorType == MCT_Float ? TEXT("r") : TEXT("a")) : TEXT("")
			);

		auto* Expression = GetParameterUniformExpression(Vector);
		if (Expression)
		{
			int8 Mask[4] = {-1, -1, -1, -1};
			for (int32 Index = 0; Index < MaskString.Len(); ++Index)
			{
				Mask[Index] = SwizzleComponentToIndex(MaskString[Index]);
			}
			return AddUniformExpression(
				new FMaterialUniformExpressionComponentSwizzle(Expression, Mask[0], Mask[1], Mask[2], Mask[3]),
				ResultType,
				TEXT("%s.%s"),
				*GetParameterCode(Vector),
				*MaskString
				);
		}

		return AddInlinedCodeChunk(
			ResultType,
			TEXT("%s.%s"),
			*GetParameterCode(Vector),
			*MaskString
			);
	}

	virtual int32 AppendVector(int32 A,int32 B) override
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		int32 NumResultComponents = GetNumComponents(GetParameterType(A)) + GetNumComponents(GetParameterType(B));
		EMaterialValueType	ResultType = GetVectorType(NumResultComponents);

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionAppendVector(GetParameterUniformExpression(A),GetParameterUniformExpression(B),GetNumComponents(GetParameterType(A))),ResultType,TEXT("MaterialFloat%u(%s,%s)"),NumResultComponents,*GetParameterCode(A),*GetParameterCode(B));
		}
		else
		{
			return AddInlinedCodeChunk(ResultType,TEXT("MaterialFloat%u(%s,%s)"),NumResultComponents,*GetParameterCode(A),*GetParameterCode(B));
		}
	}

	int32 TransformBase(EMaterialCommonBasis SourceCoordBasis, EMaterialCommonBasis DestCoordBasis, int32 A, int AWComponent)
	{
		if (A == INDEX_NONE)
		{
			// unable to compile
			return INDEX_NONE;
		}
		
		{ // validation
			if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Domain && ShaderFrequency != SF_Vertex)
			{
				return NonPixelShaderExpressionError();
			}

			if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Vertex)
			{
				if ((SourceCoordBasis == MCB_Local || DestCoordBasis == MCB_Local))
				{
					return Errorf(TEXT("Local space in only supported for vertex, compute or pixel shader"));
				}
			}

			if (AWComponent != 0 && (SourceCoordBasis == MCB_Tangent || DestCoordBasis == MCB_Tangent))
			{
				return Errorf(TEXT("Tangent basis not available for position transformations"));
			}
		
			// Construct float3(0,0,x) out of the input if it is a scalar
			// This way artists can plug in a scalar and it will be treated as height, or a vector displacement
			if (GetType(A) == MCT_Float1 && SourceCoordBasis == MCB_Tangent)
			{
				A = AppendVector(Constant2(0, 0), A);
			}
			else if (GetNumComponents(GetParameterType(A)) < 3)
			{
				return Errorf(TEXT("input must be a vector (%s: %s) or a scalar (if source is Tangent)"), *GetParameterCode(A), DescribeType(GetParameterType(A)));
			}
		}
		
		if (SourceCoordBasis == DestCoordBasis)
		{
			// no transformation needed
			return A;
		}
		
		FString CodeStr;
		EMaterialCommonBasis IntermediaryBasis = MCB_World;

		switch (SourceCoordBasis)
		{
			case MCB_Tangent:
			{
				check(AWComponent == 0);
				if (DestCoordBasis == MCB_World)
				{
					if (ShaderFrequency == SF_Domain)
					{
						// domain shader uses a prescale value to preserve scaling factor on WorldTransform	when sampling a displacement map
						CodeStr = FString(TEXT("TransformTangent<TO>World_PreScaled(Parameters, <A>.xyz)"));
					}
					else
					{
						CodeStr = TEXT("mul(<A>, <MATRIX>(Parameters.TangentToWorld))");
					}
				}
				// else use MCB_World as intermediary basis
				break;
			}
			case MCB_Local:
			{
				if (DestCoordBasis == MCB_World)
				{
					 // TODO: need <PREV>
					CodeStr = TEXT("TransformLocal<TO>World(Parameters, <A>.xyz)");
				}
				// else use MCB_World as intermediary basis
				break;
			}
			case MCB_TranslatedWorld:
			{
				if (DestCoordBasis == MCB_World)
				{
					if (AWComponent)
					{
						CodeStr = TEXT("(<A>.xyz - ResolvedView.<PREV>PreViewTranslation.xyz)");
					}
					else
					{
						CodeStr = TEXT("<A>");
					}
				}
				else if (DestCoordBasis == MCB_Camera)
				{
					CodeStr = TEXT("mul(<A>, <MATRIX>(ResolvedView.<PREV>TranslatedWorldToCameraView))");
				}
				else if (DestCoordBasis == MCB_View)
				{
					CodeStr = TEXT("mul(<A>, <MATRIX>(ResolvedView.<PREV>TranslatedWorldToView))");
				}
				// else use MCB_World as intermediary basis
				break;
			}
			case MCB_World:
			{
				if (DestCoordBasis == MCB_Tangent)
				{
					CodeStr = TEXT("mul(<MATRIX>(Parameters.TangentToWorld), <A>)");
				}
				else if (DestCoordBasis == MCB_Local)
				{
					const EMaterialDomain Domain = (const EMaterialDomain)Material->GetMaterialDomain();

					if(Domain != MD_Surface)
					{
						// TODO: for decals we could support it
						Errorf(TEXT("This transformation is only supported in the 'Surface' material domain."));
						return INDEX_NONE;
					}

					// TODO: need Primitive.PrevWorldToLocal
					// TODO: inconsistent with TransformLocal<TO>World with instancing
					CodeStr = TEXT("mul(<A>, <MATRIX>(Primitive.WorldToLocal))");
				}
				else if (DestCoordBasis == MCB_TranslatedWorld)
				{
					if (AWComponent)
					{
						CodeStr = TEXT("(<A>.xyz + ResolvedView.<PREV>PreViewTranslation.xyz)");
					}
					else
					{
						CodeStr = TEXT("<A>");
					}
				}
				else if (DestCoordBasis == MCB_MeshParticle)
				{
					CodeStr = TEXT("mul(<A>, <MATRIX>(Parameters.Particle.LocalToWorld))");
					bUsesParticleTransform = true;
				}

				// else use MCB_TranslatedWorld as intermediary basis
				IntermediaryBasis = MCB_TranslatedWorld;
				break;
			}
			case MCB_Camera:
			{
				if (DestCoordBasis == MCB_TranslatedWorld)
				{
					CodeStr = TEXT("mul(<A>, <MATRIX>(ResolvedView.<PREV>CameraViewToTranslatedWorld))");
				}
				// else use MCB_TranslatedWorld as intermediary basis
				IntermediaryBasis = MCB_TranslatedWorld;
				break;
			}
			case MCB_View:
			{
				if (DestCoordBasis == MCB_TranslatedWorld)
				{
					CodeStr = TEXT("mul(<A>, <MATRIX>(ResolvedView.<PREV>ViewToTranslatedWorld))");
				}
				// else use MCB_TranslatedWorld as intermediary basis
				IntermediaryBasis = MCB_TranslatedWorld;
				break;
			}
			case MCB_MeshParticle:
			{
				if (DestCoordBasis == MCB_World)
				{
					CodeStr = TEXT("mul(<MATRIX>(Parameters.Particle.LocalToWorld), <A>)");
					bUsesParticleTransform = true;
				}
				else
				{
					return Errorf(TEXT("Can transform only to world space from particle space"));
				}
				break;
			}
			default:
				check(0);
				break;
		}

		if (CodeStr.IsEmpty())
		{
			// check intermediary basis so we don't have infinite recursion
			check(IntermediaryBasis != SourceCoordBasis);
			check(IntermediaryBasis != DestCoordBasis);

			// use intermediary basis
			const int32 IntermediaryA = TransformBase(SourceCoordBasis, IntermediaryBasis, A, AWComponent);

			return TransformBase(IntermediaryBasis, DestCoordBasis, IntermediaryA, AWComponent);
		}
		
		if (AWComponent != 0)
		{
			if (GetType(A) == MCT_Float3)
			{
				A = AppendVector(A, Constant(1));
			}
			CodeStr.ReplaceInline(TEXT("<TO>"),TEXT("PositionTo"));
			CodeStr.ReplaceInline(TEXT("<MATRIX>"),TEXT(""));
			CodeStr += ".xyz";
		}
		else
		{
			CodeStr.ReplaceInline(TEXT("<TO>"),TEXT("VectorTo"));
			CodeStr.ReplaceInline(TEXT("<MATRIX>"),TEXT("(MaterialFloat3x3)"));
		}
		
		if (bCompilingPreviousFrame)
		{
			CodeStr.ReplaceInline(TEXT("<PREV>"),TEXT("Prev"));
		}
		else
		{
			CodeStr.ReplaceInline(TEXT("<PREV>"),TEXT(""));
		}
		
		CodeStr.ReplaceInline(TEXT("<A>"), *GetParameterCode(A));

		return AddCodeChunk(
			MCT_Float3,
			*CodeStr
			);
	}
	
	virtual int32 TransformVector(EMaterialCommonBasis SourceCoordBasis, EMaterialCommonBasis DestCoordBasis, int32 A) override
	{
		return TransformBase(SourceCoordBasis, DestCoordBasis, A, 0);
	}

	virtual int32 TransformPosition(EMaterialCommonBasis SourceCoordBasis, EMaterialCommonBasis DestCoordBasis, int32 A) override
	{
		return TransformBase(SourceCoordBasis, DestCoordBasis, A, 1);
	}

	virtual int32 DynamicParameter(FLinearColor& DefaultValue) override
	{
		if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonVertexOrPixelShaderExpressionError();
		}

		bNeedsParticleDynamicParameter = true;

		int32 Default = Constant4(DefaultValue.R, DefaultValue.G, DefaultValue.B, DefaultValue.A);
		return AddInlinedCodeChunk(
			MCT_Float4,
			TEXT("GetDynamicParameter(Parameters.Particle, %s)"),
			*GetParameterCode(Default)
			);
	}

	virtual int32 LightmapUVs() override
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonPixelShaderExpressionError();
		}

		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM4) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		bUsesLightmapUVs = true;

		int32 ResultIdx = INDEX_NONE;
		FString CodeChunk = FString::Printf(TEXT("GetLightmapUVs(Parameters)"));
		ResultIdx = AddCodeChunk(
			MCT_Float2,
			*CodeChunk
			);
		return ResultIdx;
	}

	virtual int32 PrecomputedAOMask() override
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonPixelShaderExpressionError();
		}

		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM4) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		bUsesAOMaterialMask = true;

		int32 ResultIdx = INDEX_NONE;
		FString CodeChunk = FString::Printf(TEXT("Parameters.AOMaterialMask"));
		ResultIdx = AddCodeChunk(
			MCT_Float,
			*CodeChunk
			);
		return ResultIdx;
	}

	virtual int32 LightmassReplace(int32 Realtime, int32 Lightmass) override { return Realtime; }

	virtual int32 GIReplace(int32 Direct, int32 StaticIndirect, int32 DynamicIndirect) override 
	{ 
		if(Direct == INDEX_NONE || DynamicIndirect == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		EMaterialValueType ResultType = GetArithmeticResultType(Direct, DynamicIndirect);

		return AddCodeChunk(ResultType,TEXT("(GetGIReplaceState() ? (%s) : (%s))"), *GetParameterCode(DynamicIndirect), *GetParameterCode(Direct));
	}

	virtual int32 MaterialProxyReplace(int32 Realtime, int32 MaterialProxy) override { return Realtime; }

	virtual int32 ObjectOrientation() override
	{ 
		return GetPrimitiveProperty(MCT_Float3, TEXT("ObjectOrientation"), TEXT("ObjectOrientation.xyz"));
	}

	virtual int32 RotateAboutAxis(int32 NormalizedRotationAxisAndAngleIndex, int32 PositionOnAxisIndex, int32 PositionIndex) override
	{
		if (NormalizedRotationAxisAndAngleIndex == INDEX_NONE
			|| PositionOnAxisIndex == INDEX_NONE
			|| PositionIndex == INDEX_NONE)
		{
			return INDEX_NONE;
		}
		else
		{
			return AddCodeChunk(
				MCT_Float3,
				TEXT("RotateAboutAxis(%s,%s,%s)"),
				*CoerceParameter(NormalizedRotationAxisAndAngleIndex,MCT_Float4),
				*CoerceParameter(PositionOnAxisIndex,MCT_Float3),
				*CoerceParameter(PositionIndex,MCT_Float3)
				);	
		}
	}

	virtual int32 TwoSidedSign() override
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonPixelShaderExpressionError();
		}
		return AddInlinedCodeChunk(MCT_Float,TEXT("Parameters.TwoSidedSign"));	
	}

	virtual int32 VertexNormal() override
	{
		if (ShaderFrequency != SF_Vertex)
		{
			bUsesTransformVector = true;
		}
		return AddInlinedCodeChunk(MCT_Float3,TEXT("Parameters.TangentToWorld[2]"));	
	}

	virtual int32 PixelNormalWS() override
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonPixelShaderExpressionError();
		}
		if(MaterialProperty == MP_Normal)
		{
			return Errorf(TEXT("Invalid node PixelNormalWS used for Normal input."));
		}
		if (ShaderFrequency != SF_Vertex)
		{
			bUsesTransformVector = true;
		}
		return AddInlinedCodeChunk(MCT_Float3,TEXT("Parameters.WorldNormal"));	
	}

	virtual int32 DDX( int32 X ) override
	{
		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::ES3_1) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (ShaderFrequency == SF_Compute)
		{
			// running a material in a compute shader pass (e.g. when using SVOGI)
			return AddInlinedCodeChunk(MCT_Float, TEXT("0"));	
		}

		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}

		return AddCodeChunk(GetParameterType(X),TEXT("DDX(%s)"),*GetParameterCode(X));
	}

	virtual int32 DDY( int32 X ) override
	{
		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::ES3_1) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (ShaderFrequency == SF_Compute)
		{
			// running a material in a compute shader pass
			return AddInlinedCodeChunk(MCT_Float, TEXT("0"));	
		}
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}

		return AddCodeChunk(GetParameterType(X),TEXT("DDY(%s)"),*GetParameterCode(X));
	}

	virtual int32 AntialiasedTextureMask(int32 Tex, int32 UV, float Threshold, uint8 Channel) override
	{
		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM4) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (Tex == INDEX_NONE || UV == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		int32 ThresholdConst = Constant(Threshold);
		int32 ChannelConst = Constant(Channel);
		FString TextureName = CoerceParameter(Tex, GetParameterType(Tex));

		return AddCodeChunk(MCT_Float, 
			TEXT("AntialiasedTextureMask(%s,%sSampler,%s,%s,%s)"), 
			*GetParameterCode(Tex),
			*TextureName,
			*GetParameterCode(UV),
			*GetParameterCode(ThresholdConst),
			*GetParameterCode(ChannelConst));
	}

	virtual int32 DepthOfFieldFunction(int32 Depth, int32 FunctionValueIndex) override
	{
		if (ShaderFrequency == SF_Hull)
		{
			return Errorf(TEXT("Invalid node DepthOfFieldFunction used in hull shader input!"));
		}

		if (Depth == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		return AddCodeChunk(MCT_Float, 
			TEXT("MaterialExpressionDepthOfFieldFunction(%s, %d)"), 
			*GetParameterCode(Depth), FunctionValueIndex);
	}

	virtual int32 Noise(int32 Position, float Scale, int32 Quality, uint8 NoiseFunction, bool bTurbulence, int32 Levels, float OutputMin, float OutputMax, float LevelScale, int32 FilterWidth, bool bTiling, uint32 RepeatSize) override
	{
		// GradientTex3D uses 3D texturing, which is not available on ES2
		if (NoiseFunction == NOISEFUNCTION_GradientTex3D)
		{
			if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM4) == INDEX_NONE)
			{
				Errorf(TEXT("3D textures are not supported for ES2"));
				return INDEX_NONE;
			}
		}
		// all others are fine for ES2 feature level
		else if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::ES2) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(Position == INDEX_NONE || FilterWidth == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		// to limit performance problems due to values outside reasonable range
		Levels = FMath::Clamp(Levels, 1, 10);

		int32 ScaleConst = Constant(Scale);
		int32 QualityConst = Constant(Quality);
		int32 NoiseFunctionConst = Constant(NoiseFunction);
		int32 TurbulenceConst = Constant(bTurbulence);
		int32 LevelsConst = Constant(Levels);
		int32 OutputMinConst = Constant(OutputMin);
		int32 OutputMaxConst = Constant(OutputMax);
		int32 LevelScaleConst = Constant(LevelScale);
		int32 TilingConst = Constant(bTiling);
		int32 RepeatSizeConst = Constant(RepeatSize);

		return AddCodeChunk(MCT_Float, 
			TEXT("MaterialExpressionNoise(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)"), 
			*GetParameterCode(Position),
			*GetParameterCode(ScaleConst),
			*GetParameterCode(QualityConst),
			*GetParameterCode(NoiseFunctionConst),
			*GetParameterCode(TurbulenceConst),
			*GetParameterCode(LevelsConst),
			*GetParameterCode(OutputMinConst),
			*GetParameterCode(OutputMaxConst),
			*GetParameterCode(LevelScaleConst),
			*GetParameterCode(FilterWidth),
			*GetParameterCode(TilingConst),
			*GetParameterCode(RepeatSizeConst));
	}

	virtual int32 BlackBody( int32 Temp ) override
	{
		if( Temp == INDEX_NONE )
		{
			return INDEX_NONE;
		}

		return AddCodeChunk( MCT_Float3, TEXT("MaterialExpressionBlackBody(%s)"), *GetParameterCode(Temp) );
	}

	virtual int32 DistanceToNearestSurface(int32 PositionArg) override
	{
		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM5) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (PositionArg == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		MaterialCompilationOutput.bUsesGlobalDistanceField = true;

		return AddCodeChunk(MCT_Float, TEXT("GetDistanceToNearestSurfaceGlobal(%s)"), *GetParameterCode(PositionArg));
	}

	virtual int32 DistanceFieldGradient(int32 PositionArg) override
	{
		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM5) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (PositionArg == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		MaterialCompilationOutput.bUsesGlobalDistanceField = true;

		return AddCodeChunk(MCT_Float3, TEXT("GetDistanceFieldGradientGlobal(%s)"), *GetParameterCode(PositionArg));
	}

	virtual int32 AtmosphericFogColor( int32 WorldPosition ) override
	{
		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM4) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		bUsesAtmosphericFog = true;
		if( WorldPosition == INDEX_NONE )
		{
			return AddCodeChunk( MCT_Float4, TEXT("MaterialExpressionAtmosphericFog(Parameters, Parameters.AbsoluteWorldPosition)"));
		}
		else
		{
			return AddCodeChunk( MCT_Float4, TEXT("MaterialExpressionAtmosphericFog(Parameters, %s)"), *GetParameterCode(WorldPosition) );
		}
	}

	virtual int32 AtmosphericLightVector() override
	{
		bUsesAtmosphericFog = true;

		return AddCodeChunk(MCT_Float3, TEXT("MaterialExpressionAtmosphericLightVector(Parameters)"));

	}

	virtual int32 AtmosphericLightColor() override
	{
		bUsesAtmosphericFog = true;

		return AddCodeChunk(MCT_Float3, TEXT("MaterialExpressionAtmosphericLightColor(Parameters)"));

	}

	virtual int32 CustomExpression( class UMaterialExpressionCustom* Custom, TArray<int32>& CompiledInputs ) override
	{
		int32 ResultIdx = INDEX_NONE;

		FString OutputTypeString;
		EMaterialValueType OutputType;
		switch( Custom->OutputType )
		{
		case CMOT_Float2:
			OutputType = MCT_Float2;
			OutputTypeString = TEXT("MaterialFloat2");
			break;
		case CMOT_Float3:
			OutputType = MCT_Float3;
			OutputTypeString = TEXT("MaterialFloat3");
			break;
		case CMOT_Float4:
			OutputType = MCT_Float4;
			OutputTypeString = TEXT("MaterialFloat4");
			break;
		default:
			OutputType = MCT_Float;
			OutputTypeString = TEXT("MaterialFloat");
			break;
		}

		// Declare implementation function
		FString InputParamDecl;
		check( Custom->Inputs.Num() == CompiledInputs.Num() );
		for( int32 i = 0; i < Custom->Inputs.Num(); i++ )
		{
			// skip over unnamed inputs
			if( Custom->Inputs[i].InputName.Len()==0 )
			{
				continue;
			}
			InputParamDecl += TEXT(",");
			switch(GetParameterType(CompiledInputs[i]))
			{
			case MCT_Float:
			case MCT_Float1:
				InputParamDecl += TEXT("MaterialFloat ");
				InputParamDecl += Custom->Inputs[i].InputName;
				break;
			case MCT_Float2:
				InputParamDecl += TEXT("MaterialFloat2 ");
				InputParamDecl += Custom->Inputs[i].InputName;
				break;
			case MCT_Float3:
				InputParamDecl += TEXT("MaterialFloat3 ");
				InputParamDecl += Custom->Inputs[i].InputName;
				break;
			case MCT_Float4:
				InputParamDecl += TEXT("MaterialFloat4 ");
				InputParamDecl += Custom->Inputs[i].InputName;
				break;
			case MCT_Texture2D:
				InputParamDecl += TEXT("Texture2D ");
				InputParamDecl += Custom->Inputs[i].InputName;
				InputParamDecl += TEXT(", SamplerState ");
				InputParamDecl += Custom->Inputs[i].InputName;
				InputParamDecl += TEXT("Sampler ");
				break;
			case MCT_TextureCube:
				InputParamDecl += TEXT("TextureCube ");
				InputParamDecl += Custom->Inputs[i].InputName;
				InputParamDecl += TEXT(", SamplerState ");
				InputParamDecl += Custom->Inputs[i].InputName;
				InputParamDecl += TEXT("Sampler ");
				break;
			default:
				return Errorf(TEXT("Bad type %s for %s input %s"),DescribeType(GetParameterType(CompiledInputs[i])), *Custom->Description, *Custom->Inputs[i].InputName);
				break;
			}
		}
		int32 CustomExpressionIndex = CustomExpressionImplementations.Num();
		FString Code = Custom->Code;
		if( !Code.Contains(TEXT("return")) )
		{
			Code = FString(TEXT("return "))+Code+TEXT(";");
		}
		Code.ReplaceInline(TEXT("\n"),TEXT("\r\n"), ESearchCase::CaseSensitive);

		FString ParametersType = ShaderFrequency == SF_Vertex ? TEXT("Vertex") : (ShaderFrequency == SF_Domain ? TEXT("Tessellation") : TEXT("Pixel"));

		FString ImplementationCode = FString::Printf(TEXT("%s CustomExpression%d(FMaterial%sParameters Parameters%s)\r\n{\r\n%s\r\n}\r\n"), *OutputTypeString, CustomExpressionIndex, *ParametersType, *InputParamDecl, *Code);
		CustomExpressionImplementations.Add( ImplementationCode );

		// Add call to implementation function
		FString CodeChunk = FString::Printf(TEXT("CustomExpression%d(Parameters"), CustomExpressionIndex);
		for( int32 i = 0; i < CompiledInputs.Num(); i++ )
		{
			// skip over unnamed inputs
			if( Custom->Inputs[i].InputName.Len()==0 )
			{
				continue;
			}

			FString ParamCode = GetParameterCode(CompiledInputs[i]);
			EMaterialValueType ParamType = GetParameterType(CompiledInputs[i]);

			CodeChunk += TEXT(",");
			CodeChunk += *ParamCode;
			if (ParamType == MCT_Texture2D || ParamType == MCT_TextureCube)
			{
				CodeChunk += TEXT(",");
				CodeChunk += *ParamCode;
				CodeChunk += TEXT("Sampler");
			}
		}
		CodeChunk += TEXT(")");

		ResultIdx = AddCodeChunk(
			OutputType,
			*CodeChunk
			);
		return ResultIdx;
	}

	virtual int32 CustomOutput(class UMaterialExpressionCustomOutput* Custom, int32 OutputIndex, int32 OutputCode) override
	{
		if (MaterialProperty != MP_MAX)
		{
			return Errorf(TEXT("A Custom Output node should not be attached to the %s material property"), *GetNameOfMaterialProperty(MaterialProperty));
		}

		if (OutputCode == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		EMaterialValueType OutputType = GetParameterType(OutputCode);
		FString OutputTypeString;
		switch (OutputType)
		{
			case MCT_Float:
			case MCT_Float1:
				OutputTypeString = TEXT("MaterialFloat");
				break;
			case MCT_Float2:
				OutputTypeString += TEXT("MaterialFloat2");
				break;
			case MCT_Float3:
				OutputTypeString += TEXT("MaterialFloat3");
				break;
			case MCT_Float4:
				OutputTypeString += TEXT("MaterialFloat4");
				break;
			default:
				return Errorf(TEXT("Bad type %s for %s"), DescribeType(GetParameterType(OutputCode)), *Custom->GetDescription());
				break;
		}

		FString Definitions;
		FString Body;

		if ((*CurrentScopeChunks)[OutputCode].UniformExpression && !(*CurrentScopeChunks)[OutputCode].UniformExpression->IsConstant())
		{
			Body = GetParameterCode(OutputCode);
		}
		else
		{
			GetFixedParameterCode(OutputCode, *CurrentScopeChunks, Definitions, Body);
		}

		FString ImplementationCode = FString::Printf(TEXT("%s %s%d(FMaterial%sParameters Parameters)\r\n{\r\n%s return %s;\r\n}\r\n"), *OutputTypeString, *Custom->GetFunctionName(), OutputIndex, ShaderFrequency == SF_Vertex ? TEXT("Vertex") : TEXT("Pixel"), *Definitions, *Body);
		CustomOutputImplementations.Add(ImplementationCode);

		// return value is not used
		return INDEX_NONE;
	}

	/**
	 * Adds code to return a random value shared by all geometry for any given instanced static mesh
	 *
	 * @return	Code index
	 */
	virtual int32 PerInstanceRandom() override
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Vertex)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		else
		{
			return AddInlinedCodeChunk(MCT_Float, TEXT("GetPerInstanceRandom(Parameters)"));
		}
	}

	/**
	 * Returns a mask that either enables or disables selection on a per-instance basis when instancing
	 *
	 * @return	Code index
	 */
	virtual int32 PerInstanceFadeAmount() override
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Vertex)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		else
		{
			return AddInlinedCodeChunk(MCT_Float, TEXT("GetPerInstanceFadeAmount(Parameters)"));
		}
	}

	/**
	* Handles SpeedTree vertex animation (wind, smooth LOD)
	*
	* @return	Code index
	*/
	virtual int32 SpeedTree(ESpeedTreeGeometryType GeometryType, ESpeedTreeWindType WindType, ESpeedTreeLODType LODType, float BillboardThreshold, bool bAccurateWindVelocities) override 
	{ 
		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM4) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (ShaderFrequency != SF_Vertex)
		{
			return NonVertexShaderExpressionError();
		}
		else
		{
			bUsesSpeedTree = true;

			NumUserVertexTexCoords = FMath::Max<uint32>(NumUserVertexTexCoords, 8);
			// Only generate previous frame's computations if required and opted-in
			const bool bEnablePreviousFrameInformation = bCompilingPreviousFrame && bAccurateWindVelocities;
			return AddCodeChunk(MCT_Float3, TEXT("GetSpeedTreeVertexOffset(Parameters, %d, %d, %d, %g, %s)"), GeometryType, WindType, LODType, BillboardThreshold, bEnablePreviousFrameInformation ? TEXT("true") : TEXT("false"));
		}
	}

	/**
	 * Adds code for texture coordinate offset to localize large UV
	 *
	 * @return	Code index
	 */
	virtual int32 TextureCoordinateOffset() override
	{
		if (FeatureLevel < ERHIFeatureLevel::SM4 && ShaderFrequency == SF_Vertex)
		{
			return AddInlinedCodeChunk(MCT_Float2, TEXT("Parameters.TexCoordOffset"));
		}
		else
		{
			return Constant(0.f);
		}
	}

	/**Experimental access to the EyeAdaptation RT for Post Process materials. Can be one frame behind depending on the value of BlendableLocation. */
	virtual int32 EyeAdaptation() override
	{
		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM5) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if( ShaderFrequency != SF_Pixel )
		{
			NonPixelShaderExpressionError();
		}

		MaterialCompilationOutput.bUsesEyeAdaptation = true;

		return AddInlinedCodeChunk(MCT_Float, TEXT("EyeAdaptationLookup()"));
	}

	// to only have one piece of code dealing with error handling if the Primitive constant buffer is not used.
	// @param Name e.g. TEXT("ObjectWorldPositionAndRadius.w")
	int32 GetPrimitiveProperty(EMaterialValueType Type, const TCHAR* ExpressionName, const TCHAR* HLSLName)
	{
		const EMaterialDomain Domain = (const EMaterialDomain)Material->GetMaterialDomain();

		if(Domain != MD_Surface)
		{
			Errorf(TEXT("The material expression '%s' is only supported in the 'Surface' material domain."), ExpressionName);
			return INDEX_NONE;
		}

		return AddInlinedCodeChunk(Type, TEXT("Primitive.%s"), HLSLName);
	}

	// The compiler can run in a different state and this affects caching of sub expression, Expressions are different (e.g. View.PrevWorldViewOrigin) when using previous frame's values
	virtual bool IsCurrentlyCompilingForPreviousFrame() const { return bCompilingPreviousFrame; }
};

#endif
