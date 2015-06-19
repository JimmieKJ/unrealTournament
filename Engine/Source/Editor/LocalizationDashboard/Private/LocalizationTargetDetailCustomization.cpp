// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LocalizationDashboardPrivatePCH.h"
#include "LocalizationTargetDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "LocalizationTargetTypes.h"
#include "DetailWidgetRow.h"
#include "SLocalizationTargetStatusButton.h"
#include "SLocalizationTargetEditorCultureRow.h"
#include "SCulturePicker.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "IDetailsView.h"
#include "LocalizationCommandletTasks.h"
#include "ObjectEditorUtils.h"
#include "LocalizationDashboardSettings.h"
#include "SErrorText.h"

#define LOCTEXT_NAMESPACE "LocalizationTargetEditor"

namespace
{
	struct FConfigValueIdentity
	{
		FString SectionName;
		FString KeyName;
		FString ConfigFilePath;
	};

	TMap<ELocalizationTargetLoadingPolicy, FConfigValueIdentity> LoadingPolicyToConfigSettingMap = []()
	{
		TMap<ELocalizationTargetLoadingPolicy, FConfigValueIdentity> Map;
		Map.Add(ELocalizationTargetLoadingPolicy::Always,			FConfigValueIdentity {TEXT("Internationalization"),	TEXT("LocalizationPaths"),				GEngineIni });
		Map.Add(ELocalizationTargetLoadingPolicy::Editor,			FConfigValueIdentity {TEXT("Internationalization"),	TEXT("LocalizationPaths"),				GEditorIni });
		Map.Add(ELocalizationTargetLoadingPolicy::Editor,			FConfigValueIdentity {TEXT("Internationalization"),	TEXT("LocalizationPaths"),				GEditorIni });
		Map.Add(ELocalizationTargetLoadingPolicy::PropertyNames,	FConfigValueIdentity {TEXT("Internationalization"),	TEXT("PropertyNameLocalizationPaths"),	GEditorIni });
		Map.Add(ELocalizationTargetLoadingPolicy::ToolTips,			FConfigValueIdentity {TEXT("Internationalization"),	TEXT("ToolTipLocalizationPaths"),		GEditorIni });
		Map.Add(ELocalizationTargetLoadingPolicy::Game,				FConfigValueIdentity {TEXT("Internationalization"),	TEXT("LocalizationPaths"),				GGameIni });
		return Map;
	}();
}

FLocalizationTargetDetailCustomization::FLocalizationTargetDetailCustomization()
	: DetailLayoutBuilder(nullptr)
	, NewEntryIndexToBeInitialized(INDEX_NONE)
{
}

class FLocalizationTargetEditorCommands : public TCommands<FLocalizationTargetEditorCommands>
{
public:
	FLocalizationTargetEditorCommands() 
		: TCommands<FLocalizationTargetEditorCommands>("LocalizationTargetEditor", NSLOCTEXT("Contexts", "LocalizationTargetEditor", "Localization Target Editor"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}

	TSharedPtr<FUICommandInfo> Gather;
	TSharedPtr<FUICommandInfo> ImportAllCultures;
	TSharedPtr<FUICommandInfo> ExportAllCultures;
	TSharedPtr<FUICommandInfo> CountWords;
	TSharedPtr<FUICommandInfo> CompileAllCultures;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};

void FLocalizationTargetEditorCommands::RegisterCommands() 
{
	UI_COMMAND( Gather, "Gather", "Gathers text for this target.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( ImportAllCultures, "Import All", "Imports translations for all cultures of this target.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( ExportAllCultures, "Export All", "Exports translations for all cultures of this target.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( CountWords, "Count Words", "Recalculates the word counts for this target.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( CompileAllCultures, "Compile All", "Compiles translations for all cultures of this target.", EUserInterfaceActionType::Button, FInputChord() );
}

void FLocalizationTargetDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailLayoutBuilder = &DetailBuilder;
	{
		TArray< TWeakObjectPtr<UObject> > ObjectsBeingCustomized;
		DetailLayoutBuilder->GetObjectsBeingCustomized(ObjectsBeingCustomized);
		LocalizationTarget = CastChecked<ULocalizationTarget>(ObjectsBeingCustomized.Top().Get());
		TargetSet = CastChecked<ULocalizationTargetSet>(LocalizationTarget->GetOuter());
	}

	TargetSettingsPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULocalizationTarget, Settings));

	if (TargetSettingsPropertyHandle.IsValid() && TargetSettingsPropertyHandle->IsValidHandle())
	{
		TargetSettingsPropertyHandle->MarkHiddenByCustomization();
	}

	typedef TFunction<void (const TSharedRef<IPropertyHandle>&, IDetailCategoryBuilder&)> FPropertyCustomizationFunction;
	TMap<FName, FPropertyCustomizationFunction> PropertyCustomizationMap;

	PropertyCustomizationMap.Add(GET_MEMBER_NAME_CHECKED(FLocalizationTargetSettings, Name), [&](const TSharedRef<IPropertyHandle>& MemberPropertyHandle, IDetailCategoryBuilder& DetailCategoryBuilder)
	{
		FDetailWidgetRow& DetailWidgetRow = DetailCategoryBuilder.AddCustomRow( MemberPropertyHandle->GetPropertyDisplayName() );
		DetailWidgetRow.NameContent()
			[
				MemberPropertyHandle->CreatePropertyNameWidget()
			];
		DetailWidgetRow.ValueContent()
			[
				SAssignNew(TargetNameEditableTextBox, SEditableTextBox)
				.Text(this, &FLocalizationTargetDetailCustomization::GetTargetName)
				.RevertTextOnEscape(true)
				.OnTextChanged(this, &FLocalizationTargetDetailCustomization::OnTargetNameChanged)
				.OnTextCommitted(this, &FLocalizationTargetDetailCustomization::OnTargetNameCommitted)
			];
	});

	PropertyCustomizationMap.Add(GET_MEMBER_NAME_CHECKED(FLocalizationTargetSettings, ConflictStatus), [&](const TSharedRef<IPropertyHandle>& MemberPropertyHandle, IDetailCategoryBuilder& DetailCategoryBuilder)
	{
		FDetailWidgetRow& StatusRow = DetailCategoryBuilder.AddCustomRow( MemberPropertyHandle->GetPropertyDisplayName() );
		StatusRow.NameContent()
			[
				MemberPropertyHandle->CreatePropertyNameWidget()
			];
		StatusRow.ValueContent()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SLocalizationTargetStatusButton, *LocalizationTarget)
			];
	});

	PropertyCustomizationMap.Add(GET_MEMBER_NAME_CHECKED(FLocalizationTargetSettings, TargetDependencies), [&](const TSharedRef<IPropertyHandle>& MemberPropertyHandle, IDetailCategoryBuilder& DetailCategoryBuilder)
	{
		const auto& MenuContentLambda = [this]() -> TSharedRef<SWidget>
		{
			RebuildTargetsList();

			if (TargetDependenciesOptionsList.Num() > 0)
			{
				return SNew(SBox)
					.MaxDesiredHeight(400.0f)
					.MaxDesiredWidth(300.0f)
					[
						SNew(SListView<ULocalizationTarget*>)
						.SelectionMode(ESelectionMode::None)
						.ListItemsSource(&TargetDependenciesOptionsList)
						.OnGenerateRow(this, &FLocalizationTargetDetailCustomization::OnGenerateTargetRow)
					];
			}
			else
			{
				return SNullWidget::NullWidget;
			}
		};

		FDetailWidgetRow& TargetDependenciesRow = DetailCategoryBuilder.AddCustomRow( MemberPropertyHandle->GetPropertyDisplayName() );
		TargetDependenciesRow.NameContent()
			[
				MemberPropertyHandle->CreatePropertyNameWidget()
			];
		TargetDependenciesRow.ValueContent()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SComboButton)
				.ButtonContent()
				[
					SAssignNew(TargetDependenciesHorizontalBox, SHorizontalBox)
				]
				.HasDownArrow(true)
				.OnGetMenuContent_Lambda(MenuContentLambda)
			];

		RebuildTargetDependenciesBox();
	});

	PropertyCustomizationMap.Add(GET_MEMBER_NAME_CHECKED(FLocalizationTargetSettings, NativeCultureIndex), [&](const TSharedRef<IPropertyHandle>& MemberPropertyHandle, IDetailCategoryBuilder& DetailCategoryBuilder)
	{
		NativeCultureIndexPropertyHandle = MemberPropertyHandle;
	});

	PropertyCustomizationMap.Add(GET_MEMBER_NAME_CHECKED(FLocalizationTargetSettings, SupportedCulturesStatistics), [&](const TSharedRef<IPropertyHandle>& MemberPropertyHandle, IDetailCategoryBuilder& DetailCategoryBuilder)
	{
		SupportedCulturesStatisticsPropertyHandle = MemberPropertyHandle;

		SupportedCulturesStatisticsPropertyHandle_OnNumElementsChanged = FSimpleDelegate::CreateSP(this, &FLocalizationTargetDetailCustomization::RebuildListedCulturesList);
		SupportedCulturesStatisticsPropertyHandle->AsArray()->SetOnNumElementsChanged(SupportedCulturesStatisticsPropertyHandle_OnNumElementsChanged);

		FLocalizationTargetEditorCommands::Register();
		const TSharedRef< FUICommandList > CommandList = MakeShareable(new FUICommandList);
		FToolBarBuilder ToolBarBuilder( CommandList, FMultiBoxCustomization::AllowCustomization("LocalizationTargetEditor") );

		TAttribute<FText> GatherToolTipTextAttribute = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda([this]() -> FText
			{
				return CanGather() ? FLocalizationTargetEditorCommands::Get().Gather->GetDescription() : LOCTEXT("GatherDisabledToolTip", "Must have a native culture specified in order to gather.");
			}));
		CommandList->MapAction( FLocalizationTargetEditorCommands::Get().Gather, FExecuteAction::CreateSP(this, &FLocalizationTargetDetailCustomization::Gather), FCanExecuteAction::CreateSP(this, &FLocalizationTargetDetailCustomization::CanGather));
		ToolBarBuilder.AddToolBarButton(FLocalizationTargetEditorCommands::Get().Gather, NAME_None, TAttribute<FText>(), GatherToolTipTextAttribute, FSlateIcon(FEditorStyle::GetStyleSetName(), "LocalizationTargetEditor.Gather"));

		CommandList->MapAction( FLocalizationTargetEditorCommands::Get().ImportAllCultures, FExecuteAction::CreateSP(this, &FLocalizationTargetDetailCustomization::ImportAllCultures), FCanExecuteAction::CreateSP(this, &FLocalizationTargetDetailCustomization::CanImportAllCultures));
		ToolBarBuilder.AddToolBarButton(FLocalizationTargetEditorCommands::Get().ImportAllCultures, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "LocalizationTargetEditor.ImportAllCultures"));

		CommandList->MapAction( FLocalizationTargetEditorCommands::Get().ExportAllCultures, FExecuteAction::CreateSP(this, &FLocalizationTargetDetailCustomization::ExportAllCultures), FCanExecuteAction::CreateSP(this, &FLocalizationTargetDetailCustomization::CanExportAllCultures));
		ToolBarBuilder.AddToolBarButton(FLocalizationTargetEditorCommands::Get().ExportAllCultures, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "LocalizationTargetEditor.ExportAllCultures"));

		CommandList->MapAction( FLocalizationTargetEditorCommands::Get().CountWords, FExecuteAction::CreateSP(this, &FLocalizationTargetDetailCustomization::CountWords), FCanExecuteAction::CreateSP(this, &FLocalizationTargetDetailCustomization::CanCountWords));
		ToolBarBuilder.AddToolBarButton(FLocalizationTargetEditorCommands::Get().CountWords, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "LocalizationTargetEditor.CountWords"));

		CommandList->MapAction( FLocalizationTargetEditorCommands::Get().CompileAllCultures, FExecuteAction::CreateSP(this, &FLocalizationTargetDetailCustomization::CompileAllCultures), FCanExecuteAction::CreateSP(this, &FLocalizationTargetDetailCustomization::CanCompileAllCultures));
		ToolBarBuilder.AddToolBarButton(FLocalizationTargetEditorCommands::Get().CompileAllCultures, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "LocalizationTargetEditor.CompileAllCultures"));

		BuildListedCulturesList();

		DetailCategoryBuilder.AddCustomRow( SupportedCulturesStatisticsPropertyHandle->GetPropertyDisplayName() )
			.WholeRowContent()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					ToolBarBuilder.MakeWidget()
				]
				+SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(SupportedCultureListView, SListView< TSharedPtr<IPropertyHandle> >)
						.OnGenerateRow(this, &FLocalizationTargetDetailCustomization::OnGenerateCultureRow)
						.ListItemsSource(&ListedCultureStatisticProperties)
						.SelectionMode(ESelectionMode::None)
						.HeaderRow
						(
						SNew(SHeaderRow)
						+SHeaderRow::Column("IsNative")
						.DefaultLabel( NSLOCTEXT("LocalizationCulture", "IsNativeColumnLabel", "Native"))
						.HAlignHeader(HAlign_Center)
						.HAlignCell(HAlign_Center)
						.VAlignCell(VAlign_Center)
						.FillWidth(0.1f)
						+SHeaderRow::Column("Culture")
						.DefaultLabel( NSLOCTEXT("LocalizationCulture", "CultureColumnLabel", "Culture"))
						.HAlignHeader(HAlign_Fill)
						.HAlignCell(HAlign_Fill)
						.VAlignCell(VAlign_Center)
						.FillWidth(0.2f)
						+SHeaderRow::Column("WordCount")
						.DefaultLabel( NSLOCTEXT("LocalizationCulture", "WordCountColumnLabel", "Word Count"))
						.HAlignHeader(HAlign_Center)
						.HAlignCell(HAlign_Fill)
						.VAlignCell(VAlign_Center)
						.FillWidth(0.4f)
						+SHeaderRow::Column("Actions")
						.DefaultLabel( NSLOCTEXT("LocalizationCulture", "ActionsColumnLabel", "Actions"))
						.HAlignHeader(HAlign_Center)
						.HAlignCell(HAlign_Center)
						.VAlignCell(VAlign_Center)
						.FillWidth(0.3f)
						)
					]
				+SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Center)
					[
						SAssignNew(NoSupportedCulturesErrorText, SErrorText)
					]
				+SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Center)
					[
						SAssignNew(AddNewSupportedCultureComboButton, SComboButton)
						.ButtonContent()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("LocalizationCulture", "AddNewCultureButtonLabel", "Add New Culture"))
						]
						.MenuContent()
							[
								SNew(SBox)
								.MaxDesiredHeight(400.0f)
								.MaxDesiredWidth(300.0f)
								[
									SAssignNew(SupportedCulturePicker, SCulturePicker)
									.OnSelectionChanged(this, &FLocalizationTargetDetailCustomization::OnNewSupportedCultureSelected)
									.IsCulturePickable(this, &FLocalizationTargetDetailCustomization::IsCultureSelectableAsSupported)
								]
							]
					]
			];
	});

	UStructProperty* const SettingsStructProperty = CastChecked<UStructProperty>(TargetSettingsPropertyHandle->GetProperty());
	for (TFieldIterator<UProperty> Iterator(SettingsStructProperty->Struct); Iterator; ++Iterator)
	{
		UProperty* const MemberProperty = *Iterator;
		
		const bool IsEditable = MemberProperty->HasAnyPropertyFlags(CPF_Edit);
		if (IsEditable)
		{
			const FName CategoryName = FObjectEditorUtils::GetCategoryFName(MemberProperty);
			IDetailCategoryBuilder& DetailCategoryBuilder = DetailBuilder.EditCategory(CategoryName);

			const FName PropertyName = MemberProperty->GetFName();

			const TSharedPtr<IPropertyHandle> MemberPropertyHandle = TargetSettingsPropertyHandle->GetChildHandle(MemberProperty->GetFName());
			if (MemberPropertyHandle.IsValid() && MemberPropertyHandle->IsValidHandle())
			{
				FPropertyCustomizationFunction* const Function = PropertyCustomizationMap.Find(PropertyName);
				if (Function)
				{
					MemberPropertyHandle->MarkHiddenByCustomization();
					(*Function)(MemberPropertyHandle.ToSharedRef(), DetailCategoryBuilder);
				}
				else
				{
					DetailCategoryBuilder.AddProperty(MemberPropertyHandle);
				}
			}
		}
	}

	{
		IDetailCategoryBuilder& DetailCategoryBuilder = DetailBuilder.EditCategory("Target");
		FDetailWidgetRow& DetailWidgetRow = DetailCategoryBuilder.AddCustomRow(LOCTEXT("LocalizationTargetLoadingPolicyRowFilterString", "Loading Policy"));

		static const TArray< TSharedPtr<ELocalizationTargetLoadingPolicy> > LoadingPolicies = []()
		{
			UEnum* const LoadingPolicyEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("ELocalizationTargetLoadingPolicy"));
			TArray< TSharedPtr<ELocalizationTargetLoadingPolicy> > Array;
			for (int32 i = 0; i < LoadingPolicyEnum->NumEnums() - 1; ++i)
			{
				Array.Add( MakeShareable( new ELocalizationTargetLoadingPolicy(static_cast<ELocalizationTargetLoadingPolicy>(i)) ) );
			}
			return Array;
		}();

		DetailWidgetRow.NameContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LocalizationTargetLoadingPolicyRowName", "Loading Policy"))
			];
		DetailWidgetRow.ValueContent()
			[
				SNew(SComboBox< TSharedPtr<ELocalizationTargetLoadingPolicy> >)
				.OptionsSource(&LoadingPolicies)
				.OnSelectionChanged(this, &FLocalizationTargetDetailCustomization::OnLoadingPolicySelectionChanged)
				.OnGenerateWidget(this, &FLocalizationTargetDetailCustomization::GenerateWidgetForLoadingPolicy)
				.InitiallySelectedItem(LoadingPolicies[static_cast<int32>(GetLoadingPolicy())])
				.Content()
				[
					SNew(STextBlock)
					.Text_Lambda([this]() 
					{
						UEnum* const LoadingPolicyEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("ELocalizationTargetLoadingPolicy"));
						return LoadingPolicyEnum->GetDisplayNameText(static_cast<int32>(GetLoadingPolicy()));
					})
				]
			];
	}


	//{
	//	IDetailCategoryBuilder& ServiceProviderCategoryBuilder = DetailBuilder.EditCategory(FName("LocalizationServiceProvider"));

	//	ILocalizationServiceProvider* const LSP = ILocalizationDashboardModule::Get().GetCurrentLocalizationServiceProvider();
	//	if (LSP && LocalizationTarget)
	//	{
	//		LSP->CustomizeTargetDetails(ServiceProviderCategoryBuilder, *LocalizationTarget);
	//	}
	//}
}

