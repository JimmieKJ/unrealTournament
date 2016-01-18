// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.ComponentModel.Design;
using System.IO;
using System.Linq;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using Microsoft.VisualStudio.OLE.Interop;
using EnvDTE;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using EnvDTE80;


namespace UnrealVS
{
	internal class CommandLineEditor
	{
		/** constants */

		private const int ComboID = 0x1030;
		private const int ComboListID = 0x1040;
		private const string InvalidProjectString = "<No Command-line>";
		private const int ComboListCountMax = 32;

		/** methods */

		public CommandLineEditor()
		{
			// Create the handlers for our commands
			{
				// CommandLineCombo
				var ComboCommandID = new CommandID(GuidList.UnrealVSCmdSet, ComboID);
				ComboCommand = new OleMenuCommand(new EventHandler(ComboHandler), ComboCommandID);
				UnrealVSPackage.Instance.MenuCommandService.AddCommand(ComboCommand);

				// CommandLineComboList
				var ComboListCommandID = new CommandID(GuidList.UnrealVSCmdSet, ComboListID);
				ComboCommandList = new OleMenuCommand(new EventHandler(ComboListHandler), ComboListCommandID);
				UnrealVSPackage.Instance.MenuCommandService.AddCommand(ComboCommandList);
			}

			// Register for events that we care about
			UnrealVSPackage.Instance.OnSolutionOpened += UpdateCommandLineCombo;
			UnrealVSPackage.Instance.OnSolutionClosed += UpdateCommandLineCombo;
			UnrealVSPackage.Instance.OnStartupProjectChanged += (p) => UpdateCommandLineCombo();
			UnrealVSPackage.Instance.OnStartupProjectPropertyChanged += OnStartupProjectPropertyChanged;
			UnrealVSPackage.Instance.OnStartupProjectConfigChanged += OnStartupProjectConfigChanged;

			UpdateCommandLineCombo();
		}

		/// <summary>
		/// Called when a startup project property has changed.  We'll refresh our interface.
		/// </summary>
		public void OnStartupProjectPropertyChanged(UInt32 itemid, Int32 propid, UInt32 flags)
		{
			IVsHierarchy ProjectHierarchy;
			UnrealVSPackage.Instance.SolutionBuildManager.get_StartupProject(out ProjectHierarchy);
			if (ProjectHierarchy != null)
			{
				
				// @todo: filter this so that we only respond to changes in the command line property
				UpdateCommandLineCombo();
			}
		}

		/// <summary>
		/// Reads options out of the solution file.
		/// </summary>
		public void LoadOptions(Stream Stream)
		{
			using (BinaryReader Reader = new BinaryReader(Stream))
			{
				List<string> CommandLines = new List<string>();
				int Count = Reader.ReadInt32();
				for (int CommandLineIdx = 0; CommandLineIdx < Count; CommandLineIdx++)
				{
					CommandLines.Add(Reader.ReadString());
				}
				ComboList.Clear();
				ComboList.AddRange(CommandLines.ToArray());
			}
		}

		/// <summary>
		/// Writes options to the solution file.
		/// </summary>
		public void SaveOptions(Stream Stream)
		{
			using (BinaryWriter Writer = new BinaryWriter(Stream))
			{
				string[] CommandLines = ComboList.ToArray();
				Writer.Write(CommandLines.Length);
				for (int CommandLineIdx = 0; CommandLineIdx < CommandLines.Length; CommandLineIdx++)
				{
					Writer.Write(CommandLines[CommandLineIdx]);
				}
			}
		}

		/// <summary>
		/// Called when the startup project active config has changed.
		/// </summary>
		private void OnStartupProjectConfigChanged(Project Project)
		{
			UpdateCommandLineCombo();
		}

