// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "InternationalizationSettingsModelDetails.h"
#include "Misc/Paths.h"
#include "Internationalization/Culture.h"
#include "UObject/Class.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "InternationalizationSettingsModel.h"
#include "EdGraph/EdGraphSchema.h"
#include "SCulturePicker.h"

#define LOCTEXT_NAMESPACE "InternationalizationSettingsModelDetails"

TSharedRef<IDetailCustomization> FInternationalizationSettingsModelDetails::MakeInstance()
{
	return MakeShareable(new FInternationalizationSettingsModelDetails);
}

namespace
{
	struct FLocalizedCulturesFlyweight
	{
		TArray<FCultureRef> LocalizedCulturesForEditor;
		TArray<FCultureRef> LocalizedCulturesForGame;

		FLocalizedCulturesFlyweight()
		{
			FInternationalization::Get().GetCulturesWithAvailableLocalization(FPaths::GetEditorLocalizationPaths(), LocalizedCulturesForEditor, true);
			FInternationalization::Get().GetCulturesWithAvailableLocalization(FPaths::GetGameLocalizationPaths(), LocalizedCulturesForGame, true);
		}
	};

	class SEditorCultureComboButton : public SCompoundWidget
	{
		SLATE_BEGIN_ARGS(SEditorCultureComboButton){}
		SLATE_END_ARGS()

	public:
		void Construct(const FArguments& InArgs, const TWeakObjectPtr<UInternationalizationSettingsModel>& InSettingsModel, const TSharedRef<FLocalizedCulturesFlyweight>& InLocalizedCulturesFlyweight)
		{
			SettingsModel = InSettingsModel;
			LocalizedCulturesFlyweight = InLocalizedCulturesFlyweight;

			ChildSlot
				[
					SAssignNew(EditorCultureComboButton, SComboButton)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(this, &SEditorCultureComboButton::GetContentText)
					]
				];
			EditorCultureComboButton->SetOnGetMenuContent(FOnGetContent::CreateSP(this, &SEditorCultureComboButton::GetMenuContent));
		}

	private:
		TWeakObjectPtr<UInternationalizationSettingsModel> SettingsModel;
		TSharedPtr<FLocalizedCulturesFlyweight> LocalizedCulturesFlyweight;
		TSharedPtr<SComboButton> EditorCultureComboButton;

	private:
		FText GetContentText() const
		{
			bool IsConfigured = false;
			FString EditorCultureName;
			if (SettingsModel.IsValid())
			{
				IsConfigured = SettingsModel->GetEditorCultureName(EditorCultureName);
			}
			FInternationalization& I18N = FInternationalization::Get();
			const FCulturePtr Culture = IsConfigured ? I18N.GetCulture(EditorCultureName) : I18N.GetCurrentCulture();
			return Culture.IsValid() ? FText::FromString(Culture->GetNativeName()) : LOCTEXT("None", "None");
		}

		TSharedRef<SWidget> GetMenuContent()
		{
			FCulturePtr EditorCulture;
			{
				FInternationalization& I18N = FInternationalization::Get();
				bool IsConfigured = false;
				FString EditorCultureName;
				if (SettingsModel.IsValid())
				{
					IsConfigured = SettingsModel->GetEditorCultureName(EditorCultureName);
				}
				EditorCulture = IsConfigured ? I18N.GetCulture(EditorCultureName) : I18N.GetCurrentCulture();
			}

			const auto& OnSelectionChangedLambda = [=](FCulturePtr& SelectedCulture, ESelectInfo::Type SelectInfo)
			{
				if (SettingsModel.IsValid())
				{
					SettingsModel->SetEditorCultureName(SelectedCulture.IsValid() ? SelectedCulture->GetName() : TEXT(""));
					if (SelectedCulture.IsValid())
					{
						FInternationalization& I18N = FInternationalization::Get();
						I18N.SetCurrentCulture(SelectedCulture->GetName());

						// Find all Schemas and force a visualization cache clear
						for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
						{
							UClass* CurrentClass = *ClassIt;

							if (UEdGraphSchema* Schema = Cast<UEdGraphSchema>(CurrentClass->GetDefaultObject()))
							{
								Schema->ForceVisualizationCacheClear();
							}
						}
					}
				}
				if (EditorCultureComboButton.IsValid())
				{
					EditorCultureComboButton->SetIsOpen(false);
				}
			};

			const auto& IsCulturePickableLambda = [=](FCulturePtr Culture) -> bool
			{
				TArray<FString> CultureNames = Culture->GetPrioritizedParentCultureNames();
				for (const FString& CultureName : CultureNames)
				{
					if (LocalizedCulturesFlyweight->LocalizedCulturesForEditor.Contains(Culture))
					{
						return true;
					}
				}
				return false;
			};

			const auto& CulturePicker = SNew(SCulturePicker)
				.InitialSelection(EditorCulture)
				.OnSelectionChanged_Lambda(OnSelectionChangedLambda)
				.IsCulturePickable_Lambda(IsCulturePickableLambda)
				.DisplayNameFormat(SCulturePicker::ECultureDisplayFormat::ActiveAndNativeCultureDisplayName);

			return SNew(SBox)
				.MaxDesiredHeight(300.0f)
				.WidthOverride(300.0f)
				[
					CulturePicker
				];
		}
	};

	class SNativeGameCultureComboButton : public SCompoundWidget
	{
		SLATE_BEGIN_ARGS(SNativeGameCultureComboButton){}
		SLATE_END_ARGS()

	public:
		void Construct(const FArguments& InArgs, const TWeakObjectPtr<UInternationalizationSettingsModel>& InSettingsModel, const TSharedRef<FLocalizedCulturesFlyweight>& InLocalizedCulturesFlyweight)
		{
			SettingsModel = InSettingsModel;
			LocalizedCulturesFlyweight = InLocalizedCulturesFlyweight;

			ChildSlot
				[
					SAssignNew(NativeGameCultureComboButton, SComboButton)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(this, &SNativeGameCultureComboButton::GetContentText)
					]
				];
			NativeGameCultureComboButton->SetOnGetMenuContent(FOnGetContent::CreateSP(this, &SNativeGameCultureComboButton::GetMenuContent));
		}

	private:
		TWeakObjectPtr<UInternationalizationSettingsModel> SettingsModel;
		TSharedPtr<FLocalizedCulturesFlyweight> LocalizedCulturesFlyweight;
		TSharedPtr<SComboButton> NativeGameCultureComboButton;

	private:
		FText GetContentText() const
		{
			bool IsConfigured = false;
			FString NativeGameCultureName;
			if (SettingsModel.IsValid())
			{
				IsConfigured = SettingsModel->GetNativeGameCultureName(NativeGameCultureName);
			}
			FCulturePtr Culture;
			if (!NativeGameCultureName.IsEmpty())
			{
				FInternationalization& I18N = FInternationalization::Get();
				Culture = I18N.GetCulture(NativeGameCultureName);
			}
			return Culture.IsValid() ? FText::FromString(Culture->GetDisplayName()) : LOCTEXT("None", "None");
		}

		TSharedRef<SWidget> GetMenuContent()
		{
			FCulturePtr NativeGameCulture;
			{
				bool IsConfigured = false;
				FString NativeGameCultureName;
				if (SettingsModel.IsValid())
				{
					IsConfigured = SettingsModel->GetNativeGameCultureName(NativeGameCultureName);
				}
				if (!NativeGameCultureName.IsEmpty())
				{
					FInternationalization& I18N = FInternationalization::Get();
					NativeGameCulture = I18N.GetCulture(NativeGameCultureName);
				}
			}

			const auto& CulturePickerSelectLambda = [=](FCulturePtr& SelectedCulture, ESelectInfo::Type SelectInfo)
			{
				if (SettingsModel.IsValid())
				{
					SettingsModel->SetNativeGameCultureName(SelectedCulture.IsValid() ? SelectedCulture->GetName() : TEXT(""));
					FTextLocalizationManager::Get().RefreshResources();
				}
				if (NativeGameCultureComboButton.IsValid())
				{
					NativeGameCultureComboButton->SetIsOpen(false);
				}
			};
			const auto& CulturePickerIsPickableLambda = [=](FCulturePtr Culture) -> bool
			{
				TArray<FString> CultureNames = Culture->GetPrioritizedParentCultureNames();
				for (const FString& CultureName : CultureNames)
				{
					if (LocalizedCulturesFlyweight->LocalizedCulturesForGame.Contains(Culture))
					{
						return true;
					}
				}
				return false;
			};
			return SNew(SBox)
				.MaxDesiredHeight(300.0f)
				.WidthOverride(300.0f)
				[
					SNew(SCulturePicker)
					.InitialSelection(NativeGameCulture)
					.OnSelectionChanged_Lambda(CulturePickerSelectLambda)
					.IsCulturePickable_Lambda(CulturePickerIsPickableLambda)
					.CanSelectNone(true)
				];
		}
	};
}

void FInternationalizationSettingsModelDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	const TWeakObjectPtr<UInternationalizationSettingsModel> SettingsModel = [&]()
	{
		TArray< TWeakObjectPtr<UObject> > ObjectsBeingCustomized;
		DetailLayout.GetObjectsBeingCustomized(ObjectsBeingCustomized);
		check(ObjectsBeingCustomized.Num() == 1);
		return Cast<UInternationalizationSettingsModel>(ObjectsBeingCustomized.Top().Get());
	}();

	IDetailCategoryBuilder& DetailCategoryBuilder = DetailLayout.EditCategory("Internationalization", LOCTEXT("InternationalizationCategory", "Internationalization"));

	const TSharedRef<FLocalizedCulturesFlyweight> LocalizedCulturesFlyweight = MakeShareable(new FLocalizedCulturesFlyweight());

	// Editor Culture Setting
	const FText EditorCultureSettingDisplayName = LOCTEXT("EditorCultureSettingDisplayName", "Editor Culture");
	const FText EditorCultureSettingToolTip = LOCTEXT("EditorCultureSettingToolTip", "The culture in which the editor should be displayed.");
	FDetailWidgetRow& EditorCultureRow = DetailCategoryBuilder.AddCustomRow(EditorCultureSettingDisplayName);
	EditorCultureRow.NameContent()
		[
			SNew(STextBlock)
			.Text(EditorCultureSettingDisplayName)
			.ToolTipText(EditorCultureSettingToolTip)
			.Font(DetailLayout.GetDetailFont())
		];
	EditorCultureRow.ValueContent()
		[
			SNew(SEditorCultureComboButton, SettingsModel, LocalizedCulturesFlyweight)
		];

	// Native Game Culture Setting
	const FText GameCultureSettingDisplayName = LOCTEXT("NativeGameCultureSettingDisplayName", "Native Game Culture");
	const FText GameCultureSettingToolTip = LOCTEXT("NativeGameCultureSettingToolTip", "The culture in which game content is specified and localized from. WARNING: If this is not set to the culture the game is initially made in, modifying and saving assets may mix localized data in as native data!");
	FDetailWidgetRow& GameCultureRow = DetailCategoryBuilder.AddCustomRow(GameCultureSettingDisplayName);
	GameCultureRow.NameContent()
		[
			SNew(STextBlock)
			.Text(GameCultureSettingDisplayName)
			.ToolTipText(GameCultureSettingToolTip)
			.Font(DetailLayout.GetDetailFont())
		];
	GameCultureRow.ValueContent()
		[
			SNew(SNativeGameCultureComboButton, SettingsModel, LocalizedCulturesFlyweight)
		];

	// Localized Field Names
	const FText FieldNamesSettingDisplayName = LOCTEXT("EditorFieldNamesLabel", "Use Localized Field Names");
	const FText FieldNamesSettingToolTip = LOCTEXT("EditorFieldNamesTooltip", "Toggle showing localized field names. NOTE: Requires restart to take effect.");
	FDetailWidgetRow& FieldNamesRow = DetailCategoryBuilder.AddCustomRow(FieldNamesSettingDisplayName);
	FieldNamesRow.NameContent()
		[
			SNew(STextBlock)
			.Text(FieldNamesSettingDisplayName)
			.ToolTipText(FieldNamesSettingToolTip)
			.Font(DetailLayout.GetDetailFont())
		];
	FieldNamesRow.ValueContent()
		.MaxDesiredWidth(300.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([=]()
			{
				return SettingsModel.IsValid() && SettingsModel->ShouldLoadLocalizedPropertyNames() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
				.ToolTipText(FieldNamesSettingToolTip)
				.OnCheckStateChanged_Lambda([=](ECheckBoxState State)
			{
				if (SettingsModel.IsValid())
				{
					SettingsModel->ShouldLoadLocalizedPropertyNames(State == ECheckBoxState::Checked);
					FTextLocalizationManager::Get().RefreshResources();
				}
			})
		];

	// Localized Node and Pin Names
	const FText NodeAndPinsNamesSettingDisplayName = LOCTEXT("GraphEditorNodesAndPinsLocalized", "Use Localized Graph Editor Nodes and Pins");
	const FText NodeAndPinsNamesSettingToolTip = LOCTEXT("GraphEditorNodesAndPinsLocalized_Tooltip", "Toggle localized node and pin titles in all graph editors.");
	FDetailWidgetRow& NodesAndPinsNameRow = DetailCategoryBuilder.AddCustomRow(NodeAndPinsNamesSettingDisplayName);
	NodesAndPinsNameRow.NameContent()
		[
			SNew(STextBlock)
			.Text(NodeAndPinsNamesSettingDisplayName)
			.ToolTipText(NodeAndPinsNamesSettingToolTip)
			.Font(DetailLayout.GetDetailFont())
		];
	NodesAndPinsNameRow.ValueContent()
		.MaxDesiredWidth(300.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([=]()
			{
				return SettingsModel.IsValid() && SettingsModel->ShouldShowNodesAndPinsUnlocalized() ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
			})
				.ToolTipText(NodeAndPinsNamesSettingToolTip)
				.OnCheckStateChanged_Lambda([=](ECheckBoxState State)
			{
				if (SettingsModel.IsValid())
				{
					SettingsModel->ShouldShowNodesAndPinsUnlocalized(State == ECheckBoxState::Unchecked);

					// Find all Schemas and force a visualization cache clear
					for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
					{
						UClass* CurrentClass = *ClassIt;

						if (UEdGraphSchema* Schema = Cast<UEdGraphSchema>(CurrentClass->GetDefaultObject()))
						{
							Schema->ForceVisualizationCacheClear();
						}
					}
				}
			})
		];
}

#undef LOCTEXT_NAMESPACE
