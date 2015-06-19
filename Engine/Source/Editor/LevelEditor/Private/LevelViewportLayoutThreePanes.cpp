// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LevelEditor.h"
#include "LevelViewportLayoutThreePanes.h"
#include "SLevelViewport.h"

namespace ViewportLayoutThreePanesDefs
{
	/** Default splitters to equal 50/50 split */
	static const float DefaultSplitterPercentage = 0.5f;
}


// FLevelViewportLayoutThreePanes /////////////////////////////

TSharedRef<SWidget> FLevelViewportLayoutThreePanes::MakeViewportLayout(const FString& LayoutString)
{
	FString SpecificLayoutString = GetTypeSpecificLayoutString(LayoutString);

	FEngineShowFlags OrthoShowFlags(ESFIM_Editor);	
	ApplyViewMode(VMI_BrushWireframe, false, OrthoShowFlags);

	FEngineShowFlags PerspectiveShowFlags(ESFIM_Editor);	
	ApplyViewMode(VMI_Lit, true, PerspectiveShowFlags);

	FString ViewportKey0, ViewportKey1, ViewportKey2;
	float PrimarySplitterPercentage = ViewportLayoutThreePanesDefs::DefaultSplitterPercentage;
	float SecondarySplitterPercentage = ViewportLayoutThreePanesDefs::DefaultSplitterPercentage;
	
	if (!SpecificLayoutString.IsEmpty())
	{
		// The Layout String only holds the unique ID of the Additional Layout Configs to use
		const FString& IniSection = FLayoutSaveRestore::GetAdditionalLayoutConfigIni();
		
		ViewportKey0 = SpecificLayoutString + TEXT(".Viewport0");
		ViewportKey1 = SpecificLayoutString + TEXT(".Viewport1");
		ViewportKey2 = SpecificLayoutString + TEXT(".Viewport2");

		FString PercentageString;
		if(GConfig->GetString(*IniSection, *(SpecificLayoutString + TEXT(".Percentage0")), PercentageString, GEditorPerProjectIni))
		{
			TTypeFromString<float>::FromString(PrimarySplitterPercentage, *PercentageString);
		}		
		if(GConfig->GetString(*IniSection, *(SpecificLayoutString + TEXT(".Percentage1")), PercentageString, GEditorPerProjectIni))
		{
			TTypeFromString<float>::FromString(SecondarySplitterPercentage, *PercentageString);
		}	
	}

	TSharedRef<SWidget> LayoutWidget = MakeThreePanelWidget(
		Viewports,
		ViewportKey0, ViewportKey1, ViewportKey2,
		PrimarySplitterPercentage, SecondarySplitterPercentage);

	InitCommonLayoutFromString(SpecificLayoutString);

	return LayoutWidget;
}

void FLevelViewportLayoutThreePanes::SaveLayoutString(const FString& LayoutString) const
{
	if (!bIsTransitioning)
	{
		FString SpecificLayoutString = GetTypeSpecificLayoutString(LayoutString);

		const FString& IniSection = FLayoutSaveRestore::GetAdditionalLayoutConfigIni();

		check (PrimarySplitterWidget->GetChildren()->Num() == 2);
		float PrimaryPercentage = PrimarySplitterWidget->SlotAt(0).SizeValue.Get();
		check (SecondarySplitterWidget->GetChildren()->Num() == 2);
		float SecondaryPercentage = SecondarySplitterWidget->SlotAt(0).SizeValue.Get();

		GConfig->SetString(*IniSection, *(SpecificLayoutString + TEXT(".Percentage0")), *TTypeToString<float>::ToString(PrimaryPercentage), GEditorPerProjectIni);
		GConfig->SetString(*IniSection, *(SpecificLayoutString + TEXT(".Percentage1")), *TTypeToString<float>::ToString(SecondaryPercentage), GEditorPerProjectIni);

		SaveCommonLayoutString(SpecificLayoutString);
	}
}

void FLevelViewportLayoutThreePanes::ReplaceWidget( TSharedRef< SWidget > Source, TSharedRef< SWidget > Replacement )
{
	bool bWasFound = false;

	for (int32 SlotIdx = 0; SlotIdx < PrimarySplitterWidget->GetChildren()->Num(); SlotIdx++)
	{
		if  (PrimarySplitterWidget->GetChildren()->GetChildAt(SlotIdx) == Source)
		{
			PrimarySplitterWidget->SlotAt(SlotIdx)
			[
				Replacement
			];
			bWasFound = true;
			break;
		}
	}

	for (int32 SlotIdx = 0; SlotIdx < SecondarySplitterWidget->GetChildren()->Num(); SlotIdx++)
	{
		if  (SecondarySplitterWidget->GetChildren()->GetChildAt(SlotIdx) == Source)
		{
			SecondarySplitterWidget->SlotAt(SlotIdx)
				[
					Replacement
				];
			bWasFound = true;
			break;
		}
	}

	// Source widget should have already been a content widget for the splitter
	check( bWasFound );
}


