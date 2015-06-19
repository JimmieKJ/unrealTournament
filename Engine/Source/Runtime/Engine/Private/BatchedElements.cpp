// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "BatchedElements.h"
#include "GlobalShader.h"
#include "ShaderParameters.h"
#include "RHIStaticStates.h"
#include "SimpleElementShaders.h"
#include "RHICommandList.h"


DEFINE_LOG_CATEGORY_STATIC(LogBatchedElements, Log, All);

/** The simple element vertex declaration. */
TGlobalResource<FSimpleElementVertexDeclaration> GSimpleElementVertexDeclaration;

EBlendModeFilter::Type GetBlendModeFilter(ESimpleElementBlendMode BlendMode)
{
	if (BlendMode == SE_BLEND_Opaque || BlendMode == SE_BLEND_Masked || BlendMode == SE_BLEND_MaskedDistanceField || BlendMode == SE_BLEND_MaskedDistanceFieldShadowed)
	{
		return EBlendModeFilter::OpaqueAndMasked;
	}
	else
	{
		return EBlendModeFilter::Translucent;
	}
}

void FBatchedElements::AddLine(const FVector& Start, const FVector& End, const FLinearColor& Color, FHitProxyId HitProxyId, float Thickness, float DepthBias, bool bScreenSpace)
{
	// Ensure the line isn't masked out.  Some legacy code relies on Color.A being ignored.
	FLinearColor OpaqueColor(Color);
	OpaqueColor.A = 1;

	if (Thickness == 0.0f)
	{
		if (DepthBias == 0.0f)
		{
			new(LineVertices) FSimpleElementVertex(Start, FVector2D::ZeroVector, OpaqueColor, HitProxyId);
			new(LineVertices) FSimpleElementVertex(End, FVector2D::ZeroVector, OpaqueColor, HitProxyId);
		}
		else
		{
			// Draw degenerate triangles in wireframe mode to support depth bias (d3d11 and opengl3 don't support depth bias on line primitives, but do on wireframes)
			FBatchedWireTris* WireTri = new(WireTris) FBatchedWireTris();
			WireTri->DepthBias = DepthBias;
			new(WireTriVerts) FSimpleElementVertex(Start, FVector2D::ZeroVector, OpaqueColor, HitProxyId);
			new(WireTriVerts) FSimpleElementVertex(End, FVector2D::ZeroVector, OpaqueColor, HitProxyId);
			new(WireTriVerts) FSimpleElementVertex(End, FVector2D::ZeroVector, OpaqueColor, HitProxyId);
		}
	}
	else
	{
		FBatchedThickLines* ThickLine = new(ThickLines) FBatchedThickLines;
		ThickLine->Start = Start;
		ThickLine->End = End;
		ThickLine->Thickness = Thickness;
		ThickLine->Color = OpaqueColor;
		ThickLine->HitProxyId = HitProxyId;
		ThickLine->DepthBias = DepthBias;
		ThickLine->bScreenSpace = bScreenSpace;
	}
}

void FBatchedElements::AddPoint(const FVector& Position,float Size,const FLinearColor& Color,FHitProxyId HitProxyId)
{
	// Ensure the point isn't masked out.  Some legacy code relies on Color.A being ignored.
	FLinearColor OpaqueColor(Color);
	OpaqueColor.A = 1;

	FBatchedPoint* Point = new(Points) FBatchedPoint;
	Point->Position = Position;
	Point->Size = Size;
	Point->Color = OpaqueColor;
	Point->HitProxyId = HitProxyId;
}

int32 FBatchedElements::AddVertex(const FVector4& InPosition,const FVector2D& InTextureCoordinate,const FLinearColor& InColor,FHitProxyId HitProxyId)
{
	int32 VertexIndex = MeshVertices.Num();
	new(MeshVertices) FSimpleElementVertex(InPosition,InTextureCoordinate,InColor,HitProxyId);
	return VertexIndex;
}

void FBatchedElements::AddQuadVertex(const FVector4& InPosition,const FVector2D& InTextureCoordinate,const FLinearColor& InColor,FHitProxyId HitProxyId, const FTexture* Texture,ESimpleElementBlendMode BlendMode)
{
	check(GSupportsQuads);

	// Find an existing mesh element for the given texture.
	FBatchedQuadMeshElement* MeshElement = NULL;
	for (int32 MeshIndex = 0;MeshIndex < QuadMeshElements.Num();MeshIndex++)
	{
		if(QuadMeshElements[MeshIndex].Texture == Texture && QuadMeshElements[MeshIndex].BlendMode == BlendMode)
		{
			MeshElement = &QuadMeshElements[MeshIndex];
			break;
		}
	}

	if (!MeshElement)
	{
		// Create a new mesh element for the texture if this is the first triangle encountered using it.
		MeshElement = new(QuadMeshElements) FBatchedQuadMeshElement;
		MeshElement->Texture = Texture;
		MeshElement->BlendMode = BlendMode;
	}

	new(MeshElement->Vertices) FSimpleElementVertex(InPosition,InTextureCoordinate,InColor,HitProxyId);
}

/** Adds a triangle to the batch. */
void FBatchedElements::AddTriangle(int32 V0,int32 V1,int32 V2,const FTexture* Texture,EBlendMode BlendMode)
{
	ESimpleElementBlendMode SimpleElementBlendMode = SE_BLEND_Opaque;
	switch (BlendMode)
	{
	case BLEND_Opaque:
		SimpleElementBlendMode = SE_BLEND_Opaque; 
		break;
	case BLEND_Masked:
	case BLEND_Translucent:
		SimpleElementBlendMode = SE_BLEND_Translucent; 
		break;
	case BLEND_Additive:
		SimpleElementBlendMode = SE_BLEND_Additive; 
		break;
	case BLEND_Modulate:
		SimpleElementBlendMode = SE_BLEND_Modulate; 
		break;
	};	
	AddTriangle(V0,V1,V2,Texture,SimpleElementBlendMode);
}

void FBatchedElements::AddTriangle(int32 V0, int32 V1, int32 V2, const FTexture* Texture, ESimpleElementBlendMode BlendMode, const FDepthFieldGlowInfo& GlowInfo)
{
	AddTriangleExtensive( V0, V1, V2, NULL, Texture, BlendMode, GlowInfo );
}

	
void FBatchedElements::AddTriangle(int32 V0,int32 V1,int32 V2,FBatchedElementParameters* BatchedElementParameters,ESimpleElementBlendMode BlendMode)
{
	AddTriangleExtensive( V0, V1, V2, BatchedElementParameters, NULL, BlendMode );
}


