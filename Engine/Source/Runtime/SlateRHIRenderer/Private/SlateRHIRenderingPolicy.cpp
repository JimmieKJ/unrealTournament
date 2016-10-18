// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateRHIRendererPrivatePCH.h"
#include "RenderingPolicy.h"
#include "SlateRHIRenderingPolicy.h"
#include "SlateShaders.h"
#include "SlateMaterialShader.h"
#include "SlateRHIResourceManager.h"
#include "PreviewScene.h"
#include "EngineModule.h"
#include "SlateUTextureResource.h"
#include "SlateMaterialResource.h"
#include "Rendering/DrawElements.h"
#include "SlateUpdatableBuffer.h"
#include "SlateElementIndexBuffer.h"
#include "SlateElementVertexBuffer.h"

extern void UpdateNoiseTextureParameters(FViewUniformShaderParameters& ViewUniformShaderParameters);

DECLARE_CYCLE_STAT(TEXT("Update Buffers RT"), STAT_SlateUpdateBufferRTTime, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("PreFill Buffers RT"), STAT_SlatePreFullBufferRTTime, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Layers"), STAT_SlateNumLayers, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Batches"), STAT_SlateNumBatches, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Vertices"), STAT_SlateVertexCount, STATGROUP_Slate);

FSlateRHIRenderingPolicy::FSlateRHIRenderingPolicy(TSharedRef<FSlateFontServices> InSlateFontServices, TSharedRef<FSlateRHIResourceManager> InResourceManager, TOptional<int32> InitialBufferSize)
	: FSlateRenderingPolicy(InSlateFontServices, 0)
	, ResourceManager(InResourceManager)
	, bGammaCorrect(true)
	, InitialBufferSizeOverride(InitialBufferSize)
{
	InitResources();
}

void FSlateRHIRenderingPolicy::InitResources()
{
	int32 NumVertices = 100;

	if ( InitialBufferSizeOverride.IsSet() )
	{
		NumVertices = InitialBufferSizeOverride.GetValue();
	}
	else if ( GConfig )
	{
		int32 NumVertsInConfig = 0;
		if ( GConfig->GetInt(TEXT("SlateRenderer"), TEXT("NumPreallocatedVertices"), NumVertsInConfig, GEngineIni) )
		{
			NumVertices = NumVertsInConfig;
		}
	}

	// Always create a little space but never allow it to get too high
#if !SLATE_USE_32BIT_INDICES
	NumVertices = FMath::Clamp(NumVertices, 100, 65535);
#else
	NumVertices = FMath::Clamp(NumVertices, 100, 1000000);
#endif

	UE_LOG(LogSlate, Verbose, TEXT("Allocating space for %d vertices"), NumVertices);

	VertexBuffers.Init(NumVertices);
	IndexBuffers.Init(NumVertices);
}

void FSlateRHIRenderingPolicy::ReleaseResources()
{
	VertexBuffers.Destroy();
	IndexBuffers.Destroy();
}

void FSlateRHIRenderingPolicy::BeginDrawingWindows()
{
	check( IsInRenderingThread() );
}

void FSlateRHIRenderingPolicy::EndDrawingWindows()
{
	check( IsInParallelRenderingThread() );
}

struct FSlateUpdateVertexAndIndexBuffers : public FRHICommand<FSlateUpdateVertexAndIndexBuffers>
{
	FVertexBufferRHIRef VertexBufferRHI;
	FIndexBufferRHIRef IndexBufferRHI;
	FSlateBatchData& BatchData;

	FSlateUpdateVertexAndIndexBuffers(TSlateElementVertexBuffer<FSlateVertex>& InVertexBuffer, FSlateElementIndexBuffer& InIndexBuffer, FSlateBatchData& InBatchData)
		: VertexBufferRHI(InVertexBuffer.VertexBufferRHI)
		, IndexBufferRHI(InIndexBuffer.IndexBufferRHI)
		, BatchData(InBatchData)
	{
		check(IsInRenderingThread());
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		SCOPE_CYCLE_COUNTER(STAT_SlateUpdateBufferRTTime);

		const int32 NumBatchedVertices = BatchData.GetNumBatchedVertices();
		const int32 NumBatchedIndices = BatchData.GetNumBatchedIndices();

		int32 RequiredVertexBufferSize = NumBatchedVertices*sizeof(FSlateVertex);
		uint8* VertexBufferData = (uint8*)GDynamicRHI->RHILockVertexBuffer( VertexBufferRHI, 0, RequiredVertexBufferSize, RLM_WriteOnly );

		uint32 RequiredIndexBufferSize = NumBatchedIndices*sizeof(SlateIndex);		
		uint8* IndexBufferData = (uint8*)GDynamicRHI->RHILockIndexBuffer( IndexBufferRHI, 0, RequiredIndexBufferSize, RLM_WriteOnly );

		BatchData.FillVertexAndIndexBuffer( VertexBufferData, IndexBufferData );

		GDynamicRHI->RHIUnlockVertexBuffer( VertexBufferRHI );
		GDynamicRHI->RHIUnlockIndexBuffer( IndexBufferRHI );
	}
};

void FSlateRHIRenderingPolicy::UpdateVertexAndIndexBuffers(FRHICommandListImmediate& RHICmdList, FSlateBatchData& InBatchData)
{
	UpdateVertexAndIndexBuffers(RHICmdList, InBatchData, VertexBuffers, IndexBuffers);
}

void FSlateRHIRenderingPolicy::UpdateVertexAndIndexBuffers(FRHICommandListImmediate& RHICmdList, FSlateBatchData& InBatchData, const TSharedRef<FSlateRenderDataHandle, ESPMode::ThreadSafe>& RenderHandle)
{
	// Should only be called by the rendering thread
	check(IsInRenderingThread());

	FCachedRenderBuffers* Buffers = ResourceManager->FindOrCreateCachedBuffersForHandle(RenderHandle);

	UpdateVertexAndIndexBuffers(RHICmdList, InBatchData, Buffers->VertexBuffer, Buffers->IndexBuffer);
}

void FSlateRHIRenderingPolicy::ReleaseCachingResourcesFor(FRHICommandListImmediate& RHICmdList, const ILayoutCache* Cacher)
{
	ResourceManager->ReleaseCachingResourcesFor(RHICmdList, Cacher);
}

void FSlateRHIRenderingPolicy::UpdateVertexAndIndexBuffers(FRHICommandListImmediate& RHICmdList, FSlateBatchData& InBatchData, TSlateElementVertexBuffer<FSlateVertex>& VertexBuffer, FSlateElementIndexBuffer& IndexBuffer)
{
	SCOPE_CYCLE_COUNTER( STAT_SlateUpdateBufferRTTime );

	// Should only be called by the rendering thread
	check(IsInRenderingThread());

	const int32 NumVertices = InBatchData.GetNumBatchedVertices();
	const int32 NumIndices = InBatchData.GetNumBatchedIndices();

	if( InBatchData.GetRenderBatches().Num() > 0  && NumVertices > 0 && NumIndices > 0)
	{
		bool bShouldShrinkResources = false;

		VertexBuffer.PreFillBuffer(NumVertices, bShouldShrinkResources);
		IndexBuffer.PreFillBuffer(NumIndices, bShouldShrinkResources);

		if(!GRHIThread || RHICmdList.Bypass())
		{
			uint8* VertexBufferData = (uint8*)VertexBuffer.LockBuffer_RenderThread(NumVertices);
			uint8* IndexBufferData =  (uint8*)IndexBuffer.LockBuffer_RenderThread(NumIndices);

			InBatchData.FillVertexAndIndexBuffer( VertexBufferData, IndexBufferData );
	
			VertexBuffer.UnlockBuffer_RenderThread();
			IndexBuffer.UnlockBuffer_RenderThread();
		}
		else
		{
			new (RHICmdList.AllocCommand<FSlateUpdateVertexAndIndexBuffers>()) FSlateUpdateVertexAndIndexBuffers(VertexBuffer, IndexBuffer, InBatchData);
		}
	}

	checkSlow(VertexBuffer.GetBufferUsageSize() <= VertexBuffer.GetBufferSize());
	checkSlow(IndexBuffer.GetBufferUsageSize() <= IndexBuffer.GetBufferSize());

	SET_DWORD_STAT( STAT_SlateNumLayers, InBatchData.GetNumLayers() );
	SET_DWORD_STAT( STAT_SlateNumBatches, InBatchData.GetRenderBatches().Num() );
	SET_DWORD_STAT( STAT_SlateVertexCount, InBatchData.GetNumBatchedVertices() );
}

