// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class WebSockets : ModuleRules
{

    public WebSockets(TargetInfo Target)
	{
		Type = ModuleType.External;
            string WebsocketPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "WebSockets/libwebsockets/";
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


