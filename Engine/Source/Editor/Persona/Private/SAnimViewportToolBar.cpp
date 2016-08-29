// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"
#include "SAnimViewportToolBar.h"
#include "SAnimationEditorViewport.h"
#include "EditorViewportCommands.h"
#include "AnimViewportMenuCommands.h"
#include "AnimViewportShowCommands.h"
#include "AnimViewportLODCommands.h"
#include "AnimViewportPlaybackCommands.h"
#include "SAnimPlusMinusSlider.h"
#include "AnimationEditorViewportClient.h"
#include "Editor/UnrealEd/Public/SEditorViewportToolBarMenu.h"
#include "Editor/UnrealEd/Public/STransformViewportToolbar.h"
#include "SEditorViewportViewMenu.h"
#include "SColorPicker.h"
#include "SNumericEntryBox.h"
#include "STextComboBox.h"

#include "AssetViewerSettings.h"


#define LOCTEXT_NAMESPACE "AnimViewportToolBar"


//Class definition which represents widget to modify viewport's background color.
class SBackgroundColorSettings : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SBackgroundColorSettings)
		{}
		SLATE_ARGUMENT( TWeakPtr<SAnimationEditorViewportTabBody>, AnimEditorViewport )
	SLATE_END_ARGS()

	/** Constructs this widget from its declaration */
	void Construct(const FArguments& InArgs )
	{
		AnimViewportPtr = InArgs._AnimEditorViewport;

		TSharedPtr<SWidget> ExtraWidget = 
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("FilledBorder"))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(1)
				[
					SNew( SColorBlock )
					.Color(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::GetViewportBackgroundColor)
					.IgnoreAlpha(true)
					.ToolTipText(LOCTEXT("ColorBlock_ToolTip", "Select background color"))
					.OnMouseButtonDown(this, &SBackgroundColorSettings::OnColorBoxClicked)
				]
			];

		this->ChildSlot
		[
			SNew(SAnimPlusMinusSlider)
				.Label( LOCTEXT("BrightNess", "Brightness:")  )
				.IsEnabled( this, &SBackgroundColorSettings::IsBrightnessSliderEnabled )
				.OnMinusClicked( this, &SBackgroundColorSettings::OnDecreaseBrightness )
				.MinusTooltip( LOCTEXT("DecreaseBrightness_ToolTip", "Decrease brightness") )
				.SliderValue(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::GetBackgroundBrightness)
				.OnSliderValueChanged(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::SetBackgroundBrightness)
				.SliderTooltip( LOCTEXT("BackgroundBrightness_ToolTip", "Change background brightness") )
				.OnPlusClicked( this, &SBackgroundColorSettings::OnIncreaseBrightness )
				.PlusTooltip( LOCTEXT("IncreaseBrightness_ToolTip", "Increase brightness") )
				.ExtraWidget( ExtraWidget )
		];
	}

protected:

	/** Function to open color picker window when selected from context menu */
	FReply OnColorBoxClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return FReply::Unhandled();
		}

		FSlateApplication::Get().DismissAllMenus();

		FLinearColor BackgroundColor = AnimViewportPtr.Pin()->GetViewportBackgroundColor();
		TArray<FLinearColor*> LinearColorArray;
		LinearColorArray.Add(&BackgroundColor);

		FColorPickerArgs PickerArgs;
		PickerArgs.bIsModal = true;
		PickerArgs.ParentWidget = AnimViewportPtr.Pin();
		PickerArgs.bOnlyRefreshOnOk = true;
		PickerArgs.DisplayGamma = TAttribute<float>::Create( TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma) );
		PickerArgs.LinearColorArray = &LinearColorArray;
		PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::SetViewportBackgroundColor);

		if (OpenColorPicker(PickerArgs))
		{
			AnimViewportPtr.Pin()->RefreshViewport();
		}

		return FReply::Handled();
	}

	/** Callback function for decreasing color brightness */
	FReply OnDecreaseBrightness( )
	{
		const float DeltaValue = 0.05f;
		AnimViewportPtr.Pin()->SetBackgroundBrightness( AnimViewportPtr.Pin()->GetBackgroundBrightness() - DeltaValue );
		return FReply::Handled();
	}

	/** Callback function for increasing color brightness */
	FReply OnIncreaseBrightness( )
	{
		const float DeltaValue = 0.05f;
		AnimViewportPtr.Pin()->SetBackgroundBrightness( AnimViewportPtr.Pin()->GetBackgroundBrightness() + DeltaValue );
		return FReply::Handled();
	}

	/** Callback function which determines whether this widget is enabled */
	bool IsBrightnessSliderEnabled() const
	{
		const UAssetViewerSettings* Settings = UAssetViewerSettings::Get();
		const UEditorPerProjectUserSettings* PerProjectUserSettings = GetDefault<UEditorPerProjectUserSettings>();
		const int32 ProfileIndex = Settings->Profiles.IsValidIndex(PerProjectUserSettings->AssetViewerProfileIndex) ? PerProjectUserSettings->AssetViewerProfileIndex : 0;

		ensureMsgf(Settings && Settings->Profiles.IsValidIndex(PerProjectUserSettings->AssetViewerProfileIndex), TEXT("Invalid default settings pointer or current profile index"));
		
		return !Settings->Profiles[ProfileIndex].bShowEnvironment;
	}

