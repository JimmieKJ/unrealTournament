// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "StaticMeshResources.h"
#include "ObjectTools.h"
#include "FoliageEdMode.h"
#include "ScopedTransaction.h"

#include "FoliageEdModeToolkit.h"
#include "ModuleManager.h"
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "Toolkits/ToolkitManager.h"

#include "Runtime/AssetRegistry/Public/AssetRegistryModule.h"

//Slate dependencies
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "Editor/LevelEditor/Public/SLevelViewport.h"

// Classes
#include "Foliage/InstancedFoliageActor.h"
#include "Foliage/FoliageType.h"
#include "LandscapeProxy.h"
#include "LandscapeComponent.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeInfo.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/CollisionProfile.h"
#include "Components/ModelComponent.h"
#include "EngineUtils.h"
#include "LandscapeDataAccess.h"

#define LOCTEXT_NAMESPACE "FoliageEdMode"
#define FOLIAGE_SNAP_TRACE (10000.f)

//
// FEdModeFoliage
//

/** Constructor */
FEdModeFoliage::FEdModeFoliage()
	: FEdMode()
	, bToolActive(false)
	, bCanAltDrag(false)
	, SelectionIFA(nullptr)
{
	// Load resources and construct brush component
	UMaterial* BrushMaterial = nullptr;
	UStaticMesh* StaticMesh = nullptr;
	if (!IsRunningCommandlet())
	{
		BrushMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/EditorLandscapeResources/FoliageBrushSphereMaterial.FoliageBrushSphereMaterial"), nullptr, LOAD_None, nullptr);
		StaticMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/EngineMeshes/Sphere.Sphere"), nullptr, LOAD_None, nullptr);
	}

	SphereBrushComponent = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass());
	SphereBrushComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SphereBrushComponent->SetCollisionObjectType(ECC_WorldDynamic);
	SphereBrushComponent->StaticMesh = StaticMesh;
	SphereBrushComponent->OverrideMaterials.Add(BrushMaterial);
	SphereBrushComponent->SetAbsolute(true, true, true);
	SphereBrushComponent->CastShadow = false;

	bBrushTraceValid = false;
	BrushLocation = FVector::ZeroVector;
}


/** Destructor */
FEdModeFoliage::~FEdModeFoliage()
{
	// Save UI settings to config file
	UISettings.Save();
	FEditorDelegates::MapChange.RemoveAll(this);
}


/** FGCObject interface */
void FEdModeFoliage::AddReferencedObjects(FReferenceCollector& Collector)
{
	// Call parent implementation
	FEdMode::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(SphereBrushComponent);
}

/** FEdMode: Called when the mode is entered */
void FEdModeFoliage::Enter()
{
	FEdMode::Enter();

	// Clear any selection in case the instanced foliage actor is selected
	GEditor->SelectNone(false, true);

	// Load UI settings from config file
	UISettings.Load();

	// Bind to editor callbacks
	FEditorDelegates::NewCurrentLevel.AddSP(this, &FEdModeFoliage::NotifyNewCurrentLevel);

	// Force real-time viewports.  We'll back up the current viewport state so we can restore it when the
	// user exits this mode.
	const bool bWantRealTime = true;
	const bool bRememberCurrentState = true;
	ForceRealTimeViewports(bWantRealTime, bRememberCurrentState);

	// Reapply selection visualization on any foliage items
	if (UISettings.GetSelectToolSelected() || UISettings.GetLassoSelectToolSelected())
	{
		SelectionIFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());
		SelectionIFA->ApplySelectionToComponents(true);
	}
	else
	{
		SelectionIFA = nullptr;
	}

	if (!Toolkit.IsValid())
	{
		Toolkit = MakeShareable(new FFoliageEdModeToolkit);
		Toolkit->Init(Owner->GetToolkitHost());
	}
}

/** FEdMode: Called when the mode is exited */
void FEdModeFoliage::Exit()
{
	FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
	Toolkit.Reset();

	//
	FEditorDelegates::NewCurrentLevel.RemoveAll(this);

	// Remove the brush
	SphereBrushComponent->UnregisterComponent();

	// Restore real-time viewport state if we changed it
	const bool bWantRealTime = false;
	const bool bRememberCurrentState = false;
	ForceRealTimeViewports(bWantRealTime, bRememberCurrentState);

	// Clear the cache (safety, should be empty!)
	LandscapeLayerCaches.Empty();

	// Save UI settings to config file
	UISettings.Save();

	// Clear the placed level info
	FoliageMeshList.Empty();

	// Clear selection visualization on any foliage items
	if (SelectionIFA && (UISettings.GetSelectToolSelected() || UISettings.GetLassoSelectToolSelected()))
	{
		SelectionIFA->ApplySelectionToComponents(false);
	}

	// Call base Exit method to ensure proper cleanup
	FEdMode::Exit();
}

void FEdModeFoliage::PostUndo()
{
	FEdMode::PostUndo();

	StaticCastSharedPtr<FFoliageEdModeToolkit>(Toolkit)->PostUndo();
}

/** When the user changes the active streaming level with the level browser */
void FEdModeFoliage::NotifyNewCurrentLevel()
{
	// Remove any selections in the old level and reapply for the new level
	if (UISettings.GetSelectToolSelected() || UISettings.GetLassoSelectToolSelected())
	{
		if (SelectionIFA)
		{
			SelectionIFA->ApplySelectionToComponents(false);
		}
		SelectionIFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());
		SelectionIFA->ApplySelectionToComponents(true);
	}
}

/** When the user changes the current tool in the UI */
void FEdModeFoliage::NotifyToolChanged()
{
	if (UISettings.GetSelectToolSelected() || UISettings.GetLassoSelectToolSelected())
	{
		if (SelectionIFA == nullptr)
		{
			SelectionIFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());
			SelectionIFA->ApplySelectionToComponents(true);
		}
	}
	else
	{
		if (SelectionIFA)
		{
			SelectionIFA->ApplySelectionToComponents(false);
		}
		SelectionIFA = nullptr;
	}
}

bool FEdModeFoliage::DisallowMouseDeltaTracking() const
{
	// We never want to use the mouse delta tracker while painting
	return bToolActive;
}

/** FEdMode: Called once per frame */
void FEdModeFoliage::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	if (bToolActive)
	{
		ApplyBrush(ViewportClient);
	}

	FEdMode::Tick(ViewportClient, DeltaTime);

	if (UISettings.GetSelectToolSelected() || UISettings.GetLassoSelectToolSelected())
	{
		// Update pivot
		AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());
		Owner->PivotLocation = Owner->SnappedLocation = IFA->GetSelectionLocation();
	}

	// Update the position and size of the brush component
	if (bBrushTraceValid && (UISettings.GetPaintToolSelected() || UISettings.GetReapplyToolSelected() || UISettings.GetLassoSelectToolSelected()))
	{
		// Scale adjustment is due to default sphere SM size.
		FTransform BrushTransform = FTransform(FQuat::Identity, BrushLocation, FVector(UISettings.GetRadius() * 0.00625f));
		SphereBrushComponent->SetRelativeTransform(BrushTransform);

		if (!SphereBrushComponent->IsRegistered())
		{
			SphereBrushComponent->RegisterComponentWithWorld(ViewportClient->GetWorld());
		}
	}
	else
	{
		if (SphereBrushComponent->IsRegistered())
		{
			SphereBrushComponent->UnregisterComponent();
		}
	}
}

static bool FoliageTrace(UWorld* InWorld, AInstancedFoliageActor* IFA, FHitResult& OutHit, FVector InStart, FVector InEnd, FName InTraceTag, bool InbReturnFaceIndex = false)
{
	FCollisionQueryParams QueryParams(InTraceTag, true, IFA);
	QueryParams.bReturnFaceIndex = InbReturnFaceIndex;

	bool bResult = true;
	while (true)
	{
		bResult = InWorld->LineTraceSingle(OutHit, InStart, InEnd, QueryParams, FCollisionObjectQueryParams(ECC_WorldStatic));
		if (bResult)
		{
			// In the editor traces can hit "No Collision" type actors, so ugh.
			FBodyInstance* BodyInstance = OutHit.Component->GetBodyInstance();
			if (BodyInstance->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics || BodyInstance->GetResponseToChannel(ECC_WorldStatic) != ECR_Block)
			{
				AActor* Actor = OutHit.Actor.Get();
				if (Actor)
				{
					QueryParams.AddIgnoredActor(Actor);
				}
				InStart = OutHit.ImpactPoint;
				continue;
			}
		}
		break;
	}
	return bResult;
}

/** Trace under the mouse cursor and update brush position */
void FEdModeFoliage::FoliageBrushTrace(FEditorViewportClient* ViewportClient, int32 MouseX, int32 MouseY)
{
	bBrushTraceValid = false;
	if (!ViewportClient->IsMovingCamera())
	{
		if (UISettings.GetPaintToolSelected() || UISettings.GetReapplyToolSelected() || UISettings.GetLassoSelectToolSelected())
		{
			// Compute a world space ray from the screen space mouse coordinates
			FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
				ViewportClient->Viewport,
				ViewportClient->GetScene(),
				ViewportClient->EngineShowFlags)
				.SetRealtimeUpdate(ViewportClient->IsRealtime()));
			FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
			FViewportCursorLocation MouseViewportRay(View, ViewportClient, MouseX, MouseY);

			FVector Start = MouseViewportRay.GetOrigin();
			BrushTraceDirection = MouseViewportRay.GetDirection();
			FVector End = Start + WORLD_MAX * BrushTraceDirection;

			FHitResult Hit;
			UWorld* World = ViewportClient->GetWorld();
			AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(World, false);
			static FName NAME_FoliageBrush = FName(TEXT("FoliageBrush"));
			if (FoliageTrace(World, IFA, Hit, Start, End, NAME_FoliageBrush))
			{
				// Check filters
				UPrimitiveComponent* PrimComp = Hit.Component.Get();
				UMaterialInterface* Material = PrimComp ? PrimComp->GetMaterial(0) : nullptr;

				if (PrimComp &&
					PrimComp->GetOutermost() == World->GetCurrentLevel()->GetOutermost() &&
					(UISettings.bFilterLandscape || !PrimComp->IsA(ULandscapeHeightfieldCollisionComponent::StaticClass())) &&
					(UISettings.bFilterStaticMesh || !PrimComp->IsA(UStaticMeshComponent::StaticClass())) &&
					(UISettings.bFilterBSP || !PrimComp->IsA(UModelComponent::StaticClass())) &&
					(UISettings.bFilterTranslucent || !Material || !IsTranslucentBlendMode(Material->GetBlendMode()))
					)
				{
					// Adjust the sphere brush
					BrushLocation = Hit.Location;
					bBrushTraceValid = true;
				}
			}
		}
	}
}

/**
 * Called when the mouse is moved over the viewport
 *
 * @param	InViewportClient	Level editor viewport client that captured the mouse input
 * @param	InViewport			Viewport that captured the mouse input
 * @param	InMouseX			New mouse cursor X coordinate
 * @param	InMouseY			New mouse cursor Y coordinate
 *
 * @return	true if input was handled
 */
bool FEdModeFoliage::MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 MouseX, int32 MouseY)
{
	FoliageBrushTrace(ViewportClient, MouseX, MouseY);
	return false;
}

/**
 * Called when the mouse is moved while a window input capture is in effect
 *
 * @param	InViewportClient	Level editor viewport client that captured the mouse input
 * @param	InViewport			Viewport that captured the mouse input
 * @param	InMouseX			New mouse cursor X coordinate
 * @param	InMouseY			New mouse cursor Y coordinate
 *
 * @return	true if input was handled
 */
bool FEdModeFoliage::CapturedMouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 MouseX, int32 MouseY)
{
	FoliageBrushTrace(ViewportClient, MouseX, MouseY);
	return false;
}

void FEdModeFoliage::GetRandomVectorInBrush(FVector& OutStart, FVector& OutEnd)
{
	// Find Rx and Ry inside the unit circle
	float Ru = (2.f * FMath::FRand() - 1.f);
	float Rv = (2.f * FMath::FRand() - 1.f) * FMath::Sqrt(1.f - FMath::Square(Ru));

	// find random point in circle thru brush location parallel to screen surface
	FVector U, V;
	BrushTraceDirection.FindBestAxisVectors(U, V);
	FVector Point = Ru * U + Rv * V;

	// find distance to surface of sphere brush from this point
	float Rw = FMath::Sqrt(1.f - (FMath::Square(Ru) + FMath::Square(Rv)));

	OutStart = BrushLocation + UISettings.GetRadius() * (Ru * U + Rv * V - Rw * BrushTraceDirection);
	OutEnd = BrushLocation + UISettings.GetRadius() * (Ru * U + Rv * V + Rw * BrushTraceDirection);
}


// Number of buckets for layer weight histogram distribution.
#define NUM_INSTANCE_BUCKETS 10

bool CheckCollisionWithWorld(UFoliageType* Settings, const FFoliageInstance& Inst, const FVector& HitNormal, const FVector& HitLocation, UWorld* InWorld, AInstancedFoliageActor* InIFA)
{
	FMatrix InstTransform = Inst.GetInstanceWorldTransform().ToMatrixWithScale();
	FVector LocalHit = InstTransform.InverseTransformPosition(HitLocation);

	if (Settings->CollisionWithWorld)
	{
		// Check for overhanging ledge
		{
			FVector LocalSamplePos[4] = { FVector(Settings->LowBoundOriginRadius.Z, 0, 0),
				FVector(-Settings->LowBoundOriginRadius.Z, 0, 0),
				FVector(0, Settings->LowBoundOriginRadius.Z, 0),
				FVector(0, -Settings->LowBoundOriginRadius.Z, 0)
			};

			for (uint32 i = 0; i < 4; ++i)
			{
				FHitResult Hit;
				FVector SamplePos = InstTransform.TransformPosition(FVector(Settings->LowBoundOriginRadius.X, Settings->LowBoundOriginRadius.Y, 2.f) + LocalSamplePos[i]);
				float WorldRadius = (Settings->LowBoundOriginRadius.Z + 2.f)*FMath::Max(Inst.DrawScale3D.X, Inst.DrawScale3D.Y);
				FVector NormalVector = Settings->AlignToNormal ? HitNormal : FVector(0, 0, 1);
				if (FoliageTrace(InWorld, InIFA, Hit, SamplePos, SamplePos - NormalVector*WorldRadius, NAME_None))
				{
					if (LocalHit.Z - Inst.ZOffset < Settings->LowBoundOriginRadius.Z)
					{
						continue;
					}
				}
				return false;
			}
		}

		// Check collision with Bounding Box
		{
			FBox MeshBox = Settings->MeshBounds.GetBox();
			MeshBox.Min.Z = FMath::Min(MeshBox.Max.Z, LocalHit.Z + Settings->MeshBounds.BoxExtent.Z * 0.05f);
			FBoxSphereBounds ShrinkBound(MeshBox);
			FBoxSphereBounds WorldBound = ShrinkBound.TransformBy(InstTransform);
			//::DrawDebugBox(InWorld, WorldBound.Origin, WorldBound.BoxExtent, FColor::Red, true, 10.f);
			static FName NAME_FoliageCollisionWithWorld = FName(TEXT("FoliageCollisionWithWorld"));
			if (InWorld->OverlapTest(WorldBound.Origin, FQuat(Inst.Rotation), FCollisionShape::MakeBox(ShrinkBound.BoxExtent * Inst.DrawScale3D * Settings->CollisionScale), FCollisionQueryParams(NAME_FoliageCollisionWithWorld, false), FCollisionObjectQueryParams(ECC_WorldStatic)))
			{
				return false;
			}
		}
	}

	return true;
}

// Struct to hold potential instances we've randomly sampled
struct FPotentialInstance
{
	FVector HitLocation;
	FVector HitNormal;
	UPrimitiveComponent* HitComponent;
	float HitWeight;

	FPotentialInstance(FVector InHitLocation, FVector InHitNormal, UPrimitiveComponent* InHitComponent, float InHitWeight)
		: HitLocation(InHitLocation)
		, HitNormal(InHitNormal)
		, HitComponent(InHitComponent)
		, HitWeight(InHitWeight)
	{}

	bool PlaceInstance(UFoliageType* Settings, FFoliageInstance& Inst, UWorld* InWorld, AInstancedFoliageActor* InIFA, bool bSkipCollision = false)
	{
		if (Settings->UniformScale)
		{
			float Scale = Settings->ScaleMinX + FMath::FRand() * (Settings->ScaleMaxX - Settings->ScaleMinX);
			Inst.DrawScale3D = FVector(Scale, Scale, Scale);
		}
		else
		{
			float LockRand = FMath::FRand();
			Inst.DrawScale3D.X = Settings->ScaleMinX + (Settings->LockScaleX ? LockRand : FMath::FRand()) * (Settings->ScaleMaxX - Settings->ScaleMinX);
			Inst.DrawScale3D.Y = Settings->ScaleMinY + (Settings->LockScaleY ? LockRand : FMath::FRand()) * (Settings->ScaleMaxY - Settings->ScaleMinY);
			Inst.DrawScale3D.Z = Settings->ScaleMinZ + (Settings->LockScaleZ ? LockRand : FMath::FRand()) * (Settings->ScaleMaxZ - Settings->ScaleMinZ);
		}

		Inst.ZOffset = Settings->ZOffsetMin + FMath::FRand() * (Settings->ZOffsetMax - Settings->ZOffsetMin);

		Inst.Location = HitLocation;

		// Random yaw and optional random pitch up to the maximum
		Inst.Rotation = FRotator(FMath::FRand() * Settings->RandomPitchAngle, 0.f, 0.f);

		if (Settings->RandomYaw)
		{
			Inst.Rotation.Yaw = FMath::FRand() * 360.f;
		}
		else
		{
			Inst.Flags |= FOLIAGE_NoRandomYaw;
		}

		if (Settings->AlignToNormal)
		{
			Inst.AlignToNormal(HitNormal, Settings->AlignMaxAngle);
		}

		// Apply the Z offset in local space
		if (FMath::Abs(Inst.ZOffset) > KINDA_SMALL_NUMBER)
		{
			Inst.Location = Inst.GetInstanceWorldTransform().TransformPosition(FVector(0, 0, Inst.ZOffset));
		}

		UModelComponent* ModelComponent = Cast<UModelComponent>(HitComponent);
		if (ModelComponent)
		{
			ABrush* BrushActor = ModelComponent->GetModel()->FindBrush(HitLocation);
			if (BrushActor)
			{
				HitComponent = BrushActor->GetBrushComponent();
			}
		}

		Inst.Base = HitComponent;

		return bSkipCollision || CheckCollisionWithWorld(Settings, Inst, HitNormal, HitLocation, InWorld, InIFA);
	}
};

static bool CheckLocationForPotentialInstance(FFoliageMeshInfo& MeshInfo, const UFoliageType* Settings, float DensityCheckRadius, const FVector& Location, const FVector& Normal, TArray<FVector>& PotentialInstanceLocations, FFoliageInstanceHash& PotentialInstanceHash)
{
	// Check slope
	if ((Settings->GroundSlope > 0.f && Normal.Z <= FMath::Cos(PI * Settings->GroundSlope / 180.f) - SMALL_NUMBER) ||
		(Settings->GroundSlope < 0.f && Normal.Z >= FMath::Cos(-PI * Settings->GroundSlope / 180.f) + SMALL_NUMBER))
	{
		return false;
	}

	// Check height range
	if (Location.Z < Settings->HeightMin || Location.Z > Settings->HeightMax)
	{
		return false;
	}

	// Check existing instances. Use the Density radius rather than the minimum radius
	if (MeshInfo.CheckForOverlappingSphere(FSphere(Location, DensityCheckRadius)))
	{
		return false;
	}

	// Check if we're too close to any other instances
	if (Settings->Radius > 0.f)
	{
		// Check with other potential instances we're about to add.
		bool bFoundOverlapping = false;
		float RadiusSquared = FMath::Square(DensityCheckRadius/*Settings->Radius*/);

		auto TempInstances = PotentialInstanceHash.GetInstancesOverlappingBox(FBox::BuildAABB(Location, FVector(Settings->Radius)));
		for (int32 Idx : TempInstances)
		{
			if ((PotentialInstanceLocations[Idx] - Location).SizeSquared() < RadiusSquared)
			{
				bFoundOverlapping = true;
				break;
			}
		}
		if (bFoundOverlapping)
		{
			return false;
		}
	}

	int32 PotentialIdx = PotentialInstanceLocations.Add(Location);
	PotentialInstanceHash.InsertInstance(Location, PotentialIdx);
	return true;
}

static bool CheckVertexColor(const UFoliageType* Settings, const FColor& VertexColor)
{
	uint8 ColorChannel;
	switch (Settings->VertexColorMask)
	{
	case FOLIAGEVERTEXCOLORMASK_Red:
		ColorChannel = VertexColor.R;
		break;
	case FOLIAGEVERTEXCOLORMASK_Green:
		ColorChannel = VertexColor.G;
		break;
	case FOLIAGEVERTEXCOLORMASK_Blue:
		ColorChannel = VertexColor.B;
		break;
	case FOLIAGEVERTEXCOLORMASK_Alpha:
		ColorChannel = VertexColor.A;
		break;
	default:
		return true;
	}

	if (Settings->VertexColorMaskInvert)
	{
		if (ColorChannel > FMath::RoundToInt(Settings->VertexColorMaskThreshold * 255.f))
		{
			return false;
		}
	}
	else
	{
		if (ColorChannel < FMath::RoundToInt(Settings->VertexColorMaskThreshold * 255.f))
		{
			return false;
		}
	}

	return true;
}


