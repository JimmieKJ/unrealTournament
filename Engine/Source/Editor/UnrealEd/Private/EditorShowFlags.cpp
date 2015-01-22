// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "EditorShowFlags.h"


FShowFlagData::FShowFlagData(const FString& InName, const FText& InDisplayName, const uint32 InEngineShowFlagIndex, EShowFlagGroup InGroup )
	:	DisplayName( InDisplayName )
	,	EngineShowFlagIndex( InEngineShowFlagIndex )
	,	Group( InGroup )
{
	ShowFlagName = *InName;
}

FShowFlagData::FShowFlagData(const FString& InName, const FText& InDisplayName, const uint32 InEngineShowFlagIndex, EShowFlagGroup InGroup, const FInputGesture& InInputGesture)
	:	DisplayName( InDisplayName )
	,	EngineShowFlagIndex( InEngineShowFlagIndex )
	,	Group( InGroup )
	,	InputGesture( InInputGesture )
{
	ShowFlagName = *InName;
}

bool FShowFlagData::IsEnabled(const FLevelEditorViewportClient* ViewportClient) const
{
	return ViewportClient->EngineShowFlags.GetSingleFlag(EngineShowFlagIndex);
}

void FShowFlagData::ToggleState(FLevelEditorViewportClient* ViewportClient) const
{
	bool bOldState = ViewportClient->EngineShowFlags.GetSingleFlag(EngineShowFlagIndex);
	ViewportClient->EngineShowFlags.SetSingleFlag(EngineShowFlagIndex, !bOldState);
}

TArray<FShowFlagData>& GetShowFlagMenuItems()
{
	static TArray<FShowFlagData> OutShowFlags;

	static bool bFirst = true; 
	if(bFirst)
	{
		// do this only once
		bFirst = false;

		// FEngineShowFlags 
		{
			TMap<FString, FInputGesture> EngineFlagsGestures;

			EngineFlagsGestures.Add("Navigation", FInputGesture(EKeys::P) );
			EngineFlagsGestures.Add("BSP", FInputGesture() );
			EngineFlagsGestures.Add("Collision", FInputGesture(EKeys::C, EModifierKey::Alt) );
			EngineFlagsGestures.Add("Fog", FInputGesture(EKeys::F, EModifierKey::Alt ) );
			EngineFlagsGestures.Add("LightRadius", FInputGesture(EKeys::R, EModifierKey::Alt) );
			EngineFlagsGestures.Add("StaticMeshes", FInputGesture() );
			EngineFlagsGestures.Add("Landscape", FInputGesture(EKeys::L, EModifierKey::Alt) );
			EngineFlagsGestures.Add("Volumes", FInputGesture(EKeys::O, EModifierKey::Alt) );

			struct FIterSink
			{
				FIterSink(TArray<FShowFlagData>& InShowFlagData, const TMap<FString, FInputGesture>& InGesturesMap) 
					: ShowFlagData(InShowFlagData), GesturesMap(InGesturesMap)
				{
				}

				bool OnEngineShowFlag(uint32 InIndex, const FString& InName)
				{
					EShowFlagGroup Group = FEngineShowFlags::FindShowFlagGroup(*InName);
					if( Group != SFG_Hidden  )
					{
						FText FlagDisplayName;
						FEngineShowFlags::FindShowFlagDisplayName(InName, FlagDisplayName);

						const FInputGesture* Gesture = GesturesMap.Find(InName);
						if (Gesture != NULL)
						{
							ShowFlagData.Add( FShowFlagData( InName, FlagDisplayName, InIndex, Group, *Gesture ) );
						}
						else
						{
							ShowFlagData.Add( FShowFlagData( InName, FlagDisplayName, InIndex, Group ) );
						}
					}
					return true;
				}

				TArray<FShowFlagData>& ShowFlagData;
				const TMap<FString, FInputGesture>& GesturesMap;
			};

			FIterSink Sink(OutShowFlags, EngineFlagsGestures);

			FEngineShowFlags::IterateAllFlags(Sink);
		}	

		// Sort the show flags alphabetically by string.
		struct FCompareFShowFlagDataByName
		{
			FORCEINLINE bool operator()( const FShowFlagData& A, const FShowFlagData& B ) const
			{
				return A.DisplayName.ToString() < B.DisplayName.ToString();
			}
		};
		OutShowFlags.Sort( FCompareFShowFlagDataByName() );
	}

	return OutShowFlags;
}