static FSceneView& CreateSceneView( FSceneViewFamilyContext& ViewFamilyContext, FSlateBackBuffer& BackBuffer, const FMatrix& ViewProjectionMatrix )
{
	FIntRect ViewRect(FIntPoint(0, 0), BackBuffer.GetSizeXY());

	// make a temporary view
	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = &ViewFamilyContext;
	ViewInitOptions.SetViewRectangle(ViewRect);
	ViewInitOptions.ViewOrigin = FVector::ZeroVector;
	ViewInitOptions.ViewRotationMatrix = FMatrix::Identity;
	ViewInitOptions.ProjectionMatrix = ViewProjectionMatrix;
	ViewInitOptions.BackgroundColor = FLinearColor::Black;
	ViewInitOptions.OverlayColor = FLinearColor::White;

	FSceneView* View = new FSceneView( ViewInitOptions );
	ViewFamilyContext.Views.Add( View );

	/** The view transform, starting from world-space points translated by -ViewOrigin. */
	FMatrix EffectiveTranslatedViewMatrix = FTranslationMatrix(-View->ViewMatrices.PreViewTranslation) * View->ViewMatrices.ViewMatrix;

	const FIntPoint BufferSize = BackBuffer.GetSizeXY();

	// Create the view's uniform buffer.
	FViewUniformShaderParameters ViewUniformShaderParameters;

	View->SetupCommonViewUniformBufferParameters(
		ViewUniformShaderParameters, 
		BufferSize,
		ViewRect,
		EffectiveTranslatedViewMatrix, 
		View->InvViewMatrix * FTranslationMatrix(View->ViewMatrices.PreViewTranslation),
		FViewMatrices(), 
		FMatrix::Identity,
		FMatrix::Identity);

	ViewUniformShaderParameters.WorldViewOrigin = View->ViewMatrices.ViewOrigin;

	UpdateNoiseTextureParameters(ViewUniformShaderParameters);

	View->ViewUniformBuffer = TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(ViewUniformShaderParameters, UniformBuffer_SingleFrame);

	return *View;
}

