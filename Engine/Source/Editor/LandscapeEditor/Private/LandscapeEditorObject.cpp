// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorPrivatePCH.h"
#include "LandscapeEditorObject.h"
#include "LandscapeRender.h"
#include "ImageWrapper.h"

#include "LandscapeMaterialInstanceConstant.h"
#include "EngineUtils.h"
#include "LandscapeGizmoActor.h"

ULandscapeEditorObject::ULandscapeEditorObject(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)

	// Tool Settings:
	, ToolStrength(0.3f)
	, bUseWeightTargetValue(false)
	, WeightTargetValue(1.0f)
	, MaximumValueRadius(10000.0f)

	, FlattenMode(ELandscapeToolFlattenMode::Both)
	, bUseSlopeFlatten(false)
	, bPickValuePerApply(false)
	, bUseFlattenTarget(false)
	, FlattenTarget(0)
	, bShowFlattenTargetPreview(true)

	, RampWidth(2000)
	, RampSideFalloff(0.4f)

	, SmoothFilterKernelSize(4)
	, bDetailSmooth(false)
	, DetailScale(0.3f)

	, ErodeThresh(64)
	, ErodeSurfaceThickness(256)
	, ErodeIterationNum(28)
	, ErosionNoiseMode(ELandscapeToolErosionMode::Lower)
	, ErosionNoiseScale(60.0f)

	, RainAmount(128)
	, SedimentCapacity(0.3f)
	, HErodeIterationNum(75)
	, RainDistMode(ELandscapeToolHydroErosionMode::Both)
	, RainDistScale(60.0f)
	, bHErosionDetailSmooth(true)
	, HErosionDetailScale(0.01f)

	, NoiseMode(ELandscapeToolNoiseMode::Both)
	, NoiseScale(128.0f)

	, bUseSelectedRegion(true)
	, bUseNegativeMask(true)

	, PasteMode(ELandscapeToolNoiseMode::Both)
	, bApplyToAllTargets(true)
	, bSnapGizmo(false)
	, bSmoothGizmoBrush(true)

	, ResizeLandscape_QuadsPerSection(0)
	, ResizeLandscape_SectionsPerComponent(0)
	, ResizeLandscape_ComponentCount(0, 0)
	, ResizeLandscape_ConvertMode(ELandscapeConvertMode::Expand)

	, NewLandscape_Material(NULL)
	, NewLandscape_QuadsPerSection(63)
	, NewLandscape_SectionsPerComponent(1)
	, NewLandscape_ComponentCount(8, 8)
	, NewLandscape_Location(0, 0, 100)
	, NewLandscape_Rotation(0, 0, 0)
	, NewLandscape_Scale(100, 100, 100)
	, ImportLandscape_Width(0)
	, ImportLandscape_Height(0)

	// Brush Settings:
	, BrushRadius(2048.0f)
	, BrushFalloff(0.5f)
	, bUseClayBrush(false)

	, AlphaBrushScale(0.5f)
	, bAlphaBrushAutoRotate(true)
	, AlphaBrushRotation(0.0f)
	, AlphaBrushPanU(0.5f)
	, AlphaBrushPanV(0.5f)
	, bUseWorldSpacePatternBrush(false)
	, WorldSpacePatternBrushSettings(FLandscapePatternBrushWorldSpaceSettings{FVector2D::ZeroVector, 0.0f, false, 3200})
	, AlphaTexture(NULL)
	, AlphaTextureChannel(EColorChannel::Red)
	, AlphaTextureSizeX(1)
	, AlphaTextureSizeY(1)

	, BrushComponentSize(1)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UTexture2D> AlphaTexture;

		FConstructorStatics()
			: AlphaTexture(TEXT("/Engine/EditorLandscapeResources/DefaultAlphaTexture"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	SetAlphaTexture(ConstructorStatics.AlphaTexture.Object, AlphaTextureChannel);
}

void ULandscapeEditorObject::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	SetbUseSelectedRegion(bUseSelectedRegion);
	SetbUseNegativeMask(bUseNegativeMask);
	SetPasteMode(PasteMode);
	SetbSnapGizmo(bSnapGizmo);

	if (PropertyChangedEvent.MemberProperty == NULL ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, AlphaTexture) ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, AlphaTextureChannel))
	{
		SetAlphaTexture(AlphaTexture, AlphaTextureChannel);
	}

	if (PropertyChangedEvent.MemberProperty == NULL ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, GizmoHeightmapFilenameString))
	{
		GuessGizmoImportSize();
	}

	if (PropertyChangedEvent.MemberProperty == NULL ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_QuadsPerSection) ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_SectionsPerComponent) ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_ComponentCount))
	{
		NewLandscape_ClampSize();
	}

	if (PropertyChangedEvent.MemberProperty == NULL ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ResizeLandscape_QuadsPerSection) ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ResizeLandscape_SectionsPerComponent) ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ResizeLandscape_ConvertMode))
	{
		UpdateComponentCount();
	}

	if (PropertyChangedEvent.MemberProperty == NULL ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_Material) ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ImportLandscape_HeightmapFilename))
	{
		RefreshImportLayersList();
	}
}

