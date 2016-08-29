// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
PostProcessVisualizeComplexity.cpp: Contains definitions for complexity viewmode.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessStreamingAccuracyLegend.h"
#include "SceneUtils.h"
#include "PostProcessing.h"
#include "DebugViewModeRendering.h"

void FRCPassPostProcessStreamingAccuracyLegend::DrawDesc(FCanvas& Canvas, float PosX, float PosY, const FText& Text)
{
	Canvas.DrawShadowedText(PosX + 18, PosY, Text, GetStatsFont(), FLinearColor(0.7f, 0.7f, 0.7f), FLinearColor::Black);
}

void FRCPassPostProcessStreamingAccuracyLegend::DrawBox(FCanvas& Canvas, float PosX, float PosY, const FLinearColor& Color, const FText& Text)
{
	Canvas.DrawTile(PosX, PosY, 16, 16, 0, 0, 1, 1, FLinearColor::Black);
	Canvas.DrawTile(PosX + 1, PosY + 1, 14, 14, 0, 0, 1, 1, Color);
	Canvas.DrawShadowedText(PosX + 18, PosY, Text, GetStatsFont(), FLinearColor(0.7f, 0.7f, 0.7f), FLinearColor::Black);
}

void FRCPassPostProcessStreamingAccuracyLegend::DrawCheckerBoard(FCanvas& Canvas, float PosX, float PosY, const FLinearColor& Color0, const FLinearColor& Color1, const FText& Text)
{
	Canvas.DrawTile(PosX, PosY, 16, 16, 0, 0, 1, 1, FLinearColor::Black);
	Canvas.DrawTile(PosX + 1, PosY + 1, 14, 14, 0, 0, 1, 1, Color0);
	Canvas.DrawTile(PosX + 1, PosY + 1, 7, 7, 0, 0, 1, 1, Color1);
	Canvas.DrawTile(PosX + 8, PosY + 8, 7, 7, 0, 0, 1, 1, Color1);
	Canvas.DrawShadowedText(PosX + 18, PosY, Text, GetStatsFont(), FLinearColor(0.7f, 0.7f, 0.7f), FLinearColor::Black);
}

#define LOCTEXT_NAMESPACE "TextureStreamingBuild"

void FRCPassPostProcessStreamingAccuracyLegend::DrawCustom(FRenderingCompositePassContext& Context)
{
	if (Colors.Num() != NumStreamingAccuracyColors) return;

	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessStreamingAccuracyLegend);

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	FIntRect DestRect = View.UnscaledViewRect;

	FRenderTargetTemp TempRenderTarget(View, (const FTexture2DRHIRef&)PassOutputs[0].RequestSurface(Context).TargetableTexture);
	FCanvas Canvas(&TempRenderTarget, NULL, ViewFamily.CurrentRealTime, ViewFamily.CurrentWorldTime, ViewFamily.DeltaWorldTime, Context.GetFeatureLevel());

	if (ViewFamily.GetDebugViewShaderMode() == DVSM_MaterialTexCoordScalesAccuracy)
	{
		DrawDesc(Canvas, DestRect.Min.X + 115, DestRect.Max.Y - 75, LOCTEXT("DescMaterialTexCoordScalesAccuracy", "Shows under/over texture streaming caused by material texcoord scales."));
	}
	else if (ViewFamily.GetDebugViewShaderMode() == DVSM_MeshTexCoordSizeAccuracy)
	{
		DrawDesc(Canvas, DestRect.Min.X + 115, DestRect.Max.Y - 75, LOCTEXT("DescMeshTexCoordSizeAccuracy", "Shows under/over texture streaming caused by mesh texcoord mappings in world units."));
	}
	else if (ViewFamily.GetDebugViewShaderMode() == DVSM_PrimitiveDistanceAccuracy)
	{
		DrawDesc(Canvas, DestRect.Min.X + 115, DestRect.Max.Y - 100, LOCTEXT("DescPrimitiveDistanceAccuracy", "Shows under/over texture streaming caused by the difference between the streamer calculated"));
		DrawDesc(Canvas, DestRect.Min.X + 165, DestRect.Max.Y - 75, LOCTEXT("DescPrimitiveDistanceAccuracy2", "distance-to-mesh via bounding box versus the actual per-pixel depth value."));
	}

	DrawBox(Canvas, DestRect.Min.X + 115, DestRect.Max.Y - 50, Colors[0], LOCTEXT("2XUnder", "2X Under"));
	DrawBox(Canvas, DestRect.Min.X + 215, DestRect.Max.Y - 50, Colors[1], LOCTEXT("1XUnder", "1X Under"));
	DrawBox(Canvas, DestRect.Min.X + 315, DestRect.Max.Y - 50, Colors[2], LOCTEXT("Good", "Good"));
	DrawBox(Canvas, DestRect.Min.X + 415, DestRect.Max.Y - 50, Colors[3], LOCTEXT("1xOver", "1X Over"));
	DrawBox(Canvas, DestRect.Min.X + 515, DestRect.Max.Y - 50, Colors[4], LOCTEXT("2XOver", "2X Over"));

	const FLinearColor UndefColor(UndefinedStreamingAccuracyIntensity, UndefinedStreamingAccuracyIntensity, UndefinedStreamingAccuracyIntensity, 1.f);
	DrawBox(Canvas, DestRect.Min.X + 615, DestRect.Max.Y - 50, UndefColor, LOCTEXT("Undefined", "Undefined"));
	
	if (ViewFamily.GetDebugViewShaderMode() == DVSM_MaterialTexCoordScalesAccuracy)
	{
		DrawCheckerBoard(Canvas, DestRect.Min.X + 115, DestRect.Max.Y - 25, Colors[0], Colors[4], LOCTEXT("WorstUnderAndOver", "Worst Under / Worst Over"));
		DrawCheckerBoard(Canvas, DestRect.Min.X + 415, DestRect.Max.Y - 25, FLinearColor::Black, FLinearColor::White, LOCTEXT("MissingShader", "Missing Shader"));
	}

	Canvas.Flush_RenderThread(Context.RHICmdList);
}
	
#undef LOCTEXT_NAMESPACE
