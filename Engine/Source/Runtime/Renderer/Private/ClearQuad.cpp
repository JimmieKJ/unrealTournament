// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "RendererPrivate.h"
#include "OneColorShader.h"

FGlobalBoundShaderState GClearMRTBoundShaderState[8];

// TODO support ExcludeRect
void DrawClearQuadMRT(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil)
{
	// Set new states
	FBlendStateRHIParamRef BlendStateRHI;
		
	if (NumClearColors <= 1)
	{
		BlendStateRHI = bClearColor
			? TStaticBlendState<>::GetRHI()
			: TStaticBlendState<CW_NONE>::GetRHI();
	}
	else
	{
		BlendStateRHI = bClearColor
			? TStaticBlendState<>::GetRHI()
			: TStaticBlendStateWriteMask<CW_NONE,CW_NONE,CW_NONE,CW_NONE,CW_NONE,CW_NONE,CW_NONE,CW_NONE>::GetRHI();
	}
	
	const FDepthStencilStateRHIParamRef DepthStencilStateRHI = 
		(bClearDepth && bClearStencil)
			? TStaticDepthStencilState<
				true, CF_Always,
				true,CF_Always,SO_Replace,SO_Replace,SO_Replace,
				false,CF_Always,SO_Replace,SO_Replace,SO_Replace,
				0xff,0xff
				>::GetRHI()
			: bClearDepth
				? TStaticDepthStencilState<true, CF_Always>::GetRHI()
				: bClearStencil
					? TStaticDepthStencilState<
						false, CF_Always,
						true,CF_Always,SO_Replace,SO_Replace,SO_Replace,
						false,CF_Always,SO_Replace,SO_Replace,SO_Replace,
						0xff,0xff
						>::GetRHI()
					: TStaticDepthStencilState<false, CF_Always>::GetRHI();

	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
	RHICmdList.SetBlendState(BlendStateRHI);
	RHICmdList.SetDepthStencilState(DepthStencilStateRHI);

	auto ShaderMap = GetGlobalShaderMap(FeatureLevel);


	// Set the new shaders
	TShaderMapRef<TOneColorVS<true> > VertexShader(ShaderMap);

	FOneColorPS* PixelShader = NULL;

	// Set the shader to write to the appropriate number of render targets
	// On AMD PC hardware, outputting to a color index in the shader without a matching render target set has a significant performance hit
	if (NumClearColors <= 1)
	{
		TShaderMapRef<TOneColorPixelShaderMRT<1> > MRTPixelShader(ShaderMap);
		PixelShader = *MRTPixelShader;
	}
	else if (NumClearColors == 2)
	{
		TShaderMapRef<TOneColorPixelShaderMRT<2> > MRTPixelShader(ShaderMap);
		PixelShader = *MRTPixelShader;
	}
	else if (NumClearColors == 3)
	{
		TShaderMapRef<TOneColorPixelShaderMRT<3> > MRTPixelShader(ShaderMap);
		PixelShader = *MRTPixelShader;
	}
	else if (NumClearColors == 4)
	{
		TShaderMapRef<TOneColorPixelShaderMRT<4> > MRTPixelShader(ShaderMap);
		PixelShader = *MRTPixelShader;
	}
	else if (NumClearColors == 5)
	{
		TShaderMapRef<TOneColorPixelShaderMRT<5> > MRTPixelShader(ShaderMap);
		PixelShader = *MRTPixelShader;
	}
	else if (NumClearColors == 6)
	{
		TShaderMapRef<TOneColorPixelShaderMRT<6> > MRTPixelShader(ShaderMap);
		PixelShader = *MRTPixelShader;
	}
	else if (NumClearColors == 7)
	{
		TShaderMapRef<TOneColorPixelShaderMRT<7> > MRTPixelShader(ShaderMap);
		PixelShader = *MRTPixelShader;
	}
	else if (NumClearColors == 8)
	{
		TShaderMapRef<TOneColorPixelShaderMRT<8> > MRTPixelShader(ShaderMap);
		PixelShader = *MRTPixelShader;
	}

	SetGlobalBoundShaderState(RHICmdList, FeatureLevel, GClearMRTBoundShaderState[FMath::Max(NumClearColors - 1, 0)], GetVertexDeclarationFVector4(), *VertexShader, PixelShader);	
	PixelShader->SetColors(RHICmdList, ClearColorArray, NumClearColors);
		
	{
		// Draw a fullscreen quad
		/*if(ExcludeRect.Width() > 0 && ExcludeRect.Height() > 0)
		{
			// with a hole in it (optimization in case the hardware has non constant clear performance)
			FVector4 OuterVertices[4];
			OuterVertices[0].Set( -1.0f,  1.0f, Depth, 1.0f );
			OuterVertices[1].Set(  1.0f,  1.0f, Depth, 1.0f );
			OuterVertices[2].Set(  1.0f, -1.0f, Depth, 1.0f );
			OuterVertices[3].Set( -1.0f, -1.0f, Depth, 1.0f );

			float InvViewWidth = 1.0f / Viewport.Width;
			float InvViewHeight = 1.0f / Viewport.Height;
			FVector4 FractionRect = FVector4(ExcludeRect.Min.X * InvViewWidth, ExcludeRect.Min.Y * InvViewHeight, (ExcludeRect.Max.X - 1) * InvViewWidth, (ExcludeRect.Max.Y - 1) * InvViewHeight);

			FVector4 InnerVertices[4];
			InnerVertices[0].Set( FMath::Lerp(-1.0f,  1.0f, FractionRect.X), FMath::Lerp(1.0f, -1.0f, FractionRect.Y), Depth, 1.0f );
			InnerVertices[1].Set( FMath::Lerp(-1.0f,  1.0f, FractionRect.Z), FMath::Lerp(1.0f, -1.0f, FractionRect.Y), Depth, 1.0f );
			InnerVertices[2].Set( FMath::Lerp(-1.0f,  1.0f, FractionRect.Z), FMath::Lerp(1.0f, -1.0f, FractionRect.W), Depth, 1.0f );
			InnerVertices[3].Set( FMath::Lerp(-1.0f,  1.0f, FractionRect.X), FMath::Lerp(1.0f, -1.0f, FractionRect.W), Depth, 1.0f );
				
			FVector4 Vertices[10];
			Vertices[0] = OuterVertices[0];
			Vertices[1] = InnerVertices[0];
			Vertices[2] = OuterVertices[1];
			Vertices[3] = InnerVertices[1];
			Vertices[4] = OuterVertices[2];
			Vertices[5] = InnerVertices[2];
			Vertices[6] = OuterVertices[3];
			Vertices[7] = InnerVertices[3];
			Vertices[8] = OuterVertices[0];
			Vertices[9] = InnerVertices[0];

			DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 8, Vertices, sizeof(Vertices[0]) );
		}
		else*/
		{
			// without a hole
			FVector4 Vertices[4];
			Vertices[0].Set( -1.0f,  1.0f, Depth, 1.0f );
			Vertices[1].Set(  1.0f,  1.0f, Depth, 1.0f );
			Vertices[2].Set( -1.0f, -1.0f, Depth, 1.0f );
			Vertices[3].Set(  1.0f, -1.0f, Depth, 1.0f );
			DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 2, Vertices, sizeof(Vertices[0]));
		}
	}
}