/** Add instances inside the brush to match DesiredInstanceCount */
void FEdModeFoliage::AddInstancesForBrush(UWorld* InWorld, AInstancedFoliageActor* IFA, UFoliageType* Settings, FFoliageMeshInfo& MeshInfo, int32 DesiredInstanceCount, const TArray<int32>& ExistingInstances, float Pressure)
{
	checkf(InWorld == IFA->GetWorld(), TEXT("Warning:World does not match Foliage world"));
	if (DesiredInstanceCount > ExistingInstances.Num())
	{
		int32 ExistingInstanceBuckets[NUM_INSTANCE_BUCKETS];
		FMemory::Memzero(ExistingInstanceBuckets, sizeof(ExistingInstanceBuckets));

		// Cache store mapping between component and weight data
		TMap<ULandscapeComponent*, TArray<uint8> >* LandscapeLayerCache = nullptr;

		FName LandscapeLayerName = Settings->LandscapeLayer;
		if (LandscapeLayerName != NAME_None)
		{
			LandscapeLayerCache = &LandscapeLayerCaches.FindOrAdd(LandscapeLayerName);

			// Find the landscape weights of existing ExistingInstances
			for (int32 Idx : ExistingInstances)
			{
				FFoliageInstance& Instance = MeshInfo.Instances[Idx];
				ULandscapeHeightfieldCollisionComponent* HitLandscapeCollision = Cast<ULandscapeHeightfieldCollisionComponent>(Instance.Base);
				if (HitLandscapeCollision)
				{
					ULandscapeComponent* HitLandscape = HitLandscapeCollision->RenderComponent.Get();
					if (HitLandscape)
					{
						TArray<uint8>* LayerCache = &LandscapeLayerCache->FindOrAdd(HitLandscape);
						// TODO: FName to LayerInfo?
						float HitWeight = HitLandscape->GetLayerWeightAtLocation(Instance.Location, HitLandscape->GetLandscapeInfo()->GetLayerInfoByName(LandscapeLayerName), LayerCache);

						// Add count to bucket.
						ExistingInstanceBuckets[FMath::RoundToInt(HitWeight * (float)(NUM_INSTANCE_BUCKETS - 1))]++;
					}
				}
			}
		}
		else
		{
			// When not tied to a layer, put all the ExistingInstances in the last bucket.
			ExistingInstanceBuckets[NUM_INSTANCE_BUCKETS - 1] = ExistingInstances.Num();
		}

		// We calculate a set of potential ExistingInstances for the brush area.
		TArray<FPotentialInstance> PotentialInstanceBuckets[NUM_INSTANCE_BUCKETS];
		// Reserve space in buckets for a potential instances 
		for (auto& Bucket : PotentialInstanceBuckets)
		{
			Bucket.Reserve(DesiredInstanceCount);
		}

		// Quick lookup of potential instance locations, used for overlapping check.
		TArray<FVector> PotentialInstanceLocations;
		FFoliageInstanceHash PotentialInstanceHash(7);	// use 128x128 cell size, as the brush radius is typically small.
		PotentialInstanceLocations.Empty(DesiredInstanceCount);

		// Radius where we expect to have a single instance, given the density rules
		const float DensityCheckRadius = FMath::Max<float>(FMath::Sqrt((1000.f*1000.f) / (PI * Settings->Density)), Settings->Radius);

		for (int32 DesiredIdx = 0; DesiredIdx < DesiredInstanceCount; DesiredIdx++)
		{
			FVector Start, End;

			GetRandomVectorInBrush(Start, End);

			FHitResult Hit;
			static FName NAME_AddInstancesForBrush = FName(TEXT("AddInstancesForBrush"));
			if (FoliageTrace(InWorld, IFA, Hit, Start, End, NAME_AddInstancesForBrush, true))
			{
				// Check filters
				UPrimitiveComponent* PrimComp = Hit.Component.Get();
				UMaterialInterface* Material = PrimComp ? PrimComp->GetMaterial(0) : nullptr;

				if (!PrimComp ||
					PrimComp->GetOutermost() != InWorld->GetCurrentLevel()->GetOutermost() ||
					(!UISettings.bFilterLandscape && PrimComp->IsA(ULandscapeHeightfieldCollisionComponent::StaticClass())) ||
					(!UISettings.bFilterStaticMesh && PrimComp->IsA(UStaticMeshComponent::StaticClass())) ||
					(!UISettings.bFilterBSP && PrimComp->IsA(UModelComponent::StaticClass())) ||
					(!UISettings.bFilterTranslucent && Material && IsTranslucentBlendMode(Material->GetBlendMode()))
					)
				{
					continue;
				}

				if (!CheckLocationForPotentialInstance(MeshInfo, Settings, DensityCheckRadius, Hit.Location, Hit.Normal, PotentialInstanceLocations, PotentialInstanceHash))
				{
					continue;
				}

				// Check vertex color mask
				if (Settings->VertexColorMask != FOLIAGEVERTEXCOLORMASK_Disabled && Hit.FaceIndex != INDEX_NONE)
				{
					UStaticMeshComponent* HitStaticMeshComponent = Cast<UStaticMeshComponent>(Hit.Component.Get());
					if (HitStaticMeshComponent)
					{
						FColor VertexColor;
						if (GetStaticMeshVertexColorForHit(HitStaticMeshComponent, Hit.FaceIndex, Hit.Location, VertexColor))
						{
							if (!CheckVertexColor(Settings, VertexColor))
							{
								continue;
							}
						}
					}
				}

				// Check landscape layer
				float HitWeight = 1.f;
				if (LandscapeLayerName != NAME_None)
				{
					ULandscapeHeightfieldCollisionComponent* HitLandscapeCollision = Cast<ULandscapeHeightfieldCollisionComponent>(Hit.Component.Get());
					if (HitLandscapeCollision)
					{
						ULandscapeComponent* HitLandscape = HitLandscapeCollision->RenderComponent.Get();
						if (HitLandscape)
						{
							TArray<uint8>* LayerCache = &LandscapeLayerCache->FindOrAdd(HitLandscape);
							// TODO: FName to LayerInfo?
							HitWeight = HitLandscape->GetLayerWeightAtLocation(Hit.Location, HitLandscape->GetLandscapeInfo()->GetLayerInfoByName(LandscapeLayerName), LayerCache);

							// Reject instance randomly in proportion to weight
							if (HitWeight <= FMath::FRand())
							{
								continue;
							}
						}
					}
				}

				const int32 BucketIndex = FMath::RoundToInt(HitWeight * (float)(NUM_INSTANCE_BUCKETS - 1));
				check(BucketIndex < ARRAY_COUNT(PotentialInstanceBuckets));

				PotentialInstanceBuckets[BucketIndex].Add(FPotentialInstance(Hit.Location, Hit.Normal, Hit.Component.Get(), HitWeight));
			}
		}

		for (int32 BucketIdx = 0; BucketIdx < NUM_INSTANCE_BUCKETS; BucketIdx++)
		{
			TArray<FPotentialInstance>& PotentialInstances = PotentialInstanceBuckets[BucketIdx];
			float BucketFraction = (float)(BucketIdx + 1) / (float)NUM_INSTANCE_BUCKETS;

			// We use the number that actually succeeded in placement (due to parameters) as the target
			// for the number that should be in the brush region.
			int32 AdditionalInstances = FMath::Clamp<int32>(FMath::RoundToInt(BucketFraction * (float)(PotentialInstances.Num() - ExistingInstanceBuckets[BucketIdx]) * Pressure), 0, PotentialInstances.Num());
			for (int32 Idx = 0; Idx < AdditionalInstances; Idx++)
			{
				FFoliageInstance Inst;
				if (PotentialInstances[Idx].PlaceInstance(Settings, Inst, InWorld, IFA))
				{
					MeshInfo.AddInstance(IFA, Settings, Inst);
				}
			}
		}
	}
}

/** Remove instances inside the brush to match DesiredInstanceCount */
void FEdModeFoliage::RemoveInstancesForBrush(AInstancedFoliageActor* IFA, FFoliageMeshInfo& MeshInfo, int32 DesiredInstanceCount, TArray<int32>& PotentialInstancesToRemove, float Pressure)
{
	int32 InstancesToRemove = FMath::RoundToInt((float)(PotentialInstancesToRemove.Num() - DesiredInstanceCount) * Pressure);
	int32 InstancesToKeep = PotentialInstancesToRemove.Num() - InstancesToRemove;
	if (InstancesToKeep > 0)
	{
		// Remove InstancesToKeep random PotentialInstancesToRemove from the array to leave those PotentialInstancesToRemove behind, and delete all the rest
		for (int32 i = 0; i < InstancesToKeep; i++)
		{
			PotentialInstancesToRemove.RemoveAtSwap(FMath::Rand() % PotentialInstancesToRemove.Num());
		}
	}

	if (!UISettings.bFilterLandscape || !UISettings.bFilterStaticMesh || !UISettings.bFilterBSP || !UISettings.bFilterTranslucent)
	{
		// Filter PotentialInstancesToRemove
		for (int32 Idx = 0; Idx < PotentialInstancesToRemove.Num(); Idx++)
		{
			UPrimitiveComponent* Base = Cast<UPrimitiveComponent> (MeshInfo.Instances[PotentialInstancesToRemove[Idx]].Base);
			UMaterialInterface* Material = Base ? Base->GetMaterial(0) : nullptr;

			// Check if instance is candidate for removal based on filter settings
			if (Base && (
				(!UISettings.bFilterLandscape && Base->IsA(ULandscapeHeightfieldCollisionComponent::StaticClass())) ||
				(!UISettings.bFilterStaticMesh && Base->IsA(UStaticMeshComponent::StaticClass())) ||
				(!UISettings.bFilterBSP && Base->IsA(UModelComponent::StaticClass())) ||
				(!UISettings.bFilterTranslucent && Material && IsTranslucentBlendMode(Material->GetBlendMode()))
				))
			{
				// Instance should not be removed, so remove it from the removal list.
				PotentialInstancesToRemove.RemoveAtSwap(Idx);
				Idx--;
			}
		}
	}

	// Remove PotentialInstancesToRemove to reduce it to desired count
	if (PotentialInstancesToRemove.Num() > 0)
	{
		MeshInfo.RemoveInstances(IFA, PotentialInstancesToRemove);
	}
}

