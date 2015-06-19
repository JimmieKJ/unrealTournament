// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EditorLiveStreamingSettings.generated.h"


/** Describes the available preset resolutions for web camera video with the editor's live streaming feature */
UENUM()
namespace EEditorLiveStreamingWebCamResolution
{
	enum Type
	{
		Normal_320x240 UMETA( DisplayName="320 x 240 (4:3)" ),
		Wide_320x180 UMETA( DisplayName="320 x 180 (16:9)" ),

		Normal_640x480 UMETA( DisplayName="640 x 480 (4:3)" ),
		Wide_640x360 UMETA( DisplayName="640 x 360 (16:9)" ),

		Normal_800x600 UMETA( DisplayName="800 x 600 (4:3)" ),
		Wide_800x450 UMETA( DisplayName="800 x 450 (16:9)" ),

		Normal_1024x768 UMETA( DisplayName="1024 x 768 (4:3)" ),
		Wide_1024x576 UMETA( DisplayName="1024 x 576 (16:9)" ),
		
		Normal_1080x810 UMETA( DisplayName="1080 x 810 (4:3)" ),
		Wide_1080x720 UMETA( DisplayName="1080 x 720 (16:9)" ),

		Normal_1280x960 UMETA( DisplayName="1280 x 960 (4:3)" ),
		Wide_1280x720 UMETA( DisplayName="1280 x 720 (16:9, 720p)" ),

		Normal_1920x1440 UMETA( DisplayName="1920 x 1440 (4:3)" ),
		Wide_1920x1080 UMETA( DisplayName="1920 x 1080 (16:9, 1080p)" ),
	};
}


/** Holds preferences for the editor live streaming features */
UCLASS(config=EditorSettings)
class UEditorLiveStreamingSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Frame rate to stream video from the editor at when broadcasting to services like Twitch. */
	UPROPERTY(Config, EditAnywhere, Category=EditorLiveStreaming, Meta=(UIMin=5,ClampMin=1,UIMax=60))
	int32 FrameRate;

	/** How much to scale the broadcast video resolution down to reduce streaming bandwidth.  We recommend broadcasting at resolutions of 1280x720 or lower.  Some live streaming providers will not be able to transcode your video to a lower resolution, so using a high resolution stream may prevent low-bandwidth users from having a good viewing experience.*/
	UPROPERTY(Config, EditAnywhere, Category=EditorLiveStreaming, Meta=(ClampMin=0.1,ClampMax=1.0))
	float ScreenScaling;

	/** By default, we only broadcast video from your primary monitor's work area (excludes the task bar area.)  Turning off this feature enables broadcasting from all monitors, covering your entire usable desktop area.  This may greatly increase the video resolution of your stream, so we recommend leaving this option turned off. */
	UPROPERTY(Config, EditAnywhere, Category=EditorLiveStreaming )
	bool bPrimaryMonitorOnly;

	/** If enabled, video from your web camera will be captured and displayed in the editor while broadcasting, so that your viewers can see your presence. */
	UPROPERTY(Config, EditAnywhere, Category=EditorLiveStreaming )
	bool bEnableWebCam;

	/** The camera video resolution that you'd prefer.  The camera may not support the exact resolution you specify, so we'll try to find the best match automatically */
	UPROPERTY(Config, EditAnywhere, Category=EditorLiveStreaming, Meta=(EditCondition="bEnableWebCam") )
	TEnumAsByte<EEditorLiveStreamingWebCamResolution::Type> WebCamResolution;

	/** You can enable this to flip the web cam image horizontally so that it looks more like a mirror */
	UPROPERTY(Config, EditAnywhere, Category=EditorLiveStreaming )
	bool bMirrorWebCamImage;

	/** Enables broadcast of audio being played by your computer, such as in-game sounds */
	UPROPERTY(Config, EditAnywhere, Category=EditorLiveStreaming )
	bool bCaptureAudioFromComputer;

	/** Enables broadcast of audio from your default microphone recording device */
	UPROPERTY(Config, EditAnywhere, Category=EditorLiveStreaming )
	bool bCaptureAudioFromMicrophone;


protected:

	/** UObject overrides */
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) override;
};


