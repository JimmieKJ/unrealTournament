// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HierarchicalLODUtilitiesModulePrivatePCH.h"
#include "HierarchicalLODProxyProcessor.h"
#include "HierarchicalLODUtilities.h"
#include "UnrealEd.h"
#include "Classes/Engine/LODActor.h"

FHierarchicalLODProxyProcessor::FHierarchicalLODProxyProcessor()
{
#if WITH_EDITOR
	FEditorDelegates::MapChange.AddRaw(this, &FHierarchicalLODProxyProcessor::OnMapChange);
	FEditorDelegates::NewCurrentLevel.AddRaw(this, &FHierarchicalLODProxyProcessor::OnNewCurrentLevel);
#endif // WITH_EDITOR
}

FHierarchicalLODProxyProcessor::~FHierarchicalLODProxyProcessor()
{
#if WITH_EDITOR
	FEditorDelegates::MapChange.RemoveAll(this);
	FEditorDelegates::NewCurrentLevel.RemoveAll(this);
#endif // WITH_EDITOR
}

bool FHierarchicalLODProxyProcessor::Tick(float DeltaTime)
{
	FScopeLock Lock(&StateLock);

	while (ToProcessJobs.Num())
	{
		FProcessData* Data = ToProcessJobs.Pop();
		UStaticMesh* MainMesh = nullptr;		
		for (UObject* AssetObject : Data->AssetObjects)
		{
			// We make it private, so it can't be used by outside of map and is invisible in the content browser, and then remove standalone
			AssetObject->ClearFlags(RF_Public | RF_Standalone);
			
			// Check if this is the generated proxy (static-)mesh
			UStaticMesh* StaticMesh = Cast<UStaticMesh>(AssetObject);
			if (StaticMesh)
			{
				check(StaticMesh != nullptr);
				MainMesh = StaticMesh;
			}
		}
		check(MainMesh != nullptr);

		// Set new static mesh, location and sub-objects (UObjects)
		Data->LODActor->SetStaticMesh(MainMesh);
		Data->LODActor->SetActorLocation(FVector::ZeroVector);
		Data->LODActor->SubObjects = Data->AssetObjects;

		// Calculate correct drawing distance according to given screensize
		// At the moment this assumes a fixed field of view of 90 degrees (horizontal and vertical axi)
		static const float FOVRad = 90.0f * (float)PI / 360.0f;
		static const FMatrix ProjectionMatrix = FPerspectiveMatrix(FOVRad, 1920, 1080, 0.01f);
		FBoxSphereBounds Bounds = Data->LODActor->GetStaticMeshComponent()->CalcBounds(FTransform());
		Data->LODActor->LODDrawDistance = FHierarchicalLODUtilities::CalculateDrawDistanceFromScreenSize(Bounds.SphereRadius, Data->LODSetup.TransitionScreenSize, ProjectionMatrix);
		Data->LODActor->UpdateSubActorLODParents();

		// Freshly build so mark not dirty
		Data->LODActor->SetIsDirty(false);

		delete Data;
	}

	return true;
}

FGuid FHierarchicalLODProxyProcessor::AddProxyJob(ALODActor* InLODActor, const FHierarchicalSimplification& LODSetup)
{
	FScopeLock Lock(&StateLock);
	check(InLODActor);
	// Create new unique Guid which will be used to identify this job
	FGuid JobGuid = FGuid::NewGuid();
	// Set up processing data
	FProcessData* Data = new FProcessData();
	Data->LODActor = InLODActor;
	Data->LODSetup = LODSetup;	

	JobActorMap.Add(JobGuid, Data);

	return JobGuid;
}

void FHierarchicalLODProxyProcessor::ProcessProxy(const FGuid InGuid, TArray<UObject*>& InAssetsToSync)
{
	FScopeLock Lock(&StateLock);

	// Check if there is data stored for the given Guid
	FProcessData** DataPtr = JobActorMap.Find(InGuid);
	if (DataPtr)
	{
		// If so push the job onto the processing queue
		FProcessData* Data = *DataPtr;
		JobActorMap.Remove(InGuid);
		if (Data && Data->LODActor)
		{
			Data->AssetObjects = InAssetsToSync;
			ToProcessJobs.Push(Data);
		}
	}
}

FCreateProxyDelegate& FHierarchicalLODProxyProcessor::GetCallbackDelegate()
{
	// Bind ProcessProxy to the delegate (if not bound yet)
	if (!CallbackDelegate.IsBound())
	{
		CallbackDelegate.BindRaw(this, &FHierarchicalLODProxyProcessor::ProcessProxy);
	}

	return CallbackDelegate;
}

void FHierarchicalLODProxyProcessor::OnMapChange(uint32 MapFlags)
{
	ClearProcessingData();
}

void FHierarchicalLODProxyProcessor::OnNewCurrentLevel()
{
	ClearProcessingData();
}

void FHierarchicalLODProxyProcessor::ClearProcessingData()
{
	FScopeLock Lock(&StateLock);
	JobActorMap.Empty();
	ToProcessJobs.Empty();
}