/** Reapply instance settings to exiting instances */
void FEdModeFoliage::ReapplyInstancesForBrush(UWorld* InWorld, AInstancedFoliageActor* IFA, UFoliageType* Settings, const TArray<int32>& ExistingInstances)
{
	checkf(InWorld == IFA->GetWorld(), TEXT("Warning:World does not match Foliage world"));
	FFoliageMeshInfo* MeshInfo = IFA->FindMesh(Settings);
	check(MeshInfo);

	bool bUpdated = false;

	TArray<int32> UpdatedInstances;
	TSet<int32> InstancesToDelete;

	// Setup cache if we're reapplying landscape layer weights
	FName LandscapeLayerName = Settings->LandscapeLayer;
	TMap<ULandscapeComponent*, TArray<uint8>>* LandscapeLayerCache = nullptr;
	if (Settings->ReapplyLandscapeLayer && LandscapeLayerName != NAME_None)
	{
		LandscapeLayerCache = &LandscapeLayerCaches.FindOrAdd(LandscapeLayerName);
	}

	IFA->Modify();

	for (int32 Idx = 0; Idx < ExistingInstances.Num(); Idx++)
	{
		int32 InstanceIndex = ExistingInstances[Idx];
		FFoliageInstance& Instance = MeshInfo->Instances[InstanceIndex];

		if ((Instance.Flags & FOLIAGE_Readjusted) == 0)
		{
			// record that we've made changes to this instance already, so we don't touch it again.
			Instance.Flags |= FOLIAGE_Readjusted;

			// See if we need to update the location in the instance hash
			bool bReapplyLocation = false;
			FVector OldInstanceLocation = Instance.Location;

			// remove any Z offset first, so the offset is reapplied to any new 
			if (FMath::Abs(Instance.ZOffset) > KINDA_SMALL_NUMBER)
			{
				Instance.Location = Instance.GetInstanceWorldTransform().TransformPosition(FVector(0, 0, -Instance.ZOffset));
				bReapplyLocation = true;
			}


			// Defer normal reapplication 
			bool bReapplyNormal = false;

			// Reapply normal alignment
			if (Settings->ReapplyAlignToNormal)
			{
				if (Settings->AlignToNormal)
				{
					if ((Instance.Flags & FOLIAGE_AlignToNormal) == 0)
					{
						bReapplyNormal = true;
						bUpdated = true;
					}
				}
				else
				{
					if (Instance.Flags & FOLIAGE_AlignToNormal)
					{
						Instance.Rotation = Instance.PreAlignRotation;
						Instance.Flags &= (~FOLIAGE_AlignToNormal);
						bUpdated = true;
					}
				}
			}

			// Reapply random yaw
			if (Settings->ReapplyRandomYaw)
			{
				if (Settings->RandomYaw)
				{
					if (Instance.Flags & FOLIAGE_NoRandomYaw)
					{
						// See if we need to remove any normal alignment first
						if (!bReapplyNormal && (Instance.Flags & FOLIAGE_AlignToNormal))
						{
							Instance.Rotation = Instance.PreAlignRotation;
							bReapplyNormal = true;
						}
						Instance.Rotation.Yaw = FMath::FRand() * 360.f;
						Instance.Flags &= (~FOLIAGE_NoRandomYaw);
						bUpdated = true;
					}
				}
				else
				{
					if ((Instance.Flags & FOLIAGE_NoRandomYaw) == 0)
					{
						// See if we need to remove any normal alignment first
						if (!bReapplyNormal && (Instance.Flags & FOLIAGE_AlignToNormal))
						{
							Instance.Rotation = Instance.PreAlignRotation;
							bReapplyNormal = true;
						}
						Instance.Rotation.Yaw = 0.f;
						Instance.Flags |= FOLIAGE_NoRandomYaw;
						bUpdated = true;
					}
				}
			}

			// Reapply random pitch angle
			if (Settings->ReapplyRandomPitchAngle)
			{
				// See if we need to remove any normal alignment first
				if (!bReapplyNormal && (Instance.Flags & FOLIAGE_AlignToNormal))
				{
					Instance.Rotation = Instance.PreAlignRotation;
					bReapplyNormal = true;
				}

				Instance.Rotation.Pitch = FMath::FRand() * Settings->RandomPitchAngle;
				Instance.Flags |= FOLIAGE_NoRandomYaw;

				bUpdated = true;
			}

			// Reapply scale
			if (Settings->UniformScale)
			{
				if (Settings->ReapplyScaleX)
				{
					float Scale = Settings->ScaleMinX + FMath::FRand() * (Settings->ScaleMaxX - Settings->ScaleMinX);
					Instance.DrawScale3D = FVector(Scale, Scale, Scale);
					bUpdated = true;
				}
			}
			else
			{
				float LockRand;
				// If we're doing axis scale locking, get an existing scale for a locked axis that we're not changing, for use as the locked scale value.
				if (Settings->LockScaleX && !Settings->ReapplyScaleX && (Settings->ScaleMaxX - Settings->ScaleMinX) > KINDA_SMALL_NUMBER)
				{
					LockRand = (Instance.DrawScale3D.X - Settings->ScaleMinX) / (Settings->ScaleMaxX - Settings->ScaleMinX);
				}
				else if (Settings->LockScaleY && !Settings->ReapplyScaleY && (Settings->ScaleMaxY - Settings->ScaleMinY) > KINDA_SMALL_NUMBER)
				{
					LockRand = (Instance.DrawScale3D.Y - Settings->ScaleMinY) / (Settings->ScaleMaxY - Settings->ScaleMinY);
				}
				else if (Settings->LockScaleZ && !Settings->ReapplyScaleZ && (Settings->ScaleMaxZ - Settings->ScaleMinZ) > KINDA_SMALL_NUMBER)
				{
					LockRand = (Instance.DrawScale3D.Z - Settings->ScaleMinZ) / (Settings->ScaleMaxZ - Settings->ScaleMinZ);
				}
				else
				{
					LockRand = FMath::FRand();
				}

				if (Settings->ReapplyScaleX)
				{
					Instance.DrawScale3D.X = Settings->ScaleMinX + (Settings->LockScaleX ? LockRand : FMath::FRand()) * (Settings->ScaleMaxX - Settings->ScaleMinX);
					bUpdated = true;
				}

				if (Settings->ReapplyScaleY)
				{
					Instance.DrawScale3D.Y = Settings->ScaleMinY + (Settings->LockScaleY ? LockRand : FMath::FRand()) * (Settings->ScaleMaxY - Settings->ScaleMinY);
					bUpdated = true;
				}

				if (Settings->ReapplyScaleZ)
				{
					Instance.DrawScale3D.Z = Settings->ScaleMinZ + (Settings->LockScaleZ ? LockRand : FMath::FRand()) * (Settings->ScaleMaxZ - Settings->ScaleMinZ);
					bUpdated = true;
				}
			}

			if (Settings->ReapplyZOffset)
			{
				Instance.ZOffset = Settings->ZOffsetMin + FMath::FRand() * (Settings->ZOffsetMax - Settings->ZOffsetMin);
				bUpdated = true;
			}

			// Find a ground normal for either normal or ground slope check.
			if (bReapplyNormal || Settings->ReapplyGroundSlope || Settings->ReapplyVertexColorMask || (Settings->ReapplyLandscapeLayer && LandscapeLayerName != NAME_None))
			{
				FHitResult Hit;
				static const FName NAME_ReapplyInstancesForBrush = TEXT("ReapplyInstancesForBrush");

				// trace along the mesh's Z axis.
				FVector ZAxis = Instance.Rotation.Quaternion().GetAxisZ();
				FVector Start = Instance.Location + 16.f * ZAxis;
				FVector End = Instance.Location - 16.f * ZAxis;
				if (FoliageTrace(InWorld, IFA, Hit, Start, End, NAME_ReapplyInstancesForBrush, true))
				{
					// Reapply the normal
					if (bReapplyNormal)
					{
						Instance.PreAlignRotation = Instance.Rotation;
						Instance.AlignToNormal(Hit.Normal, Settings->AlignMaxAngle);
					}

					// Cull instances that don't meet the ground slope check.
					if (Settings->ReapplyGroundSlope)
					{
						if ((Settings->GroundSlope > 0.f && Hit.Normal.Z <= FMath::Cos(PI * Settings->GroundSlope / 180.f) - SMALL_NUMBER) ||
							(Settings->GroundSlope < 0.f && Hit.Normal.Z >= FMath::Cos(-PI * Settings->GroundSlope / 180.f) + SMALL_NUMBER))
						{
							InstancesToDelete.Add(InstanceIndex);
							if (bReapplyLocation)
							{
								// restore the location so the hash removal will succeed
								Instance.Location = OldInstanceLocation;
							}
							continue;
						}
					}

					// Cull instances for the landscape layer
					if (Settings->ReapplyLandscapeLayer && LandscapeLayerName != NAME_None)
					{
						float HitWeight = 1.f;
						ULandscapeHeightfieldCollisionComponent* HitLandscapeCollision = Cast<ULandscapeHeightfieldCollisionComponent>(Hit.Component.Get());
						if (HitLandscapeCollision)
						{
							ULandscapeComponent* HitLandscape = HitLandscapeCollision->RenderComponent.Get();
							if (HitLandscape)
							{
								TArray<uint8>* LayerCache = &LandscapeLayerCache->FindOrAdd(HitLandscape);
								// TODO: FName to LayerInfo?
								HitWeight = HitLandscape->GetLayerWeightAtLocation(Hit.Location, HitLandscape->GetLandscapeInfo()->GetLayerInfoByName(LandscapeLayerName), LayerCache);

								// Reject instance randomly in proportion to weight
								if (HitWeight <= FMath::FRand())
								{
									InstancesToDelete.Add(InstanceIndex);
									if (bReapplyLocation)
									{
										// restore the location so the hash removal will succeed
										Instance.Location = OldInstanceLocation;
									}
									continue;
								}
							}
						}
					}

					// Reapply vertex color mask
					if (Settings->ReapplyVertexColorMask)
					{
						if (Settings->VertexColorMask != FOLIAGEVERTEXCOLORMASK_Disabled && Hit.FaceIndex != INDEX_NONE)
						{
							UStaticMeshComponent* HitStaticMeshComponent = Cast<UStaticMeshComponent>(Hit.Component.Get());
							if (HitStaticMeshComponent)
							{
								FColor VertexColor;
								if (GetStaticMeshVertexColorForHit(HitStaticMeshComponent, Hit.FaceIndex, Hit.Location, VertexColor))
								{
									if (!CheckVertexColor(Settings, VertexColor))
									{
										InstancesToDelete.Add(InstanceIndex);
										if (bReapplyLocation)
										{
											// restore the location so the hash removal will succeed
											Instance.Location = OldInstanceLocation;
										}
										continue;
									}
								}
							}
						}
					}
				}
			}

			// Cull instances that don't meet the height range
			if (Settings->ReapplyHeight)
			{
				if (Instance.Location.Z < Settings->HeightMin || Instance.Location.Z > Settings->HeightMax)
				{
					InstancesToDelete.Add(InstanceIndex);
					if (bReapplyLocation)
					{
						// restore the location so the hash removal will succeed
						Instance.Location = OldInstanceLocation;
					}
					continue;
				}
			}

			if (bUpdated && FMath::Abs(Instance.ZOffset) > KINDA_SMALL_NUMBER)
			{
				// Reapply the Z offset in new local space
				Instance.Location = Instance.GetInstanceWorldTransform().TransformPosition(FVector(0, 0, Instance.ZOffset));
				bReapplyLocation = true;
			}

			// Update the hash
			if (bReapplyLocation)
			{
				MeshInfo->InstanceHash->RemoveInstance(OldInstanceLocation, InstanceIndex);
				MeshInfo->InstanceHash->InsertInstance(Instance.Location, InstanceIndex);
			}

			// Cull overlapping based on radius
			if (Settings->ReapplyRadius && Settings->Radius > 0.f)
			{
				if (MeshInfo->CheckForOverlappingInstanceExcluding(InstanceIndex, Settings->Radius, InstancesToDelete))
				{
					InstancesToDelete.Add(InstanceIndex);
					continue;
				}
			}

			// Remove mesh collide with world
			if (Settings->ReapplyCollisionWithWorld)
			{
				FHitResult Hit;
				static const FName NAME_ReapplyInstancesForBrush = TEXT("ReapplyCollisionWithWorld");
				FVector Start = Instance.Location + FVector(0.f, 0.f, 16.f);
				FVector End = Instance.Location - FVector(0.f, 0.f, 16.f);
				if (FoliageTrace(InWorld, IFA, Hit, Start, End, NAME_ReapplyInstancesForBrush))
				{
					if (!CheckCollisionWithWorld(Settings, Instance, Hit.Normal, Hit.Location, InWorld, IFA))
					{
						InstancesToDelete.Add(InstanceIndex);
						continue;
					}
				}
				else
				{
					InstancesToDelete.Add(InstanceIndex);
				}
			}

			if (bUpdated)
			{
				UpdatedInstances.Add(InstanceIndex);
			}
		}
	}

	if (UpdatedInstances.Num() > 0)
	{
		MeshInfo->PostUpdateInstances(IFA, UpdatedInstances);
		IFA->RegisterAllComponents();
	}

	if (InstancesToDelete.Num())
	{
		MeshInfo->RemoveInstances(IFA, InstancesToDelete.Array());
	}
}