protected:

	/** The viewport hosting this widget */
	TWeakPtr<SAnimationEditorViewportTabBody> AnimViewportPtr;
};

//Class definition which represents widget to modify strength of wind for clothing
class SClothWindSettings : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SClothWindSettings)
	{}

		SLATE_ARGUMENT(TWeakPtr<SAnimationEditorViewportTabBody>, AnimEditorViewport)
	SLATE_END_ARGS()

	/** Constructs this widget from its declaration */
	void Construct(const FArguments& InArgs )
	{
		AnimViewportPtr = InArgs._AnimEditorViewport;

		TSharedPtr<SWidget> ExtraWidget = SNew( STextBlock )
			.Text(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::GetWindStrengthLabel)
			.Font( FEditorStyle::GetFontStyle( TEXT( "MenuItem.Font" ) ) );

		this->ChildSlot
			[
				SNew(SAnimPlusMinusSlider)
				.IsEnabled( this, &SClothWindSettings::IsWindEnabled ) 
				.Label( LOCTEXT("WindStrength", "Wind Strength:") )
				.OnMinusClicked( this, &SClothWindSettings::OnDecreaseWindStrength )
				.MinusTooltip( LOCTEXT("DecreaseWindStrength_ToolTip", "Decrease Wind Strength") )
				.SliderValue(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::GetWindStrengthSliderValue)
				.OnSliderValueChanged(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::SetWindStrength)
				.SliderTooltip( LOCTEXT("WindStrength_ToolTip", "Change wind strength") )
				.OnPlusClicked( this, &SClothWindSettings::OnIncreaseWindStrength )
				.PlusTooltip( LOCTEXT("IncreasetWindStrength_ToolTip", "Increase Wind Strength") )
				.ExtraWidget( ExtraWidget )
			];
	}

protected:
	/** Callback function for decreasing size */
	FReply OnDecreaseWindStrength()
	{
		const float DeltaValue = 0.1f;
		AnimViewportPtr.Pin()->SetWindStrength( AnimViewportPtr.Pin()->GetWindStrengthSliderValue() - DeltaValue );
		return FReply::Handled();
	}

	/** Callback function for increasing size */
	FReply OnIncreaseWindStrength()
	{
		const float DeltaValue = 0.1f;
		AnimViewportPtr.Pin()->SetWindStrength( AnimViewportPtr.Pin()->GetWindStrengthSliderValue() + DeltaValue );
		return FReply::Handled();
	}

	/** Callback function which determines whether this widget is enabled */
	bool IsWindEnabled() const
	{
		return AnimViewportPtr.Pin()->IsApplyingClothWind();
	}

protected:
	/** The viewport hosting this widget */
	TWeakPtr<SAnimationEditorViewportTabBody> AnimViewportPtr;
};

//Class definition which represents widget to modify gravity for preview
class SGravitySettings : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SGravitySettings)
	{}

		SLATE_ARGUMENT(TWeakPtr<SAnimationEditorViewportTabBody>, AnimEditorViewport)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs )
	{
		AnimViewportPtr = InArgs._AnimEditorViewport;

		TSharedPtr<SWidget> ExtraWidget = SNew( STextBlock )
			.Text(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::GetGravityScaleLabel)
			.Font( FEditorStyle::GetFontStyle( TEXT( "MenuItem.Font" ) ) );

		this->ChildSlot
			[
				SNew(SAnimPlusMinusSlider)
				.Label( LOCTEXT("Gravity Scale", "Gravity Scale Preview:")  )
				.OnMinusClicked( this, &SGravitySettings::OnDecreaseGravityScale )
				.MinusTooltip( LOCTEXT("DecreaseGravitySize_ToolTip", "Decrease Gravity Scale") )
				.SliderValue(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::GetGravityScaleSliderValue)
				.OnSliderValueChanged(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::SetGravityScale)
				.SliderTooltip( LOCTEXT("GravityScale_ToolTip", "Change Gravity Scale") )
				.OnPlusClicked( this, &SGravitySettings::OnIncreaseGravityScale )
				.PlusTooltip( LOCTEXT("IncreaseGravityScale_ToolTip", "Increase Gravity Scale") )
				.ExtraWidget( ExtraWidget )
			];
	}

protected:
	FReply OnDecreaseGravityScale()
	{
		const float DeltaValue = 0.025f;
		AnimViewportPtr.Pin()->SetGravityScale( AnimViewportPtr.Pin()->GetGravityScaleSliderValue() - DeltaValue );
		return FReply::Handled();
	}

	FReply OnIncreaseGravityScale()
	{
		const float DeltaValue = 0.025f;
		AnimViewportPtr.Pin()->SetGravityScale( AnimViewportPtr.Pin()->GetGravityScaleSliderValue() + DeltaValue );
		return FReply::Handled();
	}

protected:
	TWeakPtr<SAnimationEditorViewportTabBody> AnimViewportPtr;
};

///////////////////////////////////////////////////////////
// SAnimViewportToolBar

void SAnimViewportToolBar::Construct(const FArguments& InArgs, TSharedPtr<class SAnimationEditorViewportTabBody> InViewport, TSharedPtr<class SEditorViewport> InRealViewport)
{
	Viewport = InViewport;

	TSharedRef<SAnimationEditorViewportTabBody> ViewportRef = Viewport.Pin().ToSharedRef();

	TSharedRef<SHorizontalBox> LeftToolbar = SNew(SHorizontalBox)
			// Generic viewport options
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 2.0f, 2.0f )
			[
				//Menu
				SNew( SEditorViewportToolbarMenu )
				.ParentToolBar( SharedThis( this ) )
				.Image("EditorViewportToolBar.MenuDropdown")
//					.Label( this, &SAnimViewportToolBar::GetViewMenuLabel )
				.OnGetMenuContent( this, &SAnimViewportToolBar::GenerateViewMenu ) 
			]

			// Camera Type (Perspective/Top/etc...)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 2.0f, 2.0f )
			[
				SNew( SEditorViewportToolbarMenu )
				.ParentToolBar( SharedThis( this ) )
				.Label(this, &SAnimViewportToolBar::GetCameraMenuLabel)
				.LabelIcon(this, &SAnimViewportToolBar::GetCameraMenuLabelIcon)
				.OnGetMenuContent(this, &SAnimViewportToolBar::GenerateViewportTypeMenu)
			]

			// View menu (lit, unlit, etc...)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 2.0f)
			[
				SNew( SEditorViewportViewMenu, InRealViewport.ToSharedRef(), SharedThis(this) )
			]

			// Show flags menu
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 2.0f, 2.0f )
			[
				SNew( SEditorViewportToolbarMenu )
				.ParentToolBar( SharedThis( this ) )
				.Label( LOCTEXT("ShowMenu", "Show") )
				.OnGetMenuContent( this, &SAnimViewportToolBar::GenerateShowMenu ) 
			]

			// LOD menu
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 2.0f, 2.0f )
			[
				//LOD
				SNew( SEditorViewportToolbarMenu )
				.ParentToolBar( SharedThis( this ) )
				.Label( this, &SAnimViewportToolBar::GetLODMenuLabel )
				.OnGetMenuContent( this, &SAnimViewportToolBar::GenerateLODMenu ) 
			]

			// Playback speed menu
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 2.0f, 2.0f )
			[
				SNew( SEditorViewportToolbarMenu )
				.ParentToolBar( SharedThis( this ) )
				.Label( this, &SAnimViewportToolBar::GetPlaybackMenuLabel )
				.LabelIcon(FEditorStyle::GetBrush("AnimViewportMenu.PlayBackSpeed"))
				.OnGetMenuContent( this, &SAnimViewportToolBar::GeneratePlaybackMenu ) 
			]

			// Turn table menu
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 2.0f)
			[
				SNew(SEditorViewportToolbarMenu)
				.ParentToolBar(SharedThis(this))
				.Label(this, &SAnimViewportToolBar::GetTurnTableMenuLabel)
				.LabelIcon(FEditorStyle::GetBrush("AnimViewportMenu.TurnTableSpeed"))
				.OnGetMenuContent(this, &SAnimViewportToolBar::GenerateTurnTableMenu)
			]
				
 			+SHorizontalBox::Slot()
			.Padding(3.0f, 1.0f)
			.HAlign(HAlign_Right)
			[
				SNew(STransformViewportToolBar)
				.Viewport(InRealViewport)
				.CommandList(InRealViewport->GetCommandList())
				.Visibility(this, &SAnimViewportToolBar::GetTransformToolbarVisibility)
			];
	//@TODO: Need clipping horizontal box: LeftToolbar->AddWrapButton();

	static const FName DefaultForegroundName("DefaultForeground");

	ChildSlot
	[
		SNew( SBorder )
		.BorderImage( FEditorStyle::GetBrush("NoBorder") )
		// Color and opacity is changed based on whether or not the mouse cursor is hovering over the toolbar area
		.ColorAndOpacity( this, &SViewportToolBar::OnGetColorAndOpacity )
		.ForegroundColor( FEditorStyle::GetSlateColor(DefaultForegroundName) )
		[
			SNew( SVerticalBox )
			+ SVerticalBox::Slot()
			.AutoHeight()
			[

				LeftToolbar
			]
			+SVerticalBox::Slot()
			.Padding(FMargin(4.0f, 3.0f, 0.0f, 0.0f))
			[
				// Display text (e.g., item being previewed)
				SNew(STextBlock)
				.Text(ViewportRef, &SAnimationEditorViewportTabBody::GetDisplayString)
				.Font(FEditorStyle::GetFontStyle(TEXT("AnimViewport.MessageFont")))
				.ShadowOffset(FVector2D(0.5f, 0.5f))
				.ShadowColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.3f))
				.ColorAndOpacity(this, &SAnimViewportToolBar::GetFontColor)
			]
		]
	];

	SViewportToolBar::Construct(SViewportToolBar::FArguments());
}

EVisibility SAnimViewportToolBar::GetTransformToolbarVisibility() const
{
	return Viewport.Pin()->CanUseGizmos() ? EVisibility::Visible : EVisibility::Hidden;
}