FLocalizationTargetSettings* FLocalizationTargetDetailCustomization::GetTargetSettings() const
{
	return LocalizationTarget.IsValid() ? &(LocalizationTarget->Settings) : nullptr;
}

TSharedPtr<IPropertyHandle> FLocalizationTargetDetailCustomization::GetTargetSettingsPropertyHandle() const
{
	return TargetSettingsPropertyHandle;
}

FText FLocalizationTargetDetailCustomization::GetTargetName() const
{
	if (LocalizationTarget.IsValid())
	{
		return FText::FromString(LocalizationTarget->Settings.Name);
	}
	return FText::GetEmpty();
}

bool FLocalizationTargetDetailCustomization::IsTargetNameUnique(const FString& Name) const
{
	TArray<ULocalizationTarget*> AllLocalizationTargets;
	ULocalizationTargetSet* EngineTargetSet = ULocalizationDashboardSettings::GetEngineTargetSet();
	if (EngineTargetSet != TargetSet)
	{
		AllLocalizationTargets.Append(EngineTargetSet->TargetObjects);
	}
	AllLocalizationTargets.Append(TargetSet->TargetObjects);

	for (ULocalizationTarget* const TargetObject : AllLocalizationTargets)
	{
		if (TargetObject != LocalizationTarget)
		{
			if (TargetObject->Settings.Name == LocalizationTarget->Settings.Name)
			{
				return false;
			}
		}
	}
	return true;
}

