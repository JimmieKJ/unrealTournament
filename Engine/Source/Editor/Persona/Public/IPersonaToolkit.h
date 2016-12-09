// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IEditableSkeleton.h"

/**
 * Reusable 'Persona' features for asset editors concerned with USkeleton-related assets
 */
class IPersonaToolkit
{
public:
	/** Virtual destructor */
	virtual ~IPersonaToolkit() {}

	/** Get the skeleton that we are editing */
	virtual class USkeleton* GetSkeleton() const = 0;

	/** Get the editable skeleton that we are editing */
	virtual TSharedPtr<class IEditableSkeleton> GetEditableSkeleton() const = 0;

	/** Get the preview component that we are using */
	virtual class UDebugSkelMeshComponent* GetPreviewMeshComponent() const = 0;

	/** Get the skeletal mesh that we are editing */
	virtual class USkeletalMesh* GetMesh() const = 0;

	/** Set the skeletal mesh we are editing */
	virtual void SetMesh(class USkeletalMesh* InSkeletalMesh) = 0;

	/** Get the anim blueprint that we are editing */
	virtual class UAnimBlueprint* GetAnimBlueprint() const = 0;

	/** Get the animation asset that we are editing */
	virtual class UAnimationAsset* GetAnimationAsset() const = 0;

	/** Set the animation asset we are editing */
	virtual void SetAnimationAsset(class UAnimationAsset* InAnimationAsset) = 0;

	/** Get the preview scene that we are using */
	virtual TSharedRef<class IPersonaPreviewScene> GetPreviewScene() const = 0;

	/** Set the preview mesh, according to context (mesh, skeleton or animation etc.) */
	virtual class USkeletalMesh* GetPreviewMesh() const = 0;

	/** Set the preview mesh, according to context (mesh, skeleton or animation etc.) */
	virtual void SetPreviewMesh(class USkeletalMesh* InSkeletalMesh) = 0;

	/** Get the context in which this toolkit is being used (usually the class name of the asset) */
	virtual FName GetContext() const = 0;
};
