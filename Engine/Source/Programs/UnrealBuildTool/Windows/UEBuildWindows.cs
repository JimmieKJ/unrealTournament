// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;
using System.Linq;

namespace UnrealBuildTool
{
    /// <summary>
    /// Available compiler toolchains on Windows platform
    /// </summary>
    public enum WindowsCompiler
    {
        /// Visual Studio 2012 (Visual C++ 11.0). No longer supported for building on Windows, but required for other platform toolchains.
        VisualStudio2012,

        /// Visual Studio 2013 (Visual C++ 12.0)
        VisualStudio2013,
    }


    public class WindowsPlatform : UEBuildPlatform
    {
        /// Property caching.
        private static WindowsCompiler? CachedCompiler;

        /// Version of the compiler toolchain to use on Windows platform
        public static WindowsCompiler Compiler
        {
            get
            {
                // Cache the result because Compiler is often used.
                if (CachedCompiler.HasValue)
                {
                    return CachedCompiler.Value;
                }

                // First check if Visual Sudio 2013 is installed.
                if (!String.IsNullOrEmpty(WindowsPlatform.GetVSComnToolsPath(WindowsCompiler.VisualStudio2013)))
                {
                    CachedCompiler = WindowsCompiler.VisualStudio2013;
                }
                // Finally assume 2013 is installed to defer errors somewhere else like VCToolChain
                else
                {
                    CachedCompiler = WindowsCompiler.VisualStudio2013;
                }

                return CachedCompiler.Value;
            }
        }

        /// True if we should use Clang/LLVM instead of MSVC to compile code on Windows platform
        public static readonly bool bCompileWithClang = false;

        /// When using Clang, enabling enables the MSVC-like "clang-cl" wrapper, otherwise we pass arguments to Clang directly
		public static readonly bool bUseVCCompilerArgs = !bCompileWithClang || false;

		/// True if we should use the Clang linker (LLD) when bCompileWithClang is enabled, otherwise we use the MSVC linker
		public static readonly bool bAllowClangLinker = bCompileWithClang && true;

        /// True if we're targeting Windows XP as a minimum spec.  In Visual Studio 2012 and higher, this may change how
        /// we compile and link the application (http://blogs.msdn.com/b/vcblog/archive/2012/10/08/10357555.aspx)
		/// This is a flag to determine we should support XP if possible from XML
		public static bool SupportWindowsXP;
		public static bool IsWindowsXPSupported()
		{
			return SupportWindowsXP;
		}

        /** True if VS EnvDTE is available (false when building using Visual Studio Express) */
        public static bool bHasVisualStudioDTE
        {
            get
            {
				// @todo clang: DTE #import doesn't work with Clang compiler
				if( bCompileWithClang )
				{
					return false;
				}

                try
                {
                    // Interrogate the Win32 registry
                    string DTEKey = (Compiler == WindowsCompiler.VisualStudio2013) ? "VisualStudio.DTE.12.0" : "VisualStudio.DTE.11.0";
                    return RegistryKey.OpenBaseKey(RegistryHive.ClassesRoot, RegistryView.Registry32).OpenSubKey(DTEKey) != null;
                }
                catch(Exception) 
                {
                    return false;
                }
            }
        }

		/** The current architecture */
		public override string GetActiveArchitecture()
		{
			return IsWindowsXPSupported() ? "_XP" : base.GetActiveArchitecture();
		}

        protected override SDKStatus HasRequiredManualSDKInternal()
        {
            return SDKStatus.Valid;
        }

        /**
         * Returns VisualStudio common tools path for current compiler.
         * 
         * @return Common tools path.
         */
        public static string GetVSComnToolsPath()
        {
            return GetVSComnToolsPath(Compiler);
        }

