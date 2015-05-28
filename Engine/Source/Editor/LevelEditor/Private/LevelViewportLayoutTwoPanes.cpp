// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LevelEditor.h"
#include "LevelViewportLayoutTwoPanes.h"
#include "SLevelViewport.h"

namespace ViewportLayoutTwoPanesDefs
{
	/** Default splitters to equal 50/50 split */
	static const float DefaultSplitterPercentage = 0.5f;
}


// TLevelViewportLayoutTwoPanes<T> /////////////////////////////

template <EOrientation TOrientation>
TSharedRef<SWidget> TLevelViewportLayoutTwoPanes<TOrientation>::MakeViewportLayout(const FString& LayoutString)
{
	FString SpecificLayoutString = GetTypeSpecificLayoutString(LayoutString);

	TSharedPtr< SLevelViewport > ViewportWidget0;
	TSharedPtr< SLevelViewport > ViewportWidget1;

	FEngineShowFlags OrthoShowFlags(ESFIM_Editor);	
	ApplyViewMode(VMI_BrushWireframe, false, OrthoShowFlags);

	FEngineShowFlags PerspectiveShowFlags(ESFIM_Editor);	
	ApplyViewMode(VMI_Lit, true, PerspectiveShowFlags);

	FString ViewportKey0, ViewportKey1;
	float SplitterPercentage = ViewportLayoutTwoPanesDefs::DefaultSplitterPercentage;

	if (!SpecificLayoutString.IsEmpty())
	{
		// The Layout String only holds the unique ID of the Additional Layout Configs to use
		const FString& IniSection = FLayoutSaveRestore::GetAdditionalLayoutConfigIni();

		ViewportKey0 = SpecificLayoutString + TEXT(".Viewport0");
		ViewportKey1 = SpecificLayoutString + TEXT(".Viewport1");


		FString PercentageString;
		if(GConfig->GetString(*IniSection, *(SpecificLayoutString + TEXT(".Percentage")), PercentageString, GEditorPerProjectIni))
		{
			TTypeFromString<float>::FromString(SplitterPercentage, *PercentageString);
		}						
	}

	SplitterWidget = 
		SNew( SSplitter )
		.Orientation(TOrientation)
		+SSplitter::Slot()
		.Value(SplitterPercentage)
		[
			SAssignNew( ViewportWidget0, SLevelViewport )
			.ViewportType( LVT_OrthoXY )	
			.ParentLayout( AsShared() )
			.ParentLevelEditor( ParentLevelEditor )
			.ConfigKey( ViewportKey0 )
		]
	+SSplitter::Slot()
		.Value(1.0f - SplitterPercentage)
		[
			SAssignNew( ViewportWidget1, SLevelViewport )
			.ViewportType( LVT_Perspective )
			.Realtime( true )
			.ParentLayout( AsShared() )
			.ParentLevelEditor( ParentLevelEditor )
			.ConfigKey( ViewportKey1 )
		];

	Viewports.Add( ViewportWidget0 );
	Viewports.Add( ViewportWidget1 );

	// Make newly-created perspective viewports active by default
	GCurrentLevelEditingViewportClient = &ViewportWidget1->GetLevelViewportClient();

	InitCommonLayoutFromString(SpecificLayoutString);

	return SplitterWidget.ToSharedRef();
}

template <EOrientation TOrientation>
void TLevelViewportLayoutTwoPanes<TOrientation>::SaveLayoutString(const FString& LayoutString) const
{
	if (!bIsTransitioning)
	{
		FString SpecificLayoutString = GetTypeSpecificLayoutString(LayoutString);

		const FString& IniSection = FLayoutSaveRestore::GetAdditionalLayoutConfigIni();

		check (SplitterWidget->GetChildren()->Num() == 2);
		float Percentage = SplitterWidget->SlotAt(0).SizeValue.Get();

		GConfig->SetString(*IniSection, *(SpecificLayoutString + TEXT(".Percentage")), *TTypeToString<float>::ToString(Percentage), GEditorPerProjectIni);

		SaveCommonLayoutString(SpecificLayoutString);
	}
}

template <EOrientation TOrientation>
void TLevelViewportLayoutTwoPanes<TOrientation>::ReplaceWidget( TSharedRef< SWidget > Source, TSharedRef< SWidget > Replacement )
{
	bool bWasFound = false;

	for (int32 SlotIdx = 0; SlotIdx < SplitterWidget->GetChildren()->Num(); SlotIdx++)
	{
		if  (SplitterWidget->GetChildren()->GetChildAt(SlotIdx) == Source)
		{
			SplitterWidget->SlotAt(SlotIdx)
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


/**
 * Function avoids linker errors on the template class functions in this cpp file.
 * It avoids the need to put the contents of this file into the header.
 * It doesn't get called.
 */
void ViewportLayoutTwoPanes_LinkErrorFixFunc()
{
	check(0);
	FLevelViewportLayoutTwoPanesVert DummyVert;
	FLevelViewportLayoutTwoPanesHoriz DummyHoriz;
}