void FLocalizationTargetDetailCustomization::OnTargetNameChanged(const FText& NewText)
{
	const FString& NewName = NewText.ToString();

	// TODO: Target names must be valid directory names, because they are used as directory names.
	// ValidatePath allows /, which is not a valid directory name character
	FText Error;
	if (!FPaths::ValidatePath(NewName, &Error))
	{
		TargetNameEditableTextBox->SetError(Error);
		return;		
	}

	// Target name must be unique.
	if (!IsTargetNameUnique(NewName))
	{
		TargetNameEditableTextBox->SetError(LOCTEXT("DuplicateTargetNameError", "Target name must be unique."));
		return;
	}

	// Clear error if nothing has failed.
	TargetNameEditableTextBox->SetError(FText::GetEmpty());
}

void FLocalizationTargetDetailCustomization::OnTargetNameCommitted(const FText& NewText, ETextCommit::Type Type)
{
	// Target name must be unique.
	if (!IsTargetNameUnique(NewText.ToString()))
	{
		return;
	}

	if (TargetSettingsPropertyHandle.IsValid() && TargetSettingsPropertyHandle->IsValidHandle())
	{
		FLocalizationTargetSettings* const TargetSettings = GetTargetSettings();
		if (TargetSettings)
		{
			// Early out if the committed name is the same as the current name.
			if (TargetSettings->Name == NewText.ToString())
			{
				return;
			}

			const TSharedPtr<IPropertyHandle> TargetNamePropertyHandle = TargetSettingsPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLocalizationTargetSettings, Name));

			if (TargetNamePropertyHandle.IsValid() && TargetNamePropertyHandle->IsValidHandle())
			{
				TargetNamePropertyHandle->NotifyPreChange();
			}

			LocalizationTarget->RenameTargetAndFiles(NewText.ToString());

			if (TargetNamePropertyHandle.IsValid() && TargetNamePropertyHandle->IsValidHandle())
			{
				TargetNamePropertyHandle->NotifyPostChange();
			}
		}
	}
}

ELocalizationTargetLoadingPolicy FLocalizationTargetDetailCustomization::GetLoadingPolicy() const
{
	const FString DataDirectory = LocalizationConfigurationScript::GetDataDirectory(LocalizationTarget.Get());

	for (const TPair<ELocalizationTargetLoadingPolicy, FConfigValueIdentity>& Pair : LoadingPolicyToConfigSettingMap)
	{
		TArray<FString> LocalizationPaths;
		GConfig->GetArray( *Pair.Value.SectionName, *Pair.Value.KeyName, LocalizationPaths, Pair.Value.ConfigFilePath );

		if (LocalizationPaths.Contains(DataDirectory))
		{
			return Pair.Key;
		}
	}

	return ELocalizationTargetLoadingPolicy::Never;
}

void FLocalizationTargetDetailCustomization::SetLoadingPolicy(const ELocalizationTargetLoadingPolicy LoadingPolicy)
{
	const FString DataDirectory = LocalizationConfigurationScript::GetDataDirectory(LocalizationTarget.Get());

	for (const TPair<ELocalizationTargetLoadingPolicy, FConfigValueIdentity>& Pair : LoadingPolicyToConfigSettingMap)
	{
		TArray<FString> LocalizationPaths;
		GConfig->GetArray( *Pair.Value.SectionName, *Pair.Value.KeyName, LocalizationPaths, Pair.Value.ConfigFilePath );
		// Add this localization target's data directory to the appropriate localization path setting.
		if (Pair.Key == LoadingPolicy && !LocalizationPaths.Contains(DataDirectory))
		{
			LocalizationPaths.Add(DataDirectory);
			GConfig->SetArray( *Pair.Value.SectionName, *Pair.Value.KeyName, LocalizationPaths, Pair.Value.ConfigFilePath );
		}
		// Remove this localization target's data directory from any inappropriate localization path setting.
		else if (LocalizationPaths.Contains(DataDirectory))
		{
			LocalizationPaths.Remove(DataDirectory);
			GConfig->SetArray( *Pair.Value.SectionName, *Pair.Value.KeyName, LocalizationPaths, Pair.Value.ConfigFilePath );
		}
	}
}

void FLocalizationTargetDetailCustomization::OnLoadingPolicySelectionChanged(TSharedPtr<ELocalizationTargetLoadingPolicy> LoadingPolicy, ESelectInfo::Type SelectInfo)
{
	SetLoadingPolicy(*LoadingPolicy.Get());
};

TSharedRef<SWidget> FLocalizationTargetDetailCustomization::GenerateWidgetForLoadingPolicy(TSharedPtr<ELocalizationTargetLoadingPolicy> LoadingPolicy)
{
	UEnum* const LoadingPolicyEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("ELocalizationTargetLoadingPolicy"));
	return SNew(STextBlock)
		.Text(LoadingPolicyEnum->GetDisplayNameText(static_cast<int32>(*LoadingPolicy.Get())));
};

void FLocalizationTargetDetailCustomization::RebuildTargetDependenciesBox()
{
	if (TargetDependenciesHorizontalBox.IsValid())
	{
		for (const TSharedPtr<SWidget>& Widget : TargetDependenciesWidgets)
		{
			TargetDependenciesHorizontalBox->RemoveSlot(Widget.ToSharedRef());
		}
		TargetDependenciesWidgets.Empty();

		TArray<ULocalizationTarget*> AllLocalizationTargets;
		ULocalizationTargetSet* EngineTargetSet = ULocalizationDashboardSettings::GetEngineTargetSet();
		if (EngineTargetSet != TargetSet)
		{
			AllLocalizationTargets.Append(EngineTargetSet->TargetObjects);
		}
		AllLocalizationTargets.Append(TargetSet->TargetObjects);

		for (const FGuid& TargetDependencyGuid : LocalizationTarget->Settings.TargetDependencies)
		{
			ULocalizationTarget** const TargetDependency = AllLocalizationTargets.FindByPredicate([TargetDependencyGuid](ULocalizationTarget* SomeLocalizationTarget)->bool{return SomeLocalizationTarget->Settings.Guid == TargetDependencyGuid;});
			if (TargetDependency)
			{
				TWeakObjectPtr<ULocalizationTarget> TargetDependencyPtr = *TargetDependency;
				const auto& GetTargetDependencyName = [TargetDependencyPtr] {return FText::FromString(TargetDependencyPtr->Settings.Name);};
				const TSharedRef<SWidget> Widget = SNew(SBorder)
				[
					SNew(STextBlock)
					.Text_Lambda(GetTargetDependencyName)
				];

				TargetDependenciesWidgets.Add(Widget);
				TargetDependenciesHorizontalBox->AddSlot()
				[
					Widget
				];
			}
		}
	}
}

