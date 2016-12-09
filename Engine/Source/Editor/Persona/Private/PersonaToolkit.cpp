// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PersonaToolkit.h"
#include "Modules/ModuleManager.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimationAsset.h"
#include "AnimationEditorPreviewScene.h"
#include "ISkeletonEditorModule.h"
#include "Animation/AnimBlueprint.h"
#include "GameFramework/WorldSettings.h"
#include "ScopedTransaction.h"
#include "PersonaModule.h"

FPersonaToolkit::FPersonaToolkit()
	: Skeleton(nullptr)
	, Mesh(nullptr)
	, AnimBlueprint(nullptr)
	, AnimationAsset(nullptr)
{
}

void FPersonaToolkit::Initialize(USkeleton* InSkeleton)
{
	check(InSkeleton);
	Skeleton = InSkeleton;
	Mesh = InSkeleton->GetPreviewMesh();
	if (Mesh == nullptr)
	{
		Mesh = Skeleton->FindCompatibleMesh();
	}
	InitialAssetClass = USkeleton::StaticClass();
}

void FPersonaToolkit::Initialize(UAnimationAsset* InAnimationAsset)
{
	check(InAnimationAsset);
	AnimationAsset = InAnimationAsset;
	Skeleton = InAnimationAsset->GetSkeleton();
	Mesh = InAnimationAsset->GetPreviewMesh();
	if (Mesh == nullptr)
	{
		Mesh = Skeleton->GetPreviewMesh();
	}
	if (Mesh == nullptr)
	{
		Mesh = Skeleton->FindCompatibleMesh();
	}
	InitialAssetClass = UAnimationAsset::StaticClass();
}

void FPersonaToolkit::Initialize(USkeletalMesh* InSkeletalMesh)
{
	check(InSkeletalMesh);
	Skeleton = InSkeletalMesh->Skeleton;
	Mesh = InSkeletalMesh;
	InitialAssetClass = USkeletalMesh::StaticClass();
}

void FPersonaToolkit::Initialize(UAnimBlueprint* InAnimBlueprint)
{
	check(InAnimBlueprint);
	AnimBlueprint = InAnimBlueprint;
	Skeleton = InAnimBlueprint->TargetSkeleton;
	check(InAnimBlueprint->TargetSkeleton);
	Mesh = InAnimBlueprint->TargetSkeleton->GetPreviewMesh();
	if (Mesh == nullptr)
	{
		Mesh = Skeleton->FindCompatibleMesh();
	}
	InitialAssetClass = UAnimBlueprint::StaticClass();
}

void FPersonaToolkit::CreatePreviewScene()
{
	if (!PreviewScene.IsValid())
	{
		if (!EditableSkeleton.IsValid())
		{
			ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::LoadModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
			EditableSkeleton = SkeletonEditorModule.CreateEditableSkeleton(Skeleton);
		}

		PreviewScene = MakeShareable(new FAnimationEditorPreviewScene(FPreviewScene::ConstructionValues().AllowAudioPlayback(true).ShouldSimulatePhysics(true), EditableSkeleton.ToSharedRef(), AsShared()));

		//Temporary fix for missing attached assets - MDW
		PreviewScene->GetWorld()->GetWorldSettings()->SetIsTemporarilyHiddenInEditor(false);

		// allow external systems to add components or otherwise manipulate the scene
		FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>(TEXT("Persona"));
		PersonaModule.OnPreviewSceneCreated().Broadcast(PreviewScene.ToSharedRef());

		bool bSetMesh = false;

		// Set the mesh
		if (Mesh != nullptr)
		{
			PreviewScene->SetPreviewMesh(Mesh);
			bSetMesh = true;
		}
		else if (AnimationAsset != nullptr)
		{
			USkeletalMesh* AssetMesh = AnimationAsset->GetPreviewMesh();
			if (AssetMesh)
			{
				PreviewScene->SetPreviewMesh(AssetMesh);
				bSetMesh = true;
			}
		}

		if (!bSetMesh && Skeleton)
		{
			//If no preview mesh set, just find the first mesh that uses this skeleton
			USkeletalMesh* PreviewMesh = Skeleton->FindCompatibleMesh();
			if (PreviewMesh)
			{
				PreviewScene->SetPreviewMesh(PreviewMesh);
			}
		}
	}
}

USkeleton* FPersonaToolkit::GetSkeleton() const
{
	return Skeleton;
}

TSharedPtr<class IEditableSkeleton> FPersonaToolkit::GetEditableSkeleton() const
{
	return EditableSkeleton;
}

UDebugSkelMeshComponent* FPersonaToolkit::GetPreviewMeshComponent() const
{
	return PreviewScene.IsValid() ? PreviewScene->GetPreviewMeshComponent() : nullptr;
}

USkeletalMesh* FPersonaToolkit::GetMesh() const
{
	return Mesh;
}

void FPersonaToolkit::SetMesh(class USkeletalMesh* InSkeletalMesh)
{
	if (InSkeletalMesh != nullptr)
	{
		check(InSkeletalMesh->Skeleton == Skeleton);
	}

	Mesh = InSkeletalMesh;
}

UAnimBlueprint* FPersonaToolkit::GetAnimBlueprint() const
{
	return AnimBlueprint;
}

UAnimationAsset* FPersonaToolkit::GetAnimationAsset() const
{
	return AnimationAsset;
}

void FPersonaToolkit::SetAnimationAsset(class UAnimationAsset* InAnimationAsset)
{
	if (InAnimationAsset != nullptr)
	{
		check(InAnimationAsset->GetSkeleton() == Skeleton);
	}

	AnimationAsset = InAnimationAsset;
}

TSharedRef<IPersonaPreviewScene> FPersonaToolkit::GetPreviewScene() const
{
	return PreviewScene.ToSharedRef();
}
USkeletalMesh* FPersonaToolkit::GetPreviewMesh() const
{
	if (InitialAssetClass == UAnimationAsset::StaticClass())
	{
		check(AnimationAsset);
		return AnimationAsset->GetPreviewMesh();
	}
	else if(InitialAssetClass == USkeletalMesh::StaticClass())
	{
		check(Mesh);
		return Mesh;
	}
	else
	{
		check(Skeleton);
		return Skeleton->GetPreviewMesh();
	}
}

void FPersonaToolkit::SetPreviewMesh(class USkeletalMesh* InSkeletalMesh)
{
	// Cant set preview mesh on a skeletal mesh (makes for a confusing experience!)
	if (InitialAssetClass != USkeletalMesh::StaticClass())
	{
		if (InitialAssetClass == UAnimationAsset::StaticClass())
		{
			FScopedTransaction Transaction(NSLOCTEXT("PersonaToolkit", "SetAnimationPreviewMesh", "Set Animation Preview Mesh"));

			check(AnimationAsset);
			AnimationAsset->SetPreviewMesh(InSkeletalMesh);
		}
		else
		{
			check(EditableSkeleton.IsValid());
			EditableSkeleton->SetPreviewMesh(InSkeletalMesh);
		}

		GetPreviewScene()->SetPreviewMesh(InSkeletalMesh);
	}
}

FName FPersonaToolkit::GetContext() const
{
	if (InitialAssetClass != nullptr)
	{
		return InitialAssetClass->GetFName();
	}

	return NAME_None;
}