void FSlateRHIRenderingPolicy::DrawElements(FRHICommandListImmediate& RHICmdList, FSlateBackBuffer& BackBuffer, const FMatrix& ViewProjectionMatrix, const TArray<FSlateRenderBatch>& RenderBatches, bool bAllowSwitchVerticalAxis)
{
	// Should only be called by the rendering thread
	check(IsInRenderingThread());

	float TimeSeconds = FApp::GetCurrentTime() - GStartTime;
	float RealTimeSeconds =  FApp::GetCurrentTime() - GStartTime;
	float DeltaTimeSeconds = FApp::GetDeltaTime();

	static const FEngineShowFlags DefaultShowFlags(ESFIM_Game);

	const float DisplayGamma = bGammaCorrect ? GEngine ? GEngine->GetDisplayGamma() : 2.2f : 1.0f;

	FSceneViewFamilyContext SceneViewContext
	(
		FSceneViewFamily::ConstructionValues
		(
			&BackBuffer,
			nullptr,
			DefaultShowFlags
		)
		.SetWorldTimes( TimeSeconds, DeltaTimeSeconds, RealTimeSeconds )
		.SetGammaCorrection( DisplayGamma )
	);

	FSceneView* SceneView = NULL;

	TShaderMapRef<FSlateElementVS> GlobalVertexShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	FSamplerStateRHIRef BilinearClamp = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

	TSlateElementVertexBuffer<FSlateVertex>* VertexBuffer = &VertexBuffers;
	FSlateElementIndexBuffer* IndexBuffer = &IndexBuffers;

	// Disable depth/stencil testing by default
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	const FSlateRenderDataHandle* LastHandle = nullptr;

	// Draw each element
	for( int32 BatchIndex = 0; BatchIndex < RenderBatches.Num(); ++BatchIndex )
	{
		const FSlateRenderBatch& RenderBatch = RenderBatches[BatchIndex];
		const FSlateRenderDataHandle* RenderHandle = RenderBatch.CachedRenderHandle.Get();

		if ( RenderHandle != LastHandle )
		{
			LastHandle = RenderHandle;

			if ( RenderHandle )
			{
				FCachedRenderBuffers* Buffers = ResourceManager->FindCachedBuffersForHandle(RenderHandle);
				if ( Buffers != nullptr )
				{
					VertexBuffer = &Buffers->VertexBuffer;
					IndexBuffer = &Buffers->IndexBuffer;
				}
			}
			else
			{
				VertexBuffer = &VertexBuffers;
				IndexBuffer = &IndexBuffers;
			}

			checkSlow(VertexBuffer);
			checkSlow(IndexBuffer);
		}

		const FSlateShaderResource* ShaderResource = RenderBatch.Texture;
		const ESlateBatchDrawFlag::Type DrawFlags = RenderBatch.DrawFlags;
		const ESlateDrawEffect::Type DrawEffects = RenderBatch.DrawEffects;
		const ESlateShader::Type ShaderType = RenderBatch.ShaderType;
		const FShaderParams& ShaderParams = RenderBatch.ShaderParams;

		if( !RenderBatch.CustomDrawer.IsValid() )
		{
			check(RenderBatch.NumIndices > 0);

			FMatrix DynamicOffset = FTranslationMatrix::Make(FVector(RenderBatch.DynamicOffset.X, RenderBatch.DynamicOffset.Y, 0));

			if (RenderBatch.ScissorRect.IsSet())
			{
				RHICmdList.SetScissorRect(true, RenderBatch.ScissorRect.GetValue().Left, RenderBatch.ScissorRect.GetValue().Top, RenderBatch.ScissorRect.GetValue().Right, RenderBatch.ScissorRect.GetValue().Bottom);
			}
			else
			{
				RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
			}

			ESlateShaderResource::Type ResourceType = ShaderResource ? ShaderResource->GetType() : ESlateShaderResource::Invalid;
			if( ResourceType != ESlateShaderResource::Material ) 
			{
				FSlateElementPS* PixelShader = GetTexturePixelShader(ShaderType, DrawEffects);

				const bool bUseInstancing = RenderBatch.InstanceCount > 1 && RenderBatch.InstanceData != nullptr;
				check(bUseInstancing == false);
				
				RHICmdList.SetLocalBoundShaderState(RHICmdList.BuildLocalBoundShaderState(
					GSlateVertexDeclaration.VertexDeclarationRHI,
					GlobalVertexShader->GetVertexShader(),
					nullptr,
					nullptr,
					PixelShader->GetPixelShader(),
					FGeometryShaderRHIRef()));


				GlobalVertexShader->SetViewProjection(RHICmdList, DynamicOffset * ViewProjectionMatrix);
				GlobalVertexShader->SetVerticalAxisMultiplier(RHICmdList, bAllowSwitchVerticalAxis && RHINeedsToSwitchVerticalAxis(GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel]) ? -1.0f : 1.0f );

#if !DEBUG_OVERDRAW
				RHICmdList.SetBlendState(
					( RenderBatch.DrawFlags & ESlateBatchDrawFlag::NoBlending )
					? TStaticBlendState<>::GetRHI()
#if SLATE_PRE_MULTIPLY
					: ( ( RenderBatch.DrawFlags & ESlateBatchDrawFlag::PreMultipliedAlpha )
						? TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI()
						: TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI() )
#else
					: TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI()
#endif
					);
#else
				RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
#endif

				if (DrawFlags & ESlateBatchDrawFlag::Wireframe)
				{
					RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Wireframe, CM_None, false>::GetRHI());
				}
				else
				{
					RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None, false>::GetRHI());
				}

				FSamplerStateRHIParamRef SamplerState = BilinearClamp;
				FTextureRHIParamRef TextureRHI = GWhiteTexture->TextureRHI;
				if( ShaderResource )
				{
					TextureFilter Filter = TF_Bilinear;

					if (ResourceType == ESlateShaderResource::TextureObject)
					{
						FSlateBaseUTextureResource* TextureObjectResource = (FSlateBaseUTextureResource*)ShaderResource;
		
						TextureRHI = TextureObjectResource->AccessRHIResource();

						if ( UTexture* TextureObj = TextureObjectResource->TextureObject )
						{
							Filter = TextureObj->Filter;
						}
					}
					else
					{	
						FTextureRHIParamRef NativeTextureRHI = ((TSlateTexture<FTexture2DRHIRef>*)ShaderResource)->GetTypedResource();
						// Atlas textures that have no content are never initialised but null textures are invalid on many platforms.
						TextureRHI = NativeTextureRHI ? NativeTextureRHI : (FTextureRHIParamRef)GWhiteTexture->TextureRHI;
					}

					if ( DrawFlags == ( ESlateBatchDrawFlag::TileU | ESlateBatchDrawFlag::TileV ) )
					{
						switch ( Filter )
						{
						case TF_Nearest:
							SamplerState = TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
							break;
						case TF_Trilinear:
							SamplerState = TStaticSamplerState<SF_Trilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
							break;
						default:
							SamplerState = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
						}
					}
					else if ( DrawFlags & ESlateBatchDrawFlag::TileU )
					{
						switch ( Filter )
						{
						case TF_Nearest:
							SamplerState = TStaticSamplerState<SF_Point, AM_Wrap, AM_Clamp, AM_Wrap>::GetRHI();
							break;
						case TF_Trilinear:
							SamplerState = TStaticSamplerState<SF_Trilinear, AM_Wrap, AM_Clamp, AM_Wrap>::GetRHI();
							break;
						default:
							SamplerState = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Clamp, AM_Wrap>::GetRHI();
						}
					}
					else if ( DrawFlags & ESlateBatchDrawFlag::TileV )
					{
						switch ( Filter )
						{
						case TF_Nearest:
							SamplerState = TStaticSamplerState<SF_Point, AM_Clamp, AM_Wrap, AM_Wrap>::GetRHI();
							break;
						case TF_Trilinear:
							SamplerState = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Wrap, AM_Wrap>::GetRHI();
							break;
						default:
							SamplerState = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Wrap, AM_Wrap>::GetRHI();
						}
					}
					else
					{
						switch ( Filter )
						{
						case TF_Nearest:
							SamplerState = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							break;
						case TF_Trilinear:
							SamplerState = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							break;
						default:
							SamplerState = BilinearClamp;
							break;
						}
					}
				}

				PixelShader->SetTexture(RHICmdList, TextureRHI, SamplerState);
				PixelShader->SetShaderParams(RHICmdList, ShaderParams.PixelParams);
				PixelShader->SetDisplayGamma(RHICmdList, (DrawFlags & ESlateBatchDrawFlag::NoGamma) ? 1.0f : DisplayGamma);

				uint32 PrimitiveCount = RenderBatch.DrawPrimitiveType == ESlateDrawPrimitive::LineList ? RenderBatch.NumIndices / 2 : RenderBatch.NumIndices / 3;

				// for RHIs that can't handle VertexOffset, we need to offset the stream source each time
				if (!GRHISupportsBaseVertexIndex)
				{
					RHICmdList.SetStreamSource(0, VertexBuffer->VertexBufferRHI, sizeof(FSlateVertex), RenderBatch.VertexOffset * sizeof(FSlateVertex));
					RHICmdList.DrawIndexedPrimitive(IndexBuffer->IndexBufferRHI, GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType), 0, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, RenderBatch.InstanceCount);
				}
				else
				{
					RHICmdList.SetStreamSource(0, VertexBuffer->VertexBufferRHI, sizeof(FSlateVertex), 0);
					RHICmdList.DrawIndexedPrimitive(IndexBuffer->IndexBufferRHI, GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType), RenderBatch.VertexOffset, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, RenderBatch.InstanceCount);
				}

			}
			else if (ShaderResource && ShaderResource->GetType() == ESlateShaderResource::Material && GEngine)
			{
				// Note: This code is only executed if the engine is loaded (in early loading screens attemping to use a material is unsupported
				if (!SceneView)
				{
					SceneView = &CreateSceneView(SceneViewContext, BackBuffer, ViewProjectionMatrix);
				}

				FSlateMaterialResource* MaterialShaderResource = (FSlateMaterialResource*)ShaderResource;
				if( MaterialShaderResource->GetMaterialObject() != nullptr )
				{
					FMaterialRenderProxy* MaterialRenderProxy = MaterialShaderResource->GetRenderProxy();

					const FMaterial* Material = MaterialRenderProxy->GetMaterial(SceneView->GetFeatureLevel());

					FSlateMaterialShaderPS* PixelShader = GetMaterialPixelShader(Material, ShaderType, DrawEffects);

					const bool bUseInstancing = RenderBatch.InstanceCount > 0 && RenderBatch.InstanceData != nullptr;
					FSlateMaterialShaderVS* VertexShader = GetMaterialVertexShader(Material, bUseInstancing);

					if (VertexShader && PixelShader)
					{
						RHICmdList.SetLocalBoundShaderState(RHICmdList.BuildLocalBoundShaderState(
							bUseInstancing ? GSlateInstancedVertexDeclaration.VertexDeclarationRHI : GSlateVertexDeclaration.VertexDeclarationRHI,
							VertexShader->GetVertexShader(),
							nullptr,
							nullptr,
							PixelShader->GetPixelShader(),
							FGeometryShaderRHIRef()));

						VertexShader->SetViewProjection(RHICmdList, DynamicOffset * ViewProjectionMatrix);
						VertexShader->SetVerticalAxisMultiplier(RHICmdList, bAllowSwitchVerticalAxis && RHINeedsToSwitchVerticalAxis(GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel]) ? -1.0f : 1.0f);
						VertexShader->SetMaterialShaderParameters(RHICmdList, *SceneView, MaterialRenderProxy, Material);

						PixelShader->SetParameters(RHICmdList, *SceneView, MaterialRenderProxy, Material, DisplayGamma, ShaderParams.PixelParams);
						PixelShader->SetDisplayGamma(RHICmdList, (DrawFlags & ESlateBatchDrawFlag::NoGamma) ? 1.0f : DisplayGamma);

						FSlateShaderResource* MaskResource = MaterialShaderResource->GetTextureMaskResource();
						if (MaskResource)
						{
							RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_InverseDestAlpha, BF_One>::GetRHI());

							FTexture2DRHIRef TextureRHI;
							TextureRHI = ((TSlateTexture<FTexture2DRHIRef>*)MaskResource)->GetTypedResource();

							PixelShader->SetAdditionalTexture(RHICmdList, TextureRHI, BilinearClamp);
						}

#if SLATE_PRE_MULTIPLY
						if (RenderBatch.DrawFlags & ESlateBatchDrawFlag::PreMultipliedAlpha)
						{
							RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI());
						}