FText SAnimViewportToolBar::GetViewMenuLabel() const
{
	FText Label = LOCTEXT("ViewMenu_AutoLabel", "Menu");
	if (Viewport.IsValid())
	{
		// lock mode on
		if (Viewport.Pin()->IsPreviewModeOn(1))
		{
			Label = LOCTEXT("ViewMenu_LockLabel", "Lock");
		}
	}

	return Label;
}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateViewMenu() const
{
	const FAnimViewportMenuCommands& Actions = FAnimViewportMenuCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ViewMenuBuilder( bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList() );
	{
		// View modes
		{
			ViewMenuBuilder.BeginSection("AnimViewportPreviewMode", LOCTEXT("ViewMenu_PreviewModeLabel", "Preview Mode") );
			{
				ViewMenuBuilder.AddMenuEntry( Actions.Auto );
				ViewMenuBuilder.AddMenuEntry( Actions.Lock );
			}
			ViewMenuBuilder.EndSection();

			ViewMenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().CameraFollow);

			ViewMenuBuilder.BeginSection("AnimViewportPreview", LOCTEXT("ViewMenu_PreviewLabel", "Preview") );
			{
				ViewMenuBuilder.AddMenuEntry( Actions.UseInGameBound );
			}
			ViewMenuBuilder.EndSection();
		}
	}

	return ViewMenuBuilder.MakeWidget();

}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateShowMenu() const
{
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList());
	{
			ShowMenuBuilder.BeginSection("AnimViewportFOV", LOCTEXT("Viewport_FOVLabel", "Field Of View"));
			{
				const float FOVMin = 5.f;
				const float FOVMax = 170.f;

				TSharedPtr<SWidget> FOVWidget = SNew(SSpinBox<float>)
					.Font(FEditorStyle::GetFontStyle(TEXT("MenuItem.Font")))
					.MinValue(FOVMin)
					.MaxValue(FOVMax)
					.Value(this, &SAnimViewportToolBar::OnGetFOVValue)
					.OnValueChanged(this, &SAnimViewportToolBar::OnFOVValueChanged)
					.OnValueCommitted(this, &SAnimViewportToolBar::OnFOVValueCommitted);

				ShowMenuBuilder.AddWidget(FOVWidget.ToSharedRef(), FText());
			}
			ShowMenuBuilder.EndSection();

			ShowMenuBuilder.BeginSection("AnimViewportAudio", LOCTEXT("Viewport_AudioLabel", "Audio"));
			{
				ShowMenuBuilder.AddMenuEntry(Actions.MuteAudio);
			}
			ShowMenuBuilder.EndSection();

			ShowMenuBuilder.BeginSection("AnimViewportRootMotion", LOCTEXT("Viewport_RootMotionLabel", "Root Motion"));
			{
				ShowMenuBuilder.AddMenuEntry(Actions.ProcessRootMotion);
			}
			ShowMenuBuilder.EndSection();

			ShowMenuBuilder.BeginSection("AnimViewportMesh", LOCTEXT("ShowMenu_Actions_Mesh", "Mesh"));
			{
				ShowMenuBuilder.AddMenuEntry( Actions.ShowReferencePose );
				ShowMenuBuilder.AddMenuEntry( Actions.ShowRetargetBasePose );
				ShowMenuBuilder.AddMenuEntry( Actions.ShowBound );
				ShowMenuBuilder.AddMenuEntry( Actions.ShowPreviewMesh );
				ShowMenuBuilder.AddMenuEntry( Actions.ShowMorphTargets );
			}
			ShowMenuBuilder.EndSection();

			ShowMenuBuilder.BeginSection("AnimViewportAnimation", LOCTEXT("ShowMenu_Actions_Asset", "Asset"));
			{
				ShowMenuBuilder.AddMenuEntry( Actions.ShowRawAnimation );
				ShowMenuBuilder.AddMenuEntry( Actions.ShowNonRetargetedAnimation );
				ShowMenuBuilder.AddMenuEntry( Actions.ShowAdditiveBaseBones );
				ShowMenuBuilder.AddMenuEntry( Actions.ShowSourceRawAnimation);
				ShowMenuBuilder.AddMenuEntry( Actions.ShowBakedAnimation );
			}
			ShowMenuBuilder.EndSection();

			ShowMenuBuilder.BeginSection("AnimViewportPreviewBones", LOCTEXT("ShowMenu_Actions_Bones", "Hierarchy") );
			{
				ShowMenuBuilder.AddSubMenu(
					LOCTEXT("AnimViewportBoneDrawSubMenu", "Bone"),
					LOCTEXT("AnimViewportBoneDrawSubMenuToolTip", "Bone Draw Option"),
					FNewMenuDelegate::CreateRaw(this, &SAnimViewportToolBar::FillShowBoneDrawMenu)
					);

				ShowMenuBuilder.AddMenuEntry( Actions.ShowSockets );
				ShowMenuBuilder.AddMenuEntry( Actions.ShowBoneNames );
				ShowMenuBuilder.AddMenuEntry( Actions.ShowBoneWeight );
			}
			ShowMenuBuilder.EndSection();

			ShowMenuBuilder.AddMenuSeparator();
			ShowMenuBuilder.AddSubMenu(
				LOCTEXT("AnimviewportInfo", "Display Info"),
				LOCTEXT("AnimviewportInfoSubMenuToolTip", "Display Mesh Info in Viewport"),
				FNewMenuDelegate::CreateRaw(this, &SAnimViewportToolBar::FillShowDisplayInfoMenu));

#if WITH_APEX_CLOTHING
			UDebugSkelMeshComponent* PreviewComp = Viewport.Pin()->GetPersona().Pin()->PreviewComponent;

			if( PreviewComp && PreviewComp->HasValidClothingActors() )
			{
				ShowMenuBuilder.AddMenuSeparator();
				ShowMenuBuilder.AddSubMenu(
					LOCTEXT("AnimViewportClothingSubMenu", "Clothing"),
					LOCTEXT("AnimViewportClothingSubMenuToolTip", "Options relating to clothing"),
					FNewMenuDelegate::CreateRaw(this, &SAnimViewportToolBar::FillShowClothingMenu));
			}
			else // if skeletal mesh has clothing assets without mapping yet or assets have only collision volumes without clothing sections, then need to show collision volumes which assets include 
			if( PreviewComp && PreviewComp->SkeletalMesh && PreviewComp->SkeletalMesh->ClothingAssets.Num() > 0)
			{				
				ShowMenuBuilder.BeginSection("AnimViewportClothingOptions", LOCTEXT("ShowMenu_Actions_Clothing", "Clothing") );
				{
					ShowMenuBuilder.AddMenuEntry( Actions.ShowClothCollisionVolumes );
				}
				ShowMenuBuilder.EndSection();
			}
#endif // #if WITH_APEX_CLOTHING

		ShowMenuBuilder.AddMenuSeparator();

		ShowMenuBuilder.AddSubMenu(
			LOCTEXT("AnimViewportSceneSubMenu", "Scene Setup"),
			LOCTEXT("AnimViewportSceneSubMenuToolTip", "Options relating to the preview scene"),
			FNewMenuDelegate::CreateRaw( this, &SAnimViewportToolBar::FillShowSceneMenu ) );

		ShowMenuBuilder.AddSubMenu(
			LOCTEXT("AnimViewportAdvancedSubMenu", "Advanced"),
			LOCTEXT("AnimViewportAdvancedSubMenuToolTip", "Advanced options"),
			FNewMenuDelegate::CreateRaw( this, &SAnimViewportToolBar::FillShowAdvancedMenu ) );
	}

	return ShowMenuBuilder.MakeWidget();

}

