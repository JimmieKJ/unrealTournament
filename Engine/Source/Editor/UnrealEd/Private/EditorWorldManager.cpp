// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EditorWorldManager.h"
#include "ViewportWorldInteraction.h"
#include "VREditorMode.h"
#include "IViewportInteractionModule.h"
#include "EngineGlobals.h"
#include "Editor.h"

/************************************************************************/
/* FEditorWorldWrapper                                                  */
/************************************************************************/
FEditorWorldWrapper::FEditorWorldWrapper(FWorldContext& InWorldContext):
	WorldContext (InWorldContext)
{
	WorldInteraction = NewObject<UViewportWorldInteraction>();
	WorldInteraction->Init(InWorldContext.World());

	VREditorMode = NewObject<UVREditorMode>();
	VREditorMode->Init(WorldInteraction);
}

FEditorWorldWrapper::~FEditorWorldWrapper()
{
	WorldInteraction = nullptr;
	VREditorMode = nullptr;
}

UWorld* FEditorWorldWrapper::GetWorld() const
{
	return WorldContext.World();
}

FWorldContext& FEditorWorldWrapper::GetWorldContext() const
{
	return WorldContext;
}

UViewportWorldInteraction* FEditorWorldWrapper::GetViewportWorldInteraction() const
{
	return WorldInteraction;
}

UVREditorMode* FEditorWorldWrapper::GetVREditorMode() const
{
	return VREditorMode;
}

void FEditorWorldWrapper::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( VREditorMode );
	Collector.AddReferencedObject( WorldInteraction );
}

/************************************************************************/
/* FEditorWorldManager                                                  */
/************************************************************************/
FEditorWorldManager::FEditorWorldManager()
{
	if(GEngine)
	{
		GEngine->OnWorldContextDestroyed().AddRaw(this, &FEditorWorldManager::OnWorldContextRemove);
	}
}

FEditorWorldManager::~FEditorWorldManager()
{
	if(GEngine)
	{
		GEngine->OnWorldContextDestroyed().RemoveAll(this);
	}

	EditorWorldMap.Empty();
}

TSharedPtr<FEditorWorldWrapper> FEditorWorldManager::GetEditorWorldWrapper(const UWorld* InWorld)
{
	// Try to find this world in the map and return it or create and add one if nothing found
	TSharedPtr<FEditorWorldWrapper> Result;
	if(InWorld)
	{
		TSharedPtr<FEditorWorldWrapper>* FoundWorld = EditorWorldMap.Find(InWorld->GetUniqueID());
		if(FoundWorld != nullptr)
		{
			Result = *FoundWorld;
		}
		else
		{
			FWorldContext* WorldContext = GEditor->GetWorldContextFromWorld(InWorld);
			Result = OnWorldContextAdd(*WorldContext);
		}
	}
	return Result;
}

TSharedPtr<FEditorWorldWrapper> FEditorWorldManager::OnWorldContextAdd(FWorldContext& InWorldContext)
{
	//Only add editor type world to the map
	UWorld* World = InWorldContext.World();
	TSharedPtr<FEditorWorldWrapper> Result;
	if(World && (InWorldContext.WorldType == EWorldType::Editor || InWorldContext.WorldType == EWorldType::EditorPreview))
	{
		TSharedPtr<FEditorWorldWrapper> EditorWorld(new FEditorWorldWrapper(InWorldContext));
		Result = EditorWorld;
		EditorWorldMap.Add(World->GetUniqueID(), Result);
	}
	return Result;
}

void FEditorWorldManager::OnWorldContextRemove(FWorldContext& InWorldContext)
{
	UWorld* World = InWorldContext.World();
	if(World)
	{
		EditorWorldMap.Remove(World->GetUniqueID());
	}
}

void FEditorWorldManager::Tick( float DeltaTime )
{
	IViewportInteractionModule& ViewportInteraction = IViewportInteractionModule::Get();
	ViewportInteraction.Tick( DeltaTime );
}
