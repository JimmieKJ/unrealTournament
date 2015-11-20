// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "SEditorViewportViewMenu.h"
#include "SViewportToolBar.h"
#include "SEditorViewport.h"
#include "EditorViewportCommands.h"

#define LOCTEXT_NAMESPACE "EditorViewportViewMenu"

void SEditorViewportViewMenu::Construct( const FArguments& InArgs, TSharedRef<SEditorViewport> InViewport, TSharedRef<class SViewportToolBar> InParentToolBar )
{
	Viewport = InViewport;
	MenuExtenders = InArgs._MenuExtenders;

	SEditorViewportToolbarMenu::Construct
	(
		SEditorViewportToolbarMenu::FArguments()
			.ParentToolBar( InParentToolBar)
			.Cursor( EMouseCursor::Default )
			.Label( this, &SEditorViewportViewMenu::GetViewMenuLabel )
			.LabelIcon( this, &SEditorViewportViewMenu::GetViewMenuLabelIcon )
			.OnGetMenuContent( this, &SEditorViewportViewMenu::GenerateViewMenuContent )
	);
		
}



FText SEditorViewportViewMenu::GetViewMenuLabel() const
{
	FText Label = LOCTEXT("ViewMenuTitle_Default", "View");
	TSharedPtr< SEditorViewport > PinnedViewport = Viewport.Pin();
	if( PinnedViewport.IsValid() )
	{
		switch( PinnedViewport->GetViewportClient()->GetViewMode() )
		{
			case VMI_BrushWireframe:
				Label = LOCTEXT("ViewMenuTitle_BrushWireframe", "Wireframe");
				break;

			case VMI_Wireframe:
				Label = LOCTEXT("ViewMenuTitle_Wireframe", "Wireframe");
				break;

			case VMI_Unlit:
				Label = LOCTEXT("ViewMenuTitle_Unlit", "Unlit");
				break;

			case VMI_Lit:
				Label = LOCTEXT("ViewMenuTitle_Lit", "Lit");
				break;

			case VMI_Lit_DetailLighting:
				Label = LOCTEXT("ViewMenuTitle_DetailLighting", "Detail Lighting");
				break;

			case VMI_LightingOnly:
				Label = LOCTEXT("ViewMenuTitle_LightingOnly", "Lighting Only");
				break;

			case VMI_LightComplexity:
				Label = LOCTEXT("ViewMenuTitle_LightComplexity", "Light Complexity");
				break;

			case VMI_ShaderComplexity:
				Label = LOCTEXT("ViewMenuTitle_ShaderComplexity", "Shader Complexity");
				break;

			case VMI_StationaryLightOverlap:
				Label = LOCTEXT("ViewMenuTitle_StationaryLightOverlap", "Stationary Light Overlap");
				break;

			case VMI_LightmapDensity:
				Label = LOCTEXT("ViewMenuTitle_LightmapDensity", "Lightmap Density");
				break;

			case VMI_ReflectionOverride:
				Label = LOCTEXT("ViewMenuTitle_ReflectionOverride", "Reflections");
				break;

			case VMI_VisualizeBuffer:
				Label = LOCTEXT("ViewMenuTitle_VisualizeBuffer", "Buffer Visualization");
				break;

			case VMI_CollisionPawn:
				Label = LOCTEXT("ViewMenuTitle_CollisionPawn", "Player Collision");
				break;

			case VMI_CollisionVisibility:
				Label = LOCTEXT("ViewMenuTitle_CollisionVisibility", "Visibility Collision");
				break;

			case VMI_LitLightmapDensity:
				break;

			case VMI_LODColoration:
				Label = LOCTEXT("ViewMenuTitle_LODColoration", "LOD Coloration");
				break;
		}
	}

	return Label;
}

