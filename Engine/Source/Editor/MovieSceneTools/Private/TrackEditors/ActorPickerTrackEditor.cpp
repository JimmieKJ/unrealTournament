// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "ActorPickerTrackEditor.h"
#include "ActorPickerMode.h"
#include "Editor/SceneOutliner/Public/SceneOutliner.h"
#include "Editor/SceneOutliner/Private/SSocketChooser.h"
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "SceneOutlinerPublicTypes.h"

#define LOCTEXT_NAMESPACE "FActorPickerTrackEditor"

FActorPickerTrackEditor::FActorPickerTrackEditor(TSharedRef<ISequencer> InSequencer) 
	: FMovieSceneTrackEditor( InSequencer ) 

{
}

void FActorPickerTrackEditor::PickActorInteractive(FGuid ObjectBinding, UMovieSceneSection* Section)
{
	if(GUnrealEd->GetSelectedActorCount())
	{
		FActorPickerModeModule& ActorPickerMode = FModuleManager::Get().GetModuleChecked<FActorPickerModeModule>("ActorPickerMode");

		ActorPickerMode.BeginActorPickingMode(
			FOnGetAllowedClasses(), 
			FOnShouldFilterActor::CreateSP(this, &FActorPickerTrackEditor::IsActorPickable, ObjectBinding, Section), 
			FOnActorSelected::CreateSP(this, &FActorPickerTrackEditor::ActorPicked, ObjectBinding, Section)
			);
	}
}

void FActorPickerTrackEditor::ShowActorSubMenu(FMenuBuilder& MenuBuilder, FGuid ObjectBinding, UMovieSceneSection* Section)
{
	struct Local
	{
		static FReply OnInteractiveActorPickerClicked(FActorPickerTrackEditor* ActorPickerTrackEditor, FGuid TheObjectBinding, UMovieSceneSection* TheSection)
		{
			FSlateApplication::Get().DismissAllMenus();
			ActorPickerTrackEditor->PickActorInteractive(TheObjectBinding, TheSection);
			return FReply::Handled();
		}
	};

	using namespace SceneOutliner;

	SceneOutliner::FInitializationOptions InitOptions;
	{
		InitOptions.Mode = ESceneOutlinerMode::ActorPicker;			
		InitOptions.bShowHeaderRow = false;
		InitOptions.bFocusSearchBoxWhenOpened = true;
		InitOptions.bShowTransient = true;
		InitOptions.bShowCreateNewFolder = false;
		// Only want the actor label column
		InitOptions.ColumnMap.Add(FBuiltInColumnTypes::Label(), FColumnInfo(EColumnVisibility::Visible, 0));

		// Only display Actors that we can attach too
		InitOptions.Filters->AddFilterPredicate( SceneOutliner::FActorFilterPredicate::CreateSP(this, &FActorPickerTrackEditor::IsActorPickable, ObjectBinding, Section) );
	}		

	// Actor selector to allow the user to choose a parent actor
	FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>( "SceneOutliner" );

	TSharedRef< SWidget > MenuWidget = 
		SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.MaxHeight(400.0f)
			[
				SceneOutlinerModule.CreateSceneOutliner(
					InitOptions,
					FOnActorPicked::CreateSP(this, &FActorPickerTrackEditor::ActorPicked, ObjectBinding, Section )
					)
			]
		]
	
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Top)
		.AutoWidth()
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.ToolTipText( LOCTEXT( "PickButtonLabel", "Pick a parent actor to attach to") )
				.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.OnClicked(FOnClicked::CreateStatic(&Local::OnInteractiveActorPickerClicked, this, ObjectBinding, Section))
				.ContentPadding(4.0f)
				.ForegroundColor(FSlateColor::UseForeground())
				.IsFocusable(false)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.Button_PickActorInteractive"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		];

	MenuBuilder.AddWidget(MenuWidget, FText::GetEmpty(), false);
}

void FActorPickerTrackEditor::ActorPicked(AActor* ParentActor, FGuid ObjectGuid, UMovieSceneSection* Section)
{
	USceneComponent* ComponentWithSockets = NULL;
	if (ParentActor != NULL)
	{
		if (USceneComponent* RootComponent = Cast<USceneComponent>(ParentActor->GetRootComponent()))
		{
			if (RootComponent->HasAnySockets())
			{
				ComponentWithSockets = RootComponent;
			}
		}
	}

	// Show socket chooser if we have sockets to select
	if (ComponentWithSockets != NULL)
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( "LevelEditor");
		TSharedPtr< ILevelEditor > LevelEditor = LevelEditorModule.GetFirstLevelEditor();

		// Create as context menu
		FSlateApplication::Get().PushMenu(
			LevelEditor.ToSharedRef(),
			FWidgetPath(),
			SNew(SSocketChooserPopup)
			.SceneComponent( ComponentWithSockets )
			.OnSocketChosen( this, &FActorPickerTrackEditor::ActorSocketPicked, ParentActor, ObjectGuid, Section ),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu )
			);
	}
	else
	{
		FSlateApplication::Get().DismissAllMenus();
		ActorSocketPicked( NAME_None, ParentActor, ObjectGuid, Section );
	}
}

#undef LOCTEXT_NAMESPACE