		/// <summary>
		/// Updates the command-line combo box after the project loaded state has changed
		/// </summary>
		private void UpdateCommandLineCombo()
		{
			// Enable or disable our command-line selector
			DesiredCommandLine = null;	// clear this state var used by the combo box handler
			IVsHierarchy ProjectHierarchy;
			UnrealVSPackage.Instance.SolutionBuildManager.get_StartupProject( out ProjectHierarchy );
			if( ProjectHierarchy != null )
			{
				ComboCommand.Enabled = true;
				ComboCommandList.Enabled = true;
			}
			else
			{
				ComboCommand.Enabled = false;
				ComboCommandList.Enabled = false;
			}

			string CommandLine = MakeCommandLineComboText();
			CommitCommandLineToMRU(CommandLine);
		}


		/// <summary>
		/// Returns a string to display in the command-line combo box, based on project state
		/// </summary>
		/// <returns>String to display</returns>
		private string MakeCommandLineComboText()
		{
			string Text = "";

			IVsHierarchy ProjectHierarchy;
			UnrealVSPackage.Instance.SolutionBuildManager.get_StartupProject( out ProjectHierarchy );
			if( ProjectHierarchy != null )
			{
				var SelectedStartupProject = Utils.HierarchyObjectToProject( ProjectHierarchy );

				if (SelectedStartupProject != null)
				{
					var CommandLineArgumentsProperty = GetProjectCommandLineProperty(SelectedStartupProject);

					if (CommandLineArgumentsProperty != null)
					{
						var CommandLineArguments = (string) CommandLineArgumentsProperty.Value;

						// for "Game" projects automatically remove the game project filename from the start of the command line
						var ActiveConfiguration = (SolutionConfiguration2) UnrealVSPackage.Instance.DTE.Solution.SolutionBuild.ActiveConfiguration;
						if (UnrealVSPackage.Instance.IsUE4Loaded && Utils.IsGameProject(SelectedStartupProject) && Utils.HasUProjectCommandLineArg(ActiveConfiguration.Name))
						{
							var AutoPrefix = Utils.GetAutoUProjectCommandLinePrefix(SelectedStartupProject);

							if (!string.IsNullOrEmpty(AutoPrefix))
							{
								if (CommandLineArguments.Trim().StartsWith(AutoPrefix, StringComparison.OrdinalIgnoreCase))
								{
									CommandLineArguments = CommandLineArguments.Trim().Substring(AutoPrefix.Length).Trim();
								}
								//else if (CommandLineArguments.Trim().StartsWith(SelectedStartupProject.Name + " ", StringComparison.OrdinalIgnoreCase))
								//{
								//	CommandLineArguments = CommandLineArguments.Trim().Substring(SelectedStartupProject.Name.Length + 1).Trim();
								//}
							}
						}

						Text = CommandLineArguments;
					}
					else
					{
						Text = InvalidProjectString;
					}
				}
			}

			return Text;
		}


		/// Called by combo control to query the text to display or to apply newly-entered text
		private void ComboHandler(object Sender, EventArgs Args)
		{
			try
			{
				var OleArgs = (OleMenuCmdEventArgs)Args;

				string InString = OleArgs.InValue as string;
				if (InString != null)
				{
					// New text set on the combo - set the command line property
					DesiredCommandLine = null;
					CommitCommandLineText(InString);
				}
				else if (OleArgs.OutValue != IntPtr.Zero)
				{
					string EditingString = null;
					if (OleArgs.InValue != null)
					{
						object[] InArray = OleArgs.InValue as object[];
						if (InArray != null && 0 < InArray.Length)
						{
							EditingString = InArray.Last() as string;
						}
					}

					string TextToDisplay = string.Empty;
					if (EditingString != null)
					{
						// The control wants to put EditingString in the box
						TextToDisplay = DesiredCommandLine = EditingString;
					}
					else
					{
						// This is always hit at the end of interaction with the combo
						if (DesiredCommandLine != null)
						{
							TextToDisplay = DesiredCommandLine;
							DesiredCommandLine = null;
							CommitCommandLineText(TextToDisplay);
						}
						else
						{
							TextToDisplay = MakeCommandLineComboText();
						}
					}

					Marshal.GetNativeVariantForObject(TextToDisplay, OleArgs.OutValue);
				}
			}
			catch (Exception ex)
			{
				Exception AppEx = new ApplicationException("CommandLineEditor threw an exception in ComboHandler()", ex);
				Logging.WriteLine(AppEx.ToString());
				throw AppEx;
			}
		}

