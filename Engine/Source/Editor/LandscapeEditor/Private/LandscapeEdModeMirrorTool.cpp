// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorPrivatePCH.h"
#include "LandscapeEdMode.h"
#include "ScopedTransaction.h"
#include "LandscapeEdit.h"
#include "LandscapeDataAccess.h"
#include "LandscapeRender.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "Algo/Reverse.h"
//#include "LandscapeDataAccess.h"
//#include "AI/Navigation/NavigationSystem.h"

#define LOCTEXT_NAMESPACE "Landscape"

class FLandscapeToolMirror : public FLandscapeTool
{
protected:
	FEdModeLandscape* EdMode;
	UMaterialInstanceDynamic* MirrorPlaneMaterial;

	ECoordSystem SavedCoordSystem;

public:
	FLandscapeToolMirror(FEdModeLandscape* InEdMode)
		: EdMode(InEdMode)
	{
		UMaterialInterface* BaseMirrorPlaneMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EditorLandscapeResources/MirrorPlaneMaterial.MirrorPlaneMaterial"));
		MirrorPlaneMaterial = UMaterialInstanceDynamic::Create(BaseMirrorPlaneMaterial, GetTransientPackage());
		MirrorPlaneMaterial->SetScalarParameterValue(FName("LineThickness"), 2.0f);
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(MirrorPlaneMaterial);
	}

	virtual const TCHAR* GetToolName() override { return TEXT("Mirror"); }
	virtual FText GetDisplayName() override { return FText(); /*NSLOCTEXT("UnrealEd", "LandscapeTool_Mirror", "Mirror Landscape");*/ };

	virtual void SetEditRenderType() override { GLandscapeEditRenderMode = ELandscapeEditRenderMode::None | (GLandscapeEditRenderMode & ELandscapeEditRenderMode::BitMaskForMask); }
	virtual bool SupportsMask() override { return false; }

	virtual ELandscapeToolTargetTypeMask::Type GetSupportedTargetTypes() override
	{
		return ELandscapeToolTargetTypeMask::Heightmap;
	}

	virtual void EnterTool() override
	{
		if (EdMode->UISettings->MirrorPoint == FVector2D::ZeroVector)
		{
			CenterMirrorPoint();
		}
		GLevelEditorModeTools().SetWidgetMode(FWidget::WM_Translate);
		SavedCoordSystem = GLevelEditorModeTools().GetCoordSystem();
		GLevelEditorModeTools().SetCoordSystem(COORD_Local);
	}

	virtual void ExitTool() override
	{
		GLevelEditorModeTools().SetCoordSystem(SavedCoordSystem);
	}

	virtual bool BeginTool(FEditorViewportClient* ViewportClient, const FLandscapeToolTarget& Target, const FVector& InHitLocation) override
	{
		return true;
	}

	virtual void EndTool(FEditorViewportClient* ViewportClient) override
	{
	}

	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) override
	{
		return false;
	}

	virtual bool InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent) override
	{
		if (InKey == EKeys::Enter && InEvent == IE_Pressed)
		{
			ApplyMirror();
		}

		return false;
	}

	virtual bool InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale) override
	{
		if (InViewportClient->GetCurrentWidgetAxis() != EAxisList::None)
		{
			const ALandscapeProxy* LandscapeProxy = EdMode->CurrentToolTarget.LandscapeInfo->GetLandscapeProxy();
			const FTransform LandscapeToWorld = LandscapeProxy->LandscapeActorToWorld();

			EdMode->UISettings->MirrorPoint += FVector2D(LandscapeToWorld.InverseTransformVector(InDrag));

			return true;
		}

		return false;
	}

	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override
	{
		// The editor can try to render the tool before the UpdateLandscapeEditorData command runs and the landscape editor realizes that the landscape has been hidden/deleted
		const ULandscapeInfo* const LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo.Get();
		const ALandscapeProxy* const LandscapeProxy = LandscapeInfo->GetLandscapeProxy();
		if (LandscapeProxy)
		{
			const FTransform LandscapeToWorld = LandscapeProxy->LandscapeActorToWorld();

			int32 MinX, MinY, MaxX, MaxY;
			if (LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
			{
				FVector MirrorPoint3D = FVector((MaxX + MinX) / 2.0f, (MaxY + MinY) / 2.0f, 0);
				FVector MirrorPlaneScale = FVector(0, 1, 100);

				if (EdMode->UISettings->MirrorOp == ELandscapeMirrorOperation::MinusXToPlusX ||
					EdMode->UISettings->MirrorOp == ELandscapeMirrorOperation::PlusXToMinusX)
				{
					MirrorPoint3D.X = EdMode->UISettings->MirrorPoint.X;
					MirrorPlaneScale.Y = (MaxY - MinY) / 2.0f;
				}
				else
				{
					MirrorPoint3D.Y = EdMode->UISettings->MirrorPoint.Y;
					MirrorPlaneScale.Y = (MaxX - MinX) / 2.0f;
				}

				MirrorPoint3D.Z = GetLocalZAtPoint(LandscapeInfo, FMath::RoundToInt(MirrorPoint3D.X), FMath::RoundToInt(MirrorPoint3D.Y));
				MirrorPoint3D = LandscapeToWorld.TransformPosition(MirrorPoint3D);

				FMatrix Matrix;
				if (EdMode->UISettings->MirrorOp == ELandscapeMirrorOperation::MinusYToPlusY ||
					EdMode->UISettings->MirrorOp == ELandscapeMirrorOperation::PlusYToMinusY)
				{
					Matrix = FScaleRotationTranslationMatrix(MirrorPlaneScale, FRotator(0, 90, 0), FVector::ZeroVector);
				}
				else
				{
					Matrix = FScaleMatrix(MirrorPlaneScale);
				}

				Matrix *= LandscapeToWorld.ToMatrixWithScale();
				Matrix.SetOrigin(MirrorPoint3D);

				// Convert plane from horizontal to vertical
				Matrix = FMatrix(FVector(0, 1, 0), FVector(0, 0, 1), FVector(1, 0, 0), FVector(0, 0, 0)) * Matrix;

				const FBox Box = FBox(FVector(-1, -1, 0), FVector(+1, +1, 0));
				DrawWireBox(PDI, Matrix, Box, FLinearColor::Green, SDPG_World);

				const float LandscapeScaleRatio = LandscapeToWorld.GetScale3D().Z / LandscapeToWorld.GetScale3D().X;
				FVector2D UVScale = FVector2D(FMath::RoundToFloat(MirrorPlaneScale.Y / 10), FMath::RoundToFloat(MirrorPlaneScale.Z * LandscapeScaleRatio / 10 / 2) * 2);
				MirrorPlaneMaterial->SetVectorParameterValue(FName("GridSize"), FVector(UVScale, 0));
				DrawPlane10x10(PDI, Matrix, 1, FVector2D(0, 0), FVector2D(1, 1), MirrorPlaneMaterial->GetRenderProxy(false), SDPG_World);
			}
		}
	}

	virtual bool OverrideSelection() const override
	{
		return true;
	}

	virtual bool IsSelectionAllowed(AActor* InActor, bool bInSelection) const override
	{
		// Only filter selection not deselection
		if (bInSelection)
		{
			return false;
		}

		return true;
	}

	virtual bool UsesTransformWidget() const override
	{
		// The editor can try to render the transform widget before the landscape editor ticks and realises that the landscape has been hidden/deleted
		const ULandscapeInfo* const LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo.Get();
		const ALandscapeProxy* const LandscapeProxy = LandscapeInfo->GetLandscapeProxy();
		if (LandscapeProxy)
		{
			return true;
		}

		return false;
	}

	virtual EAxisList::Type GetWidgetAxisToDraw(FWidget::EWidgetMode CheckMode) const override
	{
		if (CheckMode == FWidget::WM_Translate)
		{
			switch (EdMode->UISettings->MirrorOp)
			{
			case ELandscapeMirrorOperation::MinusXToPlusX:
			case ELandscapeMirrorOperation::PlusXToMinusX:
				return EAxisList::X;
			case ELandscapeMirrorOperation::MinusYToPlusY:
			case ELandscapeMirrorOperation::PlusYToMinusY:
				return EAxisList::Y;
			default:
				check(0);
				return EAxisList::None;
			}
		}
		else
		{
			return EAxisList::None;
		}

		return EAxisList::None;
	}

	virtual FVector GetWidgetLocation() const override
	{
		const ULandscapeInfo* const LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo.Get();
		const ALandscapeProxy* const LandscapeProxy = LandscapeInfo->GetLandscapeProxy();
		if (LandscapeProxy)
		{
			const FTransform LandscapeToWorld = LandscapeProxy->LandscapeActorToWorld();

			int32 MinX, MinY, MaxX, MaxY;
			if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
			{
				MinX = MinY = 0;
				MaxX = MaxY = 0;
			}

			FVector MirrorPoint3D = FVector((MaxX + MinX) / 2.0f, (MaxY + MinY) / 2.0f, 0);
			if (EdMode->UISettings->MirrorOp == ELandscapeMirrorOperation::MinusXToPlusX ||
				EdMode->UISettings->MirrorOp == ELandscapeMirrorOperation::PlusXToMinusX)
			{
				MirrorPoint3D.X = EdMode->UISettings->MirrorPoint.X;
			}
			else
			{
				MirrorPoint3D.Y = EdMode->UISettings->MirrorPoint.Y;
			}
			MirrorPoint3D.Z = GetLocalZAtPoint(LandscapeInfo, FMath::RoundToInt(MirrorPoint3D.X), FMath::RoundToInt(MirrorPoint3D.Y));
			MirrorPoint3D = LandscapeToWorld.TransformPosition(MirrorPoint3D);
			MirrorPoint3D.Z += 1000.f; // place the widget a little off the ground for better visibility
			return MirrorPoint3D;
		}

		return FVector::ZeroVector;
	}

	virtual FMatrix GetWidgetRotation() const override
	{
		const ALandscapeProxy* const LandscapeProxy = EdMode->CurrentToolTarget.LandscapeInfo->GetLandscapeProxy();
		if (LandscapeProxy)
		{
			const FTransform LandscapeToWorld = LandscapeProxy->LandscapeActorToWorld();

			FMatrix Result = FQuatRotationTranslationMatrix(LandscapeToWorld.GetRotation(), FVector::ZeroVector);
			if (EdMode->UISettings->MirrorOp == ELandscapeMirrorOperation::PlusXToMinusX ||
				EdMode->UISettings->MirrorOp == ELandscapeMirrorOperation::PlusYToMinusY)
			{
				Result = FRotationMatrix(FRotator(0, 180, 0)) * Result;
			}
			return Result;
		}

		return FMatrix::Identity;
	}

protected:
	float GetLocalZAtPoint(const ULandscapeInfo* LandscapeInfo, int32 x, int32 y) const
	{
		// try to find Z location
		TSet<ULandscapeComponent*> Components;
		LandscapeInfo->GetComponentsInRegion(x, y, x, y, Components);
		for (ULandscapeComponent* Component : Components)
		{
			FLandscapeComponentDataInterface DataInterface(Component);
			return LandscapeDataAccess::GetLocalHeight(DataInterface.GetHeight(x - Component->SectionBaseX, y - Component->SectionBaseY));
		}
		return 0.0f;
	}

	template<typename T>
	void ApplyMirrorInternal(T* MirrorData, int32 SizeX, int32 SizeY, int32 MirrorSize)
	{
		switch (EdMode->UISettings->MirrorOp)
		{
		case ELandscapeMirrorOperation::MinusXToPlusX:
		{
			for (int32 Y = 0; Y < SizeY; ++Y) // copy extra column to calc normals for mirror column
			{
				Algo::Reverse(&MirrorData[Y * (2 + MirrorSize) + 1], MirrorSize + 1);
				MirrorData[Y * (2 + MirrorSize) + 0] = MirrorData[Y * (2 + MirrorSize) + 2];
			}
		}
		break;
		case ELandscapeMirrorOperation::PlusXToMinusX:
		{
			for (int32 Y = 0; Y < SizeY; ++Y) // copy extra column to calc normals for mirror column
			{
				Algo::Reverse(&MirrorData[Y * (2 + MirrorSize) + 0], MirrorSize + 1);
				MirrorData[Y * (2 + MirrorSize) + MirrorSize + 1] = MirrorData[Y * (2 + MirrorSize) + MirrorSize - 1];
			}
		}
		break;
		case ELandscapeMirrorOperation::MinusYToPlusY:
		{
			FMemory::Memcpy(&MirrorData[(MirrorSize + 1) * SizeX], &MirrorData[(MirrorSize - 1) * SizeX], SizeX * sizeof(T)); // copy extra row to calc normals for mirror row
		}
			break;
		case ELandscapeMirrorOperation::PlusYToMinusY:
		{
			FMemory::Memcpy(&MirrorData[0 * SizeX], &MirrorData[2 * SizeX], SizeX * sizeof(T)); // copy extra row to calc normals for mirror row
		}
			break;
		default:
			check(0);
			return;
		}
	}

public:
	virtual void ApplyMirror()
	{
		FScopedTransaction Transaction(LOCTEXT("Mirror_Apply", "Landscape Editing: Mirror Landscape"));

		const ULandscapeInfo* const LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo.Get();
		const ALandscapeProxy* const LandscapeProxy = LandscapeInfo->GetLandscapeProxy();
		const FTransform LandscapeToWorld = LandscapeProxy->LandscapeActorToWorld();
		const FVector2D MirrorPoint = EdMode->UISettings->MirrorPoint;

		FLandscapeEditDataInterface LandscapeEdit(EdMode->CurrentToolTarget.LandscapeInfo.Get());

		int32 MinX, MinY, MaxX, MaxY;
		if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
		{
			return;
		}

		const int32 SizeX = (1 + MaxX - MinX);
		const int32 SizeY = (1 + MaxY - MinY);

		int32 SourceMinX, SourceMinY;
		int32 SourceMaxX, SourceMaxY;
		int32 DestMinX, DestMinY;
		int32 DestMaxX, DestMaxY;
		int32 MirrorSize;
		int32 DataSize;
		int32 ReadOffset;
		int32 ReadStride;
		int32 WriteOffset;
		int32 WriteStride;

		switch (EdMode->UISettings->MirrorOp)
		{
		case ELandscapeMirrorOperation::MinusXToPlusX:
		{
			const int32 MirrorX = FMath::RoundToInt(MirrorPoint.X);
			if (MirrorX <= MinX || MirrorX >= MaxX)
			{
				return;
			}
			MirrorSize = FMath::Max(MaxX - MirrorX, MirrorX - MinX); // not including the mirror column itself
			DataSize = SizeY * (2 + MirrorSize); // +2 for mirror column and extra to calc normals for mirror column
			SourceMinX = MirrorX - MirrorSize;
			SourceMaxX = MirrorX;
			SourceMinY = MinY;
			SourceMaxY = MaxY;
			DestMinX = MirrorX - 1; // extra column to calc normals for mirror column
			DestMaxX = MirrorX + MirrorSize;
			DestMinY = MinY;
			DestMaxY = MaxY;
			ReadOffset = 1;
			ReadStride = (2 + MirrorSize);
			WriteOffset = 0;
			WriteStride = (2 + MirrorSize);
		}
		break;
		case ELandscapeMirrorOperation::PlusXToMinusX:
		{
			const int32 MirrorX = FMath::RoundToInt(MirrorPoint.X);
			if (MirrorX <= MinX || MirrorX >= MaxX)
			{
				return;
			}
			MirrorSize = FMath::Max(MaxX - MirrorX, MirrorX - MinX); // not including the mirror column itself
			DataSize = SizeY * (2 + MirrorSize); // +2 for mirror column and extra to calc normals for mirror column
			SourceMinX = MirrorX;
			SourceMaxX = MirrorX + MirrorSize;
			SourceMinY = MinY;
			SourceMaxY = MaxY;
			DestMinX = MirrorX - MirrorSize;
			DestMaxX = MirrorX + 1; // extra column to calc normals for mirror column
			DestMinY = MinY;
			DestMaxY = MaxY;
			ReadOffset = 0;
			ReadStride = (2 + MirrorSize);
			WriteOffset = 0;
			WriteStride = (2 + MirrorSize);
		}
		break;
		case ELandscapeMirrorOperation::MinusYToPlusY:
		{
			const int32 MirrorY = FMath::RoundToInt(MirrorPoint.Y);
			if (MirrorY <= MinY || MirrorY >= MaxY)
			{
				return;
			}
			MirrorSize = FMath::Max(MaxY - MirrorY, MirrorY - MinY); // not including the mirror row itself
			DataSize = (2 + MirrorSize) * SizeX; // +2 for mirror row and extra to calc normals for mirror row
			SourceMinX = MinX;
			SourceMaxX = MaxX;
			SourceMinY = MirrorY - MirrorSize;
			SourceMaxY = MirrorY;
			DestMinX = MinX;
			DestMaxX = MaxX;
			DestMinY = MirrorY - 1; // extra column to calc normals for mirror column
			DestMaxY = MirrorY + MirrorSize;
			ReadOffset = 0 * SizeX;
			ReadStride = SizeX;
			WriteOffset = DataSize - SizeX; // last line first and negative stride to flip :)
			WriteStride = -SizeX;
		}
			break;
		case ELandscapeMirrorOperation::PlusYToMinusY:
		{
			const int32 MirrorY = FMath::RoundToInt(MirrorPoint.Y);
			if (MirrorY <= MinY || MirrorY >= MaxY)
			{
				return;
			}
			MirrorSize = FMath::Max(MaxY - MirrorY, MirrorY - MinY); // not including the mirror row itself
			DataSize = (2 + MirrorSize) * SizeX; // +2 for mirror row and extra to calc normals for mirror row
			SourceMinX = MinX;
			SourceMaxX = MaxX;
			SourceMinY = MirrorY;
			SourceMaxY = MirrorY + MirrorSize;
			DestMinX = MinX;
			DestMaxX = MaxX;
			DestMinY = MirrorY - MirrorSize;
			DestMaxY = MirrorY + 1; // extra column to calc normals for mirror column
			ReadOffset = 1 * SizeX;
			ReadStride = SizeX;
			WriteOffset = DataSize - SizeX; // last line first and negative stride to flip :)
			WriteStride = -SizeX;
		}
			break;
		default:
			check(0);
			return;
		}

		TArray<uint16> MirrorHeightData;
		MirrorHeightData.AddZeroed(DataSize);
		int32 TempMinX = SourceMinX; // GetHeightData overwrites its input min/max x/y
		int32 TempMaxX = SourceMaxX;
		int32 TempMinY = SourceMinY;
		int32 TempMaxY = SourceMaxY;
		LandscapeEdit.GetHeightData(TempMinX, TempMinY, TempMaxX, TempMaxY, &MirrorHeightData[ReadOffset], ReadStride);
		ApplyMirrorInternal(MirrorHeightData.GetData(), SizeX, SizeY, MirrorSize);
		LandscapeEdit.SetHeightData(DestMinX, DestMinY, DestMaxX, DestMaxY, &MirrorHeightData[WriteOffset], WriteStride, true);

		TArray<uint8> MirrorWeightData;
		MirrorWeightData.AddZeroed(DataSize);
		for (const auto& LayerSettings : LandscapeInfo->Layers)
		{
			ULandscapeLayerInfoObject* LayerInfo = LayerSettings.LayerInfoObj;
			if (LayerInfo)
			{
				TempMinX = SourceMinX; // GetWeightData overwrites its input min/max x/y
				TempMaxX = SourceMaxX;
				TempMinY = SourceMinY;
				TempMaxY = SourceMaxY;
				LandscapeEdit.GetWeightData(LayerInfo, TempMinX, TempMinY, TempMaxX, TempMaxY, &MirrorWeightData[ReadOffset], ReadStride);
				ApplyMirrorInternal(MirrorWeightData.GetData(), SizeX, SizeY, MirrorSize);
				LandscapeEdit.SetAlphaData(LayerInfo, DestMinX, DestMinY, DestMaxX, DestMaxY, &MirrorWeightData[WriteOffset], WriteStride, ELandscapeLayerPaintingRestriction::None, false, false);
			}
		}

		LandscapeEdit.Flush();

		TSet<ULandscapeComponent*> Components;
		if (LandscapeEdit.GetComponentsInRegion(DestMinX, DestMinY, DestMaxX, DestMaxY, &Components) && Components.Num() > 0)
		{
			UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(*begin(Components));

			for (ULandscapeComponent* Component : Components)
			{
				// Recreate collision for modified components and update the navmesh
				ULandscapeHeightfieldCollisionComponent* CollisionComponent = Component->CollisionComponent.Get();
				if (CollisionComponent)
				{
					CollisionComponent->RecreateCollision();
					if (NavSys)
					{
						NavSys->UpdateComponentInNavOctree(*CollisionComponent);
					}
				}
			}

			// Flush dynamic foliage (grass)
			ALandscapeProxy::InvalidateGeneratedComponentData(Components);
		}
	}

	void CenterMirrorPoint()
	{
		ULandscapeInfo* const LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo.Get();
		ALandscapeProxy* const LandscapeProxy = LandscapeInfo->GetLandscapeProxy();
		int32 MinX, MinY, MaxX, MaxY;
		if (LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
		{
			EdMode->UISettings->MirrorPoint = FVector2D((float)(MinX + MaxX) / 2.0f, (float)(MinY + MaxY) / 2.0f);
		}
		else
		{
			EdMode->UISettings->MirrorPoint = FVector2D(0, 0);
		}
	}
};

void FEdModeLandscape::ApplyMirrorTool()
{
	if (CurrentTool->GetToolName() == FName("Mirror"))
	{
		FLandscapeToolMirror* MirrorTool = (FLandscapeToolMirror*)CurrentTool;
		MirrorTool->ApplyMirror();
		GEditor->RedrawLevelEditingViewports();
	}
}

void FEdModeLandscape::CenterMirrorTool()
{
	if (CurrentTool->GetToolName() == FName("Mirror"))
	{
		FLandscapeToolMirror* MirrorTool = (FLandscapeToolMirror*)CurrentTool;
		MirrorTool->CenterMirrorPoint();
		GEditor->RedrawLevelEditingViewports();
	}
}

//
// Toolset initialization
//
void FEdModeLandscape::InitializeTool_Mirror()
{
	auto Tool_Mirror = MakeUnique<FLandscapeToolMirror>(this);
	Tool_Mirror->ValidBrushes.Add("BrushSet_Dummy");
	LandscapeTools.Add(MoveTemp(Tool_Mirror));
}

#undef LOCTEXT_NAMESPACE
