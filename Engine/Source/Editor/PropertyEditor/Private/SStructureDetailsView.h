// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SDetailsViewBase.h"
#include "IStructureDetailsView.h"

class SStructureDetailsView : public SDetailsViewBase, public IStructureDetailsView
{
	TSharedPtr<class FStructOnScope> StructData;
	TSharedPtr<class FStructurePropertyNode> RootNode;
	FText CustomName;
public:

	SLATE_BEGIN_ARGS(SStructureDetailsView)
		: _DetailsViewArgs()
	{ }
		/** User defined arguments for the details view */
		SLATE_ARGUMENT(FDetailsViewArgs, DetailsViewArgs)

		/** Custom name for the root property node. */
		SLATE_ARGUMENT(FText, CustomName)
	SLATE_END_ARGS()

	/** Destructor. */
	~SStructureDetailsView();

	/** Constructs the property view widgets. */
	void Construct(const FArguments& InArgs);

	UStruct* GetBaseScriptStruct() const;

	virtual bool IsConnected() const override;

	virtual bool DontUpdateValueWhileEditing() const override
	{
		return true;
	}

public:

	// IStructureDetailsView interface

	virtual IDetailsView& GetDetailsView() override
	{
		return *this;
	}

	virtual TSharedPtr<SWidget> GetWidget() override
	{
		return AsShared();
	}

	virtual void SetStructureData(TSharedPtr<FStructOnScope> InStructData) override;

	virtual FOnFinishedChangingProperties& GetOnFinishedChangingPropertiesDelegate() override
	{
		return OnFinishedChangingProperties();
	}

public:

	// IDetailsViewPrivate interface

	virtual const UClass* GetBaseClass() const override
	{
		return NULL;
	}

	virtual UClass* GetBaseClass() override
	{
		return NULL;
	}

	virtual bool IsCategoryHiddenByClass(FName CategoryName) const override
	{
		return false;
	}

	virtual void ForceRefresh() override;
	virtual void AddExternalRootPropertyNode(TSharedRef<FPropertyNode> ExternalRootNode) override {}

public:

	// IDetailsView interface

	virtual UStruct* GetBaseStruct() const override
	{
		return GetBaseScriptStruct();
	}

	virtual const TArray< TWeakObjectPtr<UObject> >& GetSelectedObjects() const override;
	virtual const TArray< TWeakObjectPtr<AActor> >& GetSelectedActors() const override;
	virtual const struct FSelectedActorInfo& GetSelectedActorInfo() const override;

	virtual bool HasClassDefaultObject() const override
	{
		return false;
	}

	virtual void SetOnObjectArrayChanged(FOnObjectArrayChanged OnObjectArrayChangedDelegate) override {}
	virtual void RegisterInstancedCustomPropertyLayout(UStruct* Class, FOnGetDetailCustomizationInstance DetailLayoutDelegate) override;
	virtual void UnregisterInstancedCustomPropertyLayout(UStruct* Class) override;
	virtual void SetObjects(const TArray<UObject*>& InObjects, bool bForceRefresh = false, bool bOverrideLock = false) override {}
	virtual void SetObjects(const TArray< TWeakObjectPtr< UObject > >& InObjects, bool bForceRefresh = false, bool bOverrideLock = false) override {}
	virtual void SetObject(UObject* InObject, bool bForceRefresh = false) override{}
	virtual void RemoveInvalidObjects() override {}

	virtual TSharedPtr<class FComplexPropertyNode> GetRootNode() override;

protected:

	virtual void CustomUpdatePropertyMap() override;

	EVisibility GetPropertyEditingVisibility() const;
};