void SAnimViewportToolBar::FillShowSceneMenu(FMenuBuilder& MenuBuilder) const
{
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();

	MenuBuilder.BeginSection("AnimViewportAccessory", LOCTEXT("Viewport_AccessoryLabel", "Accessory"));
	{
		MenuBuilder.AddMenuEntry(Actions.AutoAlignFloorToMesh);
	}
	MenuBuilder.EndSection();
	
	MenuBuilder.BeginSection("AnimViewportFloorOffset", LOCTEXT("Viewport_FloorOffsetLabel", "Floor Height Offset"));
	{
		TSharedPtr<SWidget> FloorOffsetWidget = SNew(SNumericEntryBox<float>)
			.Font(FEditorStyle::GetFontStyle(TEXT("MenuItem.Font")))
			.Value(this, &SAnimViewportToolBar::OnGetFloorOffset)
			.OnValueChanged(this, &SAnimViewportToolBar::OnFloorOffsetChanged)
			.ToolTipText(LOCTEXT("FloorOffsetToolTip", "Height offset for the floor mesh (stored per-mesh)"));

		MenuBuilder.AddWidget(FloorOffsetWidget.ToSharedRef(), FText());
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AnimViewportGrid", LOCTEXT("Viewport_GridLabel", "Grid"));
	{
		MenuBuilder.AddMenuEntry(Actions.ToggleGrid);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AnimViewportBackground", LOCTEXT("Viewport_BackgroundLabel", "Background"));
	{
		TSharedPtr<SWidget> BackgroundColorWidget = SNew(SBackgroundColorSettings).AnimEditorViewport(Viewport);
		MenuBuilder.AddWidget(BackgroundColorWidget.ToSharedRef(), FText());
	}
	MenuBuilder.EndSection();
}

void SAnimViewportToolBar::FillShowBoneDrawMenu(FMenuBuilder& MenuBuilder) const
{
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();

	MenuBuilder.BeginSection("AnimViewportPreviewHierarchyBoneDraw", LOCTEXT("ShowMenu_Actions_HierarchyAxes", "Hierarchy Local Axes"));
	{
		MenuBuilder.AddMenuEntry(Actions.ShowBoneDrawAll);
		MenuBuilder.AddMenuEntry(Actions.ShowBoneDrawSelected);
		MenuBuilder.AddMenuEntry(Actions.ShowBoneDrawNone);
	}
	MenuBuilder.EndSection();
}

void SAnimViewportToolBar::FillShowAdvancedMenu(FMenuBuilder& MenuBuilder) const
{
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();

	// Draw UVs
	MenuBuilder.BeginSection("UVVisualization", LOCTEXT("UVVisualization_Label", "UV Visualization"));
	{
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().AnimSetDrawUVs);
		MenuBuilder.AddWidget(Viewport.Pin()->UVChannelCombo.ToSharedRef(), FText());
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Skinning", LOCTEXT("Skinning_Label", "Skinning"));
	{
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetCPUSkinning);
	}
	MenuBuilder.EndSection();
	

	MenuBuilder.BeginSection("ShowVertex", LOCTEXT("ShowVertex_Label", "Vertex Normal Visualization"));
	{
		// Vertex debug flags
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetShowNormals);
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetShowTangents);
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetShowBinormals);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AnimViewportPreviewHierarchyLocalAxes", LOCTEXT("ShowMenu_Actions_HierarchyAxes", "Hierarchy Local Axes") );
	{
		MenuBuilder.AddMenuEntry( Actions.ShowLocalAxesAll );
		MenuBuilder.AddMenuEntry( Actions.ShowLocalAxesSelected );
		MenuBuilder.AddMenuEntry( Actions.ShowLocalAxesNone );
	}
	MenuBuilder.EndSection();
}

