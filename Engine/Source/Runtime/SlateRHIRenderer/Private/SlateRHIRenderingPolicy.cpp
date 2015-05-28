// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

DECLARE_CYCLE_STAT(TEXT("Update Buffers RT"), STAT_SlateUpdateBufferRTTime, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("Draw Time"), STAT_SlateDrawTime, STATGROUP_Slate);

DECLARE_MEMORY_STAT(TEXT("Vertex Buffer Memory"), STAT_SlateVertexBufferMemory, STATGROUP_SlateMemory);
DECLARE_MEMORY_STAT(TEXT("Index Buffer Memory"), STAT_SlateIndexBufferMemory, STATGROUP_SlateMemory);

FSlateElementIndexBuffer::FSlateElementIndexBuffer()
	: BufferSize(0)	 
	, BufferUsageSize(0)
{

}

FSlateElementIndexBuffer::~FSlateElementIndexBuffer()
{

}

/** Initializes the index buffers RHI resource. */
void FSlateElementIndexBuffer::InitDynamicRHI()
{
	check( IsInRenderingThread() );

	if( BufferSize == 0 )
	{
		BufferSize = 200 * sizeof(SlateIndex);
	}

	FRHIResourceCreateInfo CreateInfo;
	IndexBufferRHI = RHICreateIndexBuffer( sizeof(SlateIndex), BufferSize, BUF_Dynamic, CreateInfo );
	check( IsValidRef(IndexBufferRHI) );
}

/** Resizes the buffer to the passed in size.  Preserves internal data */
void FSlateElementIndexBuffer::ResizeBuffer( uint32 NewSizeBytes )
{
	check( IsInRenderingThread() );

	if( NewSizeBytes != 0 && NewSizeBytes != BufferSize)
	{
		IndexBufferRHI.SafeRelease();
		FRHIResourceCreateInfo CreateInfo;
		IndexBufferRHI = RHICreateIndexBuffer( sizeof(SlateIndex), NewSizeBytes, BUF_Dynamic, CreateInfo );
		check(IsValidRef(IndexBufferRHI));

		BufferSize = NewSizeBytes;
	}
}

void FSlateElementIndexBuffer::FillBuffer( const TArray<SlateIndex>& InIndices, bool bShrinkToFit  )
{
	check( IsInRenderingThread() );

	if( InIndices.Num() )
	{
		uint32 NumIndices = InIndices.Num();

		uint32 RequiredBufferSize = NumIndices*sizeof(SlateIndex);

		// resize if needed
		if( RequiredBufferSize > GetBufferSize() || bShrinkToFit )
		{
			// Use array resize techniques for the vertex buffer
			ResizeBuffer( InIndices.GetAllocatedSize() );
		}

		BufferUsageSize += RequiredBufferSize;

		void* IndicesPtr = RHILockIndexBuffer( IndexBufferRHI, 0, RequiredBufferSize, RLM_WriteOnly );

		FMemory::Memcpy( IndicesPtr, InIndices.GetData(), RequiredBufferSize );

		RHIUnlockIndexBuffer(IndexBufferRHI);
	}
}

/** Releases the index buffers RHI resource. */
void FSlateElementIndexBuffer::ReleaseDynamicRHI()
{
	IndexBufferRHI.SafeRelease();
	BufferSize = 0;
}

FSlateRHIRenderingPolicy::FSlateRHIRenderingPolicy( TSharedPtr<FSlateFontCache> InFontCache, TSharedRef<FSlateRHIResourceManager> InResourceManager )
	: FSlateRenderingPolicy(0)
	, ResourceManager( InResourceManager )
	, FontCache( InFontCache )
	, CurrentBufferIndex(0)
	, bShouldShrinkResources(false)
	, bGammaCorrect(true)
{
	InitResources();
};

FSlateRHIRenderingPolicy::~FSlateRHIRenderingPolicy()
{
}

