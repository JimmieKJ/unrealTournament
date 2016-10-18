// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MeshPaintPrivatePCH.h"
#include "MeshPaintEdMode.h"
#include "SMeshPaint.h"

#include "SNumericEntryBox.h"
#include "PackageTools.h"
#include "AssetToolsModule.h"
#include "DesktopPlatformModule.h"
#include "ISourceControlModule.h"
#include "SColorPicker.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"

#define LOCTEXT_NAMESPACE "MeshPaint_Mode"


/** Panel to display options associated with importing vertex colors from a TGA  */
class SImportVertexColorsFromTGA : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SImportVertexColorsFromTGA )
	{}
		SLATE_ATTRIBUTE( TSharedPtr<SWindow>, ParentWindow )
	SLATE_END_ARGS()

	SImportVertexColorsFromTGA()
	{
		UVValue = 0;
		LODValue = 0;
		ImportColorMask = 0;
	}

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct( const FArguments& InArgs, class FEdModeMeshPaint* InMeshPaintEditMode )
	{	
		MeshPaintEditMode = InMeshPaintEditMode;
			
		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.f))
			.Padding(4.0f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f, 4.0f)
				[
					// Path
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.FillWidth(1.0f)
					[
						SNew(SHorizontalBox)
								
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.FillWidth(1.0f)
						[
							SAssignNew(TGATextBox, SEditableTextBox)
							.MinDesiredWidth(128.0f)
							.RevertTextOnEscape(true)
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2.0f, 0.0f, 0.0f, 0.0f )
						.VAlign(VAlign_Center)
						[
							SNew( SButton )
							.OnClicked(this, &SImportVertexColorsFromTGA::FindTGAButtonClicked)
							[
								SNew(SImage) .Image(FEditorStyle::GetBrush("ContentBrowser.PathPickerButton"))
							]
						]
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f, 4.0f)
				[
					SNew(SWrapBox)
					.UseAllottedWidth(true)
					+SWrapBox::Slot()
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						// UV option
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth() 
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("MeshPaint_ImportUVLabel", "UV"))
						]
						+SHorizontalBox::Slot()
						.AutoWidth() 
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.Padding(4.0f, 0.0f, 0.0f, 0.0f)
						[
							SAssignNew( UVComboButton, SComboButton )
							.OnGetMenuContent(this, &SImportVertexColorsFromTGA::GetUVMenu)
							.ContentPadding(FMargin(2.0f, 0.0f))
							.ButtonContent()
							[
								SNew(STextBlock)
								.Text(this, &SImportVertexColorsFromTGA::GetUVSelectionString )
							]
						]
					]
					+SWrapBox::Slot()
					.Padding(0.0f, 0.0f, 16.0f, 0.0f)
					[
						// LOD option
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth() 
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("MeshPaint_ImportLODLabel", "LOD"))
						]
						+SHorizontalBox::Slot()
						.AutoWidth() 
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.Padding(4.0f, 0.0f, 0.0f, 0.0f)
						[
							SAssignNew( LODComboButton, SComboButton )
							.OnGetMenuContent(this, &SImportVertexColorsFromTGA::GetLODMenu)
							.ContentPadding(FMargin(2.0f, 0.0f))
							.ButtonContent()
							[
								SNew(STextBlock)
								.Text(this, &SImportVertexColorsFromTGA::GetLODSelectionString )
							]
						]
					]
					+SWrapBox::Slot()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						[
							// RGBA checkboxes
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.AutoWidth() 
							[
								SNew(STextBlock)
								.Text(LOCTEXT("MeshPaint_ImportChannelsLabel", "Channels"))
							]
							+SHorizontalBox::Slot()
							.AutoWidth() 
							.Padding(4.0f, 0.0f, 4.0f, 0.0f)
							[
								SNew(SCheckBox)
								.OnCheckStateChanged( this, &SImportVertexColorsFromTGA::OnImportColorChannelChanged, FImportVertexTextureHelper::ChannelsMask::ERed )
								.IsChecked( this, &SImportVertexColorsFromTGA::IsRadioChecked, FImportVertexTextureHelper::ChannelsMask::ERed )
								[
									SNew(STextBlock)
									.Text(LOCTEXT("MeshPaint_ImportColorChannelsRed", "R"))
								]	
							]
							+SHorizontalBox::Slot()
							.AutoWidth() 
							.Padding(0.0f, 0.0f, 4.0f, 0.0f)
							[
								SNew(SCheckBox)
								.OnCheckStateChanged( this, &SImportVertexColorsFromTGA::OnImportColorChannelChanged, FImportVertexTextureHelper::ChannelsMask::EGreen )
								.IsChecked( this, &SImportVertexColorsFromTGA::IsRadioChecked, FImportVertexTextureHelper::ChannelsMask::EGreen )
								[
									SNew(STextBlock)
									.Text(LOCTEXT("MeshPaint_ImportColorChannelsGreen", "G"))
								]
							]
							+SHorizontalBox::Slot()
							.AutoWidth() 
							.Padding(0.0f, 0.0f, 4.0f, 0.0f)
							[
								SNew(SCheckBox)
								.OnCheckStateChanged( this, &SImportVertexColorsFromTGA::OnImportColorChannelChanged, FImportVertexTextureHelper::ChannelsMask::EBlue )
								.IsChecked( this, &SImportVertexColorsFromTGA::IsRadioChecked, FImportVertexTextureHelper::ChannelsMask::EBlue )
								[
									SNew(STextBlock)
									.Text(LOCTEXT("MeshPaint_ImportColorChannels_Blue", "B"))
								]
							]
							+SHorizontalBox::Slot()
							.AutoWidth() 
							.Padding(0.0f, 0.0f, 4.0f, 0.0f)
							[
								SNew(SCheckBox)
								.OnCheckStateChanged( this, &SImportVertexColorsFromTGA::OnImportColorChannelChanged, FImportVertexTextureHelper::ChannelsMask::EAlpha )
								.IsChecked( this, &SImportVertexColorsFromTGA::IsRadioChecked, FImportVertexTextureHelper::ChannelsMask::EAlpha )
								[
									SNew(STextBlock)
									.Text(LOCTEXT("MeshPaint_ImportColorChannelsAlpha", "A"))
								]
							]
						]
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f, 4.0f)
				.HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew( SButton )
						.OnClicked(this, &SImportVertexColorsFromTGA::ImportButtonClicked)
						.ContentPadding(FMargin(6.0f, 2.0f))
						[
							SNew( STextBlock )
							.Text(LOCTEXT("MeshPaint_ImportButtonLabel", "Import"))
						]
					]

				]
			]
		];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

private:

	void OnImportColorChannelChanged(ECheckBoxState InNewValue, FImportVertexTextureHelper::ChannelsMask InColorChannelMask)
	{
		// Toggle the appropriate bit in the import color mask
		ImportColorMask ^= InColorChannelMask;
	}

	ECheckBoxState IsRadioChecked( FImportVertexTextureHelper::ChannelsMask InColorChannelMask ) const
	{
		// Bitwise check to see if the specified color channel should be checked
		return (ImportColorMask & InColorChannelMask)
			? ECheckBoxState::Checked
			: ECheckBoxState::Unchecked;
	}

	FReply FindTGAButtonClicked()
	{
		// Prompt the user for the filenames
		TArray<FString> OpenFilenames;
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		bool bOpened = false;
		if (DesktopPlatform != NULL)
		{
			TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
			void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

			bOpened = DesktopPlatform->OpenFileDialog(
				ParentWindowHandle, 
				TEXT("Select TGA file.."), 
				TEXT(""), 
				TEXT(""), 
				TEXT("TGA file (*.tga)|*.tga"), 
				EFileDialogFlags::None, 
				OpenFilenames);
		}

		if (bOpened == true)
		{
			if (OpenFilenames.Num() > 0)
			{
				TGATextBox->SetText(FText::FromString(OpenFilenames[0]));
			}
			else
			{
				bOpened = false;
			}
		}
		return FReply::Handled();
	}

	FReply ImportButtonClicked()
	{
		FImportVertexTextureHelper ImportVertex;
		FString Path = TGATextBox->GetText().ToString();
		ImportVertex.ImportVertexColors(MeshPaintEditMode->GetModeManager(), Path, UVValue, LODValue, ImportColorMask);

		return FReply::Handled();
	}


	void OnChangeUV(int32 InCount)
	{
		UVValue = InCount;
	}

	FText GetUVSelectionString() const
	{
		return FText::AsNumber(UVValue);
	}

	void OnChangeLOD(int32 InCount)
	{
		LODValue = InCount;
	}

	FText GetLODSelectionString() const
	{
		return FText::AsNumber(LODValue);
	}

	TSharedRef<SWidget> GetUVMenu()
	{
		FMenuBuilder MenuBuilder(true, NULL);
		for(int UVCount = 0; UVCount < 4; UVCount++)
		{
			MenuBuilder.AddMenuEntry( FText::AsNumber( UVCount), FText::GetEmpty(), FSlateIcon(), FExecuteAction::CreateSP(this, &SImportVertexColorsFromTGA::OnChangeUV, UVCount));
		}
		return MenuBuilder.MakeWidget();
	}

	TSharedRef<SWidget> GetLODMenu()
	{
		FMenuBuilder MenuBuilder(true, NULL);
		for(int LODCount = 0; LODCount < 4; LODCount++)
		{
			MenuBuilder.AddMenuEntry( FText::AsNumber( LODCount), FText::GetEmpty(), FSlateIcon(), FExecuteAction::CreateSP(this, &SImportVertexColorsFromTGA::OnChangeLOD, LODCount));
		}
		return MenuBuilder.MakeWidget();
	}

private:

	/* Holds the text box for the input TGA */
	TSharedPtr<SEditableTextBox> TGATextBox;

	/** The UV combo button */
	TSharedPtr< SComboButton > UVComboButton;

	/** The LOD combo button */
	TSharedPtr< SComboButton > LODComboButton;

	/** The currently selected UV value */
	int32 UVValue;

	/** The currently selected LOD value */
	int32 LODValue;

	/** Mask representing the import color channels that are selected */
	uint8 ImportColorMask;
	
	/** Mesh paint edit mode */
	class FEdModeMeshPaint* MeshPaintEditMode;
};

DECLARE_DELEGATE_OneParam( FOMeshPaintResourceChanged, EMeshPaintResource::Type );

/** Radio button widget for selecting the vertex paint target  */
class SMeshPaintResourceRadioButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SMeshPaintResourceRadioButton )
		: _Target(EMeshPaintResource::VertexColors)
		, _OnSelectionChanged()
	{}
	SLATE_ARGUMENT( EMeshPaintResource::Type, Target )
		SLATE_EVENT( FOMeshPaintResourceChanged, OnSelectionChanged )
		SLATE_END_ARGS()

	void Construct( const FArguments& InArgs )
	{
		CurrentChoice = InArgs._Target;
		OnSelectionChanged = InArgs._OnSelectionChanged;

		this->ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight() 
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth() 
				.Padding(0.0f, 0.0f, 2.0f, 0.0f)
				[
					CreateRadioButton( LOCTEXT("MeshPaint_Vertices", "Vertices"), EMeshPaintResource::VertexColors )
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					CreateRadioButton( LOCTEXT("MeshPaint_Texture", "Textures"), EMeshPaintResource::Texture )
				]
			]
		];
	}
private:

	TSharedRef<SWidget> CreateRadioButton( const FText& RadioText, EMeshPaintResource::Type RadioButtonChoice )
	{
		return
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "RadioButton")
			.IsChecked( this, &SMeshPaintResourceRadioButton::IsRadioChecked, RadioButtonChoice )
			.OnCheckStateChanged( this, &SMeshPaintResourceRadioButton::OnRadioChanged, RadioButtonChoice )
			.Content()
			[
				SNew(STextBlock).Text(RadioText)
			];
	}

	ECheckBoxState IsRadioChecked( EMeshPaintResource::Type ButtonId ) const
	{
		return (CurrentChoice == ButtonId)
			? ECheckBoxState::Checked
			: ECheckBoxState::Unchecked;
	}

	void OnRadioChanged( ECheckBoxState NewRadioState, EMeshPaintResource::Type RadioThatChanged )
	{
		if (NewRadioState == ECheckBoxState::Checked)
		{
			CurrentChoice = RadioThatChanged;

			if (OnSelectionChanged.IsBound())
			{
				OnSelectionChanged.Execute(CurrentChoice);
			}
		}
	}

	EMeshPaintResource::Type CurrentChoice;
	FOMeshPaintResourceChanged OnSelectionChanged;
};


DECLARE_DELEGATE_OneParam( FOnVertexPaintTargetChanged, EMeshVertexPaintTarget::Type );

/** Radio button widget for selecting the vertex paint target  */
class SVertexPaintTargetRadioButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SVertexPaintTargetRadioButton )
		: _Target(EMeshVertexPaintTarget::ComponentInstance)
		, _OnSelectionChanged()
	{}
		SLATE_ARGUMENT( EMeshVertexPaintTarget::Type, Target )
		SLATE_EVENT( FOnVertexPaintTargetChanged, OnSelectionChanged )
	SLATE_END_ARGS()

		void Construct( const FArguments& InArgs )
	{
		CurrentChoice = InArgs._Target;
		OnSelectionChanged = InArgs._OnSelectionChanged;

		this->ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight() 
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth() 
				.Padding(0.0f, 0.0f, 2.0f, 0.0f)
				[
					CreateRadioButton( LOCTEXT("MeshPaint_PaintTargetActor", "Actor"), EMeshVertexPaintTarget::ComponentInstance )
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					CreateRadioButton( LOCTEXT("MeshPaint_PaintTargetMesh", "Mesh asset"), EMeshVertexPaintTarget::Mesh )
				]
			]
		];
	}

private:

	TSharedRef<SWidget> CreateRadioButton( const FText& RadioText, EMeshVertexPaintTarget::Type RadioButtonChoice )
	{
		return
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "RadioButton")
			.IsChecked( this, &SVertexPaintTargetRadioButton::IsRadioChecked, RadioButtonChoice )
			.OnCheckStateChanged( this, &SVertexPaintTargetRadioButton::OnRadioChanged, RadioButtonChoice )
			.Content()
			[
				SNew(STextBlock).Text(RadioText)
			];
	}

	ECheckBoxState IsRadioChecked( EMeshVertexPaintTarget::Type ButtonId ) const
	{
		return (CurrentChoice == ButtonId)
			? ECheckBoxState::Checked
			: ECheckBoxState::Unchecked;
	}

	void OnRadioChanged( ECheckBoxState NewRadioState, EMeshVertexPaintTarget::Type RadioThatChanged )
	{
		if (NewRadioState == ECheckBoxState::Checked)
		{
			CurrentChoice = RadioThatChanged;

			if (OnSelectionChanged.IsBound())
			{
				OnSelectionChanged.Execute(CurrentChoice);
			}
		}
	}

	EMeshVertexPaintTarget::Type CurrentChoice;
	FOnVertexPaintTargetChanged OnSelectionChanged;
};



DECLARE_DELEGATE_OneParam( FOnVertexPaintModeChanged, EMeshPaintMode::Type );

/** Radio button widget for selecting the vertex paint mode */
class SVertexPaintModeRadioButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SVertexPaintModeRadioButton )
		: _Mode(EMeshPaintMode::PaintColors)
		, _OnSelectionChanged()
	{}
		SLATE_ARGUMENT( EMeshPaintMode::Type, Mode )
		SLATE_EVENT( FOnVertexPaintModeChanged, OnSelectionChanged )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs )
	{
		CurrentChoice = InArgs._Mode;
		OnSelectionChanged = InArgs._OnSelectionChanged;

		this->ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight() 
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth() 
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					CreateRadioButton( LOCTEXT("MeshPaint_PaintModeColor", "Colors"), EMeshPaintMode::PaintColors )
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					CreateRadioButton( LOCTEXT("MeshPaint_PaintModeBlendWeights", "Blend Weights"), EMeshPaintMode::PaintWeights )
				]
			]
		];
	}

private:

	TSharedRef<SWidget> CreateRadioButton( const FText& RadioText, EMeshPaintMode::Type RadioButtonChoice )
	{
		return
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "RadioButton")
			.IsChecked( this, &SVertexPaintModeRadioButton::IsRadioChecked, RadioButtonChoice )
			.OnCheckStateChanged( this, &SVertexPaintModeRadioButton::OnRadioChanged, RadioButtonChoice )
			.Content()
			[
				SNew(STextBlock).Text(RadioText)
			];
	}

	ECheckBoxState IsRadioChecked( EMeshPaintMode::Type ButtonId ) const
	{
		return (CurrentChoice == ButtonId)
			? ECheckBoxState::Checked
			: ECheckBoxState::Unchecked;
	}

	void OnRadioChanged( ECheckBoxState NewRadioState, EMeshPaintMode::Type RadioThatChanged )
	{
		if (NewRadioState == ECheckBoxState::Checked)
		{
			CurrentChoice = RadioThatChanged;

			if (OnSelectionChanged.IsBound())
			{
				OnSelectionChanged.Execute(CurrentChoice);
			}
		}
	}

	EMeshPaintMode::Type CurrentChoice;
	FOnVertexPaintModeChanged OnSelectionChanged;
};



DECLARE_DELEGATE_OneParam( FOnVertexColorViewModeChanged, EMeshPaintColorViewMode::Type );

/** Radio button widget for selecting the color view mode  */
class SVertexPaintColorViewRadioButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SVertexPaintColorViewRadioButton )
		: _Mode(EMeshPaintColorViewMode::Normal)
		, _OnSelectionChanged()
	{}
		SLATE_ARGUMENT( EMeshPaintColorViewMode::Type, Mode )
		SLATE_EVENT( FOnVertexColorViewModeChanged, OnSelectionChanged )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs )
	{
		CurrentChoice = InArgs._Mode;
		OnSelectionChanged = InArgs._OnSelectionChanged;

		this->ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight() 
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth() 
				.Padding(0.0f, 0.0f, 2.0f, 0.0f) 
				[
					CreateRadioButton( LOCTEXT("MeshPaint_ColorViewOff", "Off"), EMeshPaintColorViewMode::Normal )
				]
				+SHorizontalBox::Slot()
				.AutoWidth() 
				.Padding(2.0f, 0.0f) 
				[
					CreateRadioButton(  LOCTEXT("MeshPaint_ColorViewRGB", "RGB"), EMeshPaintColorViewMode::RGB )
				]
				+SHorizontalBox::Slot()
				.AutoWidth() 
				.Padding(2.0f, 0.0f)
				[
					CreateRadioButton( LOCTEXT("MeshPaint_ColorViewR", "R"), EMeshPaintColorViewMode::Red )
				]
				+SHorizontalBox::Slot()
				.AutoWidth() 
				.Padding(2.0f, 0.0f) 
				[
					CreateRadioButton( LOCTEXT("MeshPaint_ColorViewG", "G"), EMeshPaintColorViewMode::Green )
				]
				+SHorizontalBox::Slot()
				.AutoWidth() 
				.Padding(2.0f, 0.0f) 
				[
					CreateRadioButton( LOCTEXT("MeshPaint_ColorViewB", "B"), EMeshPaintColorViewMode::Blue )
				]
				+SHorizontalBox::Slot()
				.AutoWidth() 
				.Padding(2.0f, 0.0f) 
				[
					CreateRadioButton( LOCTEXT("MeshPaint_ColorViewA", "A"), EMeshPaintColorViewMode::Alpha )
				]
			]
		];
	}

private:

	TSharedRef<SWidget> CreateRadioButton( const FText& RadioText, EMeshPaintColorViewMode::Type RadioButtonChoice )
	{
		return
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "RadioButton")
			.IsChecked( this, &SVertexPaintColorViewRadioButton::IsRadioChecked, RadioButtonChoice )
			.OnCheckStateChanged( this, &SVertexPaintColorViewRadioButton::OnRadioChanged, RadioButtonChoice )
			.Content()
			[
				SNew(STextBlock).Text(RadioText)
			];
	}

	ECheckBoxState IsRadioChecked( EMeshPaintColorViewMode::Type ButtonId ) const
	{
		return (CurrentChoice == ButtonId)
			? ECheckBoxState::Checked
			: ECheckBoxState::Unchecked;
	}

	void OnRadioChanged( ECheckBoxState NewRadioState, EMeshPaintColorViewMode::Type RadioThatChanged )
	{
		if (NewRadioState == ECheckBoxState::Checked)
		{
			CurrentChoice = RadioThatChanged;

			if (OnSelectionChanged.IsBound())
			{
				OnSelectionChanged.Execute(CurrentChoice);
			}
		}
	}

	EMeshPaintColorViewMode::Type CurrentChoice;
	FOnVertexColorViewModeChanged OnSelectionChanged;
};

DECLARE_DELEGATE_OneParam( FOnVertexPaintColorSetChanged, EMeshPaintColorSet::Type );

/** Radio button widget that allows users to select between paint and erase color  */
class SVertexPaintColorSetRadioButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SVertexPaintColorSetRadioButton )
		: _Color(EMeshPaintColorSet::PaintColor)
		, _OnSelectionChanged()
	{}
		SLATE_ARGUMENT( EMeshPaintColorSet::Type, Color )
		SLATE_EVENT( FOnVertexPaintColorSetChanged, OnSelectionChanged )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs )
	{
		CurrentChoice = InArgs._Color;
		OnSelectionChanged = InArgs._OnSelectionChanged;

		this->ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[

				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 8.0f)
					[
						CreateRadioButton( LOCTEXT("MeshPaint_ColorSet", "Paint color"), EMeshPaintColorSet::PaintColor )
					]
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						CreateRadioButton( LOCTEXT("MeshPaint_EraseSet", "Erase color"), EMeshPaintColorSet::EraseColor )
					]
				]
				+SHorizontalBox::Slot()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				.FillWidth(1)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 8.0f)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.FillWidth(1)
						[
							SNew(SColorBlock)
							.Color(this, &SVertexPaintColorSetRadioButton::GetPaintColor) 
							.IgnoreAlpha(true)
							.OnMouseButtonDown(this, &SVertexPaintColorSetRadioButton::PaintColorBlock_OnMouseButtonDown)
						]
						+SHorizontalBox::Slot()
						.FillWidth(1)
						[
							SNew(SColorBlock) 
							.Color(this,  &SVertexPaintColorSetRadioButton::GetPaintColor) 
							.ShowBackgroundForAlpha(true)
							.OnMouseButtonDown(this, &SVertexPaintColorSetRadioButton::PaintColorBlock_OnMouseButtonDown)
						]
					]
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.FillWidth(1)
						[
							SNew(SColorBlock) 
							.Color(this, &SVertexPaintColorSetRadioButton::GetEraseColor) 
							.ColorIsHSV(false) 
							.IgnoreAlpha(true)
							.OnMouseButtonDown( this, &SVertexPaintColorSetRadioButton::EraseColorBlock_OnMouseButtonDown )
						]
						+SHorizontalBox::Slot()
						.FillWidth(1)
						[
							SNew(SColorBlock) 
							.Color(this,  &SVertexPaintColorSetRadioButton::GetEraseColor) 
							.ColorIsHSV(false) 
							.ShowBackgroundForAlpha(true)
							.OnMouseButtonDown( this, &SVertexPaintColorSetRadioButton::EraseColorBlock_OnMouseButtonDown )
						]
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 0.0f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ToolTipText( LOCTEXT("MeshPaint_SwapToolTip", "Swap") )
					.ContentPadding(0.0f) 
					.OnClicked(this, &SVertexPaintColorSetRadioButton::SwapPaintAndEraseColorButtonClicked)
					.Content()
					[
						SNew(SImage) .Image( FEditorStyle::GetBrush("MeshPaint.Swap") )
					]
				]
			]
		];
	}

protected:
	FLinearColor GetPaintColor() const
	{
		return FMeshPaintSettings::Get().PaintColor;
	}

	FLinearColor GetEraseColor() const
	{
		return FMeshPaintSettings::Get().EraseColor;
	}

	void OnPaintColorChanged(FLinearColor InNewColor)
	{
		FMeshPaintSettings::Get().PaintColor = InNewColor;
	}

	void OnEraseColorChanged(FLinearColor InNewColor)
	{
		FMeshPaintSettings::Get().EraseColor = InNewColor;
	}

	FReply SwapPaintAndEraseColorButtonClicked()
	{
		FLinearColor TempColor = FMeshPaintSettings::Get().PaintColor;
		FMeshPaintSettings::Get().PaintColor = FMeshPaintSettings::Get().EraseColor;
		FMeshPaintSettings::Get().EraseColor = TempColor;
		return FReply::Handled();
	}

private:

	TSharedRef<SWidget> CreateRadioButton( const FText& RadioText, EMeshPaintColorSet::Type RadioButtonChoice )
	{
		return
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "RadioButton")
			.IsChecked( this, &SVertexPaintColorSetRadioButton::IsRadioChecked, RadioButtonChoice )
			.OnCheckStateChanged( this, &SVertexPaintColorSetRadioButton::OnRadioChanged, RadioButtonChoice )
			.Content()
			[
				SNew(STextBlock).Text(RadioText)
			];
	}

	ECheckBoxState IsRadioChecked( EMeshPaintColorSet::Type ButtonId ) const
	{
		return (CurrentChoice == ButtonId)
			? ECheckBoxState::Checked
			: ECheckBoxState::Unchecked;
	}

	void OnRadioChanged( ECheckBoxState NewRadioState, EMeshPaintColorSet::Type RadioThatChanged )
	{
		if (NewRadioState == ECheckBoxState::Checked)
		{
			CurrentChoice = RadioThatChanged;

			if (OnSelectionChanged.IsBound())
			{
				OnSelectionChanged.Execute(CurrentChoice);
			}
		}
	}

	FReply PaintColorBlock_OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if ( MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton )
		{
			return FReply::Unhandled();
		}

		CreateColorPickerWindow(GetPaintColor(), FOnLinearColorValueChanged::CreateSP( this, &SVertexPaintColorSetRadioButton::OnPaintColorChanged));
		
		return FReply::Handled();
	}

	FReply EraseColorBlock_OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if ( MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton )
		{
			return FReply::Unhandled();
		}

		CreateColorPickerWindow(GetEraseColor(), FOnLinearColorValueChanged::CreateSP( this, &SVertexPaintColorSetRadioButton::OnEraseColorChanged));

		return FReply::Handled();
	}
	
	void CreateColorPickerWindow(const FLinearColor& InInitialColor, const FOnLinearColorValueChanged& InOnColorCommitted)
	{
		FColorPickerArgs PickerArgs;
		PickerArgs.ParentWidget = AsShared();
		PickerArgs.bUseAlpha = true;
		PickerArgs.DisplayGamma = TAttribute<float>::Create( TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma) );
		PickerArgs.OnColorCommitted = InOnColorCommitted;
		PickerArgs.InitialColorOverride = InInitialColor;
		PickerArgs.bOnlyRefreshOnOk = true;

		OpenColorPicker(PickerArgs);
	}
	
	EMeshPaintColorSet::Type CurrentChoice;
	FOnVertexPaintColorSetChanged OnSelectionChanged;
};


DECLARE_DELEGATE_OneParam( FOnWeightIndexChanged, int32 );

/** Radio button that switches between color and blend weight mode  */
class SVertexPaintWeightRadioButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SVertexPaintWeightRadioButton )
		: _WeightIndex(0)
		, _OnSelectionChanged()
	{}
		SLATE_ATTRIBUTE( int32, WeightIndex )
		SLATE_EVENT( FOnWeightIndexChanged, OnSelectionChanged )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs )
	{
		CurrentChoice = InArgs._WeightIndex;
		OnSelectionChanged = InArgs._OnSelectionChanged;

		this->ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight() 
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth() 
				.Padding(0.0f, 0.0f, 2.0f, 0.0f) 
				[
					SNew(SVerticalBox)
					.Visibility(this, &SVertexPaintWeightRadioButton::GetIndexVisibility, 0 )
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						CreateRadioButton( FText::FromString(TEXT("1")), 0 )
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth() 
				.Padding(2.0f, 0.0f) 
				[
					SNew(SVerticalBox)
					.Visibility(this, &SVertexPaintWeightRadioButton::GetIndexVisibility, 1 )
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						CreateRadioButton( FText::FromString(TEXT("2")), 1 )
					]
					
				]
				+SHorizontalBox::Slot()
				.AutoWidth() 
				.Padding(2.0f, 0.0f)
				[
					SNew(SVerticalBox)
					.Visibility(this, &SVertexPaintWeightRadioButton::GetIndexVisibility, 2 )
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						CreateRadioButton( FText::FromString(TEXT("3")), 2 )
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth() 
				.Padding(2.0f, 0.0f) 
				[
					SNew(SVerticalBox)
					.Visibility(this, &SVertexPaintWeightRadioButton::GetIndexVisibility, 3 )
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						CreateRadioButton( FText::FromString(TEXT("4")), 3 )
					]
					
				]
				+SHorizontalBox::Slot()
				.AutoWidth() 
				.Padding(2.0f, 0.0f) 
				[
					SNew(SVerticalBox)
					.Visibility(this, &SVertexPaintWeightRadioButton::GetIndexVisibility, 4 )
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						CreateRadioButton( FText::FromString(TEXT("5")), 4 )
					]
				]
			]
		];
	}

private:

	TSharedRef<SWidget> CreateRadioButton( const FText& RadioText, int32 RadioButtonChoice )
	{
		return
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "RadioButton")
			.IsChecked( this, &SVertexPaintWeightRadioButton::IsRadioChecked, RadioButtonChoice )
			.OnCheckStateChanged( this, &SVertexPaintWeightRadioButton::OnRadioChanged, RadioButtonChoice )
			.Content()
			[
				SNew(STextBlock).Text(RadioText)
			];
	}

	ECheckBoxState IsRadioChecked( int32 ButtonId ) const
	{
		return (CurrentChoice.Get() == ButtonId)
			? ECheckBoxState::Checked
			: ECheckBoxState::Unchecked;
	}

	void OnRadioChanged( ECheckBoxState NewRadioState, int32 RadioThatChanged )
	{
		if (NewRadioState == ECheckBoxState::Checked)
		{
			if (OnSelectionChanged.IsBound())
			{
				OnSelectionChanged.Execute(RadioThatChanged);
			}
		}
	}

	EVisibility GetIndexVisibility(int32 InIndex) const
	{
		if( InIndex < FMeshPaintSettings::Get().TotalWeightCount)
		{
			return EVisibility::Visible;
		}
		return EVisibility::Collapsed;

	}

	TAttribute<int32> CurrentChoice;
	FOnWeightIndexChanged OnSelectionChanged;
};


FMeshPaintToolKit::FMeshPaintToolKit(class FEdModeMeshPaint* InOwningMode)
	: MeshPaintEdMode(InOwningMode)
{
}

void FMeshPaintToolKit::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{

}

void FMeshPaintToolKit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{

}

void FMeshPaintToolKit::Init(const TSharedPtr< class IToolkitHost >& InitToolkitHost)
{
	MeshPaintWidgets = SNew(SMeshPaint, SharedThis(this), MeshPaintEdMode);

	FModeToolkit::Init(InitToolkitHost);
}

FName FMeshPaintToolKit::GetToolkitFName() const
{
	return FName("MeshPaintMode");
}

FText FMeshPaintToolKit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Mesh Paint");
}

class FEdMode* FMeshPaintToolKit::GetEditorMode() const
{
	return MeshPaintEdMode;
}

TSharedPtr<SWidget> FMeshPaintToolKit::GetInlineContent() const
{
	return MeshPaintWidgets;
}