void FBatchedElements::AddTriangleExtensive(int32 V0,int32 V1,int32 V2,FBatchedElementParameters* BatchedElementParameters,const FTexture* Texture,ESimpleElementBlendMode BlendMode, const FDepthFieldGlowInfo& GlowInfo)
{
	// Find an existing mesh element for the given texture and blend mode
	FBatchedMeshElement* MeshElement = NULL;
	for(int32 MeshIndex = 0;MeshIndex < MeshElements.Num();MeshIndex++)
	{
		const FBatchedMeshElement& CurMeshElement = MeshElements[MeshIndex];
		if( CurMeshElement.Texture == Texture && 
			CurMeshElement.BatchedElementParameters.GetReference() == BatchedElementParameters &&
			CurMeshElement.BlendMode == BlendMode &&
			// make sure we are not overflowing on indices
			(CurMeshElement.Indices.Num()+3) < MaxMeshIndicesAllowed &&
			CurMeshElement.GlowInfo == GlowInfo )
		{
			// make sure we are not overflowing on vertices
			int32 DeltaV0 = (V0 - (int32)CurMeshElement.MinVertex);
			int32 DeltaV1 = (V1 - (int32)CurMeshElement.MinVertex);
			int32 DeltaV2 = (V2 - (int32)CurMeshElement.MinVertex);
			if( DeltaV0 >= 0 && DeltaV0 < MaxMeshVerticesAllowed &&
				DeltaV1 >= 0 && DeltaV1 < MaxMeshVerticesAllowed &&
				DeltaV2 >= 0 && DeltaV2 < MaxMeshVerticesAllowed )
			{
				MeshElement = &MeshElements[MeshIndex];
				break;
			}			
		}
	}
	if(!MeshElement)
	{
		// make sure that vertex indices are close enough to fit within MaxVerticesAllowed
		if( FMath::Abs(V0 - V1) >= MaxMeshVerticesAllowed ||
			FMath::Abs(V0 - V2) >= MaxMeshVerticesAllowed )
		{
			UE_LOG(LogBatchedElements, Warning, TEXT("Omitting FBatchedElements::AddTriangle due to sparce vertices V0=%i,V1=%i,V2=%i"),V0,V1,V2);
		}
		else
		{
			// Create a new mesh element for the texture if this is the first triangle encountered using it.
			MeshElement = new(MeshElements) FBatchedMeshElement;
			MeshElement->Texture = Texture;
			MeshElement->BatchedElementParameters = BatchedElementParameters;
			MeshElement->BlendMode = BlendMode;
			MeshElement->GlowInfo = GlowInfo;
			MeshElement->MaxVertex = V0;
			// keep track of the min vertex index used
			MeshElement->MinVertex = FMath::Min(FMath::Min(V0,V1),V2);
		}
	}

	if( MeshElement )
	{
		// Add the triangle's indices to the mesh element's index array.
		MeshElement->Indices.Add(V0 - MeshElement->MinVertex);
		MeshElement->Indices.Add(V1 - MeshElement->MinVertex);
		MeshElement->Indices.Add(V2 - MeshElement->MinVertex);

		// keep track of max vertex used in this mesh batch
		MeshElement->MaxVertex = FMath::Max(FMath::Max(FMath::Max(V0,(int32)MeshElement->MaxVertex),V1),V2);
	}
}

/** 
* Reserves space in mesh vertex array
* 
* @param NumMeshVerts - number of verts to reserve space for
* @param Texture - used to find the mesh element entry
* @param BlendMode - used to find the mesh element entry
*/
void FBatchedElements::AddReserveVertices(int32 NumMeshVerts)
{
	MeshVertices.Reserve( MeshVertices.Num() + NumMeshVerts );
}

/** 
 * Reserves space in line vertex array
 * 
 * @param NumLines - number of lines to reserve space for
 * @param bDepthBiased - whether reserving depth-biased lines or non-biased lines
 * @param bThickLines - whether reserving regular lines or thick lines
 */
void FBatchedElements::AddReserveLines(int32 NumLines, bool bDepthBiased, bool bThickLines)
{
	if (!bThickLines)
	{
		if (!bDepthBiased)
		{
			LineVertices.Reserve(LineVertices.Num() + NumLines * 2);
		}
		else
		{
			WireTris.Reserve(WireTris.Num() + NumLines);
			WireTriVerts.Reserve(WireTriVerts.Num() + NumLines * 3);
		}
	}
	else
	{
		ThickLines.Reserve(ThickLines.Num() + NumLines * 2);
	}
}

/** 
* Reserves space in triangle arrays 
* 
* @param NumMeshTriangles - number of triangles to reserve space for
* @param Texture - used to find the mesh element entry
* @param BlendMode - used to find the mesh element entry
*/
void FBatchedElements::AddReserveTriangles(int32 NumMeshTriangles,const FTexture* Texture,ESimpleElementBlendMode BlendMode)
{
	for(int32 MeshIndex = 0;MeshIndex < MeshElements.Num();MeshIndex++)
	{
		FBatchedMeshElement& CurMeshElement = MeshElements[MeshIndex];
		if( CurMeshElement.Texture == Texture && 
			CurMeshElement.BatchedElementParameters.GetReference() == NULL &&
			CurMeshElement.BlendMode == BlendMode &&
			(CurMeshElement.Indices.Num()+3) < MaxMeshIndicesAllowed )
		{
			CurMeshElement.Indices.Reserve( CurMeshElement.Indices.Num() + NumMeshTriangles );
			break;
		}
	}	
}

void FBatchedElements::AddSprite(
	const FVector& Position,
	float SizeX,
	float SizeY,
	const FTexture* Texture,
	const FLinearColor& Color,
	FHitProxyId HitProxyId,
	float U,
	float UL,
	float V,
	float VL,
	uint8 BlendMode
	)
{
	FBatchedSprite* Sprite = new(Sprites) FBatchedSprite;
	Sprite->Position = Position;
	Sprite->SizeX = SizeX;
	Sprite->SizeY = SizeY;
	Sprite->Texture = Texture;
	Sprite->Color = Color;
	Sprite->HitProxyId = HitProxyId;
	Sprite->U = U;
	Sprite->UL = UL == 0.f ? Texture->GetSizeX() : UL;
	Sprite->V = V;
	Sprite->VL = VL == 0.f ? Texture->GetSizeY() : VL;
	Sprite->BlendMode = BlendMode;
}