void FEdModeFoliage::ApplyBrush(FEditorViewportClient* ViewportClient)
{
	if (!bBrushTraceValid)
	{
		return;
	}

	AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());

	float BrushArea = PI * FMath::Square(UISettings.GetRadius());

	// Tablet pressure
	float Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.f;

	// Cache a copy of the world pointer
	UWorld* World = ViewportClient->GetWorld();

	for (auto& MeshPair : IFA->FoliageMeshes)
	{
		FFoliageMeshInfo& MeshInfo = *MeshPair.Value;
		UFoliageType* Settings = MeshPair.Key;

		if (Settings->IsSelected)
		{
			// Find the instances already in the area.
			TArray<int32> Instances;
			FSphere BrushSphere(BrushLocation, UISettings.GetRadius());
			MeshInfo.GetInstancesInsideSphere(BrushSphere, Instances);

			if (UISettings.GetLassoSelectToolSelected())
			{
				// Shift unpaints
				MeshInfo.SelectInstances(IFA, !IsShiftDown(ViewportClient->Viewport), Instances);
			}
			else
			if (UISettings.GetReapplyToolSelected())
			{
				if (Settings->ReapplyDensity)
				{
					// Adjust instance density
					FMeshInfoSnapshot* SnapShot = InstanceSnapshot.Find(MeshPair.Key);
					if (SnapShot)
					{
						// Use snapshot to determine number of instances at the start of the brush stroke
						int32 NewInstanceCount = FMath::RoundToInt((float)SnapShot->CountInstancesInsideSphere(BrushSphere) * Settings->ReapplyDensityAmount);
						if (Settings->ReapplyDensityAmount > 1.f && NewInstanceCount > Instances.Num())
						{
							AddInstancesForBrush(World, IFA, Settings, MeshInfo, NewInstanceCount, Instances, Pressure);
						}
						else if (Settings->ReapplyDensityAmount < 1.f && NewInstanceCount < Instances.Num())
						{
							RemoveInstancesForBrush(IFA, MeshInfo, NewInstanceCount, Instances, Pressure);
						}
					}
				}

				// Reapply any settings checked by the user
				ReapplyInstancesForBrush(World, IFA, Settings, Instances);
			}
			else if (UISettings.GetPaintToolSelected())
			{
				// Shift unpaints
				if (IsShiftDown(ViewportClient->Viewport))
				{
					int32 DesiredInstanceCount = FMath::RoundToInt(BrushArea * Settings->Density * UISettings.GetUnpaintDensity() / (1000.f*1000.f));

					if (DesiredInstanceCount < Instances.Num())
					{
						RemoveInstancesForBrush(IFA, MeshInfo, DesiredInstanceCount, Instances, Pressure);
					}
				}
				else
				{
					// This is the total set of instances disregarding parameters like slope, height or layer.
					float DesiredInstanceCountFloat = BrushArea * Settings->Density * UISettings.GetPaintDensity() / (1000.f*1000.f);

					// Allow a single instance with a random chance, if the brush is smaller than the density
					int32 DesiredInstanceCount = DesiredInstanceCountFloat > 1.f ? FMath::RoundToInt(DesiredInstanceCountFloat) : FMath::FRand() < DesiredInstanceCountFloat ? 1 : 0;

					AddInstancesForBrush(World, IFA, Settings, MeshInfo, DesiredInstanceCount, Instances, Pressure);
				}
			}
		}
	}
	if (UISettings.GetLassoSelectToolSelected())
	{
		IFA->CheckSelection();
		Owner->PivotLocation = Owner->SnappedLocation = IFA->GetSelectionLocation();
		IFA->MarkComponentsRenderStateDirty();
	}
}

struct FFoliagePaintBucketTriangle
{
	FFoliagePaintBucketTriangle(const FTransform& InLocalToWorld, const FVector& InVertex0, const FVector& InVertex1, const FVector& InVertex2, const FColor InColor0, const FColor InColor1, const FColor InColor2)
	{
		Vertex = InLocalToWorld.TransformPosition(InVertex0);
		Vector1 = InLocalToWorld.TransformPosition(InVertex1) - Vertex;
		Vector2 = InLocalToWorld.TransformPosition(InVertex2) - Vertex;
		VertexColor[0] = InColor0;
		VertexColor[1] = InColor1;
		VertexColor[2] = InColor2;

		WorldNormal = InLocalToWorld.GetDeterminant() >= 0.f ? Vector2 ^ Vector1 : Vector1 ^ Vector2;
		float WorldNormalSize = WorldNormal.Size();
		Area = WorldNormalSize * 0.5f;
		if (WorldNormalSize > SMALL_NUMBER)
		{
			WorldNormal /= WorldNormalSize;
		}
	}

	void GetRandomPoint(FVector& OutPoint, FColor& OutBaryVertexColor)
	{
		// Sample parallelogram
		float x = FMath::FRand();
		float y = FMath::FRand();

		// Flip if we're outside the triangle
		if (x + y > 1.f)
		{
			x = 1.f - x;
			y = 1.f - y;
		}

		OutBaryVertexColor = (1.f - x - y) * VertexColor[0] + x * VertexColor[1] + y * VertexColor[2];
		OutPoint = Vertex + x * Vector1 + y * Vector2;
	}

	FVector	Vertex;
	FVector Vector1;
	FVector Vector2;
	FVector WorldNormal;
	float Area;
	FColor VertexColor[3];
};

/** Apply paint bucket to actor */
void FEdModeFoliage::ApplyPaintBucket(AActor* Actor, bool bRemove)
{
	// Apply only to current level
	if (!Actor->GetLevel()->IsCurrentLevel())
	{
		return;
	}

	if (bRemove)
	{
		// Remove all instances of the selected meshes
		AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());

		TArray<UActorComponent*> Components;
		Actor->GetComponents(Components);

		for (int32 ComponentIdx = 0; ComponentIdx < Components.Num(); ComponentIdx++)
		{
			for (auto& MeshPair : IFA->FoliageMeshes)
			{
				FFoliageMeshInfo& MeshInfo = *MeshPair.Value;
				UFoliageType* Settings = MeshPair.Key;

				if (Settings->IsSelected)
				{
					FFoliageComponentHashInfo* ComponentHashInfo = MeshInfo.ComponentHash.Find(Components[ComponentIdx]);
					if (ComponentHashInfo)
					{
						TArray<int32> InstancesToRemove = ComponentHashInfo->Instances.Array();
						MeshInfo.RemoveInstances(IFA, InstancesToRemove);
					}
				}
			}
		}
	}
	else
	{
		TMap<UPrimitiveComponent*, TArray<FFoliagePaintBucketTriangle> > ComponentPotentialTriangles;

		// Check all the components of the hit actor
		TArray<UStaticMeshComponent*> StaticMeshComponents;
		Actor->GetComponents(StaticMeshComponents);

		for (int32 ComponentIdx = 0; ComponentIdx < StaticMeshComponents.Num(); ComponentIdx++)
		{
			UStaticMeshComponent* StaticMeshComponent = StaticMeshComponents[ComponentIdx];
			UMaterialInterface* Material = StaticMeshComponent->GetMaterial(0);

			if (UISettings.bFilterStaticMesh && StaticMeshComponent->StaticMesh && StaticMeshComponent->StaticMesh->RenderData &&
				(UISettings.bFilterTranslucent || !Material || !IsTranslucentBlendMode(Material->GetBlendMode())))
			{
				UStaticMesh* StaticMesh = StaticMeshComponent->StaticMesh;
				FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[0];
				TArray<FFoliagePaintBucketTriangle>& PotentialTriangles = ComponentPotentialTriangles.Add(StaticMeshComponent, TArray<FFoliagePaintBucketTriangle>());

				bool bHasInstancedColorData = false;
				const FStaticMeshComponentLODInfo* InstanceMeshLODInfo = nullptr;
				if (StaticMeshComponent->LODData.Num() > 0)
				{
					InstanceMeshLODInfo = StaticMeshComponent->LODData.GetData();
					bHasInstancedColorData = InstanceMeshLODInfo->PaintedVertices.Num() == LODModel.VertexBuffer.GetNumVertices();
				}

				const bool bHasColorData = bHasInstancedColorData || LODModel.ColorVertexBuffer.GetNumVertices();

				// Get the raw triangle data for this static mesh
				FTransform LocalToWorld = StaticMeshComponent->ComponentToWorld;
				FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();
				const FPositionVertexBuffer& PositionVertexBuffer = LODModel.PositionVertexBuffer;
				const FColorVertexBuffer& ColorVertexBuffer = LODModel.ColorVertexBuffer;

				if (USplineMeshComponent* SplineMesh = Cast<USplineMeshComponent>(StaticMeshComponent))
				{
					// Transform spline mesh verts correctly
					FVector Mask = FVector(1, 1, 1);
					USplineMeshComponent::GetAxisValue(Mask, SplineMesh->ForwardAxis) = 0;

					for (int32 Idx = 0; Idx < Indices.Num(); Idx += 3)
					{
						const int32 Index0 = Indices[Idx];
						const int32 Index1 = Indices[Idx + 1];
						const int32 Index2 = Indices[Idx + 2];

						const FVector Vert0 = SplineMesh->CalcSliceTransform(USplineMeshComponent::GetAxisValue(PositionVertexBuffer.VertexPosition(Index0), SplineMesh->ForwardAxis)).TransformPosition(PositionVertexBuffer.VertexPosition(Index0) * Mask);
						const FVector Vert1 = SplineMesh->CalcSliceTransform(USplineMeshComponent::GetAxisValue(PositionVertexBuffer.VertexPosition(Index1), SplineMesh->ForwardAxis)).TransformPosition(PositionVertexBuffer.VertexPosition(Index1) * Mask);
						const FVector Vert2 = SplineMesh->CalcSliceTransform(USplineMeshComponent::GetAxisValue(PositionVertexBuffer.VertexPosition(Index2), SplineMesh->ForwardAxis)).TransformPosition(PositionVertexBuffer.VertexPosition(Index2) * Mask);

						new(PotentialTriangles)FFoliagePaintBucketTriangle(LocalToWorld
							, Vert0
							, Vert1
							, Vert2
							, bHasInstancedColorData ? InstanceMeshLODInfo->PaintedVertices[Index0].Color : (bHasColorData ? ColorVertexBuffer.VertexColor(Index0) : FColor::White)
							, bHasInstancedColorData ? InstanceMeshLODInfo->PaintedVertices[Index1].Color : (bHasColorData ? ColorVertexBuffer.VertexColor(Index1) : FColor::White)
							, bHasInstancedColorData ? InstanceMeshLODInfo->PaintedVertices[Index2].Color : (bHasColorData ? ColorVertexBuffer.VertexColor(Index2) : FColor::White)
							);
					}
				}
				else
				{
					// Build a mapping of vertex positions to vertex colors.  Using a TMap will allow for fast lookups so we can match new static mesh vertices with existing colors 
					for (int32 Idx = 0; Idx < Indices.Num(); Idx += 3)
					{
						const int32 Index0 = Indices[Idx];
						const int32 Index1 = Indices[Idx + 1];
						const int32 Index2 = Indices[Idx + 2];

						new(PotentialTriangles)FFoliagePaintBucketTriangle(LocalToWorld
							, PositionVertexBuffer.VertexPosition(Index0)
							, PositionVertexBuffer.VertexPosition(Index1)
							, PositionVertexBuffer.VertexPosition(Index2)
							, bHasInstancedColorData ? InstanceMeshLODInfo->PaintedVertices[Index0].Color : (bHasColorData ? ColorVertexBuffer.VertexColor(Index0) : FColor::White)
							, bHasInstancedColorData ? InstanceMeshLODInfo->PaintedVertices[Index1].Color : (bHasColorData ? ColorVertexBuffer.VertexColor(Index1) : FColor::White)
							, bHasInstancedColorData ? InstanceMeshLODInfo->PaintedVertices[Index2].Color : (bHasColorData ? ColorVertexBuffer.VertexColor(Index2) : FColor::White)
							);
					}
				}
			}
		}

		// Place foliage
		AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());

		for (auto& MeshPair : IFA->FoliageMeshes)
		{
			FFoliageMeshInfo& MeshInfo = *MeshPair.Value;
			UFoliageType* Settings = MeshPair.Key;

			if (Settings->IsSelected)
			{
				// Quick lookup of potential instance locations, used for overlapping check.
				TArray<FVector> PotentialInstanceLocations;
				FFoliageInstanceHash PotentialInstanceHash(7);	// use 128x128 cell size, as the brush radius is typically small.
				TArray<FPotentialInstance> InstancesToPlace;

				// Radius where we expect to have a single instance, given the density rules
				const float DensityCheckRadius = FMath::Max<float>(FMath::Sqrt((1000.f*1000.f) / (PI * Settings->Density * UISettings.GetPaintDensity())), Settings->Radius);

				for (TMap<UPrimitiveComponent*, TArray<FFoliagePaintBucketTriangle> >::TIterator ComponentIt(ComponentPotentialTriangles); ComponentIt; ++ComponentIt)
				{
					UPrimitiveComponent* Component = ComponentIt.Key();
					TArray<FFoliagePaintBucketTriangle>& PotentialTriangles = ComponentIt.Value();

					for (int32 TriIdx = 0; TriIdx<PotentialTriangles.Num(); TriIdx++)
					{
						FFoliagePaintBucketTriangle& Triangle = PotentialTriangles[TriIdx];

						// Check if we can reject this triangle based on normal.
						if ((Settings->GroundSlope > 0.f && Triangle.WorldNormal.Z <= FMath::Cos(PI * Settings->GroundSlope / 180.f) - SMALL_NUMBER) ||
							(Settings->GroundSlope < 0.f && Triangle.WorldNormal.Z >= FMath::Cos(-PI * Settings->GroundSlope / 180.f) + SMALL_NUMBER))
						{
							continue;
						}

						// This is the total set of instances disregarding parameters like slope, height or layer.
						float DesiredInstanceCountFloat = Triangle.Area * Settings->Density * UISettings.GetPaintDensity() / (1000.f*1000.f);

						// Allow a single instance with a random chance, if the brush is smaller than the density
						int32 DesiredInstanceCount = DesiredInstanceCountFloat > 1.f ? FMath::RoundToInt(DesiredInstanceCountFloat) : FMath::FRand() < DesiredInstanceCountFloat ? 1 : 0;

						for (int32 Idx = 0; Idx < DesiredInstanceCount; Idx++)
						{
							FVector InstLocation;
							FColor VertexColor;
							Triangle.GetRandomPoint(InstLocation, VertexColor);

							// Check color mask and filters at this location
							if (!CheckVertexColor(Settings, VertexColor) ||
								!CheckLocationForPotentialInstance(MeshInfo, Settings, DensityCheckRadius, InstLocation, Triangle.WorldNormal, PotentialInstanceLocations, PotentialInstanceHash))
							{
								continue;
							}

							new(InstancesToPlace)FPotentialInstance(InstLocation, Triangle.WorldNormal, Component, 1.f);
						}
					}
				}

				// We need a world pointer
				UWorld* World = Actor->GetWorld();
				check(World);
				// Place instances
				for (int32 Idx = 0; Idx < InstancesToPlace.Num(); Idx++)
				{
					FFoliageInstance Inst;
					if (InstancesToPlace[Idx].PlaceInstance(Settings, Inst, World, IFA))
					{
						MeshInfo.AddInstance(IFA, Settings, Inst);
					}
				}
			}
		}
	}
}

