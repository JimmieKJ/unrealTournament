// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCapturePCH.h"
#include "MovieSceneCaptureHelpers.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "MovieSceneAudioTrack.h"
#include "MovieSceneCinematicShotSection.h"
#include "MovieSceneCinematicShotTrack.h"
#include "LevelSequence.h"
#include "AssetRegistryModule.h"
#include "CoreMisc.h"

/* MovieSceneCaptureHelpers
 *****************************************************************************/

struct FShotData
{
	enum class ETrackType
	{
		TT_Video,
		TT_A,
		TT_A2,
		TT_AA,
		TT_None
	};

	enum class EEditType
	{
		ET_Cut,
		ET_Dissolve,
		ET_Wipe,
		ET_KeyEdit,
		ET_None
	};

	FShotData(const FString& InElementName, const FString& InElementPath, ETrackType InTrackType, EEditType InEditType, float InSourceInTime, float InSourceOutTime, float InEditInTime, float InEditOutTime) : 
		  ElementName(InElementName)
		, ElementPath(InElementPath)
		, TrackType(InTrackType)
		, EditType(InEditType)
		, SourceInTime(InSourceInTime)
		, SourceOutTime(InSourceOutTime)
		, EditInTime(InEditInTime)
		, EditOutTime(InEditOutTime) {}

	bool operator<(const FShotData& Other) const { return EditInTime < Other.EditInTime; }

	FString ElementName;
	FString ElementPath;
	ETrackType TrackType;
	EEditType EditType;
	float SourceInTime;
	float SourceOutTime;
	float EditInTime;
	float EditOutTime;
};

void SMPTEToTime(FString SMPTE, float FrameRate, float& OutTime)
{
	TArray<FString> OutArray;
	SMPTE.ParseIntoArray(OutArray, TEXT(":"));
	
	if (OutArray.Num() == 4)
	{
		float Hours = FCString::Atof(*OutArray[0]);
		float Minutes = FCString::Atof(*OutArray[1]);
		float Seconds = FCString::Atof(*OutArray[2]);
		float Frames = FCString::Atof(*OutArray[3]);

		OutTime = Hours * 3600.f + Minutes * 60.f  + Seconds + (Frames / FrameRate);
	}
	// The edl is in frames
	else
	{
		float Frames = FCString::Atof(*SMPTE);
		OutTime = Frames / FrameRate;
	}
}

FString TimeToSMPTE(float InTime, float FrameRate)
{
	float Hours, Minutes, Seconds, Frames;
	Frames = 0.5f + FrameRate * (InTime - FMath::FloorToInt(InTime));

	Hours = FMath::FloorToInt(InTime / (60.f * 60.f));
	InTime -= Hours * 60.f * 60.f;
	Minutes = FMath::FloorToInt(InTime / 60.f);
	InTime -= Minutes * 60.f;
	Seconds = InTime;

	FString OutputString = FString::Printf(TEXT("%02d:%02d:%02d:%02d"), (int)Hours, (int)Minutes, (int)Seconds, (int)Frames);

	return OutputString;
}

