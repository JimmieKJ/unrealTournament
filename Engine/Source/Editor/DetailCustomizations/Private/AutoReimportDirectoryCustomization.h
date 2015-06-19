// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements a details view customization for the FAutoReimportDirectoryConfig struct.
 */
class FAutoReimportWildcardCustomization : public IPropertyTypeCustomization
{
public:

	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InStructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& InStructBuilder, IPropertyTypeCustomizationUtils& InStructCustomizationUtils) override;

public:

	/** Creates an instance of this class. */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FAutoReimportWildcardCustomization());
	}

private:

	FText GetWildcardText() const;
	void OnWildcardCommitted(const FText& InValue, ETextCommit::Type CommitType);
	void OnWildcardChanged(const FText& InValue);

	void OnCheckStateChanged(ECheckBoxState InState);
	ECheckBoxState GetCheckState() const;
	
	TSharedPtr<IPropertyHandle> PropertyHandle;
	TSharedPtr<IPropertyHandle> WildcardProperty;
	TSharedPtr<IPropertyHandle> IncludeProperty;
};

/**
 * Implements a details view customization for the FAutoReimportDirectoryConfig struct.
 */
class FAutoReimportDirectoryCustomization : public IPropertyTypeCustomization
{
public:

	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InStructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& InStructBuilder, IPropertyTypeCustomizationUtils& InStructCustomizationUtils) override;

public:

	/** Creates an instance of this class. */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FAutoReimportDirectoryCustomization());
	}

private:


	EVisibility GetMountPathVisibility() const;

	FText GetDirectoryText() const;
	void OnDirectoryCommitted(const FText& InValue, ETextCommit::Type CommitType);
	void OnDirectoryChanged(const FText& InValue);

	FText GetMountPointText() const;
	void OnMountPointCommitted(const FText& InValue, ETextCommit::Type CommitType);
	void OnMountPointChanged(const FText& InValue);

	void SetSourcePath(FString InSourceDir);
	void UpdateMountPath();
	FReply BrowseForFolder();

	EVisibility MountPathVisibility;
	TSharedPtr<IPropertyHandle> PropertyHandle;
	TSharedPtr<IPropertyHandle> SourceDirProperty;
	TSharedPtr<IPropertyHandle> MountPointProperty;
	TSharedPtr<IPropertyHandle> WildcardsProperty;
};
