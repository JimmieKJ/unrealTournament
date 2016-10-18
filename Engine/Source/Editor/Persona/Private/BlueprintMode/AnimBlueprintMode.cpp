// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"

#include "Animation/AnimInstance.h"

#include "Persona.h"

#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/Kismet/Public/BlueprintEditorTabs.h"
#include "Editor/Kismet/Public/SBlueprintEditorToolbar.h"
#include "Editor/Kismet/Public/BlueprintEditorModes.h"
#include "SSkeletonSlotNames.h"
#include "SSkeletonAnimNotifies.h"

#include "AnimBlueprintMode.h"

/////////////////////////////////////////////////////
// FAnimBlueprintEditAppMode

FAnimBlueprintEditAppMode::FAnimBlueprintEditAppMode(TSharedPtr<FPersona> InPersona)
	: FBlueprintEditorApplicationMode(StaticCastSharedPtr<FBlueprintEditor>(InPersona), FPersonaModes::AnimBlueprintEditMode, FPersonaModes::GetLocalizedMode, false, false)
{
	TabLayout = FTabManager::NewLayout( "Persona_AnimBlueprintEditMode_Layout_v7" )
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				// Top toolbar
				FTabManager::NewStack() ->SetSizeCoefficient( 0.186721f )
				->SetHideTabWell(true)
				->AddTab( InPersona->GetToolbarTabId(), ETabState::OpenedTab )
			)
			->Split
			(
				// Main application area
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					// Left side
					FTabManager::NewSplitter()
					->SetSizeCoefficient(0.25f)
					->SetOrientation( Orient_Vertical )
					->Split
					(
						// Left top - viewport
						FTabManager::NewStack()
						->SetSizeCoefficient(0.5f)
						->SetHideTabWell(true)
						->AddTab( FPersonaTabs::PreviewViewportID, ETabState::OpenedTab )
					)
					->Split
					(
						//	Left bottom - preview settings
						FTabManager::NewStack()
						->SetSizeCoefficient(0.5f)
						->AddTab( FPersonaTabs::AnimBlueprintPreviewEditorID, ETabState::OpenedTab )
						->AddTab( FPersonaTabs::AnimBlueprintParentPlayerEditorID, ETabState::ClosedTab )
					)
				)
				->Split
				(
					// Right side 
					FTabManager::NewSplitter()
					->SetOrientation( Orient_Vertical )
					->SetSizeCoefficient(0.75f)
					->Split
					(
						// Right top - document edit area
						FTabManager::NewStack()
						->SetSizeCoefficient(0.70f)
						->AddTab( "Document", ETabState::ClosedTab )
					)
					->Split
					(
						// Right bottom
						FTabManager::NewSplitter()
						->SetSizeCoefficient(0.30f)
						->SetOrientation(Orient_Horizontal)
						->Split
						(
							// Right bottom, left area - various tabs
							FTabManager::NewStack()
							->SetSizeCoefficient(0.5f)
							->AddTab( FPersonaTabs::AssetBrowserID, ETabState::OpenedTab )
							->AddTab( FBlueprintEditorTabs::MyBlueprintID, ETabState::OpenedTab )
							->AddTab( FPersonaTabs::SkeletonTreeViewID, ETabState::ClosedTab )
						)
						->Split
						(
							// Right bottom, right area - selection details panel
							FTabManager::NewStack()
							->SetSizeCoefficient(0.5f)
							->SetHideTabWell(true)
							->AddTab( FBlueprintEditorTabs::DetailsID, ETabState::OpenedTab )
						)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient( 0.20f )
						->AddTab( FBlueprintEditorTabs::CompilerResultsID, ETabState::ClosedTab )
						->AddTab( FBlueprintEditorTabs::FindResultsID, ETabState::ClosedTab )
					)
				)
			)
		);

	PersonaTabFactories.RegisterFactory(MakeShareable(new FSkeletonTreeSummoner(InPersona)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FAnimationAssetBrowserSummoner(InPersona)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FPreviewViewportSummoner(InPersona)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FSkeletonAnimNotifiesSummoner(InPersona)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FAnimBlueprintPreviewEditorSummoner(InPersona)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FAnimBlueprintParentPlayerEditorSummoner(InPersona)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FSkeletonSlotNamesSummoner(InPersona)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FAdvancedPreviewSceneTabSummoner(InPersona)));

	// setup toolbar - clear existing toolbar extender from the BP mode
	//@TODO: Keep this in sync with BlueprintEditorModes.cpp
	ToolbarExtender = MakeShareable(new FExtender);
	InPersona->GetToolbarBuilder()->AddCompileToolbar(ToolbarExtender);
	InPersona->GetToolbarBuilder()->AddScriptingToolbar(ToolbarExtender);
	InPersona->GetToolbarBuilder()->AddBlueprintGlobalOptionsToolbar(ToolbarExtender);
	InPersona->GetToolbarBuilder()->AddDebuggingToolbar(ToolbarExtender);
}

void FAnimBlueprintEditAppMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	
	BP->RegisterToolbarTab(InTabManager.ToSharedRef());

	// Mode-specific setup
	BP->PushTabFactories(CoreTabFactories);
	BP->PushTabFactories(BlueprintEditorTabFactories);
	BP->PushTabFactories(PersonaTabFactories);
}

void FAnimBlueprintEditAppMode::PostActivateMode()
{
	TSharedPtr<FPersona> Persona = StaticCastSharedPtr<FPersona>(MyBlueprintEditor.Pin());

	if (UAnimBlueprint* AnimBlueprint = Persona->GetAnimBlueprint())
	{
		// Switch off any active preview when going to graph editing mode
		Persona->SetPreviewAnimationAsset(NULL, false);

		// When switching to anim blueprint mode, make sure the object being debugged is either a valid world object or the preview instance
		UDebugSkelMeshComponent* PreviewComponent = Persona->GetPreviewMeshComponent();
		if ((AnimBlueprint->GetObjectBeingDebugged() == NULL) && (PreviewComponent->IsAnimBlueprintInstanced()))
		{
			AnimBlueprint->SetObjectBeingDebugged(PreviewComponent->GetAnimInstance());
		}

		// If we are a derived anim blueprint always show the overrides tab
		if(UAnimBlueprint::FindRootAnimBlueprint(AnimBlueprint))
		{
			Persona->GetTabManager()->InvokeTab(FPersonaTabs::AnimBlueprintParentPlayerEditorID);
		}
	}

	FBlueprintEditorApplicationMode::PostActivateMode();
}