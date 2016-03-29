// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "HMDPrivatePCH.h"
#include "IOculusRiftPlugin.h"

#if OCULUS_RIFT_SUPPORTED_PLATFORMS


#include "OculusRiftHMD.h"
#include "EngineGlobals.h"
#include "EngineVersion.h"
#include "Matinee/MatineeActor.h"
#include "Matinee/InterpData.h"
#include "Matinee/InterpGroup.h"
#include "Matinee/InterpGroupDirector.h"
#include "Matinee/InterpTrack.h"
#include "Matinee/InterpTrackDirector.h"
#include "AutomationCommon.h"
#include "Misc/OutputDevice.h"
#include "ChartCreation.h"


//-------------------------------------------------------------------------------------------------
// Common commands
//-------------------------------------------------------------------------------------------------

namespace
{
	// Wait a number of seconds
	// Cannot use macro, since need to redefine StartTime as a double.
	class FOculusAutomationLatentCommand_Wait : public IAutomationLatentCommand
	{
		double Seconds;
		double StartTime;

	public:
		FOculusAutomationLatentCommand_Wait(double seconds)
		{
			Seconds = seconds;
			StartTime = 0.0;
		}

		virtual bool Update() override
		{
			double time = FPlatformTime::Seconds();

			if(StartTime == 0.0)
				StartTime = time;

			return time - StartTime >= Seconds;
		}
	};


	// Delay a number of frames
	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FOculusAutomationLatentCommand_Delay, uint32, Frames);

	bool FOculusAutomationLatentCommand_Delay::Update()
	{
		if(Frames)
		{
			Frames--;
			return false;
		}

		return true;
	}


	// Execute a console command
	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FOculusAutomationLatentCommand_Exec, FString, Command);

	bool FOculusAutomationLatentCommand_Exec::Update()
	{
		UE_LOG(LogHMD, Log, TEXT("FOculusAutomationLatentCommand_Exec('%s')"), *Command);

		if(GEngine->GameViewport)
			GEngine->GameViewport->Exec(NULL, *Command, *GLog);
		else
			GEngine->Exec(NULL, *Command);

		return true;
	}


	// Enable/Disable stereo rendering
	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FOculusAutomationLatentCommand_EnableStereo, bool, Enable);

	bool FOculusAutomationLatentCommand_EnableStereo::Update()
	{
		static uint32 Frames;
		static FFeedbackContext* SWarn;

		if(!Frames)
		{
			UE_LOG(LogHMD, Log, TEXT("FOculusAutomationLatentCommand_EnableStereo(%s) >>>"), Enable ? TEXT("true") : TEXT("false"));

			IStereoRendering* StereoRenderingDevice = GEngine->StereoRenderingDevice.Get();

			if(StereoRenderingDevice && StereoRenderingDevice->IsStereoEnabled() != Enable)
			{
				SWarn = GWarn;
				GWarn = nullptr;

				StereoRenderingDevice->EnableStereo(Enable);

				Frames = 3;
			}
		}
		else if(!--Frames)
		{
			GWarn = SWarn;
			SWarn = nullptr;

			UE_LOG(LogHMD, Log, TEXT("FOculusAutomationLatentCommand_EnableStereo(%s) <<<"), Enable ? TEXT("true") : TEXT("false"));
		}

		return !Frames;
	}
}


//-------------------------------------------------------------------------------------------------
// FOculusAutomationTest_Performance
//-------------------------------------------------------------------------------------------------

namespace
{
	static OculusRift::FPerformanceStats GPerformanceStats;

	DEFINE_LATENT_AUTOMATION_COMMAND(FOculusAutomationLatentCommand_Performance_StartFPSChart);

