// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UNativeWidgetHost

UNativeWidgetHost::UNativeWidgetHost(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = false;
}

void UNativeWidgetHost::SetContent(TSharedRef<SWidget> InContent)
{
	MyWidget = InContent;
}

void UNativeWidgetHost::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyWidget.Reset();
}

TSharedRef<SWidget> UNativeWidgetHost::RebuildWidget()
{
	if ( MyWidget.IsValid() )
	{
		return MyWidget.ToSharedRef();
	}
	else
	{
		return SNew(SBorder)
		.Visibility(EVisibility::HitTestInvisible)
		.BorderImage(FUMGStyle::Get().GetBrush("MarchingAnts"))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NativeWidgetHostText", "Slate Widget Host"))
		];
	}
}

#if WITH_EDITOR

const FSlateBrush* UNativeWidgetHost::GetEditorIcon()
{
	return FUMGStyle::Get().GetBrush("Widget.NativeWidgetHost");
}

const FText UNativeWidgetHost::GetPaletteCategory()
{
	return LOCTEXT("Primitive", "Primitive");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
