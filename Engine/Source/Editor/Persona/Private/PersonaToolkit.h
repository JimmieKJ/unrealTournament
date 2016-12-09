// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPersonaPreviewScene.h"
#include "IEditableSkeleton.h"
#include "IPersonaToolkit.h"

class FAnimationEditorPreviewScene;
class UAnimationAsset;
class UAnimBlueprint;
class USkeletalMesh;

class FPersonaToolkit : public IPersonaToolkit, public TSharedFromThis<FPersonaToolkit>
{
public:
	FPersonaToolkit();
	virtual ~FPersonaToolkit() {}

	/** Initialize from a various sources */
	void Initialize(USkeleton* InSkeleton);
	void Initialize(UAnimationAsset* InAnimationAsset);
	void Initialize(USkeletalMesh* InSkeletalMesh);
	void Initialize(UAnimBlueprint* InAnimBlueprint);

	/** Optionally create a preview scene - note: creates an editable skeleton */
	void CreatePreviewScene();

	/** IPersonaToolkit interface */
	virtual class USkeleton* GetSkeleton() const override;
	virtual TSharedPtr<class IEditableSkeleton> GetEditableSkeleton() const override;
	virtual class UDebugSkelMeshComponent* GetPreviewMeshComponent() const override;
	virtual class USkeletalMesh* GetMesh() const override;
	virtual void SetMesh(class USkeletalMesh* InSkeletalMesh) override;
	virtual class UAnimBlueprint* GetAnimBlueprint() const override;
	virtual class UAnimationAsset* GetAnimationAsset() const override;
	virtual void SetAnimationAsset(class UAnimationAsset* InAnimationAsset) override;
	virtual class TSharedRef<IPersonaPreviewScene> GetPreviewScene() const override;
	virtual class USkeletalMesh* GetPreviewMesh() const override;
	virtual void SetPreviewMesh(class USkeletalMesh* InSkeletalMesh) override;
	virtual FName GetContext() const override;

private:
	/** The skeleton we are editing */
	USkeleton* Skeleton;

	/** Editable skeleton */
	TSharedPtr<IEditableSkeleton> EditableSkeleton;

	/** The mesh we are editing */
	USkeletalMesh* Mesh;

	/** The anim blueprint we are editing */
	UAnimBlueprint* AnimBlueprint;

	/** the animation asset we are editing */
	UAnimationAsset* AnimationAsset;

	/** Preview scene for the editor */
	TSharedPtr<FAnimationEditorPreviewScene> PreviewScene;

	/** The class of the initial asset we were created with */
	UClass* InitialAssetClass;
};
