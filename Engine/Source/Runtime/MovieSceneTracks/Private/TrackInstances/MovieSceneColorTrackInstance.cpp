// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneColorTrackInstance.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneColorTrack.h"
#include "SlateCore.h"


FMovieSceneColorTrackInstance::FMovieSceneColorTrackInstance( UMovieSceneColorTrack& InColorTrack )
	: ColorTrack( &InColorTrack )
	, ColorType( EColorType::Linear )
{
	PropertyBindings = MakeShareable( new FTrackInstancePropertyBindings( ColorTrack->GetPropertyName(), ColorTrack->GetPropertyPath() ) );
}


void FMovieSceneColorTrackInstance::SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (auto ObjectPtr : RuntimeObjects)
	{
		UObject* Object = ObjectPtr.Get();
		
		if( ColorType == EColorType::Slate )
		{
			if (InitSlateColorMap.Find(Object) == nullptr)
			{			
				FSlateColor ColorValue = PropertyBindings->GetCurrentValue<FSlateColor>(Object);
				InitSlateColorMap.Add(Object, ColorValue);
			}
		}
		else if( ColorType == EColorType::Linear )
		{
			if (InitLinearColorMap.Find(Object) == nullptr)
			{			
				UProperty* Property = PropertyBindings->GetProperty(Object);
				FLinearColor ColorValue = PropertyBindings->GetCurrentValue<FLinearColor>(Object);
				InitLinearColorMap.Add(Object, ColorValue);
			}
		}
		else if( ColorType == EColorType::Color )
		{
			if (InitLinearColorMap.Find(Object) == nullptr)
			{			
				UProperty* Property = PropertyBindings->GetProperty(Object);
				FColor ColorValue = PropertyBindings->GetCurrentValue<FColor>(Object);
				FLinearColor LinearColorValue = ColorValue;
				InitLinearColorMap.Add(Object, ColorValue);
			}
		}
		else 
		{
			checkf(0, TEXT("Invalid Color Type"));
		}
	}
}


void FMovieSceneColorTrackInstance::RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (auto ObjectPtr : RuntimeObjects )
	{
		UObject* Object = ObjectPtr.Get();

		if (!IsValid(Object))
		{
			continue;
		}

		if( ColorType == EColorType::Slate )
		{
			FSlateColor* ColorValue = InitSlateColorMap.Find(Object);
			if (ColorValue != nullptr)
			{
				PropertyBindings->CallFunction<FSlateColor>(Object, ColorValue);
			}
		}
		else if( ColorType == EColorType::Linear || ColorType == EColorType::Color )
		{
			//todo
			FLinearColor* ColorValue = InitLinearColorMap.Find(Object);
			if (ColorValue != nullptr)
			{
				PropertyBindings->CallFunction<FLinearColor>(Object, ColorValue);
			}
		}
		else
		{
			checkf(0, TEXT("Invalid Color Type"));
		}
	}

	PropertyBindings->UpdateBindings( RuntimeObjects );
}


void FMovieSceneColorTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) 
{
	for (auto ObjectPtr : RuntimeObjects)
	{
		UObject* Object = ObjectPtr.Get();
		if ( Object != nullptr )
		{
			if ( ColorType == EColorType::Slate )
			{
				FSlateColor ColorValue = PropertyBindings->GetCurrentValue<FSlateColor>( Object );
				FLinearColor LinearColor = ColorValue.GetSpecifiedColor();

				if ( ColorTrack->Eval( UpdateData.Position, UpdateData.LastPosition, LinearColor ) )
				{
					FSlateColor NewColor( LinearColor );
					PropertyBindings->CallFunction<FSlateColor>( Object, &NewColor );
				}
			}
			else if ( ColorType == EColorType::Color )
			{
				// We assume the color we get back is in sRGB, assigning it to a linear color will implicitly
				// convert it to a linear color instead of using ReinterpretAsLinear which will just change the
				// bytes into floats using divide by 255.
				FColor SrgbColor = PropertyBindings->GetCurrentValue<FColor>( Object );
				FLinearColor LinearColorValue = SrgbColor;
				bool bConvertBackToSRgb = true;
				if ( ColorTrack->Eval( UpdateData.Position, UpdateData.LastPosition, LinearColorValue ) )
				{
					// Light components have to be handled specially here because their set function takes two values, the linear color
					// and whether or not the linear color needs to be converted back to sRGB.  All other other set function cases should
					// follow the sequencer convention of having a single parameter of the correct type, which in this case is an FColor
					// already in sRGB format.
					ULightComponent* LightComponent = Cast<ULightComponent>( Object );
					if ( LightComponent != nullptr )
					{
						LightComponent->SetLightColor( LinearColorValue, bConvertBackToSRgb );
					}
					else
					{
						FColor SRgbColorValue = LinearColorValue.ToFColor( bConvertBackToSRgb );
						PropertyBindings->CallFunction<FColor>( Object, &SRgbColorValue );
					}
				}
			}
			else
			{
				FLinearColor ColorValue = PropertyBindings->GetCurrentValue<FLinearColor>( Object );

				if ( ColorTrack->Eval( UpdateData.Position, UpdateData.LastPosition, ColorValue ) )
				{
					PropertyBindings->CallFunction<FLinearColor>( Object, &ColorValue );
				}
			}
		}
	}
}


void FMovieSceneColorTrackInstance::RefreshInstance( const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance )
{
	if( RuntimeObjects.Num() == 0 )
	{
		return;
	}

	PropertyBindings->UpdateBindings( RuntimeObjects );

	// Cache off what type of color this is. Just examine the first object since the property should be the same
	UProperty* ColorProp = PropertyBindings->GetProperty( RuntimeObjects[0].Get() );
	const UStructProperty* StructProp = Cast<const UStructProperty>(ColorProp);

	if (StructProp && StructProp->Struct)
	{
		FName StructName = StructProp->Struct->GetFName();

		static const FName SlateColor("SlateColor");

		if( StructName == NAME_Color )
		{
			ColorType = EColorType::Color;
		}
		else if( StructName == SlateColor )
		{
			ColorType = EColorType::Slate;
		}
		else
		{
			ColorType = EColorType::Linear;
		}
	}
}
