// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "TileSetEditor.h"
#include "PaperEditorViewportClient.h"
#include "CanvasTypes.h"
#include "SEditorViewport.h"
#include "PreviewScene.h"

//////////////////////////////////////////////////////////////////////////
// FPaperEditorViewportClient

FPaperEditorViewportClient::FPaperEditorViewportClient(const TWeakPtr<SEditorViewport>& InEditorViewportWidget)
	: FEditorViewportClient(new FAssetEditorModeManager(), nullptr, InEditorViewportWidget)
	, CheckerboardTexture(nullptr)
{
	bOwnsModeTools = true;
	ZoomPos = FVector2D::ZeroVector;
	ZoomAmount = 1.0f;

	//ModifyCheckerboardTextureColors();
	//@TODO: ModeTools->SetToolkitHost

	//@TODO: Pretty lame hardcoding
	//@TODO: Doesn't handle negatives either (not really)
	const bool XX = FMath::IsNearlyEqual(PaperAxisX.X, 1.0f);
	const bool XY = FMath::IsNearlyEqual(PaperAxisX.Y, 1.0f);
	const bool YY = FMath::IsNearlyEqual(PaperAxisY.Y, 1.0f);
	const bool YZ = FMath::IsNearlyEqual(PaperAxisY.Z, 1.0f);

	ELevelViewportType NewViewportType = LVT_OrthoXZ;
	if (XX && YY)
	{
		NewViewportType = LVT_OrthoXY;
	}
	else if (XX && YZ)
	{
		NewViewportType = LVT_OrthoXZ;
	}
	else if (XY && YZ)
	{
		NewViewportType = LVT_OrthoYZ;
	}
	else
	{
		//@TODO: Unsupported axes
	}
	SetViewModes(VMI_Lit, VMI_Lit);
	SetViewportType(NewViewportType);

	bDeferZoomToSprite = true;
	bDeferZoomToSpriteIsInstant = true;

	// Get the correct general direction of the perspective mode; the distance doesn't matter much as we've queued up a deferred zoom that will calculate a much better distance
 	SetInitialViewTransform(LVT_Perspective, -100.0f * PaperAxisZ, PaperAxisZ.Rotation(), 0.0f);
}

FPaperEditorViewportClient::~FPaperEditorViewportClient()
{
}

FLinearColor FPaperEditorViewportClient::GetBackgroundColor() const
{
	//@TODO: Make adjustable - TextureEditorPtr.Pin()->GetBackgroundColor());
	return FLinearColor(0, 0, 127, 0);
}

void FPaperEditorViewportClient::Tick(float DeltaSeconds)
{
	// Zoom in on the sprite
	//@TODO: Fix this properly so it doesn't need to be deferred, or wait for the viewport to initialize
	FIntPoint Size = Viewport->GetSizeXY();
	if (bDeferZoomToSprite && (Size.X > 0) && (Size.Y > 0))
	{
		FBox BoundsToFocus = GetDesiredFocusBounds();
		if (ViewportType != LVT_Perspective)
		{
			TGuardValue<ELevelViewportType> SaveViewportType(ViewportType, LVT_Perspective);
			FocusViewportOnBox(BoundsToFocus, bDeferZoomToSpriteIsInstant);
		}

		FocusViewportOnBox(BoundsToFocus, bDeferZoomToSpriteIsInstant);
		bDeferZoomToSprite = false;
	}

	FEditorViewportClient::Tick(DeltaSeconds);
}

void FPaperEditorViewportClient::DrawSelectionRectangles(FViewport* Viewport, FCanvas* Canvas)
{
	for (auto RectangleIt = SelectionRectangles.CreateConstIterator(); RectangleIt; ++RectangleIt)
	{
		const FViewportSelectionRectangle& Rect = *RectangleIt;

		const float X = (Rect.TopLeft.X - ZoomPos.X) * ZoomAmount;
		const float Y = (Rect.TopLeft.Y - ZoomPos.Y) * ZoomAmount;
		const float W = Rect.Dimensions.X * ZoomAmount;
		const float H = Rect.Dimensions.Y * ZoomAmount;
		const bool bAlphaBlend = true;

		Canvas->DrawTile(X, Y, W, H, 0, 0, 1, 1, Rect.Color, GWhiteTexture, bAlphaBlend);
	}
}