/** Load UI settings from ini file */
void ULandscapeEditorObject::Load()
{
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("ToolStrength"), ToolStrength, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("WeightTargetValue"), WeightTargetValue, GEditorPerProjectIni);
	bool InbUseWeightTargetValue = bUseWeightTargetValue;
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bUseWeightTargetValue"), InbUseWeightTargetValue, GEditorPerProjectIni);
	bUseWeightTargetValue = InbUseWeightTargetValue;

	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("BrushRadius"), BrushRadius, GEditorPerProjectIni);
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("BrushComponentSize"), BrushComponentSize, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("BrushFalloff"), BrushFalloff, GEditorPerProjectIni);
	bool InbUseClayBrush = bUseClayBrush;
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bUseClayBrush"), InbUseClayBrush, GEditorPerProjectIni);
	bUseClayBrush = InbUseClayBrush;
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("AlphaBrushScale"), AlphaBrushScale, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("AlphaBrushAutoRotate"), bAlphaBrushAutoRotate, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("AlphaBrushRotation"), AlphaBrushRotation, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("AlphaBrushPanU"), AlphaBrushPanU, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("AlphaBrushPanV"), AlphaBrushPanV, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bUseWorldSpacePatternBrush"), bUseWorldSpacePatternBrush, GEditorPerProjectIni);
	GConfig->GetVector2D(TEXT("LandscapeEdit"), TEXT("WorldSpacePatternBrushSettings.Origin"), WorldSpacePatternBrushSettings.Origin, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("WorldSpacePatternBrushSettings.bCenterTextureOnOrigin"), WorldSpacePatternBrushSettings.bCenterTextureOnOrigin, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("WorldSpacePatternBrushSettings.RepeatSize"), WorldSpacePatternBrushSettings.RepeatSize, GEditorPerProjectIni);
	FString AlphaTextureName = (AlphaTexture != NULL) ? AlphaTexture->GetPathName() : FString();
	int32 InAlphaTextureChannel = AlphaTextureChannel;
	GConfig->GetString(TEXT("LandscapeEdit"), TEXT("AlphaTextureName"), AlphaTextureName, GEditorPerProjectIni);
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("AlphaTextureChannel"), InAlphaTextureChannel, GEditorPerProjectIni);
	AlphaTextureChannel = (EColorChannel::Type)InAlphaTextureChannel;
	SetAlphaTexture(LoadObject<UTexture2D>(NULL, *AlphaTextureName, NULL, LOAD_NoWarn), AlphaTextureChannel);

	int32 InFlattenMode = ELandscapeToolFlattenMode::Both;
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("FlattenMode"), InFlattenMode, GEditorPerProjectIni);
	FlattenMode = (ELandscapeToolFlattenMode::Type)InFlattenMode;

	bool InbUseSlopeFlatten = bUseSlopeFlatten;
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bUseSlopeFlatten"), InbUseSlopeFlatten, GEditorPerProjectIni);
	bUseSlopeFlatten = InbUseSlopeFlatten;

	bool InbPickValuePerApply = bPickValuePerApply;
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bPickValuePerApply"), InbPickValuePerApply, GEditorPerProjectIni);
	bPickValuePerApply = InbPickValuePerApply;

	bool InbUseFlattenTarget = bUseFlattenTarget;
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bUseFlattenTarget"), InbUseFlattenTarget, GEditorPerProjectIni);
	bUseFlattenTarget = InbUseFlattenTarget;
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("FlattenTarget"), FlattenTarget, GEditorPerProjectIni);

	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("RampWidth"), RampWidth, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("RampSideFalloff"), RampSideFalloff, GEditorPerProjectIni);

	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("ErodeThresh"), ErodeThresh, GEditorPerProjectIni);
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("ErodeIterationNum"), ErodeIterationNum, GEditorPerProjectIni);
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("ErodeSurfaceThickness"), ErodeSurfaceThickness, GEditorPerProjectIni);
	int32 InErosionNoiseMode = ErosionNoiseMode;
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("ErosionNoiseMode"), InErosionNoiseMode, GEditorPerProjectIni);
	ErosionNoiseMode = (ELandscapeToolErosionMode::Type)InErosionNoiseMode;
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("ErosionNoiseScale"), ErosionNoiseScale, GEditorPerProjectIni);

	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("RainAmount"), RainAmount, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("SedimentCapacity"), SedimentCapacity, GEditorPerProjectIni);
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("HErodeIterationNum"), HErodeIterationNum, GEditorPerProjectIni);
	int32 InRainDistMode = RainDistMode;
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("RainDistNoiseMode"), InRainDistMode, GEditorPerProjectIni);
	RainDistMode = (ELandscapeToolHydroErosionMode::Type)InRainDistMode;
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("RainDistScale"), RainDistScale, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("HErosionDetailScale"), HErosionDetailScale, GEditorPerProjectIni);
	bool InbHErosionDetailSmooth = bHErosionDetailSmooth;
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bHErosionDetailSmooth"), InbHErosionDetailSmooth, GEditorPerProjectIni);
	bHErosionDetailSmooth = InbHErosionDetailSmooth;

	int32 InNoiseMode = NoiseMode;
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("NoiseMode"), InNoiseMode, GEditorPerProjectIni);
	NoiseMode = (ELandscapeToolNoiseMode::Type)InNoiseMode;
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("NoiseScale"), NoiseScale, GEditorPerProjectIni);

	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("SmoothFilterKernelSize"), SmoothFilterKernelSize, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("DetailScale"), DetailScale, GEditorPerProjectIni);
	bool InbDetailSmooth = bDetailSmooth;
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bDetailSmooth"), InbDetailSmooth, GEditorPerProjectIni);
	bDetailSmooth = InbDetailSmooth;

	GConfig->GetFloat(TEXT("LandscapeEdit"), TEXT("MaximumValueRadius"), MaximumValueRadius, GEditorPerProjectIni);

	bool InbSmoothGizmoBrush = bSmoothGizmoBrush;
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bSmoothGizmoBrush"), InbSmoothGizmoBrush, GEditorPerProjectIni);
	bSmoothGizmoBrush = InbSmoothGizmoBrush;

	int32 InPasteMode = ELandscapeToolNoiseMode::Both; 
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("PasteMode"), InPasteMode, GEditorPerProjectIni);
	//PasteMode = (ELandscapeToolPasteMode::Type)InPasteMode;
	SetPasteMode((ELandscapeToolPasteMode::Type)InPasteMode);

	int32 InConvertMode = ResizeLandscape_ConvertMode;
	GConfig->GetInt(TEXT("LandscapeEdit"), TEXT("ConvertMode"), InConvertMode, GEditorPerProjectIni);
	ResizeLandscape_ConvertMode = (ELandscapeConvertMode::Type)InConvertMode;

	// Region
	//GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bUseSelectedRegion"), bUseSelectedRegion, GEditorPerProjectIni);
	//GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bUseNegativeMask"), bUseNegativeMask, GEditorPerProjectIni);
	bool InbApplyToAllTargets = bApplyToAllTargets;
	GConfig->GetBool(TEXT("LandscapeEdit"), TEXT("bApplyToAllTargets"), InbApplyToAllTargets, GEditorPerProjectIni);
	bApplyToAllTargets = InbApplyToAllTargets;

	// Set EditRenderMode
	SetbUseSelectedRegion(bUseSelectedRegion);
	SetbUseNegativeMask(bUseNegativeMask);

	// Gizmo History (not saved!)
	GizmoHistories.Empty();
	for (TActorIterator<ALandscapeGizmoActor> It(ParentMode->GetWorld()); It; ++It)
	{
		ALandscapeGizmoActor* Gizmo = *It;
		if (!Gizmo->IsEditable())
		{
			new(GizmoHistories) FGizmoHistory(Gizmo);
		}
	}

	FString NewLandscapeMaterialName = (NewLandscape_Material != NULL) ? NewLandscape_Material->GetPathName() : FString();
	GConfig->GetString(TEXT("LandscapeEdit"), TEXT("NewLandscapeMaterialName"), NewLandscapeMaterialName, GEditorPerProjectIni);
	NewLandscape_Material = LoadObject<UMaterialInterface>(NULL, *NewLandscapeMaterialName, NULL, LOAD_NoWarn);

	RefreshImportLayersList();
}