        /**
         * Returns VisualStudio common tools path for given compiler.
         * 
         * @param Compiler Compiler for which to return tools path.
         * 
         * @return Common tools path.
         */
        public static string GetVSComnToolsPath(WindowsCompiler Compiler)
        {
            int VSVersion;

            switch(Compiler)
            {
                case WindowsCompiler.VisualStudio2012:
                    VSVersion = 11;
                    break;
                case WindowsCompiler.VisualStudio2013:
                    VSVersion = 12;
                    break;
                default:
                    throw new NotSupportedException("Not supported compiler.");
            }

            string[] PossibleRegPaths = new string[] {
                @"Wow6432Node\Microsoft\VisualStudio",	// Non-express VS2013 on 64-bit machine.
                @"Microsoft\VisualStudio",				// Non-express VS2013 on 32-bit machine.
                @"Wow6432Node\Microsoft\WDExpress",		// Express VS2013 on 64-bit machine.
                @"Microsoft\WDExpress"					// Express VS2013 on 32-bit machine.
            };

            string VSPath = null;

            foreach(var PossibleRegPath in PossibleRegPaths)
            {
                VSPath = (string) Registry.GetValue(string.Format(@"HKEY_LOCAL_MACHINE\SOFTWARE\{0}\{1}.0", PossibleRegPath, VSVersion), "InstallDir", null);

                if(VSPath != null)
                {
                    break;
                }
            }

            if(VSPath == null)
            {
                return null;
            }

            return new DirectoryInfo(Path.Combine(VSPath, "..", "Tools")).FullName;
        }