void SAnimViewportToolBar::FillShowClothingMenu(FMenuBuilder& MenuBuilder) const
{
#if WITH_APEX_CLOTHING
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();

	MenuBuilder.BeginSection("ClothPreview", LOCTEXT("ClothPreview_Label", "Preview"));
	{
		MenuBuilder.AddMenuEntry(Actions.DisableClothSimulation);
		MenuBuilder.AddMenuEntry(Actions.ApplyClothWind);
		TSharedPtr<SWidget> WindWidget = SNew(SClothWindSettings).AnimEditorViewport(Viewport);
		MenuBuilder.AddWidget(WindWidget.ToSharedRef(), FText());
		TSharedPtr<SWidget> GravityWidget = SNew(SGravitySettings).AnimEditorViewport(Viewport);
		MenuBuilder.AddWidget(GravityWidget.ToSharedRef(), FText());
		MenuBuilder.AddMenuEntry(Actions.EnableCollisionWithAttachedClothChildren);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("ClothNormalVisualization", LOCTEXT("ClothNormalVisualization_Label", "Normal Visualization"));
	{
		MenuBuilder.AddMenuEntry(Actions.ShowClothSimulationNormals);
		MenuBuilder.AddMenuEntry(Actions.ShowClothGraphicalTangents);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("ClothConstraintsVisualization", LOCTEXT("ClothConstraintsVisualization_Label", "Constraints Visualization"));
	{
		MenuBuilder.AddMenuEntry(Actions.ShowClothCollisionVolumes);
		MenuBuilder.AddMenuEntry(Actions.ShowClothPhysicalMeshWire);
		MenuBuilder.AddMenuEntry(Actions.ShowClothMaxDistances);
		MenuBuilder.AddMenuEntry(Actions.ShowClothBackstop);
		MenuBuilder.AddMenuEntry(Actions.ShowClothFixedVertices);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("ClothAdditionalVisualization", LOCTEXT("ClothAdditionalVisualization_Label", "Sections Display Mode"));
	{
		MenuBuilder.AddMenuEntry(Actions.ShowAllSections);
		MenuBuilder.AddMenuEntry(Actions.ShowOnlyClothSections);
		MenuBuilder.AddMenuEntry(Actions.HideOnlyClothSections);
	}
	MenuBuilder.EndSection();
#endif // #if WITH_APEX_CLOTHING
}

void SAnimViewportToolBar::FillShowDisplayInfoMenu(FMenuBuilder& MenuBuilder) const
{
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();

	// display info levels
	{
		MenuBuilder.AddMenuEntry(Actions.ShowDisplayInfoBasic);
		MenuBuilder.AddMenuEntry(Actions.ShowDisplayInfoDetailed);
		MenuBuilder.AddMenuEntry(Actions.ShowDisplayInfoSkelControls);
		MenuBuilder.AddMenuEntry(Actions.HideDisplayInfo);
	}
}

FText SAnimViewportToolBar::GetLODMenuLabel() const
{
	FText Label = LOCTEXT("LODMenu_AutoLabel", "LOD Auto");
	if (Viewport.IsValid())
	{
		int32 LODSelectionType = Viewport.Pin()->GetLODSelection();

		if (LODSelectionType > 0)
		{
			FString TitleLabel = FString::Printf(TEXT("LOD %d"), LODSelectionType - 1);
			Label = FText::FromString(TitleLabel);
		}
	}
	return Label;
}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateLODMenu() const
{
	const FAnimViewportLODCommands& Actions = FAnimViewportLODCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ShowMenuBuilder( bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList() );
	{
		// LOD Models
		ShowMenuBuilder.BeginSection("AnimViewportPreviewLODs", LOCTEXT("ShowLOD_PreviewLabel", "Preview LODs") );
		{
			ShowMenuBuilder.AddMenuEntry( Actions.LODAuto );
			ShowMenuBuilder.AddMenuEntry( Actions.LOD0 );

			int32 LODCount = Viewport.Pin()->GetLODModelCount();
			for (int32 LODId = 1; LODId < LODCount; ++LODId)
			{
				FString TitleLabel = FString::Printf(TEXT("LOD %d"), LODId);

				FUIAction Action(FExecuteAction::CreateSP(Viewport.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::OnSetLODModel, LODId + 1),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(Viewport.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::IsLODModelSelected, LODId + 1));

				ShowMenuBuilder.AddMenuEntry(FText::FromString(TitleLabel), FText::GetEmpty(), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::RadioButton);
			}
		}
		ShowMenuBuilder.EndSection();
	}

	return ShowMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateViewportTypeMenu() const
{
	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder CameraMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetViewportWidget()->GetCommandList());

	// Camera types
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Perspective);

	CameraMenuBuilder.BeginSection("LevelViewportCameraType_Ortho", LOCTEXT("CameraTypeHeader_Ortho", "Orthographic"));
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Top);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Bottom);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Left);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Right);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Front);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Back);
	CameraMenuBuilder.EndSection();

	return CameraMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SAnimViewportToolBar::GeneratePlaybackMenu() const
{
	const FAnimViewportPlaybackCommands& Actions = FAnimViewportPlaybackCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;

	FMenuBuilder PlaybackMenuBuilder( bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList() );
	{
		// View modes
		{
			PlaybackMenuBuilder.BeginSection("AnimViewportPlaybackSpeed", LOCTEXT("PlaybackMenu_SpeedLabel", "Playback Speed") );
			{
				for(int i = 0; i < EAnimationPlaybackSpeeds::NumPlaybackSpeeds; ++i)
				{
					PlaybackMenuBuilder.AddMenuEntry( Actions.PlaybackSpeedCommands[i] );
				}
			}
			PlaybackMenuBuilder.EndSection();
		}
	}

	return PlaybackMenuBuilder.MakeWidget();

}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateTurnTableMenu() const
{
	const FAnimViewportPlaybackCommands& Actions = FAnimViewportPlaybackCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;

	FMenuBuilder TurnTableMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList());
	{
		TurnTableMenuBuilder.BeginSection("AnimViewportTurnTableMode", LOCTEXT("TurnTableMenu_ModeLabel", "Turn Table Mode"));
		{
			TurnTableMenuBuilder.AddMenuEntry(Actions.PersonaTurnTablePlay);
			TurnTableMenuBuilder.AddMenuEntry(Actions.PersonaTurnTablePause);
			TurnTableMenuBuilder.AddMenuEntry(Actions.PersonaTurnTableStop);
		}
		TurnTableMenuBuilder.EndSection();

		TurnTableMenuBuilder.BeginSection("AnimViewportTurnTableSpeed", LOCTEXT("TurnTableMenu_SpeedLabel", "Turn Table Speed"));
		{
			for (int i = 0; i < EAnimationPlaybackSpeeds::NumPlaybackSpeeds; ++i)
			{
				TurnTableMenuBuilder.AddMenuEntry(Actions.TurnTableSpeeds[i]);
			}
		}
		TurnTableMenuBuilder.EndSection();
	}

	return TurnTableMenuBuilder.MakeWidget();
}

FText SAnimViewportToolBar::GetTurnTableMenuLabel() const
{
	FText Label = LOCTEXT("TurnTableError", "Error");
	if (Viewport.IsValid())
	{
		for (int i = 0; i < EAnimationPlaybackSpeeds::NumPlaybackSpeeds; ++i)
		{
			if (Viewport.Pin()->IsTurnTableSpeedSelected(i))
			{
				Label = FText::FromString(FString::Printf(
					(i == EAnimationPlaybackSpeeds::Quarter) ? TEXT("x%.2f") : TEXT("x%.1f"),
					EAnimationPlaybackSpeeds::Values[i]
					));
				break;
			}
		}
	}
	return Label;
}

FSlateColor SAnimViewportToolBar::GetFontColor() const
{
	const UAssetViewerSettings* Settings = UAssetViewerSettings::Get();
	const UEditorPerProjectUserSettings* PerProjectUserSettings = GetDefault<UEditorPerProjectUserSettings>();
	const int32 ProfileIndex = Settings->Profiles.IsValidIndex(PerProjectUserSettings->AssetViewerProfileIndex) ? PerProjectUserSettings->AssetViewerProfileIndex : 0;

	ensureMsgf(Settings && Settings->Profiles.IsValidIndex(PerProjectUserSettings->AssetViewerProfileIndex), TEXT("Invalid default settings pointer or current profile index"));

	FLinearColor FontColor;
	if (Settings->Profiles[ProfileIndex].bShowEnvironment)
	{
		FontColor = FLinearColor::White;
	}
	else
	{
		FLinearColor BackgroundColorInHSV = Viewport.Pin()->GetViewportBackgroundColor().LinearRGBToHSV();

		// see if it's dark, if V is less than 0.2
		if ( BackgroundColorInHSV.B < 0.3f )
		{
			FontColor = FLinearColor::White;
		}
		else
		{
			FontColor = FLinearColor::Black;
		}
	}

	return FontColor;
}