const FSlateBrush* SEditorViewportViewMenu::GetViewMenuLabelIcon() const
{
	FName Icon = NAME_None;
	TSharedPtr< SEditorViewport > PinnedViewport = Viewport.Pin();
	if( PinnedViewport.IsValid() )
	{
		static FName WireframeIcon( "EditorViewport.WireframeMode" );
		static FName UnlitIcon( "EditorViewport.UnlitMode" );
		static FName LitIcon( "EditorViewport.LitMode" );
		static FName DetailLightingIcon("EditorViewport.DetailLightingMode");
		static FName LightingOnlyIcon("EditorViewport.LightingOnlyMode");
		static FName LightComplexityIcon("EditorViewport.LightComplexityMode");
		static FName ShaderComplexityIcon("EditorViewport.ShaderComplexityMode");
		static FName LightOverlapIcon("EditorViewport.StationaryLightOverlapMode");
		static FName LightmapDensityIcon("EditorViewport.LightmapDensityMode");
		static FName ReflectionModeIcon("EditorViewport.ReflectionOverrideMode");
		static FName LODColorationIcon("EditorViewport.LODColorationMode");
		static FName VisualizeBufferIcon("EditorViewport.VisualizeBufferMode");
		static FName CollisionPawnIcon("EditorViewport.CollisionPawn");
		static FName CollisionVisibilityIcon("EditorViewport.CollisionVisibility");

		switch( PinnedViewport->GetViewportClient()->GetViewMode() )
		{
			case VMI_BrushWireframe:
				Icon = WireframeIcon;
				break;

			case VMI_Wireframe:
				Icon = WireframeIcon;
				break;

			case VMI_Unlit:
				Icon = UnlitIcon;
				break;

			case VMI_Lit:
				Icon = LitIcon;
				break;

			case VMI_Lit_DetailLighting:
				Icon = DetailLightingIcon;
				break;

			case VMI_LightingOnly:
				Icon = LightingOnlyIcon;
				break;

			case VMI_LightComplexity:
				Icon = LightComplexityIcon;
				break;

			case VMI_ShaderComplexity:
				Icon = ShaderComplexityIcon;
				break;

			case VMI_StationaryLightOverlap:
				Icon = LightOverlapIcon;
				break;

			case VMI_LightmapDensity:
				Icon = LightmapDensityIcon;
				break;

			case VMI_ReflectionOverride:
				Icon = ReflectionModeIcon;
				break;

			case VMI_VisualizeBuffer:
				Icon = VisualizeBufferIcon;
				break;

			case VMI_CollisionPawn:
				Icon = CollisionPawnIcon;
				break;

			case VMI_CollisionVisibility:
				Icon = CollisionVisibilityIcon;
				break;

			case VMI_LitLightmapDensity:
				break;

			case VMI_LODColoration:
				Icon = LODColorationIcon;
				break;
		}
	}

	return FEditorStyle::GetBrush( Icon );
}


