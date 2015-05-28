// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "Slate/SlateBrushAsset.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UImage

UImage::UImage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ColorAndOpacity(FLinearColor::White)
{
}

void UImage::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_ASSETS && Image_DEPRECATED != nullptr )
	{
		Brush = Image_DEPRECATED->Brush;
		Image_DEPRECATED = nullptr;
	}
}

void UImage::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyImage.Reset();
}

TSharedRef<SWidget> UImage::RebuildWidget()
{
	MyImage = SNew(SImage);
	return MyImage.ToSharedRef();
}

void UImage::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	TAttribute<FSlateColor> ColorAndOpacityBinding = OPTIONAL_BINDING(FSlateColor, ColorAndOpacity);
	TAttribute<const FSlateBrush*> ImageBinding = OPTIONAL_BINDING_CONVERT(FSlateBrush, Brush, const FSlateBrush*, ConvertImage);

	MyImage->SetImage(ImageBinding);
	MyImage->SetColorAndOpacity(ColorAndOpacityBinding);
	MyImage->SetOnMouseButtonDown(BIND_UOBJECT_DELEGATE(FPointerEventHandler, HandleMouseButtonDown));
}

void UImage::SetColorAndOpacity(FLinearColor InColorAndOpacity)
{
	ColorAndOpacity = InColorAndOpacity;
	if ( MyImage.IsValid() )
	{
		MyImage->SetColorAndOpacity(ColorAndOpacity);
	}
}

void UImage::SetOpacity(float InOpacity)
{
	ColorAndOpacity.A = InOpacity;
	if ( MyImage.IsValid() )
	{
		MyImage->SetColorAndOpacity(ColorAndOpacity);
	}
}

const FSlateBrush* UImage::ConvertImage(TAttribute<FSlateBrush> InImageAsset) const
{
	UImage* MutableThis = const_cast<UImage*>( this );
	MutableThis->Brush = InImageAsset.Get();

	return &Brush;
}

void UImage::SetBrush(const FSlateBrush& InBrush)
{
	Brush = InBrush;

	if ( MyImage.IsValid() )
	{
		MyImage->SetImage(&Brush);
	}
}

void UImage::SetBrushFromAsset(USlateBrushAsset* Asset)
{
	Brush = Asset ? Asset->Brush : FSlateBrush();

	if ( MyImage.IsValid() )
	{
		MyImage->SetImage(&Brush);
	}
}

void UImage::SetBrushFromTexture(UTexture2D* Texture)
{
	Brush.SetResourceObject(Texture);

	if ( MyImage.IsValid() )
	{
		MyImage->SetImage(&Brush);
	}
}

void UImage::SetBrushFromMaterial(UMaterialInterface* Material)
{
	Brush.SetResourceObject(Material);

	//TODO UMG Check if the material can be used with the UI

	if ( MyImage.IsValid() )
	{
		MyImage->SetImage(&Brush);
	}
}

UMaterialInstanceDynamic* UImage::GetDynamicMaterial()
{
	UMaterialInterface* Material = NULL;

	UObject* Resource = Brush.GetResourceObject();
	Material = Cast<UMaterialInterface>(Resource);

	if ( Material )
	{
		UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(Material);

		if ( !DynamicMaterial )
		{
			DynamicMaterial = UMaterialInstanceDynamic::Create(Material, this);
			Brush.SetResourceObject(DynamicMaterial);

			if ( MyImage.IsValid() )
			{
				MyImage->SetImage(&Brush);
			}
		}
		
		return DynamicMaterial;
	}

	//TODO UMG can we do something for textures?  General purpose dynamic material for them?

	return NULL;
}

FReply UImage::HandleMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if ( OnMouseButtonDownEvent.IsBound() )
	{
		return OnMouseButtonDownEvent.Execute(Geometry, MouseEvent).NativeReply;
	}

	return FReply::Unhandled();
}

#if WITH_EDITOR

const FSlateBrush* UImage::GetEditorIcon()
{
	return FUMGStyle::Get().GetBrush("Widget.Image");
}

const FText UImage::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}

#endif


/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