/** Save UI settings to ini file */
void ULandscapeEditorObject::Save()
{
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("ToolStrength"), ToolStrength, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("WeightTargetValue"), WeightTargetValue, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bUseWeightTargetValue"), bUseWeightTargetValue, GEditorPerProjectIni);

	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("BrushRadius"), BrushRadius, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("BrushComponentSize"), BrushComponentSize, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("BrushFalloff"), BrushFalloff, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bUseClayBrush"), bUseClayBrush, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("AlphaBrushScale"), AlphaBrushScale, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("AlphaBrushAutoRotate"), bAlphaBrushAutoRotate, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("AlphaBrushRotation"), AlphaBrushRotation, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("AlphaBrushPanU"), AlphaBrushPanU, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("AlphaBrushPanV"), AlphaBrushPanV, GEditorPerProjectIni);
	GConfig->SetVector2D(TEXT("LandscapeEdit"), TEXT("WorldSpacePatternBrushSettings.Origin"), WorldSpacePatternBrushSettings.Origin, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("WorldSpacePatternBrushSettings.bCenterTextureOnOrigin"), WorldSpacePatternBrushSettings.bCenterTextureOnOrigin, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("WorldSpacePatternBrushSettings.RepeatSize"), WorldSpacePatternBrushSettings.RepeatSize, GEditorPerProjectIni);
	const FString AlphaTextureName = (AlphaTexture != NULL) ? AlphaTexture->GetPathName() : FString();
	GConfig->SetString(TEXT("LandscapeEdit"), TEXT("AlphaTextureName"), *AlphaTextureName, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("AlphaTextureChannel"), (int32)AlphaTextureChannel, GEditorPerProjectIni);

	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("FlattenMode"), FlattenMode, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bUseSlopeFlatten"), bUseSlopeFlatten, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bPickValuePerApply"), bPickValuePerApply, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bUseFlattenTarget"), bUseFlattenTarget, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("FlattenTarget"), FlattenTarget, GEditorPerProjectIni);

	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("RampWidth"), RampWidth, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("RampSideFalloff"), RampSideFalloff, GEditorPerProjectIni);

	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("ErodeThresh"), ErodeThresh, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("ErodeIterationNum"), ErodeIterationNum, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("ErodeSurfaceThickness"), ErodeSurfaceThickness, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("ErosionNoiseMode"), (int32)ErosionNoiseMode, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("ErosionNoiseScale"), ErosionNoiseScale, GEditorPerProjectIni);

	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("RainAmount"), RainAmount, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("SedimentCapacity"), SedimentCapacity, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("HErodeIterationNum"), ErodeIterationNum, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("RainDistMode"), (int32)RainDistMode, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("RainDistScale"), RainDistScale, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("HErosionDetailScale"), HErosionDetailScale, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bHErosionDetailSmooth"), bHErosionDetailSmooth, GEditorPerProjectIni);

	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("NoiseMode"), (int32)NoiseMode, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("NoiseScale"), NoiseScale, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("SmoothFilterKernelSize"), SmoothFilterKernelSize, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("DetailScale"), DetailScale, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bDetailSmooth"), bDetailSmooth, GEditorPerProjectIni);

	GConfig->SetFloat(TEXT("LandscapeEdit"), TEXT("MaximumValueRadius"), MaximumValueRadius, GEditorPerProjectIni);

	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bSmoothGizmoBrush"), bSmoothGizmoBrush, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("PasteMode"), (int32)PasteMode, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("LandscapeEdit"), TEXT("ConvertMode"), (int32)ResizeLandscape_ConvertMode, GEditorPerProjectIni);
	//GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bUseSelectedRegion"), bUseSelectedRegion, GEditorPerProjectIni);
	//GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bUseNegativeMask"), bUseNegativeMask, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("LandscapeEdit"), TEXT("bApplyToAllTargets"), bApplyToAllTargets, GEditorPerProjectIni);

	const FString NewLandscapeMaterialName = (NewLandscape_Material != NULL) ? NewLandscape_Material->GetPathName() : FString();
	GConfig->SetString(TEXT("LandscapeEdit"), TEXT("NewLandscapeMaterialName"), *NewLandscapeMaterialName, GEditorPerProjectIni);
}