TSharedRef<SWidget> SEditorViewportViewMenu::GenerateViewMenuContent() const
{
	const FEditorViewportCommands& BaseViewportActions = FEditorViewportCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;

	FMenuBuilder ViewMenuBuilder( bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList(), MenuExtenders );
	{
		// View modes
		{
			ViewMenuBuilder.BeginSection("ViewMode", LOCTEXT("ViewModeHeader", "View Mode") );
			{
				ViewMenuBuilder.AddMenuEntry( BaseViewportActions.LitMode, NAME_None, LOCTEXT("LitViewModeDisplayName", "Lit") );
				ViewMenuBuilder.AddMenuEntry( BaseViewportActions.UnlitMode, NAME_None, LOCTEXT("UnlitViewModeDisplayName", "Unlit") );
				ViewMenuBuilder.AddMenuEntry( BaseViewportActions.WireframeMode, NAME_None, LOCTEXT("BrushWireframeViewModeDisplayName", "Wireframe") );
				ViewMenuBuilder.AddMenuEntry( BaseViewportActions.DetailLightingMode, NAME_None, LOCTEXT("DetailLightingViewModeDisplayName", "Detail Lighting") );
				ViewMenuBuilder.AddMenuEntry( BaseViewportActions.LightingOnlyMode, NAME_None, LOCTEXT("LightingOnlyViewModeDisplayName", "Lighting Only") );
				ViewMenuBuilder.AddMenuEntry( BaseViewportActions.LightComplexityMode, NAME_None, LOCTEXT("LightComplexityViewModeDisplayName", "Light Complexity") );
				ViewMenuBuilder.AddMenuEntry( BaseViewportActions.ShaderComplexityMode, NAME_None, LOCTEXT("ShaderComplexityViewModeDisplayName", "Shader Complexity") );
				ViewMenuBuilder.AddMenuEntry( BaseViewportActions.StationaryLightOverlapMode, NAME_None, LOCTEXT("StationaryLightOverlapViewModeDisplayName", "Stationary Light Overlap") );
				ViewMenuBuilder.AddMenuEntry( BaseViewportActions.LightmapDensityMode, NAME_None, LOCTEXT("LightmapDensityViewModeDisplayName", "Lightmap Density") );
				ViewMenuBuilder.AddMenuEntry( BaseViewportActions.ReflectionOverrideMode, NAME_None, LOCTEXT("ReflectionOverrideViewModeDisplayName", "Reflections") );
				ViewMenuBuilder.AddMenuEntry( BaseViewportActions.LODColorationMode, NAME_None, LOCTEXT("LODColorationViewModeDisplayName", "LOD Coloration") );
			}
			ViewMenuBuilder.EndSection();
		}

		// Auto Exposure
		{
			struct Local
			{
				static void BuildExposureMenu( FMenuBuilder& Menu )
				{
					const FEditorViewportCommands& BaseViewportCommands = FEditorViewportCommands::Get();

					Menu.AddMenuEntry( BaseViewportCommands.ToggleAutoExposure, NAME_None );
					Menu.AddMenuEntry( BaseViewportCommands.FixedExposure4m, NAME_None, LOCTEXT("FixedExposure4m", "Fixed at Log -4 = Brightness 1/16x") );
					Menu.AddMenuEntry( BaseViewportCommands.FixedExposure3m, NAME_None, LOCTEXT("FixedExposure3m", "Fixed at Log -3 = Brightness 1/8x") );
					Menu.AddMenuEntry( BaseViewportCommands.FixedExposure2m, NAME_None, LOCTEXT("FixedExposure2m", "Fixed at Log -2 = Brightness 1/4x") );
					Menu.AddMenuEntry( BaseViewportCommands.FixedExposure1m, NAME_None, LOCTEXT("FixedExposure1m", "Fixed at Log -1 = Brightness 1/2x") );
					Menu.AddMenuEntry( BaseViewportCommands.FixedExposure0,  NAME_None, LOCTEXT("FixedExposure0",  "Fixed at Log  0") );
					Menu.AddMenuEntry( BaseViewportCommands.FixedExposure1p, NAME_None, LOCTEXT("FixedExposure1p", "Fixed at Log +1 = Brightness 2x") );
					Menu.AddMenuEntry( BaseViewportCommands.FixedExposure2p, NAME_None, LOCTEXT("FixedExposure2p", "Fixed at Log +2 = Brightness 4x") );
					Menu.AddMenuEntry( BaseViewportCommands.FixedExposure3p, NAME_None, LOCTEXT("FixedExposure3p", "Fixed at Log +3 = Brightness 8x") );
					Menu.AddMenuEntry( BaseViewportCommands.FixedExposure4p, NAME_None, LOCTEXT("FixedExposure4p", "Fixed at Log +4 = Brightness 16x") );
				}
			};

			ViewMenuBuilder.BeginSection("Exposure");
			ViewMenuBuilder.AddSubMenu( LOCTEXT("ExposureSubMenu", "Exposure"), LOCTEXT("ExposureSubMenu_ToolTip", "Select exposure"), FNewMenuDelegate::CreateStatic( &Local::BuildExposureMenu ) );
			ViewMenuBuilder.EndSection();
		}
	}

	return ViewMenuBuilder.MakeWidget();
	
}

#undef LOCTEXT_NAMESPACE