	bool FOculusAutomationLatentCommand_Performance_StartFPSChart::Update()
	{
		UE_LOG(LogHMD, Log, TEXT("FOculusAutomationLatentCommand_Performance_StartFPSChart()"));

		IHeadMountedDisplay* HeadMountedDisplay = GEngine->HMDDevice.Get();

		if(HeadMountedDisplay && HeadMountedDisplay->GetHMDDeviceType() == EHMDDeviceType::DT_OculusRift)
		{
			FOculusRiftHMD* OculusRiftHMD = (FOculusRiftHMD*) HeadMountedDisplay;
			GPerformanceStats = OculusRiftHMD->GetPerformanceStats();
		}		
		
		return true;
	}
	

	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FOculusAutomationLatentCommand_Performance_Play, AMatineeActor*, MatineeActor);

	bool FOculusAutomationLatentCommand_Performance_Play::Update() 
	{
		UE_LOG(LogHMD, Log, TEXT("FOculusAutomationLatentCommand_Performance_Play(%s)"), *MatineeActor->GetName());
		
		FFeedbackContext* Warn = GWarn;
		GWarn = nullptr;
		{
			MatineeActor->Stop();
			MatineeActor->bLooping = false;
			MatineeActor->bRewindOnPlay = true;
			MatineeActor->Play();
		}
		GWarn = Warn;

		return true;
	}


	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FOculusAutomationLatentCommand_Performance_Pause, AMatineeActor*, MatineeActor);

	bool FOculusAutomationLatentCommand_Performance_Pause::Update() 
	{
		UE_LOG(LogHMD, Log, TEXT("FOculusAutomationLatentCommand_Performance_Pause(%s)"), *MatineeActor->GetName());
		
		FFeedbackContext* Warn = GWarn;
		GWarn = nullptr;
		{
			MatineeActor->Pause();
		}
		GWarn = Warn;

		return true;
	}


	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FOculusAutomationLatentCommand_Performance_WaitUntilFinished, AMatineeActor*, MatineeActor);

	bool FOculusAutomationLatentCommand_Performance_WaitUntilFinished::Update()
	{
		return !MatineeActor->bIsPlaying;
	}


	DEFINE_LATENT_AUTOMATION_COMMAND(FOculusAutomationLatentCommand_Performance_StopFPSChart);

	bool FOculusAutomationLatentCommand_Performance_StopFPSChart::Update()
	{
		UE_LOG(LogHMD, Log, TEXT("FOculusAutomationLatentCommand_Performance_StopFPSChart()"));
		
		IHeadMountedDisplay* HeadMountedDisplay = GEngine->HMDDevice.Get();

		if(HeadMountedDisplay && HeadMountedDisplay->GetHMDDeviceType() == EHMDDeviceType::DT_OculusRift)
		{
			FOculusRiftHMD* OculusRiftHMD = (FOculusRiftHMD*) HeadMountedDisplay;
			FPerformanceStats PerformanceStats = OculusRiftHMD->GetPerformanceStats() - GPerformanceStats;

			if(PerformanceStats.Frames)
			{
				float AverageFPS = (float) ((double) PerformanceStats.Frames / PerformanceStats.Seconds);
				float TargetFPS = 1.0f / OculusRiftHMD->GetVsyncToNextVsync();

				if(AverageFPS < TargetFPS * 0.75f)
				{
					UE_LOG(LogHMD, Error, TEXT("Average FPS: %g"), AverageFPS);
				}
				else if(AverageFPS < TargetFPS * 0.95f)
				{
					UE_LOG(LogHMD, Warning, TEXT("Average FPS: %g"), AverageFPS);
				}
				else
				{
					UE_LOG(LogHMD, Display, TEXT("Average FPS: %g"), AverageFPS);
				}
			}
		}

		return true;
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOculusAutomationTest_Performance, "Oculus.Performance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::NonNullRHI | EAutomationTestFlags::PerfFilter)

bool FOculusAutomationTest_Performance::RunTest(const FString& Parameters)
{
	UE_LOG(LogHMD, Log, TEXT("FOculusAutomationTest_Performance()"));

	IStereoRendering* StereoRendering = GEngine->StereoRenderingDevice.Get();
	IHeadMountedDisplay* HeadMountedDisplay = GEngine->HMDDevice.Get();
	FOculusRiftHMD* OculusRiftHMD = (HeadMountedDisplay && HeadMountedDisplay->GetHMDDeviceType() == EHMDDeviceType::DT_OculusRift) ? (FOculusRiftHMD*) HeadMountedDisplay : NULL;

	if(OculusRiftHMD)
		UE_LOG(LogHMD, Display, TEXT("%s"), *OculusRiftHMD->GetVersionString());


	// Switch to stereo
	if(StereoRendering && !StereoRendering->IsStereoEnabled())
		ADD_LATENT_AUTOMATION_COMMAND(FOculusAutomationLatentCommand_EnableStereo(true));

	if(OculusRiftHMD)
		ADD_LATENT_AUTOMATION_COMMAND(FOculusAutomationLatentCommand_Exec(TEXT("hmdpos reset 180")));


	// Pause at start of first "OculusAnimation" MatineeActor sequence for a second
	for(TObjectIterator<AMatineeActor> It; It; ++It)
	{
		AMatineeActor* MatineeActor = *It;

		if(!MatineeActor || !MatineeActor->GetName().StartsWith(TEXT("OculusAutomation")))
			continue;

		ADD_LATENT_AUTOMATION_COMMAND(FOculusAutomationLatentCommand_Performance_Play(MatineeActor));
		ADD_LATENT_AUTOMATION_COMMAND(FOculusAutomationLatentCommand_Performance_Pause(MatineeActor));
		break;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FOculusAutomationLatentCommand_Wait(1.0));


	// Play all "OculusAutomation" MatineeActor sequences, recording FPS
	ADD_LATENT_AUTOMATION_COMMAND(FOculusAutomationLatentCommand_Performance_StartFPSChart());
	
	bool MatineeActorFound = false;

	for(TObjectIterator<AMatineeActor> It; It; ++It)
	{
		AMatineeActor* MatineeActor = *It;

		if(!MatineeActor || !MatineeActor->GetName().StartsWith(TEXT("OculusAutomation")))
			continue;

		MatineeActorFound = true;

		ADD_LATENT_AUTOMATION_COMMAND(FOculusAutomationLatentCommand_Performance_Play(MatineeActor));
		ADD_LATENT_AUTOMATION_COMMAND(FOculusAutomationLatentCommand_Performance_WaitUntilFinished(MatineeActor));
	}

	if(!MatineeActorFound)
	{
		UE_LOG(LogHMD, Log, TEXT("No MatineeActor with name starting with \"OculusAutomation\" found"));
		ADD_LATENT_AUTOMATION_COMMAND(FOculusAutomationLatentCommand_Wait(10.0));
	}

	ADD_LATENT_AUTOMATION_COMMAND(FOculusAutomationLatentCommand_Performance_StopFPSChart());


	// Restore video mode
	if(StereoRendering && !StereoRendering->IsStereoEnabled())
		ADD_LATENT_AUTOMATION_COMMAND(FOculusAutomationLatentCommand_EnableStereo(false));

	return true;
}



//-------------------------------------------------------------------------------------------------
// FOculusAutomationTest_Screenshots
// (Disabled until Engine can take screenshots correctly in Stereo mode.)
//-------------------------------------------------------------------------------------------------

#if 0
namespace
{
	struct SOculusAutomationLatentCommand_Screenshots_Screenshot
	{
		AMatineeActor* MatineeActor;
		float Position;
		FString ScreenshotName;

		SOculusAutomationLatentCommand_Screenshots_Screenshot(AMatineeActor* _MatineeActor, float _StartPosition, const FString& _ScreenshotName) : MatineeActor(_MatineeActor), Position(_StartPosition), ScreenshotName(_ScreenshotName) {}
	};


	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FOculusAutomationLatentCommand_Screenshots_Play, AMatineeActor*, MatineeActor);

	bool FOculusAutomationLatentCommand_Screenshots_Play::Update() 
	{
		FFeedbackContext* Warn = GWarn;
		GWarn = nullptr;
		{
			MatineeActor->Stop();
			MatineeActor->bLooping = false;
			MatineeActor->bRewindOnPlay = true;
			MatineeActor->Play();
			MatineeActor->Pause();
		}
		GWarn = Warn;

		return true;
	}


	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FOculusAutomationLatentCommand_Screenshots_Screenshot, SOculusAutomationLatentCommand_Screenshots_Screenshot, Context);

	bool FOculusAutomationLatentCommand_Screenshots_Screenshot::Update()
	{
		UE_LOG(LogHMD, Log, TEXT("FOculusAutomationLatentCommand_Screenshots_Screenshot(%s, %g)"), *Context.MatineeActor->GetName(), Context.Position);

		// Jump to position of cut
		FFeedbackContext* Warn = GWarn;
		GWarn = nullptr;
		{
			Context.MatineeActor->UpdateInterp(Context.Position, false, true);
		}
		GWarn = Warn;

		// Request screenshot
		FString FolderName, MapName;
		Context.MatineeActor->GetOutermost()->GetName().Split(TEXT("/"), &FolderName, &MapName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);

		FString ScreenshotName = FString::Printf(TEXT("Engine/Programs/UnrealFrontend/Saved/Automation/Oculus/Screenshots/%s/%s/%s/%4.2f/%s"),
			FApp::GetGameName(), *MapName, *Context.MatineeActor->GetName(), (double) Context.Position, *Context.ScreenshotName);

		UE_LOG(LogHMD, Display, TEXT("%s"), *ScreenshotName);
		FScreenshotRequest::RequestScreenshot(ScreenshotName, false, false);

		return true;
	}


	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FOculusAutomationLatentCommand_Screenshots_Stop, AMatineeActor*, MatineeActor);

	bool FOculusAutomationLatentCommand_Screenshots_Stop::Update() 
	{
		FFeedbackContext* Warn = GWarn;
		GWarn = nullptr;
		{
			MatineeActor->Stop();
		}
		GWarn = Warn;

		return true;
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOculusAutomationTest_Screenshots, "Oculus.Screenshots", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::NonNullRHI | EAutomationTestFlags::EngineFilter)

