// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LocalizationDashboardPrivatePCH.h"
#include "LocalizationTargetSetDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "LocalizationTargetTypes.h"
#include "DetailWidgetRow.h"
#include "SLocalizationDashboardTargetRow.h"
#include "DetailCategoryBuilder.h"
#include "IDetailsView.h"
#include "LocalizationCommandletTasks.h"
#include "ObjectEditorUtils.h"

#define LOCTEXT_NAMESPACE "LocalizationDashboard"

FLocalizationTargetSetDetailCustomization::FLocalizationTargetSetDetailCustomization()
	: DetailLayoutBuilder(nullptr)
	//, ServiceProviderCategoryBuilder(nullptr)
	, NewEntryIndexToBeInitialized(INDEX_NONE)
{
	//TArray<ILocalizationServiceProvider*> ActualProviders = ILocalizationDashboardModule::Get().GetLocalizationServiceProviders();
	//for (ILocalizationServiceProvider* ActualProvider : ActualProviders)
	//{
	//	TSharedPtr<FLocalizationServiceProviderWrapper> Provider = MakeShareable(new FLocalizationServiceProviderWrapper(ActualProvider));
	//	Providers.Add(Provider);
	//}
}

//class FindProviderPredicate
//{
//public:
//	FindProviderPredicate(ILocalizationServiceProvider* const InActualProvider)
//		: ActualProvider(InActualProvider)
//	{
//	}

//	bool operator()(const TSharedPtr<FLocalizationServiceProviderWrapper>& Provider)
//	{
//		return Provider->Provider == ActualProvider;
//	}

//private:
//	ILocalizationServiceProvider* ActualProvider;
//};

namespace
{
	class FLocalizationDashboardCommands : public TCommands<FLocalizationDashboardCommands>
	{
	public:
		FLocalizationDashboardCommands() 
			: TCommands<FLocalizationDashboardCommands>("LocalizationDashboard", NSLOCTEXT("Contexts", "LocalizationDashboard", "Localization Dashboard"), NAME_None, FEditorStyle::GetStyleSetName())
		{
		}

		TSharedPtr<FUICommandInfo> GatherAllTargets;
		TSharedPtr<FUICommandInfo> ImportAllTargets;
		TSharedPtr<FUICommandInfo> ExportAllTargets;
		TSharedPtr<FUICommandInfo> CountWordsForAllTargets;
		TSharedPtr<FUICommandInfo> CompileAllTargets;

		/** Initialize commands */
		virtual void RegisterCommands() override;
	};

	void FLocalizationDashboardCommands::RegisterCommands() 
	{
		UI_COMMAND( GatherAllTargets, "Gather All", "Gathers text for all targets in the project.", EUserInterfaceActionType::Button, FInputChord() );
		UI_COMMAND( ImportAllTargets, "Import All", "Imports translations for all cultures of all targets in the project.", EUserInterfaceActionType::Button, FInputChord() );
		UI_COMMAND( ExportAllTargets, "Export All", "Exports translations for all cultures of all targets in the project.", EUserInterfaceActionType::Button, FInputChord() );
		UI_COMMAND( CountWordsForAllTargets, "Count Words All", "Counts translations for all targets in the project.", EUserInterfaceActionType::Button, FInputChord() );
		UI_COMMAND( CompileAllTargets, "Compile All", "Compiles translations for all targets in the project.", EUserInterfaceActionType::Button, FInputChord() );
	}
}

void FLocalizationTargetSetDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailLayoutBuilder = &DetailBuilder;
	{
		TArray< TWeakObjectPtr<UObject> > ObjectsBeingCustomized;
		DetailLayoutBuilder->GetObjectsBeingCustomized(ObjectsBeingCustomized);
		TargetSet = CastChecked<ULocalizationTargetSet>(ObjectsBeingCustomized.Top().Get());
	}

	{
		TargetObjectsPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULocalizationTargetSet,TargetObjects));
		if (TargetObjectsPropertyHandle->IsValidHandle())
		{
			const FName CategoryName = FObjectEditorUtils::GetCategoryFName(TargetObjectsPropertyHandle->GetProperty());
			IDetailCategoryBuilder& DetailCategoryBuilder = DetailBuilder.EditCategory(CategoryName);

			TargetObjectsPropertyHandle->MarkHiddenByCustomization();
			TargetsArrayPropertyHandle_OnNumElementsChanged = FSimpleDelegate::CreateSP(this, &FLocalizationTargetSetDetailCustomization::RebuildTargetsList);
			TargetObjectsPropertyHandle->AsArray()->SetOnNumElementsChanged(TargetsArrayPropertyHandle_OnNumElementsChanged);

			FLocalizationDashboardCommands::Register();
			const TSharedRef< FUICommandList > CommandList = MakeShareable(new FUICommandList);
			FToolBarBuilder ToolBarBuilder( CommandList, FMultiBoxCustomization::AllowCustomization("LocalizationDashboard") );

			TAttribute<FText> GatherAllTargetsToolTipTextAttribute = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda([this]() -> FText
				{
					return CanGatherAllTargets() ? FLocalizationDashboardCommands::Get().GatherAllTargets->GetDescription() : LOCTEXT("GatherAllTargetsDisabledToolTip", "At least one target must have a native culture specified in order to gather.");
				}));
			CommandList->MapAction( FLocalizationDashboardCommands::Get().GatherAllTargets, FExecuteAction::CreateSP(this, &FLocalizationTargetSetDetailCustomization::GatherAllTargets), FCanExecuteAction::CreateSP(this, &FLocalizationTargetSetDetailCustomization::CanGatherAllTargets));
			ToolBarBuilder.AddToolBarButton(FLocalizationDashboardCommands::Get().GatherAllTargets, NAME_None, TAttribute<FText>(), GatherAllTargetsToolTipTextAttribute, FSlateIcon(FEditorStyle::GetStyleSetName(), "LocalizationDashboard.GatherAllTargets"));

			CommandList->MapAction( FLocalizationDashboardCommands::Get().ImportAllTargets, FExecuteAction::CreateSP(this, &FLocalizationTargetSetDetailCustomization::ImportAllTargets), FCanExecuteAction::CreateSP(this, &FLocalizationTargetSetDetailCustomization::CanImportAllTargets));
			ToolBarBuilder.AddToolBarButton(FLocalizationDashboardCommands::Get().ImportAllTargets, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "LocalizationDashboard.ImportAllTargetsAllCultures"));

			CommandList->MapAction( FLocalizationDashboardCommands::Get().ExportAllTargets, FExecuteAction::CreateSP(this, &FLocalizationTargetSetDetailCustomization::ExportAllTargets), FCanExecuteAction::CreateSP(this, &FLocalizationTargetSetDetailCustomization::CanExportAllTargets));
			ToolBarBuilder.AddToolBarButton(FLocalizationDashboardCommands::Get().ExportAllTargets, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "LocalizationDashboard.ExportAllTargetsAllCultures"));

			CommandList->MapAction( FLocalizationDashboardCommands::Get().CountWordsForAllTargets, FExecuteAction::CreateSP(this, &FLocalizationTargetSetDetailCustomization::CountWordsForAllTargets), FCanExecuteAction::CreateSP(this, &FLocalizationTargetSetDetailCustomization::CanCountWordsForAllTargets));
			ToolBarBuilder.AddToolBarButton(FLocalizationDashboardCommands::Get().CountWordsForAllTargets, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "LocalizationDashboard.CountWordsForAllTargets"));

			CommandList->MapAction( FLocalizationDashboardCommands::Get().CompileAllTargets, FExecuteAction::CreateSP(this, &FLocalizationTargetSetDetailCustomization::CompileAllTargets), FCanExecuteAction::CreateSP(this, &FLocalizationTargetSetDetailCustomization::CanCompileAllTargets));
			ToolBarBuilder.AddToolBarButton(FLocalizationDashboardCommands::Get().CompileAllTargets, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "LocalizationDashboard.CompileAllTargetsAllCultures"));

			BuildTargetsList();

			DetailCategoryBuilder.AddCustomRow(TargetObjectsPropertyHandle->GetPropertyDisplayName())
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
							SAssignNew(TargetsListView, SListView< TSharedPtr<IPropertyHandle> >)
							.OnGenerateRow(this, &FLocalizationTargetSetDetailCustomization::OnGenerateRow)
							.ListItemsSource(&TargetsList)
							.SelectionMode(ESelectionMode::None)
							.HeaderRow
							(
							SNew(SHeaderRow)
							+SHeaderRow::Column("Target")
							.DefaultLabel( LOCTEXT("TargetColumnLabel", "Target"))
							.HAlignHeader(HAlign_Left)
							.HAlignCell(HAlign_Left)
							.VAlignCell(VAlign_Center)
							+SHeaderRow::Column("Status")
							.DefaultLabel( LOCTEXT("StatusColumnLabel", "Conflict Status"))
							.HAlignHeader(HAlign_Center)
							.HAlignCell(HAlign_Center)
							.VAlignCell(VAlign_Center)
							+SHeaderRow::Column("Cultures")
							.DefaultLabel( LOCTEXT("CulturesColumnLabel", "Cultures"))
							.HAlignHeader(HAlign_Fill)
							.HAlignCell(HAlign_Fill)
							.VAlignCell(VAlign_Center)
							+SHeaderRow::Column("WordCount")
							.DefaultLabel( LOCTEXT("WordCountColumnLabel", "Word Count"))
							.HAlignHeader(HAlign_Center)
							.HAlignCell(HAlign_Center)
							.VAlignCell(VAlign_Center)
							+SHeaderRow::Column("Actions")
							.DefaultLabel( LOCTEXT("ActionsColumnLabel", "Actions"))
							.HAlignHeader(HAlign_Center)
							.HAlignCell(HAlign_Center)
							.VAlignCell(VAlign_Center)
							)
						]
					+SVerticalBox::Slot()
						.AutoHeight()
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.Text(LOCTEXT("AddNewTargetButtonLabel", "Add New Target"))
							.OnClicked(this, &FLocalizationTargetSetDetailCustomization::OnNewTargetButtonClicked)
						]
				];
		}
	}
}

void FLocalizationTargetSetDetailCustomization::BuildTargetsList()
{
	const TSharedPtr<IPropertyHandleArray> TargetObjectsArrayPropertyHandle = TargetObjectsPropertyHandle->AsArray();
	if (TargetObjectsArrayPropertyHandle.IsValid())
	{
		uint32 ElementCount = 0;
		TargetObjectsArrayPropertyHandle->GetNumElements(ElementCount);
		for (uint32 i = 0; i < ElementCount; ++i)
		{
			const TSharedPtr<IPropertyHandle> TargetObjectPropertyHandle = TargetObjectsArrayPropertyHandle->GetElement(i);
			TargetsList.Add(TargetObjectPropertyHandle);
		}
	}
}

void FLocalizationTargetSetDetailCustomization::RebuildTargetsList()
{
	const TSharedPtr<IPropertyHandleArray> TargetObjectsArrayPropertyHandle = TargetObjectsPropertyHandle->AsArray();
	if (TargetObjectsArrayPropertyHandle.IsValid() && NewEntryIndexToBeInitialized != INDEX_NONE)
	{
		const TSharedPtr<IPropertyHandle> TargetObjectPropertyHandle = TargetObjectsArrayPropertyHandle->GetElement(NewEntryIndexToBeInitialized);
		if (TargetObjectPropertyHandle.IsValid() && TargetObjectPropertyHandle->IsValidHandle())
		{
			ULocalizationTarget* const NewTarget = NewObject<ULocalizationTarget>(TargetSet.Get());

			TArray<void*> RawData;
			TargetObjectsPropertyHandle->AccessRawData(RawData);
			void* RawDatum = RawData.Top();
			TArray<ULocalizationTarget*>& TargetObjectsArray = *(reinterpret_cast< TArray<ULocalizationTarget*>* >(RawDatum));
			FString NewTargetName = "NewTarget";
			auto TargetNameIsIdentical = [&](ULocalizationTarget* Target) -> bool
			{
				return (Target != NewTarget) && Target && (Target->Settings.Name == NewTargetName);
			};

			for (uint32 i = 0; TargetObjectsArray.ContainsByPredicate(TargetNameIsIdentical); ++i)
			{
				NewTargetName = FString::Printf(TEXT("NewTarget%u"), i);
			}

			NewTarget->Settings.Name = NewTargetName;
			const int32 NativeCultureIndex = NewTarget->Settings.SupportedCulturesStatistics.Add( FCultureStatistics(FInternationalization::Get().GetCurrentCulture()->GetName()) );
			NewTarget->Settings.NativeCultureIndex = NativeCultureIndex;

			const UObject* SetValue = NewTarget;
			TargetObjectPropertyHandle->SetValue(SetValue);

			NewEntryIndexToBeInitialized = INDEX_NONE;
		}
	}

	TargetsList.Empty();
	BuildTargetsList();

	if (TargetsListView.IsValid())
	{
		TargetsListView->RequestListRefresh();
	}
}

//FText FLocalizationTargetSetDetailCustomization::GetCurrentServiceProviderDisplayName() const
//{
//	ILocalizationServiceProvider* const LSP = ILocalizationDashboardModule::Get().GetCurrentLocalizationServiceProvider();
//	return LSP ? LSP->GetDisplayName() : LOCTEXT("NoServiceProviderName", "None");
//}
//
//TSharedRef<SWidget> FLocalizationTargetSetDetailCustomization::ServiceProviderComboBox_OnGenerateWidget(TSharedPtr<FLocalizationServiceProviderWrapper> LSPWrapper) const
//{
//	ILocalizationServiceProvider* const LSP = LSPWrapper->Provider;
//	return	SNew(STextBlock)
//		.Text(LSP ? LSP->GetDisplayName() : LOCTEXT("NoServiceProviderName", "None"));
//}
//
//void FLocalizationTargetSetDetailCustomization::ServiceProviderComboBox_OnSelectionChanged(TSharedPtr<FLocalizationServiceProviderWrapper> LSPWrapper, ESelectInfo::Type SelectInfo)
//{
//	ILocalizationServiceProvider* const LSP = LSPWrapper->Provider;
//	FString ServiceProviderName = LSP ? LSP->GetName().ToString() : TEXT("None");
//	ServiceProviderPropertyHandle->SetValue(ServiceProviderName);
//
//	if (LSP)
//	{
//		LSP->CustomizeSettingsDetails(*ServiceProviderCategoryBuilder);
//	}
//	DetailLayoutBuilder->ForceRefreshDetails();
//}

bool FLocalizationTargetSetDetailCustomization::CanGatherAllTargets() const
{
	for (ULocalizationTarget* const LocalizationTarget : TargetSet->TargetObjects)
	{
		// If any target can be gathered, then gathering all can be executed.
		if (LocalizationTarget && LocalizationTarget->Settings.SupportedCulturesStatistics.Num() > 0 && LocalizationTarget->Settings.SupportedCulturesStatistics.IsValidIndex(LocalizationTarget->Settings.NativeCultureIndex))
		{
			return true;
		}
	}

	return false;
}

void FLocalizationTargetSetDetailCustomization::GatherAllTargets()
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

	TArray<ULocalizationTarget*> TargetObjectsToProcess;
	for (ULocalizationTarget* const LocalizationTarget : TargetSet->TargetObjects)
	{
		if (LocalizationTarget && LocalizationTarget->Settings.SupportedCulturesStatistics.Num() > 0 && LocalizationTarget->Settings.SupportedCulturesStatistics.IsValidIndex(LocalizationTarget->Settings.NativeCultureIndex))
		{
			TargetObjectsToProcess.Add(LocalizationTarget);
		}
	}

	// Execute gather.
	const TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(DetailLayoutBuilder->GetDetailsView().AsShared());
	LocalizationCommandletTasks::GatherTargets(ParentWindow.ToSharedRef(), TargetObjectsToProcess);

	for (ULocalizationTarget* const LocalizationTarget : TargetObjectsToProcess)
	{
		UpdateTargetFromReports(LocalizationTarget);
	}
}

bool FLocalizationTargetSetDetailCustomization::CanImportAllTargets() const
{
	for (ULocalizationTarget* const LocalizationTarget : TargetSet->TargetObjects)
	{
		// If any target can be imported, then importing all can be executed.
		if (LocalizationTarget && LocalizationTarget->Settings.SupportedCulturesStatistics.Num() > 0)
		{
			return true;
		}
	}

	return false;
}

void FLocalizationTargetSetDetailCustomization::ImportAllTargets()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		void* ParentWindowWindowHandle = NULL;
		const TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(DetailLayoutBuilder->GetDetailsView().AsShared());
		if (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
		{
			ParentWindowWindowHandle = ParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}


		const FString DefaultPath = FPaths::ConvertRelativePathToFull(FPaths::GameContentDir() / TEXT("Localization"));

		// Prompt the user for the directory
		FString OutputDirectory;
		if (DesktopPlatform->OpenDirectoryDialog(ParentWindowWindowHandle, LOCTEXT("ImportAllTranslationsDialogTitle", "Import All Translations from Directory").ToString(), DefaultPath, OutputDirectory))
		{
			TArray<ULocalizationTarget*> TargetObjectsToProcess;
			for (ULocalizationTarget* const LocalizationTarget : TargetSet->TargetObjects)
			{
				if (LocalizationTarget && LocalizationTarget->Settings.SupportedCulturesStatistics.Num() > 0)
				{
					TargetObjectsToProcess.Add(LocalizationTarget);
				}
			}

			LocalizationCommandletTasks::ImportTargets(ParentWindow.ToSharedRef(), TargetObjectsToProcess, TOptional<FString>(OutputDirectory));

			for (ULocalizationTarget* const LocalizationTarget : TargetObjectsToProcess)
			{
				UpdateTargetFromReports(LocalizationTarget);
			}
		}
	}
}

bool FLocalizationTargetSetDetailCustomization::CanExportAllTargets() const
{
	for (ULocalizationTarget* const LocalizationTarget : TargetSet->TargetObjects)
	{
		// If any target can be imported, then exporting all can be executed.
		if (LocalizationTarget && LocalizationTarget->Settings.SupportedCulturesStatistics.Num() > 0)
		{
			return true;
		}
	}

	return false;
}

void FLocalizationTargetSetDetailCustomization::ExportAllTargets()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		void* ParentWindowWindowHandle = NULL;
		const TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(DetailLayoutBuilder->GetDetailsView().AsShared());
		if (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
		{
			ParentWindowWindowHandle = ParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}

		const FString DefaultPath = FPaths::ConvertRelativePathToFull(FPaths::GameContentDir() / TEXT("Localization"));

		// Prompt the user for the directory
		FString OutputDirectory;
		if (DesktopPlatform->OpenDirectoryDialog(ParentWindowWindowHandle, LOCTEXT("ExportAllTranslationsDialogTitle", "Export All Translations to Directory").ToString(), DefaultPath, OutputDirectory))
		{
			TArray<ULocalizationTarget*> TargetObjectsToProcess;
			for (ULocalizationTarget* const LocalizationTarget : TargetSet->TargetObjects)
			{
				if (LocalizationTarget && LocalizationTarget->Settings.SupportedCulturesStatistics.Num() > 0)
				{
					TargetObjectsToProcess.Add(LocalizationTarget);
				}
			}

			LocalizationCommandletTasks::ExportTargets(ParentWindow.ToSharedRef(), TargetObjectsToProcess, TOptional<FString>(OutputDirectory));
		}
	}
}

bool FLocalizationTargetSetDetailCustomization::CanCountWordsForAllTargets() const
{
	for (ULocalizationTarget* const LocalizationTarget : TargetSet->TargetObjects)
	{
		// If any target can be imported, then counting words for all can be executed.
		if (LocalizationTarget && LocalizationTarget->Settings.SupportedCulturesStatistics.Num() > 0)
		{
			return true;
		}
	}

	return false;
}

void FLocalizationTargetSetDetailCustomization::CountWordsForAllTargets()
{
	TArray<ULocalizationTarget*> TargetObjectsToProcess;
	for (ULocalizationTarget* const LocalizationTarget : TargetSet->TargetObjects)
	{
		if (LocalizationTarget && LocalizationTarget->Settings.SupportedCulturesStatistics.Num() > 0)
		{
			TargetObjectsToProcess.Add(LocalizationTarget);
		}
	}

	// Execute compile.
	const TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(DetailLayoutBuilder->GetDetailsView().AsShared());
	LocalizationCommandletTasks::GenerateWordCountReportsForTargets(ParentWindow.ToSharedRef(), TargetObjectsToProcess);
}

bool FLocalizationTargetSetDetailCustomization::CanCompileAllTargets() const
{
	for (ULocalizationTarget* const LocalizationTarget : TargetSet->TargetObjects)
	{
		// If any target can be imported, then compiling all can be executed.
		if (LocalizationTarget && LocalizationTarget->Settings.SupportedCulturesStatistics.Num() > 0)
		{
			return true;
		}
	}

	return false;
}

void FLocalizationTargetSetDetailCustomization::CompileAllTargets()
{
	TArray<ULocalizationTarget*> TargetObjectsToProcess;
	for (ULocalizationTarget* const LocalizationTarget : TargetSet->TargetObjects)
	{
		if (LocalizationTarget && LocalizationTarget->Settings.SupportedCulturesStatistics.Num() > 0)
		{
			TargetObjectsToProcess.Add(LocalizationTarget);
		}
	}

	// Execute compile.
	const TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(DetailLayoutBuilder->GetDetailsView().AsShared());
	LocalizationCommandletTasks::CompileTargets(ParentWindow.ToSharedRef(), TargetObjectsToProcess);
}

void FLocalizationTargetSetDetailCustomization::UpdateTargetFromReports(ULocalizationTarget* const LocalizationTarget)
{
	//TArray< TSharedPtr<IPropertyHandle> > WordCountPropertyHandles;

	//const TSharedPtr<IPropertyHandle> TargetSettingsPropertyHandle = TargetEditor->GetTargetSettingsPropertyHandle();
	//if (TargetSettingsPropertyHandle.IsValid() && TargetSettingsPropertyHandle->IsValidHandle())
	//{
	//	const TSharedPtr<IPropertyHandle> NativeCultureStatisticsPropertyHandle = TargetSettingsPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLocalizationTargetSettings, NativeCultureStatistics));
	//	if (NativeCultureStatisticsPropertyHandle.IsValid() && NativeCultureStatisticsPropertyHandle->IsValidHandle())
	//	{
	//		const TSharedPtr<IPropertyHandle> WordCountPropertyHandle = NativeCultureStatisticsPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FCultureStatistics, WordCount));
	//		if (WordCountPropertyHandle.IsValid() && WordCountPropertyHandle->IsValidHandle())
	//		{
	//			WordCountPropertyHandles.Add(WordCountPropertyHandle);
	//		}
	//	}

	//	const TSharedPtr<IPropertyHandle> SupportedCulturesStatisticsPropertyHandle = TargetSettingsPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLocalizationTargetSettings, SupportedCulturesStatistics));
	//	if (SupportedCulturesStatisticsPropertyHandle.IsValid() && SupportedCulturesStatisticsPropertyHandle->IsValidHandle())
	//	{
	//		uint32 SupportedCultureCount = 0;
	//		SupportedCulturesStatisticsPropertyHandle->GetNumChildren(SupportedCultureCount);
	//		for (uint32 i = 0; i < SupportedCultureCount; ++i)
	//		{
	//			const TSharedPtr<IPropertyHandle> ElementPropertyHandle = SupportedCulturesStatisticsPropertyHandle->GetChildHandle(i);
	//			if (ElementPropertyHandle.IsValid() && ElementPropertyHandle->IsValidHandle())
	//			{
	//				const TSharedPtr<IPropertyHandle> WordCountPropertyHandle = SupportedCulturesStatisticsPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FCultureStatistics, WordCount));
	//				if (WordCountPropertyHandle.IsValid() && WordCountPropertyHandle->IsValidHandle())
	//				{
	//					WordCountPropertyHandles.Add(WordCountPropertyHandle);
	//				}
	//			}
	//		}
	//	}
	//}

	//for (const TSharedPtr<IPropertyHandle>& WordCountPropertyHandle : WordCountPropertyHandles)
	//{
	//	WordCountPropertyHandle->NotifyPreChange();
	//}
	LocalizationTarget->UpdateWordCountsFromCSV();
	LocalizationTarget->UpdateStatusFromConflictReport();
	//for (const TSharedPtr<IPropertyHandle>& WordCountPropertyHandle : WordCountPropertyHandles)
	//{
	//	WordCountPropertyHandle->NotifyPostChange();
	//}
}

TSharedRef<ITableRow> FLocalizationTargetSetDetailCustomization::OnGenerateRow(TSharedPtr<IPropertyHandle> TargetObjectPropertyHandle, const TSharedRef<STableViewBase>& Table)
{
	return SNew(SLocalizationDashboardTargetRow, Table, DetailLayoutBuilder->GetPropertyUtilities(), TargetObjectPropertyHandle.ToSharedRef());
}

FReply FLocalizationTargetSetDetailCustomization::OnNewTargetButtonClicked()
{
	if(TargetObjectsPropertyHandle.IsValid() && TargetObjectsPropertyHandle->IsValidHandle())
	{
		uint32 NewElementIndex;
		TargetObjectsPropertyHandle->AsArray()->GetNumElements(NewElementIndex);
		TargetObjectsPropertyHandle->AsArray()->AddItem();
		NewEntryIndexToBeInitialized = NewElementIndex;
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE