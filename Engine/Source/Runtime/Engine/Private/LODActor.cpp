// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LODActorBase.cpp: Static mesh actor base class implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "Engine/LODActor.h"
#include "Engine/HLODMeshCullingVolume.h"
#include "MapErrors.h"
#include "MessageLog.h"
#include "UObjectToken.h"

#include "StaticMeshResources.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "LODActor"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static void HLODConsoleCommand(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() == 1)
	{
		const int32 State = FCString::Atoi(*Args[0]);

		if (State == 0 || State == 1)
		{
			const bool bHLODEnabled = (State == 1) ?  true : false;
			FlushRenderingCommands();
			const TArray<ULevel*>& Levels = World->GetLevels();
			for (ULevel* Level : Levels)
			{
				for (AActor* Actor : Level->Actors)
				{
					ALODActor* LODActor = Cast<ALODActor>(Actor);
					if (LODActor)
					{
						LODActor->SetActorHiddenInGame(!bHLODEnabled);
#if WITH_EDITOR
						LODActor->SetIsTemporarilyHiddenInEditor(!bHLODEnabled);
#endif // WITH_EDITOR
						LODActor->MarkComponentsRenderStateDirty();
					}
				}
			}
		}
	}
	else if (Args.Num() == 2)
	{
#if WITH_EDITOR
		if (Args[0] == "force")
		{
			const int32 ForcedLevel = FCString::Atoi(*Args[1]);

			if (ForcedLevel >= -1 && ForcedLevel < World->GetWorldSettings()->HierarchicalLODSetup.Num())
			{
				const TArray<ULevel*>& Levels = World->GetLevels();
				for (ULevel* Level : Levels)
				{
					for (AActor* Actor : Level->Actors)
					{
						ALODActor* LODActor = Cast<ALODActor>(Actor);

						if (LODActor)
						{
							if (LODActor->LODLevel == ForcedLevel + 1)
							{
								LODActor->SetForcedView(true);
							}
							else
							{
								LODActor->SetHiddenFromEditorView(true, ForcedLevel + 1);
							}
						}
					}
				}
			}
		}
#endif // WITH_EDITOR
	}
}

