// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "SUWPlayerSettingsDialog.h"
#include "SUWindowsStyle.h"
#include "SUWWeaponConfigDialog.h"
#include "SNumericEntryBox.h"
#include "../Public/UTCanvasRenderTarget2D.h"
#include "EngineModule.h"
#include "SlateMaterialBrush.h"
#include "../Public/UTPlayerCameraManager.h"
#include "UTCharacterContent.h"

#if !UE_SERVER
#include "Runtime/AppFramework/Public/Widgets/Colors/SColorPicker.h"
#endif

#include "AssetData.h"

// scale factor for weapon/view bob sliders (i.e. configurable value between 0 and this)
static const float BOB_SCALING_FACTOR = 2.0f;

#if !UE_SERVER

#include "SScaleBox.h"

#define LOCTEXT_NAMESPACE "SUWPlayerSettingsDialog"

DECLARE_DELEGATE_OneParam(FDragHandler, FVector2D);

class SDragImage : public SImage
{
public:
	SLATE_BEGIN_ARGS(SDragImage)
		: _Image(FCoreStyle::Get().GetDefaultBrush()), _ColorAndOpacity(FLinearColor::White), _OnDrag()
	{}

		/** Image resource */
		SLATE_ATTRIBUTE(const FSlateBrush*, Image)

		/** Color and opacity */
		SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity)

		/** Invoked when the mouse is dragged in the widget */
		SLATE_EVENT(FDragHandler, OnDrag)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		OnDrag = InArgs._OnDrag;
		SImage::Construct(SImage::FArguments().Image(InArgs._Image).ColorAndOpacity(InArgs._ColorAndOpacity));
	}

	virtual bool SupportsKeyboardFocus() const
	{
		return true;
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse).CaptureMouse(AsShared());
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (HasMouseCapture())
		{
			return FReply::Handled().ReleaseMouseCapture();
		}
		else
		{
			return FReply::Handled();
		}
	}

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (HasMouseCapture())
		{
			OnDrag.ExecuteIfBound(MouseEvent.GetCursorDelta());
		}
		return FReply::Handled();
	}

protected:
	FDragHandler OnDrag;
};

