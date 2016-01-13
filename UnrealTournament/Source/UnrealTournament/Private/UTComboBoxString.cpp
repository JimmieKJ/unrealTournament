// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTComboBoxString.h"
#if !UE_SERVER
#include "STextBlock.h"
#endif

TSharedRef<SWidget> UUTComboBoxString::HandleGenerateWidget(TSharedPtr<FString> Item) const
{
#if UE_SERVER
	return Super::HandleGenerateWidget(Item);
#else
	FString StringItem = Item.IsValid() ? *Item : FString();

	// Call the user's delegate to see if they want to generate a custom widget bound to the data source.
	if (!IsDesignTime() && OnGenerateWidgetEvent.IsBound())
	{
		UWidget* Widget = OnGenerateWidgetEvent.Execute(StringItem);
		if (Widget != NULL)
		{
			return Widget->TakeWidget();
		}
	}

	// If a row wasn't generated just create the default one, a simple text block of the item's name.
	if (ItemFont.HasValidFont())
	{
		return SNew(STextBlock).Text(FText::FromString(StringItem)).Font(ItemFont);
	}
	else
	{
		return SNew(STextBlock).Text(FText::FromString(StringItem));
	}
#endif
}