// Region
void ULandscapeEditorObject::SetbUseSelectedRegion(bool InbUseSelectedRegion)
{ 
	bUseSelectedRegion = InbUseSelectedRegion;
	if (bUseSelectedRegion)
	{
		GLandscapeEditRenderMode |= ELandscapeEditRenderMode::Mask;
	}
	else
	{
		GLandscapeEditRenderMode &= ~(ELandscapeEditRenderMode::Mask);
	}
}
void ULandscapeEditorObject::SetbUseNegativeMask(bool InbUseNegativeMask) 
{ 
	bUseNegativeMask = InbUseNegativeMask; 
	if (bUseNegativeMask)
	{
		GLandscapeEditRenderMode |= ELandscapeEditRenderMode::InvertedMask;
	}
	else
	{
		GLandscapeEditRenderMode &= ~(ELandscapeEditRenderMode::InvertedMask);
	}
}

void ULandscapeEditorObject::SetPasteMode(ELandscapeToolPasteMode::Type InPasteMode)
{
	GLandscapePreviewMeshRenderMode = PasteMode = InPasteMode;
}

void ULandscapeEditorObject::SetbSnapGizmo(bool InbSnapGizmo)
{
	bSnapGizmo = InbSnapGizmo;

	FEdModeLandscape* EdMode = (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
	if (EdMode && EdMode->CurrentGizmoActor.IsValid())
	{
		EdMode->CurrentGizmoActor->bSnapToLandscapeGrid = bSnapGizmo;
	}

	if (bSnapGizmo)
	{
		if (EdMode && EdMode->CurrentGizmoActor.IsValid())
		{
			check(EdMode->CurrentGizmoActor->TargetLandscapeInfo);

			const FVector WidgetLocation = EdMode->CurrentGizmoActor->GetActorLocation();
			const FRotator WidgetRotation = EdMode->CurrentGizmoActor->GetActorRotation();

			const FVector SnappedWidgetLocation = EdMode->CurrentGizmoActor->SnapToLandscapeGrid(WidgetLocation);
			const FRotator SnappedWidgetRotation = EdMode->CurrentGizmoActor->SnapToLandscapeGrid(WidgetRotation);

			EdMode->CurrentGizmoActor->SetActorLocation(SnappedWidgetLocation, false);
			EdMode->CurrentGizmoActor->SetActorRotation(SnappedWidgetRotation);
		}
	}
}

void ULandscapeEditorObject::GuessGizmoImportSize()
{
	int64 GizmoFileSize = IFileManager::Get().FileSize(*GizmoHeightmapFilenameString);

	if (GizmoFileSize != INDEX_NONE && (GizmoFileSize % 2 == 0))
	{
		GizmoFileSize /= 2;

		if (GizmoImportSize.X * GizmoImportSize.Y != GizmoFileSize)
		{
			GizmoImportSize = FIntPoint(0, 0);
			//GizmoImportButton->IsEnabled = false;

			// Guess dimensions from filesize
			// Keep searching for the most squarelike size
			for (int32 w = FMath::TruncToInt(FMath::Sqrt((float)GizmoFileSize)); w > 0; w--)
			{
				if (GizmoFileSize % w == 0)
				{
					int32 h = GizmoFileSize / w;
					checkSlow(w * h == GizmoFileSize);
					GizmoImportSize = FIntPoint(w, h);
					//GizmoImportButton->IsEnabled = true;

					break;
				}
			}
		}
	}
	else
	{
		GizmoImportSize = FIntPoint(0, 0);
		//GizmoImportButton->IsEnabled = false;
	}
}

bool ULandscapeEditorObject::SetAlphaTexture(UTexture2D* InTexture, EColorChannel::Type InTextureChannel)
{
	bool Result = true;

	TArray<uint8> NewTextureData;
	UTexture2D* NewAlphaTexture = InTexture;

	// No texture or no source art, try to use the previous texture.
	if (NewAlphaTexture == NULL || !NewAlphaTexture->Source.IsValid())
	{
		NewAlphaTexture = AlphaTexture;
		Result = false;
	}

	if (NewAlphaTexture != NULL && NewAlphaTexture->Source.IsValid())
	{
		NewAlphaTexture->Source.GetMipData(NewTextureData, 0);
	}

	// Load fallback if there's no texture or data
	if (NewAlphaTexture == NULL || (NewTextureData.Num() != 4 * NewAlphaTexture->Source.GetSizeX() * NewAlphaTexture->Source.GetSizeY()))
	{
		NewAlphaTexture = GetClass()->GetDefaultObject<ULandscapeEditorObject>()->AlphaTexture;
		NewAlphaTexture->Source.GetMipData(NewTextureData, 0);
		Result = false;
	}

	check(NewAlphaTexture);
	AlphaTexture = NewAlphaTexture;
	AlphaTextureSizeX = NewAlphaTexture->Source.GetSizeX();
	AlphaTextureSizeY = NewAlphaTexture->Source.GetSizeY();
	AlphaTextureChannel = InTextureChannel;
	AlphaTextureData.Empty(AlphaTextureSizeX*AlphaTextureSizeY);

	if (NewTextureData.Num() != 4 *AlphaTextureSizeX*AlphaTextureSizeY)
	{
		// Don't crash if for some reason we couldn't load any source art
		AlphaTextureData.AddZeroed(AlphaTextureSizeX*AlphaTextureSizeY);
	}
	else
	{
		uint8* SrcPtr;
		switch(AlphaTextureChannel)
		{
		case 1:
			SrcPtr = &((FColor*)NewTextureData.GetData())->G;
			break;
		case 2:
			SrcPtr = &((FColor*)NewTextureData.GetData())->B;
			break;
		case 3:
			SrcPtr = &((FColor*)NewTextureData.GetData())->A;
			break;
		default:
			SrcPtr = &((FColor*)NewTextureData.GetData())->R;
			break;
		}

		for (int32 i=0;i<AlphaTextureSizeX*AlphaTextureSizeY;i++)
		{
			AlphaTextureData.Add(*SrcPtr);
			SrcPtr += 4;
		}
	}

	return Result;
}

const TArray<uint16>& ULandscapeEditorObject::GetImportLandscapeData()
{
	if (ImportLandscape_HeightmapFilename.EndsWith(".png"))
	{
		if (ImportLandscape_Data.Num() == 0)
		{
			ImportLandscape_Data.Empty();

			TArray<uint8> ImportData;
			if (!FFileHelper::LoadFileToArray(ImportData, *ImportLandscape_HeightmapFilename, FILEREAD_Silent))
			{
				ImportLandscape_HeightmapError = ELandscapeImportHeightmapError::FileNotFound;
			}
			else
			{
				IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
				IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

				const TArray<uint8>* RawData = NULL;
				if (ImageWrapper->SetCompressed(ImportData.GetData(), ImportData.Num()) &&
					ImageWrapper->GetWidth() == ImportLandscape_Width && ImageWrapper->GetHeight() == ImportLandscape_Height)
				{
					if (ImageWrapper->GetBitDepth() <= 8)
					{
						ImageWrapper->GetRaw(ERGBFormat::Gray, 8, RawData);

						if (ensure(RawData->Num() == ImportLandscape_Width * ImportLandscape_Height))
						{
							ImportLandscape_Data.Empty(ImportLandscape_Width * ImportLandscape_Height);
							ImportLandscape_Data.AddUninitialized(ImportLandscape_Width * ImportLandscape_Height);

							for (int32 i = 0; i < RawData->Num(); i++)
							{
								ImportLandscape_Data[i] = (*RawData)[i] * 0x101; // Expand to 16-bit
							}
						}
					}
					else
					{
						ImageWrapper->GetRaw(ERGBFormat::Gray, 16, RawData);

						if (ensure(RawData->Num() == ImportLandscape_Width * ImportLandscape_Height * 2))
						{
							ImportLandscape_Data.Empty(ImportLandscape_Width * ImportLandscape_Height);
							ImportLandscape_Data.AddUninitialized(ImportLandscape_Width * ImportLandscape_Height);
							FMemory::Memcpy(ImportLandscape_Data.GetData(), RawData->GetData(), RawData->Num());
						}
					}
				}
			}
		}
	}
	else
	{
		if (ImportLandscape_Width * ImportLandscape_Height != ImportLandscape_Data.Num())
		{
			ImportLandscape_Data.Empty();

			TArray<uint8> ImportData;
			if (ImportLandscape_Width > 0 && ImportLandscape_Height > 0 &&
				FFileHelper::LoadFileToArray(ImportData, *ImportLandscape_HeightmapFilename, FILEREAD_Silent) &&
				ImportData.Num() == ImportLandscape_Width * ImportLandscape_Height * 2)
			{
				ImportLandscape_Data.Empty(ImportLandscape_Width * ImportLandscape_Height);
				ImportLandscape_Data.AddUninitialized(ImportLandscape_Width * ImportLandscape_Height);
				FMemory::Memcpy(ImportLandscape_Data.GetData(), ImportData.GetData(), ImportData.Num());
			}
		}
	}

	return ImportLandscape_Data;
}

void ULandscapeEditorObject::RefreshImportLayersList()
{
	UTexture2D* ThumbnailWeightmap = LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorLandscapeResources/LandscapeThumbnailWeightmap.LandscapeThumbnailWeightmap"), NULL, LOAD_None, NULL);
	UTexture2D* ThumbnailHeightmap = LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorLandscapeResources/LandscapeThumbnailHeightmap.LandscapeThumbnailHeightmap"), NULL, LOAD_None, NULL);

	UMaterialInterface* Material = NewLandscape_Material.Get();
	TArray<FName> LayerNames = ALandscapeProxy::GetLayersFromMaterial(Material);

	const TArray<FLandscapeImportLayer> OldLayersList = MoveTemp(ImportLandscape_Layers);
	ImportLandscape_Layers.Reset(LayerNames.Num());

	for (int32 i = 0; i < LayerNames.Num(); i++)
	{
		const FName& LayerName = LayerNames[i];

		bool bFound = false;
		FLandscapeImportLayer NewImportLayer;
		for (int32 j = 0; j < OldLayersList.Num(); j++)
		{
			if (OldLayersList[j].LayerName == LayerName)
			{
				NewImportLayer = OldLayersList[j];
				bFound = true;
				break;
			}
		}

		if (bFound)
		{
			UMaterialInstanceConstant* CombinationMaterialInstance = CastChecked<UMaterialInstanceConstant>(NewImportLayer.ThumbnailMIC->Parent);
			if (CombinationMaterialInstance->Parent != Material)
			{
				CombinationMaterialInstance->SetParentEditorOnly(Material);
				//NewImportLayer.ThumbnailMIC = ALandscapeProxy::GetLayerThumbnailMIC(Material, LayerName, ThumbnailWeightmap, ThumbnailHeightmap, NULL);
			}

			NewImportLayer.ImportError = ELandscapeImportLayerError::None;
			if (!NewImportLayer.SourceFilePath.IsEmpty())
			{
				if (NewImportLayer.LayerInfo == NULL)
				{
					NewImportLayer.ImportError = ELandscapeImportLayerError::MissingLayerInfo;
				}
				else
				{
					if (NewImportLayer.SourceFilePath.EndsWith(".png"))
					{
						TArray<uint8> ImportData;
						if (!FFileHelper::LoadFileToArray(ImportData, *NewImportLayer.SourceFilePath, FILEREAD_Silent))
						{
							NewImportLayer.ImportError = ELandscapeImportLayerError::FileNotFound;
						}
						else
						{
							IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
							IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

							if (!ImageWrapper->SetCompressed(ImportData.GetData(), ImportData.Num()))
							{
								NewImportLayer.ImportError = ELandscapeImportLayerError::CorruptFile;
							}
							else if (ImageWrapper->GetWidth() != ImportLandscape_Width || ImageWrapper->GetHeight() != ImportLandscape_Height)
							{
								NewImportLayer.ImportError = ELandscapeImportLayerError::FileSizeMismatch;
							}
							else if (ImageWrapper->GetFormat() != ERGBFormat::Gray)
							{
								NewImportLayer.ImportError = ELandscapeImportLayerError::ColorPng;
							}
						}
					}
					else
					{
						int64 ImportFileSize = IFileManager::Get().FileSize(*NewImportLayer.SourceFilePath);

						if (ImportFileSize < 0)
						{
							NewImportLayer.ImportError = ELandscapeImportLayerError::FileNotFound;
						}
						else if (ImportFileSize != ImportLandscape_Width * ImportLandscape_Height)
						{
							NewImportLayer.ImportError = ELandscapeImportLayerError::FileSizeMismatch;
						}
					}
				}
			}
		}
		else
		{
			NewImportLayer.LayerName = LayerName;
			NewImportLayer.ThumbnailMIC = ALandscapeProxy::GetLayerThumbnailMIC(Material, LayerName, ThumbnailWeightmap, ThumbnailHeightmap, NULL);
		}

		ImportLandscape_Layers.Add(MoveTemp(NewImportLayer));
	}
}