#endif

						uint32 PrimitiveCount = RenderBatch.DrawPrimitiveType == ESlateDrawPrimitive::LineList ? RenderBatch.NumIndices / 2 : RenderBatch.NumIndices / 3;

						if ( bUseInstancing )
						{
							uint32 InstanceCount = RenderBatch.InstanceCount;

							if ( GRHISupportsInstancing )
							{
								FSlateUpdatableInstanceBuffer* InstanceBuffer = (FSlateUpdatableInstanceBuffer*)RenderBatch.InstanceData;
								InstanceBuffer->BindStreamSource(RHICmdList, 1, RenderBatch.InstanceOffset);

								// for RHIs that can't handle VertexOffset, we need to offset the stream source each time
								if ( !GRHISupportsBaseVertexIndex )
								{
									RHICmdList.SetStreamSource(0, VertexBuffer->VertexBufferRHI, sizeof(FSlateVertex), RenderBatch.VertexOffset * sizeof(FSlateVertex));
									RHICmdList.DrawIndexedPrimitive(IndexBuffer->IndexBufferRHI, GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType), 0, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, InstanceCount);
								}
								else
								{
									RHICmdList.SetStreamSource(0, VertexBuffer->VertexBufferRHI, sizeof(FSlateVertex), 0);
									RHICmdList.DrawIndexedPrimitive(IndexBuffer->IndexBufferRHI, GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType), RenderBatch.VertexOffset, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, InstanceCount);
								}
							}
							//else
							//{
							//	for ( uint32 InstanceIndex = 0; InstanceIndex < InstanceCount; InstanceIndex++ )
							//	{
							//		FSlateUpdatableInstanceBuffer* InstanceBuffer = (FSlateUpdatableInstanceBuffer*)RenderBatch.InstanceData;
							//		InstanceBuffer->BindStreamSource(RHICmdList, 1, RenderBatch.InstanceOffset + InstanceIndex);

							//		// for RHIs that can't handle VertexOffset, we need to offset the stream source each time
							//		if ( !GRHISupportsBaseVertexIndex )
							//		{
							//			RHICmdList.SetStreamSource(0, VertexBuffer->VertexBufferRHI, sizeof(FSlateVertex), RenderBatch.VertexOffset * sizeof(FSlateVertex));
							//			RHICmdList.DrawIndexedPrimitive(IndexBuffer->IndexBufferRHI, GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType), 0, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, 1);
							//		}
							//		else
							//		{
							//			RHICmdList.DrawIndexedPrimitive(IndexBuffer->IndexBufferRHI, GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType), RenderBatch.VertexOffset, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, 1);
							//		}
							//	}
							//}
						}
						else
						{
							RHICmdList.SetStreamSource(1, nullptr, 0, 0);

							// for RHIs that can't handle VertexOffset, we need to offset the stream source each time
							if ( !GRHISupportsBaseVertexIndex )
							{
								RHICmdList.SetStreamSource(0, VertexBuffer->VertexBufferRHI, sizeof(FSlateVertex), RenderBatch.VertexOffset * sizeof(FSlateVertex));
								RHICmdList.DrawIndexedPrimitive(IndexBuffer->IndexBufferRHI, GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType), 0, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, 1);
							}
							else
							{
								RHICmdList.SetStreamSource(0, VertexBuffer->VertexBufferRHI, sizeof(FSlateVertex), 0);
								RHICmdList.DrawIndexedPrimitive(IndexBuffer->IndexBufferRHI, GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType), RenderBatch.VertexOffset, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, 1);
							}
						}
					}
				}
			}

		}
		else
		{
			TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> CustomDrawer = RenderBatch.CustomDrawer.Pin();
			if (CustomDrawer.IsValid())
			{
				// Disable scissor rect. A previous draw element may have had one
				RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

				// This element is custom and has no Slate geometry.  Tell it to render itself now
				CustomDrawer->DrawRenderThread(RHICmdList, &BackBuffer.GetRenderTargetTexture());

				// Something may have messed with the viewport size so set it back to the full target.
				RHICmdList.SetViewport( 0,0,0,BackBuffer.GetSizeXY().X, BackBuffer.GetSizeXY().Y, 0.0f ); 
				RHICmdList.SetStreamSource(0, VertexBuffer->VertexBufferRHI, sizeof(FSlateVertex), 0);
			}
		}

	}
}


FSlateElementPS* FSlateRHIRenderingPolicy::GetTexturePixelShader( ESlateShader::Type ShaderType, ESlateDrawEffect::Type DrawEffects )
{
	FSlateElementPS* PixelShader = nullptr;
	const auto FeatureLevel = GMaxRHIFeatureLevel;
	auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

#if !DEBUG_OVERDRAW
	
	const bool bDrawDisabled = ( DrawEffects & ESlateDrawEffect::DisabledEffect ) != 0;
	const bool bUseTextureAlpha = ( DrawEffects & ESlateDrawEffect::IgnoreTextureAlpha ) == 0;

	if ( bDrawDisabled )
	{
		switch ( ShaderType )
		{
		default:
		case ESlateShader::Default:
			if ( bUseTextureAlpha )
			{
				PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Default, true, true> >(ShaderMap);
			}
			else
			{
				PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Default, true, false> >(ShaderMap);
			}
			break;
		case ESlateShader::Border:
			if ( bUseTextureAlpha )
			{
				PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Border, true, true> >(ShaderMap);
			}
			else
			{
				PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Border, true, false> >(ShaderMap);
			}
			break;
		case ESlateShader::Font:
			PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Font, true> >(ShaderMap);
			break;
		case ESlateShader::LineSegment:
			PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::LineSegment, true> >(ShaderMap);
			break;
		}
	}
	else
	{
		switch ( ShaderType )
		{
		default:
		case ESlateShader::Default:
			if ( bUseTextureAlpha )
			{
				PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Default, false, true> >(ShaderMap);
			}
			else
			{
				PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Default, false, false> >(ShaderMap);
			}
			break;
		case ESlateShader::Border:
			if ( bUseTextureAlpha )
			{
				PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Border, false, true> >(ShaderMap);
			}
			else
			{
				PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Border, false, false> >(ShaderMap);
			}
			break;
		case ESlateShader::Font:
			PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Font, false> >(ShaderMap);
			break;
		case ESlateShader::LineSegment:
			PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::LineSegment, false> >(ShaderMap);
			break;
		}
	}
#else
	PixelShader = *TShaderMapRef<FSlateDebugOverdrawPS>(ShaderMap);
#endif

#undef PixelShaderLookupTable

	return PixelShader;
}

