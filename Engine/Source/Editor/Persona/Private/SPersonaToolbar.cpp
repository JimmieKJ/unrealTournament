// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"
#include "Persona.h"
#include "SPersonaToolbar.h"
#include "Shared/PersonaMode.h"
#include "WorkflowOrientedApp/SModeWidget.h"
#include "WorkflowOrientedApp/SContentReference.h"
#include "AssetData.h"
#include "AnimGraphDefinitions.h"
#include "Editor/MainFrame/Public/MainFrame.h"
#include "IDocumentation.h"

#define LOCTEXT_NAMESPACE "PersonaToolbar"

const float ContentRefWidth = 80.0f;
const FVector2D AssetPickerSize(250, 700);

//////////////////////////////////////////////////////////////////////////
// SPipelineSeparator

class SPipelineSeparator : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SPipelineSeparator) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArg)
	{
		SBorder::Construct(
			SBorder::FArguments()
			.BorderImage(FEditorStyle::GetBrush("Persona.PipelineSeparator"))
			.Padding(0.0f)
			);
	}

	// SWidget interface
	virtual FVector2D ComputeDesiredSize(float) const override
	{
		const float Height = 24.0f;
		const float Thickness = 16.0f;
		return FVector2D(Thickness, Height);
	}
	// End of SWidget interface
};

//////////////////////////////////////////////////////////////////////////
// SPersonaToolbar

void FPersonaToolbar::SetupPersonaToolbar(TSharedPtr<FExtender> Extender, TSharedPtr<FPersona> InPersona)
{
	Persona = InPersona;

	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		Persona.Pin()->GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP( this, &FPersonaToolbar::FillPersonaToolbar ) );

	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		Persona.Pin()->GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP( this, &FPersonaToolbar::FillPersonaModeToolbar ) );
}

void FPersonaToolbar::FillPersonaToolbar(FToolBarBuilder& ParentToolbarBuilder)
{
}