/** Translates a ESimpleElementBlendMode into a RHI state change for rendering a mesh with the blend mode normally. */
static void SetBlendState(FRHICommandList& RHICmdList, ESimpleElementBlendMode BlendMode)
{
	switch(BlendMode)
	{
	case SE_BLEND_Opaque:
	case SE_BLEND_Masked:
	case SE_BLEND_MaskedDistanceField:
	case SE_BLEND_MaskedDistanceFieldShadowed:
		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
		break;
	case SE_BLEND_Translucent:
	case SE_BLEND_TranslucentDistanceField:
	case SE_BLEND_TranslucentDistanceFieldShadowed:
	case SE_BLEND_TranslucentAlphaOnly:
		RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI());
		break;
	case SE_BLEND_Additive:
		RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI());
		break;
	case SE_BLEND_Modulate:
		RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI());
		break;
	case SE_BLEND_AlphaComposite:
		RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI());
		break;
	case SE_BLEND_AlphaBlend:
		RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA,BO_Add,BF_SourceAlpha,BF_InverseSourceAlpha,BO_Add,BF_SourceAlpha,BF_InverseSourceAlpha>::GetRHI());
		break;
	case SE_BLEND_RGBA_MASK_END:
	case SE_BLEND_RGBA_MASK_START:
		break;
	}
}

/** Translates a ESimpleElementBlendMode into a RHI state change for rendering a mesh with the blend mode for hit testing. */
static void SetHitTestingBlendState(FRHICommandList& RHICmdList, ESimpleElementBlendMode BlendMode)
{
	switch(BlendMode)
	{
	case SE_BLEND_Opaque:
		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
		break;
	case SE_BLEND_Masked:
	case SE_BLEND_MaskedDistanceField:
	case SE_BLEND_MaskedDistanceFieldShadowed:
		RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI());
		break;
	case SE_BLEND_AlphaComposite:
	case SE_BLEND_AlphaBlend:
	case SE_BLEND_Translucent:
	case SE_BLEND_TranslucentDistanceField:
	case SE_BLEND_TranslucentDistanceFieldShadowed:
	case SE_BLEND_TranslucentAlphaOnly:
		RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI());
		break;
	case SE_BLEND_Additive:
		RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI());
		break;
	case SE_BLEND_Modulate:
		RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI());
		break;
	case SE_BLEND_RGBA_MASK_END:
	case SE_BLEND_RGBA_MASK_START:
		break;
	}
}

FGlobalBoundShaderState FBatchedElements::SimpleBoundShaderState;
FGlobalBoundShaderState FBatchedElements::RegularSRGBBoundShaderState;
FGlobalBoundShaderState FBatchedElements::RegularLinearBoundShaderState;
FGlobalBoundShaderState FBatchedElements::MaskedBoundShaderState;
FGlobalBoundShaderState FBatchedElements::DistanceFieldBoundShaderState;
FGlobalBoundShaderState FBatchedElements::HitTestingBoundShaderState;
FGlobalBoundShaderState FBatchedElements::ColorChannelMaskShaderState;
FGlobalBoundShaderState FBatchedElements::AlphaOnlyShaderState;
FGlobalBoundShaderState FBatchedElements::GammaAlphaOnlyShaderState;

/** Global alpha ref test value for rendering masked batched elements */
float GBatchedElementAlphaRefVal = 128.f;
/** Global smoothing width for rendering batched elements with distance field blend modes */
float GBatchedElementSmoothWidth = 12;

/**
 * Sets the appropriate vertex and pixel shader.
 */