void SUWPlayerSettingsDialog::Construct(const FArguments& InArgs)
{
	SUWDialog::Construct(SUWDialog::FArguments()
							.PlayerOwner(InArgs._PlayerOwner)
							.DialogTitle(NSLOCTEXT("SUWindowsDesktop", "PlayerSettings", "Player Settings"))
							.DialogSize(InArgs._DialogSize)
							.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
							.DialogPosition(InArgs._DialogPosition)
							.DialogAnchorPoint(InArgs._DialogAnchorPoint)
							.ContentPadding(InArgs._ContentPadding)
							.IsScrollable(false)
							.ButtonMask(UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_CANCEL)
							.OnDialogResult(InArgs._OnDialogResult)
						);

	PlayerPreviewMesh = NULL;

	WeaponConfigDelayFrames = 0;

	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Unreal"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("United States"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("United Kingdom"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("France"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Germany"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Italy"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Russia"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Australia"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Poland"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Brazil"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Portugal"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Spain"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Sweden"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Ukraine"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Austria"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Finland"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Hungary"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Netherlands"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Belgium"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Switzerland"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("South Africa"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Canada"))));

	// Flags by _Lynx
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Armenia"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Azerbaijan"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Chile"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Estonia"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("EU"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Iceland"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("India"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Israel"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Jamaica"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Japan"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Latvija"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Lithuania"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Luxembourg"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Malaysia"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Peru"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("Scotland"))));
	CountyFlagNames.Add(MakeShareable(new FString(TEXT("South Korea"))));


	TSharedPtr< SComboBox< TSharedPtr<FString> > > CountryFlagComboBox;

	FVector2D PreviewViewportSize(1024, 1024);

	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(NULL, TEXT("/Game/RestrictedAssets/UI/PlayerPreviewProxy.PlayerPreviewProxy"));
	if (BaseMat != NULL)
	{
		PlayerPreviewTexture = Cast<UUTCanvasRenderTarget2D>(UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(GetPlayerOwner().Get(), UUTCanvasRenderTarget2D::StaticClass(), PreviewViewportSize.X, PreviewViewportSize.Y));
		PlayerPreviewTexture->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
		PlayerPreviewTexture->OnNonUObjectRenderTargetUpdate.BindSP(this, &SUWPlayerSettingsDialog::UpdatePlayerRender);
		PlayerPreviewMID = UMaterialInstanceDynamic::Create(BaseMat, GetPlayerOwner()->GetWorld());
		PlayerPreviewMID->SetTextureParameterValue(FName(TEXT("TheTexture")), PlayerPreviewTexture);
		PlayerPreviewBrush = new FSlateMaterialBrush(*PlayerPreviewMID, PreviewViewportSize);
	}
	else
	{
		PlayerPreviewTexture = NULL;
		PlayerPreviewMID = NULL;
		PlayerPreviewBrush = new FSlateMaterialBrush(*UMaterial::GetDefaultMaterial(MD_Surface), PreviewViewportSize);
	}

	// allocate a preview scene for rendering
	PlayerPreviewWorld = UWorld::CreateWorld(EWorldType::Preview, true);
	PlayerPreviewWorld->bHack_Force_UsesGameHiddenFlags_True = true;
	GEngine->CreateNewWorldContext(EWorldType::Preview).SetCurrentWorld(PlayerPreviewWorld);
	PlayerPreviewWorld->InitializeActorsForPlay(FURL(), true);
	ViewState.Allocate();
	{
		UClass* EnvironmentClass = LoadObject<UClass>(nullptr, TEXT("/Game/RestrictedAssets/UI/PlayerPreviewEnvironment.PlayerPreviewEnvironment_C"));
		PreviewEnvironment = PlayerPreviewWorld->SpawnActor<AActor>(EnvironmentClass, FVector(500.f, 50.f, 0.f), FRotator(0, 0, 0));
	}

	FVector2D ViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(ViewportSize);
	FVector2D ResolutionScale(ViewportSize.X / 1280.0f, ViewportSize.Y / 720.0f);

	UUTGameUserSettings* Settings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	
	const int EmoteMax = 6;

	{
		HatList.Add(MakeShareable(new FString(TEXT("No Hat"))));
		HatPathList.Add(TEXT(""));

		TArray<FAssetData> AssetList;
		GetAllBlueprintAssetData(AUTHat::StaticClass(), AssetList);
		for (const FAssetData& Asset : AssetList)
		{
			static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
			const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
			if (ClassPath != NULL)
			{
				UClass* TestClass = LoadObject<UClass>(NULL, **ClassPath);
				if (TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(AUTHat::StaticClass()) && !TestClass->IsChildOf(AUTHatLeader::StaticClass()))
				{
					HatList.Add(MakeShareable(new FString(TestClass->GetDefaultObject<AUTHat>()->CosmeticName)));
					HatPathList.Add(Asset.ObjectPath.ToString() + TEXT("_C"));
				}
			}
		}
	}

	{
		EyewearList.Add(MakeShareable(new FString(TEXT("No Glasses"))));
		EyewearPathList.Add(TEXT(""));

		TArray<FAssetData> AssetList;
		GetAllBlueprintAssetData(AUTEyewear::StaticClass(), AssetList);
		for (const FAssetData& Asset : AssetList)
		{
			static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
			const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
			if (ClassPath != NULL)
			{
				UClass* TestClass = LoadObject<UClass>(NULL, **ClassPath);
				if (TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(AUTEyewear::StaticClass()))
				{
					EyewearList.Add(MakeShareable(new FString(TestClass->GetDefaultObject<AUTEyewear>()->CosmeticName)));
					EyewearPathList.Add(Asset.ObjectPath.ToString() + TEXT("_C"));
				}
			}
		}
	}

	{
		CharacterList.Add(MakeShareable(new FString(TEXT("DemoGuy"))));
		CharacterPathList.Add(TEXT(""));

		TArray<FAssetData> AssetList;
		GetAllBlueprintAssetData(AUTCharacterContent::StaticClass(), AssetList);
		for (const FAssetData& Asset : AssetList)
		{
			static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
			const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
			if (ClassPath != NULL)
			{
				UClass* TestClass = LoadObject<UClass>(NULL, **ClassPath);
				// TODO: long term should probably delayed load this... but would need some way to look up the loc'ed display name without loading the class (currently impossible...)
				if (TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(AUTCharacterContent::StaticClass()))
				{
					CharacterList.Add(MakeShareable(new FString(TestClass->GetDefaultObject<AUTCharacterContent>()->DisplayName.ToString())));
					CharacterPathList.Add(Asset.ObjectPath.ToString() + TEXT("_C"));
				}
			}
		}
	}

	SelectedPlayerColor = GetDefault<AUTPlayerController>()->FFAPlayerColor;

	FMargin NameColumnPadding = FMargin(10, 4);
	FMargin ValueColumnPadding = FMargin(0, 4);
	
	if (DialogContent.IsValid())
	{
		const float MessageTextPaddingX = 10.0f;
		TSharedPtr<STextBlock> MessageTextBlock;
		DialogContent->AddSlot()
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFill)
				[
					SNew(SDragImage)
					.Image(PlayerPreviewBrush)
					.OnDrag(this, &SUWPlayerSettingsDialog::DragPlayerPreview)
				]
			]

			+ SOverlay::Slot()
			[
				SNew(SHorizontalBox)
			
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SSpacer)
					.Size(FVector2D(420, 420))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(20, 0)
				[
					SNew(SVerticalBox)
				
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(NameColumnPadding)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(LOCTEXT("PlayerName", "Name"))
						]

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.Padding(ValueColumnPadding)
						[
							SAssignNew(PlayerName, SEditableTextBox)
							.OnTextChanged(this, &SUWPlayerSettingsDialog::OnNameTextChanged)
							.Text(FText::FromString(GetPlayerOwner()->GetNickname()))
							.Style(SUWindowsStyle::Get(), "UT.Common.Editbox.Dark")
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()

					[
						SNew(SGridPanel)

						// Country Flag
						// ---------------------------------------------------------------------------------
						+ SGridPanel::Slot(0, 0)
						.Padding(NameColumnPadding)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(LOCTEXT("CountryFlag", "Country Flag"))
						]

						+ SGridPanel::Slot(1, 0)
						.Padding(ValueColumnPadding)
						[
							SAssignNew(CountryFlagComboBox, SComboBox< TSharedPtr<FString> >)
							.InitiallySelectedItem(0)
							.ComboBoxStyle(SUWindowsStyle::Get(), "UWindows.Standard.ComboBox")
							.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
							.OptionsSource(&CountyFlagNames)
							.OnGenerateWidget(this, &SUWDialog::GenerateStringListWidget)
							.OnSelectionChanged(this, &SUWPlayerSettingsDialog::OnFlagSelected)
							.Content()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.Padding(10.0f, 0.0f, 10.0f, 0.0f)
								[
									SAssignNew(SelectedFlag, STextBlock)
									.Text(FString(TEXT("Unreal")))
									.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.Dialog.Options.TextStyle")
								]
							]
						]

						// Hat
						// ---------------------------------------------------------------------------------
						+ SGridPanel::Slot(0, 1)
						.Padding(NameColumnPadding)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(LOCTEXT("HatSelectionLabel", "Hat"))
						]

						+ SGridPanel::Slot(1, 1)
						.Padding(ValueColumnPadding)
						[
							SAssignNew(HatComboBox, SComboBox< TSharedPtr<FString> >)
							.InitiallySelectedItem(0)
							.ComboBoxStyle(SUWindowsStyle::Get(), "UWindows.Standard.ComboBox")
							.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
							.OptionsSource(&HatList)
							.OnGenerateWidget(this, &SUWDialog::GenerateStringListWidget)
							.OnSelectionChanged(this, &SUWPlayerSettingsDialog::OnHatSelected)
							.ContentPadding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
							.Content()
							[
								SAssignNew(SelectedHat, STextBlock)
								.Text(FString(TEXT("No Hats Available")))
								.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.Dialog.Options.TextStyle")
							]
						]

						// Eyewear
						// ---------------------------------------------------------------------------------
						+ SGridPanel::Slot(0, 2)
						.Padding(NameColumnPadding)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(LOCTEXT("EyewearSelectionLabel", "Eyewear"))
						]

						+ SGridPanel::Slot(1, 2)
						.Padding(ValueColumnPadding)
						[
							SAssignNew(EyewearComboBox, SComboBox< TSharedPtr<FString> >)
							.InitiallySelectedItem(0)
							.ComboBoxStyle(SUWindowsStyle::Get(), "UWindows.Standard.ComboBox")
							.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
							.OptionsSource(&EyewearList)
							.OnGenerateWidget(this, &SUWDialog::GenerateStringListWidget)
							.OnSelectionChanged(this, &SUWPlayerSettingsDialog::OnEyewearSelected)
							.Content()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.Padding(10.0f, 0.0f, 10.0f, 0.0f)
								[
									SAssignNew(SelectedEyewear, STextBlock)
									.Text(FString(TEXT("No Glasses Available")))
									.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.Dialog.Options.TextStyle")
								]
							]
						]

						// Character
						// ---------------------------------------------------------------------------------
						+ SGridPanel::Slot(0, 3)
						.Padding(NameColumnPadding)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(LOCTEXT("CharSelectionLabel", "Character"))
						]

						+ SGridPanel::Slot(1, 3)
						.Padding(ValueColumnPadding)
						[
							SAssignNew(CharacterComboBox, SComboBox< TSharedPtr<FString> >)
							.InitiallySelectedItem(0)
							.ComboBoxStyle(SUWindowsStyle::Get(), "UWindows.Standard.ComboBox")
							.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
							.OptionsSource(&CharacterList)
							.OnGenerateWidget(this, &SUWDialog::GenerateStringListWidget)
							.OnSelectionChanged(this, &SUWPlayerSettingsDialog::OnCharacterSelected)
							.Content()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.Padding(10.0f, 0.0f, 10.0f, 0.0f)
								[
									SAssignNew(SelectedCharacter, STextBlock)
									.Text(FString(TEXT("No Characters Available")))
									.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.Dialog.Options.TextStyle")
								]
							]
						]
					]
					/* FIXME: temporarily removed
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 4)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(NameColumnPadding)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(LOCTEXT("FFAPlayerColor", "Free for All Player Color"))
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(ValueColumnPadding)
						[
							SNew(SComboButton)
							.MenuPlacement(MenuPlacement_ComboBoxRight)
							.HasDownArrow(false)
							.ContentPadding(0)
							.VAlign(VAlign_Fill)
							.ButtonContent()
							[
								SNew(SColorBlock)
								.Color(this, &SUWPlayerSettingsDialog::GetSelectedPlayerColor)
								.IgnoreAlpha(true)
								.Size(FVector2D(32.0f * ResolutionScale.X, 16.0f * ResolutionScale.Y))
							]
							.MenuContent()
							[
								SNew(SBorder)
								.BorderImage(SUWindowsStyle::Get().GetBrush("UWindows.Standard.Dialog.Background"))
								[
									SNew(SColorPicker)
									.OnColorCommitted(this, &SUWPlayerSettingsDialog::PlayerColorChanged)
									.TargetColorAttribute(SelectedPlayerColor)
								]
							]
						]
					]*/

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SSpacer)
						.Size(FVector2D(30, 30))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 4)
					[
						SNew(SGridPanel)
						.FillColumn(1, 1)

						// Weapon Bob
						// ---------------------------------------------------------------------------------
						+ SGridPanel::Slot(0, 0)
						.Padding(NameColumnPadding)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(LOCTEXT("WeaponBobScaling", "Weapon Bob"))
						]

						+ SGridPanel::Slot(1, 0)
						.Padding(ValueColumnPadding)
						[
							SAssignNew(WeaponBobScaling, SSlider)
							.Orientation(Orient_Horizontal)
							.Value(GetDefault<AUTPlayerController>()->WeaponBobGlobalScaling / BOB_SCALING_FACTOR)
						]

						// View Bob
						// ---------------------------------------------------------------------------------
						+ SGridPanel::Slot(0, 1)
						.Padding(NameColumnPadding)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(LOCTEXT("ViewBobScaling", "View Bob"))
						]

						+ SGridPanel::Slot(1, 1)
						.Padding(ValueColumnPadding)
						[
							SAssignNew(ViewBobScaling, SSlider)
							.Orientation(Orient_Horizontal)
							.Value(GetDefault<AUTPlayerController>()->EyeOffsetGlobalScaling / BOB_SCALING_FACTOR)
						]
					]

					//+ SVerticalBox::Slot()
					//.AutoHeight()
					//[
					//	SNew(SButton)
					//	.HAlign(HAlign_Center)
					//	.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
					//	.ContentPadding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
					//	.Text(LOCTEXT("WeaponConfig", "Weapon Settings"))
					//	.OnClicked(this, &SUWPlayerSettingsDialog::WeaponConfigClick)
					//]
				]
			]
		];

		bool bFoundSelectedHat = false;
		for (int32 i = 0; i < HatPathList.Num(); i++)
		{
			if (HatPathList[i] == GetPlayerOwner()->GetHatPath())
			{
				HatComboBox->SetSelectedItem(HatList[i]);
				bFoundSelectedHat = true;
				break;
			}
		}
		if (!bFoundSelectedHat && HatPathList.Num() > 0)
		{
			HatComboBox->SetSelectedItem(HatList[0]);
		}


		bool bFoundSelectedEyewear = false;
		for (int32 i = 0; i < EyewearPathList.Num(); i++)
		{
			if (EyewearPathList[i] == GetPlayerOwner()->GetEyewearPath())
			{
				EyewearComboBox->SetSelectedItem(EyewearList[i]);
				bFoundSelectedEyewear = true;
				break;
			}
		}
		if (!bFoundSelectedEyewear && EyewearPathList.Num() > 0)
		{
			EyewearComboBox->SetSelectedItem(EyewearList[0]);
		}

		bool bFoundSelectedCharacter = false;
		for (int32 i = 0; i < CharacterPathList.Num(); i++)
		{
			if (CharacterPathList[i] == GetPlayerOwner()->GetCharacterPath())
			{
				CharacterComboBox->SetSelectedItem(CharacterList[i]);
				bFoundSelectedCharacter = true;
				break;
			}
		}
		if (!bFoundSelectedCharacter && CharacterPathList.Num() > 0)
		{
			CharacterComboBox->SetSelectedItem(CharacterList[0]);
		}
	}

	if (GetPlayerOwner().IsValid())
	{
		uint32 CountryFlag = GetPlayerOwner()->GetCountryFlag();
		CountryFlagComboBox->SetSelectedItem(CountyFlagNames[CountryFlag]);
	}

}