		private void CommitCommandLineText(string CommandLine)
		{
			IVsHierarchy ProjectHierarchy;
			UnrealVSPackage.Instance.SolutionBuildManager.get_StartupProject(out ProjectHierarchy);
			if (ProjectHierarchy != null)
			{
				var SelectedStartupProject = Utils.HierarchyObjectToProject(ProjectHierarchy);

				if (SelectedStartupProject != null)
				{
					var CommandLineArgumentsProperty = GetProjectCommandLineProperty(SelectedStartupProject);

					if (CommandLineArgumentsProperty != null)
					{
						string FullCommandLine = CommandLine;

						// for "Game" projects automatically remove the game project filename from the start of the command line
						var ActiveConfiguration = (SolutionConfiguration2)UnrealVSPackage.Instance.DTE.Solution.SolutionBuild.ActiveConfiguration;
						if (UnrealVSPackage.Instance.IsUE4Loaded && Utils.IsGameProject(SelectedStartupProject) && Utils.HasUProjectCommandLineArg(ActiveConfiguration.Name))
						{
							var AutoPrefix = Utils.GetAutoUProjectCommandLinePrefix(SelectedStartupProject);
							
							if (FullCommandLine.IndexOf(Utils.UProjectExtension, StringComparison.OrdinalIgnoreCase) < 0 &&
								string.Compare(FullCommandLine, SelectedStartupProject.Name, StringComparison.OrdinalIgnoreCase) != 0 &&
								!FullCommandLine.StartsWith(SelectedStartupProject.Name + " ", StringComparison.OrdinalIgnoreCase))
							{
								// Committed command line does not specify a .uproject
								FullCommandLine = AutoPrefix + " " + FullCommandLine;
							}
						}

						Utils.SetPropertyValue(CommandLineArgumentsProperty, FullCommandLine);
					}
				}
			}

			CommitCommandLineToMRU(CommandLine);

		}

		private void CommitCommandLineToMRU(string CommandLine)
		{
			if (0 < CommandLine.Length)
			{
				// Maintain the MRU history
				// Adds the entered command line to the top of the list
				// Moves it to the top if it's already in the list
				// Trims the list to the max length if it's too big
				ComboList.RemoveAll(s => 0 == string.Compare(s, CommandLine));
				ComboList.Insert(0, CommandLine);
				if (ComboList.Count > ComboListCountMax)
				{
					ComboList.RemoveAt(ComboList.Count - 1);
				}
			}
		}

		/// Called by combo control to populate the drop-down list
		private void ComboListHandler(object Sender, EventArgs Args)
		{
			var OleArgs = (OleMenuCmdEventArgs)Args;

			Marshal.GetNativeVariantForObject(ComboList.ToArray(), OleArgs.OutValue);
		}

		/// <summary>
		/// Helper function to get the correct property from a project that represents its command line arguments
		/// </summary>
		private static Property GetProjectCommandLineProperty(Project InProject)
		{
			// C++ projects use "CommandArguments" as the property name
			var CommandLineArgumentsProperty = Utils.GetProjectConfigProperty(InProject, null, "CommandArguments");

			// C# projects use "StartArguments" as the property name
			if (CommandLineArgumentsProperty == null)
			{
				CommandLineArgumentsProperty = Utils.GetProjectConfigProperty(InProject, null, "StartArguments");
			}

			return CommandLineArgumentsProperty;
		}

		/// List of MRU strings to show in the combobox drop-down list
		private readonly List<string> ComboList = new List<string>();

		/// Combo control for command-line editor
		private OleMenuCommand ComboCommand;
		private OleMenuCommand ComboCommandList;

		/// Used to store the user edited commandline mid-edit, in the combo handler
		private string DesiredCommandLine;
	}
}
