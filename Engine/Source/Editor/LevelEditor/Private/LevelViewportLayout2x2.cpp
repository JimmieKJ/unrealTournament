// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LevelEditor.h"
#include "LevelViewportLayout2x2.h"
#include "SLevelViewport.h"

namespace ViewportLayout2x2Defs
{
	/** Default 2x2 splitters to equal 50/50 splits */
	static const FVector2D DefaultSplitterPercentages(0.5f, 0.5f);
}

// FLevelViewportLayout2x2 //////////////////////////////////////////

TSharedRef<SWidget> FLevelViewportLayout2x2::MakeViewportLayout(const FString& LayoutString)
{
	FString SpecificLayoutString = GetTypeSpecificLayoutString(LayoutString);

	TSharedPtr< SLevelViewport > ViewportWidgetTL;
	TSharedPtr< SLevelViewport > ViewportWidgetTR;
	TSharedPtr< SLevelViewport > ViewportWidgetBL;
	TSharedPtr< SLevelViewport > ViewportWidgetBR;

	FString TopLeftKey, BottomLeftKey, TopRightKey, BottomRightKey;
	TArray<FVector2D> SplitterPercentages;
	
	if (!SpecificLayoutString.IsEmpty())
	{
		// The Layout String only holds the unique ID of the Additional Layout Configs to use
		const FString& IniSection = FLayoutSaveRestore::GetAdditionalLayoutConfigIni();
		
		TopLeftKey = SpecificLayoutString + TEXT(".Viewport0");
		BottomLeftKey = SpecificLayoutString + TEXT(".Viewport1");
		TopRightKey = SpecificLayoutString + TEXT(".Viewport2");
		BottomRightKey = SpecificLayoutString + TEXT(".Viewport3");

		for (int32 i = 0; i < 4; ++i)
		{
			FString PercentageString;
			FVector2D NewPercentage = ViewportLayout2x2Defs::DefaultSplitterPercentages;
			if(GConfig->GetString(*IniSection, *(SpecificLayoutString + FString::Printf(TEXT(".Percentages%i"), i)), PercentageString, GEditorPerProjectIni))
			{
				NewPercentage.InitFromString(PercentageString);
			}						
			SplitterPercentages.Add(NewPercentage);
		}
	}

	SplitterWidget = 
	SNew( SSplitter2x2 )
	.TopLeft()
	[
		SAssignNew( ViewportWidgetTL, SLevelViewport )
		.ViewportType( LVT_OrthoYZ )		// Left
		.ParentLayout( AsShared() )
		.ParentLevelEditor( ParentLevelEditor )
		.ConfigKey( TopLeftKey )
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
	]
	.BottomLeft()
	[
		SAssignNew( ViewportWidgetBL, SLevelViewport )
		.ViewportType( LVT_Perspective ) // Persp
		.Realtime( true )
		.ParentLayout( AsShared() )
		.ParentLevelEditor( ParentLevelEditor )
		.ConfigKey( BottomLeftKey )
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
	]
	.TopRight()
	[
		SAssignNew( ViewportWidgetTR, SLevelViewport )
		.ViewportType( LVT_OrthoXZ )		// Front
		.ParentLayout( AsShared() )
		.ParentLevelEditor( ParentLevelEditor )
		.ConfigKey( TopRightKey )
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
	]
	.BottomRight()
	[
		SAssignNew( ViewportWidgetBR, SLevelViewport )
		.ViewportType( LVT_OrthoXY )		// Top
		.ParentLayout( AsShared() )
		.ParentLevelEditor( ParentLevelEditor )
		.ConfigKey( BottomRightKey )
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
	];

	Viewports.Add( ViewportWidgetTL );
	Viewports.Add( ViewportWidgetBL );
	Viewports.Add( ViewportWidgetTR );
	Viewports.Add( ViewportWidgetBR );

	
	// Make newly-created perspective viewports active by default
	GCurrentLevelEditingViewportClient = &ViewportWidgetBL->GetLevelViewportClient();

	
	if (SplitterPercentages.Num() > 0)
	{
		SplitterWidget->SetSplitterPercentages(SplitterPercentages);
	}

	InitCommonLayoutFromString(SpecificLayoutString);

	return SplitterWidget.ToSharedRef();
}



void FLevelViewportLayout2x2::SaveLayoutString(const FString& LayoutString) const
{
	if (!bIsTransitioning)
	{
		FString SpecificLayoutString = GetTypeSpecificLayoutString(LayoutString);

		const FString& IniSection = FLayoutSaveRestore::GetAdditionalLayoutConfigIni();

		TArray<FVector2D> Percentages;
		SplitterWidget->GetSplitterPercentages(Percentages);
		for (int32 i = 0; i < Percentages.Num(); ++i)
		{
			GConfig->SetString(*IniSection, *(SpecificLayoutString + FString::Printf(TEXT(".Percentages%i"), i)), *Percentages[i].ToString(), GEditorPerProjectIni);
		}

		SaveCommonLayoutString(SpecificLayoutString);
	}
}


void FLevelViewportLayout2x2::ReplaceWidget( TSharedRef< SWidget > Source, TSharedRef< SWidget > Replacement )
{
	bool bWasFound = false;

	if( SplitterWidget->GetTopLeftContent() == Source )
	{
		SplitterWidget->SetTopLeftContent( Replacement );
		bWasFound = true;
	}

	else if( SplitterWidget->GetBottomLeftContent() == Source )
	{
		SplitterWidget->SetBottomLeftContent( Replacement );
		bWasFound = true;
	}

	else if( SplitterWidget->GetTopRightContent() == Source )
	{
		SplitterWidget->SetTopRightContent( Replacement );
		bWasFound = true;
	}

	else if( SplitterWidget->GetBottomRightContent() == Source )
	{
		SplitterWidget->SetBottomRightContent( Replacement );
		bWasFound = true;
	}

	// Source widget should have already been a content widget for the splitter
	check( bWasFound );
}