void FSlateRHIRenderingPolicy::InitResources()
{
	for( int32 BufferIndex = 0; BufferIndex < 2; ++BufferIndex )
	{
		BeginInitResource(&VertexBuffers[BufferIndex]);
		BeginInitResource(&IndexBuffers[BufferIndex]);
	}
}

void FSlateRHIRenderingPolicy::ReleaseResources()
{
	for( int32 BufferIndex = 0; BufferIndex < 2; ++BufferIndex )
	{
		BeginReleaseResource(&VertexBuffers[BufferIndex]);
		BeginReleaseResource(&IndexBuffers[BufferIndex]);
	}
}

void FSlateRHIRenderingPolicy::BeginDrawingWindows()
{
	check( IsInRenderingThread() );

	for( int32 BufferIndex = 0; BufferIndex < 2; ++BufferIndex )
	{
		VertexBuffers[CurrentBufferIndex].ResetBufferUsage();
		IndexBuffers[CurrentBufferIndex].ResetBufferUsage();
	}
}

void FSlateRHIRenderingPolicy::EndDrawingWindows()
{
	check( IsInRenderingThread() );

	bShouldShrinkResources = false;

	uint32 TotalVertexBufferMemory = 0;
	uint32 TotalIndexBufferMemory = 0;

	uint32 TotalVertexBufferUsage = 0;
	uint32 TotalIndexBufferUsage = 0;

	for( int32 BufferIndex = 0; BufferIndex < 2; ++BufferIndex )
	{
		TotalVertexBufferMemory += VertexBuffers[BufferIndex].GetBufferSize();
		TotalVertexBufferUsage += VertexBuffers[BufferIndex].GetBufferUsageSize();
		
		TotalIndexBufferMemory += IndexBuffers[BufferIndex].GetBufferSize();
		TotalIndexBufferUsage += IndexBuffers[BufferIndex].GetBufferUsageSize();
	}

	// How much larger the buffers can be than the required size. 
	// @todo Slate probably could be more intelligent about this
	const uint32 MaxSizeMultiplier = 2;

	if( TotalVertexBufferMemory > TotalVertexBufferUsage*MaxSizeMultiplier || TotalIndexBufferMemory > TotalIndexBufferUsage*MaxSizeMultiplier )
	{
		// The vertex buffer or index is more than twice the size of what is required.  Shrink it
		bShouldShrinkResources = true;
	}


	SET_MEMORY_STAT( STAT_SlateVertexBufferMemory, TotalVertexBufferMemory );
	SET_MEMORY_STAT( STAT_SlateIndexBufferMemory, TotalIndexBufferMemory );
}