void ParseFromEDL(const FString& InputString, float FrameRate, TArray<FShotData>& OutShotData)
{
	TArray<FString> InputLines;
	InputString.ParseIntoArray(InputLines, TEXT("\n"));

	bool bFoundEventLine = false;
	FString EventName;
	FString AuxName;
	FString ReelName;
	FShotData::ETrackType TrackType = FShotData::ETrackType::TT_None;
	FShotData::EEditType EditType = FShotData::EEditType::ET_None;
	float SourceInTime = 0.f;
	float SourceOutTime = 0.f;
	float EditInTime = 0.f;
	float EditOutTime = 0.f;

	for (FString InputLine : InputLines)
	{
		TArray<FString> InputChars;
		InputLine.ParseIntoArrayWS(InputChars);

		// First look for :
		// 001 AX V C 00:00:00:00 00:00:12:02 00:00:07:20 00:00:12:03
		if (!bFoundEventLine)
		{
			if (InputChars.Num() == 8)
			{
				EventName = InputChars[0];
				AuxName = InputChars[1]; // Typically AX but unused in this case

				if (InputChars[2] == TEXT("V"))
				{
					TrackType = FShotData::ETrackType::TT_Video;
				}
				else if (InputChars[2] == TEXT("A"))
				{
					TrackType = FShotData::ETrackType::TT_A;
				}
				else if (InputChars[2] == TEXT("A2"))
				{
					TrackType = FShotData::ETrackType::TT_A2;
				}
				else if (InputChars[2] == TEXT("AA"))
				{
					TrackType = FShotData::ETrackType::TT_AA;
				}

				if (InputChars[3] == TEXT("C"))
				{
					EditType = FShotData::EEditType::ET_Cut;
				}
				else if (InputChars[3] == TEXT("D"))
				{
					EditType = FShotData::EEditType::ET_Dissolve;
				}
				else if (InputChars[3] == TEXT("W"))
				{
					EditType = FShotData::EEditType::ET_Wipe;
				}
				else if (InputChars[3] == TEXT("K"))
				{
					EditType = FShotData::EEditType::ET_KeyEdit;
				}

				// If everything checks out
				if (TrackType != FShotData::ETrackType::TT_None &&
					EditType != FShotData::EEditType::ET_None)
				{
					SMPTEToTime(InputChars[4], FrameRate, SourceInTime);
					SMPTEToTime(InputChars[5], FrameRate, SourceOutTime);
					SMPTEToTime(InputChars[6], FrameRate, EditInTime);
					SMPTEToTime(InputChars[7], FrameRate, EditOutTime);

					bFoundEventLine = true;
					continue; // Go to the next line
				}
			}
		}
		
		// Then look for:
		// * FROM CLIP NAME: shot0010_001.avi
		else
		{
			if (InputChars.Num() == 5 &&
				InputChars[0] == TEXT("*") &&
				InputChars[1].ToUpper() == TEXT("FROM") &&
				InputChars[2].ToUpper() == TEXT("CLIP") &&
				InputChars[3].ToUpper() == TEXT("NAME:"))
			{
				ReelName = InputChars[4];

				//@todo can't assume avi's written out
				ReelName = ReelName.LeftChop(4); // strip .avi

				FString ElementName = ReelName;
				FString ElementPath = ElementName;

				// If everything checks out add to OutShotData
				OutShotData.Add(FShotData(ElementName, ElementPath, TrackType, EditType, SourceInTime, SourceOutTime, EditInTime, EditOutTime));
				bFoundEventLine = false; // Reset and go to next line to look for element line
				continue;
			}
		}
	}
}

void FormatForEDL(FString& OutputString, const FString& SequenceName, float FrameRate, const TArray<FShotData>& InShotData)
{
	OutputString += TEXT("TITLE: ") + SequenceName + TEXT("\n");
	OutputString += TEXT("FCM: NON-DROP FRAME\n\n");

	for (int32 EventIndex = 0; EventIndex < InShotData.Num(); ++EventIndex)
	{
		FString EventName = FString::Printf(TEXT("%03d"), EventIndex+1);

		FString ReelName = InShotData[EventIndex].ElementName; 

		FString TypeName;
		if (InShotData[EventIndex].TrackType == FShotData::ETrackType::TT_Video)
		{
			TypeName = TEXT("V");
		}
		else if (InShotData[EventIndex].TrackType == FShotData::ETrackType::TT_A)
		{
			TypeName = TEXT("A");
		}
		else if (InShotData[EventIndex].TrackType == FShotData::ETrackType::TT_A2)
		{
			TypeName = TEXT("A2");
		}
		else if (InShotData[EventIndex].TrackType == FShotData::ETrackType::TT_AA)
		{
			TypeName = TEXT("AA");
		}

		FString EditName;
		if (InShotData[EventIndex].EditType == FShotData::EEditType::ET_Cut)
		{
			EditName = TEXT("C");
		}
		else if (InShotData[EventIndex].EditType == FShotData::EEditType::ET_Dissolve)
		{
			EditName = TEXT("D");
		}
		else if (InShotData[EventIndex].EditType == FShotData::EEditType::ET_Wipe)
		{
			EditName = TEXT("W");
		}
		else if (InShotData[EventIndex].EditType == FShotData::EEditType::ET_KeyEdit)
		{
			EditName = TEXT("K");
		}

		FString SourceSMPTEIn = TimeToSMPTE(InShotData[EventIndex].SourceInTime, FrameRate);
		FString SourceSMPTEOut = TimeToSMPTE(InShotData[EventIndex].SourceOutTime, FrameRate);
		FString EditSMPTEIn = TimeToSMPTE(InShotData[EventIndex].EditInTime, FrameRate);
		FString EditSMPTEOut = TimeToSMPTE(InShotData[EventIndex].EditOutTime, FrameRate);

		OutputString += EventName + TEXT(" ") + TEXT("AX ") + TypeName + TEXT(" ") + EditName + TEXT(" ");
		OutputString += SourceSMPTEIn + TEXT(" ") + SourceSMPTEOut + TEXT(" ") + EditSMPTEIn + TEXT(" ") + EditSMPTEOut + TEXT("\n");
		OutputString += TEXT("* FROM CLIP NAME: ") + ReelName + TEXT("\n\n");
	}
}

void FormatForRV(FString& OutputString, const FString& SequenceName, float FrameRate, const TArray<FShotData>& InShotData)
{
	// Header
	OutputString += TEXT("GTOa (3)\n\n");
	OutputString += TEXT("rv : RVSession (2)\n");
	OutputString += TEXT("{\n");
	OutputString += TEXT("\tsession\n");
	OutputString += TEXT("\t{\n");
	OutputString += TEXT("\t\tfloat fps = ") + FString::Printf(TEXT("%f"), FrameRate) + TEXT("\n");
	OutputString += TEXT("\t\tint realtime = 1\n");
	OutputString += TEXT("\t}\n\n");
	OutputString += TEXT("\twriter\n");
	OutputString += TEXT("\t{\n");
	OutputString += TEXT("\t\tstring name = \"rvSession.py\"\n");
	OutputString += TEXT("\t\tstring version = \"0.3\"\n");
	OutputString += TEXT("\t}\n");
	OutputString += TEXT("}\n\n");

	// Body
	for (int32 EventIndex = 0; EventIndex < InShotData.Num(); ++EventIndex)
	{
		FString SourceName = FString::Printf(TEXT("sourceGroup%06d"), EventIndex);

		int32 SourceInFrame = FMath::RoundToInt(InShotData[EventIndex].SourceInTime * FrameRate);
		int32 SourceOutFrame = FMath::RoundToInt(InShotData[EventIndex].SourceOutTime * FrameRate);

		OutputString += SourceName + TEXT(" : RVSourceGroup (1)\n");
		OutputString += TEXT("{\n");
		OutputString += TEXT("\tui\n");
		OutputString += TEXT("\t{\n");
		OutputString += TEXT("\t\tstring name = \"") + InShotData[EventIndex].ElementName + TEXT("\"\n");
		OutputString += TEXT("\t}\n");
		OutputString += TEXT("}\n\n");

		OutputString += SourceName + TEXT("_source : RVFileSource (1)\n");
		OutputString += TEXT("{\n");
		OutputString += TEXT("\tcut\n");
		OutputString += TEXT("\t{\n");
		OutputString += TEXT("\t\tint in = ") + FString::FromInt(SourceInFrame) + TEXT("\n");
		OutputString += TEXT("\t\tint out = ") + FString::FromInt(SourceOutFrame) + TEXT("\n");
		OutputString += TEXT("\t}\n\n");

		OutputString += TEXT("\tgroup\n");
		OutputString += TEXT("\t{\n");
		OutputString += TEXT("\t\tint noMovieAudio = 1\n");
		OutputString += TEXT("\t}\n\n");

		OutputString += TEXT("\tmedia\n");
		OutputString += TEXT("\t{\n");
		OutputString += TEXT("\t\tstring movie = \"") + InShotData[EventIndex].ElementPath + TEXT("\"\n");
		OutputString += TEXT("\t\tstring shot = \"\"\n");
		OutputString += TEXT("\t}\n");
		OutputString += TEXT("}\n\n");
	}
}

bool MovieSceneCaptureHelpers::ImportEDL(UMovieScene* InMovieScene, float InFrameRate, FString InFilename)
{
	FString InputString;
	if (!FFileHelper::LoadFileToString(InputString, *InFilename))
	{
		return false;
	}

	TArray<FShotData> ShotDataArray;
	ParseFromEDL(InputString, InFrameRate, ShotDataArray);

	UMovieSceneCinematicShotTrack* CinematicShotTrack = Cast<UMovieSceneCinematicShotTrack>(InMovieScene->FindMasterTrack<UMovieSceneCinematicShotTrack>());
	if (!CinematicShotTrack)
	{
		CinematicShotTrack = InMovieScene->AddMasterTrack<UMovieSceneCinematicShotTrack>();
	}

	for (FShotData ShotData : ShotDataArray)
	{
		if (ShotData.TrackType == FShotData::ETrackType::TT_Video)
		{
			UMovieSceneCinematicShotSection* ShotSection = nullptr;

			FString ShotName = ShotData.ElementName;
			for (UMovieSceneSection* Section : CinematicShotTrack->GetAllSections())
			{
				UMovieSceneCinematicShotSection* CinematicShotSection = Cast<UMovieSceneCinematicShotSection>(Section);
				if (CinematicShotSection != nullptr)
				{
					UMovieSceneSequence* ShotSequence = CinematicShotSection->GetSequence();
				
					if (ShotSequence != nullptr && ShotSequence->GetName() == ShotName)
					{
						ShotSection = CinematicShotSection;
						break;
					}
				}
			}

			// If the shot doesn't already exist, create it
			if (!ShotSection)
			{
				UMovieSceneSequence* SequenceToAdd = nullptr;

				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

				// Collect a full list of assets with the specified class
				TArray<FAssetData> AssetDataArray;
				AssetRegistryModule.Get().GetAssetsByClass(ULevelSequence::StaticClass()->GetFName(), AssetDataArray);

				for (FAssetData AssetData : AssetDataArray)
				{
					if (AssetData.AssetName == *ShotName)
					{	
						SequenceToAdd = Cast<ULevelSequence>(AssetData.GetAsset());
						break;
					}
				}

				CinematicShotTrack->Modify();
				ShotSection = Cast<UMovieSceneCinematicShotSection>(CinematicShotTrack->AddSequence(SequenceToAdd, ShotData.EditInTime, ShotData.EditOutTime - ShotData.EditInTime));
			}

			// Conform this shot section
			if (ShotSection)
			{
				ShotSection->Modify();
				ShotSection->StartOffset = ShotData.SourceInTime;
				ShotSection->SetStartTime(ShotData.EditInTime);
				ShotSection->SetEndTime(ShotData.EditOutTime);
			}
		}
	}

	return true;
}

