/* Copyright 2016 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "GoogleVRHMDPCH.h"
#include "GoogleVRHMD.h"
#include "ScreenRendering.h"

static bool IsDeviceScanlineRacingEnabled()
{
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS

	extern bool AndroidThunkCpp_IsScanlineRacingEnabled();
	return AndroidThunkCpp_IsScanlineRacingEnabled();

#endif

	return false;
}

void FGoogleVRHMD::GenerateDistortionCorrectionIndexBuffer()
{
	// Delete existing indices if they exist
	delete [] DistortionMeshIndices;
	DistortionMeshIndices = nullptr;

	// Allocate new indices
	DistortionMeshIndices = new uint16[NumIndices];

	uint32 InsertIndex = 0;
	for(uint32 y = 0; y < DistortionPointsY - 1; ++y)
	{
		for(uint32 x = 0; x < DistortionPointsX - 1; ++x)
		{
			// Calculate indices for the triangle
			const uint16 BottomLeft =	(y * DistortionPointsX) + x + 0;
			const uint16 BottomRight =	(y * DistortionPointsX) + x + 1;
			const uint16 TopLeft =		(y * DistortionPointsX) + x + 0 + DistortionPointsX;
			const uint16 TopRight =		(y * DistortionPointsX) + x + 1 + DistortionPointsX;

			// Insert indices
			DistortionMeshIndices[InsertIndex + 0] = BottomLeft;
			DistortionMeshIndices[InsertIndex + 1] = BottomRight;
			DistortionMeshIndices[InsertIndex + 2] = TopRight;
			DistortionMeshIndices[InsertIndex + 3] = BottomLeft;
			DistortionMeshIndices[InsertIndex + 4] = TopRight;
			DistortionMeshIndices[InsertIndex + 5] = TopLeft;
			InsertIndex += 6;
		}
	}

	check(InsertIndex == NumIndices);
}

void FGoogleVRHMD::GenerateDistortionCorrectionVertexBuffer(EStereoscopicPass Eye)
{
	FDistortionVertex** UsingPtr = (Eye == eSSP_LEFT_EYE) ? &DistortionMeshVerticesLeftEye : &DistortionMeshVerticesRightEye;
	FDistortionVertex*& Verts = *UsingPtr;

	// Cleanup old data if necessary
	delete [] Verts;
	Verts = nullptr;

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

	// Allocate new vertex buffer
	Verts = new FDistortionVertex[NumVerts];

#if LOG_VIEWER_DATA_FOR_GENERATION
	const TCHAR* EyeString = Eye == eSSP_LEFT_EYE ? TEXT("Left") : TEXT("Right");
	UE_LOG(LogHMD, Log, TEXT("===== Begin Distortion Mesh Eye %s"), EyeString);
	UE_LOG(LogHMD, Log, TEXT("const unsigned int Num%sVertices = %d;"), EyeString, NumVerts);
	UE_LOG(LogHMD, Log, TEXT("FDistortionVertex %sVertices[Num%sVertices] = {"), EyeString, EyeString);
#endif //  LOG_VIEWER_DATA_FOR_GENERATION

	// Fill out distortion vertex info, using GVR Api to calculate transformation coordinates
	const gvr_eye Type = (Eye == eSSP_RIGHT_EYE) ? GVR_RIGHT_EYE : GVR_LEFT_EYE;
	const float XPosOffset = (Type == GVR_LEFT_EYE) ? -1.0f : 0.0f;
	uint32 VertexIndex = 0;
	for(uint32 y = 0; y < DistortionPointsY; ++y)
	{
		for(uint32 x = 0; x < DistortionPointsX; ++x)
		{
			gvr_vec2f UndistortedCoord = gvr_vec2f{float(x) / float(DistortionPointsX - 1), float(y) / float(DistortionPointsY - 1)};
			gvr_vec2f DistortedCoords[3];
			gvr_compute_distorted_point(GVRAPI, Type, UndistortedCoord, DistortedCoords);

			float ScreenYDirection = -1.0f;

#if GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS && HAS_METAL
			// Metal render the scene flipped, so we need to flip to back again.
			bool bSupportsMetal = false;
			GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetal"), bSupportsMetal, GEngineIni);
			if(bSupportsMetal)
			{
				ScreenYDirection = 1.0f;
			}
#endif
			const FVector2D ScreenPos = FVector2D(UndistortedCoord.x * 2.0f - 1.0f, (UndistortedCoord.y * 2.0f - 1.0f) * ScreenYDirection);

			const FVector2D UndistortedUV = FVector2D(UndistortedCoord.x, UndistortedCoord.y);
			const FVector2D OrigRedUV = FVector2D(DistortedCoords[0].x, DistortedCoords[0].y);
			const FVector2D OrigGreenUV = FVector2D(DistortedCoords[1].x, DistortedCoords[1].y);
			const FVector2D OrigBlueUV = FVector2D(DistortedCoords[2].x, DistortedCoords[2].y);

			// Final distorted UVs
			FVector2D FinalRedUV = OrigRedUV;
			FVector2D FinalGreenUV = OrigGreenUV;
			FVector2D FinalBlueUV = OrigBlueUV;

#if GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS && HAS_METAL
			if(bSupportsMetal)
			{
				FinalRedUV.Y = 1.0f - FinalRedUV.Y;
				FinalGreenUV.Y = 1.0f - FinalGreenUV.Y;
				FinalBlueUV.Y = 1.0f - FinalBlueUV.Y;
			}
#endif
			FDistortionVertex FinalVertex = FDistortionVertex{ ScreenPos, FinalRedUV, FinalGreenUV, FinalBlueUV, 1.0f, 0.0f };
			Verts[VertexIndex++] = FinalVertex;
			
#if LOG_VIEWER_DATA_FOR_GENERATION
			UE_LOG(LogHMD, Log, TEXT("\tFDistortionVertex{ FVector2D(%ff, %ff), FVector2D(%ff, %ff), FVector2D(%ff, %ff), FVector2D(%ff, %ff), 1.0f, 0.0f }%s"), 
				ScreenPos.X, ScreenPos.Y,
				FinalRedUV.X, FinalRedUV.Y,
				FinalGreenUV.X, FinalGreenUV.Y,
				FinalBlueUV.X, FinalBlueUV.Y,
				VertexIndex != NumVerts ? TEXT(",") : TEXT(""));
#endif // LOG_VIEWER_DATA_FOR_GENERATION
		}
	}

#if LOG_VIEWER_DATA_FOR_GENERATION
	UE_LOG(LogHMD, Log, TEXT("};"));
	UE_LOG(LogHMD, Log, TEXT("===== End Distortion Mesh Eye %s"), EyeString);
#endif // LOG_VIEWER_DATA_FOR_GENERATION

	check(VertexIndex == NumVerts);

#endif
}

void FGoogleVRHMD::DrawDistortionMesh_RenderThread(struct FRenderingCompositePassContext& Context, const FIntPoint& TextureSize)
{
	const FSceneView& View = Context.View;
	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	FIntPoint ViewportSize = ViewFamily.RenderTarget->GetSizeXY();
	
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

	if(View.StereoPass == eSSP_LEFT_EYE)
	{
		RHICmdList.SetViewport(0, 0, 0.0f, ViewportSize.X / 2, ViewportSize.Y, 1.0f);
		DrawIndexedPrimitiveUP(Context.RHICmdList, PT_TriangleList, 0, NumVerts, NumTris, DistortionMeshIndices,
			sizeof(DistortionMeshIndices[0]), DistortionMeshVerticesLeftEye, sizeof(DistortionMeshVerticesLeftEye[0]));
	}
	else
	{
		RHICmdList.SetViewport(ViewportSize.X / 2, 0, 0.0f, ViewportSize.X, ViewportSize.Y, 1.0f);
		DrawIndexedPrimitiveUP(Context.RHICmdList, PT_TriangleList, 0, NumVerts, NumTris, DistortionMeshIndices,
			sizeof(DistortionMeshIndices[0]), DistortionMeshVerticesRightEye, sizeof(DistortionMeshVerticesRightEye[0]));
	}

#else

	if(GetPreviewViewerType() == EVP_None)
	{
		RHICmdList.SetViewport(0, 0, 0.0f, ViewportSize.X, ViewportSize.Y, 1.0f);

		static const uint32 LocalNumVerts = 8;
		static const uint32 LocalNumTris = 4;

		static const FDistortionVertex Verts[8] =
		{
			// left eye
			{ FVector2D(-0.9f, -0.9f), FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f), 1.0f, 0.0f },
			{ FVector2D(-0.1f, -0.9f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), 1.0f, 0.0f },
			{ FVector2D(-0.1f, 0.9f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), 1.0f, 0.0f },
			{ FVector2D(-0.9f, 0.9f), FVector2D(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(0.0f, 0.0f), 1.0f, 0.0f },
			// right eye
			{ FVector2D(0.1f, -0.9f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), 1.0f, 0.0f },
			{ FVector2D(0.9f, -0.9f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f), 1.0f, 0.0f },
			{ FVector2D(0.9f, 0.9f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.0f), 1.0f, 0.0f },
			{ FVector2D(0.1f, 0.9f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), 1.0f, 0.0f },
		};

		static const uint16 Indices[12] = { /*Left*/ 0, 1, 2, 0, 2, 3, /*Right*/ 4, 5, 6, 4, 6, 7 };

		DrawIndexedPrimitiveUP(Context.RHICmdList, PT_TriangleList, 0, LocalNumVerts, LocalNumTris, &Indices,
			sizeof(Indices[0]), &Verts, sizeof(Verts[0]));
	}
	else
	{
		// Indices are generated, mesh is stored in GoogleVRHMDViewerPreviews.h/cpp

		if(View.StereoPass == eSSP_LEFT_EYE)
		{
			RHICmdList.SetViewport(0, 0, 0.0f, ViewportSize.X / 2, ViewportSize.Y, 1.0f);
			DrawIndexedPrimitiveUP(Context.RHICmdList, PT_TriangleList, 0, GetPreviewViewerNumVertices(eSSP_LEFT_EYE), NumTris, DistortionMeshIndices,
				sizeof(DistortionMeshIndices[0]), GetPreviewViewerVertices(eSSP_LEFT_EYE), sizeof(GetPreviewViewerVertices(eSSP_LEFT_EYE)[0]));
		}
		else
		{
			RHICmdList.SetViewport(ViewportSize.X / 2, 0, 0.0f, ViewportSize.X, ViewportSize.Y, 1.0f);
			DrawIndexedPrimitiveUP(Context.RHICmdList, PT_TriangleList, 0, GetPreviewViewerNumVertices(eSSP_RIGHT_EYE), NumTris, DistortionMeshIndices,
				sizeof(DistortionMeshIndices[0]), GetPreviewViewerVertices(eSSP_RIGHT_EYE), sizeof(GetPreviewViewerVertices(eSSP_RIGHT_EYE)[0]));
		}
	}

