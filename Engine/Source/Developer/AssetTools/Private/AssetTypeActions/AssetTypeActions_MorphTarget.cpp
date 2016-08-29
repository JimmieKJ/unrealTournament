// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "Toolkits/AssetEditorManager.h"
#include "Editor/Persona/Public/PersonaModule.h"
#include "ObjectTools.h"
#include "Animation/MorphTarget.h"

DEFINE_LOG_CATEGORY_STATIC(LogMorphTarget, Warning, All);

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_MorphTarget::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	auto MorphTargets = GetTypedWeakObjectPtrs<UMorphTarget>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MorphTarget_Convert", "Move to SkeletalMesh"),
		LOCTEXT("MorphTarget_ConvertTooltip", "Moves the selected morph targets to its own Mesh and delete as an asset."),
		FSlateIcon(),
		FUIAction(
		FExecuteAction::CreateSP( this, &FAssetTypeActions_MorphTarget::ExecuteMovetoMesh, MorphTargets ),
		FCanExecuteAction()
		)
		);
}

void FAssetTypeActions_MorphTarget::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>( "Persona" );
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto MorphTarget = Cast<UMorphTarget>(*ObjIt);
		if (MorphTarget != NULL && MorphTarget->BaseSkelMesh)
		{
			if(MorphTarget->BaseSkelMesh->Skeleton)
			{
				PersonaModule.CreatePersona( Mode, EditWithinLevelEditor, MorphTarget->BaseSkelMesh->Skeleton, NULL, NULL, MorphTarget->BaseSkelMesh);
			}
		}
	}
}

void FAssetTypeActions_MorphTarget::ExecuteMovetoMesh(TArray<TWeakObjectPtr<UMorphTarget>> Objects)
{
	if ( FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("MoveToMeshWarning", "This will move the selected morphtargets to internal SkeletalMesh and delete the assets. This is recommended. Would you like to continue?")) == EAppReturnType::Yes )
	{
		TArray<UObject *> ObjectsToDelete;

		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt).Get();
			if ( Object && Object->BaseSkelMesh )
			{
				USkeletalMesh * BaseSkelMesh = Object->BaseSkelMesh;
				// duplicate
				UMorphTarget* NewObject = DuplicateObject<UMorphTarget>(Object, Object->BaseSkelMesh, Object->GetFName());
				if (NewObject)
				{
					// unregister old morphtargets
					BaseSkelMesh->UnregisterMorphTarget(Object);
					// register new object
					BaseSkelMesh->RegisterMorphTarget(NewObject);
					// mark package as dirty
					BaseSkelMesh->MarkPackageDirty();

					ObjectsToDelete.Add(Object);
				}
				else
				{
					// failed to convert, already converted?
					UE_LOG(LogMorphTarget, Warning, TEXT("%s failed to convert."), *Object->GetName());
				}
			}
		}

		ObjectTools::DeleteObjects(ObjectsToDelete);
	}
}

#undef LOCTEXT_NAMESPACE