static FAutoConsoleCommandWithWorldAndArgs GHLODCmd(
	TEXT("r.HLOD"),
	TEXT("Single argument: 0 or 1 to Disable/Enable HLOD System\nMultiple arguments: force X where X is the HLOD level that should be forced into view"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(HLODConsoleCommand)
	);

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

ALODActor::ALODActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LODDrawDistance(5000)	
{
	bCanBeDamaged = false;

	// Cast shadows if any sub-actors do
	bool bCastsShadow = false;
	bool bCastsStaticShadow = false;
	bool bCastsDynamicShadow = false;

#if WITH_EDITORONLY_DATA
	
	bListedInSceneOutliner = false;

	// Always dirty when created
	bDirty = true;

	NumTrianglesInSubActors = 0;
	NumTrianglesInMergedMesh = 0;

	if (SubActors.Num() != 0)
	{
		for (auto& Actor : SubActors)
		{
			// Adding number of triangles
			if (!Actor->IsA<ALODActor>())
			{
				TArray<UStaticMeshComponent*> StaticMeshComponents;
				Actor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
				for (UStaticMeshComponent* Component : StaticMeshComponents)
				{
					if (Component && Component->StaticMesh && Component->StaticMesh->RenderData)
					{
						NumTrianglesInSubActors += Component->StaticMesh->RenderData->LODResources[0].GetNumTriangles();

						bCastsShadow |= Component->CastShadow;
						bCastsStaticShadow |= Component->bCastStaticShadow;
						bCastsDynamicShadow |= Component->bCastDynamicShadow;
					}

					Component->MarkRenderStateDirty();
				}
			}
			else
			{
				ALODActor* LODActor = Cast<ALODActor>(Actor);
				NumTrianglesInSubActors += LODActor->GetNumTrianglesInSubActors();

				if (UStaticMeshComponent* ActorMeshComponent = LODActor->StaticMeshComponent)
				{
					bCastsShadow |= ActorMeshComponent->CastShadow;
					bCastsStaticShadow |= ActorMeshComponent->bCastStaticShadow;
					bCastsDynamicShadow |= ActorMeshComponent->bCastDynamicShadow;
				}
			}
		}
	
	}

	if (StaticMeshComponent && StaticMeshComponent->StaticMesh && StaticMeshComponent->StaticMesh->RenderData)
	{
		NumTrianglesInMergedMesh = StaticMeshComponent->StaticMesh->RenderData->LODResources[0].GetNumTriangles();
	}

	
#endif // WITH_EDITORONLY_DATA

	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent0"));
	StaticMeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	StaticMeshComponent->Mobility = EComponentMobility::Static;
	StaticMeshComponent->bGenerateOverlapEvents = false;
	StaticMeshComponent->CastShadow = bCastsShadow;
	StaticMeshComponent->bCastStaticShadow = bCastsStaticShadow;
	StaticMeshComponent->bCastDynamicShadow = bCastsDynamicShadow;
	StaticMeshComponent->bAllowCullDistanceVolume = false;

	RootComponent = StaticMeshComponent;	
}

FString ALODActor::GetDetailedInfoInternal() const
{
	return StaticMeshComponent ? StaticMeshComponent->GetDetailedInfoInternal() : TEXT("No_StaticMeshComponent");
}

void ALODActor::PostRegisterAllComponents() 
{
	Super::PostRegisterAllComponents();
#if WITH_EDITOR
	if (StaticMeshComponent->SceneProxy)
	{
		ensure (LODLevel >= 1);
		(StaticMeshComponent->SceneProxy)->SetHierarchicalLOD_GameThread(LODLevel);
	}

	// Clean up sub actor if assets were delete manually
	CleanSubActorArray();

	// Clean up sub objects if assets were delete manually
	CleanSubObjectsArray();

	UpdateSubActorLODParents();	
#endif
}

#if WITH_EDITOR

void ALODActor::PreEditChange(UProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	if (PropertyThatWillChange)
	{
		const FName PropertyName = PropertyThatWillChange->GetFName();

		// If the Sub Objects array is changed, in case of asset deletion make sure me flag as dirty since the cluster will be invalid
		if (PropertyName == TEXT("SubObjects"))
		{
			SetIsDirty(true);
		}
	}

	// Flush all pending rendering commands.
	FlushRenderingCommands();
}

void ALODActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	FName PropertyName = PropertyThatChanged != NULL ? PropertyThatChanged->GetFName() : NAME_None;
	
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ALODActor, bOverrideTransitionScreenSize) || PropertyName == GET_MEMBER_NAME_CHECKED(ALODActor, TransitionScreenSize))
	{
		float CalculateSreenSize = 0.0f;

		if (bOverrideTransitionScreenSize)
		{
			CalculateSreenSize = TransitionScreenSize;
		}
		else
		{
			UWorld* World = GetWorld();
			check(World != nullptr);
			AWorldSettings* WorldSettings = World->GetWorldSettings();
			checkf(WorldSettings->HierarchicalLODSetup.IsValidIndex(LODLevel - 1), TEXT("Out of range HLOD level (%i) found in LODActor (%s)"), LODLevel - 1, *GetName());
			CalculateSreenSize = WorldSettings->HierarchicalLODSetup[LODLevel - 1].TransitionScreenSize;
		}

		RecalculateDrawingDistance(CalculateSreenSize);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ALODActor, bOverrideScreenSize) || PropertyName == GET_MEMBER_NAME_CHECKED(ALODActor, ScreenSize)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(ALODActor, bOverrideMaterialMergeSettings) || PropertyName == GET_MEMBER_NAME_CHECKED(ALODActor, MaterialSettings))
	{
		// If we change override settings dirty the actor
		SetIsDirty(true);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

bool ALODActor::GetReferencedContentObjects( TArray<UObject*>& Objects ) const
{
	Super::GetReferencedContentObjects(Objects);
	Objects.Append(SubObjects);
	return true;
}

void ALODActor::CheckForErrors()
{
	FMessageLog MapCheck("MapCheck");

	// Only check when this is not a preview actor and actually has a static mesh	
	Super::CheckForErrors();
	if (!StaticMeshComponent)
	{
		MapCheck.Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(LOCTEXT("MapCheck_Message_StaticMeshComponent", "Static mesh actor has NULL StaticMeshComponent property - please delete")))
			->AddToken(FMapErrorToken::Create(FMapErrors::StaticMeshComponent));
	}

	if (StaticMeshComponent && StaticMeshComponent->StaticMesh == NULL)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
		FMessageLog("MapCheck").Error()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("MapCheck_Message_InvalidLODActorMissingMesh", "{ActorName} : Static mesh is missing for the built LODActor.  Did you remove the asset? Please delete it and build LOD again. "), Arguments)))
			->AddToken(FMapErrorToken::Create(FMapErrors::LODActorMissingStaticMesh));
	}
	
	if (SubActors.Num() == 0)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
		FMessageLog("MapCheck").Error()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("MapCheck_Message_InvalidLODActorEmptyActor", "{ActorName} : NoActor is assigned. We recommend you to delete this actor. "), Arguments)))
			->AddToken(FMapErrorToken::Create(FMapErrors::LODActorNoActorFound));
	}
	else
	{
		for(auto& Actor : SubActors)
		{
			// see if it's null, if so it is not good
			if(Actor == nullptr)
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
				FMessageLog("MapCheck").Error()
					->AddToken(FUObjectToken::Create(this))
					->AddToken(FTextToken::Create(FText::Format(LOCTEXT("MapCheck_Message_InvalidLODActorNullActor", "{ActorName} : Actor is missing. The actor might have been removed. We recommend you to build LOD again. "), Arguments)))
					->AddToken(FMapErrorToken::Create(FMapErrors::LODActorMissingActor));
			}
		}
	}
}