#endif
}

// If bfullResourceResolve is true: A no-op draw call is submitted which resolves all pending states
// If bFullResourceResolve is false, A no-op clear is submitted which resolves RT's only
static void ResolvePendingRenderTarget(FRHICommandListImmediate& RHICmdList, IRendererModule* RendererModule, bool bFullResourceResolve = true)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

	// HACK! Need to workaround UE4's caching mechanism. This causes the pending commands to actually apply to the device.
	class FFakeIndexBuffer : public FIndexBuffer
	{
	public:
		/** Initialize the RHI for this rendering resource */
		void InitRHI() override
		{
			// Indices 0 - 5 are used for rendering a quad. Indices 6 - 8 are used for triangle optimization. 
			const uint16 Indices[] = { 0, 1, 2, 2, 1, 3, 0, 4, 5 };
		
			TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> IndexBuffer;
			uint32 InternalNumIndices = ARRAY_COUNT(Indices);
			IndexBuffer.AddUninitialized(InternalNumIndices);
			FMemory::Memcpy(IndexBuffer.GetData(), Indices, InternalNumIndices * sizeof(uint16));

			// Create index buffer. Fill buffer with initial data upon creation
			FRHIResourceCreateInfo CreateInfo(&IndexBuffer);
			IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), IndexBuffer.GetResourceDataSize(), BUF_Static, CreateInfo);
		}
	};
	static TGlobalResource<FFakeIndexBuffer> FakeIndexBuffer;

	if(bFullResourceResolve)
	{
		const auto FeatureLevel = GMaxRHIFeatureLevel;
		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
		TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI, *VertexShader, *PixelShader);

		RHICmdList.DrawIndexedPrimitive(
			FakeIndexBuffer.IndexBufferRHI,
			PT_TriangleList,
			/*BaseVertexIndex=*/ 0,
			/*MinIndex=*/ 0,
			/*NumVertices=*/ 0,
			/*StartIndex=*/ 0,
			/*NumPrimitives=*/ 0,
			/*NumInstances=*/ 1
			);
	}
	else
	{
		RHICmdList.ClearMRT(false, 0, nullptr, false, 0.0f, false, 0, FIntRect());
	}

	RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);

#endif
}

void FGoogleVRHMD::RenderTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef BackBuffer, FTexture2DRHIParamRef SrcTexture) const
{
	check(IsInRenderingThread());

	const uint32 ViewportWidth = BackBuffer->GetSizeX();
	const uint32 ViewportHeight = BackBuffer->GetSizeY();
	const uint32 TextureWidth = SrcTexture->GetSizeX();
	const uint32 TextureHeight = SrcTexture->GetSizeY();
	
	//UE_LOG(LogHMD, Log, TEXT("RenderTexture_RenderThread() Viewport:(%d, %d) Texture:(%d, %d) BackBuffer=%p SrcTexture=%p"), ViewportWidth, ViewportHeight, TextureWidth, TextureHeight, BackBuffer, SrcTexture);

	RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
	
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

	if(IsUsingGVRApiDistortionCorrection())
	{
		if(!bDistortionCorrectionEnabled)
		{
			return;
		}

		// Perform render
		if(!bUseOffscreenFramebuffers)
		{
			// With proper resolution scaling, this should always be true!
			check(ViewportWidth == TextureWidth);
			check(ViewportHeight == TextureHeight);

			// Set target to back buffer
			SetRenderTarget(RHICmdList, BackBuffer, FTextureRHIRef());
			RHICmdList.SetViewport(0, 0, 0, ViewportWidth, ViewportHeight, 1.0f);
			ResolvePendingRenderTarget(RHICmdList, RendererModule);

			gvr_distort_to_screen(GVRAPI, *reinterpret_cast<GLuint*>(SrcTexture->GetNativeResource()),
				CachedDistortedRenderTextureParams,
				&CachedPose,
				&CachedFuturePoseTime);
		}
		else
		{
			if(!IsDeviceScanlineRacingEnabled())
			{
				// With proper resolution scaling, this should always be true!
				check(ViewportWidth == TextureWidth);
				check(ViewportHeight == TextureHeight);

				// Set target to back buffer
				SetRenderTarget(RHICmdList, BackBuffer, FTextureRHIRef());
				RHICmdList.SetViewport(0, 0, 0, ViewportWidth, ViewportHeight, 1.0f);
				ResolvePendingRenderTarget(RHICmdList, RendererModule);
			}

			gvr_distort_offscreen_framebuffer_to_screen(GVRAPI, CustomPresent->GetFrameBufferId(),
				CachedDistortedRenderTextureParams,
				&CachedPose,
				&CachedFuturePoseTime);
		}
	}
	else // falls through on purpose

#endif // GOOGLEVRHMD_SUPPORTED_PLATFORMS

	// Just render directly to output
	{
		SetRenderTarget(RHICmdList, BackBuffer, FTextureRHIRef());
		RHICmdList.SetViewport(0, 0, 0, ViewportWidth, ViewportHeight, 1.0f);

		const auto FeatureLevel = GMaxRHIFeatureLevel;
		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
		TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI, *VertexShader, *PixelShader);

		PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SrcTexture);

		RendererModule->DrawRectangle(
			RHICmdList,
			0, 0,
			ViewportWidth, ViewportHeight,
			0.0f, 0.0f,
			1.0f, 1.0f,
			FIntPoint(ViewportWidth, ViewportHeight),
			FIntPoint(1, 1),
			*VertexShader,
			EDRF_Default);
	}
}

bool FGoogleVRHMD::HasHiddenAreaMesh() const
{
	return false;
}

void FGoogleVRHMD::DrawHiddenAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
{
}

bool FGoogleVRHMD::HasVisibleAreaMesh() const
{
	return false;
}

void FGoogleVRHMD::DrawVisibleAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
{
}

FRHICustomPresent* FGoogleVRHMD::GetCustomPresent()
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

	if(bUseOffscreenFramebuffers)
	{
		return CustomPresent;
	}

#endif

	return nullptr;
}

uint32 FGoogleVRHMD::GetNumberOfBufferedFrames() const
{
	return 1;
}

bool FGoogleVRHMD::AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 InFlags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples)
{
	check(Index == 0);
	check(SizeX != 0 && SizeY != 0);
	check(IsInGameThread() && IsInRenderingThread()); // checking if rendering thread is suspended

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	if(bUseOffscreenFramebuffers)
	{
		return CustomPresent ? CustomPresent->AllocateRenderTargetTexture(Index, SizeX, SizeY, Format, NumMips, InFlags, TargetableTextureFlags, OutTargetableTexture, OutShaderResourceTexture, NumSamples) : false;
	}
#endif

	return false;
}

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

FGoogleVRHMDTexture2DSet::FGoogleVRHMDTexture2DSet(
	class FOpenGLDynamicRHI* InGLRHI,
	GLuint InResource,
	GLenum InTarget,
	GLenum InAttachment,
	uint32 InSizeX,
	uint32 InSizeY,
	uint32 InSizeZ,
	uint32 InNumMips,
	uint32 InNumSamples,
	uint32 InArraySize,
	EPixelFormat InFormat,
	bool bInCubemap,
	bool bInAllocatedStorage,
	uint32 InFlags,
	uint8* InTextureRange
)
	: FOpenGLTexture2D(
	InGLRHI,
	InResource,
	InTarget,
	InAttachment,
	InSizeX,
	InSizeY,
	InSizeZ,
	InNumMips,
	InNumSamples,
	InArraySize,
	InFormat,
	bInCubemap,
	bInAllocatedStorage,
	InFlags,
	InTextureRange,
	FClearValueBinding::Black
	)
	, ResourceId(-1)
{
}

