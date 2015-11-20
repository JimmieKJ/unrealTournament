// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using System.IO;
using System.Text.RegularExpressions;
using System.Reflection;
using System.Diagnostics;
using System.Web;
using System.Security.Cryptography;
using System.CodeDom.Compiler;
using System.Runtime.InteropServices;
using DoxygenLib;

namespace APIDocTool
{
	public class APISnippets
	{
		public static string SnippetTextDirectory { get; private set; }
		public const int FileBufferChunkSize = 2048;

		public APISnippets()
		{
			SnippetDictionary = new Dictionary<string, APISnippet>();
		}

		public static void SetSnippetTextDirectory(string DestinationDirectory)
		{
			SnippetTextDirectory = DestinationDirectory;
		}

		public static void CleanAllFiles()
		{
			if (Directory.Exists(SnippetTextDirectory))
			{
				Directory.Delete(SnippetTextDirectory, true);
			}
		}

		static string SnippetFilenameForKey(string Key)
		{
			return (SnippetTextDirectory + "\\" + Key.Replace(':', '-') + ".txt");
		}

		public static List<string> LoadSnippetTextForSymbol(string Key)
		{
			string Filename = SnippetFilenameForKey(Key);
			if (!File.Exists(Filename))
			{
				return null;
			}
			FileInfo fileInfo = new FileInfo(Filename);
			List<string> returnList = new List<string>(((int)fileInfo.Length / FileBufferChunkSize) + 1);
			string line;
			using (FileStream fileStream = File.Open(Filename, FileMode.Open, FileAccess.Read))
			using (BufferedStream bufferedStream = new BufferedStream(fileStream))
			using (StreamReader streamReader = new StreamReader(bufferedStream))
			{
				while ((line = streamReader.ReadLine()) != null)
				{
					returnList.Add(line);
				}
			}
			return returnList;
		}

		public bool WriteSnippetsToFiles()
		{
			if (!Directory.Exists(SnippetTextDirectory))
			{
				Directory.CreateDirectory(SnippetTextDirectory);
			}
			foreach (string Key in SnippetDictionary.Keys)
			{
				using (StreamWriter OutStream = new StreamWriter(SnippetFilenameForKey(Key)))
				{
					OutStream.AutoFlush = true;
					SnippetDictionary[Key].WriteToStream(OutStream);
					OutStream.Close();
				}
			}
			return true;
		}

		public void AddSnippet(string PageName)
		{
			APISnippet ExistingSnippet;
			if (SnippetDictionary.TryGetValue(PageName, out ExistingSnippet))
			{
				//Update this entry with some newlines.
				ExistingSnippet.AddSnippetText(Environment.NewLine + Environment.NewLine);
			}
			else
			{
				//Create a new entry.
				SnippetDictionary.Add(PageName, new APISnippet());
			}
		}

		public void FinishCurrentSnippet(string PageName)
		{
			APISnippet ExistingSnippet;
			if (SnippetDictionary.TryGetValue(PageName, out ExistingSnippet))
			{
				ExistingSnippet.FinishCurrentSnippet();
			}
			else
			{
				Console.WriteLine("WARNING: Called FinishCurrentSnippet() with PageName " + PageName + ", but no snippet by that name was found.");
			}
		}

		public bool AddSnippetText(string PageName, string NewSnippetText)
		{
			APISnippet ExistingSnippet;
			if (SnippetDictionary.TryGetValue(PageName, out ExistingSnippet))
			{
				ExistingSnippet.AddSnippetText(NewSnippetText);
				return true;
			}
			return false;
		}

		public bool AddSeeText(string PageName, string NewSeeText)
		{
			APISnippet ExistingSnippet;
			if (SnippetDictionary.TryGetValue(PageName, out ExistingSnippet))
			{
				ExistingSnippet.AddSeeText(NewSeeText);
				return true;
			}
			return false;
		}

		Dictionary<string, APISnippet> SnippetDictionary;

		class APISnippet
		{
			public APISnippet()
			{
				SnippetText = new List<string>(1);
				PendingSeeText = new List<string>(4);
			}

			public void AddSnippetText(string NewSnippetText)
			{
				SnippetText.Add(NewSnippetText);
			}

			public void AddSeeText(string NewSeeText)
			{
				PendingSeeText.Add(NewSeeText);
			}

			public void FinishCurrentSnippet()
			{
				if (PendingSeeText.Count > 0)
				{
					AddSnippetText("See Also:" + Environment.NewLine);
					foreach (string SeeString in PendingSeeText)
					{
						AddSnippetText(SeeString);
					}
					PendingSeeText.Clear();
				}
			}

			public void WriteToStream(StreamWriter OutStream)
			{
				foreach (string SnippetString in SnippetText)
				{
					OutStream.Write(SnippetString);
					OutStream.Flush();
				}
				OutStream.WriteLine();
			}

			//Multiple snippets can exist per entry. Each list element should be a complete snippet. We can format them, or the space between them, or sort them, after they're all harvested.
			public List<string> SnippetText { get; private set; }

			//See entries are written after the snippet code, so they're stored separately for convenience.
			public List<string> PendingSeeText { get; private set; }
		}
	}

    public class Program
	{
		[Flags]
		enum BuildActions
		{
			Clean = 0x01,
			Build = 0x02,
			Archive = 0x04,
		}

		[DllImport("kernel32.dll")]
		public static extern uint SetErrorMode(uint hHandle);

		public static IniFile Settings;
		public static HashSet<string> IgnoredFunctionMacros;
		public static HashSet<string> IncludedSourceFiles;

        public static String Availability = null;

        public static List<String> ExcludeDirectories = new List<String>();
        public static List<String> SourceDirectories = new List<String>();

		public const string TabSpaces = "&nbsp;&nbsp;&nbsp;&nbsp;";
        public const string APIFolder = "API";
		public const string BlueprintAPIFolder = "BlueprintAPI";

		public static bool bIndexOnly = false;
		public static bool bOutputPublish = false;

		static string[] ExcludeSourceDirectories =
		{
			"Apple",
			"Windows",
			"Win32",
//			"Win64",	// Need to allow Win64 now that the intermediate headers are in a Win64 directory
			"Mac",
			"XboxOne",
			"PS4",
			"IOS",
			"Android",
			"WinRT",
			"WinRT_ARM",
			"HTML5",
			"Linux",
			"TextureXboxOneFormat",
			"NoRedist",
			"NotForLicensees",
		};

		public static HashSet<string> ExcludeSourceDirectoriesHash = new HashSet<string>(ExcludeSourceDirectories.Select(x => x.ToLowerInvariant()));

		static string[] ExcludeSourceFiles = 
		{
			"*/CoreUObject/Classes/Object.h",
			"DelegateInstanceInterfaceImpl.inl",
		};

		static string[] DoxygenExpandedMacros = 
		{
		};