SUWPlayerSettingsDialog::~SUWPlayerSettingsDialog()
{
	if (PlayerPreviewTexture != NULL)
	{
		PlayerPreviewTexture->OnNonUObjectRenderTargetUpdate.Unbind();
		PlayerPreviewTexture = NULL;
	}
	FlushRenderingCommands();
	if (PlayerPreviewBrush != NULL)
	{
		// FIXME: Slate will corrupt memory if this is deleted. Must be referencing it somewhere that doesn't get cleaned up...
		//		for now, we'll take the minor memory leak (the texture still gets GC'ed so it's not too bad)
		//delete PlayerPreviewBrush;
		PlayerPreviewBrush->SetResourceObject(NULL);
		PlayerPreviewBrush = NULL;
	}
	if (PlayerPreviewWorld != NULL)
	{
		PlayerPreviewWorld->DestroyWorld(true);
		GEngine->DestroyWorldContext(PlayerPreviewWorld);
		PlayerPreviewWorld = NULL;
	}
	ViewState.Destroy();
}

void SUWPlayerSettingsDialog::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PlayerPreviewTexture);
	Collector.AddReferencedObject(PlayerPreviewMID);
	Collector.AddReferencedObject(PlayerPreviewWorld);
}

void SUWPlayerSettingsDialog::OnNameTextChanged(const FText& NewText)
{
	FString AdjustedText = NewText.ToString();
	FString InvalidNameChars = FString(INVALID_NAME_CHARACTERS);
	for (int32 i = AdjustedText.Len() - 1; i >= 0; i--)
	{
		if (InvalidNameChars.GetCharArray().Contains(AdjustedText.GetCharArray()[i]))
		{
			AdjustedText.GetCharArray().RemoveAt(i);
		}
	}
	if (AdjustedText != NewText.ToString())
	{
		PlayerName->SetText(FText::FromString(AdjustedText));
	}
}