FSlateMaterialShaderPS* FSlateRHIRenderingPolicy::GetMaterialPixelShader( const FMaterial* Material, ESlateShader::Type ShaderType, ESlateDrawEffect::Type DrawEffects )
{
	const FMaterialShaderMap* MaterialShaderMap = Material->GetRenderingThreadShaderMap();

	const bool bDrawDisabled = (DrawEffects & ESlateDrawEffect::DisabledEffect) != 0;
	const bool bUseTextureAlpha = (DrawEffects & ESlateDrawEffect::IgnoreTextureAlpha) == 0;

	FShader* FoundShader = nullptr;
	switch (ShaderType)
	{
	case ESlateShader::Default:
		if (bDrawDisabled)
		{
			FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderPS<ESlateShader::Default, true>::StaticType);
		}
		else
		{
			FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderPS<ESlateShader::Default, false>::StaticType);
		}
		break;
	case ESlateShader::Border:
		if (bDrawDisabled)
		{
			FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderPS<ESlateShader::Border, true>::StaticType);
		}
		else
		{
			FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderPS<ESlateShader::Border, false>::StaticType);
		}
		break;
	case ESlateShader::Font:
		if(bDrawDisabled)
		{
			FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderPS<ESlateShader::Font, true>::StaticType);
		}
		else
		{
			FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderPS<ESlateShader::Font, false>::StaticType);
		}
		break;
	case ESlateShader::Custom:
		{
			FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderPS<ESlateShader::Custom, false>::StaticType);
		}
		break;
	default:
		checkf(false, TEXT("Unsupported Slate shader type for use with materials"));
		break;
	}

	return FoundShader ? (FSlateMaterialShaderPS*)FoundShader->GetShaderChecked() : nullptr;
}

FSlateMaterialShaderVS* FSlateRHIRenderingPolicy::GetMaterialVertexShader( const FMaterial* Material, bool bUseInstancing )
{
	const FMaterialShaderMap* MaterialShaderMap = Material->GetRenderingThreadShaderMap();

	FShader* FoundShader = nullptr;
	if( bUseInstancing )
	{
		FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderVS<true>::StaticType);
	}
	else
	{
		FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderVS<false>::StaticType);
	}
	
	return FoundShader ? (FSlateMaterialShaderVS*)FoundShader->GetShaderChecked() : nullptr;
}

EPrimitiveType FSlateRHIRenderingPolicy::GetRHIPrimitiveType(ESlateDrawPrimitive::Type SlateType)
{
	switch(SlateType)
	{
	case ESlateDrawPrimitive::LineList:
		return PT_LineList;
	case ESlateDrawPrimitive::TriangleList:
	default:
		return PT_TriangleList;
	}

};