void FBatchedElements::PrepareShaders(
	FRHICommandList& RHICmdList,
	ERHIFeatureLevel::Type FeatureLevel,
	ESimpleElementBlendMode BlendMode,
	const FMatrix& Transform,
	bool bSwitchVerticalAxis,
	FBatchedElementParameters* BatchedElementParameters,
	const FTexture* Texture,
	bool bHitTesting,
	float Gamma,
	const FDepthFieldGlowInfo* GlowInfo,
	const FSceneView* View,
	FTexture2DRHIRef DepthTexture
	) const
{
	// used to mask individual channels and desaturate
	FMatrix ColorWeights( FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 1, 0), FPlane(0, 0, 0, 0) );
	
	float GammaToUse = Gamma;
	if(BlendMode >= SE_BLEND_RGBA_MASK_START && BlendMode <= SE_BLEND_RGBA_MASK_END)
	{
		/*
		* Red, Green, Blue and Alpha color weights all initialized to 0
		*/
		FPlane R( 0.0f, 0.0f, 0.0f, 0.0f );
		FPlane G( 0.0f, 0.0f, 0.0f, 0.0f );
		FPlane B( 0.0f, 0.0f, 0.0f, 0.0f );
		FPlane A( 0.0f,	0.0f, 0.0f, 0.0f );

		/*
		* Extract the color components from the in BlendMode to determine which channels should be active
		*/
		uint32 BlendMask = ( ( uint32 )BlendMode ) - SE_BLEND_RGBA_MASK_START;

		bool bRedChannel = ( BlendMask & ( 1 << 0 ) ) != 0;
		bool bGreenChannel = ( BlendMask & ( 1 << 1 ) ) != 0;
		bool bBlueChannel = ( BlendMask & ( 1 << 2 ) ) != 0;
		bool bAlphaChannel = ( BlendMask & ( 1 << 3 ) ) != 0;
		bool bDesaturate = ( BlendMask & ( 1 << 4 ) ) != 0;
		bool bAlphaOnly = bAlphaChannel && !bRedChannel && !bGreenChannel && !bBlueChannel;
		uint32 NumChannelsOn = ( bRedChannel ? 1 : 0 ) + ( bGreenChannel ? 1 : 0 ) + ( bBlueChannel ? 1 : 0 );
		GammaToUse = bAlphaOnly? 1.0f: Gamma;

		// If we are only to draw the alpha channel, make the Blend state opaque, to allow easy identification of the alpha values
		if( bAlphaOnly )
		{
			SetBlendState(RHICmdList, SE_BLEND_Opaque);

			R.W = G.W = B.W = 1.0f;
		}
		else
		{
			SetBlendState(RHICmdList, !bAlphaChannel ? SE_BLEND_Opaque : SE_BLEND_Translucent); // If alpha channel is disabled, do not allow alpha blending

			// Determine the red, green, blue and alpha components of their respective weights to enable that colours prominence
			R.X = bRedChannel ? 1.0f : 0.0f;
			G.Y = bGreenChannel ? 1.0f : 0.0f;
			B.Z = bBlueChannel ? 1.0f : 0.0f;
			A.W = bAlphaChannel ? 1.0f : 0.0f;

			/*
			* Determine if desaturation is enabled, if so, we determine the output colour based on the number of active channels that are displayed
			* e.g. if Only the red and green channels are being viewed, then the desaturation of the image will be divided by 2
			*/
			if( bDesaturate && NumChannelsOn )
			{
				float ValR, ValG, ValB;
				ValR = R.X / NumChannelsOn;
				ValG = G.Y / NumChannelsOn;
				ValB = B.Z / NumChannelsOn;
				R = FPlane( ValR, ValG, ValB, 0 );
				G = FPlane( ValR, ValG, ValB, 0 );
				B = FPlane( ValR, ValG, ValB, 0 );
			}
		}

		ColorWeights = FMatrix( R, G, B, A );
	}
	
	if( BatchedElementParameters != NULL )
	{
		// Use the vertex/pixel shader that we were given
		BatchedElementParameters->BindShaders(RHICmdList, FeatureLevel, Transform, Gamma, ColorWeights, Texture);
	}
	else
	{
		TShaderMapRef<FSimpleElementVS> VertexShader(GetGlobalShaderMap(FeatureLevel));
		    
	    if (bHitTesting)
	    {
			TShaderMapRef<FSimpleElementHitProxyPS> HitTestingPixelShader(GetGlobalShaderMap(FeatureLevel));
			SetGlobalBoundShaderState(RHICmdList, FeatureLevel, HitTestingBoundShaderState, GSimpleElementVertexDeclaration.VertexDeclarationRHI,
				*VertexShader, *HitTestingPixelShader);

			HitTestingPixelShader->SetParameters(RHICmdList, Texture);
			SetHitTestingBlendState(RHICmdList, BlendMode);
		    
	    }
	    else
	    {
		    if (BlendMode == SE_BLEND_Masked)
		    {
			    // use clip() in the shader instead of alpha testing as cards that don't support floating point blending
			    // also don't support alpha testing to floating point render targets
				RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());

				if (Texture->bSRGB)
				{
					TShaderMapRef<FSimpleElementMaskedGammaPS_SRGB> MaskedPixelShader(GetGlobalShaderMap(FeatureLevel));
					SetGlobalBoundShaderState(RHICmdList, FeatureLevel, MaskedBoundShaderState, GSimpleElementVertexDeclaration.VertexDeclarationRHI,
						*VertexShader, *MaskedPixelShader);

					MaskedPixelShader->SetEditorCompositingParameters(RHICmdList, View, DepthTexture);
					MaskedPixelShader->SetParameters(RHICmdList, Texture, Gamma, GBatchedElementAlphaRefVal / 255.0f, BlendMode);
				}
				else
				{
					TShaderMapRef<FSimpleElementMaskedGammaPS_Linear> MaskedPixelShader(GetGlobalShaderMap(FeatureLevel));
					SetGlobalBoundShaderState(RHICmdList, FeatureLevel, MaskedBoundShaderState, GSimpleElementVertexDeclaration.VertexDeclarationRHI,
						*VertexShader, *MaskedPixelShader);

					MaskedPixelShader->SetEditorCompositingParameters(RHICmdList, View, DepthTexture);
					MaskedPixelShader->SetParameters(RHICmdList, Texture, Gamma, GBatchedElementAlphaRefVal / 255.0f, BlendMode);
				}
		    }
		    // render distance field elements
		    else if (
			    BlendMode == SE_BLEND_MaskedDistanceField || 
			    BlendMode == SE_BLEND_MaskedDistanceFieldShadowed ||
			    BlendMode == SE_BLEND_TranslucentDistanceField	||
			    BlendMode == SE_BLEND_TranslucentDistanceFieldShadowed)
		    {
			    float AlphaRefVal = GBatchedElementAlphaRefVal;
			    if (BlendMode == SE_BLEND_TranslucentDistanceField ||
				    BlendMode == SE_BLEND_TranslucentDistanceFieldShadowed)
			    {
				    // enable alpha blending and disable clip ref value for translucent rendering
					RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI());
				    AlphaRefVal = 0.0f;
			    }
			    else
			    {
				    // clip is done in shader so just render opaque
					RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
			    }
			    
				TShaderMapRef<FSimpleElementDistanceFieldGammaPS> DistanceFieldPixelShader(GetGlobalShaderMap(FeatureLevel));
				SetGlobalBoundShaderState(RHICmdList, FeatureLevel, DistanceFieldBoundShaderState, GSimpleElementVertexDeclaration.VertexDeclarationRHI,
					*VertexShader, *DistanceFieldPixelShader );			

			    // @todo - expose these as options for batch rendering
			    static FVector2D ShadowDirection(-1.0f/Texture->GetSizeX(),-1.0f/Texture->GetSizeY());
			    static FLinearColor ShadowColor(FLinearColor::Black);
			    const float ShadowSmoothWidth = (GBatchedElementSmoothWidth * 2) / Texture->GetSizeX();
			    
			    const bool EnableShadow = (
				    BlendMode == SE_BLEND_MaskedDistanceFieldShadowed || 
				    BlendMode == SE_BLEND_TranslucentDistanceFieldShadowed
				    );
			    
			    DistanceFieldPixelShader->SetParameters(
					RHICmdList, 
				    Texture,
				    Gamma,
				    AlphaRefVal / 255.0f,
				    GBatchedElementSmoothWidth,
				    EnableShadow,
				    ShadowDirection,
				    ShadowColor,
				    ShadowSmoothWidth,
				    (GlowInfo != NULL) ? *GlowInfo : FDepthFieldGlowInfo(),
				    BlendMode
				    );
		    }
			else if(BlendMode == SE_BLEND_TranslucentAlphaOnly)
			{
				SetBlendState(RHICmdList, BlendMode);

				if (FMath::Abs(Gamma - 1.0f) < KINDA_SMALL_NUMBER)
			    {
					TShaderMapRef<FSimpleElementAlphaOnlyPS> AlphaOnlyPixelShader(GetGlobalShaderMap(FeatureLevel));
					SetGlobalBoundShaderState(RHICmdList, FeatureLevel, AlphaOnlyShaderState, GSimpleElementVertexDeclaration.VertexDeclarationRHI,
						*VertexShader, *AlphaOnlyPixelShader);

					AlphaOnlyPixelShader->SetParameters(RHICmdList, Texture);
					AlphaOnlyPixelShader->SetEditorCompositingParameters(RHICmdList, View, DepthTexture);
			    }
			    else
			    {
					TShaderMapRef<FSimpleElementGammaAlphaOnlyPS> GammaAlphaOnlyPixelShader(GetGlobalShaderMap(FeatureLevel));
					SetGlobalBoundShaderState(RHICmdList, FeatureLevel, GammaAlphaOnlyShaderState, GSimpleElementVertexDeclaration.VertexDeclarationRHI,
						*VertexShader, *GammaAlphaOnlyPixelShader);

					GammaAlphaOnlyPixelShader->SetParameters(RHICmdList, Texture, Gamma, BlendMode);
					GammaAlphaOnlyPixelShader->SetEditorCompositingParameters(RHICmdList, View, DepthTexture);
			    }
			}
			else if(BlendMode >= SE_BLEND_RGBA_MASK_START && BlendMode <= SE_BLEND_RGBA_MASK_END)
			{
				TShaderMapRef<FSimpleElementColorChannelMaskPS> ColorChannelMaskPixelShader(GetGlobalShaderMap(FeatureLevel));
				SetGlobalBoundShaderState(RHICmdList, FeatureLevel, ColorChannelMaskShaderState, GSimpleElementVertexDeclaration.VertexDeclarationRHI,
					*VertexShader, *ColorChannelMaskPixelShader );
			
				ColorChannelMaskPixelShader->SetParameters(RHICmdList, Texture, ColorWeights, GammaToUse );
			}
			else
		    {
				SetBlendState(RHICmdList, BlendMode);
    
			    if (FMath::Abs(Gamma - 1.0f) < KINDA_SMALL_NUMBER)
			    {
					TShaderMapRef<FSimpleElementPS> PixelShader(GetGlobalShaderMap(FeatureLevel));
					SetGlobalBoundShaderState(RHICmdList, FeatureLevel, SimpleBoundShaderState, GSimpleElementVertexDeclaration.VertexDeclarationRHI,
						*VertexShader, *PixelShader);

					PixelShader->SetParameters(RHICmdList, Texture);
					PixelShader->SetEditorCompositingParameters(RHICmdList, View, DepthTexture);
			    }
			    else
			    {
					FSimpleElementGammaBasePS* BasePixelShader;
					FGlobalBoundShaderState* BoundShaderState;

					TShaderMapRef<FSimpleElementGammaPS_SRGB> PixelShader_SRGB(GetGlobalShaderMap(FeatureLevel));
					TShaderMapRef<FSimpleElementGammaPS_Linear> PixelShader_Linear(GetGlobalShaderMap(FeatureLevel));
					if (Texture->bSRGB)
					{
						BasePixelShader = *PixelShader_SRGB;
						BoundShaderState = &RegularSRGBBoundShaderState;
					}
					else
					{
						BasePixelShader = *PixelShader_Linear;
						BoundShaderState = &RegularLinearBoundShaderState;
					}

					SetGlobalBoundShaderState(RHICmdList, FeatureLevel, *BoundShaderState, GSimpleElementVertexDeclaration.VertexDeclarationRHI,
						*VertexShader, BasePixelShader);

					BasePixelShader->SetParameters(RHICmdList, Texture, Gamma, BlendMode);
					BasePixelShader->SetEditorCompositingParameters(RHICmdList, View, DepthTexture);
			    }
		    }
	    }

		// Set the simple element vertex shader parameters
		VertexShader->SetParameters(RHICmdList, Transform, bSwitchVerticalAxis);
	}
}


//@todo.VC10: Apparent VC10 compiler bug here causes an access violation when drawing Point arrays (TTP 213844), this occurred in the next two methods
#if _MSC_VER
PRAGMA_DISABLE_OPTIMIZATION
#endif

void FBatchedElements::DrawPointElements(FRHICommandList& RHICmdList, const FMatrix& Transform, const uint32 ViewportSizeX, const uint32 ViewportSizeY, const FVector& CameraX, const FVector& CameraY) const
{
	// Draw the point elements.
	if( Points.Num() > 0 )
	{
		// preallocate some memory to directly fill out
		void* VerticesPtr;

		const int32 NumPoints = Points.Num();
		const int32 NumTris = NumPoints * 2;
		const int32 NumVertices = NumTris * 3;
		RHICmdList.BeginDrawPrimitiveUP(PT_TriangleList, NumTris, NumVertices, sizeof(FSimpleElementVertex), VerticesPtr);
		FSimpleElementVertex* PointVertices = (FSimpleElementVertex*)VerticesPtr;

		int32 VertIdx = 0;
		for(int32 PointIndex = 0;PointIndex < NumPoints;PointIndex++)
		{
			// TODO: Support quad primitives here
			const FBatchedPoint& Point = Points[PointIndex];
			FVector4 TransformedPosition = Transform.TransformFVector4(Point.Position);

			// Generate vertices for the point such that the post-transform point size is constant.
			const uint32 ViewportMajorAxis = ViewportSizeX;//FMath::Max(ViewportSizeX, ViewportSizeY);
			const FVector WorldPointX = CameraX * Point.Size / ViewportMajorAxis * TransformedPosition.W;
			const FVector WorldPointY = CameraY * -Point.Size / ViewportMajorAxis * TransformedPosition.W;
					
			PointVertices[VertIdx + 0] = FSimpleElementVertex(FVector4(Point.Position + WorldPointX - WorldPointY,1),FVector2D(1,0),Point.Color,Point.HitProxyId);
			PointVertices[VertIdx + 1] = FSimpleElementVertex(FVector4(Point.Position + WorldPointX + WorldPointY,1),FVector2D(1,1),Point.Color,Point.HitProxyId);
			PointVertices[VertIdx + 2] = FSimpleElementVertex(FVector4(Point.Position - WorldPointX - WorldPointY,1),FVector2D(0,0),Point.Color,Point.HitProxyId);
			PointVertices[VertIdx + 3] = FSimpleElementVertex(FVector4(Point.Position + WorldPointX + WorldPointY,1),FVector2D(1,1),Point.Color,Point.HitProxyId);
			PointVertices[VertIdx + 4] = FSimpleElementVertex(FVector4(Point.Position - WorldPointX - WorldPointY,1),FVector2D(0,0),Point.Color,Point.HitProxyId);
			PointVertices[VertIdx + 5] = FSimpleElementVertex(FVector4(Point.Position - WorldPointX + WorldPointY,1),FVector2D(0,1),Point.Color,Point.HitProxyId);

			VertIdx += 6;
		}

		// Draw the sprite.
		RHICmdList.EndDrawPrimitiveUP();
	}
}


bool FBatchedElements::Draw(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, bool bNeedToSwitchVerticalAxis, const FMatrix& Transform, uint32 ViewportSizeX, uint32 ViewportSizeY, bool bHitTesting, float Gamma, const FSceneView* View, FTexture2DRHIRef DepthTexture, EBlendModeFilter::Type Filter) const
{
	if( HasPrimsToDraw() )
	{
		FMatrix InvTransform = Transform.Inverse();
		FVector CameraX = InvTransform.TransformVector(FVector(1,0,0)).GetSafeNormal();
		FVector CameraY = InvTransform.TransformVector(FVector(0,1,0)).GetSafeNormal();
		FVector CameraZ = InvTransform.TransformVector(FVector(0,0,1)).GetSafeNormal();

		RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());

		if( (LineVertices.Num() > 0 || Points.Num() > 0 || ThickLines.Num() > 0 || WireTris.Num() > 0)
			&& (Filter & EBlendModeFilter::OpaqueAndMasked))
		{
			// Lines/points don't support batched element parameters (yet!)
			FBatchedElementParameters* BatchedElementParameters = NULL;

			// Set the appropriate pixel shader parameters & shader state for the non-textured elements.
			PrepareShaders(RHICmdList, FeatureLevel, SE_BLEND_Opaque, Transform, bNeedToSwitchVerticalAxis, BatchedElementParameters, GWhiteTexture, bHitTesting, Gamma, NULL, View, DepthTexture);

			// Draw the line elements.
			if( LineVertices.Num() > 0 )
			{
				int32 MaxVerticesAllowed = ((GDrawUPVertexCheckCount / sizeof(FSimpleElementVertex)) / 2) * 2;
				/*
				hack to avoid a crash when trying to render large numbers of line segments.
				*/
				MaxVerticesAllowed = FMath::Min(MaxVerticesAllowed, 64 * 1024);

				int32 MinVertex=0;
				int32 TotalVerts = (LineVertices.Num() / 2) * 2;
				while( MinVertex < TotalVerts )
				{
					int32 NumLinePrims = FMath::Min( MaxVerticesAllowed, TotalVerts - MinVertex ) / 2;
					DrawPrimitiveUP(RHICmdList, PT_LineList, NumLinePrims, &LineVertices[MinVertex], sizeof(FSimpleElementVertex));
					MinVertex += NumLinePrims * 2;
				}
			}

			// Draw points
			DrawPointElements(RHICmdList, Transform, ViewportSizeX, ViewportSizeY, CameraX, CameraY);

			if ( ThickLines.Num() > 0 )
			{
				float OrthoZoomFactor = 1.0f;

				if( View )
				{
					const bool bIsPerspective = View->ViewMatrices.ProjMatrix.M[3][3] < 1.0f ? true : false;
					if( !bIsPerspective )
					{
						OrthoZoomFactor = 1.0f / View->ViewMatrices.ProjMatrix.M[0][0];
					}
				}

				int32 LineIndex = 0;
				const int32 MaxLinesPerBatch = 2048;
				while (LineIndex < ThickLines.Num())
				{
					int32 FirstLineThisBatch = LineIndex;
					float DepthBiasThisBatch = ThickLines[LineIndex].DepthBias;
					while (++LineIndex < ThickLines.Num())
					{
						if ((ThickLines[LineIndex].DepthBias != DepthBiasThisBatch)
							|| ((LineIndex - FirstLineThisBatch) >= MaxLinesPerBatch))
						{
							break;
						}
					}
					int32 NumLinesThisBatch = LineIndex - FirstLineThisBatch;

					const bool bEnableMSAA = true;
					const bool bEnableLineAA = false;
					FRasterizerStateInitializerRHI Initializer = { FM_Solid, CM_None, 0, DepthBiasThisBatch, bEnableMSAA, bEnableLineAA };
					RHICmdList.SetRasterizerState(RHICreateRasterizerState(Initializer).GetReference());

					void* ThickVertexData = NULL;
					RHICmdList.BeginDrawPrimitiveUP(PT_TriangleList, 8 * NumLinesThisBatch, 8 * 3 * NumLinesThisBatch, sizeof(FSimpleElementVertex), ThickVertexData);
					FSimpleElementVertex* ThickVertices = (FSimpleElementVertex*)ThickVertexData;
					check(ThickVertices);

					for (int i = 0; i < NumLinesThisBatch; ++i)
					{
						const FBatchedThickLines& Line = ThickLines[FirstLineThisBatch + i];
						const float Thickness = FMath::Abs( Line.Thickness );

						const float StartW			= Transform.TransformFVector4(Line.Start).W;
						const float EndW			= Transform.TransformFVector4(Line.End).W;

						// Negative thickness means that thickness is calculated in screen space, positive thickness should be used for world space thickness.
						const float ScalingStart	= Line.bScreenSpace ? StartW / ViewportSizeX : 1.0f;
						const float ScalingEnd		= Line.bScreenSpace ? EndW   / ViewportSizeX : 1.0f;

						OrthoZoomFactor = Line.bScreenSpace ? OrthoZoomFactor : 1.0f;

						const float ScreenSpaceScaling = Line.bScreenSpace ? 2.0f : 1.0f;

						const float StartThickness	= Thickness * ScreenSpaceScaling * OrthoZoomFactor * ScalingStart;
						const float EndThickness	= Thickness * ScreenSpaceScaling * OrthoZoomFactor * ScalingEnd;

						const FVector WorldPointXS	= CameraX * StartThickness * 0.5f;
						const FVector WorldPointYS	= CameraY * StartThickness * 0.5f;

						const FVector WorldPointXE	= CameraX * EndThickness * 0.5f;
						const FVector WorldPointYE	= CameraY * EndThickness * 0.5f;

						// Generate vertices for the point such that the post-transform point size is constant.
						const FVector WorldPointX = CameraX * Thickness * StartW / ViewportSizeX;
						const FVector WorldPointY = CameraY * Thickness * StartW / ViewportSizeX;

						// Begin point
						ThickVertices[0] = FSimpleElementVertex(FVector4(Line.Start + WorldPointXS - WorldPointYS,1),FVector2D(1,0),Line.Color,Line.HitProxyId); // 0S
						ThickVertices[1] = FSimpleElementVertex(FVector4(Line.Start + WorldPointXS + WorldPointYS,1),FVector2D(1,1),Line.Color,Line.HitProxyId); // 1S
						ThickVertices[2] = FSimpleElementVertex(FVector4(Line.Start - WorldPointXS - WorldPointYS,1),FVector2D(0,0),Line.Color,Line.HitProxyId); // 2S
					
						ThickVertices[3] = FSimpleElementVertex(FVector4(Line.Start + WorldPointXS + WorldPointYS,1),FVector2D(1,1),Line.Color,Line.HitProxyId); // 1S
						ThickVertices[4] = FSimpleElementVertex(FVector4(Line.Start - WorldPointXS - WorldPointYS,1),FVector2D(0,0),Line.Color,Line.HitProxyId); // 2S
						ThickVertices[5] = FSimpleElementVertex(FVector4(Line.Start - WorldPointXS + WorldPointYS,1),FVector2D(0,1),Line.Color,Line.HitProxyId); // 3S

						// Ending point
						ThickVertices[0+ 6] = FSimpleElementVertex(FVector4(Line.End + WorldPointXE - WorldPointYE,1),FVector2D(1,0),Line.Color,Line.HitProxyId); // 0E
						ThickVertices[1+ 6] = FSimpleElementVertex(FVector4(Line.End + WorldPointXE + WorldPointYE,1),FVector2D(1,1),Line.Color,Line.HitProxyId); // 1E
						ThickVertices[2+ 6] = FSimpleElementVertex(FVector4(Line.End - WorldPointXE - WorldPointYE,1),FVector2D(0,0),Line.Color,Line.HitProxyId); // 2E
																																							  
						ThickVertices[3+ 6] = FSimpleElementVertex(FVector4(Line.End + WorldPointXE + WorldPointYE,1),FVector2D(1,1),Line.Color,Line.HitProxyId); // 1E
						ThickVertices[4+ 6] = FSimpleElementVertex(FVector4(Line.End - WorldPointXE - WorldPointYE,1),FVector2D(0,0),Line.Color,Line.HitProxyId); // 2E
						ThickVertices[5+ 6] = FSimpleElementVertex(FVector4(Line.End - WorldPointXE + WorldPointYE,1),FVector2D(0,1),Line.Color,Line.HitProxyId); // 3E

						// First part of line
						ThickVertices[0+12] = FSimpleElementVertex(FVector4(Line.Start - WorldPointXS - WorldPointYS,1),FVector2D(0,0),Line.Color,Line.HitProxyId); // 2S
						ThickVertices[1+12] = FSimpleElementVertex(FVector4(Line.Start + WorldPointXS + WorldPointYS,1),FVector2D(1,1),Line.Color,Line.HitProxyId); // 1S
						ThickVertices[2+12] = FSimpleElementVertex(FVector4(Line.End   - WorldPointXE - WorldPointYE,1),FVector2D(0,0),Line.Color,Line.HitProxyId); // 2E

						ThickVertices[3+12] = FSimpleElementVertex(FVector4(Line.Start + WorldPointXS + WorldPointYS,1),FVector2D(1,1),Line.Color,Line.HitProxyId); // 1S
						ThickVertices[4+12] = FSimpleElementVertex(FVector4(Line.End   + WorldPointXE + WorldPointYE,1),FVector2D(1,1),Line.Color,Line.HitProxyId); // 1E
						ThickVertices[5+12] = FSimpleElementVertex(FVector4(Line.End   - WorldPointXE - WorldPointYE,1),FVector2D(0,0),Line.Color,Line.HitProxyId); // 2E

						// Second part of line
						ThickVertices[0+18] = FSimpleElementVertex(FVector4(Line.Start - WorldPointXS + WorldPointYS,1),FVector2D(0,1),Line.Color,Line.HitProxyId); // 3S
						ThickVertices[1+18] = FSimpleElementVertex(FVector4(Line.Start + WorldPointXS - WorldPointYS,1),FVector2D(1,0),Line.Color,Line.HitProxyId); // 0S
						ThickVertices[2+18] = FSimpleElementVertex(FVector4(Line.End   - WorldPointXE + WorldPointYE,1),FVector2D(0,1),Line.Color,Line.HitProxyId); // 3E

						ThickVertices[3+18] = FSimpleElementVertex(FVector4(Line.Start + WorldPointXS - WorldPointYS,1),FVector2D(1,0),Line.Color,Line.HitProxyId); // 0S
						ThickVertices[4+18] = FSimpleElementVertex(FVector4(Line.End   + WorldPointXE - WorldPointYE,1),FVector2D(1,0),Line.Color,Line.HitProxyId); // 0E
						ThickVertices[5+18] = FSimpleElementVertex(FVector4(Line.End   - WorldPointXE + WorldPointYE,1),FVector2D(0,1),Line.Color,Line.HitProxyId); // 3E

						ThickVertices += 24;
					}
					RHICmdList.EndDrawPrimitiveUP();
				}

				RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
			}
			// Draw the wireframe triangles.
			if (WireTris.Num() > 0)
			{
				check(WireTriVerts.Num() == WireTris.Num() * 3);

				const bool bEnableMSAA = true;
				const bool bEnableLineAA = false;
				FRasterizerStateInitializerRHI Initializer = { FM_Wireframe, CM_None, 0, 0, bEnableMSAA, bEnableLineAA };

				int32 MaxVerticesAllowed = ((GDrawUPVertexCheckCount / sizeof(FSimpleElementVertex)) / 3) * 3;
				/*
				hack to avoid a crash when trying to render large numbers of line segments.
				*/
				MaxVerticesAllowed = FMath::Min(MaxVerticesAllowed, 64 * 1024);

				const int32 MaxTrisAllowed = MaxVerticesAllowed / 3;

				int32 MinTri=0;
				int32 TotalTris = WireTris.Num();
				while( MinTri < TotalTris )
				{
					int32 MaxTri = FMath::Min(MinTri + MaxTrisAllowed, TotalTris);
					float DepthBias = WireTris[MinTri].DepthBias;
					for (int32 i = MinTri + 1; i < MaxTri; ++i)
					{
						if (DepthBias != WireTris[i].DepthBias)
						{
							MaxTri = i;
							break;
						}
					}

					Initializer.DepthBias = DepthBias;
					RHICmdList.SetRasterizerState(RHICreateRasterizerState(Initializer).GetReference());

					int32 NumTris = MaxTri - MinTri;
					DrawPrimitiveUP(RHICmdList, PT_TriangleList, NumTris, &WireTriVerts[MinTri * 3], sizeof(FSimpleElementVertex));
					MinTri = MaxTri;
				}

				RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
			}
		}

		// Draw the sprites.
		if( Sprites.Num() > 0 )
		{
			// Sprites don't support batched element parameters (yet!)
			FBatchedElementParameters* BatchedElementParameters = NULL;

			int32 SpriteCount = Sprites.Num();
			//Sort sprites by texture
			/** Compare two texture pointers, return false if equal */
			struct FCompareTexture
			{
				FORCEINLINE bool operator()( const FBatchedElements::FBatchedSprite& SpriteA, const FBatchedElements::FBatchedSprite& SpriteB ) const
				{
					return (SpriteA.Texture == SpriteB.Texture && SpriteA.BlendMode == SpriteB.BlendMode) ? false : true; 
				}
			};

			Sprites.Sort(FCompareTexture());
			
			//First time init
			const FTexture* CurrentTexture = Sprites[0].Texture;
			ESimpleElementBlendMode CurrentBlendMode = (ESimpleElementBlendMode)Sprites[0].BlendMode;

			TArray<FSimpleElementVertex> SpriteList;
			for(int32 SpriteIndex = 0;SpriteIndex < SpriteCount;SpriteIndex++)
			{
				const FBatchedSprite& Sprite = Sprites[SpriteIndex];
				const EBlendModeFilter::Type SpriteFilter = GetBlendModeFilter((ESimpleElementBlendMode)Sprite.BlendMode);

				// Only render blend modes in the filter
				if (Filter & SpriteFilter)
				{
					if (CurrentTexture != Sprite.Texture || CurrentBlendMode != Sprite.BlendMode)
					{
						//New batch, draw previous and clear
						const int32 VertexCount = SpriteList.Num();
						const int32 PrimCount = VertexCount / 3;
						PrepareShaders(RHICmdList, FeatureLevel, CurrentBlendMode, Transform, bNeedToSwitchVerticalAxis, BatchedElementParameters, CurrentTexture, bHitTesting, Gamma, NULL, View, DepthTexture);
						DrawPrimitiveUP(RHICmdList, PT_TriangleList, PrimCount, SpriteList.GetData(), sizeof(FSimpleElementVertex));

						SpriteList.Empty(6);
						CurrentTexture = Sprite.Texture;
						CurrentBlendMode = (ESimpleElementBlendMode)Sprite.BlendMode;
					}

					int32 SpriteListIndex = SpriteList.AddUninitialized(6);
					FSimpleElementVertex* Vertex = SpriteList.GetData();

					// Compute the sprite vertices.
					const FVector WorldSpriteX = CameraX * Sprite.SizeX;
					const FVector WorldSpriteY = CameraY * -Sprite.SizeY * GProjectionSignY;

					const float UStart = Sprite.U/Sprite.Texture->GetSizeX();
					const float UEnd = (Sprite.U + Sprite.UL)/Sprite.Texture->GetSizeX();
					const float VStart = Sprite.V/Sprite.Texture->GetSizeY();
					const float VEnd = (Sprite.V + Sprite.VL)/Sprite.Texture->GetSizeY();

					Vertex[SpriteListIndex + 0] = FSimpleElementVertex(FVector4(Sprite.Position + WorldSpriteX - WorldSpriteY,1),FVector2D(UEnd,  VStart),Sprite.Color,Sprite.HitProxyId);
					Vertex[SpriteListIndex + 1] = FSimpleElementVertex(FVector4(Sprite.Position + WorldSpriteX + WorldSpriteY,1),FVector2D(UEnd,  VEnd  ),Sprite.Color,Sprite.HitProxyId);
					Vertex[SpriteListIndex + 2] = FSimpleElementVertex(FVector4(Sprite.Position - WorldSpriteX - WorldSpriteY,1),FVector2D(UStart,VStart),Sprite.Color,Sprite.HitProxyId);

					Vertex[SpriteListIndex + 3] = FSimpleElementVertex(FVector4(Sprite.Position + WorldSpriteX + WorldSpriteY,1),FVector2D(UEnd,  VEnd  ),Sprite.Color,Sprite.HitProxyId);
					Vertex[SpriteListIndex + 4] = FSimpleElementVertex(FVector4(Sprite.Position - WorldSpriteX - WorldSpriteY,1),FVector2D(UStart,VStart),Sprite.Color,Sprite.HitProxyId);
					Vertex[SpriteListIndex + 5] = FSimpleElementVertex(FVector4(Sprite.Position - WorldSpriteX + WorldSpriteY,1),FVector2D(UStart,VEnd  ),Sprite.Color,Sprite.HitProxyId);
				}
			}

			if (SpriteList.Num() > 0)
			{
				const EBlendModeFilter::Type SpriteFilter = GetBlendModeFilter(CurrentBlendMode);

				// Only render blend modes in the filter
				if (Filter & SpriteFilter)
				{
					//Draw last batch
					const int32 VertexCount = SpriteList.Num();
					const int32 PrimCount = VertexCount / 3;
					PrepareShaders(RHICmdList, FeatureLevel, CurrentBlendMode, Transform, bNeedToSwitchVerticalAxis, BatchedElementParameters, CurrentTexture, bHitTesting, Gamma, NULL, View, DepthTexture);
					DrawPrimitiveUP(RHICmdList, PT_TriangleList, PrimCount, SpriteList.GetData(), sizeof(FSimpleElementVertex));
				}
			}
		}

		if( MeshElements.Num() > 0 || QuadMeshElements.Num() > 0 )
		{
			// Draw the mesh elements.
			for(int32 MeshIndex = 0;MeshIndex < MeshElements.Num();MeshIndex++)
			{
				const FBatchedMeshElement& MeshElement = MeshElements[MeshIndex];
				const EBlendModeFilter::Type MeshFilter = GetBlendModeFilter(MeshElement.BlendMode);

				// Only render blend modes in the filter
				if (Filter & MeshFilter)
				{
					// Set the appropriate pixel shader for the mesh.
					PrepareShaders(RHICmdList, FeatureLevel, MeshElement.BlendMode, Transform, bNeedToSwitchVerticalAxis, MeshElement.BatchedElementParameters, MeshElement.Texture, bHitTesting, Gamma, &MeshElement.GlowInfo);

					// Draw the mesh.
					DrawIndexedPrimitiveUP(
						RHICmdList,
						PT_TriangleList,
						0,
						MeshElement.MaxVertex - MeshElement.MinVertex + 1,
						MeshElement.Indices.Num() / 3,
						MeshElement.Indices.GetData(),
						sizeof(uint16),
						&MeshVertices[MeshElement.MinVertex],
						sizeof(FSimpleElementVertex)
						);
				}
			}

			// Draw the quad mesh elements.
			for(int32 MeshIndex = 0;MeshIndex < QuadMeshElements.Num();MeshIndex++)
			{
				const FBatchedQuadMeshElement& MeshElement = QuadMeshElements[MeshIndex];
				const EBlendModeFilter::Type MeshFilter = GetBlendModeFilter(MeshElement.BlendMode);

				// Only render blend modes in the filter
				if (Filter & MeshFilter)
				{
					// Quads don't support batched element parameters (yet!)
					FBatchedElementParameters* BatchedElementParameters = NULL;

					// Set the appropriate pixel shader for the mesh.
					PrepareShaders(RHICmdList, FeatureLevel, MeshElement.BlendMode, Transform, bNeedToSwitchVerticalAxis, BatchedElementParameters, MeshElement.Texture, bHitTesting, Gamma);

					// Draw the mesh.
					DrawPrimitiveUP(
						RHICmdList,
						PT_QuadList,
						MeshElement.Vertices.Num() / 4,
						MeshElement.Vertices.GetData(),
						sizeof(FSimpleElementVertex)
						);
				}
			}
		}

		return true;
	}
	else
	{
		return false;
	}
}

#if _MSC_VER
PRAGMA_ENABLE_OPTIMIZATION
#endif


void FBatchedElements::Clear()
{
	LineVertices.Empty();
	Points.Empty();
	Sprites.Empty();
	MeshElements.Empty();
	QuadMeshElements.Empty();
	ThickLines.Empty();
}
