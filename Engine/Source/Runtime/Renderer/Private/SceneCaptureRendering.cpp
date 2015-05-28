// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "ScreenRendering.h"
#include "PostProcessAmbient.h"
#include "PostProcessing.h"
#include "SceneUtils.h"

// Copies into render target, optionally flipping it in the Y-axis
static void CopyCaptureToTarget(FRHICommandListImmediate& RHICmdList, const FRenderTarget* Target, const FIntPoint& TargetSize, FViewInfo& View, const FIntRect& ViewRect, FTextureRHIParamRef SourceTextureRHI, bool bNeedsFlippedRenderTarget)
{
	SetRenderTarget(RHICmdList, Target->GetRenderTargetTexture(), NULL);

	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
	RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());

	TShaderMapRef<FScreenVS> VertexShader(View.ShaderMap);
	TShaderMapRef<FScreenPS> PixelShader(View.ShaderMap);
	static FGlobalBoundShaderState BoundShaderState;
	SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	FRenderingCompositePassContext Context(RHICmdList, View);

	VertexShader->SetParameters(RHICmdList, View);
	PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), SourceTextureRHI);

	if (bNeedsFlippedRenderTarget)
	{
		DrawRectangle(
			RHICmdList,
			ViewRect.Min.X, ViewRect.Min.Y,
			ViewRect.Width(), ViewRect.Height(),
			ViewRect.Min.X, ViewRect.Height() - ViewRect.Min.Y,
			ViewRect.Width(), -ViewRect.Height(),
			TargetSize,
			TargetSize,
			*VertexShader,
			EDRF_UseTriangleOptimization);
	}
	else
	{
		DrawRectangle(
			RHICmdList,
			ViewRect.Min.X, ViewRect.Min.Y,
			ViewRect.Width(), ViewRect.Height(),
			ViewRect.Min.X, ViewRect.Min.Y,
			ViewRect.Width(), ViewRect.Height(),
			TargetSize,
			GSceneRenderTargets.GetBufferSizeXY(),
			*VertexShader,
			EDRF_UseTriangleOptimization);
	}
}

static void UpdateSceneCaptureContent_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneRenderer* SceneRenderer, FTextureRenderTargetResource* TextureRenderTarget, const FName OwnerName, const FResolveParams& ResolveParams, bool bUseSceneColorTexture)
{
	FMemMark MemStackMark(FMemStack::Get());

	// update any resources that needed a deferred update
	FDeferredUpdateResource::UpdateResources(RHICmdList);

	{
#if WANTS_DRAW_MESH_EVENTS
		FString EventName;
		OwnerName.ToString(EventName);
		SCOPED_DRAW_EVENTF(RHICmdList, SceneCapture, TEXT("SceneCapture %s"), *EventName);
#else
		SCOPED_DRAW_EVENT(RHICmdList, UpdateSceneCaptureContent_RenderThread);
#endif

		const bool bIsMobileHDR = IsMobileHDR();
		const bool bRHINeedsFlip = RHINeedsToSwitchVerticalAxis(GMaxRHIShaderPlatform);
		const bool bNeedsFlippedRenderTarget = !bIsMobileHDR && bRHINeedsFlip;

		// Intermediate render target that will need to be flipped (needed on !IsMobileHDR())
		TRefCountPtr<IPooledRenderTarget> FlippedPooledRenderTarget;

		const FRenderTarget* Target = SceneRenderer->ViewFamily.RenderTarget;
		if (bNeedsFlippedRenderTarget)
		{
			// We need to use an intermediate render target since the result will be flipped
			auto& RenderTarget = Target->GetRenderTargetTexture();
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(Target->GetSizeXY(), 
				RenderTarget.GetReference()->GetFormat(), 
				TexCreate_None, 
				TexCreate_RenderTargetable,
				false));
			GRenderTargetPool.FindFreeElement(Desc, FlippedPooledRenderTarget, TEXT("SceneCaptureFlipped"));
		}

		// Helper class to allow setting render target
		struct FRenderTargetOverride : public FRenderTarget
		{
			FRenderTargetOverride(FRHITexture2D* In)
			{
				RenderTargetTextureRHI = In;
			}

			virtual FIntPoint GetSizeXY() const { return FIntPoint(RenderTargetTextureRHI->GetSizeX(), RenderTargetTextureRHI->GetSizeY()); }

			FTexture2DRHIRef GetTextureParamRef() { return RenderTargetTextureRHI; }
		} FlippedRenderTarget(
			FlippedPooledRenderTarget.GetReference()
			? FlippedPooledRenderTarget.GetReference()->GetRenderTargetItem().TargetableTexture->GetTexture2D() 
			: nullptr);
		FViewInfo& View = SceneRenderer->Views[0];
		FIntRect ViewRect = View.ViewRect;
		FIntRect UnconstrainedViewRect = View.UnconstrainedViewRect;
		SetRenderTarget(RHICmdList, Target->GetRenderTargetTexture(), NULL);
		RHICmdList.Clear(true, FLinearColor::Black, false, (float)ERHIZBuffer::FarPlane, false, 0, ViewRect);

		// Render the scene normally
		{
			SCOPED_DRAW_EVENT(RHICmdList, RenderScene);

			if (bNeedsFlippedRenderTarget)
			{
				// Hijack the render target
				SceneRenderer->ViewFamily.RenderTarget = &FlippedRenderTarget; //-V506
			}
			SceneRenderer->Render(RHICmdList);
			if (bNeedsFlippedRenderTarget)
			{
				// And restore it
				SceneRenderer->ViewFamily.RenderTarget = Target;
			}
		}

		if (bNeedsFlippedRenderTarget)
		{
			// We need to flip this texture upside down (since we depended on tonemapping to fix this on the hdr path)
			SCOPED_DRAW_EVENT(RHICmdList, FlipCapture);

			FIntPoint TargetSize(UnconstrainedViewRect.Width(), UnconstrainedViewRect.Height());
			CopyCaptureToTarget(RHICmdList, Target, TargetSize, View, ViewRect, FlippedRenderTarget.GetTextureParamRef(), true);
		}
		else if (bUseSceneColorTexture && (bIsMobileHDR || SceneRenderer->FeatureLevel >= ERHIFeatureLevel::SM4))
		{
			// Copy the captured scene into the destination texture (only required on HDR or deferred as that implies post-processing)
			SCOPED_DRAW_EVENT(RHICmdList, CaptureSceneColor);
			FIntPoint TargetSize(UnconstrainedViewRect.Width(), UnconstrainedViewRect.Height());
			CopyCaptureToTarget(RHICmdList, Target, TargetSize, View, ViewRect, GSceneRenderTargets.GetSceneColorTexture(), false);
		}

		RHICmdList.CopyToResolveTarget(TextureRenderTarget->GetRenderTargetTexture(), TextureRenderTarget->TextureRHI, false, ResolveParams);
	}

	delete SceneRenderer;
}


FSceneRenderer* FScene::CreateSceneRenderer( USceneCaptureComponent* SceneCaptureComponent, UTextureRenderTarget* TextureTarget, const FMatrix& ViewRotationMatrix, const FVector& ViewLocation, float FOV, float MaxViewDistance, bool bCaptureSceneColour, FPostProcessSettings* PostProcessSettings, float PostProcessBlendWeight )
{
	FIntPoint CaptureSize(TextureTarget->GetSurfaceWidth(), TextureTarget->GetSurfaceHeight());

	FTextureRenderTargetResource* Resource = TextureTarget->GameThread_GetRenderTargetResource();
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		Resource,
		this,
		SceneCaptureComponent->ShowFlags)
		.SetResolveScene(!bCaptureSceneColour));

	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.SetViewRectangle(FIntRect(0, 0, CaptureSize.X, CaptureSize.Y));
	ViewInitOptions.ViewFamily = &ViewFamily;
	ViewInitOptions.ViewOrigin = ViewLocation;
	ViewInitOptions.ViewRotationMatrix = ViewRotationMatrix;
	ViewInitOptions.BackgroundColor = FLinearColor::Black;
	ViewInitOptions.OverrideFarClippingPlaneDistance = MaxViewDistance;
	ViewInitOptions.SceneViewStateInterface = SceneCaptureComponent->GetViewState();

	if (bCaptureSceneColour)
	{
		ViewFamily.EngineShowFlags.PostProcessing = 0;
		ViewInitOptions.OverlayColor = FLinearColor::Black;
	}

	// Build projection matrix
	{
		float XAxisMultiplier;
		float YAxisMultiplier;

		if (CaptureSize.X > CaptureSize.Y)
		{
			// if the viewport is wider than it is tall
			XAxisMultiplier = 1.0f;
			YAxisMultiplier = CaptureSize.X / (float)CaptureSize.Y;
		}
		else
		{
			// if the viewport is taller than it is wide
			XAxisMultiplier = CaptureSize.Y / (float)CaptureSize.X;
			YAxisMultiplier = 1.0f;
		}

		if ((int32)ERHIZBuffer::IsInverted != 0)
		{
			ViewInitOptions.ProjectionMatrix = FReversedZPerspectiveMatrix(
				FOV,
				FOV,
				XAxisMultiplier,
				YAxisMultiplier,
				GNearClippingPlane,
				GNearClippingPlane
				);
		}
		else
		{
			ViewInitOptions.ProjectionMatrix = FPerspectiveMatrix(
				FOV,
				FOV,
				XAxisMultiplier,
				YAxisMultiplier,
				GNearClippingPlane,
				GNearClippingPlane
				);
		}
	}

	FSceneView* View = new FSceneView(ViewInitOptions);

	View->bIsSceneCapture = true;

	check(SceneCaptureComponent);
	for (auto It = SceneCaptureComponent->HiddenComponents.CreateConstIterator(); It; ++It)
	{
		// If the primitive component was destroyed, the weak pointer will return NULL.
		UPrimitiveComponent* PrimitiveComponent = It->Get();
		if (PrimitiveComponent)
		{
			View->HiddenPrimitives.Add(PrimitiveComponent->ComponentId);
		}
	}

	ViewFamily.Views.Add(View);

	View->StartFinalPostprocessSettings(ViewLocation);
	View->OverridePostProcessSettings(*PostProcessSettings, PostProcessBlendWeight);
	View->EndFinalPostprocessSettings(ViewInitOptions);

	return FSceneRenderer::CreateSceneRenderer(&ViewFamily, NULL);
}

void FScene::UpdateSceneCaptureContents(USceneCaptureComponent2D* CaptureComponent)
{
	check(CaptureComponent);

	if (CaptureComponent->TextureTarget)
	{
		FTransform Transform = CaptureComponent->GetComponentToWorld();
		FVector ViewLocation = Transform.GetTranslation();

		// Remove the translation from Transform because we only need rotation.
		Transform.SetTranslation(FVector::ZeroVector);
		FMatrix ViewRotationMatrix = Transform.ToInverseMatrixWithScale();

		// swap axis st. x=z,y=x,z=y (unreal coord space) so that z is up
		ViewRotationMatrix = ViewRotationMatrix * FMatrix(
			FPlane(0,	0,	1,	0),
			FPlane(1,	0,	0,	0),
			FPlane(0,	1,	0,	0),
			FPlane(0,	0,	0,	1));
		const float FOV = CaptureComponent->FOVAngle * (float)PI / 360.0f;
		const bool bUseSceneColorTexture = CaptureComponent->CaptureSource == SCS_SceneColorHDR;
		FSceneRenderer* SceneRenderer = CreateSceneRenderer(CaptureComponent, CaptureComponent->TextureTarget, ViewRotationMatrix , ViewLocation, FOV, CaptureComponent->MaxViewDistanceOverride, bUseSceneColorTexture, &CaptureComponent->PostProcessSettings, CaptureComponent->PostProcessBlendWeight);

		FTextureRenderTargetResource* TextureRenderTarget = CaptureComponent->TextureTarget->GameThread_GetRenderTargetResource();
		const FName OwnerName = CaptureComponent->GetOwner() ? CaptureComponent->GetOwner()->GetFName() : NAME_None;

		ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER( 
			CaptureCommand,
			FSceneRenderer*, SceneRenderer, SceneRenderer,
			FTextureRenderTargetResource*, TextureRenderTarget, TextureRenderTarget,
			FName, OwnerName, OwnerName,
			bool, bUseSceneColorTexture, bUseSceneColorTexture,
		{
			UpdateSceneCaptureContent_RenderThread(RHICmdList, SceneRenderer, TextureRenderTarget, OwnerName, FResolveParams(), bUseSceneColorTexture);
		});
	}
}

void FScene::UpdateSceneCaptureContents(USceneCaptureComponentCube* CaptureComponent)
{
	struct FLocal
	{
		/** Creates a transformation for a cubemap face, following the D3D cubemap layout. */
		static FMatrix CalcCubeFaceTransform(ECubeFace Face)
		{
			static const FVector XAxis(1.f, 0.f, 0.f);
			static const FVector YAxis(0.f, 1.f, 0.f);
			static const FVector ZAxis(0.f, 0.f, 1.f);

			// vectors we will need for our basis
			FVector vUp(YAxis);
			FVector vDir;
			switch (Face)
			{
				case CubeFace_PosX:
					vDir = XAxis;
					break;
				case CubeFace_NegX:
					vDir = -XAxis;
					break;
				case CubeFace_PosY:
					vUp = -ZAxis;
					vDir = YAxis;
					break;
				case CubeFace_NegY:
					vUp = ZAxis;
					vDir = -YAxis;
					break;
				case CubeFace_PosZ:
					vDir = ZAxis;
					break;
				case CubeFace_NegZ:
					vDir = -ZAxis;
					break;
			}
			// derive right vector
			FVector vRight(vUp ^ vDir);
			// create matrix from the 3 axes
			return FBasisVectorMatrix(vRight, vUp, vDir, FVector::ZeroVector);
		}
	} ;

	check(CaptureComponent);

	if (GetFeatureLevel() >= ERHIFeatureLevel::SM4 && CaptureComponent->TextureTarget)
	{
		const float FOV = 90 * (float)PI / 360.0f;
		for (int32 faceidx = 0; faceidx < (int32)ECubeFace::CubeFace_MAX; faceidx++)
		{
			const ECubeFace TargetFace = (ECubeFace)faceidx;
			const FVector Location = CaptureComponent->GetComponentToWorld().GetTranslation();
			const FMatrix ViewRotationMatrix = FLocal::CalcCubeFaceTransform(TargetFace);
			FSceneRenderer* SceneRenderer = CreateSceneRenderer(CaptureComponent, CaptureComponent->TextureTarget, ViewRotationMatrix, Location, FOV, CaptureComponent->MaxViewDistanceOverride);

			FTextureRenderTargetCubeResource* TextureRenderTarget = static_cast<FTextureRenderTargetCubeResource*>(CaptureComponent->TextureTarget->GameThread_GetRenderTargetResource());
			const FName OwnerName = CaptureComponent->GetOwner() ? CaptureComponent->GetOwner()->GetFName() : NAME_None;

			ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER( 
				CaptureCommand,
				FSceneRenderer*, SceneRenderer, SceneRenderer,
				FTextureRenderTargetCubeResource*, TextureRenderTarget, TextureRenderTarget,
				FName, OwnerName, OwnerName,
				ECubeFace, TargetFace, TargetFace,
			{
				UpdateSceneCaptureContent_RenderThread(RHICmdList, SceneRenderer, TextureRenderTarget, OwnerName, FResolveParams(FResolveRect(), TargetFace), true);
			});
		}
	}
}