bool FOculusAutomationTest_Screenshots::RunTest(const FString& Parameters)
{
	UE_LOG(LogHMD, Log, TEXT("FOculusAutomationTest_Screenshots()"));

	IStereoRendering* StereoRendering = GEngine->StereoRenderingDevice.Get();
	IHeadMountedDisplay* HeadMountedDisplay = GEngine->HMDDevice.Get();
	FOculusRiftHMD* OculusRiftHMD = (HeadMountedDisplay && HeadMountedDisplay->GetHMDDeviceType() == EHMDDeviceType::DT_OculusRift) ? (FOculusRiftHMD*) HeadMountedDisplay : nullptr;

	if(OculusRiftHMD)
		UE_LOG(LogHMD, Display, TEXT("%s"), *OculusRiftHMD->GetVersionString());



	// Calculate ScreenshotName based on HardwareDetails and Changelist (if available). 
	FString ScreenshotName;
	{
		ScreenshotName = FPlatformProperties::PlatformName();

		FString HardwareDetails = FHardwareInfo::GetHardwareDetailsString();
		FString RHIString;
		FString RHILookup = NAME_RHI.ToString() + TEXT("=");

		if(FParse::Value(*HardwareDetails, *RHILookup, RHIString))
			ScreenshotName = ScreenshotName + TEXT("_") + RHIString;

		FString TextureFormatString;
		FString TextureFormatLookup = NAME_TextureFormat.ToString() + TEXT("=");

		if(FParse::Value(*HardwareDetails, *TextureFormatLookup, TextureFormatString))
			ScreenshotName = ScreenshotName + TEXT("_") + TextureFormatString;

		FString DeviceTypeString;
		FString DeviceTypeLookup = NAME_DeviceType.ToString() + TEXT("=");

		if(FParse::Value(*HardwareDetails, *DeviceTypeLookup, DeviceTypeString))
			ScreenshotName = ScreenshotName + TEXT("_") + DeviceTypeString;

		ScreenshotName += TEXT("/");

		if(GEngineVersion.GetChangelist())
		{
			ScreenshotName += FString::Printf(TEXT("%u"), GEngineVersion.GetChangelist());
		}
		else
		{
			int32 Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec;
			FPlatformTime::UtcTime(Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec);
			ScreenshotName += FString::Printf(TEXT("%0.2u%0.2u%0.2u%0.2u%0.2u"), (uint32) (Year % 100), (uint32) Month, (uint32) Day, (uint32) Hour, (uint32) Min);
		}

		ScreenshotName += TEXT(".png");
	}


	// Switch to stereo
	if(StereoRendering && !StereoRendering->IsStereoEnabled())
		ADD_LATENT_AUTOMATION_COMMAND(FOculusAutomationLatentCommand_EnableStereo(true));

	if(OculusRiftHMD)
		ADD_LATENT_AUTOMATION_COMMAND(FOculusAutomationLatentCommand_Exec(TEXT("hmdpos reset 180")));


	// Take screenshots at all "OculusAutomation" MatineeActor camera cuts
	bool MatineeActorFound = false;

	for(TObjectIterator<AMatineeActor> It; It; ++It)
	{
		AMatineeActor* MatineeActor = *It;

		if(!MatineeActor || !MatineeActor->GetName().StartsWith(TEXT("OculusAutomation")) || !MatineeActor->MatineeData)
			continue;

		MatineeActorFound = true;

		UInterpGroupDirector* DirGroup = MatineeActor->MatineeData->FindDirectorGroup();

		if(!DirGroup)
			continue;

		UInterpTrackDirector* DirTrack = DirGroup->GetDirectorTrack();

		if(!DirTrack || !DirTrack->CutTrack.Num())
			continue;

		ADD_LATENT_AUTOMATION_COMMAND(FOculusAutomationLatentCommand_Screenshots_Play(MatineeActor));

		for(int32 cutIndex = 0; cutIndex < DirTrack->CutTrack.Num(); cutIndex++)
		{
			const FDirectorTrackCut& Cut = DirTrack->CutTrack[cutIndex];
			ADD_LATENT_AUTOMATION_COMMAND(FOculusAutomationLatentCommand_Screenshots_Screenshot(SOculusAutomationLatentCommand_Screenshots_Screenshot(MatineeActor, Cut.Time, ScreenshotName)));
			ADD_LATENT_AUTOMATION_COMMAND(FOculusAutomationLatentCommand_Delay(2));
		}

		ADD_LATENT_AUTOMATION_COMMAND(FOculusAutomationLatentCommand_Screenshots_Stop(MatineeActor));
	}

	if(!MatineeActorFound)
		UE_LOG(LogHMD, Error, TEXT("No MatineeActor with name starting with \"OculusAutomation\" found"));


	// Restore video mode
	if(StereoRendering && !StereoRendering->IsStereoEnabled())
		ADD_LATENT_AUTOMATION_COMMAND(FOculusAutomationLatentCommand_EnableStereo(false));

	return true;
}
#endif

#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS