// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LevelEditor.h"
#include "LevelViewportLayoutFourPanes.h"
#include "SLevelViewport.h"

namespace ViewportLayoutFourPanesDefs
{
	/** Default main splitter to equal 50/50 split */
	static const float DefaultPrimarySplitterPercentage = 0.5f;

	/** Default secondary splitter to equal three-way split */
	static const float DefaultSecondarySplitterPercentage = 0.333f;
}


// FLevelViewportLayoutFourPanes /////////////////////////////

TSharedRef<SWidget> FLevelViewportLayoutFourPanes::MakeViewportLayout(const FString& LayoutString)
{
	FString SpecificLayoutString = GetTypeSpecificLayoutString(LayoutString);

	FString ViewportKey0, ViewportKey1, ViewportKey2, ViewportKey3;
	float PrimarySplitterPercentage = ViewportLayoutFourPanesDefs::DefaultPrimarySplitterPercentage;
	float SecondarySplitterPercentage0 = ViewportLayoutFourPanesDefs::DefaultSecondarySplitterPercentage;
	float SecondarySplitterPercentage1 = ViewportLayoutFourPanesDefs::DefaultSecondarySplitterPercentage;
	
	if (!SpecificLayoutString.IsEmpty())
	{
		// The Layout String only holds the unique ID of the Additional Layout Configs to use
		const FString& IniSection = FLayoutSaveRestore::GetAdditionalLayoutConfigIni();
		
		ViewportKey0 = SpecificLayoutString + TEXT(".Viewport0");
		ViewportKey1 = SpecificLayoutString + TEXT(".Viewport1");
		ViewportKey2 = SpecificLayoutString + TEXT(".Viewport2");
		ViewportKey3 = SpecificLayoutString + TEXT(".Viewport3");

		FString PercentageString;
		if(GConfig->GetString(*IniSection, *(SpecificLayoutString + TEXT(".Percentage0")), PercentageString, GEditorPerProjectIni))
		{
			TTypeFromString<float>::FromString(PrimarySplitterPercentage, *PercentageString);
		}		
		if(GConfig->GetString(*IniSection, *(SpecificLayoutString + TEXT(".Percentage1")), PercentageString, GEditorPerProjectIni))
		{
			TTypeFromString<float>::FromString(SecondarySplitterPercentage0, *PercentageString);
		}	
		if(GConfig->GetString(*IniSection, *(SpecificLayoutString + TEXT(".Percentage2")), PercentageString, GEditorPerProjectIni))
		{
			TTypeFromString<float>::FromString(SecondarySplitterPercentage1, *PercentageString);
		}	
	}

	TSharedRef<SWidget> LayoutWidget = MakeFourPanelWidget(
		Viewports,
		ViewportKey0, ViewportKey1, ViewportKey2, ViewportKey3,
		PrimarySplitterPercentage, SecondarySplitterPercentage0, SecondarySplitterPercentage1);

	InitCommonLayoutFromString(SpecificLayoutString);

	return LayoutWidget;
}

void FLevelViewportLayoutFourPanes::SaveLayoutString(const FString& LayoutString) const
{
	if (!bIsTransitioning)
	{
		FString SpecificLayoutString = GetTypeSpecificLayoutString(LayoutString);

		const FString& IniSection = FLayoutSaveRestore::GetAdditionalLayoutConfigIni();

		check (PrimarySplitterWidget->GetChildren()->Num() == 2);
		float PrimaryPercentage = PrimarySplitterWidget->SlotAt(0).SizeValue.Get();
		check (SecondarySplitterWidget->GetChildren()->Num() == 3);
		float SecondaryPercentage0 = SecondarySplitterWidget->SlotAt(0).SizeValue.Get();
		float SecondaryPercentage1 = SecondarySplitterWidget->SlotAt(1).SizeValue.Get();

		GConfig->SetString(*IniSection, *(SpecificLayoutString + TEXT(".Percentage0")), *TTypeToString<float>::ToString(PrimaryPercentage), GEditorPerProjectIni);
		GConfig->SetString(*IniSection, *(SpecificLayoutString + TEXT(".Percentage1")), *TTypeToString<float>::ToString(SecondaryPercentage0), GEditorPerProjectIni);
		GConfig->SetString(*IniSection, *(SpecificLayoutString + TEXT(".Percentage2")), *TTypeToString<float>::ToString(SecondaryPercentage1), GEditorPerProjectIni);

		SaveCommonLayoutString(SpecificLayoutString);
	}
}

void FLevelViewportLayoutFourPanes::ReplaceWidget( TSharedRef< SWidget > Source, TSharedRef< SWidget > Replacement )
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


// FLevelViewportLayoutFourPanesLeft /////////////////////////////

TSharedRef<SWidget> FLevelViewportLayoutFourPanesLeft::MakeFourPanelWidget(
	TArray<TSharedPtr< SLevelViewport >>& ViewportWidgets,
	const FString& ViewportKey0, const FString& ViewportKey1, const FString& ViewportKey2, const FString& ViewportKey3,
	float PrimarySplitterPercentage, float SecondarySplitterPercentage0, float SecondarySplitterPercentage1)
{
	TSharedPtr< SLevelViewport > ViewportWidget0;
	TSharedPtr< SLevelViewport > ViewportWidget1;
	TSharedPtr< SLevelViewport > ViewportWidget2;
	TSharedPtr< SLevelViewport > ViewportWidget3;

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
			.Value(SecondarySplitterPercentage0)
			[
				SAssignNew( ViewportWidget1, SLevelViewport )
				.ViewportType( LVT_OrthoXY )
				.ParentLayout( AsShared() )
				.ParentLevelEditor( ParentLevelEditor )
				.ConfigKey( ViewportKey1 )
			]
			+SSplitter::Slot()
			.Value(SecondarySplitterPercentage1)
			[
				SAssignNew( ViewportWidget2, SLevelViewport )
				.ViewportType( LVT_OrthoXZ )
				.ParentLayout( AsShared() )
				.ParentLevelEditor( ParentLevelEditor )
				.ConfigKey( ViewportKey2 )
			]
			+SSplitter::Slot()
			.Value(1.0f - SecondarySplitterPercentage0 - SecondarySplitterPercentage1)
			[
				SAssignNew( ViewportWidget3, SLevelViewport )
				.ViewportType( LVT_OrthoYZ )
				.ParentLayout( AsShared() )
				.ParentLevelEditor( ParentLevelEditor )
				.ConfigKey( ViewportKey3 )
			]
		];

	ViewportWidgets.Add( ViewportWidget0 );
	ViewportWidgets.Add( ViewportWidget1 );
	ViewportWidgets.Add( ViewportWidget2 );
	ViewportWidgets.Add( ViewportWidget3 );

	// Make newly-created perspective viewports active by default
	GCurrentLevelEditingViewportClient = &ViewportWidget0->GetLevelViewportClient();

	return Widget;
}


// FLevelViewportLayoutFourPanesRight /////////////////////////////

TSharedRef<SWidget> FLevelViewportLayoutFourPanesRight::MakeFourPanelWidget(
	TArray<TSharedPtr< SLevelViewport >>& ViewportWidgets,
	const FString& ViewportKey0, const FString& ViewportKey1, const FString& ViewportKey2, const FString& ViewportKey3,
	float PrimarySplitterPercentage, float SecondarySplitterPercentage0, float SecondarySplitterPercentage1)
{
	TSharedPtr< SLevelViewport > ViewportWidget0;
	TSharedPtr< SLevelViewport > ViewportWidget1;
	TSharedPtr< SLevelViewport > ViewportWidget2;
	TSharedPtr< SLevelViewport > ViewportWidget3;

	TSharedRef<SWidget> Widget = 
		SAssignNew( PrimarySplitterWidget, SSplitter )
		.Orientation(EOrientation::Orient_Horizontal)
		+SSplitter::Slot()
		.Value(PrimarySplitterPercentage)
		[
			SAssignNew( SecondarySplitterWidget, SSplitter )
			.Orientation(EOrientation::Orient_Vertical)
			+SSplitter::Slot()
			.Value(SecondarySplitterPercentage0)
			[
				SAssignNew( ViewportWidget1, SLevelViewport )
				.ViewportType( LVT_OrthoXY )
				.ParentLayout( AsShared() )
				.ParentLevelEditor( ParentLevelEditor )
				.ConfigKey( ViewportKey1 )
			]
			+SSplitter::Slot()
			.Value(SecondarySplitterPercentage1)
			[
				SAssignNew( ViewportWidget2, SLevelViewport )
				.ViewportType( LVT_OrthoXZ )
				.ParentLayout( AsShared() )
				.ParentLevelEditor( ParentLevelEditor )
				.ConfigKey( ViewportKey2 )
			]
			+SSplitter::Slot()
			.Value(1.0f - SecondarySplitterPercentage0 - SecondarySplitterPercentage1)
			[
				SAssignNew( ViewportWidget3, SLevelViewport )
				.ViewportType( LVT_OrthoYZ )
				.ParentLayout( AsShared() )
				.ParentLevelEditor( ParentLevelEditor )
				.ConfigKey( ViewportKey3 )
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
	ViewportWidgets.Add( ViewportWidget3 );

	// Make newly-created perspective viewports active by default
	GCurrentLevelEditingViewportClient = &ViewportWidget0->GetLevelViewportClient();

	return Widget;
}


// FLevelViewportLayoutFourPanesTop /////////////////////////////

TSharedRef<SWidget> FLevelViewportLayoutFourPanesTop::MakeFourPanelWidget(
	TArray<TSharedPtr< SLevelViewport >>& ViewportWidgets,
	const FString& ViewportKey0, const FString& ViewportKey1, const FString& ViewportKey2, const FString& ViewportKey3,
	float PrimarySplitterPercentage, float SecondarySplitterPercentage0, float SecondarySplitterPercentage1)
{
	TSharedPtr< SLevelViewport > ViewportWidget0;
	TSharedPtr< SLevelViewport > ViewportWidget1;
	TSharedPtr< SLevelViewport > ViewportWidget2;
	TSharedPtr< SLevelViewport > ViewportWidget3;

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
			.Value(SecondarySplitterPercentage0)
			[
				SAssignNew( ViewportWidget1, SLevelViewport )
				.ViewportType( LVT_OrthoXY )
				.ParentLayout( AsShared() )
				.ParentLevelEditor( ParentLevelEditor )
				.ConfigKey( ViewportKey1 )
			]
			+SSplitter::Slot()
			.Value(SecondarySplitterPercentage1)
			[
				SAssignNew( ViewportWidget2, SLevelViewport )
				.ViewportType( LVT_OrthoXZ )
				.ParentLayout( AsShared() )
				.ParentLevelEditor( ParentLevelEditor )
				.ConfigKey( ViewportKey2 )
			]
			+SSplitter::Slot()
			.Value(1.0f - SecondarySplitterPercentage0 - SecondarySplitterPercentage1)
			[
				SAssignNew( ViewportWidget3, SLevelViewport )
				.ViewportType( LVT_OrthoYZ )
				.ParentLayout( AsShared() )
				.ParentLevelEditor( ParentLevelEditor )
				.ConfigKey( ViewportKey3 )
			]
		];

	ViewportWidgets.Add( ViewportWidget0 );
	ViewportWidgets.Add( ViewportWidget1 );
	ViewportWidgets.Add( ViewportWidget2 );
	ViewportWidgets.Add( ViewportWidget3 );

	// Make newly-created perspective viewports active by default
	GCurrentLevelEditingViewportClient = &ViewportWidget0->GetLevelViewportClient();

	return Widget;
}


// FLevelViewportLayoutFourPanesBottom /////////////////////////////

TSharedRef<SWidget> FLevelViewportLayoutFourPanesBottom::MakeFourPanelWidget(
	TArray<TSharedPtr< SLevelViewport >>& ViewportWidgets,
	const FString& ViewportKey0, const FString& ViewportKey1, const FString& ViewportKey2, const FString& ViewportKey3,
	float PrimarySplitterPercentage, float SecondarySplitterPercentage0, float SecondarySplitterPercentage1)
{
	TSharedPtr< SLevelViewport > ViewportWidget0;
	TSharedPtr< SLevelViewport > ViewportWidget1;
	TSharedPtr< SLevelViewport > ViewportWidget2;
	TSharedPtr< SLevelViewport > ViewportWidget3;

	TSharedRef<SWidget> Widget = 
		SAssignNew( PrimarySplitterWidget, SSplitter )
		.Orientation(EOrientation::Orient_Vertical)
		+SSplitter::Slot()
		.Value(PrimarySplitterPercentage)
		[
			SAssignNew( SecondarySplitterWidget, SSplitter )
			.Orientation(EOrientation::Orient_Horizontal)
			+SSplitter::Slot()
			.Value(SecondarySplitterPercentage0)
			[
				SAssignNew( ViewportWidget1, SLevelViewport )
				.ViewportType( LVT_OrthoXY )
				.ParentLayout( AsShared() )
				.ParentLevelEditor( ParentLevelEditor )
				.ConfigKey( ViewportKey1 )
			]
			+SSplitter::Slot()
			.Value(SecondarySplitterPercentage1)
			[
				SAssignNew( ViewportWidget2, SLevelViewport )
				.ViewportType( LVT_OrthoXZ )
				.ParentLayout( AsShared() )
				.ParentLevelEditor( ParentLevelEditor )
				.ConfigKey( ViewportKey2 )
			]
			+SSplitter::Slot()
			.Value(1.0f - SecondarySplitterPercentage0 - SecondarySplitterPercentage1)
			[
				SAssignNew( ViewportWidget3, SLevelViewport )
				.ViewportType( LVT_OrthoYZ )
				.ParentLayout( AsShared() )
				.ParentLevelEditor( ParentLevelEditor )
				.ConfigKey( ViewportKey3 )
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
	ViewportWidgets.Add( ViewportWidget3 );

	// Make newly-created perspective viewports active by default
	GCurrentLevelEditingViewportClient = &ViewportWidget0->GetLevelViewportClient();

	return Widget;
}
