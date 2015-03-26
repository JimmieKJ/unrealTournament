// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Widget.h"
#include "ComboBoxString.h"

#include "UTComboBoxString.generated.h"

UCLASS()
class UUTComboBoxString : public UComboBoxString
{
	GENERATED_BODY()

public:
	/** font for default item widgets */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Appearance)
	FSlateFontInfo ItemFont;

protected:
	virtual TSharedRef<SWidget> HandleGenerateWidget(TSharedPtr<FString> Item) const;
};