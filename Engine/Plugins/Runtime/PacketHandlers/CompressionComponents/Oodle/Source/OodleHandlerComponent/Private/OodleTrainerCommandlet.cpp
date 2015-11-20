// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OodleHandlerComponentPCH.h"

#include "OodleTrainerCommandlet.h"


#define LOCTEXT_NAMESPACE "Oodle"


// @todo #JohnB: Make sure to note that >100mb data (for production games, even more) should be captured

// @todo #JohnB: Gather stats while merging, to give the commandlet some more informative output

/**
 * UOodleTrainerCommandlet
 */

UOodleTrainerCommandlet::UOodleTrainerCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UOodleTrainerCommandlet::Main(const FString& Params)
{
#if HAS_OODLE_SDK
	TArray<FString> Tokens, Switches;

	ParseCommandLine(*Params, Tokens, Switches);

	if (Tokens.Num() > 0)
	{
		FString MainCmd = Tokens[0];

		/**
		 * Enable
		 *
		 * Command for quick-enabling Oodle
		 */
		if (MainCmd == TEXT("Enable"))
		{
			TArray<FString> Components;
			bool bAlreadyEnabled = false;

			GConfig->GetArray(TEXT("PacketHandlerComponents"), TEXT("Components"), Components, GEngineIni);

			for (FString CurComponent : Components)
			{
				if (CurComponent.StartsWith(TEXT("OodleHandlerComponent")))
				{
					bAlreadyEnabled = true;
					break;
				}
			}


			if (!bAlreadyEnabled)
			{
				GConfig->SetBool(TEXT("PacketHandlerComponents"), TEXT("bEnabled"), true, GEngineIni);

				Components.Add(TEXT("OodleHandlerComponent"));
				GConfig->SetArray(TEXT("PacketHandlerComponents"), TEXT("Components"), Components, GEngineIni);

				OodleHandlerComponent::InitFirstRunConfig();


				UE_LOG(OodleHandlerComponentLog, Log, TEXT("Initialized first-run settings for Oodle (will start in Trainer mode)."));
			}
			else
			{
				UE_LOG(OodleHandlerComponentLog, Error, TEXT("The Oodle PacketHandler is already enabled."));
			}
		}
		/**
		 * MergePackets
		 *
		 * Command for merging capture packets
		 */
		else if (MainCmd == TEXT("MergePackets"))
		{
			if (Tokens.Num() > 2)
			{
				FString OutputCapFile = Tokens[1];
				FString MergeList = Tokens[2];
				IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
				TArray<FString> MergeFiles;
				bool bValidFileParms = true;


				if (MergeList == TEXT("all"))
				{
					bValidFileParms = false;

					// @todo #JohnB: Document

					if (Tokens.Num() > 3)
					{
						FString MergeDir = Tokens[3];

						if (FPaths::IsRelative(MergeDir))
						{
							MergeDir = FPaths::Combine(*GOodleSaveDir, TEXT("Server"), *MergeDir);
						}

						if (FPaths::DirectoryExists(MergeDir))
						{
							IFileManager::Get().FindFiles(MergeFiles, *MergeDir, TEXT("ucap"));

							if (MergeFiles.Num())
							{
								bValidFileParms = true;

								for (int32 i=0; i<MergeFiles.Num(); i++)
								{
									MergeFiles[i] = FPaths::Combine(*MergeDir, *MergeFiles[i]);
								}
							}
							else
							{
								UE_LOG(OodleHandlerComponentLog, Error, TEXT("Could not find any .ucap files in directory: %s"),
										*MergeDir);
							}
						}
						else
						{
							UE_LOG(OodleHandlerComponentLog, Error, TEXT("Could not find directory: %s"), *MergeDir);
						}
					}
					else
					{
						UE_LOG(OodleHandlerComponentLog, Error,
								TEXT("Need to specify a target directory, e.g: MergePackets OutFile All Dir"));
					}
				}
				else
				{
					MergeList.ParseIntoArray(MergeFiles, TEXT(","));
				}


				// Adjust non-absolute directories, so they are relative to the Saved\Oodle\Server directory
				if (bValidFileParms && FPaths::IsRelative(OutputCapFile))
				{
					OutputCapFile = FPaths::Combine(*GOodleSaveDir, TEXT("Server"), *OutputCapFile);

					if (FPaths::FileExists(OutputCapFile))
					{
						if (FApp::IsUnattended())
						{
							// @todo #JohnB: Error message

							bValidFileParms = false;
						}
						else
						{
							EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo,
																		LOCTEXT("MergeOverwrite", "Overwrite existing output file?"));

							bValidFileParms = (Result == EAppReturnType::Yes);
						}
					}
				}

				for (int32 i=0; i<MergeFiles.Num() && bValidFileParms; i++)
				{
					FString CurMergeFile = MergeFiles[i];

					if (FPaths::IsRelative(CurMergeFile))
					{
						MergeFiles[i] = FPaths::Combine(*GOodleSaveDir, TEXT("Server"), *CurMergeFile);

						if (!FPaths::FileExists(CurMergeFile))
						{
							UE_LOG(OodleHandlerComponentLog, Error, TEXT("Packet capture file does not exist: %s"), *CurMergeFile);

							bValidFileParms = false;
						}
					}
				}


				if (bValidFileParms && MergeFiles.Num() > 1)
				{
					bool bAllMergeHandlesValid = true;
					TMap<FArchive*, FString> MergeMap;

					for (FString CurMergeFile : MergeFiles)
					{
						FArchive* ReadArc = IFileManager::Get().CreateFileReader(*CurMergeFile);

						if (ReadArc != NULL)
						{
							MergeMap.Add(ReadArc, CurMergeFile);
						}
						else
						{
							UE_LOG(OodleHandlerComponentLog, Error, TEXT("Could not read packet capture file: %s"), *CurMergeFile);

							bAllMergeHandlesValid = false;
							break;
						}
					}

					FArchive* OutArc = (bAllMergeHandlesValid ? IFileManager::Get().CreateFileWriter(*OutputCapFile) : NULL);

					if (bAllMergeHandlesValid && OutArc != NULL)
					{
						UE_LOG(OodleHandlerComponentLog, Log, TEXT("Merging files into output file: %s"), *OutputCapFile);

						FPacketCaptureArchive OutputFile(*OutArc);
						bool bErrorAppending = false;

						OutputFile.SerializeCaptureHeader();

						for (TMap<FArchive*, FString>::TConstIterator It(MergeMap); It; ++It)
						{
							FArchive* CurReadArc = It.Key();
							FPacketCaptureArchive ReadFile(*CurReadArc);

							UE_LOG(OodleHandlerComponentLog, Log, TEXT("    Merging: %s"), *It.Value());

							OutputFile.AppendPacketFile(ReadFile);

							if (OutputFile.IsError())
							{
								bErrorAppending = true;
								break;
							}

							delete CurReadArc;
						}

						MergeMap.Empty();


						if (bErrorAppending)
						{
							UE_LOG(OodleHandlerComponentLog, Error, TEXT("Error appending packet file."));
						}


						// @todo #JohnB: Log success

						OutArc->Close();

						delete OutArc;

						OutArc = NULL;
					}
					else if (!bAllMergeHandlesValid)
					{
						UE_LOG(OodleHandlerComponentLog, Error, TEXT("Could not read all packet files."));
					}
					else // if (OutArc == NULL)
					{
						UE_LOG(OodleHandlerComponentLog, Error, TEXT("Could not create output file."));
					}
				}
				else if (!bValidFileParms)
				{
					UE_LOG(OodleHandlerComponentLog, Error,
							TEXT("The packet files must exist, and the output file must not already exist"));
				}
				else // if (MergeFiles.Num() <= 1)
				{
					UE_LOG(OodleHandlerComponentLog, Error, TEXT("You need to specify 2 or more packet files for merging"));
				}
			}
			else
			{
				UE_LOG(OodleHandlerComponentLog, Error,
						TEXT("Error parsing 'MergePackets' parameters. Must specify an output file, and packet files to merge"));
			}
		}
		else
		{
			UE_LOG(OodleHandlerComponentLog, Error, TEXT("Unrecognized sub-command '%s'"), *MainCmd);
		}
	}
	else
	{
		UE_LOG(OodleHandlerComponentLog, Error, TEXT("Error parsing main commandlet sub-command"));

		// @todo #JohnB: Add a 'help' command, with documentation output
	}
#else
	UE_LOG(OodleHandlerComponentLog, Error, TEXT("Oodle unavailable."));
#endif


	return (GWarn->Errors.Num() == 0 ? 0 : 1);
}

#undef LOCTEXT_NAMESPACE