bool MovieSceneCaptureHelpers::ExportEDL(const UMovieScene* InMovieScene, float InFrameRate, FString InSaveFilename)
{
	FString SequenceName = InMovieScene->GetOuter()->GetName();
	FString SaveBasename = FPaths::GetPath(InSaveFilename) / FPaths::GetBaseFilename(InSaveFilename);

	TArray<FString> SaveFilenames;
	if (!SaveBasename.IsEmpty())
	{
		SaveFilenames.Add(SaveBasename + TEXT(".rv"));
		SaveFilenames.Add(SaveBasename + TEXT(".edl"));
	}

	TArray<FShotData> ShotDataArray;

	for (auto MasterTrack : InMovieScene->GetMasterTracks())
	{
		TRange<float> PlaybackRange = InMovieScene->GetPlaybackRange();
		if (MasterTrack->IsA(UMovieSceneCinematicShotTrack::StaticClass()))
		{
			UMovieSceneCinematicShotTrack* CinematicShotTrack = Cast<UMovieSceneCinematicShotTrack>(MasterTrack);

			for (auto ShotSection : CinematicShotTrack->GetAllSections())
			{
				UMovieSceneCinematicShotSection* CinematicShotSection = Cast<UMovieSceneCinematicShotSection>(ShotSection);

				if ( CinematicShotSection->GetSequence() == nullptr )
				{
					// If the shot doesn't have a valid sequence skip it.  This is currently the case for filler sections.
					// TODO: Handle this properly in the edl output.
					continue;
				}

				FString ShotName = CinematicShotSection->GetShotDisplayName().ToString();
				FString ShotPath = CinematicShotSection->GetSequence()->GetMovieScene()->GetOuter()->GetPathName();

				//@todo until shot handles are rendered out, the source in time will always be 0
				float SourceInTime = 0.f; //CinematicShotSection->StartOffset;
				float SourceOutTime = SourceInTime + CinematicShotSection->GetTimeSize();
				float EditInTime = CinematicShotSection->GetStartTime();
				float EditOutTime = CinematicShotSection->GetEndTime();

				EditInTime = FMath::Clamp(EditInTime, PlaybackRange.GetLowerBoundValue(), PlaybackRange.GetUpperBoundValue());
				EditOutTime = FMath::Clamp(EditOutTime, PlaybackRange.GetLowerBoundValue(), PlaybackRange.GetUpperBoundValue());

				//@todo can't assume avi's written out
				ShotName += ".avi";

				//@todo shotpath should really be moviefile path
				ShotPath = ShotName;

				ShotDataArray.Add(FShotData(ShotName, ShotPath, FShotData::ETrackType::TT_Video, FShotData::EEditType::ET_Cut, SourceInTime, SourceOutTime, EditInTime, EditOutTime));
			}
		}
		else if (MasterTrack->IsA(UMovieSceneAudioTrack::StaticClass()))
		{
			UMovieSceneAudioTrack* AudioTrack = Cast<UMovieSceneAudioTrack>(MasterTrack);

			//@todo support audio clips
		}
	}

	ShotDataArray.Sort();

	for (auto SaveFilename : SaveFilenames)
	{
		FString OutputString;

		if (FPaths::GetExtension(SaveFilename).ToUpper() == TEXT("EDL"))
		{
			FormatForEDL(OutputString, SequenceName, InFrameRate, ShotDataArray);
		}
		else if (FPaths::GetExtension(SaveFilename).ToUpper() == TEXT("RV"))
		{
			FormatForRV(OutputString, SequenceName, InFrameRate, ShotDataArray);
		}

		FFileHelper::SaveStringToFile(OutputString, *SaveFilename);
	}

	return true;
}