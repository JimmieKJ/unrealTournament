// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.Linq;
using System.IO;
using System.ComponentModel;
using System.Diagnostics;
using System.Text.RegularExpressions;
using Tools.DotNETCommon.CaselessDictionary;

namespace AutomationTool
{
	public class RequireP4Attribute : Attribute
	{
	}

	public class DoesNotNeedP4CLAttribute : Attribute
	{
	}

	public class P4Exception : AutomationException
	{
		public P4Exception() { }
		public P4Exception(string Msg)
			: base(Msg) { }
		public P4Exception(string Msg, Exception InnerException)
			: base(InnerException, Msg) { }

		public P4Exception(string Format, params object[] Args)
			: base(Format, Args) { }
	}

    [Flags]
    public enum P4LineEnd
    {
        Local = 0,
        Unix = 1,
        Mac = 2,
        Win = 3,
        Share = 4,
    }

    [Flags]
    public enum P4SubmitOption
    {
        SubmitUnchanged = 0,
        RevertUnchanged = 1,
        LeaveUnchanged = 2,
    }

    [Flags]
    public enum P4ClientOption
    {
        None = 0,
        NoAllWrite = 1,
        NoClobber = 2,
        NoCompress = 4,
        NoModTime = 8,
        NoRmDir = 16,
        Unlocked = 32,
        AllWrite = 64,
        Clobber = 128,
        Compress = 256,
        Locked = 512,
        ModTime = 1024,
        RmDir = 2048,
    }

	public class P4ClientInfo
	{
		public string Name;
		public string RootPath;
		public string Host;
		public string Owner;
		public string Stream;
		public DateTime Access;
        public P4LineEnd LineEnd;
        public P4ClientOption Options;
        public P4SubmitOption SubmitOptions;
        public List<KeyValuePair<string, string>> View = new List<KeyValuePair<string, string>>();

		public bool Matches(P4ClientInfo Other)
		{
			return Name == Other.Name 
				&& RootPath == Other.RootPath 
				&& Host == Other.Host 
				&& Owner == Other.Owner 
				&& Stream == Other.Stream
				&& LineEnd == Other.LineEnd 
				&& Options == Other.Options 
				&& SubmitOptions == Other.SubmitOptions
				&& (!String.IsNullOrEmpty(Stream) || Enumerable.SequenceEqual(View, Other.View));
		}

		public override string ToString()
		{
			return Name;
		}
	}

	public enum P4FileType
	{
		[Description("unknown")]
		Unknown,
		[Description("text")]
		Text,
		[Description("binary")]
		Binary,
		[Description("resource")]
		Resource,
		[Description("tempobj")]
		Temp,
		[Description("symlink")]
		Symlink,
		[Description("apple")]
		Apple,
		[Description("unicode")]
		Unicode,
		[Description("utf16")]
		Utf16,
		[Description("utf8")]
		Utf8,
	}

	[Flags]
	public enum P4FileAttributes
	{
		[Description("")]
		None = 0,
		[Description("u")]
		Unicode = 1 << 0,
		[Description("x")]
		Executable = 1 << 1,
		[Description("w")]
		Writeable = 1 << 2,
		[Description("m")]
		LocalModTimes = 1 << 3,
		[Description("k")]
		RCS = 1 << 4,
		[Description("l")]
		Exclusive = 1 << 5,
		[Description("D")]
		DeltasPerRevision = 1 << 6,
		[Description("F")]
		Uncompressed = 1 << 7,
		[Description("C")]
		Compressed = 1 << 8,
		[Description("X")]
		Archive = 1 << 9,
		[Description("S")]
		Revisions = 1 << 10,
	}

	public enum P4Action
	{
		[Description("none")]
		None,
		[Description("add")]
		Add,
		[Description("edit")]
		Edit,
		[Description("delete")]
		Delete,
		[Description("branch")]
		Branch,
		[Description("move/add")]
		MoveAdd,
		[Description("move/delete")]
		MoveDelete,
		[Description("integrate")]
		Integrate,
		[Description("import")]
		Import,
		[Description("purge")]
		Purge,
		[Description("archive")]
		Archive,
		[Description("unknown")]
		Unknown,
	}

	public struct P4FileStat
	{
		public P4FileType Type;
		public P4FileAttributes Attributes;
		public P4Action Action;
		public string Change;
		public bool IsOldType;

		public P4FileStat(P4FileType Type, P4FileAttributes Attributes, P4Action Action)
		{
			this.Type = Type;
			this.Attributes = Attributes;
			this.Action = Action;
			this.Change = String.Empty;
			this.IsOldType = false;
		}

		public static readonly P4FileStat Invalid = new P4FileStat(P4FileType.Unknown, P4FileAttributes.None, P4Action.None);

		public bool IsValid { get { return Type != P4FileType.Unknown; } }
	}

	public class P4WhereRecord
	{
		public bool bUnmap;
		public string DepotFile;
		public string ClientFile;
		public string Path;
	}

	public class P4Spec
	{
		public List<KeyValuePair<string, string>> Sections;

		/// <summary>
		/// Default constructor.
		/// </summary>
		public P4Spec()
		{
			Sections = new List<KeyValuePair<string,string>>();
		}

		/// <summary>
		/// Gets the current value of a field with the given name 
		/// </summary>
		/// <param name="Name">Name of the field to search for</param>
		/// <returns>The value of the field, or null if it does not exist</returns>
		public string GetField(string Name)
		{
			int Idx = Sections.FindIndex(x => x.Key == Name);
			return (Idx == -1)? null : Sections[Idx].Value;
		}

		/// <summary>
		/// Sets the value of an existing field, or adds a new one with the given name
		/// </summary>
		/// <param name="Name">Name of the field to set</param>
		/// <param name="Value">New value of the field</param>
		public void SetField(string Name, string Value)
		{
			int Idx = Sections.FindIndex(x => x.Key == Name);
			if(Idx == -1)
			{
				Sections.Add(new KeyValuePair<string,string>(Name, Value));
			}
			else
			{
				Sections[Idx] = new KeyValuePair<string,string>(Name, Value);
			}
		}

		/// <summary>
		/// Parses a spec (clientspec, branchspec, changespec) from an array of lines
		/// </summary>
		/// <param name="Lines">Text split into separate lines</param>
		/// <returns>Array of section names and values</returns>
		public static P4Spec FromString(string Text)
		{
			P4Spec Spec = new P4Spec();

			string[] Lines = Text.Split('\n');
			for(int LineIdx = 0; LineIdx < Lines.Length; LineIdx++)
			{
				if(Lines[LineIdx].EndsWith("\r"))
				{
					Lines[LineIdx] = Lines[LineIdx].Substring(0, Lines[LineIdx].Length - 1);
				}
				if(!String.IsNullOrWhiteSpace(Lines[LineIdx]) && !Lines[LineIdx].StartsWith("#"))
				{
					// Read the section name
					int SeparatorIdx = Lines[LineIdx].IndexOf(':');
					if(SeparatorIdx == -1 || !Char.IsLetter(Lines[LineIdx][0]))
					{
						throw new P4Exception("Invalid spec format at line {0}: \"{1}\"", LineIdx, Lines[LineIdx]);
					}

					// Get the section name
					string SectionName = Lines[LineIdx].Substring(0, SeparatorIdx);

					// Parse the section value
					StringBuilder Value = new StringBuilder(Lines[LineIdx].Substring(SeparatorIdx + 1));
					for(; LineIdx + 1 < Lines.Length; LineIdx++)
					{
						if(Lines[LineIdx + 1].Length == 0)
						{
							Value.AppendLine();
						}
						else if(Lines[LineIdx + 1][0] == '\t')
						{
							Value.AppendLine(Lines[LineIdx + 1].Substring(1));
						}
						else
						{
							break;
						}
					}
					Spec.Sections.Add(new KeyValuePair<string,string>(SectionName, Value.ToString().TrimEnd()));
				}
			}

			return Spec;
		}

		/// <summary>
		/// Formats a P4 specification as a block of text
		/// </summary>
		/// <returns></returns>
		public override string ToString()
		{
			StringBuilder Result = new StringBuilder();
			foreach(KeyValuePair<string, string> Section in Sections)
			{
				if(Section.Value.Contains('\n'))
				{
					Result.AppendLine(Section.Key + ":\n\t" + Section.Value.Replace("\n", "\n\t"));
				}
				else
				{
					Result.AppendLine(Section.Key + ":\t" + Section.Value);
				}
				Result.AppendLine();
			}
			return Result.ToString();
		}
	}

	public partial class CommandUtils
	{
		#region Environment Setup

		static private P4Connection PerforceConnection;
		static private P4Environment PerforceEnvironment;

		/// <summary>
		/// BuildEnvironment to use for this buildcommand. This is initialized by InitBuildEnvironment. As soon
		/// as the script execution in ExecuteBuild begins, the BuildEnv is set up and ready to use.
		/// </summary>
		static public P4Connection P4
		{
			get
			{
				if (PerforceConnection == null)
				{
					throw new AutomationException("Attempt to use P4 before it was initialized or P4 support is disabled.");
				}
				return PerforceConnection;
			}
		}

		/// <summary>
		/// BuildEnvironment to use for this buildcommand. This is initialized by InitBuildEnvironment. As soon
		/// as the script execution in ExecuteBuild begins, the BuildEnv is set up and ready to use.
		/// </summary>
		static public P4Environment P4Env
		{
			get
			{
				if (PerforceEnvironment == null)
				{
					throw new AutomationException("Attempt to use P4Environment before it was initialized or P4 support is disabled.");
				}
				return PerforceEnvironment;
			}
		}

		/// <summary>
		/// Initializes build environment. If the build command needs a specific env-var mapping or
		/// has an extended BuildEnvironment, it must implement this method accordingly.
		/// </summary>
		static internal void InitP4Environment()
		{
			CheckP4Enabled();

			// Temporary connection - will use only the currently set env vars to connect to P4
			var DefaultConnection = new P4Connection(User: null, Client: null);
			PerforceEnvironment = Automation.IsBuildMachine ? new P4Environment(DefaultConnection, CmdEnv) : new LocalP4Environment(DefaultConnection, CmdEnv);
		}

		/// <summary>
		/// Initializes default source control connection.
		/// </summary>
		static internal void InitDefaultP4Connection()
		{
			CheckP4Enabled();

			PerforceConnection = new P4Connection(User: P4Env.User, Client: P4Env.Client, ServerAndPort: P4Env.P4Port);
		}

		#endregion

		/// <summary>
		/// Check if P4 is supported.
		/// </summary>		
		public static bool P4Enabled
		{
			get
			{
				if (!bP4Enabled.HasValue)
				{
					throw new AutomationException("Trying to access P4Enabled property before it was initialized.");
				}
				return (bool)bP4Enabled;
			}
			private set
			{
				bP4Enabled = value;
			}
		}
		private static bool? bP4Enabled;

		/// <summary>
		/// Check if P4CL is required.
		/// </summary>		
		public static bool P4CLRequired
		{
			get
			{
				if (!bP4CLRequired.HasValue)
				{
					throw new AutomationException("Trying to access P4CLRequired property before it was initialized.");
				}
				return (bool)bP4CLRequired;
			}
			private set
			{
				bP4CLRequired = value;
			}
		}
		private static bool? bP4CLRequired;

		/// <summary>
		/// Throws an exception when P4 is disabled. This should be called in every P4 function.
		/// </summary>
		internal static void CheckP4Enabled()
		{
			if (P4Enabled == false)
			{
				throw new AutomationException("P4 is not enabled.");
			}
		}

		/// <summary>
		/// Checks whether commands are allowed to submit files into P4.
		/// </summary>
		public static bool AllowSubmit
		{
			get
			{
				if (!bAllowSubmit.HasValue)
				{
					throw new AutomationException("Trying to access AllowSubmit property before it was initialized.");
				}
				return (bool)bAllowSubmit;
			}
			private set
			{
				bAllowSubmit = value;
			}

		}
		private static bool? bAllowSubmit;

