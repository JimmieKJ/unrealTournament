// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPCH.h"
#include "MediaPlayerFactoryNew.h"
#include "MediaSoundWaveFactoryNew.h"
#include "MediaTextureFactoryNew.h"


#define LOCTEXT_NAMESPACE "UMediaPlayerFactoryNew"


/* Local helpers
 *****************************************************************************/

class SMediaPlayerFactoryDialog
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMediaPlayerFactoryDialog) { }
	SLATE_END_ARGS()

	/** Construct this widget. */
	void Construct(const FArguments& InArgs, FMediaPlayerFactoryNewOptions& InOptions, TSharedRef<SWindow> InWindow)
	{
		Options = &InOptions;
		Window = InWindow;

		ChildSlot
		[
			SNew(SBorder)
				.Visibility(EVisibility::Visible)
				.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
						.FillHeight(1)
						.VAlign(VAlign_Top)
						[
							SNew(SBorder)
								.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
								.Padding(4.0f)
								.Content()
								[
									SNew(SVerticalBox)

									+ SVerticalBox::Slot()
										[
											SNew(STextBlock)
												.Text(LOCTEXT("CreateAdditionalAssetsLabel", "Additional assets to create and link to the Media Player:"))
										]

									+ SVerticalBox::Slot()
										.Padding(0.0f, 8.0f, 0.0f, 0.0f)
										[
											SNew(SCheckBox)
												.IsChecked(Options->CreateSoundWave ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
												.OnCheckStateChanged_Lambda([this](ECheckBoxState CheckBoxState) {
													Options->CreateSoundWave = (CheckBoxState == ECheckBoxState::Checked);
												})
												.Content()
												[
													SNew(STextBlock)
														.Text(LOCTEXT("CreateSoundWaveLabel", "Audio output SoundWave asset"))
												]
										]

									+ SVerticalBox::Slot()
										.Padding(0.0f, 6.0f, 0.0f, 0.0f)
										[
											SNew(SCheckBox)
												.IsChecked(Options->CreateImageTexture ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
												.OnCheckStateChanged_Lambda([this](ECheckBoxState CheckBoxState) {
													Options->CreateImageTexture = (CheckBoxState == ECheckBoxState::Checked);
												})
												.Content()
												[
													SNew(STextBlock)
														.Text(LOCTEXT("CreateImageTextureLabel", "Still image output MediaTexture asset"))
												]
										]

									+ SVerticalBox::Slot()
										.Padding(0.0f, 6.0f, 0.0f, 0.0f)
										[
											SNew(SCheckBox)
												.IsChecked(Options->CreateVideoTexture ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
												.OnCheckStateChanged_Lambda([this](ECheckBoxState CheckBoxState) {
													Options->CreateVideoTexture = (CheckBoxState == ECheckBoxState::Checked);
												})
												.Content()
												[
													SNew(STextBlock)
														.Text(LOCTEXT("CreateVideoTextureLabel", "Video output MediaTexture asset"))
												]
										]
								]
						]

					+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Bottom)
						.Padding(8)
						[
							SNew(SUniformGridPanel)
								.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
								.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
								.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				
							+ SUniformGridPanel::Slot(0, 0)
								[
									SNew(SButton)
										.HAlign(HAlign_Center)
										.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
										.OnClicked_Lambda([this]() -> FReply { CloseDialog(true); return FReply::Handled(); })
										.Text(LOCTEXT("OkButtonLabel", "OK"))
								]
					
							+ SUniformGridPanel::Slot(1, 0)
								[
									SNew(SButton)
										.HAlign(HAlign_Center)
										.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
										.OnClicked_Lambda([this]() -> FReply { CloseDialog(false); return FReply::Handled(); })
										.Text(LOCTEXT("CancelButtonLabel", "Cancel"))
								]
						]
				]
		];
	}

protected:

	void CloseDialog(bool InOkClicked)
	{
		Options->OkClicked = InOkClicked;

		if (Window.IsValid())
		{
			Window.Pin()->RequestDestroyWindow();
		}
	}

private:

	FMediaPlayerFactoryNewOptions* Options;
	TWeakPtr<SWindow> Window;
};


/* UMediaPlayerFactoryNew structors
 *****************************************************************************/

UMediaPlayerFactoryNew::UMediaPlayerFactoryNew( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	SupportedClass = UMediaPlayer::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


/* UFactory interface
 *****************************************************************************/

bool UMediaPlayerFactoryNew::ConfigureProperties()
{
	Options.CreateImageTexture = false;
	Options.CreateSoundWave = false;
	Options.CreateVideoTexture = false;
	Options.OkClicked = false;

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("CreateMediaPlayerDialogTitle", "Create Media Player"))
		.ClientSize(FVector2D(400, 160))
		.SupportsMinimize(false)
		.SupportsMaximize(false);

	Window->SetContent(SNew(SMediaPlayerFactoryDialog, Options, Window));
	GEditor->EditorAddModalWindow(Window);

	return Options.OkClicked;
}


UObject* UMediaPlayerFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	auto NewMediaPlayer = NewObject<UMediaPlayer>(InParent, InClass, InName, Flags);

	if ((NewMediaPlayer != nullptr) && (Options.CreateImageTexture || Options.CreateSoundWave || Options.CreateVideoTexture))
	{
		IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		const FString ParentName = InParent->GetOutermost()->GetName();

		FString OutAssetName;
		FString OutPackageName;

		if (Options.CreateImageTexture)
		{
			AssetTools.CreateUniqueAssetName(ParentName, TEXT("_Image"), OutPackageName, OutAssetName);
			const FString PackagePath = FPackageName::GetLongPackagePath(OutPackageName);
			auto Factory = NewObject<UMediaTextureFactoryNew>();
			auto ImageTexture = Cast<UMediaTexture>(AssetTools.CreateAsset(OutAssetName, PackagePath, UMediaTexture::StaticClass(), Factory));

			if (ImageTexture != nullptr)
			{
				NewMediaPlayer->SetImageTexture(ImageTexture);
			}
		}

		if (Options.CreateSoundWave)
		{
			AssetTools.CreateUniqueAssetName(ParentName, TEXT("_Sound"), OutPackageName, OutAssetName);
			const FString PackagePath = FPackageName::GetLongPackagePath(OutPackageName);
			auto Factory = NewObject<UMediaSoundWaveFactoryNew>();
			auto SoundWave = Cast<UMediaSoundWave>(AssetTools.CreateAsset(OutAssetName, PackagePath, UMediaSoundWave::StaticClass(), Factory));

			if (SoundWave != nullptr)
			{
				NewMediaPlayer->SetSoundWave(SoundWave);
			}
		}

		if (Options.CreateVideoTexture)
		{
			AssetTools.CreateUniqueAssetName(ParentName, TEXT("_Video"), OutPackageName, OutAssetName);
			const FString PackagePath = FPackageName::GetLongPackagePath(OutPackageName);
			auto Factory = NewObject<UMediaTextureFactoryNew>();
			auto VideoTexture = Cast<UMediaTexture>(AssetTools.CreateAsset(OutAssetName, PackagePath, UMediaTexture::StaticClass(), Factory));

			if (VideoTexture != nullptr)
			{
				NewMediaPlayer->SetVideoTexture(VideoTexture);
			}
		}
	}

	return NewMediaPlayer;
}


uint32 UMediaPlayerFactoryNew::GetMenuCategories() const
{
	return EAssetTypeCategories::Media;
}


bool UMediaPlayerFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}


#undef LOCTEXT_NAMESPACE