// FLevelViewportLayoutThreePanesLeft /////////////////////////////

TSharedRef<SWidget> FLevelViewportLayoutThreePanesLeft::MakeThreePanelWidget(
	TArray<TSharedPtr< SLevelViewport >>& ViewportWidgets,
	const FString& ViewportKey0, const FString& ViewportKey1, const FString& ViewportKey2,
	float PrimarySplitterPercentage, float SecondarySplitterPercentage)
{
	TSharedPtr< SLevelViewport > ViewportWidget0;
	TSharedPtr< SLevelViewport > ViewportWidget1;
	TSharedPtr< SLevelViewport > ViewportWidget2;

	TSharedRef<SWidget> Widget = 
		SAssignNew( PrimarySplitterWidget, SSplitter )
		.Orientation(EOrientation::Orient_Horizontal)
		+SSplitter::Slot()
		.Value(PrimarySplitterPercentage)
		[
			SAssignNew( ViewportWidget0, SLevelViewport )
			.ViewportType( LVT_Perspective )	
			.Realtime( true )
			.ParentLayout( AsShared() )
			.ParentLevelEditor( ParentLevelEditor )
			.ConfigKey( ViewportKey0 )
		]
		+SSplitter::Slot()
		.Value(1.0f - PrimarySplitterPercentage)
		[
			SAssignNew( SecondarySplitterWidget, SSplitter )
			.Orientation(EOrientation::Orient_Vertical)
			+SSplitter::Slot()
			.Value(SecondarySplitterPercentage)
			[
				SAssignNew( ViewportWidget1, SLevelViewport )
				.ViewportType( LVT_OrthoXY )
				.ParentLayout( AsShared() )
				.ParentLevelEditor( ParentLevelEditor )
				.ConfigKey( ViewportKey1 )
			]
			+SSplitter::Slot()
			.Value(1.0f - SecondarySplitterPercentage)
			[
				SAssignNew( ViewportWidget2, SLevelViewport )
				.ViewportType( LVT_OrthoXZ )
				.ParentLayout( AsShared() )
				.ParentLevelEditor( ParentLevelEditor )
				.ConfigKey( ViewportKey2 )
			]
		];

	ViewportWidgets.Add( ViewportWidget0 );
	ViewportWidgets.Add( ViewportWidget1 );
	ViewportWidgets.Add( ViewportWidget2 );

	// Make newly-created perspective viewports active by default
	GCurrentLevelEditingViewportClient = &ViewportWidget0->GetLevelViewportClient();

	return Widget;
}


// FLevelViewportLayoutThreePanesRight /////////////////////////////

TSharedRef<SWidget> FLevelViewportLayoutThreePanesRight::MakeThreePanelWidget(
	TArray<TSharedPtr< SLevelViewport >>& ViewportWidgets,
	const FString& ViewportKey0, const FString& ViewportKey1, const FString& ViewportKey2,
	float PrimarySplitterPercentage, float SecondarySplitterPercentage)
{
	TSharedPtr< SLevelViewport > ViewportWidget0;
	TSharedPtr< SLevelViewport > ViewportWidget1;
	TSharedPtr< SLevelViewport > ViewportWidget2;

	TSharedRef<SWidget> Widget = 
		SAssignNew( PrimarySplitterWidget, SSplitter )
		.Orientation(EOrientation::Orient_Horizontal)
		+SSplitter::Slot()
		.Value(PrimarySplitterPercentage)
		[
			SAssignNew( SecondarySplitterWidget, SSplitter )
			.Orientation(EOrientation::Orient_Vertical)
			+SSplitter::Slot()
			.Value(SecondarySplitterPercentage)
			[
				SAssignNew( ViewportWidget1, SLevelViewport )
				.ViewportType( LVT_OrthoXY )
				.ParentLayout( AsShared() )
				.ParentLevelEditor( ParentLevelEditor )
				.ConfigKey( ViewportKey1 )
			]
			+SSplitter::Slot()
			.Value(1.0f - SecondarySplitterPercentage)
			[
				SAssignNew( ViewportWidget2, SLevelViewport )
				.ViewportType( LVT_OrthoXZ )
				.ParentLayout( AsShared() )
				.ParentLevelEditor( ParentLevelEditor )
				.ConfigKey( ViewportKey2 )
			]
		]
		+SSplitter::Slot()
		.Value(1.0f - PrimarySplitterPercentage)
		[
			SAssignNew( ViewportWidget0, SLevelViewport )
			.ViewportType( LVT_Perspective )	
			.Realtime( true )
			.ParentLayout( AsShared() )
			.ParentLevelEditor( ParentLevelEditor )
			.ConfigKey( ViewportKey0 )
		];

	ViewportWidgets.Add( ViewportWidget0 );
	ViewportWidgets.Add( ViewportWidget1 );
	ViewportWidgets.Add( ViewportWidget2 );

	// Make newly-created perspective viewports active by default
	GCurrentLevelEditingViewportClient = &ViewportWidget0->GetLevelViewportClient();

	return Widget;
}


