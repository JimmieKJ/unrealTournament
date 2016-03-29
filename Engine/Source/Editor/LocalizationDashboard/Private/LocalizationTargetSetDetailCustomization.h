// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "ILocalizationServiceProvider.h"

class ULocalizationTarget;
class IPropertyHandle;
class ULocalizationTargetSet;
class IDetailCategoryBuilder;

struct FLocalizationServiceProviderWrapper
{
	FLocalizationServiceProviderWrapper() : Provider(nullptr) {}
	FLocalizationServiceProviderWrapper(ILocalizationServiceProvider* const InProvider) : Provider(InProvider) {}

	ILocalizationServiceProvider* Provider;
};

class FLocalizationTargetSetDetailCustomization : public IDetailCustomization
{
public:
	FLocalizationTargetSetDetailCustomization();
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	void BuildTargetsList();
	void RebuildTargetsList();

	FText GetCurrentServiceProviderDisplayName() const;
	TSharedRef<SWidget> ServiceProviderComboBox_OnGenerateWidget(TSharedPtr<FLocalizationServiceProviderWrapper> LSPWrapper) const;
	void ServiceProviderComboBox_OnSelectionChanged(TSharedPtr<FLocalizationServiceProviderWrapper> LSPWrapper, ESelectInfo::Type SelectInfo);

	bool CanGatherTextAllTargets() const;
	void GatherTextAllTargets();

	bool CanImportTextAllTargets() const;
	void ImportTextAllTargets();

	bool CanExportTextAllTargets() const;
	void ExportTextAllTargets();

	bool CanImportDialogueScriptAllTargets() const;
	void ImportDialogueScriptAllTargets();

	bool CanExportDialogueScriptAllTargets() const;
	void ExportDialogueScriptAllTargets();

	bool CanImportDialogueAllTargets() const;
	void ImportDialogueAllTargets();

	bool CanCountWordsForAllTargets() const;
	void CountWordsForAllTargets();

	bool CanCompileTextAllTargets() const;
	void CompileTextAllTargets();

	void UpdateTargetFromReports(ULocalizationTarget* const LocalizationTarget);

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<IPropertyHandle> TargetObjectPropertyHandle, const TSharedRef<STableViewBase>& Table);
	FReply OnNewTargetButtonClicked();

private:
	IDetailLayoutBuilder* DetailLayoutBuilder;

	TWeakObjectPtr<ULocalizationTargetSet> TargetSet;

	IDetailCategoryBuilder* ServiceProviderCategoryBuilder;
	TArray< TSharedPtr<FLocalizationServiceProviderWrapper> > Providers;

	TSharedPtr<IPropertyHandle> TargetObjectsPropertyHandle;
	FSimpleDelegate TargetsArrayPropertyHandle_OnNumElementsChanged;
	TArray< TSharedPtr<IPropertyHandle> > TargetsList;
	TSharedPtr< SListView< TSharedPtr<IPropertyHandle> > > TargetsListView;

	/* If set, the entry at the index specified needs to be initialized as soon as possible. */
	int32 NewEntryIndexToBeInitialized;
};