		/// <summary>
		/// Sets up P4Enabled, AllowSubmit properties. Note that this does not initialize P4 environment.
		/// </summary>
		/// <param name="CommandsToExecute">Commands to execute</param>
		/// <param name="Commands">Commands</param>
		internal static void InitP4Support(List<CommandInfo> CommandsToExecute, CaselessDictionary<Type> Commands)
		{
			// Init AllowSubmit
			// If we do not specify on the commandline if submitting is allowed or not, this is 
			// depending on whether we run locally or on a build machine.
			LogVerbose("Initializing AllowSubmit.");
			if (GlobalCommandLine.Submit || GlobalCommandLine.NoSubmit)
			{
				AllowSubmit = GlobalCommandLine.Submit;
			}
			else
			{
				AllowSubmit = Automation.IsBuildMachine;
			}
			LogVerbose("AllowSubmit={0}", AllowSubmit);

			// Init P4Enabled
			LogVerbose("Initializing P4Enabled.");
			if (Automation.IsBuildMachine)
			{
				P4Enabled = !GlobalCommandLine.NoP4;
				P4CLRequired = P4Enabled;
			}
			else
			{
				bool bRequireP4;
				bool bRequireCL;
				CheckIfCommandsRequireP4(CommandsToExecute, Commands, out bRequireP4, out bRequireCL);

				P4Enabled = GlobalCommandLine.P4 || bRequireP4;
				P4CLRequired = GlobalCommandLine.P4 || bRequireCL;
			}
			LogVerbose("P4Enabled={0}", P4Enabled);
			LogVerbose("P4CLRequired={0}", P4CLRequired);
		}

		/// <summary>
		/// Checks if any of the commands to execute has [RequireP4] attribute.
		/// </summary>
		/// <param name="CommandsToExecute">List of commands to be executed.</param>
		/// <param name="Commands">Commands.</param>
		private static void CheckIfCommandsRequireP4(List<CommandInfo> CommandsToExecute, CaselessDictionary<Type> Commands, out bool bRequireP4, out bool bRequireCL)
		{
			bRequireP4 = false;
			bRequireCL = false;

			foreach (var CommandInfo in CommandsToExecute)
			{
				Type Command;
				if (Commands.TryGetValue(CommandInfo.CommandName, out Command))
				{
					var RequireP4Attributes = Command.GetCustomAttributes(typeof(RequireP4Attribute), true);	
					if (!CommandUtils.IsNullOrEmpty(RequireP4Attributes))
					{
						if(!GlobalCommandLine.P4)
						{
							LogWarning("Command {0} requires P4 functionality.", Command.Name);
						}
						bRequireP4 = true;

						var DoesNotNeedP4CLAttributes = Command.GetCustomAttributes(typeof(DoesNotNeedP4CLAttribute), true);
						if (CommandUtils.IsNullOrEmpty(DoesNotNeedP4CLAttributes))
						{
							bRequireCL = true;
						}
					}
				}
			}
		}
	}

	/// <summary>
	/// Class that stores labels info.
	/// </summary>
	public class P4Label
	{
		// The name of the label.
		public string Name { get; private set; }

		// The date of the label.
		public DateTime Date { get; private set; }

		public P4Label(string Name, DateTime Date)
		{
			this.Name = Name;
			this.Date = Date;
		}
	}

	/// <summary>
	/// Perforce connection.
	/// </summary>
	public partial class P4Connection
	{
		/// <summary>
		/// List of global options for this connection (client/user)
		/// </summary>
		private string GlobalOptions;
        /// <summary>
        /// List of global options for this connection (client/user)
        /// </summary>
        private string GlobalOptionsWithoutClient;
		/// <summary>
		/// Path where this connection's log is to go to
		/// </summary>
		public string LogPath { get; private set; }

		/// <summary>
		/// Initializes P4 connection
		/// </summary>
		/// <param name="User">Username (can be null, in which case the environment variable default will be used)</param>
		/// <param name="Client">Workspace (can be null, in which case the environment variable default will be used)</param>
		/// <param name="ServerAndPort">Server:Port (can be null, in which case the environment variable default will be used)</param>
		/// <param name="P4LogPath">Log filename (can be null, in which case CmdEnv.LogFolder/p4.log will be used)</param>
		public P4Connection(string User, string Client, string ServerAndPort = null, string P4LogPath = null)
		{
			var UserOpts = String.IsNullOrEmpty(User) ? "" : ("-u" + User + " ");
			var ClientOpts = String.IsNullOrEmpty(Client) ? "" : ("-c" + Client + " ");
			var ServerOpts = String.IsNullOrEmpty(ServerAndPort) ? "" : ("-p" + ServerAndPort + " ");			
			GlobalOptions = UserOpts + ClientOpts + ServerOpts;
            GlobalOptionsWithoutClient = UserOpts + ServerOpts;

			if (P4LogPath == null)
			{
				LogPath = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LogFolder, String.Format("p4.log", Client));
			}
			else
			{
				LogPath = P4LogPath;
			}
		}

		/// <summary>
		/// Throws an exception when P4 is disabled. This should be called in every P4 function.
		/// </summary>
		internal static void CheckP4Enabled()
		{
			CommandUtils.CheckP4Enabled();
		}

		/// <summary>
		/// Shortcut to Run but with P4.exe as the program name.
		/// </summary>
		/// <param name="CommandLine">Command line</param>
		/// <param name="Input">Stdin</param>
		/// <param name="AllowSpew">true for spew</param>
		/// <returns>Exit code</returns>
        public IProcessResult P4(string CommandLine, string Input = null, bool AllowSpew = true, bool WithClient = true, bool SpewIsVerbose = false)
		{
			CheckP4Enabled();
			CommandUtils.ERunOptions RunOptions = AllowSpew ? CommandUtils.ERunOptions.AllowSpew : CommandUtils.ERunOptions.NoLoggingOfRunCommand;
			if( SpewIsVerbose )
			{
				RunOptions |= CommandUtils.ERunOptions.SpewIsVerbose;
			}
            return CommandUtils.Run(HostPlatform.Current.P4Exe, (WithClient ? GlobalOptions : GlobalOptionsWithoutClient) + CommandLine, Input, Options:RunOptions);
		}

		/// <summary>
		/// Calls p4 and returns the output.
		/// </summary>
		/// <param name="Output">Output of the comman.</param>
		/// <param name="CommandLine">Commandline for p4.</param>
		/// <param name="Input">Stdin input.</param>
		/// <param name="AllowSpew">Whether the command should spew.</param>
		/// <returns>True if succeeded, otherwise false.</returns>
        public bool P4Output(out string Output, string CommandLine, string Input = null, bool AllowSpew = true, bool WithClient = true)
		{
			CheckP4Enabled();
			Output = "";

            var Result = P4(CommandLine, Input, AllowSpew, WithClient);

			Output = Result.Output;
			return Result.ExitCode == 0;
		}

		/// <summary>
		/// Calls p4 command and writes the output to a logfile.
		/// </summary>
		/// <param name="CommandLine">Commandline to pass to p4.</param>
		/// <param name="Input">Stdin input.</param>
		/// <param name="AllowSpew">Whether the command is allowed to spew.</param>
        public void LogP4(string CommandLine, string Input = null, bool AllowSpew = true, bool WithClient = true, bool SpewIsVerbose = false)
		{
			CheckP4Enabled();
			string Output;
            if (!LogP4Output(out Output, CommandLine, Input, AllowSpew, WithClient, SpewIsVerbose:SpewIsVerbose))
			{
				throw new P4Exception("p4.exe {0} failed.", CommandLine);
			}
		}

		/// <summary>
		/// Calls p4 and returns the output and writes it also to a logfile.
		/// </summary>
		/// <param name="Output">Output of the comman.</param>
		/// <param name="CommandLine">Commandline for p4.</param>
		/// <param name="Input">Stdin input.</param>
		/// <param name="AllowSpew">Whether the command should spew.</param>
		/// <returns>True if succeeded, otherwise false.</returns>
        public bool LogP4Output(out string Output, string CommandLine, string Input = null, bool AllowSpew = true, bool WithClient = true, bool SpewIsVerbose = false)
		{
			CheckP4Enabled();
			Output = "";

			if (String.IsNullOrEmpty(LogPath))
			{
				CommandUtils.LogError("P4Utils.SetupP4() must be called before issuing Peforce commands");
				return false;
			}

            var Result = P4(CommandLine, Input, AllowSpew, WithClient, SpewIsVerbose:SpewIsVerbose);

			CommandUtils.WriteToFile(LogPath, CommandLine + "\n");
			CommandUtils.WriteToFile(LogPath, Result.Output);
			Output = Result.Output;
			return Result.ExitCode == 0;
		}

		/// <summary>
		/// Invokes p4 login command.
		/// </summary>
		public string GetAuthenticationToken()
		{
			string AuthenticationToken = null;

			string Output;
            string P4Passwd = InternalUtils.GetEnvironmentVariable("uebp_PASS", "", true) + '\n';
            P4Output(out Output, "login -a -p", P4Passwd);

			// Validate output.
			const string PasswordPromptString = "Enter password: \r\n";
			if (Output.Substring(0, PasswordPromptString.Length) == PasswordPromptString)
			{
				int AuthenticationResultStartIndex = PasswordPromptString.Length;
				Regex TokenRegex = new Regex("[0-9A-F]{32}");
				Match TokenMatch = TokenRegex.Match(Output, AuthenticationResultStartIndex);
				if (TokenMatch.Success)
				{
					AuthenticationToken = Output.Substring(TokenMatch.Index, TokenMatch.Length);
				}
			}

			return AuthenticationToken;
		}

        /// <summary>
        /// Invokes p4 changes command.
        /// </summary>
        /// <param name="CommandLine">CommandLine to pass on to the command.</param>
        public class ChangeRecord
        {
            public int CL = 0;
            public string User = "";
            public string UserEmail = "";
            public string Summary = "";
            public static int Compare(ChangeRecord A, ChangeRecord B)
            {
                return (A.CL < B.CL) ? -1 : (A.CL > B.CL) ? 1 : 0;
            }
        }
        static Dictionary<string, string> UserToEmailCache = new Dictionary<string, string>();
        public string UserToEmail(string User)
        {
            if (UserToEmailCache.ContainsKey(User))
            {
                return UserToEmailCache[User];
            }
            string Result = "";
            try
            {
                var P4Result = P4(String.Format("user -o {0}", User), AllowSpew: false);
			    if (P4Result.ExitCode == 0)
			    {
				    var Tags = ParseTaggedP4Output(P4Result.Output);
                    Tags.TryGetValue("Email", out Result);
                }
            }
            catch(Exception)
            {
            }
            if (Result == "")
            {
				CommandUtils.LogWarning("Could not find email for P4 user {0}", User);
            }
            UserToEmailCache.Add(User, Result);
            return Result;
        }
        static Dictionary<string, List<ChangeRecord>> ChangesCache = new Dictionary<string, List<ChangeRecord>>();
        public bool Changes(out List<ChangeRecord> ChangeRecords, string CommandLine, bool AllowSpew = true, bool UseCaching = false, bool LongComment = false, bool WithClient = false)
        {
            // If the user specified '-l' or '-L', the summary will appear on subsequent lines (no quotes) instead of the same line (surrounded by single quotes)
            bool ContainsDashL = CommandLine.StartsWith("-L ", StringComparison.InvariantCultureIgnoreCase) ||
                CommandLine.IndexOf(" -L ", StringComparison.InvariantCultureIgnoreCase) > 0;
            bool bSummaryIsOnSameLine = !ContainsDashL;
            if (bSummaryIsOnSameLine && LongComment)
            {
                CommandLine = "-L " + CommandLine;
                bSummaryIsOnSameLine = false;
            } 
            if (UseCaching && ChangesCache.ContainsKey(CommandLine))
            {
                ChangeRecords = ChangesCache[CommandLine];
                return true;
            }
            ChangeRecords = new List<ChangeRecord>();
            CheckP4Enabled();
            try
            {
                // Change 1999345 on 2014/02/16 by buildmachine@BuildFarm_BUILD-23_buildmachine_++depot+UE4 'GUBP Node Shadow_LabelPromotabl'

                string Output;
                if (!LogP4Output(out Output, "changes " + CommandLine, null, AllowSpew, WithClient: WithClient))
                {
                    throw new AutomationException("P4 returned failure.");
                }

                var Lines = Output.Split(new string[] { Environment.NewLine }, StringSplitOptions.None);
                for(int LineIndex = 0; LineIndex < Lines.Length; ++LineIndex)
                {
					var Line = Lines[ LineIndex ];

					// If we've hit a blank line, then we're done
					if( String.IsNullOrEmpty( Line ) )
					{
						break;
					}

                    ChangeRecord Change = new ChangeRecord();
                    string MatchChange = "Change ";
                    string MatchOn = " on "; 
                    string MatchBy = " by ";

                    int ChangeAt = Line.IndexOf(MatchChange);
                    int OnAt = Line.IndexOf(MatchOn);
                    int ByAt = Line.IndexOf(MatchBy);
                    if (ChangeAt == 0 && OnAt > ChangeAt && ByAt > OnAt)
                    {
                        var ChangeString = Line.Substring(ChangeAt + MatchChange.Length, OnAt - ChangeAt - MatchChange.Length);
                        Change.CL = int.Parse(ChangeString);

						int AtAt = Line.IndexOf("@");
                        Change.User = Line.Substring(ByAt + MatchBy.Length, AtAt - ByAt - MatchBy.Length);

						if( bSummaryIsOnSameLine )
						{ 
							int TickAt = Line.IndexOf("'");
							int EndTick = Line.LastIndexOf("'");
							if( TickAt > ByAt && EndTick > TickAt )
							{ 
								Change.Summary = Line.Substring(TickAt + 1, EndTick - TickAt - 1);
							}
						}
						else
						{
							++LineIndex;
							if( LineIndex >= Lines.Length )
							{
								throw new AutomationException("Was expecting a change summary to appear after Change header output from P4, but there were no more lines to read");
							}

							Line = Lines[ LineIndex ];
							if( !String.IsNullOrEmpty( Line ) )
							{
                                throw new AutomationException("Was expecting blank line after Change header output from P4, got {0}", Line);
							}

							++LineIndex;
							for( ; LineIndex < Lines.Length; ++LineIndex )
							{
								Line = Lines[ LineIndex ];

								int SummaryChangeAt = Line.IndexOf(MatchChange);
								int SummaryOnAt = Line.IndexOf(MatchOn);
								int SummaryByAt = Line.IndexOf(MatchBy);
								if (SummaryChangeAt == 0 && SummaryOnAt > SummaryChangeAt && SummaryByAt > SummaryOnAt)
								{
									// OK, we found a new change. This isn't part of our summary.  We're done with the summary.  Back we go.
                                    //CommandUtils.Log("Next summary is {0}", Line);
									--LineIndex;
									break;
								}

								// Summary lines are supposed to begin with a single tab character (even empty lines)
								if( !String.IsNullOrEmpty( Line ) && Line[0] != '\t' )
								{
									throw new AutomationException("Was expecting every line of the P4 changes summary to start with a tab character or be totally empty");
								}

								// Remove the tab
								var SummaryLine = Line;
								if( Line.StartsWith( "\t" ) )
								{ 
									SummaryLine = Line.Substring( 1 );
								}

								// Add a CR if we already had some summary text
								if( !String.IsNullOrEmpty( Change.Summary ) )
								{
									Change.Summary += "\n";
								}

								// Append the summary line!
								Change.Summary += SummaryLine;
							}
						}
                        Change.UserEmail = UserToEmail(Change.User);
                        ChangeRecords.Add(Change);
                    }
					else
					{
						throw new AutomationException("Output of 'p4 changes' was not formatted how we expected.  Could not find 'Change', 'on' and 'by' in the output line: " + Line);
					}
                }
            }
			catch (Exception Ex)
            {
				CommandUtils.LogWarning("Unable to get P4 changes with {0}", CommandLine);
				CommandUtils.LogWarning(" Exception was {0}", LogUtils.FormatException(Ex));
                return false;
            }
            ChangeRecords.Sort((A, B) => ChangeRecord.Compare(A, B));
			if( ChangesCache.ContainsKey(CommandLine) )
			{
				ChangesCache[CommandLine] = ChangeRecords;
			}
			else
			{ 
				ChangesCache.Add(CommandLine, ChangeRecords);
			}
            return true;
        }

	
        public class DescribeRecord
        {
            public int CL = 0;
            public string User = "";
            public string UserEmail = "";
            public string Summary = "";
			public string Header = "";
			
