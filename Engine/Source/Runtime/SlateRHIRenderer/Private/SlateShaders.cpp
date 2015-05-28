// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateRHIRendererPrivatePCH.h"


/** Flag to determine if we are running with a color vision deficiency shader on */
uint32 GSlateShaderColorVisionDeficiencyType = 0;


IMPLEMENT_SHADER_TYPE(, FSlateElementVS, TEXT("SlateVertexShader"),TEXT("Main"),SF_Vertex);

IMPLEMENT_SHADER_TYPE(, FSlateDebugOverdrawPS, TEXT("SlateElementPixelShader"), TEXT("DebugOverdrawMain"), SF_Pixel );

#define IMPLEMENT_SLATE_PIXELSHADER_TYPE(ShaderType, bDrawDisabledEffect, bUseTextureAlpha) \
	typedef TSlateElementPS<ESlateShader::ShaderType,bDrawDisabledEffect,bUseTextureAlpha> TSlateElementPS##ShaderType##bDrawDisabledEffect##bUseTextureAlpha; \
	IMPLEMENT_SHADER_TYPE(template<>,TSlateElementPS##ShaderType##bDrawDisabledEffect##bUseTextureAlpha,TEXT("SlateElementPixelShader"),TEXT("Main"),SF_Pixel);

/**
 * All the different permutations of shaders used by slate. Uses #defines to avoid dynamic branches                 
 */
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default,		false, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Border,		false, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default,		true, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Border,		true, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default,		false, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Border,		false, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default,		true, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Border,		true, false);

IMPLEMENT_SLATE_PIXELSHADER_TYPE(Font,			false, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(LineSegment,	false, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Font,			true, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(LineSegment,	true, true);

/** The Slate vertex declaration. */
TGlobalResource<FSlateVertexDeclaration> GSlateVertexDeclaration;


/************************************************************************/
/* FSlateVertexDeclaration                                              */
/************************************************************************/
void FSlateVertexDeclaration::InitRHI()
{
	FVertexDeclarationElementList Elements;
	uint32 Stride = sizeof(FSlateVertex);
	Elements.Add(FVertexElement(0,STRUCT_OFFSET(FSlateVertex,TexCoords),VET_Float4,0,Stride));
	Elements.Add(FVertexElement(0,STRUCT_OFFSET(FSlateVertex,Position),VET_Short2,1,Stride));
	bool bUseFloat16 =
#if SLATE_USE_FLOAT16
		true;
#else
		false;
#endif
	Elements.Add(FVertexElement(0,STRUCT_OFFSET(FSlateVertex,ClipRect),bUseFloat16 ? VET_Half2 : VET_Float2,2,Stride));
	Elements.Add(FVertexElement(0,STRUCT_OFFSET(FSlateVertex,ClipRect)+STRUCT_OFFSET(FSlateRotatedClipRectType,ExtentX),bUseFloat16 ? VET_Half4 : VET_Float4,3,Stride));
	Elements.Add(FVertexElement(0,STRUCT_OFFSET(FSlateVertex,Color),VET_Color,4,Stride));

	VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
}

void FSlateVertexDeclaration::ReleaseRHI()
{
	VertexDeclarationRHI.SafeRelease();
}

void FSlateElementPS::ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Tonemapper709"));
	OutEnvironment.SetDefine(TEXT("USE_709"), CVar ? (CVar->GetValueOnGameThread() != 0) : 1);
}

/************************************************************************/
/* FSlateDefaultVertexShader                                            */
/************************************************************************/

FSlateElementVS::FSlateElementVS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
	: FGlobalShader(Initializer)
{
	ViewProjection.Bind(Initializer.ParameterMap, TEXT("ViewProjection"));
	VertexShaderParams.Bind( Initializer.ParameterMap, TEXT("VertexShaderParams"));
	SwitchVerticalAxisMultiplier.Bind( Initializer.ParameterMap, TEXT("SwitchVerticalAxisMultiplier"));
}

void FSlateElementVS::SetViewProjection(FRHICommandList& RHICmdList, const FMatrix& InViewProjection )
{
	SetShaderValue(RHICmdList, GetVertexShader(), ViewProjection, InViewProjection );
}

void FSlateElementVS::SetShaderParameters(FRHICommandList& RHICmdList, const FVector4& ShaderParams )
{
	SetShaderValue(RHICmdList, GetVertexShader(), VertexShaderParams, ShaderParams );
}

void FSlateElementVS::SetVerticalAxisMultiplier(FRHICommandList& RHICmdList, float InMultiplier )
{
	SetShaderValue(RHICmdList, GetVertexShader(), SwitchVerticalAxisMultiplier, InMultiplier );
}

/** Serializes the shader data */
bool FSlateElementVS::Serialize( FArchive& Ar )
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );

	Ar << ViewProjection;
	Ar << VertexShaderParams;
	Ar << SwitchVerticalAxisMultiplier;

	return bShaderHasOutdatedParameters;
}