FGoogleVRHMDTexture2DSet::~FGoogleVRHMDTexture2DSet()
{
	if(ResourceId != 0)
	{
		gvr_release_offscreen_framebuffer(GVRAPI, ResourceId);
	}
}

int FGoogleVRHMDTexture2DSet::GetResourceId()
{
	return ResourceId;
}

void FGoogleVRHMDTexture2DSet::InitializeWithActualResource(int InResourceId)
{
	check(InResourceId != 0);
	ResourceId = InResourceId;
}

FGoogleVRHMDTexture2DSet* FGoogleVRHMDTexture2DSet::CreateTexture2DSet(
	FOpenGLDynamicRHI* InGLRHI,
	uint32 DesiredSizeX, uint32 DesiredSizeY,
	uint32 InNumSamples,
	EPixelFormat InFormat,
	uint32 InFlags)
{
	// Get screen size
	gvr_recti Bounds = gvr_get_window_bounds(GVRAPI);
	int ActualScreenSizeX = Bounds.right - Bounds.left;
	int ActualScreenSizeY = Bounds.top - Bounds.bottom;

	// Should this match the desired size? What about resolution scaling?
	check(DesiredSizeX == (uint32)ActualScreenSizeX);
	check(DesiredSizeY == (uint32)ActualScreenSizeY);


	// Get MobileMSAA Settings and use that to set the MSAA on offscreen buffer
	// TODO: Use the MSAA setting from Unreal.
	// We are using the OffscreenBufferMSAASetting in our plugin for now since Unreal only support
	// 2x MSAA for android using gles.

	// Create resource using GVR Api
	gvr_framebuffer_spec* FramebufferSpec = gvr_framebuffer_spec_create(GVRAPI);
	gvr_sizei TextureSize = gvr_sizei{ActualScreenSizeX, ActualScreenSizeY};
	gvr_framebuffer_spec_set_size(FramebufferSpec, TextureSize);
	gvr_framebuffer_spec_set_samples(FramebufferSpec, FGoogleVRHMD::GetOffscreenBufferMSAASetting());
	int ResourceIdentifier = gvr_create_offscreen_framebuffer(GVRAPI, FramebufferSpec);
	gvr_framebuffer_spec_destroy(&FramebufferSpec);

	// Fail?
	if(ResourceIdentifier == 0)
	{
		UE_LOG(LogHMD, Warning, TEXT("Failed to create offscreen frame buffer! Returned Handle: [%d]"), ResourceIdentifier);
		return nullptr;
	}

	GLenum Target = (InNumSamples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
	GLenum Attachment = GL_NONE;//GL_COLOR_ATTACHMENT0;
	bool bAllocatedStorage = false;
	uint32 NumMips = 1;
	uint8* TextureRange = nullptr;

	FGoogleVRHMDTexture2DSet* NewTextureSet = new FGoogleVRHMDTexture2DSet(
		InGLRHI, 0, Target, Attachment, ActualScreenSizeX, ActualScreenSizeY, 0, NumMips, InNumSamples, 1, InFormat, false, bAllocatedStorage, InFlags, TextureRange);

	UE_LOG(LogHMD, Log, TEXT("Created FGoogleVRHMDTexture2DSet of size (%d, %d), resource id [%d], NewTextureSet [%p]"), ActualScreenSizeX, ActualScreenSizeY, ResourceIdentifier, NewTextureSet);

	NewTextureSet->InitializeWithActualResource(ResourceIdentifier);

	return NewTextureSet;
}

bool FGoogleVRHMDCustomPresent::AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 InFlags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples)
{
	check(SizeX != 0 && SizeY != 0);

	InFlags |= TargetableTextureFlags;

	FOpenGLDynamicRHI* GLRHI = static_cast<FOpenGLDynamicRHI*>(GDynamicRHI);
	TextureSet = FGoogleVRHMDTexture2DSet::CreateTexture2DSet(
		GLRHI,
		SizeX, SizeY,
		1,
		EPixelFormat(Format),
		TexCreate_RenderTargetable | TexCreate_ShaderResource
		);

	if(!TextureSet.IsValid())
	{
		return false;
	}

	OutTargetableTexture = TextureSet->GetTexture2D();
	OutShaderResourceTexture = TextureSet->GetTexture2D();

	return true;
}