void FPaperEditorViewportClient::AddReferencedObjects(FReferenceCollector& Collector)
{
	FEditorViewportClient::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(CheckerboardTexture);
}

// Called to request a focus on the current selection
void FPaperEditorViewportClient::RequestFocusOnSelection(bool bInstant)
{
	bDeferZoomToSprite = true;
	bDeferZoomToSpriteIsInstant = bInstant;
}

void FPaperEditorViewportClient::ModifyCheckerboardTextureColors()
{
	const FColor ColorOne = FColor(128, 128, 128);//TextureEditorPtr.Pin()->GetCheckeredBackground_ColorOne();
	const FColor ColorTwo = FColor(64, 64, 64);//TextureEditorPtr.Pin()->GetCheckeredBackground_ColorTwo();
	const int32 CheckerSize = 32;//TextureEditorPtr.Pin()->GetCheckeredBackground_Size();

	DestroyCheckerboardTexture();
	SetupCheckerboardTexture(ColorOne, ColorTwo, CheckerSize);
}

void FPaperEditorViewportClient::SetupCheckerboardTexture(const FColor& ColorOne, const FColor& ColorTwo, int32 CheckerSize)
{
// 	GConfig->GetInt(TEXT("TextureProperties"), TEXT("CheckerboardCheckerPixelNum"), CheckerSize, GEditorPerProjectIni);
// 	GConfig->GetColor(TEXT("TextureProperties"), TEXT("CheckerboardColorOne"), CheckerColorOne, GEditorPerProjectIni);
// 	GConfig->GetColor(TEXT("TextureProperties"), TEXT("CheckerboardColorTwo"), CheckerColorTwo, GEditorPerProjectIni);
	
// 	CheckerColorOne = FColor(128, 128, 128);
// 	CheckerColorTwo = FColor(64, 64, 64);
// 	bIsCheckeredBackgroundFill = false;
// 	CheckerSize = 32;
	CheckerSize = FMath::RoundUpToPowerOfTwo(CheckerSize);
	const int32 HalfPixelNum = CheckerSize >> 1;

	if (CheckerboardTexture == nullptr)
	{
		// Create the texture
		CheckerboardTexture = UTexture2D::CreateTransient(CheckerSize, CheckerSize, PF_B8G8R8A8);

		// Lock the checkerboard texture so it can be modified
		FColor* MipData = static_cast<FColor*>(CheckerboardTexture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));

		// Fill in the colors in a checkerboard pattern
		for (int32 RowNum = 0; RowNum < CheckerSize; ++RowNum)
		{
			for (int32 ColNum = 0; ColNum < CheckerSize; ++ColNum)
			{
				FColor& CurColor = MipData[(ColNum + (RowNum * CheckerSize))];

				if (ColNum < HalfPixelNum)
				{
					CurColor = (RowNum < HalfPixelNum) ? ColorOne: ColorTwo;
				}
				else
				{
					CurColor = (RowNum < HalfPixelNum) ? ColorTwo: ColorOne;
				}
			}
		}

		// Unlock the texture
		CheckerboardTexture->PlatformData->Mips[0].BulkData.Unlock();
		CheckerboardTexture->UpdateResource();
	}
}

void FPaperEditorViewportClient::DestroyCheckerboardTexture()
{
	if (CheckerboardTexture)
	{
		if (CheckerboardTexture->Resource)
		{
			CheckerboardTexture->ReleaseResource();
		}
		CheckerboardTexture->MarkPendingKill();
		CheckerboardTexture = nullptr;
	}
}