// FLevelViewportLayoutThreePanesTop /////////////////////////////

TSharedRef<SWidget> FLevelViewportLayoutThreePanesTop::MakeThreePanelWidget(
	TArray<TSharedPtr< SLevelViewport >>& ViewportWidgets,
	const FString& ViewportKey0, const FString& ViewportKey1, const FString& ViewportKey2,
	float PrimarySplitterPercentage, float SecondarySplitterPercentage)
{
	TSharedPtr< SLevelViewport > ViewportWidget0;
	TSharedPtr< SLevelViewport > ViewportWidget1;
	TSharedPtr< SLevelViewport > ViewportWidget2;

	TSharedRef<SWidget> Widget = 
		SAssignNew( PrimarySplitterWidget, SSplitter )
		.Orientation(EOrientation::Orient_Vertical)
		+SSplitter::Slot()
		.Value(PrimarySplitterPercentage)
		[
			SAssignNew( ViewportWidget0, SLevelViewport )
			.ViewportType( LVT_Perspective )	
			.Realtime( true )
			.ParentLayout( AsShared() )
			.ParentLevelEditor( ParentLevelEditor )
			.ConfigKey( ViewportKey0 )
		]
		+SSplitter::Slot()
		.Value(1.0f - PrimarySplitterPercentage)
		[
			SAssignNew( SecondarySplitterWidget, SSplitter )
			.Orientation(EOrientation::Orient_Horizontal)
			+SSplitter::Slot()
			.Value(SecondarySplitterPercentage)
			[
				SAssignNew( ViewportWidget1, SLevelViewport )
				.ViewportType( LVT_OrthoXY )
				.ParentLayout( AsShared() )
				.ParentLevelEditor( ParentLevelEditor )
				.ConfigKey( ViewportKey1 )
			]
			+SSplitter::Slot()
			.Value(1.0f - SecondarySplitterPercentage)
			[
				SAssignNew( ViewportWidget2, SLevelViewport )
				.ViewportType( LVT_OrthoXZ )
				.ParentLayout( AsShared() )
				.ParentLevelEditor( ParentLevelEditor )
				.ConfigKey( ViewportKey2 )
			]
		];

	ViewportWidgets.Add( ViewportWidget0 );
	ViewportWidgets.Add( ViewportWidget1 );
	ViewportWidgets.Add( ViewportWidget2 );

	// Make newly-created perspective viewports active by default
	GCurrentLevelEditingViewportClient = &ViewportWidget0->GetLevelViewportClient();

	return Widget;
}


// FLevelViewportLayoutThreePanesBottom /////////////////////////////

TSharedRef<SWidget> FLevelViewportLayoutThreePanesBottom::MakeThreePanelWidget(
	TArray<TSharedPtr< SLevelViewport >>& ViewportWidgets,
	const FString& ViewportKey0, const FString& ViewportKey1, const FString& ViewportKey2,
	float PrimarySplitterPercentage, float SecondarySplitterPercentage)
{
	TSharedPtr< SLevelViewport > ViewportWidget0;
	TSharedPtr< SLevelViewport > ViewportWidget1;
	TSharedPtr< SLevelViewport > ViewportWidget2;

	TSharedRef<SWidget> Widget = 
		SAssignNew( PrimarySplitterWidget, SSplitter )
		.Orientation(EOrientation::Orient_Vertical)
		+SSplitter::Slot()
		.Value(PrimarySplitterPercentage)
		[
			SAssignNew( SecondarySplitterWidget, SSplitter )
			.Orientation(EOrientation::Orient_Horizontal)
			+SSplitter::Slot()
			.Value(SecondarySplitterPercentage)
			[
				SAssignNew( ViewportWidget1, SLevelViewport )
				.ViewportType( LVT_OrthoXY )
				.ParentLayout( AsShared() )
				.ParentLevelEditor( ParentLevelEditor )
				.ConfigKey( ViewportKey1 )
			]
			+SSplitter::Slot()
			.Value(1.0f - SecondarySplitterPercentage)
			[
				SAssignNew( ViewportWidget2, SLevelViewport )
				.ViewportType( LVT_OrthoXZ )
				.ParentLayout( AsShared() )
				.ParentLevelEditor( ParentLevelEditor )
				.ConfigKey( ViewportKey2 )
			]
		]
		+SSplitter::Slot()
		.Value(1.0f - PrimarySplitterPercentage)
		[
			SAssignNew( ViewportWidget0, SLevelViewport )
			.ViewportType( LVT_Perspective )	
			.Realtime( true )
			.ParentLayout( AsShared() )
			.ParentLevelEditor( ParentLevelEditor )
			.ConfigKey( ViewportKey0 )
		];

	ViewportWidgets.Add( ViewportWidget0 );
	ViewportWidgets.Add( ViewportWidget1 );
	ViewportWidgets.Add( ViewportWidget2 );

	// Make newly-created perspective viewports active by default
	GCurrentLevelEditingViewportClient = &ViewportWidget0->GetLevelViewportClient();

	return Widget;
}