bool FEdModeFoliage::GetStaticMeshVertexColorForHit(UStaticMeshComponent* InStaticMeshComponent, int32 InTriangleIndex, const FVector& InHitLocation, FColor& OutVertexColor) const
{
	UStaticMesh* StaticMesh = InStaticMeshComponent->StaticMesh;

	if (StaticMesh == nullptr || StaticMesh->RenderData == nullptr)
	{
		return false;
	}

	FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[0];
	bool bHasInstancedColorData = false;
	const FStaticMeshComponentLODInfo* InstanceMeshLODInfo = nullptr;
	if (InStaticMeshComponent->LODData.Num() > 0)
	{
		InstanceMeshLODInfo = InStaticMeshComponent->LODData.GetData();
		bHasInstancedColorData = InstanceMeshLODInfo->PaintedVertices.Num() == LODModel.VertexBuffer.GetNumVertices();
	}

	const FColorVertexBuffer& ColorVertexBuffer = LODModel.ColorVertexBuffer;

	// no vertex color data
	if (!bHasInstancedColorData && ColorVertexBuffer.GetNumVertices() == 0)
	{
		return false;
	}

	// Get the raw triangle data for this static mesh
	FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();
	const FPositionVertexBuffer& PositionVertexBuffer = LODModel.PositionVertexBuffer;

	int32 SectionFirstTriIndex = 0;
	for (TArray<FStaticMeshSection>::TConstIterator SectionIt(LODModel.Sections); SectionIt; ++SectionIt)
	{
		const FStaticMeshSection& Section = *SectionIt;

		if (Section.bEnableCollision)
		{
			int32 NextSectionTriIndex = SectionFirstTriIndex + Section.NumTriangles;
			if (InTriangleIndex >= SectionFirstTriIndex && InTriangleIndex < NextSectionTriIndex)
			{

				int32 IndexBufferIdx = (InTriangleIndex - SectionFirstTriIndex) * 3 + Section.FirstIndex;

				// Look up the triangle vertex indices
				int32 Index0 = Indices[IndexBufferIdx];
				int32 Index1 = Indices[IndexBufferIdx + 1];
				int32 Index2 = Indices[IndexBufferIdx + 2];

				// Lookup the triangle world positions and colors.
				FVector WorldVert0 = InStaticMeshComponent->ComponentToWorld.TransformPosition(PositionVertexBuffer.VertexPosition(Index0));
				FVector WorldVert1 = InStaticMeshComponent->ComponentToWorld.TransformPosition(PositionVertexBuffer.VertexPosition(Index1));
				FVector WorldVert2 = InStaticMeshComponent->ComponentToWorld.TransformPosition(PositionVertexBuffer.VertexPosition(Index2));

				FLinearColor Color0;
				FLinearColor Color1;
				FLinearColor Color2;

				if (bHasInstancedColorData)
				{
					Color0 = InstanceMeshLODInfo->PaintedVertices[Index0].Color.ReinterpretAsLinear();
					Color1 = InstanceMeshLODInfo->PaintedVertices[Index1].Color.ReinterpretAsLinear();
					Color2 = InstanceMeshLODInfo->PaintedVertices[Index2].Color.ReinterpretAsLinear();
				}
				else
				{
					Color0 = ColorVertexBuffer.VertexColor(Index0).ReinterpretAsLinear();
					Color1 = ColorVertexBuffer.VertexColor(Index1).ReinterpretAsLinear();
					Color2 = ColorVertexBuffer.VertexColor(Index2).ReinterpretAsLinear();
				}

				// find the barycentric coordiantes of the hit location, so we can interpolate the vertex colors
				FVector BaryCoords = FMath::GetBaryCentric2D(InHitLocation, WorldVert0, WorldVert1, WorldVert2);

				FLinearColor InterpColor = BaryCoords.X * Color0 + BaryCoords.Y * Color1 + BaryCoords.Z * Color2;

				// convert back to FColor.
				OutVertexColor = InterpColor.ToFColor(false);

				return true;
			}

			SectionFirstTriIndex = NextSectionTriIndex;
		}
	}

	return false;
}



/** Get list of meshs for current level for UI */

TArray<struct FFoliageMeshUIInfo>& FEdModeFoliage::GetFoliageMeshList()
{
	UpdateFoliageMeshList();
	return FoliageMeshList;
}

void FEdModeFoliage::UpdateFoliageMeshList()
{
	FoliageMeshList.Empty();

	AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());

	for (auto& MeshPair : IFA->FoliageMeshes)
	{
		new(FoliageMeshList)FFoliageMeshUIInfo(MeshPair.Key, &*MeshPair.Value);
	}

	auto CompareDisplayOrder = [](const FFoliageMeshUIInfo& A, const FFoliageMeshUIInfo& B)
	{
		return A.Settings->DisplayOrder < B.Settings->DisplayOrder;
	};
	FoliageMeshList.Sort(CompareDisplayOrder);
}

/** Add a new mesh (if it doesn't already exist) */
void FEdModeFoliage::AddFoliageMesh(UStaticMesh* Mesh)
{
	AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());

	if (IFA->GetSettingsForMesh(Mesh) == nullptr)
	{
		IFA->AddMesh(Mesh);

		// Update mesh list.
		UpdateFoliageMeshList();
	}
}

void FEdModeFoliage::AddFoliageMesh(UFoliageType* Settings)
{
	AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());
	if (IFA->FindMesh(Settings) == nullptr)
	{
		IFA->AddMesh(Settings);

		// Update mesh list.
		UpdateFoliageMeshList();
	}
}

/** Remove a mesh */
bool FEdModeFoliage::RemoveFoliageMesh(UFoliageType* Settings)
{
	bool bMeshRemoved = false;

	AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());
	FFoliageMeshInfo* MeshInfo = IFA->FindMesh(Settings);
	if (MeshInfo != nullptr)
	{
		int32 InstancesNum = MeshInfo->Instances.Num();

		bool bProceed = true;
		if (Settings->GetStaticMesh() != nullptr && InstancesNum > 0)
		{
			FText Message = FText::Format(NSLOCTEXT("UnrealEd", "FoliageMode_DeleteMesh", "Are you sure you want to remove all {0} instances of {1} from this level?"), FText::AsNumber(InstancesNum), FText::FromName(Settings->GetStaticMesh()->GetFName()));
			bProceed = (FMessageDialog::Open(EAppMsgType::YesNo, Message) == EAppReturnType::Yes);
		}

		if (bProceed)
		{
			GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "FoliageMode_RemoveMeshTransaction", "Foliage Editing: Remove Mesh"));
			IFA->RemoveMesh(Settings);
			GEditor->EndTransaction();

			bMeshRemoved = true;
		}

		// Update mesh list.
		UpdateFoliageMeshList();
	}

	return bMeshRemoved;
}

