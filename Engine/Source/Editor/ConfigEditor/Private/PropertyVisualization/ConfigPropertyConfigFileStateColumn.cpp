// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ConfigEditorPCH.h"
#include "ConfigPropertyConfigFileStateColumn.h"

#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/PropertyHandle.h"
#include "Editor/PropertyEditor/Public/PropertyPath.h"
#include "Editor/PropertyEditor/Public/IPropertyTable.h"


#define LOCTEXT_NAMESPACE "ConfigEditor"


FConfigPropertyConfigFileStateCellPresenter::FConfigPropertyConfigFileStateCellPresenter(const TSharedPtr< IPropertyHandle > PropertyHandle)
{
	//		Text = GetPropertyAsText(PropertyHandle);
}


TSharedRef< class SWidget > FConfigPropertyConfigFileStateCellPresenter::ConstructDisplayWidget()
{
	return
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SCC unconnected","SCC"))
		];
}


bool FConfigPropertyConfigFileStateCellPresenter::RequiresDropDown()
{
	return false;
}


TSharedRef< class SWidget > FConfigPropertyConfigFileStateCellPresenter::ConstructEditModeCellWidget()
{
	return ConstructDisplayWidget();
}


TSharedRef< class SWidget > FConfigPropertyConfigFileStateCellPresenter::ConstructEditModeDropDownWidget()
{
	return SNullWidget::NullWidget;
}


TSharedRef< class SWidget > FConfigPropertyConfigFileStateCellPresenter::WidgetToFocusOnEdit()
{
	return SNullWidget::NullWidget;
}


bool FConfigPropertyConfigFileStateCellPresenter::HasReadOnlyEditMode()
{
	return true;
}


FString FConfigPropertyConfigFileStateCellPresenter::GetValueAsString()
{
	return Text.ToString();
}


FText FConfigPropertyConfigFileStateCellPresenter::GetValueAsText()
{
	return Text;
}


bool FConfigPropertyConfigFileStateCustomColumn::Supports(const TSharedRef< IPropertyTableColumn >& Column, const TSharedRef< IPropertyTableUtilities >& Utilities) const
{
	bool IsSupported = false;

	if (Column->GetDataSource()->IsValid())
	{
		TSharedPtr< FPropertyPath > PropertyPath = Column->GetDataSource()->AsPropertyPath();
		if (PropertyPath.IsValid() && PropertyPath->GetNumProperties() > 0)
		{
			const FPropertyInfo& PropertyInfo = PropertyPath->GetRootProperty();
			UProperty* Property = PropertyInfo.Property.Get();
			if (SupportedProperty == Property)
			{
				IsSupported = true;
			}
		}
	}

	return IsSupported;
}

TSharedPtr< SWidget > FConfigPropertyConfigFileStateCustomColumn::CreateColumnLabel(const TSharedRef< IPropertyTableColumn >& Column, const TSharedRef< IPropertyTableUtilities >& Utilities, const FName& Style) const
{
	return NULL;
}

TSharedPtr< IPropertyTableCellPresenter > FConfigPropertyConfigFileStateCustomColumn::CreateCellPresenter(const TSharedRef< IPropertyTableCell >& Cell, const TSharedRef< IPropertyTableUtilities >& Utilities, const FName& Style) const
{
	TSharedPtr< IPropertyHandle > PropertyHandle = Cell->GetPropertyHandle();
	if (PropertyHandle.IsValid())
	{
		TSharedRef<FPropertyPath> Path =  FPropertyPath::Create(PropertyHandle->GetProperty());
		return MakeShareable(new FConfigPropertyConfigFileStateCellPresenter(PropertyHandle));
	}

	return NULL;
	/*
	SNew(SPropertyTableCell, Cell)
		.Presenter(CellPresenter)
		.Style(Style);
	*/
}


#undef LOCTEXT_NAMESPACE