void FPersonaToolbar::FillPersonaModeToolbar(FToolBarBuilder& ParentToolbarBuilder)
{
	TSharedPtr<FPersona> PersonaPtr = Persona.Pin();
	UBlueprint* BlueprintObj = PersonaPtr->GetBlueprintObj();

	
	FToolBarBuilder ToolbarBuilder( ParentToolbarBuilder.GetTopCommandList(), ParentToolbarBuilder.GetCustomization() );
	
	{
		TAttribute<FName> GetActiveMode(PersonaPtr.ToSharedRef(), &FPersona::GetCurrentMode);
		FOnModeChangeRequested SetActiveMode = FOnModeChangeRequested::CreateSP(PersonaPtr.ToSharedRef(), &FPersona::SetCurrentMode);

		// Left side padding
		PersonaPtr->AddToolbarWidget(SNew(SSpacer).Size(FVector2D(4.0f, 1.0f)));

		// Skeleton 'mode' (never active, just informative)
		PersonaPtr->AddToolbarWidget(
			SNew(SModeWidget, FPersonaModes::GetLocalizedMode( FPersonaModes::SkeletonDisplayMode ), FPersonaModes::SkeletonDisplayMode)
			.OnGetActiveMode(GetActiveMode)
			.OnSetActiveMode(SetActiveMode)
			.DirtyMarkerBrush( PersonaPtr.Get(), &FPersona::GetDirtyImageForMode, FPersonaModes::SkeletonDisplayMode)
			.ToolTip(IDocumentation::Get()->CreateToolTip(
				LOCTEXT("SkeletonModeButtonTooltip", "Switch to skeleton editing mode"),
				NULL,
				TEXT("Shared/Editors/Persona"),
				TEXT("SkeletonMode")))
			.AddMetaData<FTagMetaData>(TEXT("Persona.Skeleton"))
			.ShortContents()
			[
				SNew(SContentReference)
				.AssetReference(PersonaPtr.ToSharedRef(), &FPersona::GetSkeletonAsObject)
				.AllowSelectingNewAsset(false)
				.AllowClearingReference(false)
				.WidthOverride(ContentRefWidth)
			]
		);

		PersonaPtr->AddToolbarWidget(SNew(SPipelineSeparator));

		// Skeletal mesh mode
		PersonaPtr->AddToolbarWidget(
			SNew(SModeWidget, FPersonaModes::GetLocalizedMode( FPersonaModes::MeshEditMode ), FPersonaModes::MeshEditMode)
			.OnGetActiveMode(GetActiveMode)
			.OnSetActiveMode(SetActiveMode)
			.DirtyMarkerBrush( PersonaPtr.Get(), &FPersona::GetDirtyImageForMode, FPersonaModes::MeshEditMode)
			.ToolTip(IDocumentation::Get()->CreateToolTip(
				LOCTEXT("MeshModeButtonTooltip", "Switch to skeletal mesh editing mode"),
				NULL,
				TEXT("Shared/Editors/Persona"),
				TEXT("MeshMode")))
			.AddMetaData<FTagMetaData>(TEXT("Persona.Mesh"))
			.ShortContents()
			[
				SNew(SContentReference)
				.WidthOverride(ContentRefWidth)
				.AllowSelectingNewAsset(true)
				.AssetReference(PersonaPtr.ToSharedRef(), &FPersona::GetMeshAsObject)
				.AllowedClass(USkeletalMesh::StaticClass())
				.OnShouldFilterAsset(this, &FPersonaToolbar::ShouldFilterAssetBasedOnSkeleton)
				.OnSetReference(this, &FPersonaToolbar::OnSetSkeletalMeshReference)
				.AssetPickerSizeOverride(AssetPickerSize)
				.InitialAssetViewType(EAssetViewType::List)
			]
		);

		PersonaPtr->AddToolbarWidget(SNew(SPipelineSeparator));

		/*
		// Physics asset mode
		Container->AddSlot()
			[
				SNew(SBox)
				.Padding(FMargin(8,2))
				[
					SNew(SModeWidget, FPersonaModes::GetLocalizedMode( FPersonaModes::PhysicsEditMode ), FPersonaModes::PhysicsEditMode)
					.OnGetActiveMode(GetActiveMode)
					.OnSetActiveMode(SetActiveMode)
					.ShortContents()
					[
						SNew(SContentReference)
						.WidthOverride(ContentRefWidth)
					]
				]
			];
		*/

		// Animation asset mode
		PersonaPtr->AddToolbarWidget(
			SNew(SModeWidget, FPersonaModes::GetLocalizedMode( FPersonaModes::AnimationEditMode ), FPersonaModes::AnimationEditMode)
			.OnGetActiveMode(GetActiveMode)
			.OnSetActiveMode(SetActiveMode)
			.DirtyMarkerBrush( PersonaPtr.Get(), &FPersona::GetDirtyImageForMode, FPersonaModes::AnimationEditMode)
			.ToolTip(IDocumentation::Get()->CreateToolTip(
				LOCTEXT("AnimationModeButtonTooltip", "Switch to animation editing mode"),
				NULL,
				TEXT("Shared/Editors/Persona"),
				TEXT("AnimationMode")))
			.AddMetaData<FTagMetaData>(TEXT("Persona.Anim"))
			.ShortContents()
			[
				//@TODO: How to show this guy?
				SNew(SContentReference)
				.WidthOverride(ContentRefWidth)
				.AllowSelectingNewAsset(true)
				.AssetReference(PersonaPtr.ToSharedRef(), &FPersona::GetPreviewAnimationAsset)//@TODO: Should probably be GetAnimationAssetBeingEdited, though it may depend on the mode!
				.AllowedClass(UAnimationAsset::StaticClass())
				.OnShouldFilterAsset(this, &FPersonaToolbar::ShouldFilterAssetBasedOnSkeleton)
				.OnSetReference(this, &FPersonaToolbar::OnSetAnimationAssetReference)
				.AssetPickerSizeOverride(AssetPickerSize)
				.InitialAssetViewType(EAssetViewType::List)
			]
		);

		// Anim Blueprint mode
		//@TODO: Shouldn't be hiding this, just making it bulletproof!!!
		if (PersonaPtr->GetBlueprintObj() != NULL)
		{
			PersonaPtr->AddToolbarWidget(SNew(SPipelineSeparator));

			PersonaPtr->AddToolbarWidget(
				SNew(SModeWidget, FPersonaModes::GetLocalizedMode( FPersonaModes::AnimBlueprintEditMode ), FPersonaModes::AnimBlueprintEditMode)
				.OnGetActiveMode(GetActiveMode)
				.OnSetActiveMode(SetActiveMode)
				.DirtyMarkerBrush( PersonaPtr.Get(), &FPersona::GetDirtyImageForMode, FPersonaModes::AnimBlueprintEditMode)
				.ToolTip(IDocumentation::Get()->CreateToolTip(
					LOCTEXT("AnimModeButtonTooltip", "Switch to Anim Blueprint editing mode"),
					NULL,
					TEXT("Shared/Editors/Persona"),
					TEXT("GraphMode")))
				.IconImage(FEditorStyle::GetBrush("FullBlueprintEditor.SwitchToScriptingMode"))
				.SmallIconImage(FEditorStyle::GetBrush("FullBlueprintEditor.SwitchToScriptingMode.Small"))
				.AddMetaData<FTagMetaData>(TEXT("Persona.Graph"))
				.ShortContents()
				[
					SNew(SContentReference)
					.AssetReference(PersonaPtr->GetBlueprintObj())
					.AllowSelectingNewAsset(false)
					.AllowClearingReference(false)
					.WidthOverride(ContentRefWidth)
				]
			);
		}

		// Right side padding
		PersonaPtr->AddToolbarWidget(SNew(SSpacer).Size(FVector2D(4.0f, 1.0f)));
	}
}

bool FPersonaToolbar::ShouldFilterAssetBasedOnSkeleton(const FAssetData& AssetData)
{
	const FString* SkeletonName = AssetData.TagsAndValues.Find(TEXT("Skeleton"));

	if ( SkeletonName )
	{
		USkeleton* Skeleton = Persona.Pin()->GetSkeleton();
		if ( (*SkeletonName) == FString::Printf(TEXT("%s'%s'"), *Skeleton->GetClass()->GetName(), *Skeleton->GetPathName()) )
		{
			return false;
		}
	}

	return true;
}

void FPersonaToolbar::OnSetSkeletalMeshReference( UObject* Object )
{
	if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(Object))
	{
		Persona.Pin()->SetPreviewMesh(Mesh);
	}
}

void FPersonaToolbar::OnSetAnimationAssetReference(UObject* Object)
{
	UAnimationAsset* Asset = Cast<UAnimationAsset>(Object);
	Persona.Pin()->SetPreviewAnimationAsset(Asset);
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE