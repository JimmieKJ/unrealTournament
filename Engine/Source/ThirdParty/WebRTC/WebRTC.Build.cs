// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class WebRTC : ModuleRules
{
	public WebRTC(TargetInfo Target)
	{
		Type = ModuleType.External;

		bool bShouldUseWebRTC = false;
		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
			bShouldUseWebRTC = true;
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			bShouldUseWebRTC = true;
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			bShouldUseWebRTC = true;
		}
		else if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			bShouldUseWebRTC = true;
		}

		if (bShouldUseWebRTC)
		{
			Definitions.Add("WITH_XMPP_JINGLE=1");

			string VS2013Friendly_WebRtcSdkPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "WebRTC/VS2013_friendly";
			string LinuxTrunk_WebRtcSdkPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "WebRTC/sdk_trunk_linux";

			string PlatformSubdir = (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32") ? "Win32" :
				Target.Platform.ToString();
			string ConfigPath = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ? "Debug" : "Release";


			if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 ||
				(Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32"))
			{
				Definitions.Add("WEBRTC_WIN=1");

				string VisualStudioVersionFolder = "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
				
				string IncludePath = Path.Combine(VS2013Friendly_WebRtcSdkPath, "include", PlatformSubdir, VisualStudioVersionFolder);
				PublicSystemIncludePaths.Add(IncludePath);

				string LibraryPath = Path.Combine(VS2013Friendly_WebRtcSdkPath, "lib", PlatformSubdir, VisualStudioVersionFolder, ConfigPath);

				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "rtc_base.lib"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "rtc_base_approved.lib"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "rtc_xmllite.lib"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "rtc_xmpp.lib"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "expat.lib"));
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				Definitions.Add("WEBRTC_MAC=1");
				Definitions.Add("WEBRTC_POSIX=1");

				string IncludePath = Path.Combine(VS2013Friendly_WebRtcSdkPath, "include", PlatformSubdir);
				PublicSystemIncludePaths.Add(IncludePath);

				string LibraryPath = Path.Combine(VS2013Friendly_WebRtcSdkPath, "lib", PlatformSubdir, ConfigPath);

				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "librtc_base.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "librtc_base_approved.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "librtc_xmllite.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "librtc_xmpp.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "libexpat.a"));
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				Definitions.Add("WEBRTC_LINUX=1");
				Definitions.Add("WEBRTC_POSIX=1");

				string IncludePath = Path.Combine(LinuxTrunk_WebRtcSdkPath, "include");
				PublicSystemIncludePaths.Add(IncludePath);

				// This is slightly different than the other platforms
				string LibraryPath = Path.Combine(LinuxTrunk_WebRtcSdkPath, "lib", Target.Architecture, ConfigPath);

				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "librtc_base.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "librtc_base_approved.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "librtc_xmllite.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "librtc_xmpp.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "libexpat.a"));
			}
			else if (Target.Platform == UnrealTargetPlatform.PS4)
			{
				Definitions.Add("WEBRTC_ORBIS");
				Definitions.Add("FEATURE_ENABLE_SSL");
				Definitions.Add("SSL_USE_OPENSSL");
				Definitions.Add("EXPAT_RELATIVE_PATH");

                string IncludePath = Path.Combine(VS2013Friendly_WebRtcSdkPath, "include", PlatformSubdir);
                PublicSystemIncludePaths.Add(IncludePath);

                string LibraryPath = Path.Combine(VS2013Friendly_WebRtcSdkPath, "lib", PlatformSubdir, ConfigPath);

				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "rtc_base.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "rtc_base_approved.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "rtc_xmllite.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "rtc_xmpp.a"));
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "expat.a"));
			}

			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
		}

	}
}