void ALODActor::EditorApplyTranslation(const FVector& DeltaTranslation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
}

void ALODActor::EditorApplyRotation(const FRotator& DeltaRotation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
}

void ALODActor::EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
}

void ALODActor::EditorApplyMirror(const FVector& MirrorScale, const FVector& PivotLocation)
{
}

void ALODActor::AddSubActor(AActor* InActor)
{
	SubActors.Add(InActor);
	InActor->SetLODParent(StaticMeshComponent, LODDrawDistance);
	SetIsDirty(true);

	// Cast shadows if any sub-actors do
	bool bCastsShadow = false;
	bool bCastsStaticShadow = false;
	bool bCastsDynamicShadow = false;

	// Adding number of triangles
	if (!InActor->IsA<ALODActor>())
	{
		TArray<UStaticMeshComponent*> StaticMeshComponents;
		InActor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
		for (UStaticMeshComponent* Component : StaticMeshComponents)
		{
			if (Component && Component->StaticMesh && Component->StaticMesh->RenderData)
			{
				NumTrianglesInSubActors += Component->StaticMesh->RenderData->LODResources[0].GetNumTriangles();

				StaticMeshComponent->CastShadow |= Component->CastShadow;
				StaticMeshComponent->bCastStaticShadow |= Component->bCastStaticShadow;
				StaticMeshComponent->bCastDynamicShadow |= Component->bCastDynamicShadow;
			}

			Component->MarkRenderStateDirty();
		}
	}
	else
	{
		ALODActor* LODActor = Cast<ALODActor>(InActor);
		NumTrianglesInSubActors += LODActor->GetNumTrianglesInSubActors();
		
		if (UStaticMeshComponent* ActorMeshComponent = LODActor->StaticMeshComponent)
		{
			StaticMeshComponent->CastShadow |= ActorMeshComponent->CastShadow;
			StaticMeshComponent->bCastStaticShadow |= ActorMeshComponent->bCastStaticShadow;
			StaticMeshComponent->bCastDynamicShadow |= ActorMeshComponent->bCastDynamicShadow;
		}
	}

	StaticMeshComponent->MarkRenderStateDirty();
	
}

