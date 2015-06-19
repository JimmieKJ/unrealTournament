// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "PersonaModule.h"
#include "EditorAnimUtils.h"
#include "NotificationManager.h"
#include "SNotificationList.h"
#include "SSkeletonWidget.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_AnimBlueprint::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	FAssetTypeActions_Blueprint::GetActions(InObjects, MenuBuilder);

	auto AnimBlueprints = GetTypedWeakObjectPtrs<UAnimBlueprint>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AnimBlueprint_FindSkeleton", "Find Skeleton"),
		LOCTEXT("AnimBlueprint_FindSkeletonTooltip", "Finds the skeleton used by the selected Anim Blueprints in the content browser."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.FindSkeleton"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_AnimBlueprint::ExecuteFindSkeleton, AnimBlueprints ),
			FCanExecuteAction()
			)
		);

	MenuBuilder.AddSubMenu( 
		LOCTEXT("RetargetBlueprintSubmenu", "Retarget Anim Blueprints"),
		LOCTEXT("RetargetBlueprintSubmenu_ToolTip", "Opens the retarget blueprints menu"),
		FNewMenuDelegate::CreateSP( this, &FAssetTypeActions_AnimBlueprint::FillRetargetMenu, InObjects ),
		false,
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.RetargetSkeleton")
		);

}

void FAssetTypeActions_AnimBlueprint::FillRetargetMenu( FMenuBuilder& MenuBuilder, const TArray<UObject*> InObjects )
{
	bool bAllSkeletonsNull = true;

	for(auto Iter = InObjects.CreateConstIterator(); Iter; ++Iter)
	{
		if(UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(*Iter))
		{
			if(AnimBlueprint->TargetSkeleton)
			{
				bAllSkeletonsNull = false;
				break;
			}
		}
	}

	if(bAllSkeletonsNull)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AnimBlueprint_RetargetSkeletonInPlace", "Retarget skeleton on existing Anim Blueprints"),
			LOCTEXT("AnimBlueprint_RetargetSkeletonInPlaceTooltip", "Retargets the selected Anim Blueprints to a new skeleton (and optionally all referenced animations too)"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.RetargetSkeleton"),
			FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_AnimBlueprint::RetargetAssets, InObjects, false ), // false = do not duplicate assets first
			FCanExecuteAction()
			)
			);
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AnimBlueprint_DuplicateAndRetargetSkeleton", "Duplicate Anim Blueprints and Retarget"),
		LOCTEXT("AnimBlueprint_DuplicateAndRetargetSkeletonTooltip", "Duplicates and then retargets the selected Anim Blueprints to a new skeleton (and optionally all referenced animations too)"),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.DuplicateAndRetargetSkeleton"),
		FUIAction(
		FExecuteAction::CreateSP( this, &FAssetTypeActions_AnimBlueprint::RetargetAssets, InObjects, true ), // true = duplicate assets and retarget them
		FCanExecuteAction()
		)
		);
}

UThumbnailInfo* FAssetTypeActions_AnimBlueprint::GetThumbnailInfo(UObject* Asset) const
{
	UAnimBlueprint* AnimBlueprint = CastChecked<UAnimBlueprint>(Asset);
	UThumbnailInfo* ThumbnailInfo = AnimBlueprint->ThumbnailInfo;
	if (ThumbnailInfo == NULL)
	{
		ThumbnailInfo = NewObject<USceneThumbnailInfo>(AnimBlueprint);
		AnimBlueprint->ThumbnailInfo = ThumbnailInfo;
	}

	return ThumbnailInfo;
}

UFactory* FAssetTypeActions_AnimBlueprint::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UAnimBlueprintFactory* AnimBlueprintFactory = NewObject<UAnimBlueprintFactory>();
	UAnimBlueprint* AnimBlueprint = CastChecked<UAnimBlueprint>(InBlueprint);
	AnimBlueprintFactory->ParentClass = TSubclassOf<UAnimInstance>(*InBlueprint->GeneratedClass);
	AnimBlueprintFactory->TargetSkeleton = AnimBlueprint->TargetSkeleton;
	return AnimBlueprintFactory;
}

void FAssetTypeActions_AnimBlueprint::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto AnimBlueprint = Cast<UAnimBlueprint>(*ObjIt);
		if (AnimBlueprint != NULL && AnimBlueprint->SkeletonGeneratedClass && AnimBlueprint->GeneratedClass)
		{
			if(!AnimBlueprint->TargetSkeleton)
			{
				FText ShouldRetargetMessage = LOCTEXT("ShouldRetarget_Message", "Could not find the skeleton for Anim Blueprint '{BlueprintName}' Would you like to choose a new one?");
				
				FFormatNamedArguments Arguments;
				Arguments.Add( TEXT("BlueprintName"), FText::FromString(AnimBlueprint->GetName()));

				if ( FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(ShouldRetargetMessage, Arguments)) == EAppReturnType::Yes )
				{
					bool bDuplicateAssets = false;
					TArray<UObject*> AnimBlueprints;
					AnimBlueprints.Add(AnimBlueprint);
					RetargetAssets(AnimBlueprints, bDuplicateAssets);
				}
			}

			if(AnimBlueprint->TargetSkeleton)
			{
				FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>( "Persona" );
				PersonaModule.CreatePersona( Mode, EditWithinLevelEditor, NULL, AnimBlueprint, NULL, NULL);
			}
			else
			{
				FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("FailedToLoadSkeletonlessAnimBlueprint", "The Anim Blueprint could not be loaded because it's skeleton is missing."));
			}
		}
		else
		{
			FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("FailedToLoadCorruptAnimBlueprint", "The Anim Blueprint could not be loaded because it is corrupt."));
		}
	}
}

void FAssetTypeActions_AnimBlueprint::ExecuteFindSkeleton(TArray<TWeakObjectPtr<UAnimBlueprint>> Objects)
{
	TArray<UObject*> ObjectsToSync;
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if ( Object )
		{
			USkeleton* Skeleton = Object->TargetSkeleton;
			if (Skeleton)
			{
				ObjectsToSync.AddUnique(Skeleton);
			}
		}
	}

	if ( ObjectsToSync.Num() > 0 )
	{
		FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
	}
}

void FAssetTypeActions_AnimBlueprint::RetargetAnimationHandler(USkeleton* OldSkeleton, USkeleton* NewSkeleton, bool bRemapReferencedAssets, bool bConvertSpaces, bool bDuplicateAssets, TArray<TWeakObjectPtr<UObject>> AnimBlueprints)
{
	if((!OldSkeleton || OldSkeleton->GetPreviewMesh(true)) && (!NewSkeleton || NewSkeleton->GetPreviewMesh(true)))
	{
		EditorAnimUtils::RetargetAnimations(OldSkeleton, NewSkeleton, AnimBlueprints, bRemapReferencedAssets, bDuplicateAssets, bConvertSpaces);
	}
	else
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("OldSkeletonName"), FText::FromString(GetNameSafe(OldSkeleton)));
		Args.Add(TEXT("NewSkeletonName"), FText::FromString(GetNameSafe(NewSkeleton)));
		FNotificationInfo Info(FText::Format(LOCTEXT("Retarget Failed", "Old Skeleton {OldSkeletonName} and New Skeleton {NewSkeletonName} need to have Preview Mesh set up to convert animation"), Args));
		Info.ExpireDuration = 5.0f;
		Info.bUseLargeFont = false;
		TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
		if(Notification.IsValid())
		{
			Notification->SetCompletionState(SNotificationItem::CS_Fail);
		}
	}
}

void FAssetTypeActions_AnimBlueprint::RetargetAssets(TArray<UObject*> InAnimBlueprints, bool bDuplicateAssets)
{
	bool bRemapReferencedAssets = false;
	USkeleton* OldSkeleton = NULL;

	if ( InAnimBlueprints.Num() > 0 )
	{
		UAnimBlueprint * AnimBP = CastChecked<UAnimBlueprint>(InAnimBlueprints[0]);
		OldSkeleton = AnimBP->TargetSkeleton;
	}

	const FText Message = LOCTEXT("RemapSkeleton_Warning", "Select the skeleton to remap this asset to.");
	auto AnimBlueprints = GetTypedWeakObjectPtrs<UObject>(InAnimBlueprints);

	SAnimationRemapSkeleton::ShowWindow(OldSkeleton, Message, FOnRetargetAnimation::CreateSP(this, &FAssetTypeActions_AnimBlueprint::RetargetAnimationHandler, bDuplicateAssets, AnimBlueprints));
}

#undef LOCTEXT_NAMESPACE