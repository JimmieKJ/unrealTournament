// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "GraphEditorCommon.h"
#include "SGraphPinClass.h"
#include "Editor/ClassViewer/Public/ClassViewerModule.h"
#include "Editor/ClassViewer/Public/ClassViewerFilter.h"
#include "Editor/UnrealEd/Public/ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "SGraphPinClass"

/////////////////////////////////////////////////////
// SGraphPinClass

void SGraphPinClass::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

FReply SGraphPinClass::OnClickUse()
{
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

	if(GraphPinObj && GraphPinObj->GetSchema())
	{
		const UClass* PinRequiredParentClass = Cast<const UClass>(GraphPinObj->PinType.PinSubCategoryObject.Get());
		ensure(PinRequiredParentClass);

		const UClass* SelectedClass = GEditor->GetFirstSelectedClass(PinRequiredParentClass);
		if(SelectedClass)
		{
			GraphPinObj->GetSchema()->TrySetDefaultObject(*GraphPinObj, const_cast<UClass*>(SelectedClass));
		}
	}

	return FReply::Handled();
}

class FGraphPinFilter : public IClassViewerFilter
{
public:
	/** All children of these classes will be included unless filtered out by another setting. */
	TSet< const UClass* > AllowedChildrenOfClasses;

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs ) override
	{
		// If it appears on the allowed child-of classes list (or there is nothing on that list)
		return (InFilterFuncs->IfInChildOfClassesSet( AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed);
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		return (InFilterFuncs->IfInChildOfClassesSet( AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed);
	}
};

TSharedRef<SWidget> SGraphPinClass::GenerateAssetPicker()
{
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.bShowNoneOption = true;

	// Get the min. spec for the classes allowed
	const UClass* PinRequiredParentClass = Cast<const UClass>(GraphPinObj->PinType.PinSubCategoryObject.Get());
	ensure(PinRequiredParentClass);
	if (PinRequiredParentClass == NULL)
	{
		PinRequiredParentClass = UObject::StaticClass();
	}

	TSharedPtr<FGraphPinFilter> Filter = MakeShareable(new FGraphPinFilter);
	Options.ClassFilter = Filter;

	Filter->AllowedChildrenOfClasses.Add(PinRequiredParentClass);

	return
		SNew(SBox)
		.WidthOverride(280)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.MaxHeight(500)
			[ 
				SNew(SBorder)
				.Padding(4)
				.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
				[
					ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &SGraphPinClass::OnPickedNewClass))
				]
			]			
		];
}

FOnClicked SGraphPinClass::GetOnUseButtonDelegate()
{
	return FOnClicked::CreateSP( this, &SGraphPinClass::OnClickUse );
}

void SGraphPinClass::OnPickedNewClass(UClass* ChosenClass)
{
	if(GraphPinObj->DefaultObject != ChosenClass)
	{
		const FScopedTransaction Transaction( NSLOCTEXT("GraphEditor", "ChangeClassPinValue", "Change Class Pin Value" ) );
		GraphPinObj->Modify();

		AssetPickerAnchor->SetIsOpen(false);
		GraphPinObj->GetSchema()->TrySetDefaultObject(*GraphPinObj, ChosenClass);
	}
}

FText SGraphPinClass::GetDefaultComboText() const
{ 
	return LOCTEXT( "DefaultComboText", "Select Class" );
}

#undef LOCTEXT_NAMESPACE