void FSlateRHIRenderingPolicy::UpdateBuffers( const FSlateWindowElementList& WindowElementList )
{
	SCOPE_CYCLE_COUNTER( STAT_SlateUpdateBufferRTTime );
	// Should only be called by the rendering thread
	check(IsInRenderingThread());

	{
		const TArray<FSlateVertex>& Vertices = WindowElementList.GetBatchedVertices();
		const TArray<SlateIndex>& Indices = WindowElementList.GetBatchedIndices();

		TSlateElementVertexBuffer<FSlateVertex>& VertexBuffer = VertexBuffers[CurrentBufferIndex];
		FSlateElementIndexBuffer& IndexBuffer = IndexBuffers[CurrentBufferIndex];

		VertexBuffer.FillBuffer( Vertices, bShouldShrinkResources );
		IndexBuffer.FillBuffer( Indices, bShouldShrinkResources );
	}
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

	// Create the view's uniform buffer.
	FViewUniformShaderParameters ViewUniformShaderParameters;
	ViewUniformShaderParameters.TranslatedWorldToClip = View->ViewMatrices.TranslatedViewProjectionMatrix;
	ViewUniformShaderParameters.WorldToClip = ViewProjectionMatrix;
	ViewUniformShaderParameters.TranslatedWorldToView = EffectiveTranslatedViewMatrix;
	ViewUniformShaderParameters.ViewToTranslatedWorld = View->InvViewMatrix * FTranslationMatrix(View->ViewMatrices.PreViewTranslation);
	ViewUniformShaderParameters.ViewToClip = View->ViewMatrices.ProjMatrix;
	ViewUniformShaderParameters.ClipToTranslatedWorld = View->ViewMatrices.InvTranslatedViewProjectionMatrix;
	ViewUniformShaderParameters.ViewForward = EffectiveTranslatedViewMatrix.GetColumn(2);
	ViewUniformShaderParameters.ViewUp = EffectiveTranslatedViewMatrix.GetColumn(1);
	ViewUniformShaderParameters.ViewRight = EffectiveTranslatedViewMatrix.GetColumn(0);
	ViewUniformShaderParameters.InvDeviceZToWorldZTransform = View->InvDeviceZToWorldZTransform;
	ViewUniformShaderParameters.ScreenPositionScaleBias = FVector4(0,0,0,0);
	ViewUniformShaderParameters.ViewRectMin = FVector4(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, 0.0f);
	ViewUniformShaderParameters.ViewSizeAndSceneTexelSize = FVector4(ViewRect.Width(), ViewRect.Height(), 1.0f/ViewRect.Width(), 1.0f/ViewRect.Height() );
	ViewUniformShaderParameters.ViewOrigin = View->ViewMatrices.ViewOrigin;
	ViewUniformShaderParameters.TranslatedViewOrigin = View->ViewMatrices.ViewOrigin + View->ViewMatrices.PreViewTranslation;
	ViewUniformShaderParameters.DiffuseOverrideParameter = View->DiffuseOverrideParameter;
	ViewUniformShaderParameters.SpecularOverrideParameter = View->SpecularOverrideParameter;
	ViewUniformShaderParameters.NormalOverrideParameter = View->NormalOverrideParameter;
	ViewUniformShaderParameters.RoughnessOverrideParameter = View->RoughnessOverrideParameter;
	ViewUniformShaderParameters.PrevFrameGameTime = View->Family->CurrentWorldTime - View->Family->DeltaWorldTime;
	ViewUniformShaderParameters.PrevFrameRealTime = View->Family->CurrentRealTime - View->Family->DeltaWorldTime;
	ViewUniformShaderParameters.PreViewTranslation = View->ViewMatrices.PreViewTranslation;
	ViewUniformShaderParameters.CullingSign = View->bReverseCulling ? -1.0f : 1.0f;
	ViewUniformShaderParameters.NearPlane = GNearClippingPlane;
	ViewUniformShaderParameters.GameTime = View->Family->CurrentWorldTime;
	ViewUniformShaderParameters.RealTime = View->Family->CurrentRealTime;
	ViewUniformShaderParameters.Random = FMath::Rand();
	ViewUniformShaderParameters.FrameNumber = View->Family->FrameNumber;

	ViewUniformShaderParameters.DirectionalLightShadowTexture = GWhiteTexture->TextureRHI;
	ViewUniformShaderParameters.DirectionalLightShadowSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	ViewUniformShaderParameters.ScreenToWorld = FMatrix(
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, View->ProjectionMatrixUnadjustedForRHI.M[2][2], 1),
		FPlane(0, 0, View->ProjectionMatrixUnadjustedForRHI.M[3][2], 0))
		* View->InvViewProjectionMatrix;

	ViewUniformShaderParameters.ScreenToTranslatedWorld = FMatrix(
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, View->ProjectionMatrixUnadjustedForRHI.M[2][2], 1),
		FPlane(0, 0, View->ProjectionMatrixUnadjustedForRHI.M[3][2], 0))
		* View->ViewMatrices.InvTranslatedViewProjectionMatrix;

	View->UniformBuffer = TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(ViewUniformShaderParameters, UniformBuffer_SingleFrame);
	return *View;
}