void SUWPlayerSettingsDialog::PlayerColorChanged(FLinearColor NewValue)
{
	SelectedPlayerColor = NewValue;
	RecreatePlayerPreview();
}

FReply SUWPlayerSettingsDialog::OKClick()
{
	GetPlayerOwner()->SetNickname(PlayerName->GetText().ToString());

	uint32 NewFlag = 0;
	for (int32 i=0; i<CountyFlagNames.Num();i++)
	{
		if (*CountyFlagNames[i] == SelectedFlag->GetText().ToString())
		{
			NewFlag = i;
			break;
		}
	}
	
	GetPlayerOwner()->SetCountryFlag(NewFlag,false);

	// If we have a valid PC then tell the PC to set it's name
	AUTPlayerController* UTPlayerController = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);
	if (UTPlayerController != NULL)
	{
		UTPlayerController->ServerChangeName(PlayerName->GetText().ToString());
		UTPlayerController->WeaponBobGlobalScaling = WeaponBobScaling->GetValue() * BOB_SCALING_FACTOR;
		UTPlayerController->EyeOffsetGlobalScaling = ViewBobScaling->GetValue() * BOB_SCALING_FACTOR;
		UTPlayerController->FFAPlayerColor = SelectedPlayerColor;
		UTPlayerController->SaveConfig();

		if (UTPlayerController->GetUTCharacter())
		{
			UTPlayerController->GetUTCharacter()->NotifyTeamChanged();
		}
	}

	UUTProfileSettings* ProfileSettings = GetPlayerOwner()->GetProfileSettings();

	int32 Index = HatList.Find(HatComboBox->GetSelectedItem());
	GetPlayerOwner()->SetHatPath(HatPathList.IsValidIndex(Index) ? HatPathList[Index] : FString());
	Index = EyewearList.Find(EyewearComboBox->GetSelectedItem());
	GetPlayerOwner()->SetEyewearPath(EyewearPathList.IsValidIndex(Index) ? EyewearPathList[Index] : FString());
	Index = CharacterList.Find(CharacterComboBox->GetSelectedItem());
	GetPlayerOwner()->SetCharacterPath(CharacterPathList.IsValidIndex(Index) ? CharacterPathList[Index] : FString());
	
	GetPlayerOwner()->SaveProfileSettings();
	GetPlayerOwner()->CloseDialog(SharedThis(this));

	return FReply::Handled();
}

FReply SUWPlayerSettingsDialog::CancelClick()
{
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}

FReply SUWPlayerSettingsDialog::OnButtonClick(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_OK) OKClick();
	else if (ButtonID == UTDIALOG_BUTTON_CANCEL) CancelClick();
	return FReply::Handled();
}

FReply SUWPlayerSettingsDialog::WeaponConfigClick()
{
	WeaponConfigDelayFrames = 3;
	GetPlayerOwner()->ShowContentLoadingMessage();
	return FReply::Handled();
}

void SUWPlayerSettingsDialog::OnEmote1Committed(int32 NewValue, ETextCommit::Type CommitInfo)
{
	Emote1Index = NewValue;
}

void SUWPlayerSettingsDialog::OnEmote2Committed(int32 NewValue, ETextCommit::Type CommitInfo)
{
	Emote2Index = NewValue;
}

void SUWPlayerSettingsDialog::OnEmote3Committed(int32 NewValue, ETextCommit::Type CommitInfo)
{
	Emote3Index = NewValue;
}

TOptional<int32> SUWPlayerSettingsDialog::GetEmote1Value() const
{
	return Emote1Index;
}

TOptional<int32> SUWPlayerSettingsDialog::GetEmote2Value() const
{
	return Emote2Index;
}

TOptional<int32> SUWPlayerSettingsDialog::GetEmote3Value() const
{
	return Emote3Index;
}

void SUWPlayerSettingsDialog::OnHatSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedHat->SetText(*NewSelection.Get());
	RecreatePlayerPreview();
}

void SUWPlayerSettingsDialog::OnEyewearSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedEyewear->SetText(*NewSelection.Get());
	RecreatePlayerPreview();
}

void SUWPlayerSettingsDialog::OnCharacterSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedCharacter->SetText(*NewSelection.Get());
	RecreatePlayerPreview();
}

void SUWPlayerSettingsDialog::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SUWDialog::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (PlayerPreviewWorld != NULL)
	{
		PlayerPreviewWorld->Tick(LEVELTICK_All, InDeltaTime);
	}
	if (PlayerPreviewTexture != NULL)
	{
		PlayerPreviewTexture->UpdateResource();
	}

	if (WeaponConfigDelayFrames > 0)
	{
		WeaponConfigDelayFrames--;
		if (WeaponConfigDelayFrames == 0)
		{
			GetPlayerOwner()->HideContentLoadingMessage();
			GetPlayerOwner()->OpenDialog(SNew(SUWWeaponConfigDialog).PlayerOwner(GetPlayerOwner()));
		}
	}
}

void SUWPlayerSettingsDialog::RecreatePlayerPreview()
{
	if (PlayerPreviewMesh != NULL)
	{
		PlayerPreviewMesh->Destroy();
	}

	PlayerPreviewMesh = PlayerPreviewWorld->SpawnActor<AUTCharacter>(GetDefault<AUTGameMode>()->DefaultPawnClass, FVector(265.f, 0.f, 4.f), FRotator(0.0f, 180.0f, 0.0f));
	
	// set character mesh
	// NOTE: important this is first since it may affect the following items (socket locations, etc)
	int32 Index = CharacterList.Find(CharacterComboBox->GetSelectedItem());
	FString NewCharPath = CharacterPathList.IsValidIndex(Index) ? CharacterPathList[Index] : FString();
	if (NewCharPath.Len() > 0)
	{
		TSubclassOf<AUTCharacterContent> CharacterClass = LoadClass<AUTCharacterContent>(NULL, *NewCharPath, NULL, LOAD_None, NULL);
		if (CharacterClass != NULL)
		{
			PlayerPreviewMesh->ApplyCharacterData(CharacterClass);
		}
	}

	// set FFA color
	/* FIXME: temporarily removed
	const TArray<UMaterialInstanceDynamic*>& BodyMIs = PlayerPreviewMesh->GetBodyMIs();
	for (UMaterialInstanceDynamic* MI : BodyMIs)
	{
		static FName NAME_TeamColor(TEXT("TeamColor"));
		if (MI != NULL)
		{
			MI->SetVectorParameterValue(NAME_TeamColor, SelectedPlayerColor);
		}
	}*/

	// set hat
	Index = HatList.Find(HatComboBox->GetSelectedItem());
	FString NewHatPath = HatPathList.IsValidIndex(Index) ? HatPathList[Index] : FString();
	if (NewHatPath.Len() > 0)
	{
		TSubclassOf<AUTHat> HatClass = LoadClass<AUTHat>(NULL, *NewHatPath, NULL, LOAD_None, NULL);
		if (HatClass != NULL)
		{
			PlayerPreviewMesh->HatClass = HatClass;
			PlayerPreviewMesh->OnRepHat();
		}
	}

	// set eyewear
	Index = EyewearList.Find(EyewearComboBox->GetSelectedItem());
	FString NewEyewearPath = EyewearPathList.IsValidIndex(Index) ? EyewearPathList[Index] : FString();
	if (NewEyewearPath.Len() > 0)
	{
		TSubclassOf<AUTEyewear> EyewearClass = LoadClass<AUTEyewear>(NULL, *NewEyewearPath, NULL, LOAD_None, NULL);
		if (EyewearClass != NULL)
		{
			PlayerPreviewMesh->EyewearClass = EyewearClass;
			PlayerPreviewMesh->OnRepEyewear();
		}
	}
}

void SUWPlayerSettingsDialog::UpdatePlayerRender(UCanvas* C, int32 Width, int32 Height)
{
	FEngineShowFlags ShowFlags(ESFIM_Game);
	//ShowFlags.SetLighting(false); // FIXME: create some proxy light and use lit mode
	ShowFlags.SetMotionBlur(false);
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(PlayerPreviewTexture->GameThread_GetRenderTargetResource(), PlayerPreviewWorld->Scene, ShowFlags).SetRealtimeUpdate(true));
	
	FVector CameraPosition(0, -50, -50);

	FSceneViewInitOptions PlayerPreviewInitOptions;
	PlayerPreviewInitOptions.SetViewRectangle(FIntRect(0, 0, C->SizeX, C->SizeY));
	PlayerPreviewInitOptions.ViewMatrix = FTranslationMatrix(CameraPosition) * FInverseRotationMatrix(FRotator::ZeroRotator) * FMatrix(FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
	PlayerPreviewInitOptions.ProjectionMatrix = FReversedZPerspectiveMatrix(45.0f * (float)PI / 360.0f, 45.0f * (float)PI / 360.0f, 1.0f, 1.0f, GNearClippingPlane, GNearClippingPlane);
	PlayerPreviewInitOptions.ViewFamily = &ViewFamily;
	PlayerPreviewInitOptions.SceneViewStateInterface = ViewState.GetReference();
	PlayerPreviewInitOptions.BackgroundColor = FLinearColor::Black;
	PlayerPreviewInitOptions.WorldToMetersScale = GetPlayerOwner()->GetWorld()->GetWorldSettings()->WorldToMeters;
	PlayerPreviewInitOptions.CursorPos = FIntPoint(-1, -1);

	FSceneView* View = new FSceneView(PlayerPreviewInitOptions); // note: renderer gets ownership
	View->ViewLocation = FVector::ZeroVector;
	View->ViewRotation = FRotator::ZeroRotator;
	FPostProcessSettings PPSettings = GetDefault<AUTPlayerCameraManager>()->DefaultPPSettings;
	PPSettings.bOverride_AntiAliasingMethod = true;
	PPSettings.AntiAliasingMethod = AAM_None;
	View->OverridePostProcessSettings(PPSettings, 1.0f);
	ViewFamily.Views.Add(View);
	// workaround for hacky renderer code that uses GFrameNumber to decide whether to resize render targets
	--GFrameNumber;
	GetRendererModule().BeginRenderingViewFamily(C->Canvas, &ViewFamily);
}

void SUWPlayerSettingsDialog::DragPlayerPreview(const FVector2D MouseDelta)
{
	if (PlayerPreviewMesh != NULL)
	{
		PlayerPreviewMesh->SetActorRotation(PlayerPreviewMesh->GetActorRotation() + FRotator(0, 0.1f * -MouseDelta.X, 0.0f));
	}
}

void SUWPlayerSettingsDialog::OnFlagSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedFlag->SetText(*NewSelection.Get());
}

#undef LOCTEXT_NAMESPACE

#endif