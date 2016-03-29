// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class libWebSockets : ModuleRules
{

    public libWebSockets(TargetInfo Target)
	{
		Type = ModuleType.External;
            string WebsocketPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "libWebSockets/libwebsockets/";
		    if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                PublicIncludePaths.Add(WebsocketPath + "include/");
			    PublicLibraryPaths.Add(WebsocketPath + "lib/x64/" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/");
			    PublicAdditionalLibraries.Add("websockets_static.lib");
			    PublicAdditionalLibraries.Add("ZLIB.lib");
		    }
            else if ( Target.Platform == UnrealTargetPlatform.Mac)
            {
                  PublicIncludePaths.Add(WebsocketPath + "include/");
		          PublicAdditionalLibraries.Add(WebsocketPath + "lib/Mac/libwebsockets.a");
            }
     }
}