SMeshPaint::~SMeshPaint()
{
	MeshPaintEditMode = nullptr;
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMeshPaint::Construct(const FArguments& InArgs, TSharedRef<FMeshPaintToolKit> InParentToolkit, class FEdModeMeshPaint* InOwningMode)
{
	MeshPaintEditMode = InOwningMode;

	PaintColorSet = EMeshPaintColorSet::PaintColor;
	bShowImportOptions = false;

	FMargin StandardPadding(0.0f, 4.0f, 0.0f, 4.0f);

	const float BrushRadius = MeshPaintEditMode->GetBrushRadiiDefault();

	float MinBrushSliderRadius, MaxBrushSliderRadius;
	MeshPaintEditMode->GetBrushRadiiSliderLimits(MinBrushSliderRadius, MaxBrushSliderRadius);

	float MinBrushRadius, MaxBrushRadius;
	MeshPaintEditMode->GetBrushRadiiLimits(MinBrushRadius, MaxBrushRadius);

	ChildSlot
	[
		SNew( SScrollBox )
		+ SScrollBox::Slot()
		.Padding( 0.0f )
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush( "ToolPanel.GroupBorder" ))
				.Content()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(6.0f, 0.0f)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(StandardPadding)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(StandardPadding)
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot() 
								.Padding(2.0f, 0.0f) 
								.FillWidth(1) 
								.HAlign(HAlign_Left)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("MeshPaint_PaintDataLabel", "Paint"))
								]
								+SHorizontalBox::Slot()
								.AutoWidth() 
								.Padding(0.0f, 0.0f, 2.0f, 0.0f) 
								.HAlign(HAlign_Right)
								[
									SNew(SMeshPaintResourceRadioButton)
									.Target(FMeshPaintSettings::Get().ResourceType)
									.OnSelectionChanged(this, &SMeshPaint::OntMeshPaintResourceChanged)
								]
							]
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(StandardPadding)
						[
							SNew(SSeparator)
							.Orientation(Orient_Horizontal)
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(StandardPadding)
						[
							SNew(SVerticalBox)
							.Visibility(this, &SMeshPaint::GetResourceTypeTexturesVisibility)
							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(StandardPadding)
							[
								// New Texture
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.Padding(2.0f, 0.0f) 
								.FillWidth(1) 
								.HAlign(HAlign_Left)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("MeshPaint_NewTextureDataLabel", "New Texture"))
								]
								+SHorizontalBox::Slot()
								.AutoWidth() 
								.Padding(0.0f, 0.0f, 2.0f, 0.0f)
								.HAlign(HAlign_Right)
								[
									SNew(SHorizontalBox)
									+SHorizontalBox::Slot()
									.AutoWidth()
									.Padding(2.0f, 0.0f) 
									.HAlign(HAlign_Right)
									[
										SNew(SButton) 
										.Text( LOCTEXT("MeshPaint_NewTexture", "New Texture"))
										.HAlign(HAlign_Left) 
										.VAlign(VAlign_Center)
										.IsEnabled(this, &SMeshPaint::IsSelectedTextureValid )
										.OnClicked(this, &SMeshPaint::NewTextureButtonClicked)
									]
									+SHorizontalBox::Slot()
									.AutoWidth() 
									.Padding(2.0f, 0.0f) 
									[
										SNew(SButton) 
										.Text( LOCTEXT("MeshPaint_Duplicate", "Duplicate"))
										.HAlign(HAlign_Left) 
										.VAlign(VAlign_Center)
										.IsEnabled(this, &SMeshPaint::CanCreateInstanceMaterialAndTexture)
										.OnClicked(this, &SMeshPaint::DuplicateTextureButtonClicked)
									]
								]
							]
							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(StandardPadding)
							[
								// UV Channel
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot() 
								.Padding(2.0f, 0.0f) 
								.FillWidth(1) 
								.HAlign(HAlign_Left)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("MeshPaint_UVChannelDataLabel", "UV Channel"))
								]
								+SHorizontalBox::Slot()
								.AutoWidth() 
								.Padding(2.0f, 0.0f) 
								.HAlign(HAlign_Right)
								[
									SNew(SComboButton)
									.OnGetMenuContent(this, &SMeshPaint::GetUVChannels)
									.ContentPadding(2.0f)
									.IsEnabled( this, &SMeshPaint::IsSelectedTextureValid )
									.ButtonContent()
									[
										SNew(STextBlock)
										.Text(this, &SMeshPaint::GetCurrentUVChannel)
									]
								]
							]
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(StandardPadding)
						[
							SNew(SVerticalBox)
							.Visibility(this, &SMeshPaint::GetResourceTypeVerticesVisibility )
							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(StandardPadding)
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot() 
								.Padding(2.0f, 0.0f) 
								.FillWidth(1) 
								.HAlign(HAlign_Left)
								[
									SNew(STextBlock)								
									.Text(LOCTEXT("MeshPaint_InstanceVertexColorsLabel", "Instance vertex colors"))
						
								]
								+SHorizontalBox::Slot()
								.AutoWidth() 
								.Padding(2.0f, 0.0f) 
								.HAlign(HAlign_Right)
								[
									SNew(STextBlock)
									.Text(this, &SMeshPaint::GetInstanceVertexColorsText)
								]
							]
							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(StandardPadding)
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot() 
								.FillWidth(1) 
								[
									SNew(SWrapBox)
									.UseAllottedWidth(true)
									+SWrapBox::Slot()
									.Padding(2.0f, 0.0f) 
									[
										SNew(SButton) 
										.Text( LOCTEXT("MeshPaint_Copy", "Copy"))
										.HAlign(HAlign_Right) 
										.VAlign(VAlign_Center)
										.IsEnabled(this, &SMeshPaint::CanCopyToColourBufferCopy)
										.OnClicked(this, &SMeshPaint::CopyInstanceVertexColorsButtonClicked)
									]
									+SWrapBox::Slot()
									.Padding(2.0f, 0.0f) 
									[
										SNew(SButton) 
										.Text( LOCTEXT("MeshPaint_Paste", "Paste"))
										.HAlign(HAlign_Right) 
										.VAlign(VAlign_Center)
										.IsEnabled(this, &SMeshPaint::CanPasteFromColourBufferCopy)
										.OnClicked(this, &SMeshPaint::PasteInstanceVertexColorsButtonClicked)
									]
									+SWrapBox::Slot()
									.Padding(2.0f, 0.0f) 
									[
										SNew(SButton) 
										.Text( LOCTEXT("MeshPaint_Remove", "Remove")) 
										.HAlign(HAlign_Right) 
										.VAlign(VAlign_Center)
										.IsEnabled(this, &SMeshPaint::HasInstanceVertexColors)
										.OnClicked(this, &SMeshPaint::RemoveInstanceVertexColorsButtonClicked)
									]
									+SWrapBox::Slot()
									.Padding(2.0f, 0.0f) 
									[
										SNew(SButton) 
										.Text( LOCTEXT("MeshPaint_Fix", "Fix"))
										.HAlign(HAlign_Right) 
										.VAlign(VAlign_Center)
										.IsEnabled(this, &SMeshPaint::RequiresInstanceVertexColorsFixup)
										.OnClicked(this, &SMeshPaint::FixInstanceVertexColorsButtonClicked)
									]
								]
							]
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(StandardPadding)
						[
							SNew(SSeparator)
							.Orientation(Orient_Horizontal)
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(StandardPadding)
						[
							SNew(SVerticalBox)
							.Visibility(this, &SMeshPaint::GetResourceTypeTexturesVisibility)
							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(StandardPadding)
							[
								// New Texture
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot() 
								.Padding(2.0f, 0.0f) 
								.FillWidth(1) 
								.HAlign(HAlign_Left)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("MeshPaint_NewTexturePaintTargetLabel", "Texture Paint Target"))
								]
								+SHorizontalBox::Slot() 
								.Padding(2.0f, 0.0f) 
								.FillWidth(1) 
								.HAlign(HAlign_Right)
								[
									SNew(SHorizontalBox)
									+SHorizontalBox::Slot()
									.AutoWidth() 
									.Padding(2.0f, 0.0f) 
									.HAlign(HAlign_Right)
									.VAlign(VAlign_Center)
									[
										SNew(SButton)
										.ToolTipText( LOCTEXT("FindSourceMeshInContentBrowser", "Find texture in content browser") )
										.ContentPadding(0.0f) 
										.OnClicked(this, &SMeshPaint::FindTextureInContentBrowserButtonClicked)
										.IsEnabled( this, &SMeshPaint::IsSelectedTextureValid)
										[
											SNew(SImage) 
											.Image( FEditorStyle::GetBrush("MeshPaint.FindInCB") )
											.ColorAndOpacity( FSlateColor::UseForeground())
										]
									]
									+SHorizontalBox::Slot()
									.AutoWidth() 
									.Padding(2.0f, 0.0f) 
									.HAlign(HAlign_Right)
									.VAlign(VAlign_Center)
									[
										SNew(SButton) 
										.ToolTipText( LOCTEXT("CommitTextureChanges_ToolTip", "Commits changes to the texture") )
										.ContentPadding(0.0f) 
										.OnClicked(this, &SMeshPaint::CommitTextureChangesButtonClicked )
										.IsEnabled(this, &SMeshPaint::AreThereChangesToCommit)
										[
											SNew(SImage)
											.Image( FEditorStyle::GetBrush("MeshPaint.CommitChanges") )
											.ColorAndOpacity( FSlateColor::UseForeground())
										]
									]
									+SHorizontalBox::Slot()
									.AutoWidth() 
									.Padding(2.0f, 0.0f) 
									.HAlign(HAlign_Right)
									.VAlign(VAlign_Center)
									[
										SNew(SButton)
										.ToolTipText( LOCTEXT("SaveDirtyPackges", "Saves dirty source texture assets associated with current actor selection") )
										.ContentPadding(0.0f) 
										.OnClicked(this, &SMeshPaint::SaveTextureButtonClicked)
										.IsEnabled(this, &SMeshPaint::IsSelectedTextureDirty)
										[
											SNew(SImage) 
											.Image( FEditorStyle::GetBrush("MeshPaint.SavePackage") )
											.ColorAndOpacity( FSlateColor::UseForeground())
										]
									]
								]
							]
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(StandardPadding)
						[
							SNew(SVerticalBox)
							.Visibility(this, &SMeshPaint::GetResourceTypeTexturesVisibility)

							+SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot() 
								.Padding(2.0f, 0.0f) 
								.FillWidth(1) 
								.HAlign(HAlign_Left)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("SelectedMeshIndex", "Mesh To Paint"))
								]
								+SHorizontalBox::Slot() 
								.Padding(2.0f, 0.0f) 
								.FillWidth(1) 
								.HAlign(HAlign_Right)
								[
									SNew(SComboButton)
									.OnGetMenuContent(this, &SMeshPaint::GetActorSelectionMenu)
									.ContentPadding(2.0f)
									.IsEnabled( this, &SMeshPaint::HasMultipleActorsSelected )
									.ToolTipText( LOCTEXT("SelectMeshToolTip", "Controls which Mesh is edited") )
									.ButtonContent()
									[
										SNew(STextBlock)
										.Text(this, &SMeshPaint::GetEditingActorLabel)
									]
								]
							]

							+SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot() 
								.Padding(2.0f, 0.0f) 
								.FillWidth(1) 
								.HAlign(HAlign_Left)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("SelectedMaterialIndex", "Material To Paint"))
								]
								+SHorizontalBox::Slot() 
								.Padding(2.0f, 0.0f) 
								.FillWidth(1) 
								.HAlign(HAlign_Right)
								[
									SNew(SComboButton)
									.OnGetMenuContent(this, &SMeshPaint::GetMaterialSelectionMenu)
									.ContentPadding(2.0f)
									.IsEnabled( this, &SMeshPaint::HasMultipleMaterialSlots )
									.ToolTipText( LOCTEXT("SelectMaterialIndexToolTip", "Controls which material is edited") )
									.ButtonContent()
									[
										SNew(STextBlock)
										.Text(this, &SMeshPaint::GetEditingMaterial)
									]
								]
							]

							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(StandardPadding)
							[
								// Texture Target Selection
								SNew(SComboButton)
								.OnGetMenuContent(this, &SMeshPaint::GetTextureTargets)
								.ContentPadding(2.0f)
								.IsEnabled( this, &SMeshPaint::IsSelectedTextureValid )
								.ButtonContent()
								[
									GetTextureTargetWidget(NULL)
								]
							]
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(StandardPadding)
						[
							SNew(SVerticalBox)
							.Visibility(this, &SMeshPaint::GetResourceTypeVerticesVisibility )
							+SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot() 
								.Padding(2.0f, 0.0f) 
								.FillWidth(1) 
								.HAlign(HAlign_Left)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("MeshPaint_VertexPaintModeLabel", "Mode"))
								]
								+SHorizontalBox::Slot()
								.AutoWidth() 
								.Padding(0.0f, 0.0f, 2.0f, 0.0f) 
								.HAlign(HAlign_Right)
								[
									SNew(SVertexPaintModeRadioButton)
									.Mode(FMeshPaintSettings::Get().PaintMode)
									.OnSelectionChanged(this, &SMeshPaint::OnVertexPaintModeChanged)
								]
							]
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(StandardPadding)
						[
							SNew(SSeparator)
							.Orientation(Orient_Horizontal)
						]
						// Color Mode
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(StandardPadding)
						[
							SNew(SVerticalBox)
							.Visibility(this, &SMeshPaint::GetVertexPaintModeVisibility, true )

							+SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SHorizontalBox)
								.Visibility(this, &SMeshPaint::GetResourceTypeVerticesVisibility )
								// Fill
								+SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(0.0f, 0.0f, 4.0f, 0.0f)
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Center)
								[
									SNew(SButton)
									.ToolTipText( LOCTEXT("FillCurrentColorOnObject", "Fills the current color on the selected object") )
									.ContentPadding(0.0f) 
									.OnClicked(this, &SMeshPaint::FillInstanceVertexColorsButtonClicked)
									.Content()
									[
										SNew(SImage) 
											.Image( FEditorStyle::GetBrush("MeshPaint.Fill") )
											.ColorAndOpacity( FSlateColor::UseForeground() )
									]
								]
								// Copy instance vertex colors to source mesh 
								+SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(0.0f, 0.0f, 4.0f, 0.0f)
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Center)
								[
									SNew(SButton)
									.ToolTipText( LOCTEXT("CopyInstanceVertexColorsToSourceMesh_ToolTip", "Copies instance vertex colors to the source mesh. Disabled if multiple instances of the same source mesh are selected.") )
									.ContentPadding(0.0f) 
									.OnClicked(this, &SMeshPaint::PushInstanceVertexColorsToMeshButtonClicked)
									.Visibility(this, &SMeshPaint::GetPushInstanceVertexColorsToMeshButtonVisibility )
									.IsEnabled(this, &SMeshPaint::IsPushInstanceVertexColorsToMeshButtonEnabled )
									.Content()
									[
										SNew(SImage) .Image( FEditorStyle::GetBrush("MeshPaint.CopyInstVertColors") )
									]
								]
								// Import vertex colors from TGA
								+SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(0.0f, 0.0f, 4.0f, 0.0f)
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Center)
								[
									SNew(SButton)
									.ToolTipText( LOCTEXT("ImportVertexColorsFromTarga_ToolTip", "Import Vertex Colors from TGA") )
									.ContentPadding(0.0f) 
									.OnClicked(this, &SMeshPaint::ImportVertexColorsFromTGAButtonClicked)
									.Content()
									[
										SNew(SImage) 
											.Image( FEditorStyle::GetBrush("MeshPaint.ImportVertColors") )
											.ColorAndOpacity( FSlateColor::UseForeground() )
									]
								]
								// Empty space
								+SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.HAlign(HAlign_Center)
								[
									SNew(SSpacer)
								]
								// Find in CB
								+SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(0.0f, 0.0f, 4.0f, 0.0f)
								.HAlign(HAlign_Right)
								.VAlign(VAlign_Center)
								[
									SNew(SButton)
									.ToolTipText( LOCTEXT("FindSourceMeshInContentBrowser_ToolTip", "Find source mesh in content browser") )
									.ContentPadding(0.0f) 
									.OnClicked(this, &SMeshPaint::FindVertexPaintMeshInContentBrowserButtonClicked)
									.Content()
									[
										SNew(SImage) 
											.Image( FEditorStyle::GetBrush("MeshPaint.FindInCB") )
											.ColorAndOpacity( FSlateColor::UseForeground() )
									]
								]
								// Save package
								+SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(0.0f, 0.0f, 4.0f, 0.0f)
								.HAlign(HAlign_Right)
								.VAlign(VAlign_Center)
								[
									SNew(SButton)
									.ToolTipText( LOCTEXT("SaveDirtyPackages_ToolTip", "Saves dirty source mesh packages associated with current actor selection") )
									.ContentPadding(0.0f) 
									.OnClicked(this, &SMeshPaint::SaveVertexPaintPackageButtonClicked)
									.IsEnabled(this, &SMeshPaint::IsSaveVertexPaintPackageButtonEnabled)
									.Content()
									[
										SNew(SImage) 
											.Image( FEditorStyle::GetBrush("MeshPaint.SavePackage") )
											.ColorAndOpacity( FSlateColor::UseForeground() )
									]
								]
							]
							// Import Vertex Colors from TGA
							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(StandardPadding)
							[
								SNew(SVerticalBox)
								.Visibility(this, &SMeshPaint::GetImportVertexColorsVisibility)
								+SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(SImportVertexColorsFromTGA, MeshPaintEditMode)
								]
							]
							// Erase/Paint color radio button
							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(StandardPadding*2.0f)
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.FillWidth(1)
								[
									SNew(SVertexPaintColorSetRadioButton)
									.Color(PaintColorSet)
									.OnSelectionChanged(this, &SMeshPaint::OnVertexPaintColorSetChanged)
								]
							]
							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(StandardPadding)
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.FillWidth(1)
								.HAlign(HAlign_Left)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("MeshPaint_ColorChannels", "Channels"))
								]
								+SHorizontalBox::Slot()
								.AutoWidth()
								.HAlign(HAlign_Right)
								[
									SNew(SHorizontalBox)
									+SHorizontalBox::Slot()
									.AutoWidth() 
									.Padding(2.0f, 0.0f)
									[
										SNew(SCheckBox)
										.IsChecked( this, &SMeshPaint::IsWriteColorChannelChecked, EMeshPaintWriteColorChannels::Red )
										.OnCheckStateChanged( this, &SMeshPaint::OnWriteColorChannelChanged, EMeshPaintWriteColorChannels::Red )
										[
											SNew(STextBlock)
											.Text(LOCTEXT("MeshPaint_ColorChannelsRed", "Red"))
										]	
									]
									+SHorizontalBox::Slot()
									.AutoWidth() 
									.Padding(2.0f, 0.0f)
									[
										SNew(SCheckBox)
										.IsChecked( this, &SMeshPaint::IsWriteColorChannelChecked, EMeshPaintWriteColorChannels::Green )
										.OnCheckStateChanged( this, &SMeshPaint::OnWriteColorChannelChanged, EMeshPaintWriteColorChannels::Green )
										[
											SNew(STextBlock)
											.Text(LOCTEXT("MeshPaint_ColorChannelsGreen", "Green"))
										]
									]
									+SHorizontalBox::Slot()
									.AutoWidth() 
									.Padding(2.0f, 0.0f)
									[
										SNew(SCheckBox)
										.IsChecked( this, &SMeshPaint::IsWriteColorChannelChecked, EMeshPaintWriteColorChannels::Blue)
										.OnCheckStateChanged( this, &SMeshPaint::OnWriteColorChannelChanged, EMeshPaintWriteColorChannels::Blue )
										[
											SNew(STextBlock)
											.Text(LOCTEXT("MeshPaint_ColorChannels_Blue", "Blue"))
										]
									]
									+SHorizontalBox::Slot()
									.AutoWidth() 
									.Padding(2.0f, 0.0f)
									[
										SNew(SCheckBox)
										.IsChecked( this, &SMeshPaint::IsWriteColorChannelChecked, EMeshPaintWriteColorChannels::Alpha )
										.OnCheckStateChanged( this, &SMeshPaint::OnWriteColorChannelChanged, EMeshPaintWriteColorChannels::Alpha )
										[
											SNew(STextBlock)
											.Text(LOCTEXT("MeshPaint_ColorChannelsAlpha", "Alpha"))
										]
									]
								]
							]
						]
						// Blend Weight mode
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(StandardPadding)
						[
							SNew(SVerticalBox)
							.Visibility(this, &SMeshPaint::GetVertexPaintModeVisibility, false )
							+SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot() 
								.Padding(2.0f, 0.0f) 
								.FillWidth(1) 
								.HAlign(HAlign_Left)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("MeshPaint_BlendWeightTextureCount", "Texture count"))
								]
								+SHorizontalBox::Slot() 
								.Padding(2.0f, 0.0f) 
								.FillWidth(2) 
								.HAlign(HAlign_Right)
								[
									SNew(SComboButton)
									.OnGetMenuContent(this, &SMeshPaint::GetTotalWeightCountMenu)
									.ContentPadding(2.0f)
									.ButtonContent()
									[
										SNew(STextBlock)
										.Text(this, &SMeshPaint::GetTotalWeightCountSelection)
									]
								]
							]
							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(StandardPadding)
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot()
									.AutoHeight()
									.Padding(0.0f, 0.0f, 0.0f, 8.0f)
									[
										SNew(STextBlock)
										.Text(LOCTEXT("MeshPaint_BlendWeightPaintTexture", "Paint texture"))
									]
									+SVerticalBox::Slot()
									.AutoHeight()
									[
										SNew(STextBlock)
										.Text(LOCTEXT("MeshPaint_BlendWeightEraseTexture", "Erase texture"))
									]
								]
								+SHorizontalBox::Slot()
								.Padding(4.0f, 0.0f, 0.0f, 0.0f)
								.FillWidth(1)
								.HAlign(HAlign_Right)
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot()
									.AutoHeight()
									.Padding(0.0f, 0.0f, 0.0f, 8.0f)
									[
										SNew(SVertexPaintWeightRadioButton)
										.WeightIndex(this, &SMeshPaint::GetPaintWeightIndex)
										.OnSelectionChanged(this, &SMeshPaint::OnPaintWeightChanged)
									]
									+SVerticalBox::Slot()
									.AutoHeight()
									[
										SNew(SVertexPaintWeightRadioButton)
										.WeightIndex(this, &SMeshPaint::GetEraseWeightIndex)
										.OnSelectionChanged(this, &SMeshPaint::OnEraseWeightChanged)
									]
								]
								+SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(4.0f, 0.0f)
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Center)
								[
									SNew(SButton)
									.ToolTipText( LOCTEXT("MeshPaint_SwapToolTip", "Swap") )
									.ContentPadding(0.0f) 
									.OnClicked(this, &SMeshPaint::SwapPaintAndEraseWeightButtonClicked)
									.Content()
									[
										SNew(SImage) .Image( FEditorStyle::GetBrush("MeshPaint.Swap") )
									]
								]
							]

						]
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(StandardPadding)
						[
							SNew(SSeparator)
							.Orientation(Orient_Horizontal)
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(StandardPadding)
						[
							SNew(SVerticalBox)
							// Radius
							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(StandardPadding)
							[
								SNew( SHorizontalBox )
								+SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("MeshPaint_LabelRadius", "Radius"))
								]
								+SHorizontalBox::Slot()
								.FillWidth(2.0f)
								.HAlign(HAlign_Fill)
								.VAlign(VAlign_Center)
								[
									SNew(SNumericEntryBox<float>)					
									.AllowSpin(true)
									.MinSliderValue(MinBrushSliderRadius)
									.MaxSliderValue(MaxBrushSliderRadius)
									.MinValue(MinBrushRadius)
									.MaxValue(MaxBrushRadius)
									.Value(this, &SMeshPaint::GetBrushRadius)
									.OnValueChanged(this, &SMeshPaint::OnBrushRadiusChanged)
								]
							]
							// Strength
							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(StandardPadding)
							[
								SNew( SHorizontalBox )
								+SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("MeshPaint_LabelStrength", "Strength"))
								]
								+SHorizontalBox::Slot()
								.FillWidth(2.0f)
								.HAlign(HAlign_Fill)
								.VAlign(VAlign_Center)
								[
									SNew(SNumericEntryBox<float>)					
									.AllowSpin(true)
									.MinSliderValue(0.0f)
									.MaxSliderValue(1.0f)
									.MinValue(0.0f)
									.MaxValue(1.0f)
									.Value(this, &SMeshPaint::GetBrushStrength)
									.OnValueChanged(this, &SMeshPaint::OnBrushStrengthChanged)
								]
							]
							// Falloff
							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(StandardPadding)
							[
								SNew( SHorizontalBox )
								+SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("MeshPaint_LabelFalloff", "Falloff"))
								]
								+SHorizontalBox::Slot()
								.FillWidth(2.0f)
								.HAlign(HAlign_Fill)
								.VAlign(VAlign_Center)
								[
									SNew(SNumericEntryBox<float>)					
									.AllowSpin(true)
									.MinSliderValue(0.0f)
									.MaxSliderValue(1.0f)
									.MinValue(0.0f)
									.MaxValue(1.0f)
									.Value(this, &SMeshPaint::GetBrushFalloffAmount)
									.OnValueChanged(this, &SMeshPaint::OnBrushFalloffAmountChanged)
								]
							]
							// Enable brush flow
							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(StandardPadding)
							[
								SNew( SHorizontalBox )
								+SHorizontalBox::Slot()
								.AutoWidth()
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("MeshPaint_LabelEnableFlow", "Enable brush flow"))
								]
								+SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.HAlign(HAlign_Right)
								.VAlign(VAlign_Center)
								[
									SNew( SCheckBox )
									.IsChecked( this, &SMeshPaint::IsEnableFlowChecked )
									.OnCheckStateChanged( this, &SMeshPaint::OnEnableFlowChanged )
								]
							]
							// Flow amount
							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(StandardPadding)
							[
								SNew( SHorizontalBox )
								+SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("MeshPaint_LabelFlow", "Flow"))
								]
								+SHorizontalBox::Slot()
								.FillWidth(2.0f)
								.HAlign(HAlign_Fill)
								.VAlign(VAlign_Center)
								[
									SNew(SNumericEntryBox<float>)
									.AllowSpin(true)
									.MinSliderValue(0.0f)
									.MaxSliderValue(1.0f)
									.MinValue(0.0f)
									.MaxValue(1.0f)
									.Value(this, &SMeshPaint::GetFlowAmount)
									.OnValueChanged(this, &SMeshPaint::OnFlowAmountChanged)
								]
							]
							// Ignore back-facing
							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(StandardPadding)
							[
								SNew( SHorizontalBox )
								.Visibility(this, &SMeshPaint::GetResourceTypeVerticesVisibility )
								+SHorizontalBox::Slot()
								.AutoWidth()
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("MeshPaint_LabelIgnoreBackface", "Ignore back-facing"))
								]
								+SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.HAlign(HAlign_Right)
								.VAlign(VAlign_Center)
								[
									SNew( SCheckBox )
									.IsChecked( this, &SMeshPaint::IsIgnoreBackfaceChecked )
									.OnCheckStateChanged( this, &SMeshPaint::OnIgnoreBackfaceChanged )
								]
							]
							// Seam Painting
							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(StandardPadding)
							[
								SNew( SHorizontalBox )
								.Visibility(this, &SMeshPaint::GetResourceTypeTexturesVisibility )
								+SHorizontalBox::Slot()
								.AutoWidth()
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("MeshPaint_LabelSeamPainting", "Seam Painting"))
								]
								+SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.HAlign(HAlign_Right)
								.VAlign(VAlign_Center)
								[
									SNew( SCheckBox )
									.IsChecked( this, &SMeshPaint::IsSeamPaintingChecked )
									.OnCheckStateChanged( this, &SMeshPaint::OnSeamPaintingChanged )
								]
							]
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(StandardPadding)
						[
							SNew(SHorizontalBox)
							.Visibility(this, &SMeshPaint::GetResourceTypeVerticesVisibility )
							+SHorizontalBox::Slot() 
							.Padding(2.0f, 0.0f) 
							.FillWidth(1) 
							.HAlign(HAlign_Left)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("MeshPaint_VertexColorViewLabel", "View"))
							]
							+SHorizontalBox::Slot()
							.AutoWidth() 
							.Padding(0.0f, 0.0f, 2.0f, 0.0f) 
							.HAlign(HAlign_Right)
							[
								SNew(SVertexPaintColorViewRadioButton)
								.Mode(FMeshPaintSettings::Get().ColorViewMode)
								.OnSelectionChanged(this, &SMeshPaint::OnVertexPaintColorViewModeChanged)
							]
						]
					]
				]
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SMeshPaint::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	MeshPaintEditMode->UpdateTexturePaintTargetList();
}

class FEdModeMeshPaint* SMeshPaint::GetEditorMode() const
{
	return MeshPaintEditMode;
}

EVisibility SMeshPaint::GetResourceTypeVerticesVisibility() const
{
	return FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::VertexColors ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SMeshPaint::GetResourceTypeTexturesVisibility() const
{
	return FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::Texture ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SMeshPaint::GetVertexPaintModeVisibility(bool bColorPanel) const
{
	bool bShowColorPanel = bColorPanel && ( FMeshPaintSettings::Get().PaintMode == EMeshPaintMode::PaintColors || FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::Texture );
	bool bShowWeightPanel = !bColorPanel && FMeshPaintSettings::Get().PaintMode == EMeshPaintMode::PaintWeights && FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::VertexColors;

	return (bShowColorPanel || bShowWeightPanel) 
		? EVisibility::Visible 
		: EVisibility::Collapsed;
}

EVisibility SMeshPaint::GetImportVertexColorsVisibility() const
{
	return bShowImportOptions 
		? EVisibility::Visible 
		: EVisibility::Collapsed;
}

TOptional<float> SMeshPaint::GetBrushRadius() const
{
	return MeshPaintEditMode->GetBrushRadiiDefault();
}

TOptional<float> SMeshPaint::GetBrushStrength() const
{
	return FMeshPaintSettings::Get().BrushStrength;
}

TOptional<float> SMeshPaint::GetBrushFalloffAmount() const
{
	return FMeshPaintSettings::Get().BrushFalloffAmount;
}

TOptional<float> SMeshPaint::GetFlowAmount() const
{
	return FMeshPaintSettings::Get().FlowAmount;
}

int32 SMeshPaint::GetPaintWeightIndex() const
{
	return FMeshPaintSettings::Get().PaintWeightIndex;
}

int32 SMeshPaint::GetEraseWeightIndex() const
{
	return FMeshPaintSettings::Get().EraseWeightIndex;
}

FText SMeshPaint::GetInstanceVertexColorsText() const 
{
	FText Text = LOCTEXT("MeshPaint_InstVertexColorsStartText", "None");
	int32 NumBaseVertexColorBytes = 0;
	int32 NumInstanceVertexColorBytes = 0;
	bool bHasInstanceMaterialAndTexture = false;
	MeshPaintEditMode->GetSelectedMeshInfo( NumBaseVertexColorBytes, NumInstanceVertexColorBytes, bHasInstanceMaterialAndTexture );

	if( NumInstanceVertexColorBytes > 0 )
	{
		float VertexKiloBytes = NumInstanceVertexColorBytes / 1000.0f;
		static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(3)
			.SetMaximumFractionalDigits(3);
		Text = FText::Format( LOCTEXT("InstanceVertexColorsKiloBytesFmt", "{0} k"), FText::AsNumber(VertexKiloBytes, &FormatOptions) );
	}
	return Text;
}

void SMeshPaint::OntMeshPaintResourceChanged(EMeshPaintResource::Type InPaintResource)
{
	FMeshPaintSettings::Get().ResourceType = InPaintResource;
}

void SMeshPaint::OntVertexPaintTargetChanged(EMeshVertexPaintTarget::Type InVertexPaintTarget)
{
	FMeshPaintSettings::Get().VertexPaintTarget = InVertexPaintTarget;
}

void SMeshPaint::OnVertexPaintModeChanged(EMeshPaintMode::Type InPaintMode)
{
	FMeshPaintSettings::Get().PaintMode = InPaintMode;
}

void SMeshPaint::OnVertexPaintColorViewModeChanged(EMeshPaintColorViewMode::Type InColorViewMode)
{
	FMeshPaintSettings::Get().ColorViewMode = InColorViewMode;
}

void SMeshPaint::OnVertexPaintColorSetChanged(EMeshPaintColorSet::Type InPaintColorSet)
{
	PaintColorSet = InPaintColorSet;
}

void SMeshPaint::OnBrushRadiusChanged(float InBrushRadius)
{
	MeshPaintEditMode->SetBrushRadiiDefault( InBrushRadius );
}

void SMeshPaint::OnBrushStrengthChanged(float InBrushStrength)
{
	FMeshPaintSettings::Get().BrushStrength = InBrushStrength;
}

void SMeshPaint::OnBrushFalloffAmountChanged(float InBrushFalloffAmount)
{
	FMeshPaintSettings::Get().BrushFalloffAmount = InBrushFalloffAmount;
}

void SMeshPaint::OnFlowAmountChanged(float InFlowAmount)
{
	FMeshPaintSettings::Get().FlowAmount = InFlowAmount;
}

ECheckBoxState SMeshPaint::IsIgnoreBackfaceChecked() const
{
	return (FMeshPaintSettings::Get().bOnlyFrontFacingTriangles)
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

void SMeshPaint::OnIgnoreBackfaceChanged(ECheckBoxState InCheckState)
{
	FMeshPaintSettings::Get().bOnlyFrontFacingTriangles = (InCheckState == ECheckBoxState::Checked);
}

ECheckBoxState SMeshPaint::IsSeamPaintingChecked() const
{
	return (FMeshPaintSettings::Get().bEnableSeamPainting)
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

void SMeshPaint::OnSeamPaintingChanged(ECheckBoxState InCheckState)
{
	FMeshPaintSettings::Get().bEnableSeamPainting = (InCheckState == ECheckBoxState::Checked);
}

ECheckBoxState SMeshPaint::IsEnableFlowChecked() const
{
	return (FMeshPaintSettings::Get().bEnableFlow)
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

void SMeshPaint::OnEnableFlowChanged(ECheckBoxState InCheckState)
{
	FMeshPaintSettings::Get().bEnableFlow = (InCheckState == ECheckBoxState::Checked);
}

void SMeshPaint::OnEraseWeightChanged(int32 InWeightIndex)
{
	FMeshPaintSettings::Get().EraseWeightIndex = InWeightIndex;
}

void SMeshPaint::OnPaintWeightChanged(int32 InWeightIndex)
{
	FMeshPaintSettings::Get().PaintWeightIndex = InWeightIndex;
}


ECheckBoxState SMeshPaint::IsWriteColorChannelChecked(EMeshPaintWriteColorChannels::Type CheckBoxInfo) const
{
	const bool bIsRedAndChecked = ( CheckBoxInfo == EMeshPaintWriteColorChannels::Red && FMeshPaintSettings::Get().bWriteRed );
	const bool bIsGreenAndChecked = ( CheckBoxInfo == EMeshPaintWriteColorChannels::Green && FMeshPaintSettings::Get().bWriteGreen );
	const bool bIsBlueAndChecked = ( CheckBoxInfo == EMeshPaintWriteColorChannels::Blue && FMeshPaintSettings::Get().bWriteBlue );
	const bool bIsAlphaAndChecked = (CheckBoxInfo == EMeshPaintWriteColorChannels::Alpha && FMeshPaintSettings::Get().bWriteAlpha);

	const bool bIsChecked = ( bIsRedAndChecked || bIsGreenAndChecked || bIsBlueAndChecked || bIsAlphaAndChecked );
	return bIsChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SMeshPaint::OnWriteColorChannelChanged(ECheckBoxState InNewValue, EMeshPaintWriteColorChannels::Type CheckBoxInfo)
{
	bool bIsCheckedState = (InNewValue == ECheckBoxState::Checked);
	if(CheckBoxInfo == EMeshPaintWriteColorChannels::Red)
	{
		FMeshPaintSettings::Get().bWriteRed = bIsCheckedState;
	}
	else if( CheckBoxInfo == EMeshPaintWriteColorChannels::Green)
	{
		FMeshPaintSettings::Get().bWriteGreen = bIsCheckedState;
	}
	else if( CheckBoxInfo == EMeshPaintWriteColorChannels::Blue)
	{
		FMeshPaintSettings::Get().bWriteBlue = bIsCheckedState;
	}
	else if( CheckBoxInfo == EMeshPaintWriteColorChannels::Alpha)
	{
		FMeshPaintSettings::Get().bWriteAlpha = bIsCheckedState;
	}
}

FReply SMeshPaint::FillInstanceVertexColorsButtonClicked()
{
	MeshPaintEditMode->FillInstanceVertexColors();
	return FReply::Handled();
}

FReply SMeshPaint::PushInstanceVertexColorsToMeshButtonClicked()
{
	MeshPaintEditMode->PushInstanceVertexColorsToMesh();
	return FReply::Handled();
}

FReply SMeshPaint::ImportVertexColorsFromTGAButtonClicked()
{
	bShowImportOptions = !bShowImportOptions;
	return FReply::Handled();
}

FReply SMeshPaint::SaveVertexPaintPackageButtonClicked()
{
	TArray<UObject*> StaticMeshesToSave;
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = CastChecked<AActor>( *It );

		UStaticMeshComponent* StaticMeshComponent = NULL;
		AStaticMeshActor* StaticMeshActor = Cast< AStaticMeshActor >( Actor );
		if( StaticMeshActor != NULL )
		{
			StaticMeshComponent = StaticMeshActor->GetStaticMeshComponent();
		}

		if( StaticMeshComponent != NULL && StaticMeshComponent->StaticMesh != NULL )
		{
			StaticMeshesToSave.Add( StaticMeshComponent->StaticMesh );
		}
	}

	if( StaticMeshesToSave.Num() > 0 )
	{
		SavePackagesForObjects( StaticMeshesToSave );
	}
	return FReply::Handled();
}

FReply SMeshPaint::FindVertexPaintMeshInContentBrowserButtonClicked()
{
	GEditor->SyncToContentBrowser();
	return FReply::Handled();
}

FReply SMeshPaint::FindTextureInContentBrowserButtonClicked()
{
	MeshPaintEditMode->FindSelectedTextureInContentBrowser();
	return FReply::Handled();
}

FReply SMeshPaint::CommitTextureChangesButtonClicked()
{
	CommitPaintChanges();
	return FReply::Handled();
}

FReply SMeshPaint::SaveTextureButtonClicked()
{
	UTexture2D* SelectedTexture = MeshPaintEditMode->GetSelectedTexture();

	if( NULL != SelectedTexture )
	{
		TArray<UObject*> TexturesToSaveArray;
		TexturesToSaveArray.Add( SelectedTexture );

		SavePackagesForObjects( TexturesToSaveArray );
		//RefreshAllProperties();
	}
	return FReply::Handled();
}

FReply SMeshPaint::NewTextureButtonClicked()
{
	MeshPaintEditMode->CreateNewTexture();
	return FReply::Handled();
}

FReply SMeshPaint::DuplicateTextureButtonClicked()
{
	MeshPaintEditMode->DuplicateTextureMaterialCombo();
	return FReply::Handled();
}

FReply SMeshPaint::RemoveInstanceVertexColorsButtonClicked()
{
	MeshPaintEditMode->RemoveInstanceVertexColors();
	return FReply::Handled();
}

FReply SMeshPaint::FixInstanceVertexColorsButtonClicked()
{
	MeshPaintEditMode->FixupInstanceVertexColors();
	return FReply::Handled();
}

FReply SMeshPaint::CopyInstanceVertexColorsButtonClicked()
{
	MeshPaintEditMode->CopyInstanceVertexColors();
	return FReply::Handled();
}
	
FReply SMeshPaint::PasteInstanceVertexColorsButtonClicked()
{
	MeshPaintEditMode->PasteInstanceVertexColors();
	return FReply::Handled();
}

FReply SMeshPaint::SwapPaintAndEraseWeightButtonClicked()
{
	int32 TempWeightIndex = FMeshPaintSettings::Get().PaintWeightIndex;
	FMeshPaintSettings::Get().PaintWeightIndex = FMeshPaintSettings::Get().EraseWeightIndex;
	FMeshPaintSettings::Get().EraseWeightIndex = TempWeightIndex;
	return FReply::Handled();
}

EVisibility SMeshPaint::GetPushInstanceVertexColorsToMeshButtonVisibility() const
{
	return IsVertexPaintTargetComponentInstance() 
		? EVisibility::Visible 
		: EVisibility::Collapsed;
}

bool SMeshPaint::IsPushInstanceVertexColorsToMeshButtonEnabled() const
{
	bool bIsEnabled = true;

	int32 NumBaseVertexColorBytes = 0;
	int32 NumInstanceVertexColorBytes = 0;
	bool bHasInstanceMaterialAndTexture = false;

	const bool bMeshSelected = MeshPaintEditMode->GetSelectedMeshInfo( NumBaseVertexColorBytes, NumInstanceVertexColorBytes, bHasInstanceMaterialAndTexture );
	if( !bMeshSelected || NumInstanceVertexColorBytes <= 0 )
	{
		bIsEnabled = false;
	}
	else if( bMeshSelected && NumInstanceVertexColorBytes > 0 )
	{
		// If we have any instances that point to the same source mesh we disable the button because we can't multiple sets of instance data to a single source mesh.
		USelection& SelectedActors = *GEditor->GetSelectedActors();
		for( int32 LeftCompareIndex = 0; (LeftCompareIndex < SelectedActors.Num() - 1) && bIsEnabled; ++LeftCompareIndex )
		{
			AActor* LeftCompareActor = Cast<AActor>( SelectedActors.GetSelectedObject( LeftCompareIndex ) );
			if (LeftCompareActor == nullptr)
			{
				continue;
			}

			UStaticMeshComponent* LeftStaticMeshComponent = nullptr;
			AStaticMeshActor* LeftStaticMeshActor = Cast< AStaticMeshActor >( LeftCompareActor );
			if( LeftStaticMeshActor != nullptr )
			{
				LeftStaticMeshComponent = LeftStaticMeshActor->GetStaticMeshComponent();
			}

			if( LeftStaticMeshComponent != nullptr && LeftStaticMeshComponent->StaticMesh != nullptr )
			{
				// Check the left static mesh to the static meshes of all the other selected actors for a match
				for( int32 RightCompareIndex = LeftCompareIndex + 1; (RightCompareIndex < SelectedActors.Num()) && bIsEnabled; ++RightCompareIndex )
				{
					AActor* RightCompareActor = Cast<AActor>( SelectedActors.GetSelectedObject( RightCompareIndex ) );
					if (RightCompareActor == nullptr)
					{
						continue;
					}

					UStaticMeshComponent* RightStaticMeshComponent = nullptr;
					AStaticMeshActor* RightStaticMeshActor = Cast< AStaticMeshActor >( RightCompareActor );
					if( RightStaticMeshActor != nullptr )
					{
						RightStaticMeshComponent = RightStaticMeshActor->GetStaticMeshComponent();
					}
					
					if(RightStaticMeshComponent != nullptr && RightStaticMeshComponent->StaticMesh != nullptr)
					{
						if(LeftStaticMeshComponent->StaticMesh == RightStaticMeshComponent->StaticMesh)
						{
							// We found more than one actor that point to the same static mesh so we can't perform this operation.  Disable to button.  This will
							//  also stop our duplicate checking since the bool is used in the loop control
							bIsEnabled = false;
							break;
						}
					}
				}
			}
		}
	}
	return bIsEnabled;
}

bool SMeshPaint::IsSaveVertexPaintPackageButtonEnabled() const
{
	for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ); It; ++It )
	{
		AActor* Actor = CastChecked<AActor>( *It );

		UStaticMeshComponent* StaticMeshComponent = NULL;
		AStaticMeshActor* StaticMeshActor = Cast< AStaticMeshActor >( Actor );
		if( StaticMeshActor != NULL )
		{
			StaticMeshComponent = StaticMeshActor->GetStaticMeshComponent();
		}

		if( StaticMeshComponent != NULL && StaticMeshComponent->StaticMesh != NULL )
		{
			if( StaticMeshComponent->StaticMesh->GetOutermost()->IsDirty() )
			{
				return true;
			}
		}
	}
	return false;
}

bool SMeshPaint::IsVertexPaintTargetComponentInstance() const
{
	return (FMeshPaintSettings::Get().VertexPaintTarget == EMeshVertexPaintTarget::ComponentInstance);
}

void SMeshPaint::CommitPaintChanges()
{
	MeshPaintEditMode->CommitAllPaintedTextures();
}

bool SMeshPaint::IsSelectedTextureDirty() const 
{
	UTexture2D* SelectedTexture = MeshPaintEditMode->GetSelectedTexture();
	if( NULL != SelectedTexture )
	{
		return SelectedTexture->GetOutermost()->IsDirty() ? true : false;
	}

	return false;
}

bool SMeshPaint::AreThereChangesToCommit() const
{
	return MeshPaintEditMode->GetNumberOfPendingPaintChanges() > 0;
}

bool SMeshPaint::IsSelectedTextureValid() const 
{
	UTexture2D* SelectedTexture = MeshPaintEditMode->GetSelectedTexture();
	bool RetVal = SelectedTexture != NULL ? true : false;
	return RetVal;
}

bool SMeshPaint::HasInstanceVertexColors() const
{
	int32 NumBaseVertexColorBytes = 0;
	int32 NumInstanceVertexColorBytes = 0;
	bool bHasInstanceMaterialAndTexture = false;
	MeshPaintEditMode->GetSelectedMeshInfo( NumBaseVertexColorBytes, NumInstanceVertexColorBytes, bHasInstanceMaterialAndTexture );	// Out
	return ( NumInstanceVertexColorBytes > 0 );
}

bool SMeshPaint::RequiresInstanceVertexColorsFixup() const
{
	return MeshPaintEditMode->RequiresInstanceVertexColorsFixup();
}

bool SMeshPaint::CanCopyToColourBufferCopy() const
{
	// Only allow copying of a single mesh's color data
	if( GEditor->GetSelectedActors()->Num() != 1 )
	{
		return false;
	}

	// Check to see whether or not this mesh has instanced color data...
	int32 NumBaseVertexColorBytes = 0;
	int32 NumInstanceVertexColorBytes = 0;
	bool bHasInstanceMaterialAndTexture = false;
	MeshPaintEditMode->GetSelectedMeshInfo( NumBaseVertexColorBytes, NumInstanceVertexColorBytes, bHasInstanceMaterialAndTexture );	// Out
	int32 NumVertexColorBytes = NumBaseVertexColorBytes + NumInstanceVertexColorBytes;

	// If there is any instanced color data, we can copy it...
	return( NumVertexColorBytes > 0 );
}

bool SMeshPaint::CanCreateInstanceMaterialAndTexture() const 
{
	int32 NumBaseVertexColorBytes = 0;
	int32 NumInstanceVertexColorBytes = 0;
	bool bHasInstanceMaterialAndTexture = false;
	bool bAnyValidMeshes = MeshPaintEditMode->GetSelectedMeshInfo( NumBaseVertexColorBytes, NumInstanceVertexColorBytes, bHasInstanceMaterialAndTexture );	// Out
	return bAnyValidMeshes && !bHasInstanceMaterialAndTexture;
}

bool SMeshPaint::CanPasteFromColourBufferCopy() const
{
	return MeshPaintEditMode->CanPasteVertexColors();
}

TSharedRef<SWidget> SMeshPaint::GetTotalWeightCountMenu()
{
	FMenuBuilder MenuBuilder(true, NULL);
	for(int32 WeightCount = 2; WeightCount <= 5; WeightCount++)
	{
		MenuBuilder.AddMenuEntry(GetTotalWeightCountText(WeightCount), FText(), FSlateIcon(), FExecuteAction::CreateSP(this, &SMeshPaint::OnChangeTotalWeightCount, WeightCount));
	}
	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SMeshPaint::GetUVChannels()
{
	FMenuBuilder MenuBuilder(true, NULL);
	for (int32 UVSet=0; UVSet < MeshPaintEditMode->GetMaxNumUVSets(); ++UVSet)
	{
		MenuBuilder.AddMenuEntry( FText::AsNumber( UVSet ), FText(), FSlateIcon(), FExecuteAction::CreateSP(this, &SMeshPaint::OnChangeUVChannel, UVSet) );
	}
	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SMeshPaint::GetTextureTargets()
{
	TSharedRef<SScrollBox> ScrollBox = SNew(SScrollBox);

	TSharedRef<SWidget> MenuContent = 
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2.0f, 0.0f) 
		.AutoHeight()
		.MaxHeight(450.0f)
		[
			ScrollBox
		];

	TArray<FTextureTargetListInfo>* TextList = MeshPaintEditMode->GetTexturePaintTargetList();
	for (auto It = TextList->CreateConstIterator(); It; It++)
	{
		ScrollBox->AddSlot()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(2.0f, 0.0, 8.0f, 0.0)
			[
				SNew( SButton )
				.OnClicked(this,  &SMeshPaint::OnChangeTextureTarget, TWeakObjectPtr<UTexture2D>(It->TextureData) )
				[
					GetTextureTargetWidget( It->TextureData )
				]
			]
		];
	}
	return MenuContent;
}

TSharedRef<SWidget> SMeshPaint::GetTextureTargetWidget( UTexture2D* TextureData )
{
	return 
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding(2.0f, 0.0f) 
		.FillWidth(1) 
		.HAlign(HAlign_Left)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text( this, &SMeshPaint::GetCurrentTextureTargetText, TextureData, 0 )
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text( this, &SMeshPaint::GetCurrentTextureTargetText, TextureData, 1 )
			]			
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 2.0f, 0.0f) 
		.HAlign(HAlign_Right)
		[
			SNew(SImage)
			.Image( this, &SMeshPaint::GetCurrentTextureTargetImage, TextureData )
		];
}

FText SMeshPaint::GetCurrentTextureTargetText( UTexture2D* TextureData, int index ) const
{
	UTexture2D* Tex = TextureData != NULL ? TextureData :  MeshPaintEditMode->GetSelectedTexture();

	if( Tex == NULL ) 
	{
		return FText::GetEmpty();
	}

	switch ( index )
	{
		case 0:	
			return FText::FromString(Tex->GetName());
		case 1:	
			return FText::FromString(Tex->GetDesc());
		default:
			return FText::GetEmpty();
	}
}

const FSlateBrush* SMeshPaint::GetCurrentTextureTargetImage(UTexture2D* TextureData) const
{
	UTexture2D* Tex = TextureData != NULL ? TextureData :  MeshPaintEditMode->GetSelectedTexture();
	return Tex != NULL ? new FSlateDynamicImageBrush( Tex, FVector2D(64, 64), Tex->GetFName() ) : NULL;
}

FText SMeshPaint::GetCurrentUVChannel() const
{
	UTexture2D* Tex = MeshPaintEditMode->GetSelectedTexture();
	return FText::AsNumber( FMeshPaintSettings::Get().UVChannel );	
}

FReply SMeshPaint::OnChangeTextureTarget( TWeakObjectPtr<UTexture2D> TextureData )
{
	MeshPaintEditMode->SetSelectedTexture( &(*TextureData) );
	FSlateApplication::Get().DismissAllMenus();
	return FReply::Handled();
}

void SMeshPaint::OnChangeUVChannel(int32 Channel)
{
	FMeshPaintSettings::Get().UVChannel = Channel;
}

void SMeshPaint::OnChangeTotalWeightCount(int32 InCount)
{
	FMeshPaintSettings::Get().TotalWeightCount = InCount;
}

FText SMeshPaint::GetTotalWeightCountSelection() const
{
	return GetTotalWeightCountText(FMeshPaintSettings::Get().TotalWeightCount);
}

FText SMeshPaint::GetTotalWeightCountText(int32 InCount) const
{
	FText OutText;
	switch (InCount)
	{
		case 2:
			OutText = LOCTEXT("MeshPaint_TotalWeightCount_Two", "2 (alpha lerp)");
			break;
		case 3:
			OutText = LOCTEXT("MeshPaint_TotalWeightCount_Three", "3 (RGB)");
			break;
		case 4:
			OutText = LOCTEXT("MeshPaint_TotalWeightCount_Four", "4 (ARGB)");
			break;
		case 5:
			OutText = LOCTEXT("MeshPaint_TotalWeightCount_Five", "5 (one minus ARGB)");
			break;

		default:
			break;
	}
	return OutText;
}

bool SMeshPaint::SavePackagesForObjects( TArray<UObject*>& InObjects )
{
	if( InObjects.Num() > 0 )
	{
		TArray<UPackage*> PackagesToSave;
		TArray< UPackage* > PackagesWithExternalRefs;
		FString PackageNamesWithExternalRefs;

		// Find all the dirty packages that these objects belong to
		for(int32 ObjIdx = 0; ObjIdx < InObjects.Num(); ++ObjIdx )
		{
			const UObject* CurrentObj = InObjects[ ObjIdx ];
			if( CurrentObj->GetOutermost()->IsDirty() )
			{
				PackagesToSave.AddUnique( CurrentObj->GetOutermost() );
			}
		}

		if( PackagesToSave.Num() > 0 )
		{
			if( PackageTools::CheckForReferencesToExternalPackages( &PackagesToSave, &PackagesWithExternalRefs ) )
			{
				for(int32 PkgIdx = 0; PkgIdx < PackagesWithExternalRefs.Num(); ++PkgIdx)
				{
					PackageNamesWithExternalRefs += FString::Printf(TEXT("%s\n"), *PackagesWithExternalRefs[ PkgIdx ]->GetName());
				}

				bool bProceed = EAppReturnType::Yes == FMessageDialog::Open( EAppMsgType::YesNo,
					FText::Format( NSLOCTEXT("UnrealEd", "Warning_ExternalPackageRef", "The following assets have references to external assets: \n{0}\nExternal assets won't be found when in a game and all references will be broken.  Proceed?"),
						FText::FromString(PackageNamesWithExternalRefs) ) );

				if(!bProceed)
				{
					return false;
				}
			}

			const bool bCheckDirty = false;
			const bool bPromptUserToSave = false;
			FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, bCheckDirty, bPromptUserToSave);

			// Refresh source control state
			ISourceControlModule::Get().QueueStatusUpdate(PackagesToSave);
		}
	}
			
	return true;
}

bool SMeshPaint::HasMultipleMaterialSlots() const
{
	return MeshPaintEditMode->GetEditingActorsNumberOfMaterials() > 1;
}

TSharedRef<SWidget> SMeshPaint::GetMaterialSelectionMenu()
{
	FMenuBuilder MenuBuilder(true, NULL);
	for (int32 MaterialIndex=0; MaterialIndex < MeshPaintEditMode->GetEditingActorsNumberOfMaterials(); ++MaterialIndex)
	{
		MenuBuilder.AddMenuEntry( FText::AsNumber( MaterialIndex ), FText(), FSlateIcon(), FExecuteAction::CreateSP(this, &SMeshPaint::OnSetEditingMaterial, MaterialIndex) );
	}
	return MenuBuilder.MakeWidget();
}

FText SMeshPaint::GetEditingMaterial() const
{
	int32 SelectedMaterialIndex = MeshPaintEditMode->GetEditingMaterialIndex();
	return FText::AsNumber(SelectedMaterialIndex);
}

void SMeshPaint::OnSetEditingMaterial( int32 NewMaterialIndex )
{
	MeshPaintEditMode->SetEditingMaterialIndex(NewMaterialIndex);
}

TSharedRef<SWidget> SMeshPaint::GetActorSelectionMenu()
{
	FMenuBuilder MenuBuilder(true, NULL);
	TArray<TWeakObjectPtr<AActor>> SelectedActors = MeshPaintEditMode->GetEditingActors();
	for (int32 MeshNameIndex=0; MeshNameIndex < SelectedActors.Num(); ++MeshNameIndex)
	{
		if (SelectedActors[MeshNameIndex].IsValid())
		{
			MenuBuilder.AddMenuEntry( FText::FromString(SelectedActors[MeshNameIndex].Get()->GetActorLabel()), FText(), FSlateIcon(), FExecuteAction::CreateSP(this, &SMeshPaint::OnSetEditingActor, SelectedActors[MeshNameIndex]) );
		}
	}
	return MenuBuilder.MakeWidget();
}

FText SMeshPaint::GetEditingActorLabel() const
{
	TWeakObjectPtr<AActor> SelectedActor = MeshPaintEditMode->GetEditingActor();
	if (SelectedActor.IsValid())
	{
		return FText::FromString(SelectedActor.Get()->GetActorLabel());
	}
	return FText::GetEmpty();
}

void SMeshPaint::OnSetEditingActor( TWeakObjectPtr<AActor> InActor )
{
	MeshPaintEditMode->SetEditingMesh(InActor);
}

bool SMeshPaint::HasMultipleActorsSelected() const
{
	return MeshPaintEditMode->GetEditingActors().Num() > 1;
}


#undef LOCTEXT_NAMESPACE