void FGoogleVRHMDCustomPresent::BeginRendering(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	if(TextureSet.IsValid())
	{
		// Binds a framebuffer to the pipeline.  When scanline racing, this resource changes every frame.  We need to grab the texture
		// attachment id to apply to the texture set below.
		gvr_set_active_offscreen_framebuffer(GVRAPI, TextureSet->GetResourceId());

		// API returns framebuffer resource, but we need the texture resource for the pipeline
		check(PLATFORM_USES_ES2); // Some craziness will only work on OpenGL platforms.
		GLint TextureId = 0;
		glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &TextureId);
		TextureSet->Resource = TextureId;
	}
#endif
}

void FGoogleVRHMDCustomPresent::FinishRendering()
{
}

void FGoogleVRHMDCustomPresent::UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI)
{
	check(IsInGameThread());
	check(ViewportRHI);

	if(TextureSet.IsValid())
	{
		ViewportRHI->SetCustomPresent(this);
	}
}

int FGoogleVRHMDCustomPresent::GetFrameBufferId()
{
	if(TextureSet.IsValid())
	{
		return TextureSet->GetResourceId();
	}
	else
	{
		return -1;
	}
}

FGoogleVRHMDCustomPresent::FGoogleVRHMDCustomPresent(FGoogleVRHMD* InHMD)
	: FRHICustomPresent(nullptr)
	, HMD(InHMD)
{
}

FGoogleVRHMDCustomPresent::~FGoogleVRHMDCustomPresent()
{
}

void FGoogleVRHMDCustomPresent::OnBackBufferResize()
{
}

bool FGoogleVRHMDCustomPresent::Present(int32& InOutSyncInterval)
{
	FinishRendering();

	// Note: true causes normal swapbuffers(), false prevents normal swapbuffers()
	if(TextureSet.IsValid() && IsDeviceScanlineRacingEnabled())
	{
		return false;
	}
	else
	{
		return true;
	}
}

#endif // GOOGLEVRHMD_SUPPORTED_PLATFORMS
