// Copyright 2015 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FExcelImportUtil;

/**
 * Customizes an FExcelImportUtil property
 */
class FExcelImportUtilCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() 
	{
		return MakeShareable( new FExcelImportUtilCustomization );
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;

private:
	FReply OnImportFromInstance();
	FReply OnImportFromFile();
	FReply OnReimport();

	void Error(const FText& Message) const;

	FExcelImportUtil* GetValue();

private:
	TSharedPtr<class IPropertyHandle> PropertyHandle;
};
