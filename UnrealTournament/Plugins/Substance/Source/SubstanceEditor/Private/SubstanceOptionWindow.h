//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "SlateBasics.h"
#include "AssetRegistryModule.h"
#include "SubstanceImportOptionsUi.h"

namespace Substance
{
	static FString InstancePath;
	static FString MaterialPath;
}

class SSubstanceOptionWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SSubstanceOptionWindow )
		: _ImportUI(NULL)
		, _WidgetWindow()
		{}

		SLATE_ARGUMENT( USubstanceImportOptionsUi*, ImportUI )
		SLATE_ARGUMENT( TSharedPtr<SWindow>, WidgetWindow )
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);

	FReply OnImport()
	{
		bCanImport = true;
		if ( WidgetWindow.IsValid() )
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}

		ImportUI->InstanceName = InstanceNameWidget.Pin()->GetText().ToString();
		ImportUI->MaterialName = MaterialNameWidget.Pin()->GetText().ToString();

		return FReply::Handled();
	}

	FReply OnCancel()
	{
		bCanImport = false;
		if ( WidgetWindow.IsValid() )
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	bool ShouldImport()
	{
		return bCanImport;
	}

	SSubstanceOptionWindow() 
		: ImportUI(NULL)
		, bCanImport(false)
	{}

	void InstancePathSelected(const FString& InPathName)
	{
		ImportUI->InstanceDestinationPath = InPathName;
	}

	void MaterialPathSelected(const FString& InPathName)
	{
		ImportUI->MaterialDestinationPath = InPathName;
	}

private:
	bool bIsReportingError;

	USubstanceImportOptionsUi* ImportUI;
	bool			bCanImport;
	bool			bReimport;

	FString			ErrorMessage;
	SVerticalBox::FSlot* CustomBox;

	TWeakPtr< SWindow > WidgetWindow;
	TSharedPtr< SButton > ImportButton;

	TSharedPtr< SWidget > InstancePathPicker;
	TSharedPtr< SWidget > MaterialPathPicker;

	/** Instance Name textbox widget */
	TWeakPtr< SEditableTextBox > InstanceNameWidget;

	/** Material Name textbox widget */
	TWeakPtr< SEditableTextBox > MaterialNameWidget;

	void OnInstanceNameChanged(const FText& InNewName);

	void OnMaterialNameChanged(const FText& InNewName);

	void OnCreateInstanceChanged(ECheckBoxState InNewValue)
	{
		check(ImportUI);
		if (ImportUI->bForceCreateInstance)
		{
			return;
		}

		ImportUI->bCreateInstance = InNewValue == ESlateCheckBoxState::Checked ? true : false;
	}

	ECheckBoxState GetCreateInstance() const
	{
		if (ImportUI->bForceCreateInstance)
		{
			return ESlateCheckBoxState::Checked;
		}

		return ImportUI->bCreateInstance ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
	}

	ECheckBoxState GetOverrideInstancePath() const
	{
		check(ImportUI);
		return ImportUI->bOverrideInstancePath ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
	}

	void OnOverrideInstancePathChanged(ECheckBoxState InNewValue)
	{
		check(ImportUI);
		ImportUI->bOverrideInstancePath = InNewValue == ESlateCheckBoxState::Checked ? true : false;
	}

	void OnInstancePathCommitted( const FText& NewValue, ETextCommit::Type CommitInfo )
	{
		check(ImportUI);
		ImportUI->InstanceDestinationPath = NewValue.ToString();
	}

	bool ShowInstancePathPicker() const
	{
		check(ImportUI);
		return ImportUI->bOverrideInstancePath;
	}

	bool CreateInstance() const
	{
		return ImportUI->bCreateInstance ? true : false;
	}

	bool CreateMaterial() const
	{
		if (ImportUI->bCreateInstance)
		{
			return ImportUI->bCreateMaterial ? true : false;
		}
		
		return false;
	}

	ECheckBoxState GetCreateMaterial() const
	{
		return ImportUI->bCreateMaterial ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
	}

	void OnCreateMaterialChanged(ECheckBoxState InNewValue) const
	{
		check(ImportUI);
		ImportUI->bCreateMaterial = InNewValue == ESlateCheckBoxState::Checked ? true : false;
	}

	ECheckBoxState GetOverrideMaterialPath() const
	{
		check(ImportUI);
		return ImportUI->bOverrideMaterialPath ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
	}

	void OnOverrideMaterialPathChanged(ECheckBoxState InNewValue) const
	{
		check(ImportUI);
		ImportUI->bOverrideMaterialPath = InNewValue == ESlateCheckBoxState::Checked ? true : false;
	}

	void OnMaterialPathCommitted( const FText& NewValue, ETextCommit::Type CommitInfo ) const
	{
		check(ImportUI);
		ImportUI->MaterialDestinationPath = NewValue.ToString();
	}

	bool ShowMaterialPathPicker() const
	{
		check(ImportUI);
		return ImportUI->bOverrideMaterialPath;
	}
};

