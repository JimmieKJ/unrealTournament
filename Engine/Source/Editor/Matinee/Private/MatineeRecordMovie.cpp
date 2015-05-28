// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MatineeModule.h"
#include "Matinee/MatineeActor.h"
#include "Matinee.h"
#include "STextComboBox.h"
#include "SNumericEntryBox.h"


#define LOCTEXT_NAMESPACE "MatineeMovieCapture"

/**
 * A set of parameters specifying how movie capture is configured.
 */
class FCreateMovieOptions
{
public:
	FCreateMovieOptions()
	:	CloseEditor(false),
	    CaptureResolutionIndex(0),
		CaptureResolutionFPS(30),
		CaptureTypeIndex(EMatineeCaptureType::AVI),
		Compress(false),
		CinematicMode(true),
		DisableMovement(true),
		DisableTurning(true),
		HidePlayer(true),
		HideHUD(true),
		DisableTextureStreaming(false)
	{}

	/** Custom resolution idthxheight */
	FString CustomRes;

	/** Whether to close the editor or not			*/
	bool					CloseEditor;
	/** The capture resolution index to use			*/
	int32					CaptureResolutionIndex;
	/** The capture FPS								*/
	int32					CaptureResolutionFPS;
	/** The capture type							*/
	int32					CaptureTypeIndex;
	/** Whether to compress or not					*/
	bool					Compress;	
	/** Whether to turn on cinematic mode			*/
	bool					CinematicMode;
	/** Whether to disable movement					*/
	bool					DisableMovement;
	/** Whether to disable turning					*/
	bool					DisableTurning;
	/** Whether to hide the player					*/
	bool					HidePlayer;
	/** Whether to hide the HUD						*/
	bool					HideHUD;
	/** Whether to disable texture streaming		*/
	bool					DisableTextureStreaming;
};

/**
 * Dialog window for matinee movie capture. Values are read from the ini
 * and stored again if the user presses OK.
 */
class SMatineeRecordMovie : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMatineeRecordMovie)
	{}
	SLATE_END_ARGS()	

	/** SCompoundWidget interface */
	void Construct(const FArguments& InArgs, TWeakPtr<SWindow> InParentWindow);


	/** Checkbox for close editor setting */
	bool bCloseEditor;
	
	/** Checkbox for compression */
	bool bCompression;
	
	TSharedPtr<FString> CaptureResolutionSetting;
	/** Width and height of the capture resolution */
	uint32 CaptureWidth;
	uint32 CaptureHeight;

	/** Text for FPS */
	FText FPSEntry;

	/** Checkboxes for cinematic mode */
	bool bCinematicMode;
	bool bDisableMovement;
	bool bDisableTurning;
	bool bHidePlayer;
	bool bHideHud;
	bool bUsingCustomResolution;
	bool bDisableTextureStreaming;

	/** Parent Window */
	TWeakPtr< SWindow > ParentWindowPtr;

	/* CreateMovieOptions used to get options saved in the config files */
	FCreateMovieOptions Options;

	/** Array of strings to display for the screenshot capture type */
	TArray< TSharedPtr<FString> > CaptureFormats;

	/** Current capture type */
	EMatineeCaptureType::Type CaptureType;

	TArray< TSharedPtr<FString> > CaptureResolutionList;

	void OnCaptureResolutionSettingChanged( TSharedPtr<FString> ChosenString, ESelectInfo::Type );

	void OnCaptureTypeSettingChanged(  TSharedPtr<FString> ChosenString, ESelectInfo::Type );

	EVisibility OnGetCustomResolutionVisibility() const;

	TOptional<int32> OnGetCaptureWidth() const { return TOptional<int32>( CaptureWidth ); }
	TOptional<int32> OnGetCaptureHeight() const { return  TOptional<int32>( CaptureHeight ); }

	/** Checkbox handling */
	void OnFPSTextComitted( const FText& Text, ETextCommit::Type) { FPSEntry = Text; }
	FText GetFPSText() const { return FPSEntry; }

	void OnCloseEditorChecked( ECheckBoxState InCheckboxState ) { bCloseEditor = (InCheckboxState == ECheckBoxState::Checked); }
	ECheckBoxState IsCloseEditorChecked() const { return bCloseEditor ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }

	void OnCinematicModeChecked( ECheckBoxState InCheckboxState ) { bCinematicMode = (InCheckboxState == ECheckBoxState::Checked); }
	ECheckBoxState IsCinematicModeChecked() const { return bCinematicMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
	
	bool IsCinematicModeOptionsEnabled() const { return bCinematicMode; }

	void OnDisableMovementChecked( ECheckBoxState InCheckboxState ) { bDisableMovement = (InCheckboxState == ECheckBoxState::Checked); }
	ECheckBoxState IsDisableMovementChecked() const { return bDisableMovement ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }

	void OnDisableTurningChecked( ECheckBoxState InCheckboxState ) { bDisableTurning = (InCheckboxState == ECheckBoxState::Checked); }
	ECheckBoxState IsDisableTurningChecked() const { return bDisableTurning ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }

	void OnHidePlayerChecked( ECheckBoxState InCheckboxState ) { bHidePlayer = (InCheckboxState == ECheckBoxState::Checked); }
	ECheckBoxState IsHidePlayerChecked() const { return bHidePlayer ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }

	void OnHideHudChecked( ECheckBoxState InCheckboxState ) { bHideHud = (InCheckboxState == ECheckBoxState::Checked); }
	ECheckBoxState IsHideHudChecked() const { return bHideHud ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }

	void OnDisableTextureStreamingChecked( ECheckBoxState InCheckboxState ) { bDisableTextureStreaming = (InCheckboxState == ECheckBoxState::Checked); }
	ECheckBoxState IsDisableTextureStreamingChecked() const { return bDisableTextureStreaming ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }

	void OnCustomWidthChanged( int32 Width, ETextCommit::Type Type );
	void OnCustomHeightChanged( int32 Height, ETextCommit::Type Type );

	/** Window Basics */
	void InitializeOptions();
	FReply OnOK();
	FReply OnCancel();

private:
	static FString CustomResolutionStr;
};


FString SMatineeRecordMovie::CustomResolutionStr( TEXT("Custom") );

static void ParseResolutionString( const FString& ResStr, uint32 &OutWidth, uint32& OutHeight )
{
	TArray<FString> Res;
	if( ResStr.ParseIntoArray( Res, TEXT("x"), 0 ) == 2 )
	{
		OutWidth = FCString::Atof(Res[0].GetCharArray().GetData());
		OutHeight = FCString::Atof(Res[1].GetCharArray().GetData());
	}
	else
	{
		OutWidth = 1;
		OutHeight = 1;
	}
}

/**
 * The code will store the newly set options back to the ini if the user presses OK.
 */
FReply SMatineeRecordMovie::OnOK() 
{
	bool bProceedWithBuild = true;

	Options.CloseEditor = bCloseEditor;
	Options.CaptureResolutionFPS = FCString::Atof(*FPSEntry.ToString());
	Options.CaptureResolutionIndex = CaptureResolutionList.Find(CaptureResolutionSetting);

	Options.CaptureTypeIndex = (int32)CaptureType;
	Options.CinematicMode = bCinematicMode;
	Options.DisableMovement = bDisableMovement;
	Options.DisableTurning = bDisableTurning;
	Options.HidePlayer = bHidePlayer;
	Options.HideHUD = bHideHud;
	Options.CustomRes = FString::Printf( TEXT("%dx%d"), CaptureWidth, CaptureHeight );
	Options.DisableTextureStreaming = bDisableTextureStreaming;

	GConfig->SetBool( TEXT("MatineeCreateMovieOptions"), TEXT("CloseEditor"), Options.CloseEditor, GEditorPerProjectIni );
	GConfig->SetBool( TEXT("MatineeCreateMovieOptions"), TEXT("Compress"), Options.Compress, GEditorPerProjectIni );
	GConfig->SetInt(  TEXT("MatineeCreateMovieOptions"), TEXT("CaptureResolutionFPS"), Options.CaptureResolutionFPS, GEditorPerProjectIni );
	GConfig->SetInt(  TEXT("MatineeCreateMovieOptions"), TEXT("CaptureResolutionIndex"), Options.CaptureResolutionIndex, GEditorPerProjectIni );
	GConfig->SetInt(  TEXT("MatineeCreateMovieOptions"), TEXT("CaptureTypeIndex"), Options.CaptureTypeIndex, GEditorPerProjectIni );
	GConfig->SetBool( TEXT("MatineeCreateMovieOptions"), TEXT("CinematicMode"), Options.CinematicMode, GEditorPerProjectIni );
	GConfig->SetBool( TEXT("MatineeCreateMovieOptions"), TEXT("DisableMovement"), Options.DisableMovement, GEditorPerProjectIni );
	GConfig->SetBool( TEXT("MatineeCreateMovieOptions"), TEXT("DisableTurning"), Options.DisableTurning, GEditorPerProjectIni );
	GConfig->SetBool( TEXT("MatineeCreateMovieOptions"), TEXT("HidePlayer"), Options.HidePlayer, GEditorPerProjectIni );
	GConfig->SetBool( TEXT("MatineeCreateMovieOptions"), TEXT("HideHUD"), Options.HideHUD, GEditorPerProjectIni );
	GConfig->SetBool( TEXT("MatineeCreateMovieOptions"), TEXT("DisableTextureStreaming"), Options.DisableTextureStreaming, GEditorPerProjectIni );
	GConfig->SetString( TEXT("MatineeCreateMovieOptions"), TEXT("CustomRes"), *Options.CustomRes, GEditorPerProjectIni );

	GConfig->Flush( false, GEditorPerProjectIni );

	FEdModeInterpEdit* Mode = (FEdModeInterpEdit*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_InterpEdit );
	if ( Mode != NULL && Mode->InterpEd != NULL )
	{
		// Store the options for the capture of the Matinee
		GEngine->MatineeScreenshotOptions.MatineeCaptureName = Mode->InterpEd->GetMatineeActor()->GetName();
		GEngine->MatineeScreenshotOptions.MatineePackageCaptureName = FPackageName::GetShortName(Mode->InterpEd->GetMatineeActor()->GetOutermost()->GetName());

		GEngine->MatineeScreenshotOptions.bNoTextureStreaming = Options.DisableTextureStreaming;
		GEngine->MatineeScreenshotOptions.MatineeCaptureFPS = Options.CaptureResolutionFPS;
		GEngine->MatineeScreenshotOptions.bCompressMatineeCapture = Options.Compress;

		GUnrealEd->MatineeCaptureResolutionX = CaptureWidth;
		GUnrealEd->MatineeCaptureResolutionY = CaptureHeight;

		GEngine->MatineeScreenshotOptions.MatineeCaptureType = (EMatineeCaptureType::Type)Options.CaptureTypeIndex;
		Mode->InterpEd->StartRecordingMovie();
		
		//if Options.CloseEditor == true, Editor will request a close action in UEditorEngine::PlayForMovieCapture
	}		

	ParentWindowPtr.Pin()->RequestDestroyWindow();
	return FReply::Handled();
}

FReply SMatineeRecordMovie::OnCancel()
{
	ParentWindowPtr.Pin()->RequestDestroyWindow();
	return FReply::Handled();
}

/**
 * Populating default Options by reading from ini.
 */
void SMatineeRecordMovie::InitializeOptions()
{
	// Retrieve settings from ini.
	GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("CloseEditor"), Options.CloseEditor, GEditorPerProjectIni );
	GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("Compress"), Options.Compress, GEditorPerProjectIni );
	GConfig->GetInt(  TEXT("MatineeCreateMovieOptions"), TEXT("CaptureResolutionFPS"), Options.CaptureResolutionFPS, GEditorPerProjectIni );
	GConfig->GetInt(  TEXT("MatineeCreateMovieOptions"), TEXT("CaptureResolutionIndex"), Options.CaptureResolutionIndex, GEditorPerProjectIni );
	GConfig->GetInt(  TEXT("MatineeCreateMovieOptions"), TEXT("CaptureTypeIndex"), Options.CaptureTypeIndex, GEditorPerProjectIni );
	GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("CinematicMode"), Options.CinematicMode, GEditorPerProjectIni );
	GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("DisableMovement"), Options.DisableMovement, GEditorPerProjectIni );
	GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("DisableTurning"), Options.DisableTurning, GEditorPerProjectIni );
	GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("HidePlayer"), Options.HidePlayer, GEditorPerProjectIni );
	GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("HideHUD"), Options.HideHUD, GEditorPerProjectIni );
	GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("DisableTextureStreaming"), Options.DisableTextureStreaming, GEditorPerProjectIni );

	GConfig->GetString( TEXT("MatineeCreateMovieOptions"), TEXT("CustomRes"), Options.CustomRes, GEditorPerProjectIni );

	// setup format strings
	CaptureFormats.Empty();
	UEnum* FormatEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EMatineeCaptureType"), true);
	for(int32 EnumMember = 0; EnumMember < FormatEnum->NumEnums() - 1; EnumMember++)
	{
		CaptureFormats.Add(MakeShareable(new FString(FormatEnum->GetEnumText(EnumMember).ToString())));
	}	

	Options.CaptureTypeIndex = FMath::Clamp<int32>(Options.CaptureTypeIndex, 0, FormatEnum->NumEnums() - 1);

	// set up resolution strings
	CaptureResolutionList.Add( MakeShareable( new FString(TEXT("320 x 240"))));
	CaptureResolutionList.Add( MakeShareable( new FString(TEXT("640 x 480"))));
	CaptureResolutionList.Add( MakeShareable( new FString(TEXT("1280 x 720"))));
	CaptureResolutionList.Add( MakeShareable( new FString(TEXT("1920 x 1080"))));
	CaptureResolutionList.Add( MakeShareable( new FString( CustomResolutionStr )));

	int32 MaxEntries = CaptureResolutionList.Num();
	Options.CaptureResolutionIndex = FMath::Clamp<int32>(Options.CaptureResolutionIndex, 0, MaxEntries - 1);

	OnCaptureResolutionSettingChanged( CaptureResolutionList[Options.CaptureResolutionIndex], ESelectInfo::Direct );

	if( *CaptureResolutionList[Options.CaptureResolutionIndex] == CustomResolutionStr )
	{
		ParseResolutionString( Options.CustomRes, CaptureWidth, CaptureHeight );
	}


	bCloseEditor = Options.CloseEditor;
	FString FPSString = FString::Printf(TEXT("%d"), Options.CaptureResolutionFPS);
	FPSEntry = FText::FromString(FPSString);

	CaptureType = (EMatineeCaptureType::Type)Options.CaptureTypeIndex;

	bCinematicMode = Options.CinematicMode;
	bDisableMovement = Options.DisableMovement;
	bDisableTurning = Options.DisableTurning;
	bHidePlayer = Options.HidePlayer;
	bHideHud = Options.HideHUD;
	bDisableTextureStreaming = Options.DisableTextureStreaming;

}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMatineeRecordMovie::Construct(const FArguments& InArgs, TWeakPtr<SWindow> InParentWindow)
{
	ParentWindowPtr = InParentWindow;
	bUsingCustomResolution = false;
	FString MatineeName = TEXT("SeqAct_Interp");
	FEdModeInterpEdit* Mode = (FEdModeInterpEdit*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_InterpEdit );
	if ( Mode != NULL && Mode->InterpEd != NULL )
	{
		MatineeName = Mode->InterpEd->GetMatineeActor()->GetName();
	}	

	FFormatNamedArguments Args;
	Args.Add( TEXT("MatineeName"), FText::FromString(MatineeName) );
	const FText StaticBoxTitle = FText::Format( NSLOCTEXT("UnrealEd", "MatineeCaptureOptions", "{MatineeName} Capture Options"), Args );

	InitializeOptions();

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(STextBlock)
			.Text(StaticBoxTitle)
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5,5,5,0)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("UnrealEd", "CO_CaptureType", "Capture Type"))
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(STextComboBox)
			.OptionsSource(&CaptureFormats)
			.InitiallySelectedItem(CaptureFormats[CaptureType])
			.OnSelectionChanged(this, &SMatineeRecordMovie::OnCaptureTypeSettingChanged)
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("UnrealEd", "CaptureResolution", "Capture Resolution:"))
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(STextComboBox)
			.OptionsSource(&CaptureResolutionList)
			.InitiallySelectedItem(CaptureResolutionSetting)
			.OnSelectionChanged(this, &SMatineeRecordMovie::OnCaptureResolutionSettingChanged)
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew( SHorizontalBox )
			.Visibility( this, &SMatineeRecordMovie::OnGetCustomResolutionVisibility )
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew( STextBlock )
				.Text( NSLOCTEXT("UnrealEd", "CustomResolution", "Resolution") )
			]
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SNumericEntryBox<int32>)
				.Value(this, &SMatineeRecordMovie::OnGetCaptureWidth)
				.OnValueCommitted(this, &SMatineeRecordMovie::OnCustomWidthChanged)
				.AllowSpin(false)
				.LabelPadding(0)
				.MinValue(1)
				.MaxValue(16384)
				.Label()
				[
					SNumericEntryBox<int32>::BuildLabel( LOCTEXT("CaptureWidthLabel", "W"), FLinearColor::Black, FLinearColor(.33f,.33f,.33f) )
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SNumericEntryBox<int32>)
				.Value(this, &SMatineeRecordMovie::OnGetCaptureHeight)
				.OnValueCommitted(this, &SMatineeRecordMovie::OnCustomHeightChanged)
				.AllowSpin(false)
				.LabelPadding(0)
				.MinValue(1)
				.MaxValue(16384)
				.Label()
				[
					SNumericEntryBox<int32>::BuildLabel( LOCTEXT("CaptureHeightLabel", "H"), FLinearColor::Black, FLinearColor(.33f,.33f,.33f) )
				]
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("UnrealEd", "CO_FPS", "FPS:"))
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(SEditableTextBox)
			.OnTextCommitted(this, &SMatineeRecordMovie::OnFPSTextComitted)
			.Text(this, &SMatineeRecordMovie::GetFPSText)
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &SMatineeRecordMovie::OnCloseEditorChecked)
			.IsChecked(this, &SMatineeRecordMovie::IsCloseEditorChecked)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("UnrealEd", "CO_CloseEditor", "Close the editor when the capture starts"))
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5,5,5,0)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("UnrealEd", "CO_CinematicModeTitle", "Cinematic Mode"))
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this,&SMatineeRecordMovie::OnCinematicModeChecked)
			.IsChecked(this, &SMatineeRecordMovie::IsCinematicModeChecked)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("UnrealEd", "CO_CinematicMode", "Turn on Cinematic Mode"))
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(SBorder)
			.Content()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(this,&SMatineeRecordMovie::OnDisableMovementChecked)
					.IsChecked(this, &SMatineeRecordMovie::IsDisableMovementChecked)
					.IsEnabled(this, &SMatineeRecordMovie::IsCinematicModeOptionsEnabled)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("UnrealEd", "CO_CinematicModeDisableMovement", "Disable Movement"))
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(this,&SMatineeRecordMovie::OnDisableTurningChecked)
					.IsChecked(this, &SMatineeRecordMovie::IsDisableTurningChecked)
					.IsEnabled(this, &SMatineeRecordMovie::IsCinematicModeOptionsEnabled)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("UnrealEd", "CO_CinematicModeDisableTurning", "Disable Turning"))
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(this,&SMatineeRecordMovie::OnHidePlayerChecked)
					.IsChecked(this, &SMatineeRecordMovie::IsHidePlayerChecked)
					.IsEnabled(this, &SMatineeRecordMovie::IsCinematicModeOptionsEnabled)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("UnrealEd", "CO_CinematicModeHidePlayer", "Hide Player"))
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(this,&SMatineeRecordMovie::OnHideHudChecked)
					.IsChecked(this, &SMatineeRecordMovie::IsHideHudChecked)
					.IsEnabled(this, &SMatineeRecordMovie::IsCinematicModeOptionsEnabled)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("UnrealEd", "CO_CinematicModeHideHud", "Hide HUD"))
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(this,&SMatineeRecordMovie::OnDisableTextureStreamingChecked)
					.IsChecked(this, &SMatineeRecordMovie::IsDisableTextureStreamingChecked)
					.IsEnabled(this, &SMatineeRecordMovie::IsCinematicModeOptionsEnabled)
					[
						SNew(STextBlock)
						.Text( LOCTEXT("DisableTextureStreaming", "Disable Texture Streaming") )
					]
				]
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(0.5f)
			.Padding(0,0,5,0)
			[
				SNew(SButton)
				.Text(NSLOCTEXT("UnrealEd", "OK", "OK"))
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.OnClicked(this, &SMatineeRecordMovie::OnOK)
			]
			+SHorizontalBox::Slot()
			.Padding(5,0,0,0)
			.FillWidth(0.5f)
			[
				SNew(SButton)
				.Text(NSLOCTEXT("UnrealEd", "Cancel", "Cancel"))
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.OnClicked(this, &SMatineeRecordMovie::OnCancel)
			]
		]
		
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


void FMatinee::OnMenuCreateMovie()
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(NSLOCTEXT("UnrealEd", "CreateMovie", "Create Movie Options..."))
		.SizingRule( ESizingRule::Autosized )
		.SupportsMinimize(false)
		.SupportsMaximize(false);

	Window->SetContent(
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush(TEXT("PropertyWindow.WindowBorder")) )
		[
			SNew(SMatineeRecordMovie, Window)
		]);
		
	FSlateApplication::Get().AddWindow(Window);
}

void SMatineeRecordMovie::OnCaptureResolutionSettingChanged( TSharedPtr<FString> ChosenString, ESelectInfo::Type )
{
	CaptureResolutionSetting = ChosenString;

	if( *ChosenString == CustomResolutionStr )
	{
		bUsingCustomResolution = true;
	}
	else
	{	
		bUsingCustomResolution = false;
		ParseResolutionString( *ChosenString, CaptureWidth, CaptureHeight );
	}
}

void SMatineeRecordMovie::OnCaptureTypeSettingChanged(  TSharedPtr<FString> ChosenString, ESelectInfo::Type )
{
	UEnum* FormatEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EMatineeCaptureType"), true);
	for(int32 EnumMember = 0; EnumMember < FormatEnum->NumEnums() - 1; EnumMember++)
	{
		if(*ChosenString.Get() == FormatEnum->GetEnumText(EnumMember).ToString())
		{
			CaptureType = (EMatineeCaptureType::Type)EnumMember;
			break;
		}
	}
}

void SMatineeRecordMovie::OnCustomWidthChanged( int32 Width, ETextCommit::Type Type )
{
	CaptureWidth = FMath::Max(1, Width);
}

void SMatineeRecordMovie::OnCustomHeightChanged( int32 Height, ETextCommit::Type Type )
{
	CaptureHeight = FMath::Max(1, Height);
}


EVisibility SMatineeRecordMovie::OnGetCustomResolutionVisibility() const
{
	return bUsingCustomResolution ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
