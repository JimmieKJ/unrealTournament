// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneEventParametersCustomization.h"

#include "UObject/UnrealType.h"
#include "Sections/MovieSceneEventSection.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "PropertyHandle.h"
#include "PropertyCustomizationHelpers.h"
#include "ContentBrowserModule.h"
#include "IPropertyUtilities.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "Serialization/MemoryWriter.h"

#define LOCTEXT_NAMESPACE "MovieSceneEventParameters"

TSharedRef<IPropertyTypeCustomization> FMovieSceneEventParametersCustomization::MakeInstance()
{
	return MakeShared<FMovieSceneEventParametersCustomization>();
}

void FMovieSceneEventParametersCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyUtilities = CustomizationUtils.GetPropertyUtilities();
}

void FMovieSceneEventParametersCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyHandle = InPropertyHandle;

	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);
	if (RawData.Num() != 1)
	{
		return;
	}

	EditStructData = MakeShared<FStructOnScope>(nullptr);
	static_cast<FMovieSceneEventParameters*>(RawData[0])->GetInstance(*EditStructData);

	ChildBuilder.AddChildContent(LOCTEXT("ParametersText", "Parameters"))
	.NameContent()
	[
		SNew(STextBlock)
		.Font(CustomizationUtils.GetRegularFont())
		.Text(LOCTEXT("ParameterStructType", "Parameter Struct"))
	]
	.ValueContent()
	.VAlign(VAlign_Top)
	.MaxDesiredWidth(TOptional<float>())
	[
		SNew(SObjectPropertyEntryBox)
		.ObjectPath(EditStructData->GetStruct() ? EditStructData->GetStruct()->GetPathName() : FString())
		.AllowedClass(UScriptStruct::StaticClass())
		.OnObjectChanged(this, &FMovieSceneEventParametersCustomization::OnStructChanged)
	];

	if (EditStructData->GetStructMemory())
	{
		FSimpleDelegate OnEditStructContentsChangedDelegate = FSimpleDelegate::CreateSP(this, &FMovieSceneEventParametersCustomization::OnEditStructContentsChanged);

		TArray<TSharedPtr<IPropertyHandle>> ExternalHandles = ChildBuilder.AddChildStructure(EditStructData.ToSharedRef());
		for (TSharedPtr<IPropertyHandle> Handle : ExternalHandles)
		{
			Handle->SetOnPropertyValueChanged(OnEditStructContentsChangedDelegate);
			Handle->SetOnChildPropertyValueChanged(OnEditStructContentsChangedDelegate);
		}
	}
}

void FMovieSceneEventParametersCustomization::OnStructChanged(const FAssetData& AssetData)
{
	UScriptStruct* NewStruct = nullptr;

	if (AssetData.IsValid())
	{
		NewStruct = Cast<UScriptStruct>(AssetData.GetAsset());
		if (!NewStruct)
		{
			// Don't clear the type if the user managed to just choose the wrong type of object
			return;
		}
	}

	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);
	for (void* Value : RawData)
	{
		static_cast<FMovieSceneEventParameters*>(Value)->Reassign(NewStruct);
	}

	FPropertyChangedEvent BubbleChangeEvent(PropertyHandle->GetProperty(), EPropertyChangeType::ValueSet, nullptr);
	PropertyUtilities->NotifyFinishedChangingProperties(BubbleChangeEvent);
	
	PropertyUtilities->ForceRefresh();
}

void FMovieSceneEventParametersCustomization::OnEditStructContentsChanged()
{
	// @todo: call modify on the outer object if possible
	UScriptStruct* Struct = Cast<UScriptStruct>((UStruct*)EditStructData->GetStruct());
	if (!Struct)
	{
		return;
	}

	TArray<uint8> Bytes;
	{
		FMemoryWriter Writer(Bytes);
		Struct->SerializeTaggedProperties(Writer, EditStructData->GetStructMemory(), Struct, nullptr);
	}

	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	for (void* Value : RawData)
	{
		static_cast<FMovieSceneEventParameters*>(Value)->OverwriteWith(Bytes);
	}

	FPropertyChangedEvent BubbleChangeEvent(PropertyHandle->GetProperty(), EPropertyChangeType::ValueSet, nullptr);
	PropertyUtilities->NotifyFinishedChangingProperties(BubbleChangeEvent);
}

#undef LOCTEXT_NAMESPACE
