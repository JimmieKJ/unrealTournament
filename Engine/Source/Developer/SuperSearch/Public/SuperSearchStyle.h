// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateWidgetStyle.h"
#include "Styling/SlateTypes.h"

#include "SuperSearchStyle.generated.h"

UENUM()
enum class ESuperSearchEnginePlacement : uint8
{
	Left,
	Inside,
	None,
};

USTRUCT()
struct SUPERSEARCH_API FSuperSearchStyle : public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	FSuperSearchStyle();

	virtual ~FSuperSearchStyle() {}

	virtual void GetResources(TArray< const FSlateBrush* >& OutBrushes) const override;

	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };

	static const FSuperSearchStyle& GetDefault();

	/** Style to use for the search engine selector */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FComboBoxStyle ComboBoxStyle;
	FSuperSearchStyle& SetComboBoxStyle(const FComboBoxStyle& InComboBoxStyle) { ComboBoxStyle = InComboBoxStyle; return *this; }

	/** The foreground color of text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor ForegroundColor;
	FSuperSearchStyle& SetForegroundColor(const FSlateColor& InForegroundColor) { ForegroundColor = InForegroundColor; return *this; }

	/** Style to use for the selected search engine text*/
	UPROPERTY(EditAnywhere, Category = Appearance)
	FTextBlockStyle TextBlockStyle;
	FSuperSearchStyle& SetTextBlockStyle(const FTextBlockStyle& InTextBlockStyle) { TextBlockStyle = InTextBlockStyle; return *this; }

	/** Style to used for the search box*/
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSearchBoxStyle SearchBoxStyle;
	FSuperSearchStyle& SetSearchBoxStyle(const FSearchBoxStyle& InSearchBoxStyle) { SearchBoxStyle = InSearchBoxStyle; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	ESuperSearchEnginePlacement SearchEnginePlacement;
	FSuperSearchStyle& SetSearchEnginePlacement(ESuperSearchEnginePlacement InSearchEnginePlacement) { SearchEnginePlacement = InSearchEnginePlacement; return *this; }
};