FText SAnimViewportToolBar::GetPlaybackMenuLabel() const
{
	FText Label = LOCTEXT("PlaybackError", "Error");
	if (Viewport.IsValid())
	{
		for(int i = 0; i < EAnimationPlaybackSpeeds::NumPlaybackSpeeds; ++i)
		{
			if (Viewport.Pin()->IsPlaybackSpeedSelected(i))
			{
				Label = FText::FromString(FString::Printf(
					(i == EAnimationPlaybackSpeeds::Quarter) ? TEXT("x%.2f") : TEXT("x%.1f"), 
					EAnimationPlaybackSpeeds::Values[i]
					));
				break;
			}
		}
	}
	return Label;
}

FText SAnimViewportToolBar::GetCameraMenuLabel() const
{
	FText Label = LOCTEXT("Viewport_Default", "Camera");
	TSharedPtr< SAnimationEditorViewportTabBody > PinnedViewport(Viewport.Pin());
	if( PinnedViewport.IsValid() )
	{
		switch( PinnedViewport->GetLevelViewportClient().ViewportType )
		{
		case LVT_Perspective:
			Label = LOCTEXT("CameraMenuTitle_Perspective", "Perspective");
			break;

		case LVT_OrthoXY:
			Label = LOCTEXT("CameraMenuTitle_Top", "Top");
			break;

		case LVT_OrthoYZ:
			Label = LOCTEXT("CameraMenuTitle_Left", "Left");
			break;

		case LVT_OrthoXZ:
			Label = LOCTEXT("CameraMenuTitle_Front", "Front");
			break;

		case LVT_OrthoNegativeXY:
			Label = LOCTEXT("CameraMenuTitle_Bottom", "Bottom");
			break;

		case LVT_OrthoNegativeYZ:
			Label = LOCTEXT("CameraMenuTitle_Right", "Right");
			break;

		case LVT_OrthoNegativeXZ:
			Label = LOCTEXT("CameraMenuTitle_Back", "Back");
			break;
		case LVT_OrthoFreelook:
			break;
		}
	}

	return Label;
}

const FSlateBrush* SAnimViewportToolBar::GetCameraMenuLabelIcon() const
{
	FName Icon = NAME_None;
	TSharedPtr< SAnimationEditorViewportTabBody > PinnedViewport(Viewport.Pin());
	if (PinnedViewport.IsValid())
	{
		switch (PinnedViewport->GetLevelViewportClient().ViewportType)
		{
		case LVT_Perspective:
			Icon = FName("EditorViewport.Perspective");
			break;

		case LVT_OrthoXY:
			Icon = FName("EditorViewport.Top");
			break;

		case LVT_OrthoYZ:
			Icon = FName("EditorViewport.Left");
			break;

		case LVT_OrthoXZ:
			Icon = FName("EditorViewport.Front");
			break;

		case LVT_OrthoNegativeXY:
			Icon = FName("EditorViewport.Bottom");
			break;

		case LVT_OrthoNegativeYZ:
			Icon = FName("EditorViewport.Right");
			break;

		case LVT_OrthoNegativeXZ:
			Icon = FName("EditorViewport.Back");
			break;
		case LVT_OrthoFreelook:
			break;
		}
	}

	return FEditorStyle::GetBrush(Icon);
}

float SAnimViewportToolBar::OnGetFOVValue( ) const
{
	return Viewport.Pin()->GetLevelViewportClient().ViewFOV;
}

void SAnimViewportToolBar::OnFOVValueChanged( float NewValue )
{
	bool bUpdateStoredFOV = true;
	FEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();

	// @todo Viewport Cleanup
/*
	if (ViewportClient.ActorLockedToCamera.IsValid())
	{
		ACameraActor* CameraActor = Cast< ACameraActor >( ViewportClient.ActorLockedToCamera.Get() );
		if( CameraActor != NULL )
		{
			CameraActor->CameraComponent->FieldOfView = NewValue;
			bUpdateStoredFOV = false;
		}
	}*/

	if ( bUpdateStoredFOV )
	{
		ViewportClient.FOVAngle = NewValue;
		// @TODO cleanup - this interface should be in FNewAnimationViewrpotClient in the future
		// update config
		FAnimationViewportClient& AnimViewportClient = (FAnimationViewportClient&)(ViewportClient);
		AnimViewportClient.ConfigOption->SetViewFOV(NewValue);
	}

	ViewportClient.ViewFOV = NewValue;
	ViewportClient.Invalidate();
}

void SAnimViewportToolBar::OnFOVValueCommitted( float NewValue, ETextCommit::Type CommitInfo )
{
	//OnFOVValueChanged will be called... nothing needed here.
}

TOptional<float> SAnimViewportToolBar::OnGetFloorOffset() const
{
	FAnimationViewportClient& AnimViewportClient = (FAnimationViewportClient&)Viewport.Pin()->GetLevelViewportClient();

	return AnimViewportClient.GetFloorOffset();
}

void SAnimViewportToolBar::OnFloorOffsetChanged( float NewValue )
{
	FAnimationViewportClient& AnimViewportClient = (FAnimationViewportClient&)Viewport.Pin()->GetLevelViewportClient();

	AnimViewportClient.SetFloorOffset( NewValue );
}

#undef LOCTEXT_NAMESPACE
