// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LogVisualizer.h"
#include "Engine/GameInstance.h"
#include "Debug/DebugDrawService.h"
#include "GameFramework/HUD.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "GeomTools.h"
#endif // WITH_EDITOR
#include "VisualLoggerRenderingActor.h"
#include "VisualLoggerRenderingComponent.h"
#include "DebugRenderSceneProxy.h"
#include "STimeline.h"

class UVisualLoggerRenderingComponent;
class FVisualLoggerSceneProxy : public FDebugRenderSceneProxy
{
public:
	FVisualLoggerSceneProxy(const UVisualLoggerRenderingComponent* InComponent)
		: FDebugRenderSceneProxy(InComponent)
	{
		DrawType = SolidAndWireMeshes;
		ViewFlagName = TEXT("VisLog");
		ViewFlagIndex = uint32(FEngineShowFlags::FindIndexByName(*ViewFlagName));
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bDynamicRelevance = true;
		Result.bNormalTranslucencyRelevance = IsShown(View) && GIsEditor;
		return Result;
	}

	virtual uint32 GetMemoryFootprint(void) const override { return sizeof(*this) + GetAllocatedSize(); }

	uint32 GetAllocatedSize(void) const
	{
		return FDebugRenderSceneProxy::GetAllocatedSize();
	}
};

UVisualLoggerRenderingComponent::UVisualLoggerRenderingComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

FPrimitiveSceneProxy* UVisualLoggerRenderingComponent::CreateSceneProxy()
{
	AVisualLoggerRenderingActor* RenderingActor = Cast<AVisualLoggerRenderingActor>(GetOuter());
	if (RenderingActor == NULL)
	{
		return NULL;
	}

	FVisualLoggerSceneProxy *VLogSceneProxy = new FVisualLoggerSceneProxy(this);
	VLogSceneProxy->Spheres = RenderingActor->Points;
	VLogSceneProxy->Lines = RenderingActor->Lines;
	VLogSceneProxy->Cones = RenderingActor->Cones;
	VLogSceneProxy->Boxes = RenderingActor->Boxes;
	VLogSceneProxy->Meshes = RenderingActor->Meshes;
	VLogSceneProxy->Cones = RenderingActor->Cones;
	VLogSceneProxy->Texts = RenderingActor->Texts;
	VLogSceneProxy->Cylinders = RenderingActor->Cylinders;
	VLogSceneProxy->Capsles = RenderingActor->Capsles;

	return VLogSceneProxy;
}

FBoxSphereBounds UVisualLoggerRenderingComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox MyBounds;
	MyBounds.Init();

	MyBounds = FBox(FVector(-HALF_WORLD_MAX, -HALF_WORLD_MAX, -HALF_WORLD_MAX), FVector(HALF_WORLD_MAX, HALF_WORLD_MAX, HALF_WORLD_MAX));

	return MyBounds;
}

void UVisualLoggerRenderingComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

#if WITH_EDITOR
	if (SceneProxy)
	{
		static_cast<FVisualLoggerSceneProxy*>(SceneProxy)->RegisterDebugDrawDelgate();
	}
#endif
}

void UVisualLoggerRenderingComponent::DestroyRenderState_Concurrent()
{
#if WITH_EDITOR
	if (SceneProxy)
	{
		static_cast<FVisualLoggerSceneProxy*>(SceneProxy)->UnregisterDebugDrawDelgate();
	}
#endif

	Super::DestroyRenderState_Concurrent();
}

AVisualLoggerRenderingActor::AVisualLoggerRenderingActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RenderingComponent = CreateDefaultSubobject<UVisualLoggerRenderingComponent>(TEXT("RenderingComponent"));
}

void AVisualLoggerRenderingActor::AddDebugRendering()
{
	const float Thickness = 2;

	{
		const FVector BoxExtent(100, 100, 100);
		const FBox Box(FVector(128), FVector(300));
		Boxes.Add(FDebugRenderSceneProxy::FDebugBox(Box, FColor::Red));
		FTransform Trans;
		Trans.SetRotation(FQuat::MakeFromEuler(FVector(0.1, 0.2, 1.2)));
		Boxes.Add(FDebugRenderSceneProxy::FDebugBox(Box, FColor::Red, Trans));
	}
	{
		const FVector Orgin = FVector(400,0,128);
		const FVector Direction = FVector(0,0,1);
		const float Length = 300;

		FVector YAxis, ZAxis;
		Direction.FindBestAxisVectors(YAxis, ZAxis);
		Cones.Add(FDebugRenderSceneProxy::FCone(FScaleMatrix(FVector(Length)) * FMatrix(Direction, YAxis, ZAxis, Orgin), 30, 30, FColor::Blue));
	}
	{
		const FVector Start = FVector(700, 0, 128);
		const FVector End = FVector(700, 0, 128+300);
		const float Radius = 200;
		const float HalfHeight = 150;
		Cylinders.Add(FDebugRenderSceneProxy::FWireCylinder(Start + FVector(0, 0, HalfHeight), Radius, HalfHeight, FColor::Magenta));
	}

	{
		const FVector Center = FVector(1000, 0, 128);
		const float HalfHeight = 150;
		const float Radius = 50;
		const FQuat Rotation = FQuat::Identity;

		const FMatrix Axes = FQuatRotationTranslationMatrix(Rotation, FVector::ZeroVector);
		const FVector XAxis = Axes.GetScaledAxis(EAxis::X);
		const FVector YAxis = Axes.GetScaledAxis(EAxis::Y);
		const FVector ZAxis = Axes.GetScaledAxis(EAxis::Z);

		Capsles.Add(FDebugRenderSceneProxy::FCapsule(Center, Radius, XAxis, YAxis, ZAxis, HalfHeight, FColor::Yellow));
	}
	{
		const float Radius = 50;
		Points.Add(FDebugRenderSceneProxy::FSphere(10, FVector(1300, 0, 128), FColor::White));
	}
}

void AVisualLoggerRenderingActor::ObjectSelectionChanged(TSharedPtr<class STimeline> TimeLine)
{
	if (TimeLine.IsValid() == false)
	{
		Lines.Reset();
		Points.Reset();
		Cones.Reset();
		Boxes.Reset();
		Meshes.Reset();
		Texts.Reset();
		Cylinders.Reset();
		Capsles.Reset();
		LogEntriesPath.Reset();
		MarkComponentsRenderStateDirty();
		return;
	}

	const TArray<FVisualLogDevice::FVisualLogEntryItem>& Entries = TimeLine->GetEntries();
	LogEntriesPath.Reset();
	for (const auto &CurrentEntry : Entries)
	{
		if (CurrentEntry.Entry.Location != FVector::ZeroVector)
		{
			LogEntriesPath.Add(CurrentEntry.Entry.Location);
		}
	}
}

void AVisualLoggerRenderingActor::OnItemSelectionChanged(const FVisualLogDevice::FVisualLogEntryItem& EntryItem)
{
	const FVisualLogEntry* Entry = &EntryItem.Entry;
	const FVisualLogShapeElement* ElementToDraw = Entry->ElementsToDraw.GetData();
	const int32 ElementsCount = Entry->ElementsToDraw.Num();

	Lines.Reset();
	Points.Reset();
	Cones.Reset();
	Boxes.Reset();
	Meshes.Reset();
	Texts.Reset();
	Cylinders.Reset();
	Capsles.Reset();

#if 0
	AddDebugRendering();
#endif

	{
		const float Length = 100;
		const FVector DirectionNorm = FVector(0, 0, 1).GetSafeNormal();
		FVector YAxis, ZAxis;
		DirectionNorm.FindBestAxisVectors(YAxis, ZAxis);
		Cones.Add(FDebugRenderSceneProxy::FCone(FScaleMatrix(FVector(Length)) * FMatrix(DirectionNorm, YAxis, ZAxis, Entry->Location), 5, 5, FColor::Red));
	}

	if (LogEntriesPath.Num())
	{
		FVector Location = LogEntriesPath[0];
		for (int32 Index = 1; Index < LogEntriesPath.Num(); ++Index)
		{
			const FVector CurrentLocation = LogEntriesPath[Index];
			Lines.Add(FDebugRenderSceneProxy::FDebugLine(Location, CurrentLocation, FColor(160, 160, 240), 2.0));
			Location = CurrentLocation;
		}
	}

	TMap<FName, FVisualLogExtensionInterface*>& AllExtensions = FVisualLogger::Get().GetAllExtensions();
	for (auto& Extension : AllExtensions)
	{
		Extension.Value->OnTimestampChange(Entry->TimeStamp, GetWorld(), this);
	}

	for (const auto CurrentData : Entry->DataBlocks)
	{
		const FName TagName = CurrentData.TagName;
		const bool bIsValidByFilter = FCategoryFiltersManager::Get().MatchCategoryFilters(CurrentData.Category.ToString(), ELogVerbosity::All) && FCategoryFiltersManager::Get().MatchCategoryFilters(CurrentData.TagName.ToString(), ELogVerbosity::All);
		FVisualLogExtensionInterface* Extension = FVisualLogger::Get().GetExtensionForTag(TagName);
		if (!Extension)
		{
			continue;
		}

		if (!bIsValidByFilter)
		{
			Extension->DisableDrawingForData(GetWorld(), NULL, this, TagName, CurrentData, Entry->TimeStamp);
		}
		else
		{
			Extension->DrawData(GetWorld(), NULL, this, TagName, CurrentData, Entry->TimeStamp);
		}
	}

	for (int32 ElementIndex = 0; ElementIndex < ElementsCount; ++ElementIndex, ++ElementToDraw)
	{
		if (!FCategoryFiltersManager::Get().MatchCategoryFilters(ElementToDraw->Category.ToString(), ElementToDraw->Verbosity))
		{
			continue;
		}


		const FVector CorridorOffset = NavigationDebugDrawing::PathOffset * 1.25f;
		const FColor Color = ElementToDraw->GetFColor();

		switch (ElementToDraw->GetType())
		{
		case EVisualLoggerShapeElement::SinglePoint:
		{
			const float Radius = float(ElementToDraw->Radius);
			const bool bDrawLabel = ElementToDraw->Description.IsEmpty() == false;
			for (int32 Index=0; Index < ElementToDraw->Points.Num(); ++Index)
			{
				const FVector& Point = ElementToDraw->Points[Index];
				Points.Add(FDebugRenderSceneProxy::FSphere(Radius, Point, Color));
				if (bDrawLabel)
				{
					const FString PrintString = FString::Printf(TEXT("%s_%d"), *ElementToDraw->Description, Index);
					Texts.Add(FDebugRenderSceneProxy::FText3d(PrintString, Point, Color));
				}
			}
		}
		break;
		case EVisualLoggerShapeElement::Polygon:
		{
			FClipSMPolygon InPoly(ElementToDraw->Points.Num());
			for (int32 Index = 0; Index < ElementToDraw->Points.Num(); Index++)
			{
				FClipSMVertex v1;
				v1.Pos = ElementToDraw->Points[Index];
				InPoly.Vertices.Add(v1);
			}

			TArray<FClipSMTriangle> OutTris;
			if (TriangulatePoly(OutTris, InPoly, false))
			{
				int32 LastIndex = 0;
				FDebugRenderSceneProxy::FMesh TestMesh;
				TestMesh.Color = Color;

				RemoveRedundantTriangles(OutTris);
				for (const auto& CurrentTri : OutTris)
				{
					TestMesh.Vertices.Add(FDynamicMeshVertex(CurrentTri.Vertices[0].Pos + CorridorOffset));
					TestMesh.Vertices.Add(FDynamicMeshVertex(CurrentTri.Vertices[1].Pos + CorridorOffset));
					TestMesh.Vertices.Add(FDynamicMeshVertex(CurrentTri.Vertices[2].Pos + CorridorOffset));
					TestMesh.Indices.Add(LastIndex++);
					TestMesh.Indices.Add(LastIndex++);
					TestMesh.Indices.Add(LastIndex++);
				}
				Meshes.Add(TestMesh);
			}

			for (int32 VIdx = 0; VIdx < ElementToDraw->Points.Num(); VIdx++)
			{
				Lines.Add(FDebugRenderSceneProxy::FDebugLine(
					ElementToDraw->Points[VIdx] + CorridorOffset,
					ElementToDraw->Points[(VIdx + 1) % ElementToDraw->Points.Num()] + CorridorOffset,
					FColor::Cyan, 
					2)
				);
			}

		}
			break;
		case EVisualLoggerShapeElement::Segment:
		{
			const float Thickness = float(ElementToDraw->Thicknes);
			const bool bDrawLabel = ElementToDraw->Description.IsEmpty() == false && ElementToDraw->Points.Num() > 2;
			const FVector* Location = ElementToDraw->Points.GetData();
			for (int32 Index = 0; Index + 1 < ElementToDraw->Points.Num(); Index += 2, Location += 2)
			{
				Lines.Add(FDebugRenderSceneProxy::FDebugLine(*Location, *(Location + 1), Color, Thickness));

				if (bDrawLabel)
				{
					const FString PrintString = FString::Printf(TEXT("%s_%d"), *ElementToDraw->Description, Index);
					Texts.Add(FDebugRenderSceneProxy::FText3d(PrintString, (*Location + (*(Location + 1) - *Location) / 2), Color));
				}
			}
			if (ElementToDraw->Description.IsEmpty() == false)
			{
				Texts.Add(FDebugRenderSceneProxy::FText3d(ElementToDraw->Description, ElementToDraw->Points[0] + (ElementToDraw->Points[1] - ElementToDraw->Points[0]) / 2, Color));
			}
		}
			break;
		case EVisualLoggerShapeElement::Path:
		{
			const float Thickness = float(ElementToDraw->Thicknes);
			FVector Location = ElementToDraw->Points[0];
			for (int32 Index = 1; Index < ElementToDraw->Points.Num(); ++Index)
			{
				const FVector CurrentLocation = ElementToDraw->Points[Index];
				Lines.Add(FDebugRenderSceneProxy::FDebugLine(Location, CurrentLocation, Color, Thickness));
				Location = CurrentLocation;
			}
		}
		break;
		case EVisualLoggerShapeElement::Box:
		{
			const float Thickness = float(ElementToDraw->Thicknes);
			const bool bDrawLabel = ElementToDraw->Description.IsEmpty() == false && ElementToDraw->Points.Num() > 2;
			const FVector* BoxExtent = ElementToDraw->Points.GetData();
			for (int32 Index = 0; Index + 1 < ElementToDraw->Points.Num(); Index += 2, BoxExtent += 2)
			{
				const FBox Box = FBox(*BoxExtent, *(BoxExtent + 1));
				Boxes.Add(FDebugRenderSceneProxy::FDebugBox(Box, Color, FTransform(ElementToDraw->TransformationMatrix)));

				if (bDrawLabel)
				{
					const FString PrintString = FString::Printf(TEXT("%s_%d"), *ElementToDraw->Description, Index);
					Texts.Add(FDebugRenderSceneProxy::FText3d(PrintString, Box.GetCenter(), Color));
				}
			}
			if (ElementToDraw->Description.IsEmpty() == false)
			{
				Texts.Add(FDebugRenderSceneProxy::FText3d(ElementToDraw->Description, ElementToDraw->Points[0] + (ElementToDraw->Points[1] - ElementToDraw->Points[0]) / 2, Color));
			}
		}
		break;
		case EVisualLoggerShapeElement::Cone:
		{
			const float Thickness = float(ElementToDraw->Thicknes);
			const bool bDrawLabel = ElementToDraw->Description.IsEmpty() == false;
			for (int32 Index = 0; Index + 2 < ElementToDraw->Points.Num(); Index += 3)
			{
				const FVector Orgin = ElementToDraw->Points[Index];
				const FVector Direction = ElementToDraw->Points[Index + 1].GetSafeNormal();
				const FVector Angles = ElementToDraw->Points[Index + 2];
				const float Length = Angles.X;

				FVector YAxis, ZAxis;
				Direction.FindBestAxisVectors(YAxis, ZAxis);
				Cones.Add(FDebugRenderSceneProxy::FCone(FScaleMatrix(FVector(Length)) * FMatrix(Direction, YAxis, ZAxis, Orgin), Angles.Y, Angles.Z, Color));

				if (bDrawLabel)
				{
					Texts.Add(FDebugRenderSceneProxy::FText3d(ElementToDraw->Description, Orgin, Color));
				}
			}
		}
			break;
		case EVisualLoggerShapeElement::Cylinder:
		{
			const float Thickness = float(ElementToDraw->Thicknes);
			const bool bDrawLabel = ElementToDraw->Description.IsEmpty() == false;
			for (int32 Index = 0; Index + 2 < ElementToDraw->Points.Num(); Index += 3)
			{
				const FVector Start = ElementToDraw->Points[Index];
				const FVector End = ElementToDraw->Points[Index + 1];
				const FVector OtherData = ElementToDraw->Points[Index + 2];
				Cylinders.Add(FDebugRenderSceneProxy::FWireCylinder(Start, OtherData.X, (End - Start).Size()*0.5, Color));
				if (bDrawLabel)
				{
					Texts.Add(FDebugRenderSceneProxy::FText3d(ElementToDraw->Description, Start, Color));
				}
			}
		}
			break;
		case EVisualLoggerShapeElement::Capsule:
		{
			const float Thickness = float(ElementToDraw->Thicknes);
			const bool bDrawLabel = ElementToDraw->Description.IsEmpty() == false;
			for (int32 Index = 0; Index + 2 < ElementToDraw->Points.Num(); Index += 3)
			{
				const FVector Center = ElementToDraw->Points[Index + 0];
				const FVector FirstData = ElementToDraw->Points[Index + 1];
				const FVector SecondData = ElementToDraw->Points[Index + 2];
				const float HalfHeight = FirstData.X;
				const float Radius = FirstData.Y;
				const FQuat Rotation = FQuat(FirstData.Z, SecondData.X, SecondData.Y, SecondData.Z);
				
				const FMatrix Axes = FQuatRotationTranslationMatrix(Rotation, FVector::ZeroVector);
				const FVector XAxis = Axes.GetScaledAxis(EAxis::X);
				const FVector YAxis = Axes.GetScaledAxis(EAxis::Y);
				const FVector ZAxis = Axes.GetScaledAxis(EAxis::Z);

				Capsles.Add(FDebugRenderSceneProxy::FCapsule(Center, Radius, XAxis, YAxis, ZAxis, HalfHeight, Color));
				if (bDrawLabel)
				{
					Texts.Add(FDebugRenderSceneProxy::FText3d(ElementToDraw->Description, Center, Color));
				}
			}
		}
			break;
		}
	}

	MarkComponentsRenderStateDirty();
}

