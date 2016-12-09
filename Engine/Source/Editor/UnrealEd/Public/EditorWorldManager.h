// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"

class UViewportWorldInteraction;
class UVREditorMode;
struct FWorldContext;

/**
 * Holds ViewportWorldInteraction for every world with corresponding VR Editor mode
 */
class UNREALED_API FEditorWorldWrapper : public FGCObject
{
public:

	/** Constructor creates ViewportWorldInteration and VREditorMode with the passed world */
	FEditorWorldWrapper(FWorldContext& InWorldContext);

	/** Default destructor */
	virtual ~FEditorWorldWrapper();

	/** Gets the world of the world context */
	UWorld* GetWorld() const;

	/** Gets the world context */
	FWorldContext& GetWorldContext() const;

	/** Gets the viewport world interaction */
	UViewportWorldInteraction* GetViewportWorldInteraction() const;

	/** Gets the VR Editor mode */
	UVREditorMode* GetVREditorMode() const;

	/** FGCObject */
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

private:

	/** The world context */
	FWorldContext& WorldContext;

	/** The viewport world interaction */
	UViewportWorldInteraction* WorldInteraction;

	/** The VR Editor mode */
	UVREditorMode* VREditorMode;
};

/**
 * Has a map of FEditorWorldWrapper
 */
class UNREALED_API FEditorWorldManager	
{
public:

	/** Default constructor */
	FEditorWorldManager();

	/** Default destructor */
	virtual ~FEditorWorldManager();

	/** Gets the editor world wrapper that is found with the world passed.
	 * Adds one for this world if there was non found. */
	TSharedPtr<FEditorWorldWrapper> GetEditorWorldWrapper(const UWorld* InWorld);

	void Tick( float DeltaTime );

private:

	/** Adds a new editor world wrapper when a new world context was created */
	TSharedPtr<FEditorWorldWrapper> OnWorldContextAdd(FWorldContext& InWorldContext);

	/** Adds a new editor world wrapper when a new world context was created */
	void OnWorldContextRemove(FWorldContext& InWorldContext);

	/** Map of all the editor world maps */
	TMap<uint32, TSharedPtr<FEditorWorldWrapper>> EditorWorldMap;
};