		static string[] DoxygenPredefinedMacros =
		{
			"UE_BUILD_DOCS=1",
			"DECLARE_FUNCTION(X)=void X(FFrame &Stack, void *Result)",

			// Compilers
			"DLLIMPORT=",
			"DLLEXPORT=",
			"PURE_VIRTUAL(Func,Extra)=;",
			"FORCEINLINE=",
			"MSVC_PRAGMA(X)=",
			"MS_ALIGN(X)= ",
			"GCC_ALIGN(X)= ",
			"SAFE_BOOL_OPERATORS(X)= ",
			"VARARGS=",
			"VARARG_DECL(FuncRet,StaticFuncRet,Return,FuncName,Pure,FmtType,ExtraDecl,ExtraCall)=FuncRet FuncName(ExtraDecl FmtType Fmt, ...)",
			"VARARG_BODY(FuncRet,FuncName,FmtType,ExtraDecl)=FuncRet FuncName(ExtraDecl FmtType Fmt, ...)",
			"VARARG_EXTRA(Arg)=Arg,",
			"PRAGMA_DISABLE_OPTIMIZATION=",
			"PRAGMA_ENABLE_OPTIMIZATION=",
			"NO_API= ",
			"OVERRIDE= ",
			"CDECL= ",
			"DEPRECATED(X,Y)= ",

			// Platform
			"PLATFORM_SUPPORTS_DRAW_MESH_EVENTS=1",
			"PLATFORM_SUPPORTS_VOICE_CAPTURE=1",

			// Features
			"DO_CHECK=1",
			"DO_GUARD_SLOW=1",
			"STATS=0",
			"ENABLE_VISUAL_LOG=1",
			"WITH_EDITOR=1",
			"WITH_NAVIGATION_GENERATOR=1",
			"WITH_APEX_CLOTHING=1",
			"WITH_CLOTH_COLLISION_DETECTION=1",
			"WITH_EDITORONLY_DATA=1",
			"WITH_PHYSX=1",
			"WITH_SUBSTEPPING=1",
			"WITH_HOT_RELOAD=1",
			"NAVOCTREE_CONTAINS_COLLISION_DATA=1",
			"SOURCE_CONTROL_WITH_SLATE=1",
			"MATCHMAKING_HACK_FOR_EGP_IE_HOSTING=1",
			"USE_REMOTE_INTEGRATION=1",
			"WITH_FANCY_TEXT=1",

			// Online subsystems
			"PACKAGE_SCOPE:=protected",

			// Hit proxies
			"DECLARE_HIT_PROXY()=public: static HHitProxyType * StaticGetType(); virtual HHitProxyType * GetType() const;",

			// Vertex factories
			"DECLARE_VERTEX_FACTORY_TYPE(FactoryClass)=public: static FVertexFactoryType StaticType; virtual FVertexFactoryType* GetType() const;",

			// Slate declarative syntax
			"SLATE_BEGIN_ARGS(WidgetType)=public: struct FArguments : public TSlateBaseNamedArgs<WidgetType> { typedef FArguments WidgetArgsType; FArguments()",
			"SLATE_USER_ARGS(WidgetType)=public: static TSharedRef<WidgetType> New(); struct FArguments; struct FArguments : public TSlateBaseNamedArgs<WidgetType>{ typedef FArguments WidgetArgsType; FArguments()",
			"HACK_SLATE_SLOT_ARGS(WidgetType)=public: struct FArguments : public TSlateBaseNamedArgs<WidgetType>{ typedef FArguments WidgetArgsType; FArguments()",
			"SLATE_ATTRIBUTE(AttrType, AttrName)=WidgetArgsType &AttrName( const TAttribute<AttrType>& InAttribute );",
			"SLATE_TEXT_ATTRIBUTE(AttrName)=WidgetArgsType &AttrName( const TAttribute<FText>& InAttribute ); WidgetArgsType &AttrName( const TAttribute<FString>& InAttribute );",
			"SLATE_ARGUMENT(ArgType, ArgName)=WidgetArgsType &ArgName(ArgType InArg);",
			"SLATE_TEXT_ARGUMENT(ArgName)=WidgetArgsType &ArgName(FString InArg); WidgetArgsType &ArgName(FText InArg);",
			"SLATE_STYLE_ARGUMENT(ArgType, ArgName)=WidgetArgsType &ArgName(const ArgType* InArg);",
			"SLATE_SUPPORTS_SLOT(SlotType)=WidgetArgsType &operator+(SlotType &SlotToAdd);",
			"SLATE_SUPPORTS_SLOT_WITH_ARGS(SlotType)=WidgetArgsType &operator+(SlotType::FArguments &ArgumentsForNewSlot);",
			"SLATE_NAMED_SLOT(DeclarationType, SlotName)=NamedSlotProperty<DeclarationType> SlotName();",
			"SLATE_EVENT(DelegateName,EventName)=WidgetArgsType& EventName( const DelegateName& InDelegate );",
			"SLATE_END_ARGS()=};",
			"DRAG_DROP_OPERATOR_TYPE(TYPE, BASE)=static const FString& GetTypeId(); virtual bool IsOfTypeImpl(const FString& Type) const;",

			// Rendering macros
			"IMPLEMENT_SHADER_TYPE(TemplatePrefix,ShaderClass,SourceFilename,FunctionName,Frequency)= ",
			"IMPLEMENT_SHADER_TYPE2(ShaderClass,Frequency)= ",
			"IMPLEMENT_SHADER_TYPE3(ShaderClass,Frequency)= ",

			// Stats
			"DEFINE_STAT(Stat)=",
			"DECLARE_STATS_GROUP(GroupDesc,GroupId)=",
			"DECLARE_STATS_GROUP_VERBOSE(GroupDesc,GroupId)=",
			"DECLARE_STATS_GROUP_COMPILED_OUT(GroupDesc,GroupId)=",
			"DECLARE_CYCLE_STAT_EXTERN(CounterName,StatId,GroupId, API)= ",
			"DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(CounterName,StatId,GroupId, API)= ",
			"DECLARE_DWORD_COUNTER_STAT_EXTERN(CounterName,StatId,GroupId, API)= ",
			"DECLARE_MEMORY_STAT_EXTERN(CounterName,StatId,GroupId, API)= ",

			// Steam
			"STEAM_CALLBACK(C,N,P,X)=void N(P*)",
			"STEAM_GAMESERVER_CALLBACK(C,N,P,X)=void N(P*)",

			// Log categories
			"DECLARE_LOG_CATEGORY_EXTERN(CategoryName, DefaultVerbosity, CompileTimeVerbosity)= ",

			// Delegates
			"FUNC_PARAM_MEMBERS=",
			"DECLARE_DERIVED_EVENT(X,Y,Z)=",
			"DELEGATE_DEPRECATED(X)=",
		};

		static Program()
		{
			List<string> DelegateMacros = new List<string>();

			string ArgumentList = "";
			string NamedArgumentList = "";
			string[] Suffixes = { "NoParams", "OneParam", "TwoParams", "ThreeParams", "FourParams", "FiveParams", "SixParams", "SevenParams", "EightParams" };

			for (int Idx = 0; Idx < Suffixes.Length; Idx++)
			{
				string Suffix = Suffixes[Idx];
				string MacroSuffix = (Idx == 0) ? "" : "_" + Suffix;

				DelegateMacros.Add(String.Format("DECLARE_DELEGATE{0}(Name{1})=typedef TBaseDelegate_{2}<void{1}> Name;", MacroSuffix, ArgumentList, Suffix));
				DelegateMacros.Add(String.Format("DECLARE_DELEGATE_RetVal{0}(RT, Name{1})=typedef TBaseDelegate_{2}<RT{1}> Name;", MacroSuffix, ArgumentList, Suffix));
				DelegateMacros.Add(String.Format("DECLARE_EVENT{0}(OT, Name{1})=class Name : public TBaseMulticastDelegate_{2}<void{1}>{{ }}", MacroSuffix, ArgumentList, Suffix));
				DelegateMacros.Add(String.Format("DECLARE_MULTICAST_DELEGATE{0}(Name{1})=typedef TMulticastDelegate_{2}<void{1}> Name;", MacroSuffix, ArgumentList, Suffix));
				DelegateMacros.Add(String.Format("DECLARE_DYNAMIC_DELEGATE{0}(Name{1})=class Name {{ }}", MacroSuffix, NamedArgumentList));
				DelegateMacros.Add(String.Format("DECLARE_DYNAMIC_DELEGATE_RetVal{0}(RT, Name{1})=class Name {{ }}", MacroSuffix, NamedArgumentList));
				DelegateMacros.Add(String.Format("DECLARE_DYNAMIC_MULTICAST_DELEGATE{0}(Name{1})=class Name {{ }};", MacroSuffix, NamedArgumentList));

				ArgumentList += String.Format(", T{0}", Idx + 1);
				NamedArgumentList += String.Format(", T{0}, N{0}", Idx + 1);
			}

			DoxygenPredefinedMacros = DoxygenPredefinedMacros.Union(DelegateMacros).ToArray();
		}

		static int Main(string[] Arguments)
        {
			List<string> ArgumentList = new List<string>(Arguments);

			// Get the default paths
            string EngineDir = ParseArgumentDirectory(ArgumentList, "-enginedir=", Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), "..\\..\\..\\..\\..\\..\\..\\.."));
			string IntermediateDir = ParseArgumentDirectory(ArgumentList, "-intermediatedir=", Path.Combine(EngineDir, "Intermediate\\Documentation"));
			string DocumentationDir = ParseArgumentDirectory(ArgumentList, "-documentationdir=", Path.Combine(EngineDir, "Documentation"));
			string SamplesDir = ParseArgumentDirectory(ArgumentList, "-samplesdir=", Path.Combine(EngineDir, "..\\Samples"));

			// Check if we're just building an index, no actual content
			bool bIndexOnly = ArgumentList.Remove("-indexonly");

			// Check if we're running on a build machine
			bool bBuildMachine = ArgumentList.Remove("-buildmachine");

			// Remove all the filter arguments
			List<string> Filters = new List<string>();
			for (int Idx = 0; Idx < ArgumentList.Count; Idx++)
			{
				const string FilterPrefix = "-filter=";
				if (ArgumentList[Idx].StartsWith(FilterPrefix))
				{
					Filters.AddRange(ArgumentList[Idx].Substring(FilterPrefix.Length).Split(',').Select(x => x.Replace('\\', '/').Trim()));
					ArgumentList.RemoveAt(Idx--);
				}
			}

			// Remove all the arguments for what to build
			BuildActions DefaultActions;
			if(ArgumentList.Count == 0)
			{
				DefaultActions = BuildActions.Clean | BuildActions.Build | BuildActions.Archive;
			}
			else
			{
				DefaultActions = ParseBuildActions(ArgumentList, "");
			}

			BuildActions CodeActions = ParseBuildActions(ArgumentList, "code") | DefaultActions;
			BuildActions CodeSnippetsActions = ParseBuildActions(ArgumentList, "codesnippets") | CodeActions;
			BuildActions CodeTargetActions = ParseBuildActions(ArgumentList, "codetarget") | CodeActions;
			BuildActions CodeMetadataActions = ParseBuildActions(ArgumentList, "codemeta") | CodeActions;
			BuildActions CodeXmlActions = ParseBuildActions(ArgumentList, "codexml") | CodeActions;
			BuildActions CodeUdnActions = ParseBuildActions(ArgumentList, "codeudn") | CodeActions;
			BuildActions CodeHtmlActions = ParseBuildActions(ArgumentList, "codehtml") | CodeActions;
			BuildActions CodeChmActions = ParseBuildActions(ArgumentList, "codechm") | CodeActions;

			BuildActions BlueprintActions = ParseBuildActions(ArgumentList, "blueprint") | DefaultActions;
			BuildActions BlueprintJsonActions = ParseBuildActions(ArgumentList, "blueprintjson") | BlueprintActions;
			BuildActions BlueprintUdnActions = ParseBuildActions(ArgumentList, "blueprintudn") | BlueprintActions;
			BuildActions BlueprintHtmlActions = ParseBuildActions(ArgumentList, "blueprinthtml") | BlueprintActions;
			BuildActions BlueprintChmActions = ParseBuildActions(ArgumentList, "blueprintchm") | BlueprintActions;

			// Print the help message and exit
			if(Arguments.Length == 0 || ArgumentList.Count > 0)
			{
				foreach(string InvalidArgument in ArgumentList)
				{
					Console.WriteLine("Invalid argument: {0}", InvalidArgument);
				}
				Console.WriteLine("APIDocTool.exe [options] -enginedir=<...>");                                    // <-- 80 character limit to start of comment
				Console.WriteLine();
				Console.WriteLine("Options:");
				Console.WriteLine("    -rebuild:                        Clean and build everything");
				Console.WriteLine("    -rebuild<step>:                  Clean and build specific steps");
				Console.WriteLine("    -clean:                          Clean all files"); 
				Console.WriteLine("    -clean<step>:                    Clean specific steps");
				Console.WriteLine("    -build:                          Build everything");
				Console.WriteLine("    -build<step>:                    Build specific output steps");
				Console.WriteLine("    -archive:                        Archive everything");
				Console.WriteLine("    -archive<step>:                  Archive specific output steps");
				Console.WriteLine("    -enginedir=<...>:                Specifies the root engine directory");
				Console.WriteLine("    -intermediatedir=<...>:          Specifies the intermediate directory");
				Console.WriteLine("    -documentationdir=<...>:         Specifies the documentation directory");
                Console.WriteLine("    -samplesdir=<...>:               Specifies the samples directory");
                Console.WriteLine("    -indexonly:                      Just build index pages");
				Console.WriteLine("    -filter=<...>,<...>:             Filter which things to convert, eg.");
				Console.WriteLine("                                     Folders:  -filter=Core/Containers/...");
				Console.WriteLine("                                     Entities: -filter=Core/TArray (without any folders)");
				Console.WriteLine("Valid steps are:");
				Console.WriteLine("   code, codetarget, codemeta, codexml, codeudn, codehtml, codechm, codesnippets");
				Console.WriteLine("   blueprint, blueprintjson, blueprintudn, blueprinthtml, blueprintchm");
				return 1;
			}

			// Derive all the tool paths
			string DoxygenPath = Path.Combine(EngineDir, "Extras\\NotForLicensees\\Doxygen\\bin\\Release64\\doxygen.exe");
			string EditorPath = Path.Combine(EngineDir, "Binaries\\Win64\\UE4Editor-Cmd.exe");
			string DocToolPath = Path.Combine(EngineDir, "Binaries\\DotNET\\UnrealDocTool.exe");
			string ChmCompilerPath = Path.Combine(EngineDir, "Extras\\NotForLicensees\\HTML Help Workshop\\hhc.exe");

			// Derive all the intermediate paths
			string CodeSitemapDir = Path.Combine(IntermediateDir, "sitemap");
			string BlueprintSitemapDir = Path.Combine(IntermediateDir, "blueprintsitemap");
			string TargetInfoPath = Path.Combine(IntermediateDir, "build\\targetinfo.xml");
			string MetadataDir = Path.Combine(IntermediateDir, "metadata");
			string XmlDir = Path.Combine(IntermediateDir, "doxygen");
			string JsonDir = Path.Combine(IntermediateDir, "json");
			string SnippetsDir = Path.Combine(IntermediateDir, "snippets");
			string MetadataPath = Path.Combine(MetadataDir, "metadata.xml");

			// Derive all the output paths
			string UdnDir = Path.Combine(DocumentationDir, "Source");
			string HtmlDir = Path.Combine(DocumentationDir, "HTML");
			string ChmDir = Path.Combine(DocumentationDir, "CHM");
			string ArchiveDir = Path.Combine(DocumentationDir, "Builds");

			// Read the settings file
			Settings = IniFile.Read(Path.Combine(DocumentationDir, "Extras\\API\\API.ini"));
			IgnoredFunctionMacros = new HashSet<string>(Settings.FindValueOrDefault("Input.IgnoredFunctionMacros", "").Split('\n'));
			IncludedSourceFiles = new HashSet<string>(Settings.FindValueOrDefault("Output.IncludedSourceFiles", "").Split('\n'));

			// Find all the metadata pages
			AddMetadataKeyword(UdnDir, "UCLASS", "Programming/UnrealArchitecture/Reference/Classes#classdeclaration", "Programming/UnrealArchitecture/Reference/Classes/Specifiers");
			AddMetadataKeyword(UdnDir, "UFUNCTION", "Programming/UnrealArchitecture/Reference/Functions", "Programming/UnrealArchitecture/Reference/Functions/Specifiers");
			AddMetadataKeyword(UdnDir, "UPROPERTY", "Programming/UnrealArchitecture/Reference/Properties", "Programming/UnrealArchitecture/Reference/Properties/Specifiers");
			AddMetadataKeyword(UdnDir, "USTRUCT", "Programming/UnrealArchitecture/Reference/Structs", "Programming/UnrealArchitecture/Reference/Structs/Specifiers");

			// Establish snippet directory so we can look things up later
			APISnippets.SetSnippetTextDirectory(SnippetsDir);

			// Build all the code docs
			if (!BuildCodeSnippetsTxt(SamplesDir, CodeSnippetsActions))
			{
				return 1;
			}
			if (!BuildCodeTargetInfo(TargetInfoPath, EngineDir, Path.Combine(ArchiveDir, "CodeAPI-TargetInfo.tgz"), CodeTargetActions))
			{
				return 1;
			}
			if (!BuildCodeMetadata(DoxygenPath, EngineDir, MetadataDir, MetadataPath, Path.Combine(ArchiveDir, "CodeAPI-Metadata.tgz"), CodeMetadataActions))
			{
				return 1;
			}
			if (!BuildCodeXml(EngineDir, TargetInfoPath, DoxygenPath, XmlDir, Path.Combine(ArchiveDir, "CodeAPI-XML.tgz"), Filters, CodeXmlActions))
			{
				return 1;
			}
			if (!BuildCodeUdn(EngineDir, XmlDir, UdnDir, CodeSitemapDir, MetadataPath, Path.Combine(ArchiveDir, "CodeAPI-UDN.tgz"), Path.Combine(ArchiveDir, "CodeAPI-Sitemap.tgz"), Filters, CodeUdnActions))
			{
				return 1;
			}
			if (!BuildHtml(EngineDir, DocToolPath, HtmlDir, "API", Path.Combine(ArchiveDir, "CodeAPI-HTML.tgz"), CodeHtmlActions))
			{
				return 1;
			}
			if (!BuildChm(ChmCompilerPath, Path.Combine(ChmDir, "API.chm"), Path.Combine(CodeSitemapDir, "API.hhc"), Path.Combine(CodeSitemapDir, "API.hhk"), HtmlDir, false, CodeChmActions))
			{
				return 1;
			}

			// Build all the blueprint docs
			if (!BuildBlueprintJson(JsonDir, EngineDir, EditorPath, Path.Combine(ArchiveDir, "BlueprintAPI-JSON.tgz"), bBuildMachine, BlueprintJsonActions))
			{
				return 1;
			}
			if (!BuildBlueprintUdn(JsonDir, UdnDir, BlueprintSitemapDir, Path.Combine(ArchiveDir, "BlueprintAPI-UDN.tgz"), Path.Combine(ArchiveDir, "BlueprintAPI-Sitemap.tgz"), BlueprintUdnActions))
			{
				return 1;
			}
			if (!BuildHtml(EngineDir, DocToolPath, HtmlDir, "BlueprintAPI", Path.Combine(ArchiveDir, "BlueprintAPI-HTML.tgz"), BlueprintHtmlActions))
			{
				return 1;
			}
			if (!BuildChm(ChmCompilerPath, Path.Combine(ChmDir, "BlueprintAPI.chm"), Path.Combine(BlueprintSitemapDir, "BlueprintAPI.hhc"), Path.Combine(BlueprintSitemapDir, "BlueprintAPI.hhk"), HtmlDir, true, BlueprintChmActions))
			{
				return 1;
			}

			// Finished!
			Console.WriteLine("Complete.");
			return 0;
		}

		static BuildActions ParseBuildActions(List<string> ArgumentList, string Suffix)
		{
			BuildActions Actions = 0;
			if (ArgumentList.Remove("-rebuild" + Suffix))
			{
				Actions |= BuildActions.Clean | BuildActions.Build | BuildActions.Archive;
			}
			if (ArgumentList.Remove("-clean" + Suffix))
			{
				Actions |= BuildActions.Clean;
			}
			if (ArgumentList.Remove("-build" + Suffix))
			{
				Actions |= BuildActions.Build;
			}
			if (ArgumentList.Remove("-archive" + Suffix))
			{
				Actions |= BuildActions.Archive;
			}
			return Actions;
		}

		static string ParseArgumentValue(List<string> ArgumentList, string Prefix, string DefaultValue)
		{
			for (int Idx = 0; Idx < ArgumentList.Count; Idx++)
			{
				if (ArgumentList[Idx].StartsWith(Prefix))
				{
					string Value = ArgumentList[Idx].Substring(Prefix.Length);
					ArgumentList.RemoveAt(Idx);
					return Value;
				}
			}
			return DefaultValue;
		}

		static string ParseArgumentPath(List<string> ArgumentList, string Prefix, string DefaultValue)
		{
			string Value = ParseArgumentValue(ArgumentList, Prefix, DefaultValue);
			if (Value != null)
			{
				Value = Path.GetFullPath(Value);
			}
			return Value;
		}

		static string ParseArgumentDirectory(List<string> ArgumentList, string Prefix, string DefaultValue)
		{
			string Value = ParseArgumentPath(ArgumentList, Prefix, DefaultValue);
			if(Value != null && !Directory.Exists(Value))
			{
				Directory.CreateDirectory(Value);
			}
			return Value;
		}


		static void AddMetadataKeyword(string BaseDir, string Name, string TypeUrl, string SpecifierBaseUrl)
		{
			MetadataKeyword Keyword = new MetadataKeyword();
			Keyword.Url = TypeUrl;
			foreach (DirectoryInfo Specifier in new DirectoryInfo(Path.Combine(BaseDir, SpecifierBaseUrl.Replace('/', '\\'))).EnumerateDirectories())
			{
				Keyword.NodeUrls.Add(Specifier.Name, SpecifierBaseUrl.TrimEnd('/') + "/" + Specifier.Name);
			}
			MetadataDirective.AllKeywords.Add(Name, Keyword);
		}

		static bool BuildCodeTargetInfo(string TargetInfoPath, string EngineDir, string ArchivePath, BuildActions Actions)
		{
			if((Actions & BuildActions.Clean) != 0)
			{
				Console.WriteLine("Cleaning '{0}'", TargetInfoPath);
				Utility.SafeDeleteFile(TargetInfoPath);
			}
			if ((Actions & BuildActions.Build) != 0)
			{
				Console.WriteLine("Building target info...");
				Utility.SafeCreateDirectory(Path.GetDirectoryName(TargetInfoPath));

				string Arguments = String.Format("DocumentationEditor Win64 Debug -noxge -project=\"{0}\"", Path.Combine(EngineDir, "Documentation\\Extras\\API\\Build\\Documentation.uproject"));
				if (!RunUnrealBuildTool(EngineDir, Arguments + " -clean"))
				{
					return false;
				}
				foreach (FileInfo Info in new DirectoryInfo(Path.Combine(EngineDir, "Intermediate\\Build")).EnumerateFiles("UBTEXport*.xml"))
				{
					File.Delete(Info.FullName);
				}
				if (!RunUnrealBuildTool(EngineDir, Arguments + " -disableunity -xgeexport"))
				{
					return false;
				}
				File.Copy(Path.Combine(EngineDir, "Intermediate\\Build\\UBTExport.0.xge.xml"), TargetInfoPath, true);
			}
			if((Actions & BuildActions.Archive) != 0)
			{
				Console.WriteLine("Creating archive '{0}'", ArchivePath);
				Utility.CreateTgzFromDir(ArchivePath, Path.GetDirectoryName(TargetInfoPath));
			}
			return true;
		}

		static bool RunUnrealBuildTool(string EngineDir, string Arguments)
		{
			using (Process NewProcess = new Process())
			{
				NewProcess.StartInfo.WorkingDirectory = EngineDir;
				NewProcess.StartInfo.FileName = Path.Combine(EngineDir, "Binaries\\DotNET\\UnrealBuildTool.exe");
				NewProcess.StartInfo.Arguments = Arguments;
				NewProcess.StartInfo.UseShellExecute = false;
				NewProcess.StartInfo.RedirectStandardOutput = true;
				NewProcess.StartInfo.RedirectStandardError = true;
				NewProcess.StartInfo.EnvironmentVariables.Remove("UE_SDKS_ROOT");

				NewProcess.OutputDataReceived += new DataReceivedEventHandler(ProcessOutputReceived);
				NewProcess.ErrorDataReceived += new DataReceivedEventHandler(ProcessOutputReceived);

				try
				{
					NewProcess.Start();
					NewProcess.BeginOutputReadLine();
					NewProcess.BeginErrorReadLine();
					NewProcess.WaitForExit();
					return NewProcess.ExitCode == 0;
				}
				catch (Exception Ex)
				{
					Console.WriteLine(Ex.ToString() + "\n" + Ex.StackTrace);
					return false;
				}
			}
		}

		static bool BuildCodeMetadata(string DoxygenPath, string EngineDir, string MetadataDir, string MetadataPath, string ArchivePath, BuildActions Actions)
		{
			if((Actions & BuildActions.Clean) != 0)
			{
				Console.WriteLine("Cleaning '{0}'", MetadataDir);
				Utility.SafeDeleteDirectoryContents(MetadataDir, true);
			}
			if((Actions & BuildActions.Build) != 0)
			{
				string MetadataInputPath = Path.Combine(EngineDir, "Source\\Runtime\\CoreUObject\\Public\\UObject\\ObjectBase.h");
				Console.WriteLine("Building metadata descriptions from '{0}'...", MetadataInputPath);

				DoxygenConfig Config = new DoxygenConfig("Metadata", new string[]{ MetadataInputPath }, MetadataDir);
				if (!Doxygen.Run(DoxygenPath, Path.Combine(EngineDir, "Source"), Config, true))
				{
					return false;
				}

				MetadataLookup.Reset();

				// Parse the xml output
				ParseMetadataTags(Path.Combine(MetadataDir, "xml\\namespace_u_c.xml"), MetadataLookup.ClassTags);
				ParseMetadataTags(Path.Combine(MetadataDir, "xml\\namespace_u_i.xml"), MetadataLookup.InterfaceTags);
				ParseMetadataTags(Path.Combine(MetadataDir, "xml\\namespace_u_f.xml"), MetadataLookup.FunctionTags);
				ParseMetadataTags(Path.Combine(MetadataDir, "xml\\namespace_u_p.xml"), MetadataLookup.PropertyTags);
				ParseMetadataTags(Path.Combine(MetadataDir, "xml\\namespace_u_s.xml"), MetadataLookup.StructTags);
				ParseMetadataTags(Path.Combine(MetadataDir, "xml\\namespace_u_m.xml"), MetadataLookup.MetaTags);

				// Rebuild all the reference names now that we've parsed a bunch of new tags
				MetadataLookup.BuildReferenceNameList();

				// Write it to a summary file
				MetadataLookup.Save(MetadataPath);
			}
			if ((Actions & BuildActions.Archive) != 0)
			{
				Console.WriteLine("Creating archive '{0}'", ArchivePath);
				Utility.CreateTgzFromDir(ArchivePath, MetadataDir);
			}
			return true;
		}

		static bool ParseMetadataTags(string InputFile, Dictionary<string, string> Tags)
		{
			XmlDocument Document = Utility.TryReadXmlDocument(InputFile);
			if (Document != null)
			{
				XmlNode EnumNode = Document.SelectSingleNode("doxygen/compounddef/sectiondef/memberdef");
				if (EnumNode != null && EnumNode.Attributes["kind"].Value == "enum")
				{
					using (XmlNodeList ValueNodeList = EnumNode.SelectNodes("enumvalue"))
					{
						foreach (XmlNode ValueNode in ValueNodeList)
						{
							string Name = ValueNode.SelectSingleNode("name").InnerText;

							string Description = ValueNode.SelectSingleNode("briefdescription").InnerText;
							if (Description == null) Description = ValueNode.SelectSingleNode("detaileddescription").InnerText;

							Description = Description.Trim();
							if (Description.StartsWith("[") && Description.Contains(']')) Description = Description.Substring(Description.IndexOf(']') + 1);

							Tags.Add(MetadataLookup.GetReferenceName(Name), Description);
						}
					}
					return true;
				}
			}
			return false;
		}

		static bool BuildCodeSnippetsTxt(string SamplesDir, BuildActions Actions)
		{
			if ((Actions & BuildActions.Clean) != 0)
			{
				APISnippets.CleanAllFiles();
			}
			if ((Actions & BuildActions.Build) == 0)
			{
				return true;
			}

			//We should be able to trim this down further.
			DirectoryInfo Dir = new DirectoryInfo(SamplesDir);
			List<string> Files = new List<string>();
			foreach (string FileName in Directory.GetFiles(SamplesDir, "*.cpp", SearchOption.AllDirectories))
			{
				if (!FileName.EndsWith(".generated.cpp"))
				{
					Files.Add(FileName);
				}
			}
			foreach (string FileName in Directory.GetFiles(SamplesDir, "*.h", SearchOption.AllDirectories))
			{
				if (!FileName.EndsWith(".generated.h"))
				{
					Files.Add(FileName);
				}
			}

			//Do the harvesting work.
			{
				const string OpeningTag = "///CODE_SNIPPET_START:";
				const string ClosingTag = "///CODE_SNIPPET_END";
				const string SeeTag = "@see";
				APISnippets Snippets = new APISnippets();
				List<string> CurrentSnippetPageNames = new List<string>(4);		//Probably won't have a snippet that is shared by more than four different pages.
				List<string> SeePageNames = new List<string>(4);				//More than four of these on one line will probably not happen.
				string CurrentLine;
				char[] WhiteSpace = {' ', '\t'};		//Doubles as our token delimiter in one place - noted in comments.
				string[] ClassMemberDelimiters = new string[2] { "::", "()" };
				bool IsSnippetBeingProcessed = false;
				bool WasPreviousLineBlank = false;		//Two blank lines in a row will end a code block. Don't allow this to happen in a single snippet.

				foreach (string FileName in Files)
				{
					// Read the file and display it line by line.
					System.IO.StreamReader file = new System.IO.StreamReader(FileName);
					int LineNumber = 0;
					while ((CurrentLine = file.ReadLine()) != null)
					{
						++LineNumber;
						CurrentLine = CurrentLine.TrimStart(WhiteSpace);
						if (!CurrentLine.Contains(OpeningTag))
						{
							continue;
						}
						CurrentSnippetPageNames = CurrentLine.Split(WhiteSpace, StringSplitOptions.RemoveEmptyEntries).ToList<string>();		//Whitespace is used to delimit our API/snippet page names here.
						while ((CurrentSnippetPageNames.Count > 0) && (!CurrentSnippetPageNames[0].Contains(OpeningTag)))
						{
							CurrentSnippetPageNames.RemoveAt(0);											//Remove everything before the opening tag.
						}
						if (CurrentSnippetPageNames.Count > 0)
						{
							CurrentSnippetPageNames.RemoveAt(0);											//Remove the opening tag, which is always in position 0 by this point.
						}
						if (CurrentSnippetPageNames.Count < 1)
						{
							Console.WriteLine("Error: OpeningTag for snippet harvesting found without any API pages specified.");
							return false;
						}

						IsSnippetBeingProcessed = true;
						foreach (string CurrentSnippetPageName in CurrentSnippetPageNames)
						{
							Snippets.AddSnippet(CurrentSnippetPageName);
							if (!Snippets.AddSnippetText(CurrentSnippetPageName, "**" + Path.GetFileName(FileName) + "** at line " + LineNumber + ":" + Environment.NewLine + Environment.NewLine))
							{
								Console.WriteLine("Error: Failed to add header text to snippet for " + CurrentSnippetPageName + " from source file " + FileName + " at line " + LineNumber);
								return false;
							}
						}
						bool FinishSnippetAfterThisLine = false;
						while ((CurrentLine = file.ReadLine()) != null)
						{
							++LineNumber;
							string TrimmedLine = CurrentLine.TrimStart(WhiteSpace);		//This is actually a C# same-line whitespace check, not our token delimiters.
							string TrimmedLineLower = TrimmedLine.ToLower();
							if (TrimmedLine.Contains(OpeningTag))
							{
								//Snippets do not currently support overlapping. If they did, closing tags should be explicit about which entries are ending, and we'd need to skip lines with opening tags.
								Console.WriteLine("Error: Nested OpeningTag found in " + FileName + " at line " + LineNumber + "! This is not supported. Snippet harvesting process will fail.");
								return false;
							}
							else if (TrimmedLine.StartsWith(ClosingTag))
							{
								//We're done with this snippet now. Mark that we can end cleanly.
								IsSnippetBeingProcessed = false;
								foreach (string CurrentSnippetPageName in CurrentSnippetPageNames)
								{
									Snippets.FinishCurrentSnippet(CurrentSnippetPageName);
								}
								break;
							}
							else if (TrimmedLine.Contains(ClosingTag))
							{
								//We will be done this snippet once we add this line (minus the tag).
								FinishSnippetAfterThisLine = true;
								CurrentLine = CurrentLine.Replace(ClosingTag, "//");
							}
							for (int TrimmedLineStartingIndex = TrimmedLineLower.IndexOf(SeeTag); TrimmedLineStartingIndex >= 0; TrimmedLineStartingIndex = TrimmedLineLower.Substring(TrimmedLineStartingIndex + SeeTag.Length).Contains(SeeTag) ? (TrimmedLineStartingIndex + SeeTag.Length + TrimmedLineLower.Substring(TrimmedLineStartingIndex + SeeTag.Length).IndexOf(SeeTag)) : -2)
							{
								int FirstCharacter = TrimmedLineStartingIndex + SeeTag.Length;
								SeePageNames = TrimmedLine.Substring(FirstCharacter).Split(WhiteSpace, StringSplitOptions.RemoveEmptyEntries).ToList<string>();		//Whitespace is used to delimit our API/snippet page names here.

								if (SeePageNames.Count > 0)
								{
									string SeeText = SeePageNames[0];
									string[] SeeTextBreakdown = SeeText.Split(ClassMemberDelimiters, StringSplitOptions.RemoveEmptyEntries);
									foreach (string CurrentSnippetPageName in CurrentSnippetPageNames)
									{
										if (SeeTextBreakdown.Length == 2)
										{
											//This is considered "class::member" format.
											if (!Snippets.AddSeeText(CurrentSnippetPageName, "1. [](API:" + SeeText + ")" + Environment.NewLine))
											{
												Console.WriteLine("Error: Failed to add text to snippet's \"See Also\" portion for " + CurrentSnippetPageName + " from source file " + FileName + " at line " + LineNumber);
												return false;
											}
										}
										else
										{
											if (!Snippets.AddSeeText(CurrentSnippetPageName, "1. [](" + SeeText + ")" + Environment.NewLine))
											{
												Console.WriteLine("Error: Failed to add text to snippet's \"See Also\" portion for " + CurrentSnippetPageName + " from source file " + FileName + " at line " + LineNumber);
												return false;
											}
										}
									}
								}
							}

							if (CurrentLine.Trim().Length < 1)
							{
								if (WasPreviousLineBlank)
								{
									//Two (or more) blank lines in a row! Not permitted.
									continue;
								}
								else
								{
									WasPreviousLineBlank = true;
								}
							}
							else
							{
								WasPreviousLineBlank = false;
							}

							//This line should be added to the snippet(s) named in the "CODE_SNIPPET_START" line. Capture it. We need to add our own newline.
							foreach (string CurrentSnippetPageName in CurrentSnippetPageNames)
							{
								if (!Snippets.AddSnippetText(CurrentSnippetPageName, '\t' + CurrentLine + Environment.NewLine))
								{
									Console.WriteLine("Error: Failed to add text to snippet for " + CurrentSnippetPageName + " from source file " + FileName + " at line " + LineNumber);
									return false;
								}
							}
							if (FinishSnippetAfterThisLine)
							{
								IsSnippetBeingProcessed = false;
								foreach (string CurrentSnippetPageName in CurrentSnippetPageNames)
								{
									Snippets.FinishCurrentSnippet(CurrentSnippetPageName);
								}
								break;
							}
						}
					}
					//If we hit the end of the file while harvesting a snippet, we should fail or have a warning. We could also just ignore the failure to end cleanly and just store the text as-is.
					//Opting for outright failure at the moment so that these errors don't go unnoticed.
					if (IsSnippetBeingProcessed)
					{
						Console.WriteLine("Code snippet start tag not matched with code snippet end tag in " + FileName);
						return false;
					}
					file.Close();
				}

				if (!Snippets.WriteSnippetsToFiles())
				{
					Console.WriteLine("Error writing intermediate snippet files.");
					return false;
				}
			}
			//Completed without error.
			return true;
		}

		static bool BuildCodeXml(string EngineDir, string TargetInfoPath, string DoxygenPath, string XmlDir, string ArchivePath, List<string> Filters, BuildActions Actions)
        {
            if ((Actions & BuildActions.Clean) != 0)
            {
                Console.WriteLine("Cleaning '{0}'", XmlDir);
                Utility.SafeDeleteDirectoryContents(XmlDir, true);
            }
            if ((Actions & BuildActions.Build) != 0)
            {
                Console.WriteLine("Building XML...");
                Utility.SafeCreateDirectory(XmlDir);

                // Read the target that we're building
                BuildTarget Target = new BuildTarget(Path.Combine(EngineDir, "Source"), TargetInfoPath);

                // Create an invariant list of exclude directories
                string[] InvariantExcludeDirectories = ExcludeSourceDirectories.Select(x => x.ToLowerInvariant()).ToArray();

                // Get the list of folders to filter against
                List<string> FolderFilters = new List<string>();
                if (Filters != null)
                {
                    foreach (string Filter in Filters)
                    {
                        int Idx = Filter.IndexOf('/');
                        if (Idx != -1)
                        {
                            FolderFilters.Add("\\" + Filter.Substring(0, Idx) + "\\");
                        }
                    }
                }

                // Flatten the target into a list of modules
                List<string> InputModules = new List<string>();
                foreach (string DirectoryName in Target.DirectoryNames)
                {
                    for (DirectoryInfo ModuleDirectory = new DirectoryInfo(DirectoryName); ModuleDirectory.Parent != null; ModuleDirectory = ModuleDirectory.Parent)
                    {
                        IEnumerator<FileInfo> ModuleFile = ModuleDirectory.EnumerateFiles("*.build.cs").GetEnumerator();
                        if (ModuleFile.MoveNext() && (FolderFilters.Count == 0 || FolderFilters.Any(x => ModuleFile.Current.FullName.Contains(x))))
                        {
                            InputModules.AddUnique(ModuleFile.Current.FullName);
                            break;
                        }
                    }
                }

                // Just use all the input modules
                if (!bIndexOnly)
                {
                    // Set our error mode so as to not bring up the WER dialog if Doxygen crashes (which it often does)
                    SetErrorMode(0x0007);

                    // Create the output directory
                    Utility.SafeCreateDirectory(XmlDir);

                    // Build all the modules
                    Console.WriteLine("Parsing source...");

                    // Build the list of definitions
                    List<string> Definitions = new List<string>();
                    foreach (string Definition in Target.Definitions)
                    {
                        if (!Definition.StartsWith("ORIGINAL_FILE_NAME="))
                        {
                            Definitions.Add(Definition.Replace("DLLIMPORT", "").Replace("DLLEXPORT", ""));
                        }
                    }

                    // Build a list of input paths
                    List<string> InputPaths = new List<string>();
                    foreach (string InputModule in InputModules)
                    {
                        foreach (string DirectoryName in Directory.EnumerateDirectories(Path.GetDirectoryName(InputModule), "*", SearchOption.AllDirectories))
                        {
                            // Find the relative path from the engine directory
                            string NormalizedDirectoryName = DirectoryName;
                            if (NormalizedDirectoryName.StartsWith(EngineDir))
                            {
                                NormalizedDirectoryName = NormalizedDirectoryName.Substring(EngineDir.Length);
                            }
                            if (!NormalizedDirectoryName.EndsWith("\\"))
                            {
                                NormalizedDirectoryName += "\\";
                            }

                            // Check we can include it
                            if (!ExcludeSourceDirectories.Any(x => NormalizedDirectoryName.Contains("\\" + x + "\\")))
                            {
                                if (FolderFilters.Count == 0 || FolderFilters.Any(x => NormalizedDirectoryName.Contains(x)))
                                {
                                    InputPaths.Add(DirectoryName);
                                }
                            }
                        }
                    }

                    // Build the configuration for this module
                    DoxygenConfig Config = new DoxygenConfig("UE4", InputPaths.ToArray(), XmlDir);
                    Config.Definitions.AddRange(Definitions);
                    Config.Definitions.AddRange(DoxygenPredefinedMacros);
                    Config.ExpandAsDefined.AddRange(DoxygenExpandedMacros);
                    Config.IncludePaths.AddRange(Target.IncludePaths);
                    Config.ExcludePatterns.AddRange(ExcludeSourceDirectories.Select(x => "*/" + x + "/*"));
                    Config.ExcludePatterns.AddRange(ExcludeSourceFiles);

                    // Run Doxygen
                    if (!Doxygen.Run(DoxygenPath, Path.Combine(EngineDir, "Source"), Config, true))
                    {
                        Console.WriteLine("  Doxygen crashed. Skipping.");
                        return false;
                    }
                }

                // Write the modules file
                File.WriteAllLines(Path.Combine(XmlDir, "modules.txt"), InputModules);
            }
            if ((Actions & BuildActions.Archive) != 0)
            {
                Console.WriteLine("Creating archive '{0}'", ArchivePath);
                Utility.CreateTgzFromDir(ArchivePath, XmlDir);
            }
            return true;
        }

		static bool BuildBlueprintJson(string JsonDir, string EngineDir, string EditorPath, string ArchivePath, bool bBuildMachine, BuildActions Actions)
		{
			if((Actions & BuildActions.Clean) != 0)
			{
				Console.WriteLine("Cleaning '{0}'", JsonDir);
				Utility.SafeDeleteDirectoryContents(JsonDir, true);
			}
			if ((Actions & BuildActions.Build) != 0)
			{
				// Create the output directory
				Utility.SafeCreateDirectory(JsonDir);

				string Arguments = "-run=GenerateBlueprintAPI -path=" + JsonDir + " -name=BlueprintAPI -stdout -FORCELOGFLUSH -CrashForUAT -unattended -AllowStdOutLogVerbosity" + (bBuildMachine? " -buildmachine" : "");
				Console.WriteLine("Running: {0} {1}", EditorPath, Arguments);

				using (Process JsonExportProcess = new Process())
				{
					JsonExportProcess.StartInfo.WorkingDirectory = EngineDir;
					JsonExportProcess.StartInfo.FileName = EditorPath;
					JsonExportProcess.StartInfo.Arguments = Arguments;
					JsonExportProcess.StartInfo.UseShellExecute = false;
					JsonExportProcess.StartInfo.RedirectStandardOutput = true;
					JsonExportProcess.StartInfo.RedirectStandardError = true;

					JsonExportProcess.OutputDataReceived += new DataReceivedEventHandler(ProcessOutputReceived);
					JsonExportProcess.ErrorDataReceived += new DataReceivedEventHandler(ProcessOutputReceived);

					try
					{
						JsonExportProcess.Start();
						JsonExportProcess.BeginOutputReadLine();
						JsonExportProcess.BeginErrorReadLine();
						JsonExportProcess.WaitForExit();
						if(JsonExportProcess.ExitCode != 0)
						{
							return false;
						}
					}
					catch (Exception Ex)
					{
						Console.WriteLine(Ex.ToString() + "\n" + Ex.StackTrace);
						return false;
					}
				}
			}
			if((Actions & BuildActions.Archive) != 0)
			{
				Console.WriteLine("Creating archive '{0}'", ArchivePath);
				Utility.CreateTgzFromDir(ArchivePath, JsonDir);
			}
			return true;
		}

		static bool BuildCodeUdn(string EngineDir, string XmlDir, string UdnDir, string SitemapDir, string MetadataPath, string ArchivePath, string SitemapArchivePath, List<string> Filters, BuildActions Actions)
		{
			string ApiDir = Path.Combine(UdnDir, "API");
			if((Actions & BuildActions.Clean) != 0)
			{
				Console.WriteLine("Cleaning '{0}'", ApiDir);
				Utility.SafeDeleteDirectoryContents(ApiDir, true);
				Utility.SafeDeleteDirectoryContents(SitemapDir, true);
			}
			if ((Actions & BuildActions.Build) != 0)
			{
				Directory.CreateDirectory(ApiDir);
				Directory.CreateDirectory(SitemapDir);

				// Read the metadata
				MetadataLookup.Load(MetadataPath);

				// Read the list of modules
				List<string> InputModules = new List<string>(File.ReadAllLines(Path.Combine(XmlDir, "modules.txt")));

				// Build the doxygen modules
				List<DoxygenModule> Modules = new List<DoxygenModule>();
				foreach (string InputModule in InputModules)
				{
					Modules.Add(new DoxygenModule(Path.GetFileNameWithoutExtension(Path.GetFileNameWithoutExtension(InputModule)), Path.GetDirectoryName(InputModule)));
				}

				// Find all the entities
				if (!bIndexOnly)
				{
					Console.WriteLine("Reading Doxygen output...");

					// Read the engine module and split it into smaller modules
					DoxygenModule RootModule = DoxygenModule.Read("UE4", EngineDir, Path.Combine(XmlDir, "xml"));
					foreach (DoxygenEntity Entity in RootModule.Entities)
					{
						DoxygenModule Module = Modules.Find(x => Entity.File.StartsWith(x.BaseSrcDir));
						Entity.Module = Module;
						Module.Entities.Add(Entity);
					}

					// Now filter all the entities in each module
					if (Filters != null && Filters.Count > 0)
					{
						FilterEntities(Modules, Filters);
					}

					// Remove all the empty modules
					Modules.RemoveAll(x => x.Entities.Count == 0);
				}

				// Create the index page, and all the pages below it
				APIIndex Index = new APIIndex(Modules);

				// Build a list of pages to output
				List<APIPage> OutputPages = new List<APIPage>(Index.GatherPages().OrderBy(x => x.LinkPath));

				// Remove any pages that don't want to be written
				int NumRemoved = OutputPages.RemoveAll(x => !x.ShouldOutputPage());
				Console.WriteLine("Removed {0} pages", NumRemoved);

				// Dump the output stats
				string StatsPath = Path.Combine(SitemapDir, "Stats.txt");
				Console.WriteLine("Writing stats to '" + StatsPath + "'");
				Stats NewStats = new Stats(OutputPages.OfType<APIMember>());
				NewStats.Write(StatsPath);

				// Setup the output directory 
				Utility.SafeCreateDirectory(UdnDir);

				// Build the manifest
				Console.WriteLine("Writing manifest...");
				UdnManifest Manifest = new UdnManifest(Index);
				Manifest.PrintConflicts();
				Manifest.Write(Path.Combine(UdnDir, APIFolder + "\\API.manifest"));

				// Write all the pages
				using (Tracker UdnTracker = new Tracker("Writing UDN pages...", OutputPages.Count))
				{
					foreach (int Idx in UdnTracker.Indices)
					{
						APIPage Page = OutputPages[Idx];

						// Create the output directory
						string MemberDirectory = Path.Combine(UdnDir, Page.LinkPath);
						if (!Directory.Exists(MemberDirectory))
						{
							Directory.CreateDirectory(MemberDirectory);
						}

						// Write the page
						Page.WritePage(Manifest, Path.Combine(MemberDirectory, "index.INT.udn"));
					}
				}

				// Write the sitemap contents
				Console.WriteLine("Writing sitemap contents...");
				Index.WriteSitemapContents(Path.Combine(SitemapDir, "API.hhc"));

				// Write the sitemap index
				Console.WriteLine("Writing sitemap index...");
				Index.WriteSitemapIndex(Path.Combine(SitemapDir, "API.hhk"));
			}
			if ((Actions & BuildActions.Archive) != 0)
			{
				Console.WriteLine("Creating archive '{0}'", ArchivePath);
				Utility.CreateTgzFromDir(ArchivePath, ApiDir);

				Console.WriteLine("Creating archive '{0}'", SitemapArchivePath);
				Utility.CreateTgzFromDir(SitemapArchivePath, SitemapDir);
			}
			return true;
		}

		static bool BuildBlueprintUdn(string JsonDir, string UdnDir, string SitemapDir, string ArchivePath, string SitemapArchivePath, BuildActions ExecActions)
		{
			string ApiDir = Path.Combine(UdnDir, "BlueprintAPI");
			if ((ExecActions & BuildActions.Clean) != 0)
			{
				Console.WriteLine("Cleaning '{0}'", ApiDir);
				Utility.SafeDeleteDirectoryContents(ApiDir, true);
				Utility.SafeDeleteDirectoryContents(SitemapDir, true);
			}
			if ((ExecActions & BuildActions.Build) != 0)
			{
				Directory.CreateDirectory(ApiDir);
				Directory.CreateDirectory(SitemapDir);

				// Read the input json file
				string JsonFilePath = Path.Combine(JsonDir, "BlueprintAPI.json");
				var json = (Dictionary<string, object>)fastJSON.JSON.Instance.Parse(File.ReadAllText(JsonFilePath));

				APICategory.LoadTooltips((Dictionary<string, object>)json["Categories"]);

				// TODO: This path is clearly sketchy as hell, but we'll clean it up later maybe
				var Actions = (Dictionary<string, object>)((Dictionary<string, object>)((Dictionary<string, object>)((Dictionary<string, object>)json["Actor"])["Palette"])["ActionSet"])["Actions"];

				APICategory RootCategory = new APICategory(null, "BlueprintAPI", true);

				foreach (var Action in Actions)
				{
					var CategoryList = Action.Key.Split('|');
					var ActionCategory = RootCategory;

					Debug.Assert(CategoryList.Length > 0);
					Debug.Assert(CategoryList[0] == "Library");

					for (int CategoryIndex = 1; CategoryIndex < CategoryList.Length - 1; ++CategoryIndex)
					{
						ActionCategory = ActionCategory.GetSubCategory(CategoryList[CategoryIndex]);
					}

					ActionCategory.AddAction(new APIAction(ActionCategory, CategoryList.Last(), (Dictionary<string, object>)Action.Value));
				}

				// Build a list of pages to output
				List<APIPage> OutputPages = new List<APIPage>(RootCategory.GatherPages().OrderBy(x => x.LinkPath));

				// Create the output directory
				Utility.SafeCreateDirectory(UdnDir);
				Utility.SafeCreateDirectory(Path.Combine(UdnDir, APIFolder));

				// Build the manifest
				Console.WriteLine("Writing manifest...");
				UdnManifest Manifest = new UdnManifest(RootCategory);
				Manifest.PrintConflicts();
				Manifest.Write(Path.Combine(UdnDir, BlueprintAPIFolder + "\\API.manifest"));

				Console.WriteLine("Categories: " + OutputPages.Count(page => page is APICategory));
				Console.WriteLine("Actions: " + OutputPages.Count(page => page is APIAction));

				// Write all the pages
				using (Tracker UdnTracker = new Tracker("Writing UDN pages...", OutputPages.Count))
				{
					foreach (int Idx in UdnTracker.Indices)
					{
						APIPage Page = OutputPages[Idx];

						// Create the output directory
						string MemberDirectory = Path.Combine(UdnDir, Page.LinkPath);
						if (!Directory.Exists(MemberDirectory))
						{
							Directory.CreateDirectory(MemberDirectory);
						}

						// Write the page
						Page.WritePage(Manifest, Path.Combine(MemberDirectory, "index.INT.udn"));
					}
				}

				// Write the sitemap contents
				Console.WriteLine("Writing sitemap contents...");
				RootCategory.WriteSitemapContents(Path.Combine(SitemapDir, "BlueprintAPI.hhc"));

				// Write the sitemap index
				Console.WriteLine("Writing sitemap index...");
				RootCategory.WriteSitemapIndex(Path.Combine(SitemapDir, "BlueprintAPI.hhk"));
			}
			if ((ExecActions & BuildActions.Archive) != 0)
			{
				Console.WriteLine("Creating archive '{0}'", ArchivePath);
				Utility.CreateTgzFromDir(ArchivePath, ApiDir);

				Console.WriteLine("Creating archive '{0}'", SitemapArchivePath);
				Utility.CreateTgzFromDir(SitemapArchivePath, SitemapDir);
			}
			return true;
		}

		public static IEnumerable<APIPage> GatherPages(params APIPage[] RootSet)
		{
			// Visit all the pages and collect all their references
			HashSet<APIPage> PendingSet = new HashSet<APIPage>(RootSet);
			HashSet<APIPage> VisitedSet = new HashSet<APIPage>();
			while (PendingSet.Count > 0)
			{
				APIPage Page = PendingSet.First();

				List<APIPage> ReferencedPages = new List<APIPage>();
				Page.GatherReferencedPages(ReferencedPages);

				foreach (APIPage ReferencedPage in ReferencedPages)
				{
					if (!VisitedSet.Contains(ReferencedPage))
					{
						PendingSet.Add(ReferencedPage);
					}
				}

				PendingSet.Remove(Page);
				VisitedSet.Add(Page);
			}
			return VisitedSet;
		}

		public static void FilterEntities(List<DoxygenModule> Modules, List<string> Filters)
		{
			foreach (DoxygenModule Module in Modules)
			{
				HashSet<DoxygenEntity> FilteredEntities = new HashSet<DoxygenEntity>();
				foreach (DoxygenEntity Entity in Module.Entities)
				{
					if(Filters.Exists(x => FilterEntity(Entity, x)))
					{
						for(DoxygenEntity AddEntity = Entity; AddEntity != null; AddEntity = AddEntity.Parent)
						{
							FilteredEntities.Add(AddEntity);
						}
					}
				}
				Module.Entities = new List<DoxygenEntity>(FilteredEntities);
			}
		}

		public static bool FilterEntity(DoxygenEntity Entity, string Filter)
		{
			// Check the module matches
			if(!Filter.StartsWith(Entity.Module.Name + "/", StringComparison.InvariantCultureIgnoreCase))
			{
				return false;
			}

			// Remove the module from the start of the filter
			string PathFilter = Filter.Substring(Entity.Module.Name.Length + 1);
			
			// Now check what sort of filter it is
			if (PathFilter == "...")
			{
				// Let anything through matching the module name, regardless of which subdirectory it's in (maybe it doesn't match a normal subdirectory at all)
				return true;
			}
			else if(PathFilter.EndsWith("/..."))
			{
				// Remove the ellipsis
				PathFilter = PathFilter.Substring(0, PathFilter.Length - 3);

				// Check the entity starts with the base directory
				if(!Entity.File.StartsWith(Entity.Module.BaseSrcDir, StringComparison.InvariantCultureIgnoreCase))
				{
					return false;
				}

				// Get the entity path, ignoring the 
				int EntityMinIdx = Entity.File.IndexOf('\\', Entity.Module.BaseSrcDir.Length);
				string EntityPath = Entity.File.Substring(EntityMinIdx + 1).Replace('\\', '/');

				// Get the entity path. Ignore the first directory under the module directory, as it'll be public/private/classes etc...
				return EntityPath.StartsWith(PathFilter, StringComparison.InvariantCultureIgnoreCase);
			}
			else
			{
				// Get the full entity name
				string EntityFullName = Entity.Name;
				for (DoxygenEntity ParentEntity = Entity.Parent; ParentEntity != null; ParentEntity = ParentEntity.Parent)
				{
					EntityFullName = ParentEntity.Name + "::" + EntityFullName;
				}

				// Compare it to the filter
				return Filter == (Entity.Module.Name + "/" + EntityFullName);
			}
		}

		static bool BuildHtml(string EngineDir, string DocToolPath, string HtmlDir, string HtmlSuffix, string ArchivePath, BuildActions Actions)
		{
			string HtmlSubDir = Path.Combine(HtmlDir, "INT", HtmlSuffix);
			if((Actions & BuildActions.Clean) != 0)
			{
				Console.WriteLine("Cleaning '{0}'", HtmlSubDir);
				Utility.SafeDeleteDirectoryContents(HtmlSubDir, true);
			}
			if ((Actions & BuildActions.Build) != 0)
			{
				Utility.SafeCreateDirectory(HtmlSubDir);

				using (Process DocToolProcess = new Process())
				{
					DocToolProcess.StartInfo.WorkingDirectory = EngineDir;
					DocToolProcess.StartInfo.FileName = DocToolPath;
					DocToolProcess.StartInfo.Arguments = Path.Combine(HtmlSuffix, "*") + " -lang=INT -t=DefaultAPI.html -v=warn";
					DocToolProcess.StartInfo.UseShellExecute = false;
					DocToolProcess.StartInfo.RedirectStandardOutput = true;
					DocToolProcess.StartInfo.RedirectStandardError = true;

					DocToolProcess.OutputDataReceived += new DataReceivedEventHandler(ProcessOutputReceived);
					DocToolProcess.ErrorDataReceived += new DataReceivedEventHandler(ProcessOutputReceived);

					try
					{
						DocToolProcess.Start();
						DocToolProcess.BeginOutputReadLine();
						DocToolProcess.BeginErrorReadLine();
						DocToolProcess.WaitForExit();
					}
					catch (Exception Ex)
					{
						Console.WriteLine(Ex.ToString() + "\n" + Ex.StackTrace);
						return false;
					}

					if (DocToolProcess.ExitCode != 0)
					{
						return false;
					}
				}
			}
			if ((Actions & BuildActions.Archive) != 0)
			{
				Console.WriteLine("Creating archive '{0}'", ArchivePath);

				List<string> InputFiles = new List<string>();
				Utility.FindFilesForTar(HtmlDir, "Include", InputFiles, true);
				Utility.FindFilesForTar(HtmlDir, "INT\\" + HtmlSuffix, InputFiles, true);
				Utility.FindFilesForTar(HtmlDir, "Images", InputFiles, false);

				Utility.CreateTgzFromFiles(ArchivePath, HtmlDir, InputFiles);
			}
			return true;
		}

		static bool BuildChm(string ChmCompilerPath, string ChmFileName, string ContentsFileName, string IndexFileName, string BaseHtmlDir, bool bBlueprint, BuildActions Actions)
		{
			if((Actions & BuildActions.Clean) != 0)
			{
				Console.WriteLine("Cleaning '{0}'", ChmFileName);
				Utility.SafeDeleteFile(ChmFileName);
				Utility.SafeDeleteDirectory(Path.Combine(Path.GetDirectoryName(ChmFileName), Path.GetFileNameWithoutExtension(ChmFileName)));
			}
			if ((Actions & BuildActions.Build) != 0)
			{
				string ProjectFileName = (bBlueprint ? "BlueprintAPI.hhp" : "API.hhp");
				Console.WriteLine("Searching for CHM input files...");

				// Build a list of all the files we want to copy
				List<string> FilePaths = new List<string>();
				List<string> DirectoryPaths = new List<string>();
				Utility.FindRelativeContents(BaseHtmlDir, "Include\\*", true, FilePaths, DirectoryPaths);

				// Find all the HTML files
				if (bBlueprint)
				{
					Utility.FindRelativeContents(BaseHtmlDir, "INT\\BlueprintAPI\\*.html", true, FilePaths, DirectoryPaths);
					CreateChm(ChmCompilerPath, ChmFileName, "UE4 Blueprint API Documentation", "INT\\BlueprintAPI\\index.html", ContentsFileName, IndexFileName, BaseHtmlDir, FilePaths);
				}
				else
				{
					Utility.FindRelativeContents(BaseHtmlDir, "Images\\api*", false, FilePaths, DirectoryPaths);
					Utility.FindRelativeContents(BaseHtmlDir, "INT\\API\\*.html", true, FilePaths, DirectoryPaths);
					CreateChm(ChmCompilerPath, ChmFileName, "UE4 API Documentation", "INT\\API\\index.html", ContentsFileName, IndexFileName, BaseHtmlDir, FilePaths);
				}
			}
			return true;
		}

		static private void CreateChm(string ChmCompilerPath, string ChmFileName, string Title, string DefaultTopicPath, string ContentsFileName, string IndexFileName, string SourceDir, List<string> FileNames)
		{
			string ProjectName = Path.GetFileNameWithoutExtension(ChmFileName);

			// Create an intermediate directory
			string IntermediateDir = Path.Combine(Path.GetDirectoryName(ChmFileName), ProjectName);
			Directory.CreateDirectory(IntermediateDir);
			Utility.SafeDeleteDirectoryContents(IntermediateDir, true);

			// Copy all the files across
			using (Tracker CopyTracker = new Tracker("Copying files...", FileNames.Count))
			{
				foreach (int Idx in CopyTracker.Indices)
				{
					string SourceFileName = Path.Combine(SourceDir, FileNames[Idx]);
					string TargetFileName = Path.Combine(IntermediateDir, FileNames[Idx]);

					Directory.CreateDirectory(Path.GetDirectoryName(TargetFileName));

					if (SourceFileName.EndsWith(".html", StringComparison.InvariantCultureIgnoreCase))
					{
						string HtmlText = File.ReadAllText(SourceFileName);

						HtmlText = HtmlText.Replace("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n", "");
						HtmlText = HtmlText.Replace("<div id=\"crumbs_bg\"></div>", "");

						const string HeaderEndText = "<!-- end head -->";
						int HeaderMinIdx = HtmlText.IndexOf("<div id=\"head\">");
						int HeaderMaxIdx = HtmlText.IndexOf(HeaderEndText);
						HtmlText = HtmlText.Remove(HeaderMinIdx, HeaderMaxIdx + HeaderEndText.Length - HeaderMinIdx);

						int CrumbsMinIdx = HtmlText.IndexOf("<div class=\"crumbs\">");
						if (CrumbsMinIdx >= 0)
						{
							int HomeMinIdx = HtmlText.IndexOf("<strong>", CrumbsMinIdx);
							int HomeMaxIdx = HtmlText.IndexOf("&gt;", HomeMinIdx) + 4;
							HtmlText = HtmlText.Remove(HomeMinIdx, HomeMaxIdx - HomeMinIdx);
						}

						File.WriteAllText(TargetFileName, HtmlText);
					}
					else
					{
						Utility.SafeCopyFile(SourceFileName, TargetFileName);
					}
				}
			}

			// Copy the contents and index files
			Utility.SafeCopyFile(ContentsFileName, Path.Combine(IntermediateDir, ProjectName + ".hhc"));
			Utility.SafeCopyFile(IndexFileName, Path.Combine(IntermediateDir, ProjectName + ".hhk"));

			// Write the project file
			string ProjectFileName = Path.Combine(IntermediateDir, ProjectName + ".hhp");
			using (StreamWriter Writer = new StreamWriter(ProjectFileName))
			{
				Writer.WriteLine("[OPTIONS]");
				Writer.WriteLine("Title={0}", Title);
				Writer.WriteLine("Binary TOC=Yes");
				Writer.WriteLine("Compatibility=1.1 or later");
				Writer.WriteLine("Compiled file={0}", Path.GetFileName(ChmFileName));
				Writer.WriteLine("Contents file={0}.hhc", ProjectName);
				Writer.WriteLine("Index file={0}.hhk", ProjectName);
				Writer.WriteLine("Default topic={0}", DefaultTopicPath);
				Writer.WriteLine("Full-text search=Yes");
				Writer.WriteLine("Display compile progress=Yes");
				Writer.WriteLine("Language=0x409 English (United States)");
				Writer.WriteLine("Default Window=MainWindow");
				Writer.WriteLine();
				Writer.WriteLine("[WINDOWS]");
				Writer.WriteLine("MainWindow=\"{0}\",\"{1}.hhc\",\"{1}.hhk\",\"{2}\",\"{2}\",,,,,0x23520,230,0x304e,[10,10,1260,750],,,,,,,0", Title, ProjectName, DefaultTopicPath);
				Writer.WriteLine();
				Writer.WriteLine("[FILES]");
				foreach (string FileName in FileNames)
				{
					Writer.WriteLine(FileName);
				}
			}

			// Compile the project
			Console.WriteLine("Compiling CHM file...");
			using (Process CompilerProcess = new Process())
			{
				CompilerProcess.StartInfo.WorkingDirectory = IntermediateDir;
				CompilerProcess.StartInfo.FileName = ChmCompilerPath;
				CompilerProcess.StartInfo.Arguments = Path.GetFileName(ProjectFileName);
				CompilerProcess.StartInfo.UseShellExecute = false;
				CompilerProcess.StartInfo.RedirectStandardOutput = true;
				CompilerProcess.StartInfo.RedirectStandardError = true;
				CompilerProcess.OutputDataReceived += ChmOutputReceived;
				CompilerProcess.ErrorDataReceived += ChmOutputReceived;
				CompilerProcess.Start();
				CompilerProcess.BeginOutputReadLine();
				CompilerProcess.BeginErrorReadLine();
				CompilerProcess.WaitForExit();
			}

			// Copy it to the final output
			Utility.SafeCopyFile(Path.Combine(IntermediateDir, ProjectName + ".chm"), ChmFileName);
		}

		static private void ChmOutputReceived(Object Sender, DataReceivedEventArgs Line)
		{
			if(Line.Data != null && Line.Data.Length > 0)
			{
				// Building the API docs seems to always give a few of these warnings in random files, but they don't seem to be duplicated.
				// It looks like HHC hashes the filenames it's seen, and we get a couple of collisions with ~130,000 pages.
				string OutputLine = Line.Data.TrimEnd();
				if(OutputLine.EndsWith("is already listed in the [FILES] section of the project file."))
				{
					OutputLine = OutputLine.Replace("Warning:", "Note:");
				}
				Console.WriteLine(OutputLine);
			}
		}

		static private void ProcessOutputReceived(Object Sender, DataReceivedEventArgs Line)
		{
			if (Line.Data != null && Line.Data.Length > 0)
			{
				Console.WriteLine(Line.Data);
			}
		}

		public static void FindAllMembers(APIMember Member, List<APIMember> Members)
		{
			Members.Add(Member);

			foreach (APIMember Child in Member.Children)
			{
				FindAllMembers(Child, Members);
			}
		}

		public static void FindAllMatchingParents(APIMember Member, List<APIMember> Members, string[] FilterPaths)
		{
			if (Utility.MatchPath(Member.LinkPath + "\\", FilterPaths))
			{
				Members.Add(Member);
			}
			else
			{
				foreach (APIMember Child in Member.Children)
				{
					FindAllMatchingParents(Child, Members, FilterPaths);
				}
			}
		}
		public static string ResolveLink(string Id)
		{
			APIMember RefMember = APIMember.ResolveRefLink(Id);
			return (RefMember != null) ? RefMember.LinkPath : null;
		}
    }
}