const bool ALODActor::RemoveSubActor(AActor* InActor)
{
	if ((InActor != nullptr) && SubActors.Contains(InActor))
	{
		SubActors.Remove(InActor);
		InActor->SetLODParent(nullptr, 0);
		SetIsDirty(true);

		// Deducting number of triangles
		if (!InActor->IsA<ALODActor>())
		{
			TArray<UStaticMeshComponent*> StaticMeshComponents;
			InActor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
			for (UStaticMeshComponent* Component : StaticMeshComponents)
			{
				if (Component && Component->StaticMesh && Component->StaticMesh->RenderData)
				{
					NumTrianglesInSubActors -= Component->StaticMesh->RenderData->LODResources[0].GetNumTriangles();
				}

				Component->MarkRenderStateDirty();
			}
		}
		else
		{
			ALODActor* LODActor = Cast<ALODActor>(InActor);
			NumTrianglesInSubActors -= LODActor->GetNumTrianglesInSubActors();
		}

		if (StaticMeshComponent)
		{
			StaticMeshComponent->MarkRenderStateDirty();
		}	
				
		// In case the user removes an actor while the HLOD system is force viewing one LOD level
		InActor->SetIsTemporarilyHiddenInEditor(false);
				
		return true;
	}

	return false;
}

void ALODActor::SetIsDirty(const bool bNewState)
{
	bDirty = bNewState;

	// Set parent LODActor dirty as well if bNewState = true
	if (IsDirty())
	{
		// If this LODActor is a SubActor at a higher LOD level mark parent dirty as well
		UPrimitiveComponent* ParentComponent = StaticMeshComponent->GetLODParentPrimitive();
		if (ParentComponent)
		{
			ALODActor* LODParentActor = Cast<ALODActor>(ParentComponent->GetOwner());
			if (LODParentActor)
			{
				LODParentActor->Modify();
				LODParentActor->SetIsDirty(true);
			}
		}

		// Set static mesh to null (this so we can revert without destroying the previously build static mesh)
		StaticMeshComponent->StaticMesh = nullptr;
		// Mark render state dirty to update viewport
		StaticMeshComponent->MarkRenderStateDirty();
#if WITH_EDITOR
		// Broadcast actor marked dirty event
		if (GEditor)
		{
			GEditor->BroadcastHLODActorMarkedDirty(this);
		}
#endif // WITH_EDITOR
	}	
	else
	{
		// Update SubActor's LOD parent component
		for (auto& SubActor : SubActors)
		{
			SubActor->SetLODParent(StaticMeshComponent, LODDrawDistance);
		}
	}
}

const bool ALODActor::HasValidSubActors()
{
	return (SubActors.Num() != 0);	
}

void ALODActor::ToggleForceView()
{
	// Toggle the forced viewing of this LODActor, set drawing distance to 0.0f or LODDrawDistance
	StaticMeshComponent->MinDrawDistance = (StaticMeshComponent->MinDrawDistance == 0.0f) ? LODDrawDistance : 0.0f;
	StaticMeshComponent->MarkRenderStateDirty();
}

void ALODActor::SetForcedView(const bool InState)
{
	// Set forced viewing state of this LODActor, set drawing distance to 0.0f or LODDrawDistance
	StaticMeshComponent->MinDrawDistance = (InState) ? 0.0f : LODDrawDistance;
	StaticMeshComponent->MarkRenderStateDirty();
}

void ALODActor::SetHiddenFromEditorView(const bool InState, const int32 ForceLODLevel )
{
	// If we are also subactor for a higher LOD level or this actor belongs to a higher HLOD level than is being forced hide the actor
	if (GetStaticMeshComponent()->GetLODParentPrimitive() || LODLevel > ForceLODLevel )
	{
		SetIsTemporarilyHiddenInEditor(InState);			

		for (auto Actor : SubActors)
		{
			// If this actor belongs to a lower HLOD level that is being forced hide the sub-actors
			if (LODLevel < ForceLODLevel)
			{
				Actor->SetIsTemporarilyHiddenInEditor(InState);
			}

			// Toggle/set the LOD parent to nullptr or this
			Actor->SetLODParent((InState) ? nullptr : StaticMeshComponent, (InState) ? 0.0f : LODDrawDistance);
		}
	}

	StaticMeshComponent->MarkRenderStateDirty();
}

const uint32 ALODActor::GetNumTrianglesInSubActors()
{
	return NumTrianglesInSubActors;
}