			public class DescribeFile
			{
				public string File;
				public int Revision;
				public string ChangeType;
			}
			public List<DescribeFile> Files = new List<DescribeFile>();
            
			public static int Compare(DescribeRecord A, DescribeRecord B)
            {
                return (A.CL < B.CL) ? -1 : (A.CL > B.CL) ? 1 : 0;
            }
        }

		/// <summary>
		/// Wraps P4 describe
		/// </summary>
		/// <param name="Changelist">Changelist numbers to query full descriptions for</param>
		/// <param name="DescribeRecord">Describe record for the given changelist.</param>
		/// <param name="AllowSpew"></param>
		/// <returns>True if everything went okay</returns>
        public bool DescribeChangelist(int Changelist, out DescribeRecord DescribeRecord, bool AllowSpew = true)
        {
			List<DescribeRecord> DescribeRecords;
			if(!DescribeChangelists(new List<int>{ Changelist }, out DescribeRecords, AllowSpew))
			{
				DescribeRecord = null;
				return false;
			}
			else if(DescribeRecords.Count != 1)
			{
				DescribeRecord = null;
				return false;
			}
			else 
			{
				DescribeRecord = DescribeRecords[0];
				return true;
			}
		}

		/// <summary>
		/// Wraps P4 describe
		/// </summary>
		/// <param name="Changelists">List of changelist numbers to query full descriptions for</param>
		/// <param name="DescribeRecords">List of records we found.  One for each changelist number.  These will be sorted from oldest to newest.</param>
		/// <param name="AllowSpew"></param>
		/// <returns>True if everything went okay</returns>
        public bool DescribeChangelists(List<int> Changelists, out List<DescribeRecord> DescribeRecords, bool AllowSpew = true)
        {
			DescribeRecords = new List<DescribeRecord>();
            CheckP4Enabled();
            try
            {
				// Change 234641 by This.User@WORKSPACE-C2Q-67_Dev on 2008/05/06 10:32:32
				// 
				//         Desc Line 1
				// 
				// Affected files ...
				// 
				// ... //depot/UnrealEngine3/Development/Src/Engine/Classes/ArrowComponent.uc#8 edit
				// ... //depot/UnrealEngine3/Development/Src/Engine/Classes/DecalActorBase.uc#4 edit


				string Output;
				string CommandLine = "-s";		// Don't automatically diff the files
				
				// Add changelists to the command-line
				foreach( var Changelist in Changelists )
				{
					CommandLine += " " + Changelist.ToString();
				}

                if (!LogP4Output(out Output, "describe " + CommandLine, null, AllowSpew))
                {
                    return false;
                }

				int ChangelistIndex = 0;
				var Lines = Output.Split(new string[] { Environment.NewLine }, StringSplitOptions.None);
                for (var LineIndex = 0; LineIndex < Lines.Length; ++LineIndex)
                {
					var Line = Lines[ LineIndex ];

					// If we've hit a blank line, then we're done
					if( String.IsNullOrEmpty( Line ) )
					{
						break;
					}

                    string MatchChange = "Change ";
                    string MatchOn = " on "; 
                    string MatchBy = " by ";

                    int ChangeAt = Line.IndexOf(MatchChange);
                    int OnAt = Line.IndexOf(MatchOn);
                    int ByAt = Line.IndexOf(MatchBy);
                    int AtAt = Line.IndexOf("@");
                    if (ChangeAt == 0 && OnAt > ChangeAt && ByAt < OnAt)
                    {
                        var ChangeString = Line.Substring(ChangeAt + MatchChange.Length, ByAt - ChangeAt - MatchChange.Length);

						var CurrentChangelist = Changelists[ ChangelistIndex++ ];

                        if (!ChangeString.Equals( CurrentChangelist.ToString()))
                        {
                            throw new AutomationException("Was expecting changelists to be reported back in the same order we asked for them (CL {0} != {1})", ChangeString, CurrentChangelist.ToString());
                        }

						var DescribeRecord = new DescribeRecord();
						DescribeRecords.Add( DescribeRecord );

						DescribeRecord.CL = CurrentChangelist;
                        DescribeRecord.User = Line.Substring(ByAt + MatchBy.Length, AtAt - ByAt - MatchBy.Length);
						DescribeRecord.Header = Line;

						++LineIndex;
						if( LineIndex >= Lines.Length )
						{
							throw new AutomationException("Was expecting a change summary to appear after Change header output from P4, but there were no more lines to read");
						}

						Line = Lines[ LineIndex ];
						if( !String.IsNullOrEmpty( Line ) )
						{
							throw new AutomationException("Was expecting blank line after Change header output from P4");
						}

						// Summary
						++LineIndex;
						for( ; LineIndex < Lines.Length; ++LineIndex )
						{
							Line = Lines[ LineIndex ];
							if(Line.Length > 0)
							{
								// Stop once we reach a line that doesn't begin with a tab. It's possible (through changelist descriptions that contain embedded newlines, like \r\r\n on Windows) to get 
								// empty lines that don't begin with a tab as we expect.
								if(Line[0] != '\t')
								{
									break;
								}

								// Remove the tab
								var SummaryLine = Line.Substring( 1 );

								// Add a CR if we already had some summary text
								if( !String.IsNullOrEmpty( DescribeRecord.Summary ) )
								{
									DescribeRecord.Summary += "\n";
								}

								// Append the summary line!
								DescribeRecord.Summary += SummaryLine;
							}
						}

						// Remove any trailing newlines from the end of the summary
						DescribeRecord.Summary = DescribeRecord.Summary.TrimEnd('\n');

						Line = Lines[ LineIndex ];

						string MatchAffectedFiles = "Affected files";
						int AffectedFilesAt = Line.IndexOf(MatchAffectedFiles);
						if( AffectedFilesAt == 0 )
						{
							++LineIndex;
							if( LineIndex >= Lines.Length )
							{
								throw new AutomationException("Was expecting a list of files to appear after Affected Files header output from P4, but there were no more lines to read");
							}

							Line = Lines[ LineIndex ];
							if( !String.IsNullOrEmpty( Line ) )
							{
								throw new AutomationException("Was expecting blank line after Affected Files header output from P4");
							}

							// Files
							++LineIndex;
							for( ; LineIndex < Lines.Length; ++LineIndex )
							{
								Line = Lines[ LineIndex ];

								if( String.IsNullOrEmpty( Line ) )
								{
									// Summaries end with a blank line (no tabs)
									break;
								}

								// File lines are supposed to begin with a "... " string
								if( !Line.StartsWith( "... " ) )
								{
									throw new AutomationException("Was expecting every line of the P4 describe files to start with a tab character");
								}

								// Remove the "... " prefix
								var FilesLine = Line.Substring( 4 );

								var DescribeFile = new DescribeRecord.DescribeFile();
								DescribeRecord.Files.Add( DescribeFile );
 							
								// Find the revision #
								var RevisionNumberAt = FilesLine.LastIndexOf( "#" ) + 1;
								var ChangeTypeAt = 1 + FilesLine.IndexOf( " ", RevisionNumberAt );
							
								DescribeFile.File = FilesLine.Substring( 0, RevisionNumberAt - 1 );
								string RevisionString = FilesLine.Substring( RevisionNumberAt, ChangeTypeAt - RevisionNumberAt );
								DescribeFile.Revision = int.Parse( RevisionString );
								DescribeFile.ChangeType = FilesLine.Substring( ChangeTypeAt );															  
							}
						}
						else
						{
							throw new AutomationException("Output of 'p4 describe' was not formatted how we expected.  Could not find 'Affected files' in the output line: " + Line);
						}

                        DescribeRecord.UserEmail = UserToEmail(DescribeRecord.User);
                    }
					else
					{
						throw new AutomationException("Output of 'p4 describe' was not formatted how we expected.  Could not find 'Change', 'on' and 'by' in the output line: " + Line);
					}
                }
            }
            catch (Exception)
            {
                return false;
            }
            DescribeRecords.Sort((A, B) => DescribeRecord.Compare(A, B));
            return true;
        }

		/// <summary>
		/// Invokes p4 sync command.
		/// </summary>
		/// <param name="CommandLine">CommandLine to pass on to the command.</param>
        public void Sync(string CommandLine, bool AllowSpew = true, bool SpewIsVerbose = false)
		{
			CheckP4Enabled();
			LogP4("sync " + CommandLine, null, AllowSpew, SpewIsVerbose:SpewIsVerbose);
		}

		/// <summary>
		/// Invokes p4 unshelve command.
		/// </summary>
		/// <param name="FromCL">Changelist to unshelve.</param>
		/// <param name="ToCL">Changelist where the checked out files should be added.</param>
		/// <param name="CommandLine">Commandline for the command.</param>
		public void Unshelve(int FromCL, int ToCL, string CommandLine = "", bool SpewIsVerbose = false)
		{
			CheckP4Enabled();
			LogP4("unshelve " + String.Format("-s {0} ", FromCL) + String.Format("-c {0} ", ToCL) + CommandLine, SpewIsVerbose: SpewIsVerbose);
		}

        /// <summary>
        /// Invokes p4 unshelve command.
        /// </summary>
        /// <param name="FromCL">Changelist to unshelve.</param>
        /// <param name="ToCL">Changelist where the checked out files should be added.</param>
        /// <param name="CommandLine">Commandline for the command.</param>
        public void Shelve(int FromCL, string CommandLine = "", bool AllowSpew = true)
        {
            CheckP4Enabled();
            LogP4("shelve " + String.Format("-r -c {0} ", FromCL) + CommandLine, AllowSpew: AllowSpew);
        }

		/// <summary>
        /// Deletes shelved files from a changelist
		/// </summary>
        /// <param name="FromCL">Changelist to unshelve.</param>
        /// <param name="CommandLine">Commandline for the command.</param>
        public void DeleteShelvedFiles(int FromCL, bool AllowSpew = true)
        {
            CheckP4Enabled();

			string Output;
            if (!LogP4Output(out Output, String.Format("shelve -d -c {0}", FromCL), AllowSpew: AllowSpew) && !Output.StartsWith("No shelved files in changelist to delete."))
			{
				throw new P4Exception("Couldn't unshelve files: {0}", Output);
			}
        }

		/// <summary>
		/// Invokes p4 edit command.
		/// </summary>
		/// <param name="CL">Changelist where the checked out files should be added.</param>
		/// <param name="CommandLine">Commandline for the command.</param>
		public void Edit(int CL, string CommandLine)
		{
			CheckP4Enabled();
			LogP4("edit " + String.Format("-c {0} ", CL) + CommandLine);
		}

		/// <summary>
		/// Invokes p4 edit command, no exceptions
		/// </summary>
		/// <param name="CL">Changelist where the checked out files should be added.</param>
		/// <param name="CommandLine">Commandline for the command.</param>
		public bool Edit_NoExceptions(int CL, string CommandLine)
		{
			try
			{
				CheckP4Enabled();
				string Output;
				if (!LogP4Output(out Output, "edit " + String.Format("-c {0} ", CL) + CommandLine, null, true))
				{
					return false;
				}
				if (Output.IndexOf("- opened for edit") < 0)
				{
					return false;
				}
				return true;
			}
			catch (Exception)
			{
				return false;
			}
		}

		/// <summary>
		/// Invokes p4 add command.
		/// </summary>
		/// <param name="CL">Changelist where the files should be added to.</param>
		/// <param name="CommandLine">Commandline for the command.</param>
		public void Add(int CL, string CommandLine)
		{
			CheckP4Enabled();
			LogP4("add " + String.Format("-c {0} ", CL) + CommandLine);
		}

		/// <summary>
		/// Invokes p4 reconcile command.
		/// </summary>
		/// <param name="CL">Changelist to check the files out.</param>
		/// <param name="CommandLine">Commandline for the command.</param>
		public void Reconcile(int CL, string CommandLine, bool AllowSpew = true)
		{
			CheckP4Enabled();
			LogP4("reconcile " + String.Format("-c {0} -ead -f ", CL) + CommandLine, AllowSpew: AllowSpew);
		}

        /// <summary>
        /// Invokes p4 reconcile command.
        /// </summary>
        /// <param name="CL">Changelist to check the files out.</param>
        /// <param name="CommandLine">Commandline for the command.</param>
        public void ReconcilePreview(string CommandLine)
        {
            CheckP4Enabled();
            LogP4("reconcile " + String.Format("-ead -n ") + CommandLine);
        }

		/// <summary>
		/// Invokes p4 reconcile command.
		/// Ignores files that were removed.
		/// </summary>
		/// <param name="CL">Changelist to check the files out.</param>
		/// <param name="CommandLine">Commandline for the command.</param>
		public void ReconcileNoDeletes(int CL, string CommandLine, bool AllowSpew = true)
		{
			CheckP4Enabled();
			LogP4("reconcile " + String.Format("-c {0} -ea ", CL) + CommandLine, AllowSpew: AllowSpew);
		}

		/// <summary>
		/// Invokes p4 resolve command.
		/// Resolves all files by accepting yours and ignoring theirs.
		/// </summary>
		/// <param name="CL">Changelist to resolve.</param>
		/// <param name="CommandLine">Commandline for the command.</param>
		public void Resolve(int CL, string CommandLine)
		{
			CheckP4Enabled();
			LogP4("resolve -ay " + String.Format("-c {0} ", CL) + CommandLine);
		}

		/// <summary>
		/// Invokes revert command.
		/// </summary>
		/// <param name="CommandLine">Commandline for the command.</param>
		public void Revert(string CommandLine, bool AllowSpew = true)
		{
			CheckP4Enabled();
			LogP4("revert " + CommandLine, AllowSpew: AllowSpew);
		}

		/// <summary>
		/// Invokes revert command.
		/// </summary>
		/// <param name="CL">Changelist to revert</param>
		/// <param name="CommandLine">Commandline for the command.</param>
		public void Revert(int CL, string CommandLine = "", bool AllowSpew = true)
		{
			CheckP4Enabled();
			LogP4("revert " + String.Format("-c {0} ", CL) + CommandLine, AllowSpew: AllowSpew);
		}

		/// <summary>
		/// Reverts all unchanged file from the specified changelist.
		/// </summary>
		/// <param name="CL">Changelist to revert the unmodified files from.</param>
		public void RevertUnchanged(int CL)
		{
			CheckP4Enabled();
			// caution this is a really bad idea if you hope to force submit!!!
			LogP4("revert -a " + String.Format("-c {0} ", CL));
		}

		/// <summary>
		/// Reverts all files from the specified changelist.
		/// </summary>
		/// <param name="CL">Changelist to revert.</param>
		public void RevertAll(int CL, bool SpewIsVerbose = false)
		{
			CheckP4Enabled();
			LogP4("revert " + String.Format("-c {0} //...", CL), SpewIsVerbose: SpewIsVerbose);
		}

		/// <summary>
		/// Submits the specified changelist.
		/// </summary>
		/// <param name="CL">Changelist to submit.</param>
		/// <param name="SubmittedCL">Will be set to the submitted changelist number.</param>
		/// <param name="Force">If true, the submit will be forced even if resolve is needed.</param>
		/// <param name="RevertIfFail">If true, if the submit fails, revert the CL.</param>
		public void Submit(int CL, out int SubmittedCL, bool Force = false, bool RevertIfFail = false)
		{
			CheckP4Enabled();
			if (!CommandUtils.AllowSubmit)
			{
				throw new P4Exception("Submit is not allowed currently. Please use the -Submit switch to override that.");
			}

			SubmittedCL = 0;
			int Retry = 0;
			string LastCmdOutput = "none?";
			while (Retry++ < 48)
			{
				bool Pending;
				if (!ChangeExists(CL, out Pending))
				{
					throw new P4Exception("Change {0} does not exist.", CL);
				}
				if (!Pending)
				{
					throw new P4Exception("Change {0} was not pending.", CL);
				}
                bool isClPending = false;
                if (ChangeFiles(CL, out isClPending, false).Count == 0)
                {
					CommandUtils.Log("No edits left to commit after brutal submit resolve. Assuming another build committed same changes already and exiting as success.");
                    DeleteChange(CL);
                    // No changes to submit, no need to retry.
                    return;
                }
				string CmdOutput;
				if (!LogP4Output(out CmdOutput, String.Format("submit -c {0}", CL)))
				{
					if (!Force)
					{
						throw new P4Exception("Change {0} failed to submit.\n{1}", CL, CmdOutput);
					}
					CommandUtils.Log("**** P4 Returned\n{0}\n*******", CmdOutput);

					LastCmdOutput = CmdOutput;
					bool DidSomething = false;

                    string[] KnownProblems =
                    {
                        " - must resolve",
                        " - already locked by",
                        " - add of added file",
                        " - edit of deleted file",
                    };

                    bool AnyIssue = false;
                    foreach (var ProblemString in KnownProblems)
                    {
                        int ThisIndex = CmdOutput.IndexOf(ProblemString);
                        if (ThisIndex > 0)
                        {
                            AnyIssue = true;
                            break;
                        }
                    }

                    if (AnyIssue)
                    {
                        string Work = CmdOutput;
                        HashSet<string> AlreadyDone = new HashSet<string>();
                        while (Work.Length > 0)
                        {
                            string SlashSlashStr = "//";
                            int SlashSlash = Work.IndexOf(SlashSlashStr);
                            if (SlashSlash < 0)
                            {
                                break;
                            }
                            Work = Work.Substring(SlashSlash);
                            int MinMatch = Work.Length + 1;
                            foreach (var ProblemString in KnownProblems)
                            {
                                int ThisIndex = Work.IndexOf(ProblemString);
                                if (ThisIndex >= 0 && ThisIndex < MinMatch)
                                {
                                    MinMatch = ThisIndex;
                                }
                            }
                            if (MinMatch > Work.Length)
                            {
                                break;
                            }                            
                            string File = Work.Substring(0, MinMatch).Trim();                            
                            if (File.IndexOf(SlashSlashStr) != File.LastIndexOf(SlashSlashStr))
                            {
                                // this is some other line about the same line, we ignore it, removing the first // so we advance
                                Work = Work.Substring(SlashSlashStr.Length);
                            }
                            else
                            {
                                Work = Work.Substring(MinMatch);
                                if (AlreadyDone.Contains(File))
                                {
                                    continue;
                                }
								CommandUtils.Log("Brutal 'resolve' on {0} to force submit.\n", File);
								Revert(CL, "-k " + CommandUtils.MakePathSafeToUseWithCommandLine(File));  // revert the file without overwriting the local one
								Sync("-f -k " + CommandUtils.MakePathSafeToUseWithCommandLine(File + "#head"), false); // sync the file without overwriting local one
								ReconcileNoDeletes(CL, CommandUtils.MakePathSafeToUseWithCommandLine(File));  // re-check out, if it changed, or add
                                DidSomething = true;
                                AlreadyDone.Add(File);															
                            }
                        }
                    }
					if (!DidSomething)
					{
						CommandUtils.Log("Change {0} failed to submit for reasons we do not recognize.\n{1}\nWaiting and retrying.", CL, CmdOutput);
					}
					System.Threading.Thread.Sleep(30000);
				}
				else
				{
					LastCmdOutput = CmdOutput;
					if (CmdOutput.Trim().EndsWith("submitted."))
					{
						if (CmdOutput.Trim().EndsWith(" and submitted."))
						{
							string EndStr = " and submitted.";
							string ChangeStr = "renamed change ";
							int Offset = CmdOutput.LastIndexOf(ChangeStr);
							int EndOffset = CmdOutput.LastIndexOf(EndStr);
							if (Offset >= 0 && Offset < EndOffset)
							{
								SubmittedCL = int.Parse(CmdOutput.Substring(Offset + ChangeStr.Length, EndOffset - Offset - ChangeStr.Length));
							}
						}
						else
						{
							string EndStr = " submitted.";
							string ChangeStr = "Change ";
							int Offset = CmdOutput.LastIndexOf(ChangeStr);
							int EndOffset = CmdOutput.LastIndexOf(EndStr);
							if (Offset >= 0 && Offset < EndOffset)
							{
								SubmittedCL = int.Parse(CmdOutput.Substring(Offset + ChangeStr.Length, EndOffset - Offset - ChangeStr.Length));
							}
						}

						CommandUtils.Log("Submitted CL {0} which became CL {1}\n", CL, SubmittedCL);
					}

					if (SubmittedCL < CL)
					{
						throw new P4Exception("Change {0} submission seemed to succeed, but did not look like it.\n{1}", CL, CmdOutput);
					}

					// Change submitted OK!  No need to retry.
					return;
				}
			}
			if (RevertIfFail)
			{
				CommandUtils.LogError("Submit CL {0} failed, reverting files\n", CL);
				RevertAll(CL);
				CommandUtils.LogError("Submit CL {0} failed, reverting files\n", CL);
			}
			throw new P4Exception("Change {0} failed to submit after 48 retries??.\n{1}", CL, LastCmdOutput);
		}

		/// <summary>
		/// Creates a new changelist with the specified owner and description.
		/// </summary>
		/// <param name="Owner">Owner of the changelist.</param>
		/// <param name="Description">Description of the changelist.</param>
		/// <returns>Id of the created changelist.</returns>
		public int CreateChange(string Owner = null, string Description = null, bool AllowSpew = false)
		{
			CheckP4Enabled();
			var ChangeSpec = "Change: new" + "\n";
			ChangeSpec += "Client: " + ((Owner != null) ? Owner : "") + "\n";
			ChangeSpec += "Description: " + ((Description != null) ? Description.Replace("\n", "\n\t") : "(none)") + "\n";
			string CmdOutput;
			int CL = 0;
			if(AllowSpew)
			{
				CommandUtils.Log("Creating Change\n {0}\n", ChangeSpec);
			}
			if (LogP4Output(out CmdOutput, "change -i", Input: ChangeSpec, AllowSpew: AllowSpew))
			{
				string EndStr = " created.";
				string ChangeStr = "Change ";
				int Offset = CmdOutput.LastIndexOf(ChangeStr);
				int EndOffset = CmdOutput.LastIndexOf(EndStr);
				if (Offset >= 0 && Offset < EndOffset)
				{
					CL = int.Parse(CmdOutput.Substring(Offset + ChangeStr.Length, EndOffset - Offset - ChangeStr.Length));
				}
			}
			if (CL <= 0)
			{
				throw new P4Exception("Failed to create Changelist. Owner: {0} Desc: {1}", Owner, Description);
			}
			else if(AllowSpew)
			{
				CommandUtils.Log("Returned CL {0}\n", CL);
			}
			return CL;
		}


		/// <summary>
		/// Updates a changelist with the given fields
		/// </summary>
		/// <param name="CL"></param>
		/// <param name="NewOwner"></param>
		/// <param name="NewDescription"></param>
		/// <param name="SpewIsVerbose"></param>
		public void UpdateChange(int CL, string NewOwner, string NewDescription, bool SpewIsVerbose = false)
		{
			CheckP4Enabled();

			string CmdOutput;
			if(!LogP4Output(out CmdOutput, String.Format("change -o {0}", CL), SpewIsVerbose: SpewIsVerbose))
			{
				throw new P4Exception("Couldn't describe changelist {0}", CL);
			}

			P4Spec Spec = P4Spec.FromString(CmdOutput);
			if(NewOwner != null)
			{
				Spec.SetField("Client", NewOwner);
			}
			if(NewDescription != null)
			{
				Spec.SetField("Description", NewDescription);
			}

			if(!LogP4Output(out CmdOutput, "change -i", Input: Spec.ToString(), SpewIsVerbose: SpewIsVerbose))
			{
				throw new P4Exception("Failed to update spec for changelist {0}", CL);
			}
			if(!CmdOutput.TrimEnd().EndsWith(String.Format("Change {0} updated.", CL)))
			{
				throw new P4Exception("Unexpected output from p4 change -i: {0}", CmdOutput);
			}
		}

		/// <summary>
		/// Deletes the specified changelist.
		/// </summary>
		/// <param name="CL">Changelist to delete.</param>
		/// <param name="RevertFiles">Indicates whether files in that changelist should be reverted.</param>
		public void DeleteChange(int CL, bool RevertFiles = true, bool SpewIsVerbose = false, bool AllowSpew = true)
		{
			CheckP4Enabled();
			if (RevertFiles)
			{
				RevertAll(CL, SpewIsVerbose: SpewIsVerbose);
			}

			string CmdOutput;
			if (LogP4Output(out CmdOutput, String.Format("change -d {0}", CL), SpewIsVerbose: SpewIsVerbose, AllowSpew: AllowSpew))
			{
				string EndStr = " deleted.";
				string ChangeStr = "Change ";
				int Offset = CmdOutput.LastIndexOf(ChangeStr);
				int EndOffset = CmdOutput.LastIndexOf(EndStr);
				if (Offset == 0 && Offset < EndOffset)
				{
					return;
				}
			}
			throw new P4Exception("Could not delete change {0} output follows\n{1}", CL, CmdOutput);
		}

		/// <summary>
		/// Tries to delete the specified empty changelist.
		/// </summary>
		/// <param name="CL">Changelist to delete.</param>
		/// <returns>True if the changelist was deleted, false otherwise.</returns>
		public bool TryDeleteEmptyChange(int CL)
		{
			CheckP4Enabled();

			string CmdOutput;
			if (LogP4Output(out CmdOutput, String.Format("change -d {0}", CL)))
			{
				string EndStr = " deleted.";
				string ChangeStr = "Change ";
				int Offset = CmdOutput.LastIndexOf(ChangeStr);
				int EndOffset = CmdOutput.LastIndexOf(EndStr);
				if (Offset == 0 && Offset < EndOffset && !CmdOutput.Contains("can't be deleted."))
				{
					return true;
				}
			}

			return false;
		}

		/// <summary>
		/// Returns the changelist specification.
		/// </summary>
		/// <param name="CL">Changelist to get the specification from.</param>
		/// <returns>Specification of the changelist.</returns>
		public string ChangeOutput(int CL, bool AllowSpew = true)
		{
			CheckP4Enabled();
			string CmdOutput;
			if (LogP4Output(out CmdOutput, String.Format("change -o {0}", CL), AllowSpew: AllowSpew))
			{
				return CmdOutput;
			}
			throw new P4Exception("ChangeOutput failed {0} output follows\n{1}", CL, CmdOutput);
		}

		/// <summary>
		/// Checks whether the specified changelist exists.
		/// </summary>
		/// <param name="CL">Changelist id.</param>
		/// <param name="Pending">Whether it is a pending changelist.</param>
		/// <returns>Returns whether the changelist exists.</returns>
		public bool ChangeExists(int CL, out bool Pending, bool AllowSpew = true)
		{
			CheckP4Enabled();
			string CmdOutput = ChangeOutput(CL, AllowSpew);
			Pending = false;
			if (CmdOutput.Length > 0)
			{
				string EndStr = " unknown.";
				string ChangeStr = "Change ";
				int Offset = CmdOutput.LastIndexOf(ChangeStr);
				int EndOffset = CmdOutput.LastIndexOf(EndStr);
				if (Offset == 0 && Offset < EndOffset)
				{
					CommandUtils.Log("Change {0} does not exist", CL);
					return false;
				}

				string StatusStr = "Status:";
				int StatusOffset = CmdOutput.LastIndexOf(StatusStr);

				if (StatusOffset < 1)
				{
					CommandUtils.LogError("Change {0} could not be parsed\n{1}", CL, CmdOutput);
					return false;
				}

				string Status = CmdOutput.Substring(StatusOffset + StatusStr.Length).TrimStart().Split('\n')[0].TrimEnd();
				CommandUtils.Log("Change {0} exists ({1})", CL, Status);
				Pending = (Status == "pending");
				return true;
			}
			CommandUtils.LogError("Change exists failed {0} no output?", CL, CmdOutput);
			return false;
		}

		/// <summary>
		/// Returns a list of files contained in the specified changelist.
		/// </summary>
		/// <param name="CL">Changelist to get the files from.</param>
		/// <param name="Pending">Whether the changelist is a pending one.</param>
		/// <returns>List of the files contained in the changelist.</returns>
		public List<string> ChangeFiles(int CL, out bool Pending, bool AllowSpew = true)
		{
			CheckP4Enabled();
			var Result = new List<string>();

			if (ChangeExists(CL, out Pending, AllowSpew))
			{
				string CmdOutput = ChangeOutput(CL, AllowSpew);
				if (CmdOutput.Length > 0)
				{

					string FilesStr = "Files:";
					int FilesOffset = CmdOutput.LastIndexOf(FilesStr);
					if (FilesOffset < 0)
					{
						throw new P4Exception("Change {0} returned bad output\n{1}", CL, CmdOutput);
					}
					else
					{
						CmdOutput = CmdOutput.Substring(FilesOffset + FilesStr.Length);
						while (CmdOutput.Length > 0)
						{
							string SlashSlashStr = "//";
							int SlashSlash = CmdOutput.IndexOf(SlashSlashStr);
							if (SlashSlash < 0)
							{
								break;
							}
							CmdOutput = CmdOutput.Substring(SlashSlash);
							string HashStr = "#";
							int Hash = CmdOutput.IndexOf(HashStr);
							if (Hash < 0)
							{
								break;
							}
							string File = CmdOutput.Substring(0, Hash).Trim();
							CmdOutput = CmdOutput.Substring(Hash);

							Result.Add(File);
						}
					}
				}
			}
			else
			{
				throw new P4Exception("Change {0} did not exist.", CL);
			}
			return Result;
		}

        /// <summary>
        /// Returns the output from p4 opened
        /// </summary>
        /// <param name="CL">Changelist to get the specification from.</param>
        /// <returns>Specification of the changelist.</returns>
        public string OpenedOutput()
        {
            CheckP4Enabled();
            string CmdOutput;
            if (LogP4Output(out CmdOutput, "opened"))
            {
                return CmdOutput;
            }
            throw new P4Exception("OpenedOutput failed, output follows\n{0}", CmdOutput);
        }
		/// <summary>
		/// Deletes the specified label.
		/// </summary>
		/// <param name="LabelName">Label to delete.</param>
        public void DeleteLabel(string LabelName, bool AllowSpew = true)
		{
			CheckP4Enabled();
			var CommandLine = "label -d " + LabelName;

			// NOTE: We don't throw exceptions when trying to delete a label
			string Output;
			if (!LogP4Output(out Output, CommandLine, null, AllowSpew))
			{
				CommandUtils.Log("Couldn't delete label '{0}'.  It may not have existed in the first place.", LabelName);
			}
		}

		/// <summary>
		/// Creates a new label.
		/// </summary>
		/// <param name="Name">Name of the label.</param>
		/// <param name="Options">Options for the label. Valid options are "locked", "unlocked", "autoreload" and "noautoreload".</param>
		/// <param name="View">View mapping for the label.</param>
		/// <param name="Owner">Owner of the label.</param>
		/// <param name="Description">Description of the label.</param>
		/// <param name="Date">Date of the label creation.</param>
		/// <param name="Time">Time of the label creation</param>
		public void CreateLabel(string Name, string Options, string View, string Owner = null, string Description = null, string Date = null, string Time = null)
		{
			CheckP4Enabled();
			var LabelSpec = "Label: " + Name + "\n";
			LabelSpec += "Owner: " + ((Owner != null) ? Owner : "") + "\n";
			LabelSpec += "Description: " + ((Description != null) ? Description : "") + "\n";
			if (Date != null)
			{
				LabelSpec += " Date: " + Date + "\n";
			}
			if (Time != null)
			{
				LabelSpec += " Time: " + Time + "\n";
			}
			LabelSpec += "Options: " + Options + "\n";
			LabelSpec += "View: \n";
			LabelSpec += " " + View;

			CommandUtils.Log("Creating Label\n {0}\n", LabelSpec);
			LogP4("label -i", Input: LabelSpec);
		}

		/// <summary>
		/// Invokes p4 tag command.
		/// Associates a named label with a file revision.
		/// </summary>
		/// <param name="LabelName">Name of the label.</param>
		/// <param name="FilePath">Path to the file.</param>
		/// <param name="AllowSpew">Whether the command is allowed to spew.</param>
		public void Tag(string LabelName, string FilePath, bool AllowSpew = true)
		{
			CheckP4Enabled();
			LogP4("tag -l " + LabelName + " " + FilePath, null, AllowSpew);
		}

		/// <summary>
		/// Syncs a label to the current content of the client.
		/// </summary>
		/// <param name="LabelName">Name of the label.</param>
		/// <param name="AllowSpew">Whether the command is allowed to spew.</param>
		public void LabelSync(string LabelName, bool AllowSpew = true, string FileToLabel = "")
		{
			CheckP4Enabled();
			string Quiet = "";
			if (!AllowSpew)
			{
				Quiet = "-q ";
			}
			if (FileToLabel == "")
			{
				LogP4("labelsync " + Quiet + "-l " + LabelName);
			}
			else
			{
				LogP4("labelsync " + Quiet + "-l" + LabelName + " " + FileToLabel);
			}
		}

		/// <summary>
		/// Syncs a label from another label.
		/// </summary>
		/// <param name="FromLabelName">Source label name.</param>
		/// <param name="ToLabelName">Target label name.</param>
		/// <param name="AllowSpew">Whether the command is allowed to spew.</param>
		public void LabelToLabelSync(string FromLabelName, string ToLabelName, bool AllowSpew = true)
		{
			CheckP4Enabled();
			string Quiet = "";
			if (!AllowSpew)
			{
				Quiet = "-q ";
			}
			LogP4("labelsync -a " + Quiet + "-l " + ToLabelName + " //...@" + FromLabelName);
		}

		/// <summary>
		/// Checks whether the specified label exists and has any files.
		/// </summary>
		/// <param name="Name">Name of the label.</param>
		/// <returns>Whether there is an label with files.</returns>
		public bool LabelExistsAndHasFiles(string Name)
		{
			CheckP4Enabled();
			string Output;
			return LogP4Output(out Output, "files -m 1 //...@" + Name);
		}

		/// <summary>
		/// Returns the label description.
		/// </summary>
		/// <param name="Name">Name of the label.</param>
		/// <param name="Description">Description of the label.</param>
		/// <param name="AllowSpew">Whether to allow log spew</param>
		/// <returns>Returns whether the label description could be retrieved.</returns>
		public bool LabelDescription(string Name, out string Description, bool AllowSpew = true)
		{
			CheckP4Enabled();
			string Output;
			Description = "";
			if (LogP4Output(out Output, "label -o " + Name, AllowSpew: AllowSpew))
			{
				string Desc = "Description:";
				int Start = Output.LastIndexOf(Desc);
				if (Start > 0)
				{
					Start += Desc.Length;
				}
				int End = Output.LastIndexOf("Options:");
				if (Start > 0 && End > 0 && End > Start)
				{
					Description = Output.Substring(Start, End - Start).Replace("\n\t", "\n");
					Description = Description.Trim();
					return true;
				}
			}
			return false;
		}

