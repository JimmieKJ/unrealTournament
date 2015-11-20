// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WebRTC : ModuleRules
{
	public WebRTC(TargetInfo Target)
	{
		string WebRtcSdkVer = "trunk";
		string WebRtcSdkPlatform = "";		

		Type = ModuleType.External;

		if ((Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32) &&
			(WindowsPlatform.GetVisualStudioCompilerVersionName() == "2013" || WindowsPlatform.GetVisualStudioCompilerVersionName() == "2015")) // @todo samz - VS2012 libs
		{
			WebRtcSdkPlatform = "win";
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			WebRtcSdkPlatform = "mac";
		}
		else if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			WebRtcSdkPlatform = "ps4";
		}

		if (WebRtcSdkPlatform.Length > 0)
		{
			Definitions.Add("WITH_XMPP_JINGLE=1");

			string WebRtcSdkPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "WebRTC/sdk_" + WebRtcSdkVer + "_" + WebRtcSdkPlatform;
			
			PublicSystemIncludePaths.Add(WebRtcSdkPath + "/include");

			if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
			{
				Definitions.Add("WEBRTC_WIN=1");

				string ConfigPath = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ? "/Debug" : "/Release";
				string PlatformLibPath = Target.Platform == UnrealTargetPlatform.Win64 ? "/lib/Win64" : "/lib/Win32";
				string VSPath = "/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();

				PublicLibraryPaths.Add(WebRtcSdkPath + PlatformLibPath + VSPath + ConfigPath);

				PublicAdditionalLibraries.Add("rtc_base.lib");
				PublicAdditionalLibraries.Add("rtc_base_approved.lib");
				PublicAdditionalLibraries.Add("rtc_xmllite.lib");
				PublicAdditionalLibraries.Add("rtc_xmpp.lib");
				PublicAdditionalLibraries.Add("boringssl.lib");
				PublicAdditionalLibraries.Add("expat.lib"); 
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				Definitions.Add("WEBRTC_MAC=1");
				Definitions.Add("WEBRTC_POSIX=1");

				string ConfigPath = (Target.Configuration == UnrealTargetConfiguration.Debug) ? "/Debug" : "/Release";
				string LibPath = WebRtcSdkPath + "/lib" + ConfigPath;

				PublicAdditionalLibraries.Add(LibPath + "/librtc_base.a");
				PublicAdditionalLibraries.Add(LibPath + "/librtc_base_approved.a");
				PublicAdditionalLibraries.Add(LibPath + "/librtc_xmllite.a");
				PublicAdditionalLibraries.Add(LibPath + "/librtc_xmpp.a");
				PublicAdditionalLibraries.Add(LibPath + "/libboringssl.a");
				PublicAdditionalLibraries.Add(LibPath + "/libexpat.a");
			}
			else if (Target.Platform == UnrealTargetPlatform.PS4)
			{
// 				Definitions.Add("WEBRTC_PS4=1");
// 				Definitions.Add("WEBRTC_POSIX=1");
// 				Definitions.Add("BSD=1");
// 				Definitions.Add("__native_client__=1");

				string ConfigPath = "/Release";
				if (Target.Configuration == UnrealTargetConfiguration.Debug)
				{
					Definitions.Add("_DEBUG");
					Definitions.Add("DYNAMIC_ANNOTATIONS_ENABLED=1");
					Definitions.Add("WTF_USE_DYNAMIC_ANNOTATIONS=1");

					ConfigPath = "/Debug";
				}
				else
				{
					Definitions.Add("NDEBUG");
					Definitions.Add("NVALGRIND");
					Definitions.Add("DYNAMIC_ANNOTATIONS_ENABLED=0");
				}

				Definitions.Add("V8_DEPRECATION_WARNINGS");
				Definitions.Add("NOMINMAX");
				Definitions.Add("PSAPI_VERSION=1");
				Definitions.Add("_CRT_RAND_S");
				Definitions.Add("CERT_CHAIN_PARA_HAS_EXTRA_FIELDS");
				Definitions.Add("_ATL_NO_OPENGL");
				Definitions.Add("_SECURE_ATL");
				Definitions.Add("_HAS_EXCEPTIONS=0");
				Definitions.Add("_WINSOCK_DEPRECATED_NO_WARNINGS");
				Definitions.Add("CHROMIUM_BUILD");
				Definitions.Add("CR_CLANG_REVISION=239674-1");
				Definitions.Add("USE_AURA=1");
				Definitions.Add("USE_ASH=1");
				Definitions.Add("USE_DEFAULT_RENDER_THEME=1");
				Definitions.Add("USE_LIBJPEG_TURBO=1");
				Definitions.Add("ENABLE_ONE_CLICK_SIGNIN");
				Definitions.Add("ENABLE_PRE_SYNC_BACKUP");
				Definitions.Add("ENABLE_REMOTING=1");
				Definitions.Add("ENABLE_WEBRTC=1");
				Definitions.Add("ENABLE_MEDIA_ROUTER=1");
				Definitions.Add("ENABLE_PEPPER_CDMS");
				Definitions.Add("ENABLE_CONFIGURATION_POLICY");
				Definitions.Add("ENABLE_NOTIFICATIONS");
				Definitions.Add("ENABLE_HIDPI=1");
				Definitions.Add("ENABLE_TOPCHROME_MD=1");
				Definitions.Add("DONT_EMBED_BUILD_METADATA");
				Definitions.Add("NO_TCMALLOC");
				Definitions.Add("ALLOCATOR_SHIM");
				Definitions.Add("__STD_C");
				Definitions.Add("_CRT_SECURE_NO_DEPRECATE");
				Definitions.Add("_SCL_SECURE_NO_DEPRECATE");
				Definitions.Add("NTDDI_VERSION=0x06030000");
				Definitions.Add("_USING_V110_SDK71_");
				Definitions.Add("ENABLE_TASK_MANAGER=1");
				Definitions.Add("ENABLE_EXTENSIONS=1");
				Definitions.Add("ENABLE_PLUGIN_INSTALLATION=1");
				Definitions.Add("ENABLE_PLUGINS=1");
				Definitions.Add("ENABLE_SESSION_SERVICE=1");
				Definitions.Add("ENABLE_THEMES=1");
				Definitions.Add("ENABLE_AUTOFILL_DIALOG=1");
				Definitions.Add("ENABLE_BACKGROUND=1");
				Definitions.Add("ENABLE_GOOGLE_NOW=1");
				Definitions.Add("CLD_VERSION=2");
				Definitions.Add("ENABLE_PRINTING=1");
				Definitions.Add("ENABLE_BASIC_PRINTING=1");
				Definitions.Add("ENABLE_PRINT_PREVIEW=1");
				Definitions.Add("ENABLE_SPELLCHECK=1");
				Definitions.Add("ENABLE_CAPTIVE_PORTAL_DETECTION=1");
				Definitions.Add("ENABLE_APP_LIST=1");
				Definitions.Add("ENABLE_SETTINGS_APP=1");
				Definitions.Add("ENABLE_SUPERVISED_USERS=1");
				Definitions.Add("ENABLE_MDNS=1");
				Definitions.Add("ENABLE_SERVICE_DISCOVERY=1");
				Definitions.Add("ENABLE_WIFI_BOOTSTRAPPING=1");
				Definitions.Add("V8_USE_EXTERNAL_STARTUP_DATA");
				Definitions.Add("FULL_SAFE_BROWSING");
				Definitions.Add("SAFE_BROWSING_CSD");
				Definitions.Add("SAFE_BROWSING_DB_LOCAL");
				Definitions.Add("SAFE_BROWSING_SERVICE");
				Definitions.Add("WEBRTC_RESTRICT_LOGGING");
				Definitions.Add("EXPAT_RELATIVE_PATH");
				Definitions.Add("WEBRTC_POSIX");
				Definitions.Add("BSD");
				Definitions.Add("WEBRTC_PS4");
				Definitions.Add("WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE");
				Definitions.Add("FEATURE_ENABLE_SSL");
				Definitions.Add("FEATURE_ENABLE_VOICEMAIL");
				Definitions.Add("FEATURE_ENABLE_PSTN");
				Definitions.Add("SSL_USE_OPENSSL");
				Definitions.Add("HAVE_OPENSSL_SSL_H");
				Definitions.Add("XML_STATIC");
				Definitions.Add("USE_LIBPCI=1");
				Definitions.Add("USE_OPENSSL=1");
				Definitions.Add("__STDC_CONSTANT_MACROS");
				Definitions.Add("__STDC_FORMAT_MACROS");

				string LibPath = WebRtcSdkPath + "/lib" + ConfigPath;

				PublicAdditionalLibraries.Add(LibPath + "/rtc_base.a");
				PublicAdditionalLibraries.Add(LibPath + "/rtc_base_approved.a");
				PublicAdditionalLibraries.Add(LibPath + "/rtc_xmllite.a");
				PublicAdditionalLibraries.Add(LibPath + "/rtc_xmpp.a");
				PublicAdditionalLibraries.Add(LibPath + "/boringssl.a");
				PublicAdditionalLibraries.Add(LibPath + "/expat.a");
			}
		}

    }
}
