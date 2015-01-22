/**
 * Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Globalization;
using P4Inquiry;
using P4ChangeParser;
using P4ChangeParserApp.Properties;
using Perforce.P4;

namespace P4ChangeParserApp
{
	/// <summary>
	/// Simple command-line based application to query and export Perforce changelists using
	/// the new description tagging system
	/// </summary>
	class P4ChangeParserApp
	{
		#region Static Constants
		/// <summary>
		/// Array of the accepted date formats for query parameters
		/// </summary>
		private static readonly String[] AcceptedDateFormats = 
		{
			"yyyy/MM/dd:HH:mm:ss",
			"yyyy/MM/dd:H:m:s",
			"yyyy/MM/dd",
			"yyyy/M/d:HH:mm:ss",
			"yyyy/M/d:H:m:s",
			"yyyy/M/d",
			"MM/dd/yyyy:HH:mm:ss",
			"MM/dd/yyyy:H:m:s",
			"MM/dd/yyyy",
			"M/d/yyyy:HH:mm:ss",
			"M/d/yyyy:H:m:s",
			"M/d/yyyy"
		};

		/// <summary>
		/// Perforce server to connect to; Currently hard-coded to reduce number of command line arguments
		/// </summary>
		private static readonly String P4Server = Settings.Default.P4Server;

		/// <summary>
		/// String displayed when a malformed command is entered
		/// </summary>
		private static readonly String MalformedCommand = "Error: Malformed Command.";
       
        /// <summary>
		/// String demonstrating valid query operands
		/// </summary>
		private static readonly String QueryUsage = "Query Operand: <ChangelistNumber> | <LabelName> | <Date>";
		
		/// <summary>
		/// String demonstrating valid command usage
		/// </summary>
		private static readonly String CommandUsage = "Command Usage: [-p //depotPath/...] <OutputFile.ValidExtension> <QueryOperand> <QueryOperand>\n" + QueryUsage;
		
		/// <summary>
		/// String to prompt for a new command
		/// </summary>
		private static readonly String PromptForCommand = "Enter a new command:\n" + CommandUsage;

#if COMMAND_LINE_ONLY
		/// <summary>
		/// String to show command line usage
		/// </summary>
		private static readonly String CommandlineUsage = "Usage: P4ChangeParserApp <p4user> <p4password> [-p //depotPath/...] <OutputFile.ValidExtension> <QueryOperand> <QueryOperand>\n" + QueryUsage;
#else
		/// <summary>
		/// String to show command line usage
		/// </summary>
		private static readonly String CommandlineUsage = "Usage: P4ChangeParserApp <p4user> <p4password> [command]";
#endif // #if COMMAND_LINE_ONLY

		/// <summary>
		/// String to show for a Perforce connection error
		/// </summary>
		private static readonly String ConnectionGeneralError = "Unable to connect to Perforce. Please check your settings.";

        /// <summary>
        /// String to show for a Perforce connection error
        /// </summary>
        private static readonly String ConnectionUserError = "Unable to connect to Perforce. Please check the user value is correct.  User={0}";

        /// <summary>
        /// String to show for a Perforce connection error
        /// </summary>
        private static readonly String ConnectionPasswordError = "Unable to connect to Perforce. Please check the password value is correct.";

        /// <summary>
        /// String to show for a Perforce connection error
        /// </summary>
        private static readonly String ConnectionServerError = "Unable to connect to Perforce. Please check the server value is correct.  Server={0}";

        /// <summary>
		/// String to show when an invalid query operand is entered
		/// </summary>
		private static readonly String InvalidQueryOperand = "Invalid query operands.";

        /// <summary>
        /// String to show no changelists were found in the depot path specified
        /// </summary>
        private static readonly String ChangelistDepotMismatch = "No changelists found within the depot path (and user if provided) for the data range.";
        
        /// <summary>
		/// String to check for to allow the user to exit
		/// </summary>
		private static readonly String ExitString = "exit";

		/// <summary>
		/// String array representing delimiters between commands
		/// </summary>
		private static readonly String[] CommandDelimiters = { " " };
		#endregion

		#region Static Member Variables
		/// <summary>
		/// Path to filter the depot by
		/// </summary>
		private static String FilteredDepotPath = Settings.Default.DefaultDepotPath;

		/// <summary>
		/// User to filter the depot by
		/// </summary>
		private static String FilteredUser = "";

		/// <summary>
		/// Mapping of file extensions to export formats
		/// </summary>
		private static Dictionary<String, P4ParsedChangelist.EExportFormats> ExtensionToExportFormatMap;

		/// <summary>
		/// File to output results to
		/// </summary>
		private static String OutputFile = String.Empty;

		/// <summary>
		/// Export format to use, based on output file extension
		/// </summary>
		private static P4ParsedChangelist.EExportFormats ExportFormat;

		/// <summary>
		/// String representing a query operand
		/// </summary>
		private static String QueryStartString = String.Empty;

		/// <summary>
		/// String representing a query operand
		/// </summary>
		private static String QueryEndString = String.Empty;

		/// <summary>
		/// Boolean to track if a valid command is pending execution or not
		/// </summary>
		private static bool bValidCommandPending = false;

		/// <summary>
		/// Instance of P4Inquirer to query the server for changelists, labels, etc.
		/// </summary>
		private static P4Inquirer PerforceInquirer = new P4Inquirer();

		/// <summary>
		/// List of the changelists resulting from a P4Inquirer query
		/// </summary>
        private static IList<Changelist> QueriedChangelists;

		/// <summary>
		/// List of parsed changelists resulting from the parsing of a P4Inquirer query
		/// </summary>
		private static List<P4ParsedChangelist> ParsedChangelists = new List<P4ParsedChangelist>();
		#endregion

		#region Main
		static void Main(String[] Args)
		{
			// Set up the extension to export type map
			ExtensionToExportFormatMap = new Dictionary<String, P4ParsedChangelist.EExportFormats>();
			ExtensionToExportFormatMap.Add("htm", P4ParsedChangelist.EExportFormats.EF_Html);
			ExtensionToExportFormatMap.Add("html", P4ParsedChangelist.EExportFormats.EF_Html);
			ExtensionToExportFormatMap.Add("xml", P4ParsedChangelist.EExportFormats.EF_Xml);
			ExtensionToExportFormatMap.Add("txt", P4ParsedChangelist.EExportFormats.EF_PlainText );

			// Parse the command line arguments for relevant data
			ParseCommandlineArgs(Args);

			// Set the perforce server and attempt to connect to it
			PerforceInquirer.ServerName = P4Server;
			P4Inquirer.EP4ConnectionStatus ConnStatus = PerforceInquirer.Connect();

			// Ensure the connection occurred without a hitch
			if (ConnStatus != P4Inquirer.EP4ConnectionStatus.P4CS_Connected)
			{
                switch (ConnStatus)
                {
                    case P4Inquirer.EP4ConnectionStatus.P4CS_ServerLoginError:
                        DisplayError(ConnectionServerError, PerforceInquirer.ServerName, true);
                        break;
                    case P4Inquirer.EP4ConnectionStatus.P4CS_UserLoginError:
                        DisplayError(ConnectionUserError, PerforceInquirer.UserName, true);
                        break;
                    case P4Inquirer.EP4ConnectionStatus.P4CS_UserPasswordLoginError:
                        DisplayError(ConnectionPasswordError, true);
                        break;
                    default:
                        DisplayError(ConnectionGeneralError, true);
                        break;
                }
			}

			// Attempt to execute a command if one came in on the command line
			ExecuteCommand();

#if COMMAND_LINE_ONLY
			// If executing in command line only mode, exit after successfully running the command
			Exit(1);
#else
			// If executing in console mode, poll for commands repeatedly until the user quits
			for (;;)
			{
				Console.WriteLine(PromptForCommand);
				String CurInput = Console.ReadLine();
				Console.WriteLine("\n");
				ParseCommand(CurInput.Split(CommandDelimiters, StringSplitOptions.RemoveEmptyEntries));
				ExecuteCommand();
			}
#endif // #if COMMAND_LINE_ONLY
		}
		#endregion

		#region Helper Methods
		/// <summary>
		/// Exit the application with the specified error code
		/// </summary>
		/// <param name="ErrorCode">Error code to exit with</param>
		private static void Exit(int ErrorCode)
		{
			// Disconnect from the perforce connection when exiting
			PerforceInquirer.Disconnect();
			Environment.Exit(ErrorCode);
		}

		/// <summary>
		/// Displays the provided string to the console as an error
		/// </summary>
		/// <param name="InError">Error message to display</param>
		/// <param name="bForceExit">If TRUE, forcibly exit after writing out the message</param>
		private static void DisplayError(String InError, bool bForceExit)
		{
			Console.WriteLine("{0}\n", InError);
			if (bForceExit)
			{
				Exit(-1);
			}
		}

        /// <summary>
        /// Displays the provided string to the console as an error
        /// </summary>
        /// <param name="InError">Error message to display</param>
        /// <param name="ExtraDetail">Extra detail to replace {0} in InError</param>
        /// <param name="bForceExit">If TRUE, forcibly exit after writing out the message</param>
        private static void DisplayError(String InError, String ExtraDetail, bool bForceExit)
        {
            DisplayError(String.Format(InError, ExtraDetail), bForceExit);
        }


		/// <summary>
		/// Helper function to parse the command line arguments
		/// </summary>
		/// <param name="Args"></param>
		private static void ParseCommandlineArgs(String[] Args)
		{
#if COMMAND_LINE_ONLY
			if (Args.Length < 5)
#else
			if (Args.Length < 2)
#endif // #if COMMAND_LINE_ONLY
			{
				// Display an error to the screen and exit if the required arguments weren't found
				DisplayError(CommandlineUsage, true);
			}

			// Initialize the Perforce user/password based on the command line arguments
			int ArgsIndex = 0;
			PerforceInquirer.UserName = Args[ArgsIndex++];
			PerforceInquirer.UserPassword = Args[ArgsIndex++];

			// If a command was specified on the command line, parse it and prep it for execution
			if (Args.Length > 2)
			{
				String[] CommandStrings = new String[Args.Length-ArgsIndex];
				Array.Copy(Args, ArgsIndex, CommandStrings, 0, Args.Length - ArgsIndex);
				ParseCommand(CommandStrings);
			}

		}

		/// <summary>
		/// Parse an array of strings into a command to query the Perforce server
		/// </summary>
		/// <param name="Commands"></param>
		private static void ParseCommand(String[] Commands)
		{
			// Assume the command will be valid
			bool bValidCommand = true;
			int CommandIndex = 0;

			// If the user explicitly asked to exit, exit the application immediately
			if ( Commands.Length == 1 && String.Compare(Commands[CommandIndex], ExitString) == 0)
			{
				Exit(1);
			}

			// Loop to catch the existence of any optional arguments/switches/flags that start with a hyphen
			for (; CommandIndex < Commands.Length && Commands[CommandIndex].StartsWith("-"); ++CommandIndex)
			{
				// If the "-p" flag is specified, the user is attempting to filter the depot by path
				if (String.Compare(Commands[CommandIndex], "-p") == 0)
				{
					// Ensure there is at least one more argument (the filter), and that it meets the formatting requirements
					// necessary to be used in a Perforce query
					if (CommandIndex + 1 < Commands.Length)
					{
						++CommandIndex;
                        if (!Commands[CommandIndex].StartsWith(Settings.Default.DepotPathMustStartWith) || !Commands[CommandIndex].EndsWith(Settings.Default.DepotPathMustEndWith))
						{
							bValidCommand = false;
						}
						else
						{
							FilteredDepotPath = Commands[CommandIndex];
						}
					}
					else
					{
						bValidCommand = false;
					}
				}

				// If the "-u" flag is specified, the user is attempting to filter the depot by user
				if( String.Compare( Commands[ CommandIndex ], "-u" ) == 0 )
				{
					// Ensure there is at least one more argument (the filter), and that it meets the formatting requirements
					// necessary to be used in a Perforce query
					if( CommandIndex + 1 < Commands.Length )
					{
						++CommandIndex;
						FilteredUser = Commands[ CommandIndex ];
					}
					else
					{
						bValidCommand = false;
					}
				}
			}

			// After the optional arguments are accounted for, there are three more required arguments. Ensure they exist, or else
			// the command is invalid.
			if (CommandIndex + 2 >= Commands.Length)
			{
				bValidCommand = false;
			}
			else
			{
				// Extract the output file from the arguments
				OutputFile = Commands[CommandIndex++];

				// Make sure the output file has a required extension that corresponds to a valid export format
				int ExtensionIndex = OutputFile.LastIndexOf(".") + 1;
				if (ExtensionIndex <= 0 || ExtensionIndex >= OutputFile.Length || !ExtensionToExportFormatMap.TryGetValue(OutputFile.Substring(ExtensionIndex), out ExportFormat))
				{
					bValidCommand = false;
				}
				
				// Extract the query strings from the arguments
				QueryStartString = Commands[CommandIndex++];
				QueryEndString = Commands[CommandIndex++];
			}

			// If there are more commands left we haven't parsed, then extra data has been included and this is an invalid command
			if (CommandIndex != Commands.Length)
			{
				bValidCommand = false;
			}

			// If the command is valid, update the related boolean so ExecuteCommand knows all the relevant data is ok to use
			if (bValidCommand)
			{
				bValidCommandPending = true;
			}
			else
			{
				DisplayError(MalformedCommand, false);
			}
		}

		/// <summary>
		/// Attempt to execute a command based on cached data
		/// </summary>
        private static void ExecuteCommand()
        {
            // Ensure the command data is valid before proceeding
            if (bValidCommandPending)
            {
		        int QueryStartChangelistNumber = 0;
                int QueryEndChangelistNumber = 0;
                DateTime QueryStartDateTime = DateTime.MinValue;
                DateTime QueryEndDateTime = DateTime.MinValue;
                Label QueryStartLabel = null;
                Label QueryEndLabel = null;

                // Determine if both of the variables specified by the command line are valid ChangeLists
                if (IsChangelistValid(QueryStartString, out QueryStartChangelistNumber) && IsChangelistValid(QueryEndString, out QueryEndChangelistNumber))
                {
                    // Query the changelists specified by the time frame from the query operands
                    QueriedChangelists = PerforceInquirer.QueryChangelists(new P4ChangelistSpanQuery(QueryStartChangelistNumber, QueryEndChangelistNumber, FilteredDepotPath, FilteredUser));
                }
                // Determine if both of the variables specified by the command line are valid DateTimes
                else if (IsDateTimeValid(QueryStartString, out QueryStartDateTime) && IsDateTimeValid(QueryEndString, out QueryEndDateTime))
                {
                    // Query the changelists specified by the time frame from the query operands
                    QueriedChangelists = PerforceInquirer.QueryChangelists(new P4ChangelistSpanQuery(QueryStartDateTime, QueryEndDateTime, FilteredDepotPath, FilteredUser));
                }
                // Determine if both of the variables specified by the command line are valid Labels
                else if (IsLabelValid(QueryStartString, out QueryStartLabel) && IsLabelValid(QueryEndString, out QueryEndLabel))
                {
                    // Query the changelists specified by the time frame from the query operands
                    QueriedChangelists = PerforceInquirer.QueryChangelists(new P4ChangelistSpanQuery(QueryStartLabel, QueryEndLabel, FilteredDepotPath, FilteredUser));
                }
                // Warn the user of an invalid query operand
                else
                {
                    DisplayError(InvalidQueryOperand, true);
                }

                if (QueriedChangelists == null)
                {
                    DisplayError(ChangelistDepotMismatch, true);
                }
                else
                {
                    ParsedChangelists = new List<P4ParsedChangelist>();

                    // From each valid queried changelist, construct a parsed changelist (if the changelist supports the new tagging system)
                    foreach (Changelist CurrentCL in QueriedChangelists)
                    {
                        if (P4ParsedChangelist.ChangelistSupportsTagging(CurrentCL))
                        {
                            ParsedChangelists.Add(new P4ParsedChangelist(CurrentCL));
                        }
                    }
                }
                Console.WriteLine("Query returned:\n{0} Changelist(s)\n{1} Changelist(s) supporting tagging\n", QueriedChangelists.Count, ParsedChangelists.Count);

                // If at least one changelist was parsed, export it in Xml format to the specified output file
                if (ParsedChangelists.Count > 0)
                {
                    P4ParsedChangelist.Export(ExportFormat, ParsedChangelists, OutputFile);
                    Console.WriteLine("Data exported to {0}!\n", OutputFile);
                }


                // Clear both changelists lists
                if (QueriedChangelists != null)
                {
                    QueriedChangelists.Clear();
                }
                if (ParsedChangelists != null)
                {
                    ParsedChangelists.Clear();
                }
                bValidCommandPending = false;
            }
        }

        /// <summary>
        /// Check that the changelists are valid
        /// </summary>
        /// <param name="InChangelistNumber">Changelist number string to check for</param>
        /// <param name="OutChangelistNumber">Validated changelist number int</param>
        /// <returns>true if changelist exists, otherwise false</returns>
        private static bool IsChangelistValid(string InChangelistNumber, out int OutChangelistNumber)
        {
            // Assume the Changelist number is valid
            bool bChangelistIsValid = true;

            // Attempt to parse the string into a changelist number 
            if (int.TryParse(InChangelistNumber, out OutChangelistNumber))
            {
                // If the string was successfully parsed into a CL number, attempt to query for that CL's existence
                Changelist RequestedCL = PerforceInquirer.QueryChangelist(OutChangelistNumber);

                // If the changelist was invalid, return invalid
                if (RequestedCL == null)
                {
                    bChangelistIsValid = false;
                }
            }
            else
            {
                bChangelistIsValid = false;
                OutChangelistNumber = 0;
            }
            return bChangelistIsValid;
        }

        /// <summary>
        /// Check that the datetimes are valid
        /// </summary>
        /// <param name="InDateTime">DateTime string to check</param>
        /// <param name="OutDateTime">Validated DateTime</param>
        /// <returns>true if DateTime could be created, otherwise false</returns>
        private static bool IsDateTimeValid(string InDateTime, out DateTime OutDateTime)
        {
            // Assume the query time is valid
            bool bDateTimeIsValid = true;

            // Try to parse the string into a valid date format
            if (!DateTime.TryParseExact(InDateTime, AcceptedDateFormats, new CultureInfo("en-US"), DateTimeStyles.AssumeLocal, out OutDateTime))
            {
                // If not check whether the user typed the string specifying the current time, and if so, use the current time as the query time
                if (String.Compare(InDateTime.ToLower(), Settings.Default.NowString) == 0)
                {
                    OutDateTime = DateTime.UtcNow;
                }
                else
                {
                    bDateTimeIsValid = false;
                    OutDateTime = DateTime.MinValue;
                }
            }

            return bDateTimeIsValid;
        }

        /// <summary>
        /// Check that the labels are valid
        /// </summary>
        /// <param name="InLabel">Label string to check</param>
        /// <param name="OutLabel">Validated Label</param>
        /// <returns>true if Label exists, otherwise false</returns>
        private static bool IsLabelValid(string InLabel, out Label OutLabel)
        {
            // Assume the query time is valid
            bool bLabelIsValid = true;

            // Query for the requested label name
            OutLabel = PerforceInquirer.QueryLabel(InLabel);

            // If the label was invalid, return that a label could not be determined
            if (OutLabel == null)
            {
                bLabelIsValid = false;
            }

            return bLabelIsValid;
        }


		#endregion
	}
}