        /**
         *	Register the platform with the UEBuildPlatform class
         */
        protected override void RegisterBuildPlatformInternal()
        {
            // Register this build platform for both Win64 and Win32
            Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Win64.ToString());
            UEBuildPlatform.RegisterBuildPlatform(UnrealTargetPlatform.Win64, this);
            UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Win64, UnrealPlatformGroup.Windows);
            UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Win64, UnrealPlatformGroup.Microsoft);

            Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Win32.ToString());
            UEBuildPlatform.RegisterBuildPlatform(UnrealTargetPlatform.Win32, this);
            UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Win32, UnrealPlatformGroup.Windows);
            UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Win32, UnrealPlatformGroup.Microsoft);
        }

        /**
         *	Retrieve the CPPTargetPlatform for the given UnrealTargetPlatform
         *
         *	@param	InUnrealTargetPlatform		The UnrealTargetPlatform being build
         *	
         *	@return	CPPTargetPlatform			The CPPTargetPlatform to compile for
         */
        public override CPPTargetPlatform GetCPPTargetPlatform(UnrealTargetPlatform InUnrealTargetPlatform)
        {
            switch (InUnrealTargetPlatform)
            {
                case UnrealTargetPlatform.Win32:
                    return CPPTargetPlatform.Win32;

                case UnrealTargetPlatform.Win64:
                    return CPPTargetPlatform.Win64;
            }
            throw new BuildException("WindowsPlatform::GetCPPTargetPlatform: Invalid request for {0}", InUnrealTargetPlatform.ToString());
        }

        /**
         *	Get the extension to use for the given binary type
         *	
         *	@param	InBinaryType		The binrary type being built
         *	
         *	@return	string				The binary extenstion (ie 'exe' or 'dll')
         */
        public override string GetBinaryExtension(UEBuildBinaryType InBinaryType)
        {
            switch (InBinaryType)
            {
                case UEBuildBinaryType.DynamicLinkLibrary: 
                    return ".dll";
                case UEBuildBinaryType.Executable:
                    return ".exe";
                case UEBuildBinaryType.StaticLibrary:
                    return ".lib";
                case UEBuildBinaryType.Object:
					if (!BuildConfiguration.bRunUnrealCodeAnalyzer)
					{
						return ".obj";
					}
					else
					{
						return @".includes";
					}
                case UEBuildBinaryType.PrecompiledHeader:
                    return ".pch";
            }
            return base.GetBinaryExtension(InBinaryType);
        }

        
        /// <summary>
        /// When using a Visual Studio compiler, returns the version name as a string
        /// </summary>
        /// <returns>The Visual Studio compiler version name (e.g. "2012")</returns>
        public static string GetVisualStudioCompilerVersionName()
        {
            switch( Compiler )
            { 
                case WindowsCompiler.VisualStudio2012:
                    return "2012";

                case WindowsCompiler.VisualStudio2013:
                    return "2013";

                default:
                    throw new BuildException( "Unexpected WindowsCompiler version for GetVisualStudioCompilerVersionName().  Either not using a Visual Studio compiler or switch block needs to be updated");
            }
        }


        /**
         *	Get the extension to use for debug info for the given binary type
         *	
         *	@param	InBinaryType		The binary type being built
         *	
         *	@return	string				The debug info extension (i.e. 'pdb')
         */
        public override string GetDebugInfoExtension( UEBuildBinaryType InBinaryType )
        {
            switch (InBinaryType)
            {
                case UEBuildBinaryType.DynamicLinkLibrary:
                case UEBuildBinaryType.Executable:
                    return ".pdb";
            }
            return "";
        }

        /**
         *	Whether incremental linking should be used
         *	
         *	@param	InPlatform			The CPPTargetPlatform being built
         *	@param	InConfiguration		The CPPTargetConfiguration being built
         *	
         *	@return	bool	true if incremental linking should be used, false if not
         */
        public override bool ShouldUseIncrementalLinking(CPPTargetPlatform Platform, CPPTargetConfiguration Configuration)
        {
            return (Configuration == CPPTargetConfiguration.Debug);
        }

        /**
         *	Whether PDB files should be used
         *	
         *	@param	InPlatform			The CPPTargetPlatform being built
         *	@param	InConfiguration		The CPPTargetConfiguration being built
         *	@param	bInCreateDebugInfo	true if debug info is getting create, false if not
         *	
         *	@return	bool	true if PDB files should be used, false if not
         */
        public override bool ShouldUsePDBFiles(CPPTargetPlatform Platform, CPPTargetConfiguration Configuration, bool bCreateDebugInfo)
        {
            // Only supported on PC.
            if (bCreateDebugInfo && ShouldUseIncrementalLinking(Platform, Configuration))
            {
                return true;
            }
            return false;
        }

        /**
         *	Whether the editor should be built for this platform or not
         *	
         *	@param	InPlatform		The UnrealTargetPlatform being built
         *	@param	InConfiguration	The UnrealTargetConfiguration being built
         *	@return	bool			true if the editor should be built, false if not
         */
        public override bool ShouldNotBuildEditor(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
        {
            return false;
        }

        public override bool BuildRequiresCookedData(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
        {
            return false;
        }

		public override bool HasDefaultBuildConfig(UnrealTargetPlatform Platform, string ProjectPath)
		{
			if (Platform == UnrealTargetPlatform.Win32)
			{
				string[] StringKeys = new string[] {
					"MinimumOSVersion"
				};

				// look up OS specific settings
				if (!DoProjectSettingsMatchDefault(Platform, ProjectPath, "/Script/WindowsTargetPlatform.WindowsTargetSettings",
					null, null, StringKeys))
				{
					return false;
				}
			}

			// check the base settings
			return base.HasDefaultBuildConfig(Platform, ProjectPath);
		}

		private void SetupXPSupportFromConfiguration()
		{
			string[] CmdLine = Environment.GetCommandLineArgs();

			bool SupportWindowsXPIfAvailable = false;
			SupportWindowsXPIfAvailable = CmdLine.Contains("-winxp", StringComparer.InvariantCultureIgnoreCase);

			// ...check if it was supported from a config.
			if (!SupportWindowsXPIfAvailable)
			{
				ConfigCacheIni Ini = new ConfigCacheIni(UnrealTargetPlatform.Win64, "Engine", UnrealBuildTool.GetUProjectPath());
				string MinimumOS;
				if (Ini.GetString("/Script/WindowsTargetPlatform.WindowsTargetSettings", "MinimumOSVersion", out MinimumOS))
				{
					if (string.IsNullOrEmpty(MinimumOS) == false)
					{
						SupportWindowsXPIfAvailable = MinimumOS == "MSOS_XP";
					}
				}
			}

			SupportWindowsXP = SupportWindowsXPIfAvailable;
		}

        public override void ResetBuildConfiguration(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
        {
			UEBuildConfiguration.bCompileICU = true;
			if (InPlatform == UnrealTargetPlatform.Win32)
			{
				SetupXPSupportFromConfiguration();
			}

        }

        public override void ValidateBuildConfiguration(CPPTargetConfiguration Configuration, CPPTargetPlatform Platform, bool bCreateDebugInfo)
        {
            if( WindowsPlatform.bCompileWithClang )
            {
				// @todo clang: Shared PCHs don't work on clang yet because the PCH will have definitions assigned to different values
				// than the consuming translation unit.  Unlike the warning in MSVC, this is a compile in Clang error which cannot be suppressed
                BuildConfiguration.bUseSharedPCHs = false;

	            // @todo clang: PCH files aren't supported by "clang-cl" yet (no /Yc support, and "-x c++-header" cannot be specified)
 				if( WindowsPlatform.bUseVCCompilerArgs )
				{
                	BuildConfiguration.bUsePCHFiles = false;
				}
            }
        }

        /**
         *	Validate the UEBuildConfiguration for this platform
         *	This is called BEFORE calling UEBuildConfiguration to allow setting 
         *	various fields used in that function such as CompileLeanAndMean...
         */
        public override void ValidateUEBuildConfiguration()
        {
            UEBuildConfiguration.bCompileICU = true;
        }

        public override void ModifyNewlyLoadedModule(UEBuildModule InModule, TargetInfo Target)
        {
            if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64))
            {
                bool bBuildShaderFormats = UEBuildConfiguration.bForceBuildShaderFormats;

                if (!UEBuildConfiguration.bBuildRequiresCookedData)
                {
                    if (InModule.ToString() == "TargetPlatform")
                    {
                        bBuildShaderFormats = true;
                    }
                }

                // allow standalone tools to use target platform modules, without needing Engine
                if (UEBuildConfiguration.bForceBuildTargetPlatforms)
                {
                    InModule.AddDynamicallyLoadedModule("WindowsTargetPlatform");
                    InModule.AddDynamicallyLoadedModule("WindowsNoEditorTargetPlatform");
                    InModule.AddDynamicallyLoadedModule("WindowsServerTargetPlatform");
                    InModule.AddDynamicallyLoadedModule("WindowsClientTargetPlatform");
					InModule.AddDynamicallyLoadedModule("DesktopTargetPlatform");
                }

                if (bBuildShaderFormats)
                {
                    InModule.AddDynamicallyLoadedModule("ShaderFormatD3D");
                    InModule.AddDynamicallyLoadedModule("ShaderFormatOpenGL");
                }

                if (InModule.ToString() == "D3D11RHI")
                {
                    // To enable platform specific D3D11 RHI Types
                    InModule.AddPrivateIncludePath("Runtime/Windows/D3D11RHI/Private/Windows");
                }
            }
        }

        /**
         *	Setup the target environment for building
         *	
         *	@param	InBuildTarget		The target being built
         */
        public override void SetUpEnvironment(UEBuildTarget InBuildTarget)
        {
            InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WIN32=1");

			// Should we enable Windows XP support
			if ((InBuildTarget.TargetType == TargetRules.TargetType.Program) && (GetCPPTargetPlatform(InBuildTarget.Platform) == CPPTargetPlatform.Win32))
			{
				// Check if the target has requested XP support.
				if (String.Equals(InBuildTarget.Rules.PreferredSubPlatform, "WindowsXP", StringComparison.InvariantCultureIgnoreCase))
				{
					SupportWindowsXP = true;
				}
			}

			if (IsWindowsXPSupported())
            {
                // Windows XP SP3 or higher required
                InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("_WIN32_WINNT=0x0502");
                InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WINVER=0x0502");
            }
            else
            {
                // Windows Vista or higher required
                InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("_WIN32_WINNT=0x0600");
                InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WINVER=0x0600");
            }
            InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_WINDOWS=1");

            String MorpheusShaderPath = Path.Combine(BuildConfiguration.RelativeEnginePath, "Shaders/PS4/PostProcessHMDMorpheus.usf");
            if (File.Exists(MorpheusShaderPath))
            {
                InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("HAS_MORPHEUS=1");
                InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("MORPHEUS_120HZ=0");

				//on PS4 the SDK now handles distortion correction.  On PC we will still have to handle it manually,
				//but we require SDK changes before we can get the required data.  For the moment, no platform does in-engine morpheus distortion
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("MORPHEUS_ENGINE_DISTORTION=1");				
            }

            if (InBuildTarget.Rules != null)
            {
                // Explicitly exclude the MS C++ runtime libraries we're not using, to ensure other libraries we link with use the same
                // runtime library as the engine.
				bool bUseDebugCRT = InBuildTarget.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT;
				if(!InBuildTarget.Rules.bUseStaticCRT || bUseDebugCRT)
				{
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCMT");
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCPMT");
				}
				if(!InBuildTarget.Rules.bUseStaticCRT || !bUseDebugCRT)
				{
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCMTD");
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCPMTD");
				}
				if (InBuildTarget.Rules.bUseStaticCRT || bUseDebugCRT)
				{
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("MSVCRT");
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("MSVCPRT");
				}
				if(InBuildTarget.Rules.bUseStaticCRT || !bUseDebugCRT)
				{
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("MSVCRTD");
					InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("MSVCPRTD");
				}
                InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBC");
                InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCP");
                InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCD");
                InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("LIBCPD");

                //@todo ATL: Currently, only VSAccessor requires ATL (which is only used in editor builds)
                // When compiling games, we do not want to include ATL - and we can't when compiling Rocket games
                // due to VS 2012 Express not including ATL.
                // If more modules end up requiring ATL, this should be refactored into a BuildTarget flag (bNeedsATL)
                // that is set by the modules the target includes to allow for easier tracking.
                // Alternatively, if VSAccessor is modified to not require ATL than we should always exclude the libraries.
                if (InBuildTarget.ShouldCompileMonolithic() && (InBuildTarget.Rules != null) &&
                    (TargetRules.IsGameType(InBuildTarget.TargetType)) && (TargetRules.IsEditorType(InBuildTarget.TargetType) == false))
                {
                    InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("atl");
                    InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("atls");
                    InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("atlsd");
                    InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("atlsn");
                    InBuildTarget.GlobalLinkEnvironment.Config.ExcludedLibraries.Add("atlsnd");
                }

                // Add the library used for the delayed loading of DLLs.
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("delayimp.lib");

                //@todo - remove once FB implementation uses Http module
                if (UEBuildConfiguration.bCompileAgainstEngine)
                {
                    // link against wininet (used by FBX and Facebook)
                    InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("wininet.lib");
                }

                // Compile and link with Win32 API libraries.
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("rpcrt4.lib");
                //InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("wsock32.lib");
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("ws2_32.lib");
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("dbghelp.lib");
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("comctl32.lib");
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("Winmm.lib");
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("kernel32.lib");
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("user32.lib");
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("gdi32.lib");
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("winspool.lib");
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("comdlg32.lib");
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("advapi32.lib");
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("shell32.lib");
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("ole32.lib");
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("oleaut32.lib");
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("uuid.lib");
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("odbc32.lib");
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("odbccp32.lib");
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("netapi32.lib");
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("iphlpapi.lib");
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("setupapi.lib"); //  Required for access monitor device enumeration

				// Windows Vista/7 Desktop Windows Manager API for Slate Windows Compliance
				if (IsWindowsXPSupported() == false)		// Windows XP does not support DWM
                {
                    InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("dwmapi.lib");
                }

                // IME
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalLibraries.Add("imm32.lib");

                // Setup assembly path for .NET modules.  This will only be used when CLR is enabled. (CPlusPlusCLR module types)
                InBuildTarget.GlobalCompileEnvironment.Config.SystemDotNetAssemblyPaths.Add(
                    Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86) +
                    "/Reference Assemblies/Microsoft/Framework/.NETFramework/v4.0");

                // Setup default assemblies for .NET modules.  These will only be used when CLR is enabled. (CPlusPlusCLR module types)
                InBuildTarget.GlobalCompileEnvironment.Config.FrameworkAssemblyDependencies.Add("System.dll");
                InBuildTarget.GlobalCompileEnvironment.Config.FrameworkAssemblyDependencies.Add("System.Data.dll");
                InBuildTarget.GlobalCompileEnvironment.Config.FrameworkAssemblyDependencies.Add("System.Drawing.dll");
                InBuildTarget.GlobalCompileEnvironment.Config.FrameworkAssemblyDependencies.Add("System.Xml.dll");
                InBuildTarget.GlobalCompileEnvironment.Config.FrameworkAssemblyDependencies.Add("System.Management.dll");
                InBuildTarget.GlobalCompileEnvironment.Config.FrameworkAssemblyDependencies.Add("System.Windows.Forms.dll");
                InBuildTarget.GlobalCompileEnvironment.Config.FrameworkAssemblyDependencies.Add("WindowsBase.dll");
            }

            // Disable Simplygon support if compiling against the NULL RHI.
            if (InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Contains("USE_NULL_RHI=1"))
            {
                UEBuildConfiguration.bCompileSimplygon = false;
            }

            // For 64-bit builds, we'll forcibly ignore a linker warning with DirectInput.  This is
            // Microsoft's recommended solution as they don't have a fixed .lib for us.
            if (InBuildTarget.Platform == UnrealTargetPlatform.Win64)
            {
                InBuildTarget.GlobalLinkEnvironment.Config.AdditionalArguments += " /ignore:4078";
            }
        }

        /**
         *	Setup the configuration environment for building
         *	
         *	@param	InBuildTarget		The target being built
         */
        public override void SetUpConfigurationEnvironment(UEBuildTarget InBuildTarget)
        {
            // Determine the C++ compile/link configuration based on the Unreal configuration.
            CPPTargetConfiguration CompileConfiguration;
            //@todo SAS: Add a true Debug mode!
            UnrealTargetConfiguration CheckConfig = InBuildTarget.Configuration;
            switch (CheckConfig)
            {
                default:
                case UnrealTargetConfiguration.Debug:
                    CompileConfiguration = CPPTargetConfiguration.Debug;
                    if( BuildConfiguration.bDebugBuildsActuallyUseDebugCRT )
                    { 
                        InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("_DEBUG=1"); // the engine doesn't use this, but lots of 3rd party stuff does
                    }
                    else
                    {
                        InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("NDEBUG=1"); // the engine doesn't use this, but lots of 3rd party stuff does
                    }
                    InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("UE_BUILD_DEBUG=1");
                    break;
                case UnrealTargetConfiguration.DebugGame:
                    // Default to Development; can be overriden by individual modules.
                case UnrealTargetConfiguration.Development:
                    CompileConfiguration = CPPTargetConfiguration.Development;
                    InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("NDEBUG=1"); // the engine doesn't use this, but lots of 3rd party stuff does
                    InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("UE_BUILD_DEVELOPMENT=1");
                    break;
                case UnrealTargetConfiguration.Shipping:
                    CompileConfiguration = CPPTargetConfiguration.Shipping;
                    InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("NDEBUG=1"); // the engine doesn't use this, but lots of 3rd party stuff does
                    InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("UE_BUILD_SHIPPING=1");
                    break;
                case UnrealTargetConfiguration.Test:
                    CompileConfiguration = CPPTargetConfiguration.Shipping;
                    InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("NDEBUG=1"); // the engine doesn't use this, but lots of 3rd party stuff does
                    InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("UE_BUILD_TEST=1");
                    break;
            }

            // Set up the global C++ compilation and link environment.
            InBuildTarget.GlobalCompileEnvironment.Config.Target.Configuration = CompileConfiguration;
            InBuildTarget.GlobalLinkEnvironment.Config.Target.Configuration    = CompileConfiguration;

            // Create debug info based on the heuristics specified by the user.
            InBuildTarget.GlobalCompileEnvironment.Config.bCreateDebugInfo =
                !BuildConfiguration.bDisableDebugInfo && ShouldCreateDebugInfo(InBuildTarget.Platform, CheckConfig);

            // NOTE: Even when debug info is turned off, we currently force the linker to generate debug info
            //       anyway on Visual C++ platforms.  This will cause a PDB file to be generated with symbols
            //       for most of the classes and function/method names, so that crashes still yield somewhat
            //       useful call stacks, even though compiler-generate debug info may be disabled.  This gives
            //       us much of the build-time savings of fully-disabled debug info, without giving up call
            //       data completely.
            InBuildTarget.GlobalLinkEnvironment.Config.bCreateDebugInfo = true;
        }

        /**
         *	Whether this platform should create debug information or not
         *	
         *	@param	InPlatform			The UnrealTargetPlatform being built
         *	@param	InConfiguration		The UnrealTargetConfiguration being built
         *	
         *	@return	bool				true if debug info should be generated, false if not
         */
        public override bool ShouldCreateDebugInfo(UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration)
        {
            switch (Configuration)
            {
                case UnrealTargetConfiguration.Development: 
                case UnrealTargetConfiguration.Shipping: 
                case UnrealTargetConfiguration.Test: 
                    return !BuildConfiguration.bOmitPCDebugInfoInDevelopment;
                case UnrealTargetConfiguration.DebugGame:
                case UnrealTargetConfiguration.Debug:
                default: 
                    return true;
            };
        }

        /**
         *	Return whether this platform has uniquely named binaries across multiple games
         */
        public override bool HasUniqueBinaries()
        {
            // Windows applications have many shared binaries between games
            return false;
        }
    }
}
