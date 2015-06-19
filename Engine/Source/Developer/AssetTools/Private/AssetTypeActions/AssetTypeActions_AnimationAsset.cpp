// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "Toolkits/IToolkitHost.h"
#include "PersonaModule.h"
#include "EditorAnimUtils.h"
#include "NotificationManager.h"
#include "SNotificationList.h"
#include "SSkeletonWidget.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_AnimationAsset::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	auto AnimAssets = GetTypedWeakObjectPtrs<UAnimationAsset>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AnimSequenceBase_FindSkeleton", "Find Skeleton"),
		LOCTEXT("AnimSequenceBase_FindSkeletonTooltip", "Finds the skeleton for the selected assets in the content browser."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.FindSkeleton"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_AnimationAsset::ExecuteFindSkeleton, AnimAssets ),
			FCanExecuteAction()
			)
		);

	MenuBuilder.AddSubMenu( 
		LOCTEXT("RetargetAnimSubmenu", "Retarget Anim Assets"),
		LOCTEXT("RetargetAnimSubmenu_ToolTip", "Opens the retarget anim assets menu"),
		FNewMenuDelegate::CreateSP( this, &FAssetTypeActions_AnimationAsset::FillRetargetMenu, InObjects ),
		false,
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.RetargetSkeleton")
		);
}

void FAssetTypeActions_AnimationAsset::FillRetargetMenu( FMenuBuilder& MenuBuilder, const TArray<UObject*> InObjects )
{
	bool bAllSkeletonsNull = true;

	for(auto Iter = InObjects.CreateConstIterator(); Iter; ++Iter)
	{
		if(UAnimationAsset* AnimAsset = Cast<UAnimationAsset>(*Iter))
		{
			if(AnimAsset->GetSkeleton())
			{
				bAllSkeletonsNull = false;
				break;
			}
		}
	}

	if(bAllSkeletonsNull)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AnimAsset_RetargetSkeletonInPlace", "Retarget skeleton on existing Anim Assets"),
			LOCTEXT("AnimAsset_RetargetSkeletonInPlaceTooltip", "Retargets the selected Anim Assets to a new skeleton (and optionally all referenced animations too)"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.RetargetSkeleton"),
			FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_AnimationAsset::RetargetAssets, InObjects, false, false, TSharedPtr<IToolkitHost>() ), // false = do not duplicate assets first
			FCanExecuteAction()
			)
			);
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AnimAsset_DuplicateAndRetargetSkeleton", "Duplicate Anim Assets and Retarget"),
		LOCTEXT("AnimAsset_DuplicateAndRetargetSkeletonTooltip", "Duplicates and then retargets the selected Anim Assets to a new skeleton (and optionally all referenced animations too)"),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.DuplicateAndRetargetSkeleton"),
		FUIAction(
		FExecuteAction::CreateSP( this, &FAssetTypeActions_AnimationAsset::RetargetAssets, InObjects, true, false, TSharedPtr<IToolkitHost>() ), // true = duplicate assets and retarget them
		FCanExecuteAction()
		)
		);
}

UThumbnailInfo* FAssetTypeActions_AnimationAsset::GetThumbnailInfo(UObject* Asset) const
{
	UAnimationAsset* Anim = CastChecked<UAnimationAsset>(Asset);
	UThumbnailInfo* ThumbnailInfo = Anim->ThumbnailInfo;
	if (ThumbnailInfo == NULL)
	{
		ThumbnailInfo = NewObject<USceneThumbnailInfo>(Anim);
		Anim->ThumbnailInfo = ThumbnailInfo;
	}

	return ThumbnailInfo;
}

void FAssetTypeActions_AnimationAsset::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto AnimAsset = Cast<UAnimationAsset>(*ObjIt);
		if (AnimAsset != NULL)
		{
			USkeleton* AnimSkeleton = AnimAsset->GetSkeleton();
			if (!AnimSkeleton)
			{
				FText ShouldRetargetMessage = LOCTEXT("ShouldRetargetAnimAsset_Message", "Could not find the skeleton for Anim '{AnimName}' Would you like to choose a new one?");

				FFormatNamedArguments Arguments;
				Arguments.Add( TEXT("AnimName"), FText::FromString(AnimAsset->GetName()));

				if ( FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(ShouldRetargetMessage, Arguments)) == EAppReturnType::Yes )
				{
					bool bDuplicateAssets = false;
					TArray<UObject*> AnimAssets;
					AnimAssets.Add(AnimAsset);
					RetargetAssets(AnimAssets, bDuplicateAssets, true, EditWithinLevelEditor);
				}
			}
			else if (AnimSkeleton)
			{
				const bool bBringToFrontIfOpen = false;
				if (IAssetEditorInstance* EditorInstance = FAssetEditorManager::Get().FindEditorForAsset(AnimSkeleton, bBringToFrontIfOpen))
				{
					// The skeleton is already open in an editor.
					// Tell persona that an animation asset was requested
					EditorInstance->FocusWindow(AnimAsset);
				}
				else
				{
					FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>( "Persona" );
					PersonaModule.CreatePersona(Mode, EditWithinLevelEditor, AnimSkeleton, NULL, AnimAsset, NULL);
				}
			}
		}
	}
}

void FAssetTypeActions_AnimationAsset::ExecuteFindSkeleton(TArray<TWeakObjectPtr<UAnimationAsset>> Objects)
{
	TArray<UObject*> ObjectsToSync;
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if ( Object )
		{
			USkeleton* Skeleton = Object->GetSkeleton();
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

void FAssetTypeActions_AnimationAsset::RetargetAnimationHandler(USkeleton* OldSkeleton, USkeleton* NewSkeleton, bool bRemapReferencedAssets, bool bConvertSpaces, bool bDuplicateAssets,  TArray<TWeakObjectPtr<UObject>> InAnimAssets)
{
	if((!OldSkeleton || OldSkeleton->GetPreviewMesh(false)) && (!NewSkeleton || NewSkeleton->GetPreviewMesh(false)))
	{
		EditorAnimUtils::RetargetAnimations(OldSkeleton, NewSkeleton, InAnimAssets, bRemapReferencedAssets, bDuplicateAssets, bConvertSpaces);
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

void FAssetTypeActions_AnimationAsset::RetargetNonSkeletonAnimationHandler(USkeleton* OldSkeleton, USkeleton* NewSkeleton, bool bRemapReferencedAssets, bool bConvertSpaces, bool bDuplicateAssets, TArray<TWeakObjectPtr<UObject>> InAnimAssets, TWeakPtr<IToolkitHost> EditWithinLevelEditor)
{
	RetargetAnimationHandler(OldSkeleton, NewSkeleton, bRemapReferencedAssets, bConvertSpaces, bDuplicateAssets, InAnimAssets);

	if(NewSkeleton)
	{
		for(auto Asset : InAnimAssets)
		{
			if (Asset.IsValid())
			{
				const bool bBringToFrontIfOpen = false;
				if(IAssetEditorInstance* EditorInstance = FAssetEditorManager::Get().FindEditorForAsset(NewSkeleton, bBringToFrontIfOpen))
				{
					// The skeleton is already open in an editor.
					// Tell persona that an animation asset was requested
					EditorInstance->FocusWindow(Asset.Get());
				}
				else
				{
					EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
					TSharedPtr<IToolkitHost> EditWithInEditor = EditWithinLevelEditor.IsValid()? EditWithinLevelEditor.Pin() : NULL;
					FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
					PersonaModule.CreatePersona(Mode, EditWithInEditor, NewSkeleton, NULL, Cast<UAnimationAsset>(Asset.Get()), NULL);
				}
			}
		}
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToLoadSkeletonlessAnimAsset", "The Anim Asset could not be loaded because it's skeleton is missing."));
	}
}


void FAssetTypeActions_AnimationAsset::RetargetAssets(TArray<UObject*> InAnimAssets, bool bDuplicateAssets, bool bOpenEditor, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	bool bRemapReferencedAssets = false;
	USkeleton* NewSkeleton = NULL;
	USkeleton* OldSkeleton = NULL;
	if(InAnimAssets.Num() > 0)
	{
		UAnimationAsset * AnimAsset = CastChecked<UAnimationAsset>(InAnimAssets[0]);
		OldSkeleton = AnimAsset->GetSkeleton();
	}

	const FText Message = LOCTEXT("SelectSkeletonToRemap", "Select the skeleton to remap this asset to.");

	auto AnimAssets = GetTypedWeakObjectPtrs<UObject>(InAnimAssets);

	if (bOpenEditor)
	{
		SAnimationRemapSkeleton::ShowWindow(OldSkeleton, Message, FOnRetargetAnimation::CreateSP(this, &FAssetTypeActions_AnimationAsset::RetargetNonSkeletonAnimationHandler, bDuplicateAssets, AnimAssets, TWeakPtr<class IToolkitHost> (EditWithinLevelEditor)) );
	}
	else
	{
		SAnimationRemapSkeleton::ShowWindow(OldSkeleton, Message, FOnRetargetAnimation::CreateSP(this, &FAssetTypeActions_AnimationAsset::RetargetAnimationHandler, bDuplicateAssets, AnimAssets) );
	}
}


#undef LOCTEXT_NAMESPACE