void FLocalizationTargetDetailCustomization::RebuildTargetsList()
{
	TargetDependenciesOptionsList.Empty();
	TFunction<bool (ULocalizationTarget* const)> DoesTargetDependOnUs;
	DoesTargetDependOnUs = [&, this](ULocalizationTarget* const OtherTarget) -> bool
	{
		if (OtherTarget->Settings.TargetDependencies.Contains(LocalizationTarget->Settings.Guid))
		{
			return true;
		}

		for (const FGuid& OtherTargetDependencyGuid : OtherTarget->Settings.TargetDependencies)
		{
			ULocalizationTarget** const OtherTargetDependency = TargetSet->TargetObjects.FindByPredicate([OtherTargetDependencyGuid](ULocalizationTarget* SomeLocalizationTarget)->bool{return SomeLocalizationTarget->Settings.Guid == OtherTargetDependencyGuid;});
			if (OtherTargetDependency && DoesTargetDependOnUs(*OtherTargetDependency))
			{
				return true;
			}
		}

		return false;
	};

	TArray<ULocalizationTarget*> AllLocalizationTargets;
	ULocalizationTargetSet* EngineTargetSet = ULocalizationDashboardSettings::GetEngineTargetSet();
	if (EngineTargetSet != TargetSet)
	{
		AllLocalizationTargets.Append(EngineTargetSet->TargetObjects);
	}
	AllLocalizationTargets.Append(TargetSet->TargetObjects);
	for (ULocalizationTarget* const OtherTarget : AllLocalizationTargets)
	{
		if (OtherTarget != LocalizationTarget)
		{
			if (!DoesTargetDependOnUs(OtherTarget))
			{
				TargetDependenciesOptionsList.Add(OtherTarget);
			}
		}
	}

	if (TargetDependenciesListView.IsValid())
	{
		TargetDependenciesListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> FLocalizationTargetDetailCustomization::OnGenerateTargetRow(ULocalizationTarget* OtherLocalizationTarget, const TSharedRef<STableViewBase>& Table)
{
	return SNew(STableRow<ULocalizationTarget*>, Table)
		.ShowSelection(true)
		.Content()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged_Lambda([this, OtherLocalizationTarget](ECheckBoxState State)
			{
				OnTargetDependencyCheckStateChanged(OtherLocalizationTarget, State);
			})
				.IsChecked_Lambda([this, OtherLocalizationTarget]()->ECheckBoxState
			{
				return IsTargetDependencyChecked(OtherLocalizationTarget);
			})
				.Content()
				[
					SNew(STextBlock)
					.Text(FText::FromString(OtherLocalizationTarget->Settings.Name))
				]
		];
}

void FLocalizationTargetDetailCustomization::OnTargetDependencyCheckStateChanged(ULocalizationTarget* const OtherLocalizationTarget, const ECheckBoxState State)
{
	const TSharedPtr<IPropertyHandle> TargetDependenciesPropertyHandle = TargetSettingsPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLocalizationTargetSettings, TargetDependencies));

	if (TargetDependenciesPropertyHandle.IsValid() && TargetDependenciesPropertyHandle->IsValidHandle())
	{
		TargetDependenciesPropertyHandle->NotifyPreChange();
	}

	switch (State)
	{
	case ECheckBoxState::Checked:
		LocalizationTarget->Settings.TargetDependencies.Add(OtherLocalizationTarget->Settings.Guid);
		break;
	case ECheckBoxState::Unchecked:
		LocalizationTarget->Settings.TargetDependencies.Remove(OtherLocalizationTarget->Settings.Guid);
		break;
	}

	if (TargetDependenciesPropertyHandle.IsValid() && TargetDependenciesPropertyHandle->IsValidHandle())
	{
		TargetDependenciesPropertyHandle->NotifyPostChange();
	}

	RebuildTargetDependenciesBox();
}

ECheckBoxState FLocalizationTargetDetailCustomization::IsTargetDependencyChecked(ULocalizationTarget* const OtherLocalizationTarget) const
{
	return LocalizationTarget->Settings.TargetDependencies.Contains(OtherLocalizationTarget->Settings.Guid) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

bool FLocalizationTargetDetailCustomization::CanGather() const
{
	return LocalizationTarget.IsValid() && LocalizationTarget->Settings.SupportedCulturesStatistics.Num() > 0 && LocalizationTarget->Settings.SupportedCulturesStatistics.IsValidIndex(LocalizationTarget->Settings.NativeCultureIndex);
}

void FLocalizationTargetDetailCustomization::Gather()
{
	if (LocalizationTarget.IsValid())
	{
		// Save unsaved packages.
		const bool bPromptUserToSave = true;
		const bool bSaveMapPackages = true;
		const bool bSaveContentPackages = true;
		const bool bFastSave = false;
		const bool bNotifyNoPackagesSaved = false;
		const bool bCanBeDeclined = true;
		bool DidPackagesNeedSaving;
		const bool WerePackagesSaved = FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages, bFastSave, bNotifyNoPackagesSaved, bCanBeDeclined, &DidPackagesNeedSaving);

		if (DidPackagesNeedSaving && !WerePackagesSaved)
		{
			// Give warning dialog.
			const FText MessageText = NSLOCTEXT("LocalizationCultureActions", "UnsavedPackagesWarningDialogMessage", "There are unsaved changes. These changes may not be gathered from correctly.");
			const FText TitleText = NSLOCTEXT("LocalizationCultureActions", "UnsavedPackagesWarningDialogTitle", "Unsaved Changes Before Gather");
			switch(FMessageDialog::Open(EAppMsgType::OkCancel, MessageText, &TitleText))
			{
			case EAppReturnType::Cancel:
				{
					return;
				}
				break;
			}
		}

		// Execute gather.
		const TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(DetailLayoutBuilder->GetDetailsView().AsShared());
		LocalizationCommandletTasks::GatherTarget(ParentWindow.ToSharedRef(), LocalizationTarget.Get());

		UpdateTargetFromReports();
	}
}

bool FLocalizationTargetDetailCustomization::CanImportAllCultures() const
{
	return LocalizationTarget.IsValid() && LocalizationTarget->Settings.SupportedCulturesStatistics.Num() > 0;
}

void FLocalizationTargetDetailCustomization::ImportAllCultures()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (LocalizationTarget.IsValid() && DesktopPlatform)
	{
		void* ParentWindowWindowHandle = NULL;
		const TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(DetailLayoutBuilder->GetDetailsView().AsShared());
		if (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
		{
			ParentWindowWindowHandle = ParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}

		const FString DefaultPath = FPaths::ConvertRelativePathToFull(LocalizationConfigurationScript::GetDataDirectory(LocalizationTarget.Get()));

		FText DialogTitle;
		{
			FFormatNamedArguments FormatArguments;
			FormatArguments.Add(TEXT("TargetName"), FText::FromString(LocalizationTarget->Settings.Name));
			DialogTitle = FText::Format(LOCTEXT("ImportAllTranslationsForTargetDialogTitleFormat", "Import All Translations for {TargetName} from Directory"), FormatArguments);
		}

		// Prompt the user for the directory
		FString OutputDirectory;
		if (DesktopPlatform->OpenDirectoryDialog(ParentWindowWindowHandle, DialogTitle.ToString(), DefaultPath, OutputDirectory))
		{
			LocalizationCommandletTasks::ImportTarget(ParentWindow.ToSharedRef(), LocalizationTarget.Get(), TOptional<FString>(OutputDirectory));

			UpdateTargetFromReports();
		}
	}
}

bool FLocalizationTargetDetailCustomization::CanExportAllCultures() const
{
	return LocalizationTarget.IsValid() && LocalizationTarget->Settings.SupportedCulturesStatistics.Num() > 0;
}

void FLocalizationTargetDetailCustomization::ExportAllCultures()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (LocalizationTarget.IsValid() && DesktopPlatform)
	{
		void* ParentWindowWindowHandle = NULL;
		const TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(DetailLayoutBuilder->GetDetailsView().AsShared());
		if (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
		{
			ParentWindowWindowHandle = ParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}

		const FString DefaultPath = FPaths::ConvertRelativePathToFull(LocalizationConfigurationScript::GetDataDirectory(LocalizationTarget.Get()));

		FText DialogTitle;
		{
			FFormatNamedArguments FormatArguments;
			FormatArguments.Add(TEXT("TargetName"), FText::FromString(LocalizationTarget->Settings.Name));
			DialogTitle = FText::Format(LOCTEXT("ExportAllTranslationsForTargetDialogTitleFormat", "Export All Translations for {TargetName} to Directory"), FormatArguments);
		}

		// Prompt the user for the directory
		FString OutputDirectory;
		if (DesktopPlatform->OpenDirectoryDialog(ParentWindowWindowHandle, DialogTitle.ToString(), DefaultPath, OutputDirectory))
		{
			LocalizationCommandletTasks::ExportTarget(ParentWindow.ToSharedRef(), LocalizationTarget.Get(), TOptional<FString>(OutputDirectory));
		}
	}
}

bool FLocalizationTargetDetailCustomization::CanCountWords() const
{
	return LocalizationTarget.IsValid() && LocalizationTarget->Settings.SupportedCulturesStatistics.Num() > 0;
}

void FLocalizationTargetDetailCustomization::CountWords()
{
	if (LocalizationTarget.IsValid())
	{
		const TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(DetailLayoutBuilder->GetDetailsView().AsShared());
		LocalizationCommandletTasks::GenerateWordCountReportForTarget(ParentWindow.ToSharedRef(), LocalizationTarget.Get());

		UpdateTargetFromReports();
	}
}

bool FLocalizationTargetDetailCustomization::CanCompileAllCultures() const
{
	return LocalizationTarget.IsValid() && LocalizationTarget->Settings.SupportedCulturesStatistics.Num() > 0;
}

void FLocalizationTargetDetailCustomization::CompileAllCultures()
{
	if (LocalizationTarget.IsValid())
	{
		// Execute compile.
		const TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(DetailLayoutBuilder->GetDetailsView().AsShared());
		LocalizationCommandletTasks::CompileTarget(ParentWindow.ToSharedRef(), LocalizationTarget.Get());
	}
}

void FLocalizationTargetDetailCustomization::UpdateTargetFromReports()
{
	if (LocalizationTarget.IsValid())
	{
		TArray< TSharedPtr<IPropertyHandle> > WordCountPropertyHandles;

		if (TargetSettingsPropertyHandle.IsValid() && TargetSettingsPropertyHandle->IsValidHandle())
		{
			const TSharedPtr<IPropertyHandle> SupportedCulturesStatisticsPropHandle = TargetSettingsPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLocalizationTargetSettings, SupportedCulturesStatistics));
			if (SupportedCulturesStatisticsPropHandle.IsValid() && SupportedCulturesStatisticsPropHandle->IsValidHandle())
			{
				uint32 SupportedCultureCount = 0;
				SupportedCulturesStatisticsPropHandle->GetNumChildren(SupportedCultureCount);
				for (uint32 i = 0; i < SupportedCultureCount; ++i)
				{
					const TSharedPtr<IPropertyHandle> ElementPropertyHandle = SupportedCulturesStatisticsPropHandle->GetChildHandle(i);
					if (ElementPropertyHandle.IsValid() && ElementPropertyHandle->IsValidHandle())
					{
						const TSharedPtr<IPropertyHandle> WordCountPropertyHandle = SupportedCulturesStatisticsPropHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FCultureStatistics, WordCount));
						if (WordCountPropertyHandle.IsValid() && WordCountPropertyHandle->IsValidHandle())
						{
							WordCountPropertyHandles.Add(WordCountPropertyHandle);
						}
					}
				}
			}
		}

		for (const TSharedPtr<IPropertyHandle>& WordCountPropertyHandle : WordCountPropertyHandles)
		{
			WordCountPropertyHandle->NotifyPreChange();
		}
		LocalizationTarget->UpdateWordCountsFromCSV();
		LocalizationTarget->UpdateStatusFromConflictReport();
		for (const TSharedPtr<IPropertyHandle>& WordCountPropertyHandle : WordCountPropertyHandles)
		{
			WordCountPropertyHandle->NotifyPostChange();
		}
	}
}

void FLocalizationTargetDetailCustomization::BuildListedCulturesList()
{
	const TSharedPtr<IPropertyHandleArray> SupportedCulturesStatisticsArrayPropertyHandle = SupportedCulturesStatisticsPropertyHandle->AsArray();
	if (SupportedCulturesStatisticsArrayPropertyHandle.IsValid())
	{
		uint32 ElementCount = 0;
		SupportedCulturesStatisticsArrayPropertyHandle->GetNumElements(ElementCount);
		for (uint32 i = 0; i < ElementCount; ++i)
		{
			const TSharedPtr<IPropertyHandle> CultureStatisticsProperty = SupportedCulturesStatisticsArrayPropertyHandle->GetElement(i);
			ListedCultureStatisticProperties.AddUnique(CultureStatisticsProperty);
		}
	}

	const auto& CultureSorter = [](const TSharedPtr<IPropertyHandle>& Left, const TSharedPtr<IPropertyHandle>& Right) -> bool
	{
		const TSharedPtr<IPropertyHandle> LeftNameHandle = Left->GetChildHandle(GET_MEMBER_NAME_CHECKED(FCultureStatistics, CultureName));
		const TSharedPtr<IPropertyHandle> RightNameHandle = Right->GetChildHandle(GET_MEMBER_NAME_CHECKED(FCultureStatistics, CultureName));

		FString LeftName;
		LeftNameHandle->GetValue(LeftName);
		const FCulturePtr LeftCulture = FInternationalization::Get().GetCulture(LeftName);
		FString RightName;
		RightNameHandle->GetValue(RightName);
		const FCulturePtr RightCulture = FInternationalization::Get().GetCulture(RightName);

		return LeftCulture.IsValid() && RightCulture.IsValid() ? LeftCulture->GetDisplayName() < RightCulture->GetDisplayName() : LeftName < RightName;
	};
	ListedCultureStatisticProperties.Sort(CultureSorter);

	if (NoSupportedCulturesErrorText.IsValid())
	{
		if (ListedCultureStatisticProperties.Num())
		{
			NoSupportedCulturesErrorText->SetError(FText::GetEmpty());
		}
		else
		{
			NoSupportedCulturesErrorText->SetError(LOCTEXT("NoSupportedCulturesError", "At least one supported culture must be specified."));
		}
	}
}

void FLocalizationTargetDetailCustomization::RebuildListedCulturesList()
{
	if (NewEntryIndexToBeInitialized != INDEX_NONE)
	{
		const TSharedPtr<IPropertyHandle> SupportedCultureStatisticsPropertyHandle = SupportedCulturesStatisticsPropertyHandle->GetChildHandle(NewEntryIndexToBeInitialized);

		const TSharedPtr<IPropertyHandle> CultureNamePropertyHandle = SupportedCultureStatisticsPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FCultureStatistics, CultureName));
		if(CultureNamePropertyHandle.IsValid() && CultureNamePropertyHandle->IsValidHandle())
		{
			CultureNamePropertyHandle->SetValue(SelectedNewCulture->GetName());
		}

		const TSharedPtr<IPropertyHandle> WordCountPropertyHandle = SupportedCultureStatisticsPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FCultureStatistics, WordCount));
		if(WordCountPropertyHandle.IsValid() && WordCountPropertyHandle->IsValidHandle())
		{
			WordCountPropertyHandle->SetValue(0);
		}

		AddNewSupportedCultureComboButton->SetIsOpen(false);

		NewEntryIndexToBeInitialized = INDEX_NONE;
	}

	ListedCultureStatisticProperties.Empty();
	BuildListedCulturesList();

	if (SupportedCultureListView.IsValid())
	{
		SupportedCultureListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> FLocalizationTargetDetailCustomization::OnGenerateCultureRow(TSharedPtr<IPropertyHandle> CultureStatisticsPropertyHandle, const TSharedRef<STableViewBase>& Table)
{
	return SNew(SLocalizationTargetEditorCultureRow, Table, DetailLayoutBuilder->GetPropertyUtilities(), TargetSettingsPropertyHandle.ToSharedRef(), CultureStatisticsPropertyHandle->GetIndexInArray());
}

bool FLocalizationTargetDetailCustomization::IsCultureSelectableAsSupported(FCulturePtr Culture)
{
	auto Is = [&](const TSharedPtr<IPropertyHandle>& SupportedCultureStatisticProperty)
	{
		// Can't select existing supported cultures.
		const TSharedPtr<IPropertyHandle> SupportedCultureNamePropertyHandle = SupportedCultureStatisticProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FCultureStatistics, CultureName));
		if (SupportedCultureNamePropertyHandle.IsValid() && SupportedCultureNamePropertyHandle->IsValidHandle())
		{
			FString SupportedCultureName;
			SupportedCultureNamePropertyHandle->GetValue(SupportedCultureName);
			const FCulturePtr SupportedCulture = FInternationalization::Get().GetCulture(SupportedCultureName);
			return SupportedCulture == Culture;
		}

		return false;
	};

	return !ListedCultureStatisticProperties.ContainsByPredicate(Is);
}

void FLocalizationTargetDetailCustomization::OnNewSupportedCultureSelected(FCulturePtr SelectedCulture, ESelectInfo::Type SelectInfo)
{
	if(SupportedCulturesStatisticsPropertyHandle.IsValid() && SupportedCulturesStatisticsPropertyHandle->IsValidHandle())
	{
		uint32 NewElementIndex;
		SupportedCulturesStatisticsPropertyHandle->AsArray()->GetNumElements(NewElementIndex);

		// Add element, set info for later initialization.
		SupportedCulturesStatisticsPropertyHandle->AsArray()->AddItem();
		SelectedNewCulture = SelectedCulture;
		NewEntryIndexToBeInitialized = NewElementIndex;

		if (NativeCultureIndexPropertyHandle.IsValid() && NativeCultureIndexPropertyHandle->IsValidHandle())
		{
			int32 NativeCultureIndex;
			NativeCultureIndexPropertyHandle->GetValue(NativeCultureIndex);
			if (NativeCultureIndex == INDEX_NONE)
			{
				NativeCultureIndex = NewElementIndex;
				NativeCultureIndexPropertyHandle->SetValue(NativeCultureIndex);
			}
		}

		// Refresh UI.
		if (SupportedCulturePicker.IsValid())
		{
			SupportedCulturePicker->RequestTreeRefresh();
		}
	}
}

#undef LOCTEXT_NAMESPACE