// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"
#include "Persona.h"

#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"
#include "Editor/Kismet/Public/BlueprintEditorTabs.h"
#include "Editor/KismetWidgets/Public/SSingleObjectDetailsPanel.h"

#include "PersonaMeshDetails.h"

#include "MeshMode.h"

#define LOCTEXT_NAMESPACE "PersonaMeshMode"

/////////////////////////////////////////////////////
// SMeshPropertiesTabBody

class SMeshPropertiesTabBody : public SSingleObjectDetailsPanel
{
public:
	SLATE_BEGIN_ARGS(SMeshPropertiesTabBody) {}
	SLATE_END_ARGS()

private:
	// Pointer back to owning Persona instance (the keeper of state)
	TWeakPtr<class FPersona> PersonaPtr;
public:
	void Construct(const FArguments& InArgs, TSharedPtr<FPersona> InPersona)
	{
		PersonaPtr = InPersona;

		SSingleObjectDetailsPanel::Construct(SSingleObjectDetailsPanel::FArguments().HostCommandList(InPersona->GetToolkitCommands()), true, true);

		PropertyView->SetGenericLayoutDetailsDelegate( FOnGetDetailCustomizationInstance::CreateStatic( &FPersonaMeshDetails::MakeInstance, InPersona ) );
	}

	// SSingleObjectDetailsPanel interface
	virtual UObject* GetObjectToObserve() const override
	{
		return PersonaPtr.Pin()->GetMesh();
	}
	// End of SSingleObjectDetailsPanel interface
};

/////////////////////////////////////////////////////
// FMeshPropertiesSummoner

FMeshPropertiesSummoner::FMeshPropertiesSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FPersonaTabs::MeshAssetPropertiesID, InHostingApp)
{
	TabLabel = LOCTEXT("MeshProperties_TabTitle", "Mesh Details");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.SkeletalMesh");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("MeshProperties_MenuTitle", "Mesh Details");
	ViewMenuTooltip = LOCTEXT("MeshProperties_MenuToolTip", "Shows the skeletal mesh properties");
}

TSharedRef<SWidget> FMeshPropertiesSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FPersona> PersonaApp = StaticCastSharedPtr<FPersona>(HostingApp.Pin());
	return SNew(SMeshPropertiesTabBody, PersonaApp);
}

/////////////////////////////////////////////////////
// FMeshEditAppMode

FMeshEditAppMode::FMeshEditAppMode(TSharedPtr<FPersona> InPersona)
	: FPersonaAppMode(InPersona, FPersonaModes::MeshEditMode)
{
	PersonaTabFactories.RegisterFactory(MakeShareable(new FSelectionDetailsSummoner(InPersona)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FMeshPropertiesSummoner(InPersona)));

	TabLayout = FTabManager::NewLayout( "Persona_MeshEditMode_Layout_v7" )
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				// Top toolbar area
				FTabManager::NewStack()
				->SetSizeCoefficient(0.186721f)
				->SetHideTabWell(true)
				->AddTab( InPersona->GetToolbarTabId(), ETabState::OpenedTab )
			)
			->Split
			(
				// Rest of screen
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					// Left 1/3rd - Skeleton tree and mesh panel
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.3f)
					->Split
					(
						FTabManager::NewStack()
						->AddTab( FPersonaTabs::MeshAssetPropertiesID, ETabState::OpenedTab )
					)
				)
				->Split
				(
					// Middle 1/3rd - Viewport
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.5f)
					->Split
					(
						FTabManager::NewStack()
						->SetHideTabWell(true)
						->AddTab( FPersonaTabs::PreviewViewportID, ETabState::OpenedTab )
					)
				)
				->Split
				(
					// Right 1/3rd - Details panel and quick browser
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.2f)
					->Split
					(
						FTabManager::NewStack()
						->AddTab(FPersonaTabs::AdvancedPreviewSceneSettingsID, ETabState::OpenedTab)
						->AddTab( FPersonaTabs::MorphTargetsID, ETabState::OpenedTab )						
					)
				)
			)
		);
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