		/// <summary>
		/// Reads a label spec
		/// </summary>
		/// <param name="Name">Label name</param>
		/// <param name="AllowSpew">Whether to allow log spew</param>
		public P4Spec ReadLabelSpec(string Name, bool AllowSpew = true)
		{
			string LabelSpec;
			if(!LogP4Output(out LabelSpec, "label -o " + Name, AllowSpew: AllowSpew))
			{
				throw new P4Exception("Couldn't describe existing label '{0}', output was:\n", Name, LabelSpec);
			}
			return P4Spec.FromString(LabelSpec);
		}

		/// <summary>
		/// Updates a label with a new spec
		/// </summary>
		/// <param name="Spec">Label specification</param>
		/// <param name="AllowSpew">Whether to allow log spew</param>
		public void UpdateLabelSpec(P4Spec Spec, bool AllowSpew = true)
		{
			LogP4("label -i", Input: Spec.ToString(), AllowSpew: AllowSpew);
		}

		/// <summary>
		/// Updates a label description.
		/// </summary>
		/// <param name="Name">Name of the label</param>
		/// <param name="Description">Description of the label.</param>
		/// <param name="AllowSpew">Whether to allow log spew</param>
		public void UpdateLabelDescription(string Name, string NewDescription, bool AllowSpew = true)
		{
			string LabelSpec;
			if(!LogP4Output(out LabelSpec, "label -o " + Name, AllowSpew: AllowSpew))
			{
				throw new P4Exception("Couldn't describe existing label '{0}', output was:\n", Name, LabelSpec);
			}
			List<string> Lines = new List<string>(LabelSpec.Split('\n').Select(x => x.TrimEnd()));

			// Find the description text, and remove it
			int Idx = 0;
			for(; Idx < Lines.Count; Idx++)
			{
				if(Lines[Idx].StartsWith("Description:"))
				{
					int EndIdx = Idx + 1;
					while(EndIdx < Lines.Count && (Lines[EndIdx].Length == 0 || Char.IsWhiteSpace(Lines[EndIdx][0]) || Lines[EndIdx].IndexOf(':') == -1))
					{
						EndIdx++;
					}
					Lines.RemoveRange(Idx, EndIdx - Idx);
					break;
				}
			}

			// Insert the new description text
			Lines.Insert(Idx, "Description:  " + NewDescription.Replace("\n", "\n\t"));
			LabelSpec = String.Join("\n", Lines);

			// Update the label
			LogP4("label -i", Input: LabelSpec, AllowSpew: AllowSpew);
		}

		/* Pattern to parse P4 changes command output. */
		private static readonly Regex ChangesListOutputPattern = new Regex(@"^Change\s+(?<number>\d+)\s+.+$", RegexOptions.Compiled | RegexOptions.Multiline);

		/// <summary>
		/// Gets the latest CL number submitted to the depot. It equals to the @head.
		/// </summary>
		/// <returns>The head CL number.</returns>
		public int GetLatestCLNumber()
		{
			CheckP4Enabled();

			string Output;
			if (!LogP4Output(out Output, "changes -s submitted -m1") || string.IsNullOrWhiteSpace(Output))
			{
				throw new InvalidOperationException("The depot should have at least one submitted changelist. Brand new depot?");
			}

			var Match = ChangesListOutputPattern.Match(Output);

			if (!Match.Success)
			{
				throw new InvalidOperationException("The Perforce output is not in the expected format provided by 2014.1 documentation.");
			}

			return Int32.Parse(Match.Groups["number"].Value);
		}

		/* Pattern to parse P4 labels command output. */
		static readonly Regex LabelsListOutputPattern = new Regex(@"^Label\s+(?<name>[\w\/\.-]+)\s+(?<date>\d{4}/\d{2}/\d{2} \d{2}:\d{2}:\d{2})\s+'(?<description>.+)'\s*$", RegexOptions.Compiled | RegexOptions.Multiline);

		/// <summary>
		/// Gets all labels satisfying given filter.
		/// </summary>
		/// <param name="Filter">Filter for label names.</param>
		/// <param name="bCaseSensitive">Treat filter as case-sensitive.</param>
		/// <returns></returns>
		public P4Label[] GetLabels(string Filter, bool bCaseSensitive = true)
		{
			var LabelList = new List<P4Label>();

			string Output;
			if (P4Output(out Output, "labels -t " + (bCaseSensitive ? "-e" : "-E") + Filter, null, false))
			{
				foreach (Match LabelMatch in LabelsListOutputPattern.Matches(Output))
				{
					LabelList.Add(new P4Label(LabelMatch.Groups["name"].Value,
						DateTime.ParseExact(
							LabelMatch.Groups["date"].Value, "yyyy/MM/dd HH:mm:ss",
							System.Globalization.CultureInfo.InvariantCulture)
					));
				}
			}

			return LabelList.ToArray();
		}

		/// <summary>
		/// Validate label for some content.
		/// </summary>
		/// <returns>True if label exists and has at least one file tagged. False otherwise.</returns>
		public bool ValidateLabelContent(string LabelName)
		{
			string Output;
			if (P4Output(out Output, "files -m 1 @" + LabelName, null, false))
			{
				if (Output.StartsWith("//depot"))
				{
					// If it starts with depot path then label has at least one file tagged in it.
					return true;
				}
			}
			else
			{
				throw new InvalidOperationException("For some reason P4 files failed.");
			}

			return false;
		}

		/// <summary>
        /// returns the full name of a label. //depot/UE4/TEST-GUBP-Promotable-GameName-CL-CLNUMBER
		/// </summary>
		/// <param name="BuildNamePrefix">Label Prefix</param>
        public string FullLabelName(P4Environment Env, string BuildNamePrefix)
        {
            CheckP4Enabled();
			var Label = Env.LabelPrefix + BuildNamePrefix + "-CL-" + Env.ChangelistString;
			CommandUtils.LogLog("Label prefix {0}", BuildNamePrefix);
			CommandUtils.LogLog("Full Label name {0}", Label); 
            return Label;
        }

		/// <summary>
		/// Creates a downstream label.
		/// </summary>
		/// <param name="BuildNamePrefix">Label Prefix</param>
		public void MakeDownstreamLabel(P4Environment Env, string BuildNamePrefix, List<string> Files = null)
		{
			CheckP4Enabled();
			string DOWNSTREAM_LabelPrefix = CommandUtils.GetEnvVar("DOWNSTREAM_LabelPrefix");
			if (!String.IsNullOrEmpty(DOWNSTREAM_LabelPrefix))
			{
				BuildNamePrefix = DOWNSTREAM_LabelPrefix;
			}
			if (String.IsNullOrEmpty(BuildNamePrefix))
			{
				throw new P4Exception("Need a downstream label");
			}

			{
				CommandUtils.LogLog("Making downstream label");
                var Label = FullLabelName(Env, BuildNamePrefix);

				CommandUtils.LogLog("Deleting old label {0} (if any)...", Label);
                DeleteLabel(Label, false);

				CommandUtils.LogLog("Creating new label...");
				CreateLabel(
					Name: Label,
					Description: "BVT Time " + CommandUtils.CmdEnv.TimestampAsString + "  CL " + Env.ChangelistString,
					Options: "unlocked noautoreload",
					View: CommandUtils.CombinePaths(PathSeparator.Depot, Env.BuildRootP4, "...")
					);
				if (Files == null)
				{
					CommandUtils.LogLog("Adding all files to new label {0}...", Label);
					LabelSync(Label, false);
				}
				else
				{
					CommandUtils.LogLog("Adding build products to new label {0}...", Label);
					foreach (string LabelFile in Files)
					{
						LabelSync(Label, false, LabelFile);
					}
				}
			}
		}

        /// <summary>
        /// Creates a downstream label.
        /// </summary>
        /// <param name="BuildNamePrefix">Label Prefix</param>
		public void MakeDownstreamLabelFromLabel(P4Environment Env, string BuildNamePrefix, string CopyFromBuildNamePrefix)
        {
            CheckP4Enabled();
			string DOWNSTREAM_LabelPrefix = CommandUtils.GetEnvVar("DOWNSTREAM_LabelPrefix");
            if (!String.IsNullOrEmpty(DOWNSTREAM_LabelPrefix))
            {
                BuildNamePrefix = DOWNSTREAM_LabelPrefix;
            }
            if (String.IsNullOrEmpty(BuildNamePrefix) || String.IsNullOrEmpty(CopyFromBuildNamePrefix))
            {
                throw new P4Exception("Need a downstream label");
            }

            {
				CommandUtils.LogLog("Making downstream label");
                var Label = FullLabelName(Env, BuildNamePrefix);

				CommandUtils.LogLog("Deleting old label {0} (if any)...", Label);
                DeleteLabel(Label, false);

				CommandUtils.LogLog("Creating new label...");
                CreateLabel(
                    Name: Label,
					Description: "BVT Time " + CommandUtils.CmdEnv.TimestampAsString + "  CL " + Env.ChangelistString,
                    Options: "unlocked noautoreload",
					View: CommandUtils.CombinePaths(PathSeparator.Depot, Env.BuildRootP4, "...")
                    );
                LabelToLabelSync(FullLabelName(Env, CopyFromBuildNamePrefix), Label, false);
            }
        }

        /// <summary>
        /// Given a file path in the depot, returns the local disk mapping for the current view
        /// </summary>
		/// <param name="DepotFile">The full file path in depot naming form</param>
        /// <returns>The file's first reported path on disk or null if no mapping was found</returns>
        public string DepotToLocalPath(string DepotFile, bool AllowSpew = true)
        {
			P4WhereRecord[] Records = Where(DepotFile, AllowSpew);
			if (Records != null)
			{
				foreach (P4WhereRecord Record in Records)
				{
					if (!Record.bUnmap)
					{
						return Record.Path;
					}
				}
			}
			return null;
        }

		/// <summary>
		/// Determines the mappings for a depot file in the workspace, without that file having to exist. 
		/// NOTE: This function originally allowed multiple depot paths at once. The "file(s) not in client view" messages are written to stderr 
		/// rather than stdout, and buffering them separately garbles the output when they're merged together.
		/// </summary>
		/// <param name="DepotFile">Depot path</param>
		/// <param name="AllowSpew">Allows logging</param>
		/// <returns>List of records describing the file's mapping. Usually just one, but may be more.</returns>
		public P4WhereRecord[] Where(string DepotFile, bool AllowSpew = true)
		{
			CheckP4Enabled();

			//  P4 where outputs missing entries 
			string Command = String.Format("-z tag where \"{0}\"", DepotFile);

			// Run the command.
			string Output;
			if (!LogP4Output(out Output, Command, AllowSpew: AllowSpew))
			{
				throw new P4Exception("p4.exe {0} failed.", Command);
			}

			// Copy the results into the local paths lookup. Entries may occur more than once, and entries may be missing from the client view, or deleted in the client view.
			string[] Lines = Output.Split(new char[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);

			// Check for the file not existing
			if(Lines.Length == 1 && Lines[0].EndsWith(" - file(s) not in client view."))
			{
				return null;
			}

			// Parse it into records
			List<P4WhereRecord> Records = new List<P4WhereRecord>();
			for (int LineIdx = 0; LineIdx < Lines.Length; )
			{
				P4WhereRecord Record = new P4WhereRecord();

				// Parse an optional "... unmap"
				if (Lines[LineIdx].Trim() == "... unmap")
				{
					Record.bUnmap = true;
					LineIdx++;
				}

				// Parse "... depotFile <depot path>"
				const string DepotFilePrefix = "... depotFile ";
				if (LineIdx >= Lines.Length || !Lines[LineIdx].StartsWith(DepotFilePrefix))
				{
					throw new AutomationException("Unexpected output from p4 where: {0}", String.Join("\n", Lines.Skip(LineIdx)));
				}
				Record.DepotFile = Lines[LineIdx++].Substring(DepotFilePrefix.Length).Trim();

				// Parse "... clientFile <client path>"
				const string ClientFilePrefix = "... clientFile ";
				if (LineIdx >= Lines.Length || !Lines[LineIdx].StartsWith(ClientFilePrefix))
				{
					throw new AutomationException("Unexpected output from p4 where: {0}", String.Join("\n", Lines.Skip(LineIdx)));
				}
				Record.ClientFile = Lines[LineIdx++].Substring(ClientFilePrefix.Length).Trim();

				// Parse "... path <path to file>"
				const string PathPrefix = "... path ";
				if (LineIdx >= Lines.Length || !Lines[LineIdx].StartsWith(PathPrefix))
				{
					throw new AutomationException("Unexpected output from p4 where: {0}", String.Join("\n", Lines.Skip(LineIdx)));
				}
				Record.Path = Lines[LineIdx++].Substring(PathPrefix.Length).Trim();

				// Add it to the output list
				Records.Add(Record);
			}
			return Records.ToArray();
		}

		/// <summary>
		/// Determines whether a file exists in the depot.
		/// </summary>
		/// <param name="DepotFile">Depot path</param>
		/// <returns>List of records describing the file's mapping. Usually just one, but may be more.</returns>
		public bool FileExistsInDepot(string DepotFile, bool AllowSpew = true)
		{
			string CommandLine = String.Format("-z tag fstat {0}", CommandUtils.MakePathSafeToUseWithCommandLine(DepotFile));

			string Output;
			if(!LogP4Output(out Output, CommandLine, AllowSpew: false) || Output.Contains("no such file(s)"))
			{
				return false;
			}

			return true;
		}

		/// <summary>
		/// Gets file stats.
		/// </summary>
		/// <param name="Filename">Filenam</param>
		/// <returns>File stats (invalid if the file does not exist in P4)</returns>
		public P4FileStat FStat(string Filename)
		{
			CheckP4Enabled();
			string Output;
			string Command = "fstat " + CommandUtils.MakePathSafeToUseWithCommandLine(Filename);
			if (!LogP4Output(out Output, Command))
			{
				throw new P4Exception("p4.exe {0} failed.", Command);
			}

			P4FileStat Stat = P4FileStat.Invalid;

			if (Output.Contains("no such file(s)") == false)
			{
				Output = Output.Replace("\r", "");
				var FormLines = Output.Split('\n');
				foreach (var Line in FormLines)
				{
					var StatAttribute = Line.StartsWith("... ") ? Line.Substring(4) : Line;
					var StatPair = StatAttribute.Split(' ');
					if (StatPair.Length == 2 && !String.IsNullOrEmpty(StatPair[1]))
					{
						switch (StatPair[0])
						{
							case "type":
								// Use type (current CL if open) if possible
								ParseFileType(StatPair[1], ref Stat);
								break;
							case "headType":
								if (Stat.Type == P4FileType.Unknown)
								{
									ParseFileType(StatPair[1], ref Stat);
								}
								break;
							case "action":
								Stat.Action = ParseAction(StatPair[1]);
								break;
							case "change":
								Stat.Change = StatPair[1];
								break;
						}
					}
				}
				if (Stat.IsValid == false)
				{
					throw new AutomationException("Unable to parse fstat result for {0} (unknown file type).", Filename);
				}
			}
			return Stat;
		}

		/// <summary>
		/// Set file attributes (additively)
		/// </summary>
		/// <param name="Filename">File to change the attributes of.</param>
		/// <param name="Attributes">Attributes to set.</param>
		public void ChangeFileType(string Filename, P4FileAttributes Attributes, string Changelist = null)
		{
			CommandUtils.LogLog("ChangeFileType({0}, {1}, {2})", Filename, Attributes, String.IsNullOrEmpty(Changelist) ? "null" : Changelist);

			var Stat = FStat(Filename);
			if (String.IsNullOrEmpty(Changelist))
			{
				Changelist = (Stat.Action != P4Action.None) ? Stat.Change : "default";
			}
			// Only update attributes if necessary
			if ((Stat.Attributes & Attributes) != Attributes)
			{
				var CmdLine = String.Format("{0} -c {1} -t {2} {3}",
					(Stat.Action != P4Action.None) ? "reopen" : "open",
					Changelist, FileAttributesToString(Attributes | Stat.Attributes), CommandUtils.MakePathSafeToUseWithCommandLine(Filename));
				LogP4(CmdLine);
			}
		}

		/// <summary>
		/// Parses P4 forms and stores them as a key/value pairs.
		/// </summary>
		/// <param name="Output">P4 command output (must be a form).</param>
		/// <returns>Parsed output.</returns>
		public CaselessDictionary<string> ParseTaggedP4Output(string Output)
		{
			var Tags = new CaselessDictionary<string>();
			var Lines = Output.Split(new string[] { Environment.NewLine }, StringSplitOptions.RemoveEmptyEntries);
            string DelayKey = "";
            int DelayIndex = 0;
			foreach (var Line in Lines)
			{
				var TrimmedLine = Line.Trim();
                if (TrimmedLine.StartsWith("#") == false)
				{
                    if (DelayKey != "")
                    {
                        if (Line.StartsWith("\t"))
                        {
                            if (DelayIndex > 0)
                            {
                                Tags.Add(String.Format("{0}{1}", DelayKey, DelayIndex), TrimmedLine);
                            }
                            else
                            {
                                Tags.Add(DelayKey, TrimmedLine);
                            }
                            DelayIndex++;
                            continue;
                        }
                        DelayKey = "";
                        DelayIndex = 0;
                    }
					var KeyEndIndex = TrimmedLine.IndexOf(':');
					if (KeyEndIndex >= 0)
					{
						var Key = TrimmedLine.Substring(0, KeyEndIndex);
						var Value = TrimmedLine.Substring(KeyEndIndex + 1).Trim();
                        if (Value == "")
                        {
                            DelayKey = Key;
                        }
                        else
                        {
                            Tags.Add(Key, Value);
                        }
					}
				}
			}
			return Tags;
		}

		/// <summary>
		/// Checks if the client exists in P4.
		/// </summary>
		/// <param name="ClientName">Client name</param>
		/// <returns>True if the client exists.</returns>
		public bool DoesClientExist(string ClientName, bool Quiet = false)
		{
			CheckP4Enabled();
			if(!Quiet)
			{
				CommandUtils.LogLog("Checking if client {0} exists", ClientName);
			}

            var P4Result = P4(String.Format("-c {0} where //...", ClientName), AllowSpew: false, WithClient: false);
            return P4Result.Output.IndexOf("unknown - use 'client' command", StringComparison.InvariantCultureIgnoreCase) < 0 && P4Result.Output.IndexOf("doesn't exist", StringComparison.InvariantCultureIgnoreCase) < 0;
		}

		/// <summary>
		/// Gets client info.
		/// </summary>
		/// <param name="ClientName">Name of the client.</param>
		/// <returns></returns>
		public P4ClientInfo GetClientInfo(string ClientName, bool Quiet = false)
		{
			CheckP4Enabled();

			if(!Quiet)
			{
				CommandUtils.LogLog("Getting info for client {0}", ClientName);
			}
			if (!DoesClientExist(ClientName, Quiet))
			{
				return null;
			}

			return GetClientInfoInternal(ClientName);
		}
        /// <summary>
        /// Parses a string with enum values separated with spaces.
        /// </summary>
        /// <param name="ValueText"></param>
        /// <param name="EnumType"></param>
        /// <returns></returns>
        private static object ParseEnumValues(string ValueText, Type EnumType)
        {
            ValueText = ValueText.Replace(' ', ',');
            return Enum.Parse(EnumType, ValueText, true);
        }

		/// <summary>
		/// Gets client info (does not check if the client exists)
		/// </summary>
		/// <param name="ClientName">Name of the client.</param>
		/// <returns></returns>
		public P4ClientInfo GetClientInfoInternal(string ClientName)
		{
			P4ClientInfo Info = new P4ClientInfo();
            var P4Result = P4(String.Format("client -o {0}", ClientName), AllowSpew: false, WithClient: false);
			if (P4Result.ExitCode == 0)
			{
				var Tags = ParseTaggedP4Output(P4Result.Output);
                Info.Name = ClientName;
				Tags.TryGetValue("Host", out Info.Host);
				Tags.TryGetValue("Root", out Info.RootPath);
				if (!String.IsNullOrEmpty(Info.RootPath))
				{
					Info.RootPath = CommandUtils.ConvertSeparators(PathSeparator.Default, Info.RootPath);
				}
				Tags.TryGetValue("Owner", out Info.Owner);
				Tags.TryGetValue("Stream", out Info.Stream);
				string AccessTime;
				Tags.TryGetValue("Access", out AccessTime);
				if (!String.IsNullOrEmpty(AccessTime))
				{
					DateTime.TryParse(AccessTime, out Info.Access);
				}
				else
				{
					Info.Access = DateTime.MinValue;
				}
                string LineEnd;
                Tags.TryGetValue("LineEnd", out LineEnd);
                if (!String.IsNullOrEmpty(LineEnd))
                {
                    Info.LineEnd = (P4LineEnd)ParseEnumValues(LineEnd, typeof(P4LineEnd));
                }
                string ClientOptions;
                Tags.TryGetValue("Options", out ClientOptions);
                if (!String.IsNullOrEmpty(ClientOptions))
                {
                    Info.Options = (P4ClientOption)ParseEnumValues(ClientOptions, typeof(P4ClientOption));
                }
                string SubmitOptions;
                Tags.TryGetValue("SubmitOptions", out SubmitOptions);
                if (!String.IsNullOrEmpty(SubmitOptions))
                {
                    Info.SubmitOptions = (P4SubmitOption)ParseEnumValues(SubmitOptions, typeof(P4SubmitOption));
                }
                string ClientMappingRoot = "//" + ClientName;
                foreach (var Pair in Tags)
                {
                    if (Pair.Key.StartsWith("View", StringComparison.InvariantCultureIgnoreCase))
                    {
                        string Mapping = Pair.Value;
                        int ClientStartIndex = Mapping.IndexOf(ClientMappingRoot, StringComparison.InvariantCultureIgnoreCase);
                        if (ClientStartIndex > 0)
                        {
                            var ViewPair = new KeyValuePair<string, string>(
                                Mapping.Substring(0, ClientStartIndex - 1),
                                Mapping.Substring(ClientStartIndex + ClientMappingRoot.Length));
                            Info.View.Add(ViewPair);
                        }
                    }
                }
			}
			else
			{
				throw new AutomationException("p4 client -o {0} failed!", ClientName);
			}
			return Info;
		}

		/// <summary>
		/// Gets all clients owned by the user.
		/// </summary>
		/// <param name="UserName"></param>
		/// <returns>List of clients owned by the user.</returns>
		public P4ClientInfo[] GetClientsForUser(string UserName, string PathUnderClientRoot = null)
		{
			CheckP4Enabled();

			var ClientList = new List<P4ClientInfo>();

			// Get all clients for this user
            var P4Result = P4(String.Format("clients -u {0}", UserName), AllowSpew: false, WithClient: false);
			if (P4Result.ExitCode != 0)
			{
				throw new AutomationException("p4 clients -u {0} failed.", UserName);
			}

			// Parse output.
			var Lines = P4Result.Output.Split(new string[] { Environment.NewLine }, StringSplitOptions.RemoveEmptyEntries);
			foreach (string Line in Lines)
			{
				var Tokens = Line.Split(new char[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
				P4ClientInfo Info = null;

				// Retrieve the client name and info.
				for (int TokenIndex = 0; TokenIndex < Tokens.Length; ++TokenIndex)
				{
					if (Tokens[TokenIndex] == "Client")
					{
						var ClientName = Tokens[++TokenIndex];
						Info = GetClientInfoInternal(ClientName);
						break;
					}
				}

				if (Info == null || String.IsNullOrEmpty(Info.Name) || String.IsNullOrEmpty(Info.RootPath))
				{
					throw new AutomationException("Failed to retrieve p4 client info for user {0}. Unable to set up local environment", UserName);
				}
				
				bool bAddClient = true;
				// Filter the client out if the specified path is not under the client root
				if (!String.IsNullOrEmpty(PathUnderClientRoot) && !String.IsNullOrEmpty(Info.RootPath))
				{
					var ClientRootPathWithSlash = Info.RootPath;
					if (!ClientRootPathWithSlash.EndsWith("\\") && !ClientRootPathWithSlash.EndsWith("/"))
					{
						ClientRootPathWithSlash = CommandUtils.ConvertSeparators(PathSeparator.Default, ClientRootPathWithSlash + "/");
					}
					bAddClient = PathUnderClientRoot.StartsWith(ClientRootPathWithSlash, StringComparison.CurrentCultureIgnoreCase);
				}

				if (bAddClient)
				{
					ClientList.Add(Info);
				}
			}
			return ClientList.ToArray();
		}
        /// <summary>
        /// Deletes a client.
        /// </summary>
        /// <param name="Name">Client name.</param>
        /// <param name="Force">Forces the operation (-f)</param>
        public void DeleteClient(string Name, bool Force = false)
        {
            CheckP4Enabled();
            LogP4(String.Format("client -d {0} {1}", (Force ? "-f" : ""), Name), WithClient: false);
        }

        /// <summary>
        /// Creates a new client.
        /// </summary>
        /// <param name="ClientSpec">Client specification.</param>
        /// <returns></returns>
        public P4ClientInfo CreateClient(P4ClientInfo ClientSpec, bool AllowSpew = true)
        {
            string SpecInput = "Client: " + ClientSpec.Name + Environment.NewLine;
            SpecInput += "Owner: " + ClientSpec.Owner + Environment.NewLine;
            SpecInput += "Host: " + ClientSpec.Host + Environment.NewLine;
            SpecInput += "Root: " + ClientSpec.RootPath + Environment.NewLine;
            SpecInput += "Options: " + ClientSpec.Options.ToString().ToLowerInvariant().Replace(",", "") + Environment.NewLine;
            SpecInput += "SubmitOptions: " + ClientSpec.SubmitOptions.ToString().ToLowerInvariant().Replace(",", "") + Environment.NewLine;
            SpecInput += "LineEnd: " + ClientSpec.LineEnd.ToString().ToLowerInvariant() + Environment.NewLine;
			if(ClientSpec.Stream != null)
			{
				SpecInput += "Stream: " + ClientSpec.Stream + Environment.NewLine;
			}
			else
			{
				SpecInput += "View:" + Environment.NewLine;
				foreach (var Mapping in ClientSpec.View)
				{
					SpecInput += "\t" + Mapping.Key + " //" + ClientSpec.Name + Mapping.Value + Environment.NewLine;
				}
			}
			if (AllowSpew) CommandUtils.LogLog(SpecInput);
            LogP4("client -i", SpecInput, AllowSpew: AllowSpew, WithClient: false);
            return ClientSpec;
        }


		/// <summary>
		/// Lists immediate sub-directories of the specified directory.
		/// </summary>
		/// <param name="CommandLine"></param>
		/// <returns>List of sub-directories of the specified directories.</returns>
		public List<string> Dirs(string CommandLine)
		{
			CheckP4Enabled();
			var DirsCmdLine = String.Format("dirs {0}", CommandLine);
			var P4Result = P4(DirsCmdLine, AllowSpew: false);
			if (P4Result.ExitCode != 0)
			{
				throw new AutomationException("{0} failed.", DirsCmdLine);
			}
			var Result = new List<string>();
			var Lines = P4Result.Output.Split(new string[] { Environment.NewLine }, StringSplitOptions.RemoveEmptyEntries);
			foreach (string Line in Lines)
			{
				if (!Line.Contains("no such file"))
				{
					Result.Add(Line);
				}
			}
			return Result;
		}

		/// <summary>
		/// Lists files of the specified directory non-recursively.
		/// </summary>
		/// <param name="CommandLine"></param>
		/// <returns>List of files in the specified directory.</returns>
		public List<string> Files(string CommandLine)
		{
			CheckP4Enabled();
			string FilesCmdLine = String.Format("files {0}", CommandLine);
			IProcessResult P4Result = P4(FilesCmdLine, AllowSpew: false);
			if (P4Result.ExitCode != 0)
			{
				throw new AutomationException("{0} failed.", FilesCmdLine);
			}
			List<string> Result = new List<string>();
			string[] Lines = P4Result.Output.Split(new string[] { Environment.NewLine }, StringSplitOptions.RemoveEmptyEntries);
			Regex OutputSplitter = new Regex(@"(?<filename>.+)#\d+ \- (?<action>[a-zA-Z]+) .+");
			foreach (string Line in Lines)
			{
				if (!Line.Contains("no such file") && OutputSplitter.IsMatch(Line))
				{
					Match RegexMatch = OutputSplitter.Match(Line);
					string Filename = RegexMatch.Groups["filename"].Value;
					string Action = RegexMatch.Groups["action"].Value;
					if (Action != "delete")
					{
						Result.Add(Filename);
					}
				}
			}
			return Result;
		}

		/// <summary>
		/// Gets the contents of a particular file in the depot without syncing it
		/// </summary>
		/// <param name="DepotPath">Depot path to the file (with revision/range if necessary)</param>
		/// <returns>Contents of the file</returns>
		public string Print(string DepotPath, bool AllowSpew = true)
		{
			string Output;
			if(!P4Output(out Output, "print -q " + DepotPath, AllowSpew: AllowSpew, WithClient: false))
			{
				throw new AutomationException("p4 print {0} failed", DepotPath);
			}
			if(!Output.Trim().Contains("\n") && Output.Contains("no such file(s)"))
			{
				throw new AutomationException("p4 print {0} failed", DepotPath);
			}
			return Output;
		}

		/// <summary>
		/// Gets the contents of a particular file in the depot and writes it to a local file without syncing it
		/// </summary>
		/// <param name="DepotPath">Depot path to the file (with revision/range if necessary)</param>
		/// <param name="OutputFileName">Output file to write to</param>
		public void PrintToFile(string DepotPath, string FileName, bool AllowSpew = true)
		{
			string Output;
			if(!P4Output(out Output, "print -q -o \"" + FileName + "\" " + DepotPath, AllowSpew: AllowSpew, WithClient: false))
			{
				throw new AutomationException("p4 print {0} failed", DepotPath);
			}
			if(!Output.Trim().Contains("\n") && Output.Contains("no such file(s)"))
			{
				throw new AutomationException("p4 print {0} failed", DepotPath);
			}
		}

		/// <summary>
		/// Runs the 'interchanges' command on a stream, to determine a list of changelists that have not been integrated to its parent (or vice-versa, if bReverse is set).
		/// </summary>
		/// <param name="StreamName">The name of the stream, eg. //UE4/Dev-Animation</param>
		/// <param name="bReverse">If true, returns changes that have not been merged from the parent stream into this one.</param>
		/// <returns>List of changelist numbers that are pending integration</returns>
		public List<int> StreamInterchanges(string StreamName, bool bReverse)
		{
			string Output;
			if(!P4Output(out Output, String.Format("interchanges {0}-S {1}", bReverse? "-r " : "", StreamName), Input:null, AllowSpew:false))
			{
				throw new AutomationException("Couldn't get unintegrated stream changes from {0}", StreamName);
			}

			List<int> Changelists = new List<int>();
			if(!Output.StartsWith("All revision(s) already integrated"))
			{
				foreach(string Line in Output.Split('\n'))
				{
					string[] Tokens = Line.Split(new char[]{ ' ' }, StringSplitOptions.RemoveEmptyEntries);
					if(Tokens.Length > 0)
					{
						int Changelist;
						if(Tokens[0] != "Change" || !int.TryParse(Tokens[1], out Changelist))
						{
							throw new AutomationException("Unexpected output from p4 interchanges: {0}", Line);
						}
						Changelists.Add(Changelist);
					}
				}
			}
			return Changelists;
		}

		/// <summary>
		/// For a given file (and revision, potentially), returns where it was integrated from. Useful in conjunction with files in a P4DescribeRecord, with action = "integrate".
		/// </summary>
		/// <param name="DepotPath">The file to check. May have a revision specifier at the end (eg. //depot/UE4/foo.cpp#2) </param>
		/// <returns>The file that it was integrated from, without a revision specifier</returns>
		public string GetIntegrationSource(string DepotPath)
		{
			string Output;
			if(P4Output(out Output, "filelog -m 1 \"" + DepotPath + "\"", Input:null, AllowSpew:false))
			{
				foreach(string Line in Output.Split('\n').Select(x => x.Trim()))
				{
					const string MergePrefix = "... ... merge from ";
					if(Line.StartsWith(MergePrefix))
					{
						return Line.Substring(MergePrefix.Length, Line.LastIndexOf('#') - MergePrefix.Length);
					}

					const string CopyPrefix = "... ... copy from ";
					if(Line.StartsWith(CopyPrefix))
					{
						return Line.Substring(CopyPrefix.Length, Line.LastIndexOf('#') - CopyPrefix.Length);
					}

					const string EditPrefix = "... ... edit from ";
					if (Line.StartsWith(EditPrefix))
					{
						return Line.Substring(EditPrefix.Length, Line.LastIndexOf('#') - EditPrefix.Length);
					}
				}
			}
			return null;
		}

		#region Utilities

		private static object[] OldStyleBinaryFlags = new object[]
		{
			P4FileAttributes.Uncompressed,
			P4FileAttributes.Executable,
			P4FileAttributes.Compressed,
			P4FileAttributes.RCS
		};

		private static void ParseFileType(string Filetype, ref P4FileStat Stat)
		{
			var AllFileTypes = GetEnumValuesAndKeywords(typeof(P4FileType));
			var AllAttributes = GetEnumValuesAndKeywords(typeof(P4FileAttributes));

			Stat.Type = P4FileType.Unknown;
			Stat.Attributes = P4FileAttributes.None;

			// Parse file flags
			var OldFileFlags = GetEnumValuesAndKeywords(typeof(P4FileAttributes), OldStyleBinaryFlags);
			foreach (var FileTypeFlag in OldFileFlags)
			{
				if ((!String.IsNullOrEmpty(FileTypeFlag.Value) && Char.ToLowerInvariant(FileTypeFlag.Value[0]) == Char.ToLowerInvariant(Filetype[0]))
					// @todo: This is a nasty hack to get .ipa files to work - RobM plz fix?
					|| (FileTypeFlag.Value == "F" && Filetype == "ubinary"))
				{
					Stat.IsOldType = true;
					Stat.Attributes |= (P4FileAttributes)FileTypeFlag.Key;
					break;
				}
			}
			if (Stat.IsOldType)
			{
				Filetype = Filetype.Substring(1);
			}
			// Parse file type
			var TypeAndAttributes = Filetype.Split('+');
			foreach (var FileType in AllFileTypes)
			{
				if (FileType.Value == TypeAndAttributes[0])
				{
					Stat.Type = (P4FileType)FileType.Key;
					break;
				}
			}
			// Parse attributes
			if (TypeAndAttributes.Length > 1 && !String.IsNullOrEmpty(TypeAndAttributes[1]))
			{
				var FileAttributes = TypeAndAttributes[1];
				for (int AttributeIndex = 0; AttributeIndex < FileAttributes.Length; ++AttributeIndex)
				{
					char Attr = FileAttributes[AttributeIndex];
					foreach (var FileAttribute in AllAttributes)
					{
						if (!String.IsNullOrEmpty(FileAttribute.Value) && FileAttribute.Value[0] == Attr)
						{
							Stat.Attributes |= (P4FileAttributes)FileAttribute.Key;
							break;
						}
					}
				}
			}
		}

		private static P4Action ParseAction(string Action)
		{
			P4Action Result = P4Action.Unknown;
			var AllActions = GetEnumValuesAndKeywords(typeof(P4Action));
			foreach (var ActionKeyword in AllActions)
			{
				if (ActionKeyword.Value == Action)
				{
					Result = (P4Action)ActionKeyword.Key;
					break;
				}
			}
			return Result;
		}

		private static KeyValuePair<object, string>[] GetEnumValuesAndKeywords(Type EnumType)
		{
			var Values = Enum.GetValues(EnumType);
			KeyValuePair<object, string>[] ValuesAndKeywords = new KeyValuePair<object, string>[Values.Length];
			int ValueIndex = 0;
			foreach (var Value in Values)
			{
				ValuesAndKeywords[ValueIndex++] = new KeyValuePair<object, string>(Value, GetEnumDescription(EnumType, Value));
			}
			return ValuesAndKeywords;
		}

		private static KeyValuePair<object, string>[] GetEnumValuesAndKeywords(Type EnumType, object[] Values)
		{
			KeyValuePair<object, string>[] ValuesAndKeywords = new KeyValuePair<object, string>[Values.Length];
			int ValueIndex = 0;
			foreach (var Value in Values)
			{
				ValuesAndKeywords[ValueIndex++] = new KeyValuePair<object, string>(Value, GetEnumDescription(EnumType, Value));
			}
			return ValuesAndKeywords;
		}

		private static string GetEnumDescription(Type EnumType, object Value)
		{
			var MemberInfo = EnumType.GetMember(Value.ToString());
			var Atributes = MemberInfo[0].GetCustomAttributes(typeof(DescriptionAttribute), false);
			return ((DescriptionAttribute)Atributes[0]).Description;
		}

		private static string FileAttributesToString(P4FileAttributes Attributes)
		{
			var AllAttributes = GetEnumValuesAndKeywords(typeof(P4FileAttributes));
			string Text = "";
			foreach (var Attr in AllAttributes)
			{
				var AttrValue = (P4FileAttributes)Attr.Key;
				if ((Attributes & AttrValue) == AttrValue)
				{
					Text += Attr.Value;
				}
			}
			if (String.IsNullOrEmpty(Text) == false)
			{
				Text = "+" + Text;
			}
			return Text;
		}

		#endregion
	}
}