void FSlateRHIRenderingPolicy::DrawElements(FRHICommandListImmediate& RHICmdList, FSlateBackBuffer& BackBuffer, const FMatrix& ViewProjectionMatrix, const TArray<FSlateRenderBatch>& RenderBatches, bool bAllowSwitchVerticalAxis)
{
	SCOPE_CYCLE_COUNTER( STAT_SlateDrawTime );

	// Should only be called by the rendering thread
	check(IsInRenderingThread());

	TSlateElementVertexBuffer<FSlateVertex>& VertexBuffer = VertexBuffers[CurrentBufferIndex];
	FSlateElementIndexBuffer& IndexBuffer = IndexBuffers[CurrentBufferIndex];

	float TimeSeconds = FPlatformTime::Seconds() - GStartTime;
	float RealTimeSeconds = FPlatformTime::Seconds() - GStartTime;
	float DeltaTimeSeconds = FApp::GetDeltaTime();

	static const FEngineShowFlags DefaultShowFlags(ESFIM_Game);

	const float DisplayGamma = bGammaCorrect ? GEngine ? GEngine->GetDisplayGamma() : 2.2f : 1.0f;

	FSceneViewFamilyContext SceneViewContext
	(
		FSceneViewFamily::ConstructionValues
		(
			&BackBuffer,
			NULL,
			DefaultShowFlags
		)
		.SetWorldTimes( TimeSeconds, RealTimeSeconds, DeltaTimeSeconds )
		.SetGammaCorrection( DisplayGamma )
	);

	FSceneView* SceneView = NULL;

	TShaderMapRef<FSlateElementVS> VertexShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	// Disabled stencil test state
	FDepthStencilStateRHIRef DSOff = TStaticDepthStencilState<false,CF_Always>::GetRHI();

	FSamplerStateRHIRef BilinearClamp = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

	if (GRHISupportsBaseVertexIndex)
	{
		RHICmdList.SetStreamSource(0, VertexBuffer.VertexBufferRHI, sizeof(FSlateVertex), 0);
	}

	// Draw each element
	for( int32 BatchIndex = 0; BatchIndex < RenderBatches.Num(); ++BatchIndex )
	{
		const FSlateRenderBatch& RenderBatch = RenderBatches[BatchIndex];
		const FSlateShaderResource* ShaderResource = RenderBatch.Texture;

		const ESlateBatchDrawFlag::Type DrawFlags = RenderBatch.DrawFlags;
		const ESlateDrawEffect::Type DrawEffects = RenderBatch.DrawEffects;
		const ESlateShader::Type ShaderType = RenderBatch.ShaderType;
		const FShaderParams& ShaderParams = RenderBatch.ShaderParams;

		if( !RenderBatch.CustomDrawer.IsValid() )
		{
			if( !ShaderResource || ShaderResource->GetType() != ESlateShaderResource::Material ) 
			{
				FSlateElementPS* PixelShader = GetTexturePixelShader(ShaderType, DrawEffects);

				RHICmdList.SetLocalBoundShaderState(RHICmdList.BuildLocalBoundShaderState(
					GSlateVertexDeclaration.VertexDeclarationRHI,
					VertexShader->GetVertexShader(),
					nullptr,
					nullptr,
					PixelShader->GetPixelShader(),
					FGeometryShaderRHIRef()));


				VertexShader->SetViewProjection(RHICmdList, ViewProjectionMatrix);
				VertexShader->SetVerticalAxisMultiplier(RHICmdList, bAllowSwitchVerticalAxis && RHINeedsToSwitchVerticalAxis(GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel]) ? -1.0f : 1.0f );
#if !DEBUG_OVERDRAW
				RHICmdList.SetBlendState(
					(RenderBatch.DrawFlags & ESlateBatchDrawFlag::NoBlending)
					? TStaticBlendState<>::GetRHI()
					: TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_InverseDestAlpha, BF_One>::GetRHI()
					);
#else
				RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
#endif

				// Disable stencil testing by default
				RHICmdList.SetDepthStencilState(DSOff);

				if (DrawFlags & ESlateBatchDrawFlag::Wireframe)
				{
					RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Wireframe, CM_None, true>::GetRHI());
				}
				else
				{
					RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None, true>::GetRHI());
				}

				if (RenderBatch.ScissorRect.IsSet())
				{
					RHICmdList.SetScissorRect(true, RenderBatch.ScissorRect.GetValue().Left, RenderBatch.ScissorRect.GetValue().Top, RenderBatch.ScissorRect.GetValue().Right, RenderBatch.ScissorRect.GetValue().Bottom);
				}
				else
				{
					RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
				}


				FTexture2DRHIRef TextureRHI;
				if(ShaderResource)
				{
					TextureRHI = ((TSlateTexture<FTexture2DRHIRef>*)ShaderResource)->GetTypedResource();
				}

				if( ShaderResource && IsValidRef( TextureRHI ) )
				{
					FSamplerStateRHIRef SamplerState; 

					if( DrawFlags == (ESlateBatchDrawFlag::TileU | ESlateBatchDrawFlag::TileV) )
					{
						SamplerState = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
					}
					else if (DrawFlags & ESlateBatchDrawFlag::TileU)
					{
						SamplerState = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Clamp, AM_Wrap>::GetRHI();
					}
					else if (DrawFlags & ESlateBatchDrawFlag::TileV)
					{
						SamplerState = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Wrap, AM_Wrap>::GetRHI();
					}
					else
					{
						SamplerState = BilinearClamp;
					}
					PixelShader->SetTexture(RHICmdList, TextureRHI, SamplerState);
				}
				else
				{
					PixelShader->SetTexture(RHICmdList, GWhiteTexture->TextureRHI, BilinearClamp);
				}

				PixelShader->SetShaderParams(RHICmdList, ShaderParams.PixelParams);
				PixelShader->SetDisplayGamma(RHICmdList, (DrawFlags & ESlateBatchDrawFlag::NoGamma) ? 1.0f : DisplayGamma);


				check(RenderBatch.NumIndices > 0);

				uint32 PrimitiveCount = RenderBatch.DrawPrimitiveType == ESlateDrawPrimitive::LineList ? RenderBatch.NumIndices / 2 : RenderBatch.NumIndices / 3;

				// for RHIs that can't handle VertexOffset, we need to offset the stream source each time
				if (!GRHISupportsBaseVertexIndex)
				{
					RHICmdList.SetStreamSource(0, VertexBuffer.VertexBufferRHI, sizeof(FSlateVertex), RenderBatch.VertexOffset * sizeof(FSlateVertex));
					RHICmdList.DrawIndexedPrimitive(IndexBuffer.IndexBufferRHI, GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType), 0, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, 1);
				}
				else
				{
					RHICmdList.DrawIndexedPrimitive(IndexBuffer.IndexBufferRHI, GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType), RenderBatch.VertexOffset, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, 1);
				}

			}
			else if (ShaderResource && ShaderResource->GetType() == ESlateShaderResource::Material && GEngine)
			{
				// Note: This code is only executed if the engine is loaded (in early loading screens attemping to use a material is unsupported
				if (!SceneView)
				{
					SceneView = &CreateSceneView(SceneViewContext, BackBuffer, ViewProjectionMatrix);
				}

				FMaterialRenderProxy* MaterialRenderProxy = ((FSlateMaterialResource*)ShaderResource)->GetRenderProxy();

				const FMaterial* Material = MaterialRenderProxy->GetMaterial(SceneView->GetFeatureLevel());

				FSlateMaterialShaderPS* PixelShader = GetMaterialPixelShader( Material, ShaderType, DrawEffects );

				if( PixelShader )
				{
					RHICmdList.SetLocalBoundShaderState(RHICmdList.BuildLocalBoundShaderState(
						GSlateVertexDeclaration.VertexDeclarationRHI,
						VertexShader->GetVertexShader(),
						nullptr,
						nullptr,
						PixelShader->GetPixelShader(),
						FGeometryShaderRHIRef()));

					PixelShader->SetParameters(RHICmdList, *SceneView, MaterialRenderProxy, Material, DisplayGamma, ShaderParams.PixelParams);
					PixelShader->SetDisplayGamma(RHICmdList, (DrawFlags & ESlateBatchDrawFlag::NoGamma) ? 1.0f : DisplayGamma);

					VertexShader->SetViewProjection( RHICmdList, ViewProjectionMatrix );

					check(RenderBatch.NumIndices > 0);

					uint32 PrimitiveCount = RenderBatch.DrawPrimitiveType == ESlateDrawPrimitive::LineList ? RenderBatch.NumIndices / 2 : RenderBatch.NumIndices / 3;

					// for RHIs that can't handle VertexOffset, we need to offset the stream source each time
					if (!GRHISupportsBaseVertexIndex)
					{
						RHICmdList.SetStreamSource(0, VertexBuffer.VertexBufferRHI, sizeof(FSlateVertex), RenderBatch.VertexOffset * sizeof(FSlateVertex));
						RHICmdList.DrawIndexedPrimitive(IndexBuffer.IndexBufferRHI, GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType), 0, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, 1);
					}
					else
					{
						RHICmdList.DrawIndexedPrimitive(IndexBuffer.IndexBufferRHI, GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType), RenderBatch.VertexOffset, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, 1);
					}
				}
			}

		}
		else
		{
			TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> CustomDrawer = RenderBatch.CustomDrawer.Pin();
			if (CustomDrawer.IsValid())
			{
				// This element is custom and has no Slate geometry.  Tell it to render itself now
				CustomDrawer->DrawRenderThread(RHICmdList, &BackBuffer.GetRenderTargetTexture());

				// Something may have messed with the viewport size so set it back to the full target.
				RHICmdList.SetViewport( 0,0,0,BackBuffer.GetSizeXY().X, BackBuffer.GetSizeXY().Y, 0.0f ); 
				RHICmdList.SetStreamSource(0, VertexBuffer.VertexBufferRHI, sizeof(FSlateVertex), 0);

			}
		}

	}

	CurrentBufferIndex = (CurrentBufferIndex + 1) % 2;
}


FSlateElementPS* FSlateRHIRenderingPolicy::GetTexturePixelShader( ESlateShader::Type ShaderType, ESlateDrawEffect::Type DrawEffects )
{
	FSlateElementPS* PixelShader = NULL;
	const auto FeatureLevel = GMaxRHIFeatureLevel;
	auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

#if !DEBUG_OVERDRAW
	const bool bDrawDisabled = (DrawEffects & ESlateDrawEffect::DisabledEffect) != 0;
	const bool bUseTextureAlpha = (DrawEffects & ESlateDrawEffect::IgnoreTextureAlpha) == 0;
	if( bDrawDisabled )
	{
		switch( ShaderType )
		{
		default:
		case ESlateShader::Default:
			if( bUseTextureAlpha )
			{
				PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Default, true, true> >(ShaderMap);
			}
			else
			{
				PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Default, true, false> >(ShaderMap);
			}
			break;
		case ESlateShader::Border:
			if( bUseTextureAlpha )
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
		switch( ShaderType )
		{
		default:
		case ESlateShader::Default:
			if( bUseTextureAlpha )
			{
				PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Default, false, true> >(ShaderMap);
			}
			else
			{
				PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Default, false, false> >(ShaderMap);
			}
			break;
		case ESlateShader::Border:
			if( bUseTextureAlpha )
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
			FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderPS<0, true>::StaticType);
		}
		else
		{
			FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderPS<0, false>::StaticType);
		}
		break;
	case ESlateShader::Border:
		if (bDrawDisabled)
		{
			FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderPS<1, true>::StaticType);
		}
		else
		{
			FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderPS<1, false>::StaticType);
		}
		break;
	default:
		checkf(false, TEXT("Unsupported Slate shader type for use with materials"));
		break;
	}

	return FoundShader ? (FSlateMaterialShaderPS*)FoundShader->GetShaderChecked() : nullptr;
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