/** Bake instances to StaticMeshActors */
void FEdModeFoliage::BakeFoliage(UFoliageType* Settings, bool bSelectedOnly)
{
	AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());
	FFoliageMeshInfo* MeshInfo = IFA->FindMesh(Settings);
	if (MeshInfo != nullptr)
	{
		TArray<int32> InstancesToConvert;
		if (bSelectedOnly)
		{
			InstancesToConvert = MeshInfo->SelectedIndices.Array();
		}
		else
		{
			for (int32 InstanceIdx = 0; InstanceIdx < MeshInfo->Instances.Num(); InstanceIdx++)
			{
				InstancesToConvert.Add(InstanceIdx);
			}
		}

		// Convert
		for (int32 Idx = 0; Idx < InstancesToConvert.Num(); Idx++)
		{
			FFoliageInstance& Instance = MeshInfo->Instances[InstancesToConvert[Idx]];
			// We need a world in which to spawn the actor. Use the one from the original instance.
			UWorld* World = IFA->GetWorld();
			check(World != nullptr);
			AStaticMeshActor* SMA = World->SpawnActor<AStaticMeshActor>(Instance.Location, Instance.Rotation);
			SMA->GetStaticMeshComponent()->StaticMesh = Settings->GetStaticMesh();
			SMA->GetRootComponent()->SetRelativeScale3D(Instance.DrawScale3D);
			SMA->MarkComponentsRenderStateDirty();
		}

		// Remove
		MeshInfo->RemoveInstances(IFA, InstancesToConvert);
	}
}

/** Copy the settings object for this static mesh */
UFoliageType* FEdModeFoliage::CopySettingsObject(UFoliageType* Settings)
{
	FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "FoliageMode_DuplicateSettingsObject", "Foliage Editing: Copy Settings Object"));

	AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());
	IFA->Modify();

	TUniqueObj<FFoliageMeshInfo> MeshInfo;
	if (IFA->FoliageMeshes.RemoveAndCopyValue(Settings, MeshInfo))
	{
		Settings = (UFoliageType*)StaticDuplicateObject(Settings, IFA, nullptr, RF_AllFlags & ~(RF_Standalone | RF_Public));
		IFA->FoliageMeshes.Add(Settings, MoveTemp(MeshInfo));
		return Settings;
	}
	else
	{
		Transaction.Cancel();
	}

	return nullptr;
}

/** Replace the settings object for this static mesh with the one specified */
void FEdModeFoliage::ReplaceSettingsObject(UFoliageType* OldSettings, UFoliageType* NewSettings)
{
	FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "FoliageMode_ReplaceSettingsObject", "Foliage Editing: Replace Settings Object"));

	AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());
	IFA->Modify();

	TUniqueObj<FFoliageMeshInfo> MeshInfo;
	if (IFA->FoliageMeshes.RemoveAndCopyValue(OldSettings, MeshInfo))
	{
		IFA->FoliageMeshes.Add(NewSettings, MoveTemp(MeshInfo));
	}
	else
	{
		Transaction.Cancel();
	}
}

/** Save the settings object */
UFoliageType* FEdModeFoliage::SaveSettingsObject(const FText& InSettingsPackageName, UFoliageType* Settings)
{
	FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "FoliageMode_SaveSettingsObject", "Foliage Editing: Save Settings Object"));

	AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());
	IFA->Modify();

	FString PackageName = InSettingsPackageName.ToString();
	UPackage* Package = CreatePackage(nullptr, *PackageName);
	check(Package);

	UFoliageType* NewSettings = Cast<UFoliageType>(StaticDuplicateObject(Settings, Package, *FPackageName::GetLongPackageAssetName(PackageName)));
	NewSettings->SetFlags(RF_Standalone | RF_Public);
	NewSettings->Modify();

	ReplaceSettingsObject(Settings, NewSettings);

	// Notify the asset registry
	FAssetRegistryModule::AssetCreated(NewSettings);

	return NewSettings;
}

/** Reapply cluster settings to all the instances */
void FEdModeFoliage::ReallocateClusters(UFoliageType* Settings)
{
	AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());
	FFoliageMeshInfo* MeshInfo = IFA->FindMesh(Settings);
	if (MeshInfo != nullptr)
	{
		MeshInfo->ReallocateClusters(IFA, Settings);
	}
}

/** Replace a mesh with another one */
bool FEdModeFoliage::ReplaceStaticMesh(UFoliageType* OldSettings, UStaticMesh* NewStaticMesh, bool& bOutMeshMerged)
{
	bOutMeshMerged = false;

	AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());
	FFoliageMeshInfo* OldMeshInfo = IFA->FindMesh(OldSettings);

	if (OldMeshInfo != nullptr && OldSettings->GetStaticMesh() != NewStaticMesh)
	{
		int32 InstancesNum = OldMeshInfo->Instances.Num();

		// Look for the new mesh in the mesh list, and either create a new mesh or merge the instances.
		UFoliageType* NewSettings = nullptr;
		FFoliageMeshInfo* NewMeshInfo = nullptr;
		NewSettings = IFA->GetSettingsForMesh(NewStaticMesh, &NewMeshInfo);

		if (NewMeshInfo == nullptr)
		{
			FText Message = FText::Format(NSLOCTEXT("UnrealEd", "FoliageMode_ReplaceMesh", "Are you sure you want to replace all {0} instances of {1} with {2}?"), FText::AsNumber(InstancesNum), FText::FromString(OldSettings->GetStaticMesh()->GetName()), FText::FromString(NewStaticMesh->GetName()));

			if (InstancesNum > 0 && EAppReturnType::Yes != FMessageDialog::Open(EAppMsgType::YesNo, Message))
			{
				return false;
			}

			GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "FoliageMode_ChangeStaticMeshTransaction", "Foliage Editing: Change StaticMesh"));
			IFA->Modify();
			NewMeshInfo = IFA->AddMesh(NewStaticMesh, &NewSettings);
			NewSettings->DisplayOrder = OldSettings->DisplayOrder;
			NewSettings->ShowNothing = OldSettings->ShowNothing;
			NewSettings->ShowPaintSettings = OldSettings->ShowPaintSettings;
			NewSettings->ShowInstanceSettings = OldSettings->ShowInstanceSettings;
		}
		else if (InstancesNum > 0 &&
			EAppReturnType::Yes != FMessageDialog::Open(EAppMsgType::YesNo,
			FText::Format(NSLOCTEXT("UnrealEd", "FoliageMode_ReplaceMeshMerge", "Are you sure you want to merge all {0} instances of {1} into {2}?"),
			FText::AsNumber(InstancesNum), FText::FromString(OldSettings->GetStaticMesh()->GetName()), FText::FromString(NewStaticMesh->GetName()))))
		{
			return false;
		}
		else
		{
			bOutMeshMerged = true;

			GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "FoliageMode_ChangeStaticMeshTransaction", "Foliage Editing: Change StaticMesh"));
			IFA->Modify();
		}

		if (InstancesNum > 0)
		{
			// copy instances from old to new.
			for (FFoliageInstance& Instance : OldMeshInfo->Instances)
			{
					NewMeshInfo->AddInstance(IFA, NewSettings, Instance);
				}
			}

		// Remove the old mesh.
		IFA->RemoveMesh(OldSettings);

		GEditor->EndTransaction();

		// Update mesh list.
		UpdateFoliageMeshList();
	}
	return true;
}


/** FEdMode: Called when a key is pressed */
bool FEdModeFoliage::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (UISettings.GetPaintToolSelected() || UISettings.GetReapplyToolSelected() || UISettings.GetLassoSelectToolSelected())
	{
		// Require Ctrl or not as per user preference
		ELandscapeFoliageEditorControlType FoliageEditorControlType = GetDefault<ULevelEditorViewportSettings>()->FoliageEditorControlType;

		if (Key == EKeys::LeftMouseButton && Event == IE_Pressed)
		{
			// Only activate tool if we're not already moving the camera and we're not trying to drag a transform widget
			// Not using "if (!ViewportClient->IsMovingCamera())" because it's wrong in ortho viewports :D
			bool bMovingCamera = Viewport->KeyState(EKeys::MiddleMouseButton) || Viewport->KeyState(EKeys::RightMouseButton) || IsAltDown(Viewport);

			if ((Viewport->IsPenActive() && Viewport->GetTabletPressure() > 0.f) ||
				(!bMovingCamera && ViewportClient->GetCurrentWidgetAxis() == EAxisList::None &&
					(FoliageEditorControlType == ELandscapeFoliageEditorControlType::IgnoreCtrl ||
					 (FoliageEditorControlType == ELandscapeFoliageEditorControlType::RequireCtrl   && IsCtrlDown(Viewport)) ||
					 (FoliageEditorControlType == ELandscapeFoliageEditorControlType::RequireNoCtrl && !IsCtrlDown(Viewport)))))
			{
				if (!bToolActive)
				{
					GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "FoliageMode_EditTransaction", "Foliage Editing"));
					InstanceSnapshot.Empty();

					// Special setup beginning a stroke with the Reapply tool
					// Necessary so we don't keep reapplying settings over and over for the same instances.
					if (UISettings.GetReapplyToolSelected())
					{
						AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());
						for (auto& MeshPair : IFA->FoliageMeshes)
						{
							FFoliageMeshInfo& MeshInfo = *MeshPair.Value;
							if (MeshPair.Key->IsSelected)
							{
								// Take a snapshot of all the locations
								InstanceSnapshot.Add(MeshPair.Key, FMeshInfoSnapshot(&MeshInfo));

								// Clear the "FOLIAGE_Readjusted" flag
								for (int32 Idx = 0; Idx < MeshInfo.Instances.Num(); Idx++)
								{
									MeshInfo.Instances[Idx].Flags &= (~FOLIAGE_Readjusted);
								}
							}
						}
					}
					ApplyBrush(ViewportClient);
					bToolActive = true;
					return true;
				}
			}
		}

		if (bToolActive && Event == IE_Released &&
			(Key == EKeys::LeftMouseButton || (FoliageEditorControlType == ELandscapeFoliageEditorControlType::RequireCtrl && (Key == EKeys::LeftControl || Key == EKeys::RightControl))))
		{
			//Set the cursor position to that of the slate cursor so it wont snap back
			Viewport->SetPreCaptureMousePosFromSlateCursor();
			GEditor->EndTransaction();
			InstanceSnapshot.Empty();
			LandscapeLayerCaches.Empty();
			bToolActive = false;
			return true;
		}
	}

	if (UISettings.GetSelectToolSelected() || UISettings.GetLassoSelectToolSelected())
	{
		if (Event == IE_Pressed)
		{
			if (Key == EKeys::Platform_Delete)
			{
				GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "FoliageMode_EditTransaction", "Foliage Editing"));
				AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());
				IFA->Modify();
				for (auto& MeshPair : IFA->FoliageMeshes)
				{
					FFoliageMeshInfo& Mesh = *MeshPair.Value;
					if (Mesh.SelectedIndices.Num() > 0)
					{
						TArray<int32> InstancesToDelete = Mesh.SelectedIndices.Array();
						Mesh.RemoveInstances(IFA, InstancesToDelete);
					}
				}
				GEditor->EndTransaction();

				return true;
			}
			else if (Key == EKeys::End)
			{
				// Snap instances to ground
				AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());
				IFA->Modify();
				bool bMovedInstance = false;
				for (auto& MeshPair : IFA->FoliageMeshes)
				{
					FFoliageMeshInfo& Mesh = *MeshPair.Value;

					TArray<int32> SelectedIndices = Mesh.SelectedIndices.Array();

					Mesh.PreMoveInstances(IFA, SelectedIndices);

					UWorld* World = ViewportClient->GetWorld();
					for (int32 SelectedInstanceIdx : SelectedIndices)
					{
						FFoliageInstance& Instance = Mesh.Instances[SelectedInstanceIdx];

						FVector Start = Instance.Location;
						FVector End = Instance.Location - FVector(0.f, 0.f, FOLIAGE_SNAP_TRACE);

						FHitResult Hit;
						if (FoliageTrace(GetWorld(), IFA, Hit, Start, End, FName("FoliageSnap")))
						{
							// Check current level
							if ((Hit.Component.IsValid() && Hit.Component.Get()->GetOutermost() == World->GetCurrentLevel()->GetOutermost()) ||
								(Hit.GetActor() && Hit.GetActor()->IsA(AWorldSettings::StaticClass())))
							{
								Instance.Location = Hit.Location;
								Instance.ZOffset = 0.f;
								Instance.Base = Hit.Component.Get();
								// We cannot be based on an a blueprint component as these will disappear when the construction script is re-run
								if (Instance.Base && Instance.Base->bCreatedByConstructionScript)
								{
									Instance.Base = nullptr;
								}

								if (Instance.Flags & FOLIAGE_AlignToNormal)
								{
									// Remove previous alignment and align to new normal.
									Instance.Rotation = Instance.PreAlignRotation;
									Instance.AlignToNormal(Hit.Normal, MeshPair.Key->AlignMaxAngle);
								}
							}
						}
						bMovedInstance = true;
					}

					Mesh.PostMoveInstances(IFA, SelectedIndices);
				}

				if (bMovedInstance)
				{
					Owner->PivotLocation = Owner->SnappedLocation = IFA->GetSelectionLocation();
					IFA->MarkComponentsRenderStateDirty();
				}

				return true;
			}
		}
	}

	return false;
}

/** FEdMode: Render the foliage edit mode */
void FEdModeFoliage::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	/** Call parent implementation */
	FEdMode::Render(View, Viewport, PDI);
}


/** FEdMode: Render HUD elements for this tool */
void FEdModeFoliage::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
}

/** FEdMode: Check to see if an actor can be selected in this mode - no side effects */
bool FEdModeFoliage::IsSelectionAllowed(AActor* InActor, bool bInSelection) const
{
	return false;
}

/** FEdMode: Handling SelectActor */
bool FEdModeFoliage::Select(AActor* InActor, bool bInSelected)
{
	// return true if you filter that selection
	// however - return false if we are trying to deselect so that it will infact do the deselection
	if (bInSelected == false)
	{
		return false;
	}
	return true;
}

/** FEdMode: Called when the currently selected actor has changed */
void FEdModeFoliage::ActorSelectionChangeNotify()
{
}


/** Forces real-time perspective viewports */
void FEdModeFoliage::ForceRealTimeViewports(const bool bEnable, const bool bStoreCurrentState)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr< ILevelViewport > ViewportWindow = LevelEditorModule.GetFirstActiveViewport();
	if (ViewportWindow.IsValid())
	{
		FEditorViewportClient &Viewport = ViewportWindow->GetLevelViewportClient();
		if (Viewport.IsPerspective())
		{
			if (bEnable)
			{
				Viewport.SetRealtime(bEnable, bStoreCurrentState);
			}
			else
			{
				const bool bAllowDisable = true;
				Viewport.RestoreRealtime(bAllowDisable);
			}

		}
	}
}

bool FEdModeFoliage::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy *HitProxy, const FViewportClick &Click)
{
	AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());

	if (UISettings.GetSelectToolSelected())
	{
		if (HitProxy && HitProxy->IsA(HInstancedStaticMeshInstance::StaticGetType()))
		{
			HInstancedStaticMeshInstance* SMIProxy = ((HInstancedStaticMeshInstance*)HitProxy);

			IFA->SelectInstance(SMIProxy->Component, SMIProxy->InstanceIndex, Click.IsControlDown());

			// Update pivot
			Owner->PivotLocation = Owner->SnappedLocation = IFA->GetSelectionLocation();
		}
		else
		{
			if (!Click.IsControlDown())
			{
				// Select none if not trying to toggle
				IFA->SelectInstance(nullptr, -1, false);
			}
		}

		return true;
	}
	else if (UISettings.GetPaintBucketToolSelected() || UISettings.GetReapplyPaintBucketToolSelected())
	{
		if (HitProxy && HitProxy->IsA(HActor::StaticGetType()))
		{
			GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "FoliageMode_EditTransaction", "Foliage Editing"));
			ApplyPaintBucket(((HActor*)HitProxy)->Actor, Click.IsShiftDown());
			GEditor->EndTransaction();
		}

		return true;
	}


	return FEdMode::HandleClick(InViewportClient, HitProxy, Click);
}

FVector FEdModeFoliage::GetWidgetLocation() const
{
	return FEdMode::GetWidgetLocation();
}

/** FEdMode: Called when a mouse button is pressed */
bool FEdModeFoliage::StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if (UISettings.GetSelectToolSelected() || UISettings.GetLassoSelectToolSelected())
	{
		// Update pivot
		AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());
		Owner->PivotLocation = Owner->SnappedLocation = IFA->GetSelectionLocation();

		GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "FoliageMode_EditTransaction", "Foliage Editing"));

		bCanAltDrag = true;

		return true;
	}
	return FEdMode::StartTracking(InViewportClient, InViewport);
}

/** FEdMode: Called when the a mouse button is released */
bool FEdModeFoliage::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if (UISettings.GetSelectToolSelected() || UISettings.GetLassoSelectToolSelected())
	{
		GEditor->EndTransaction();
		return true;
	}
	return FEdMode::EndTracking(InViewportClient, InViewport);
}

/** FEdMode: Called when mouse drag input it applied */
bool FEdModeFoliage::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	bool bFoundSelection = false;

	bool bAltDown = InViewport->KeyState(EKeys::LeftAlt) || InViewport->KeyState(EKeys::RightAlt);

	if (InViewportClient->GetCurrentWidgetAxis() != EAxisList::None && (UISettings.GetSelectToolSelected() || UISettings.GetLassoSelectToolSelected()))
	{
		AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());
		IFA->Modify();
		for (auto& MeshPair : IFA->FoliageMeshes)
		{
			FFoliageMeshInfo& MeshInfo = *MeshPair.Value;
			TArray<int32> SelectedIndices = MeshInfo.SelectedIndices.Array();

			bFoundSelection |= SelectedIndices.Num() > 0;

			if (bAltDown && bCanAltDrag && (InViewportClient->GetCurrentWidgetAxis() & EAxisList::XYZ))
			{
				MeshInfo.DuplicateInstances(IFA, MeshPair.Key, SelectedIndices);
			}

			MeshInfo.PreMoveInstances(IFA, SelectedIndices);

			for (int32 SelectedInstanceIdx : SelectedIndices)
			{
				FFoliageInstance& Instance = MeshInfo.Instances[SelectedInstanceIdx];
				Instance.Location += InDrag;
				Instance.ZOffset = 0.f;
				Instance.Rotation += InRot;
				Instance.DrawScale3D += InScale;
			}

			MeshInfo.PostMoveInstances(IFA, SelectedIndices);
		}

		// Only allow alt-drag on first InputDelta
		bCanAltDrag = false;

		if (bFoundSelection)
		{
			IFA->MarkComponentsRenderStateDirty();
			return true;
		}
	}

	return FEdMode::InputDelta(InViewportClient, InViewport, InDrag, InRot, InScale);
}

bool FEdModeFoliage::AllowWidgetMove()
{
	return ShouldDrawWidget();
}

bool FEdModeFoliage::UsesTransformWidget() const
{
	return ShouldDrawWidget();
}

bool FEdModeFoliage::ShouldDrawWidget() const
{
	return (UISettings.GetSelectToolSelected() || (UISettings.GetLassoSelectToolSelected() && !bToolActive)) && AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld())->SelectedMesh != nullptr;
}

EAxisList::Type FEdModeFoliage::GetWidgetAxisToDraw(FWidget::EWidgetMode InWidgetMode) const
{
	switch (InWidgetMode)
	{
	case FWidget::WM_Translate:
	case FWidget::WM_Rotate:
	case FWidget::WM_Scale:
		return EAxisList::XYZ;
	default:
		return EAxisList::None;
	}
}

/** Load UI settings from ini file */
void FFoliageUISettings::Load()
{
	FString WindowPositionString;
	if (GConfig->GetString(TEXT("FoliageEdit"), TEXT("WindowPosition"), WindowPositionString, GEditorUserSettingsIni))
	{
		TArray<FString> PositionValues;
		if (WindowPositionString.ParseIntoArray(&PositionValues, TEXT(","), true) == 4)
		{
			WindowX = FCString::Atoi(*PositionValues[0]);
			WindowY = FCString::Atoi(*PositionValues[1]);
			WindowWidth = FCString::Atoi(*PositionValues[2]);
			WindowHeight = FCString::Atoi(*PositionValues[3]);
		}
	}

	GConfig->GetFloat(TEXT("FoliageEdit"), TEXT("Radius"), Radius, GEditorUserSettingsIni);
	GConfig->GetFloat(TEXT("FoliageEdit"), TEXT("PaintDensity"), PaintDensity, GEditorUserSettingsIni);
	GConfig->GetFloat(TEXT("FoliageEdit"), TEXT("UnpaintDensity"), UnpaintDensity, GEditorUserSettingsIni);
	GConfig->GetBool(TEXT("FoliageEdit"), TEXT("bFilterLandscape"), bFilterLandscape, GEditorUserSettingsIni);
	GConfig->GetBool(TEXT("FoliageEdit"), TEXT("bFilterStaticMesh"), bFilterStaticMesh, GEditorUserSettingsIni);
	GConfig->GetBool(TEXT("FoliageEdit"), TEXT("bFilterBSP"), bFilterBSP, GEditorUserSettingsIni);
	GConfig->GetBool(TEXT("FoliageEdit"), TEXT("bFilterTranslucent"), bFilterTranslucent, GEditorUserSettingsIni);
}

/** Save UI settings to ini file */
void FFoliageUISettings::Save()
{
	FString WindowPositionString = FString::Printf(TEXT("%d,%d,%d,%d"), WindowX, WindowY, WindowWidth, WindowHeight);
	GConfig->SetString(TEXT("FoliageEdit"), TEXT("WindowPosition"), *WindowPositionString, GEditorUserSettingsIni);

	GConfig->SetFloat(TEXT("FoliageEdit"), TEXT("Radius"), Radius, GEditorUserSettingsIni);
	GConfig->SetFloat(TEXT("FoliageEdit"), TEXT("PaintDensity"), PaintDensity, GEditorUserSettingsIni);
	GConfig->SetFloat(TEXT("FoliageEdit"), TEXT("UnpaintDensity"), UnpaintDensity, GEditorUserSettingsIni);
	GConfig->SetBool(TEXT("FoliageEdit"), TEXT("bFilterLandscape"), bFilterLandscape, GEditorUserSettingsIni);
	GConfig->SetBool(TEXT("FoliageEdit"), TEXT("bFilterStaticMesh"), bFilterStaticMesh, GEditorUserSettingsIni);
	GConfig->SetBool(TEXT("FoliageEdit"), TEXT("bFilterBSP"), bFilterBSP, GEditorUserSettingsIni);
	GConfig->SetBool(TEXT("FoliageEdit"), TEXT("bFilterTranslucent"), bFilterTranslucent, GEditorUserSettingsIni);
}

#undef LOCTEXT_NAMESPACE