const uint32 ALODActor::GetNumTrianglesInMergedMesh()
{
	return NumTrianglesInMergedMesh;
}

void ALODActor::SetStaticMesh(class UStaticMesh* InStaticMesh)
{
	if (StaticMeshComponent)
	{
		StaticMeshComponent->StaticMesh = InStaticMesh;
		SetIsDirty(false);

		if (StaticMeshComponent && StaticMeshComponent->StaticMesh && StaticMeshComponent->StaticMesh->RenderData)
		{
			NumTrianglesInMergedMesh = StaticMeshComponent->StaticMesh->RenderData->LODResources[0].GetNumTriangles();
		}
	}
}

void ALODActor::UpdateSubActorLODParents()
{
	for (auto& Actor : SubActors)
	{	
		Actor->SetLODParent(StaticMeshComponent, LODDrawDistance);	
	}
}

void ALODActor::CleanSubActorArray()
{
	bool bIsDirty = false;
	for (int32 SubActorIndex = 0; SubActorIndex < SubActors.Num(); ++SubActorIndex)
	{
		auto& Actor = SubActors[SubActorIndex];
		if (Actor == nullptr)
		{
			SubActors.RemoveAtSwap(SubActorIndex);
			SubActorIndex--;
			bIsDirty = true;
		}
	}

	if (bIsDirty)
	{
		SetIsDirty(true);
	}
}

void ALODActor::CleanSubObjectsArray()
{
	bool bIsDirty = false;
	for (int32 SubObjectIndex = 0; SubObjectIndex < SubObjects.Num(); ++SubObjectIndex)
	{
		UObject* Object = SubObjects[SubObjectIndex];
		if (Object == nullptr)
		{
			SubObjects.RemoveAtSwap(SubObjectIndex);
			SubObjectIndex--;
			bIsDirty = true;
		}
	}

	if (bIsDirty)
	{
		SetIsDirty(true);		
	}
}

void ALODActor::RecalculateDrawingDistance(const float InTransitionScreenSize)
{
	// At the moment this assumes a fixed field of view of 90 degrees (horizontal and vertical axi)
	static const float FOVRad = 90.0f * (float)PI / 360.0f;
	static const FMatrix ProjectionMatrix = FPerspectiveMatrix(FOVRad, 1920, 1080, 0.01f);
	FBoxSphereBounds Bounds = GetStaticMeshComponent()->CalcBounds(FTransform());

	// Get projection multiple accounting for view scaling.
	const float ScreenMultiple = FMath::Max(1920.0f / 2.0f * ProjectionMatrix.M[0][0],
		1080.0f / 2.0f * ProjectionMatrix.M[1][1]);
	// (ScreenMultiple * SphereRadius) / Sqrt(Screensize * 1920 * 1080.0f * PI) = Distance
	LODDrawDistance = (ScreenMultiple * Bounds.SphereRadius) / FMath::Sqrt((InTransitionScreenSize * 1920.0f * 1080.0f) / PI);

	UpdateSubActorLODParents();
}


#endif // WITH_EDITOR

FBox ALODActor::GetComponentsBoundingBox(bool bNonColliding) const 
{
	FBox BoundBox = Super::GetComponentsBoundingBox(bNonColliding);

	// If BoundBox ends up to nothing create a new invalid one
	if (BoundBox.GetVolume() == 0.0f)
	{
		BoundBox = FBox(0);
	}

	if (bNonColliding)
	{
		if (StaticMeshComponent && StaticMeshComponent->StaticMesh)
		{
			FBoxSphereBounds StaticBound = StaticMeshComponent->StaticMesh->GetBounds();
			FBox StaticBoundBox(BoundBox.GetCenter()-StaticBound.BoxExtent, BoundBox.GetCenter()+StaticBound.BoxExtent);
			BoundBox += StaticBoundBox;
		}
		else
		{
			for (auto Actor : SubActors)
			{
				if (Actor)
				{
					BoundBox += Actor->GetComponentsBoundingBox(bNonColliding);
				}				
			}
		}
	}

	return BoundBox;	
}

//////////////////////////////////////////////////////////////////////////
// AHLODMeshCullingVolume


#undef LOCTEXT_NAMESPACE
