// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows.Forms;
using UnrealDocTranslator.Properties;
using Perforce.P4;


namespace UnrealDocTranslator
{
    using File = System.IO.File;

    struct FileProcessingDetails
    {
        public enum State { New, Changed, MoveFrom, MoveTo, Delete, Translated, NotDeterminedYet };

        // INT File path in depot
        public string DepotFile { get; private set; }

        // INT File path on client
        public string ClientFile { get; private set; }

        // ChangeList of the Main file
        public int ChangeList { get; private set; }

        // FileState for this list row
        public State FileState { get; private set; }

        // Boolean value indicating whether another user has this file checked out
        public bool CheckedOutByOthers { get; private set; }

        // Short file name used in the list
        public string FileListShortName { get; private set; }

        public FileProcessingDetails(string DepotPath, string ClientPath, int HeadChange, State HeadState, bool CheckedOutByOthers, AppSettings _connectionDetails)
            :this()
        {
            // INT File path in depot
            this.DepotFile = DepotPath;

            // INT File path on client
            this.ClientFile = ClientPath;

            // ChangeList of the Main file
            this.ChangeList = HeadChange;

            // FileState for this list row
            this.FileState = HeadState;

            // Boolean value indicating whether another user has this file checked out
            this.CheckedOutByOthers = CheckedOutByOthers;

            // Short file name used in the list
            this.FileListShortName = CalculateFileShortName(_connectionDetails);
        }

        /// <summary>
        /// Returns a shortend name with only the crumb trail to the file, excludes the file extension
        /// </summary>
        /// <param name="_connectionDetails"></param>
        /// <returns></returns>
        private string CalculateFileShortName(AppSettings _connectionDetails )
        {
            int StartOfMatch = _connectionDetails.DepotPath.Length-Settings.Default.DepotPathMustEndWith.Length;
            int LengthOfMatch = DepotFile.Length - _connectionDetails.ShortFileNameStartPosition - 8; // 8 = ".INT.udn"

            //strip / from the beginning of the string if there.
            return DepotFile.Substring(_connectionDetails.ShortFileNameStartPosition, LengthOfMatch);
        }
    }

    struct MoveFileProcessingDetails
    {
        public string FromDepotFile { get; private set; }
        public string FromClientFile { get; private set; }
        public string ToDepotFile { get; private set; }
        public string ToClientFile { get; private set; }
        public int ChangeListId { get; private set; }
        public bool CheckedOutByOthers { get; private set; }

        public MoveFileProcessingDetails(string FromDepotPath, string FromClientPath, string ToDepotPath, string ToClientPath, int ChangeListId, bool CheckedOutByOthers)
            : this()
        {
            // File from path in depot
            this.FromDepotFile = FromDepotPath;

            // File from path on client
            this.FromClientFile = FromClientPath;

            // File to path in depot
            this.ToDepotFile = ToDepotPath;

            // File to path on client
            this.ToClientFile = ToClientPath;

            // ChangeList of the Main file
            this.ChangeListId = ChangeListId;

            // Boolean value indicating whether another user has this file checked out
            this.CheckedOutByOthers = CheckedOutByOthers;
        }
    }

    struct ChangelistDescription
    {
        private string Description { get; set; }
        private string DescriptionNoCarriageReturns { get; set; }

        public ChangelistDescription(string Description)
            : this()
        {
            // File from path in depot
            this.Description = Description;

            // File from path on client
            this.DescriptionNoCarriageReturns = Description.Replace(Environment.NewLine," ");
        }

        public string GetDescription(bool StripReturns)
        {
            return StripReturns ? DescriptionNoCarriageReturns : Description;
        }

    }
   
    
    public partial class UnrealDocTranslatorForm : Form
    {
        #region declarations

        private enum DefaultOnButton { New, Changed, Translated, None };
        
        // Define a static logger variable so that it references the
        private static readonly Logger log = new Logger();
        
        // Form which requests P4 server connection details
        private AppSettings _connectionDetails = new AppSettings();

        // List of all files from P4 documentation
        private IList<FileMetaData> ListOfAllFileStats;

        // List of new or updated INT files used for populating lists
        private Dictionary<string, FileProcessingDetails> INTFileProcessingDictionary = new Dictionary<string, FileProcessingDetails>();

        // List of deleted INT files used for populating lists
        private Dictionary<string, FileProcessingDetails> INTDeletedFileProcessingDictionary = new Dictionary<string, FileProcessingDetails>();

        // List of moved INT files used for populating lists (from this file)
        private Dictionary<string, FileProcessingDetails> INTMovedFileFromProcessingDictionary = new Dictionary<string, FileProcessingDetails>();

        // List of moved INT files used for populating lists (to this file)
        private Dictionary<string, FileProcessingDetails> INTMovedFileToProcessingDictionary = new Dictionary<string, FileProcessingDetails>();

        // List of the currently selected language files used for populating lists
        private Dictionary<string, FileProcessingDetails> LangFileProcessingDictionary = new Dictionary<string, FileProcessingDetails>();

        // List of ChangelistIds mapped to Descriptions to avoid repeated lookups for the same descriptions
        private Dictionary<int, ChangelistDescription> ChangelistIdDescriptionMapping = new Dictionary<int, ChangelistDescription>();

        //List of Language files that should be deleted as the int file is deleted since changelist
        private List<FileProcessingDetails> LangFileToDeleteProcessingList = new List<FileProcessingDetails>();

        //List of Language files that should be moved as the int file has been moved since changelist
        private List<MoveFileProcessingDetails> LangFileToMoveProcessingList = new List<MoveFileProcessingDetails>();

        //List of Language files that should be updated as the int file has been updated since changelist
        private List<FileProcessingDetails> LangFileToUpdateProcessingList = new List<FileProcessingDetails>();

        //List of Language files that have already been translated
        private List<FileProcessingDetails> LangFileTranslatedProcessingList = new List<FileProcessingDetails>();

        //List of Int files that should be used to create new language files
        private List<FileProcessingDetails> LangFileToCreateProcessingList = new List<FileProcessingDetails>();

        //List of udn Files currently checked out by this user
        private List<string> FilesCheckedOut = new List<string>();

        //Button toggle NotInLang on
        private bool NotInLangButtonOn = false;

        //Button toggle Update on
        private bool UpdatedInIntButtonOn = false;

        //Button toggle Translated on
        private bool AlreadyTranslatedButtonOn = false;

        private DefaultOnButton ButtonToSwitchOnAfterRefresh = DefaultOnButton.None;

        // P4 server and connection
        private Repository P4Repository = null;

        // Regex for selecting INT files from ListOfAllFileStats
        private readonly Regex ParseINTFileName = new Regex(@"(?<FileName>.*)\.INT\.udn$", RegexOptions.Singleline | RegexOptions.Compiled);

        /// <summary>
        /// Parameterised regex for selecting particular files based on the Language parameter
        /// </summary>
        /// <param name="Language"></param>
        /// <returns>Regex for selecting a Language's files from ListOfAllFileStats</returns>
        private Regex ParseLangFileName(string Language)
        {
            return new Regex(string.Format(@"(?<FileName>.*)\.{0}\.udn$", Language), RegexOptions.Singleline);
        }

        // Regex for retrieving INTSourceChangelist value from a file
        private Regex ParseSourceChangelist = new Regex(@"^INTSourceChangelist\:(?<ChangeListValue>[0-9]+)", RegexOptions.Multiline | RegexOptions.Compiled);

        // Hold process information for existing P4V
        private System.Diagnostics.Process P4V;

        // Hold process information for existing explorer
        private System.Diagnostics.Process WindowsExplorer;

        private int SortColumn = -1;

        #endregion declarations

        #region Form Events
        // Initialise this form
        public UnrealDocTranslatorForm()
        {
            InitializeComponent();

            TraceListener debugListener = new TraceListenerForTextBoxOutput(LogTextBox);
            Trace.Listeners.Add(debugListener);
            
            //FilterTextBox
            DefaultFilterBoxSetup();
            
            // If can not connect get connection details from the user.
            if (!CheckConnect() || !_connectionDetails.AraxisMergeDirectoryIsValid() || !_connectionDetails.PreferredLanguageIsValid() || !_connectionDetails.DepotPathIsValid())
            {
                GetConnectionDetails();
            }
            FileListView.FullRowSelect = true;
        }

        /// <summary>
        /// Populate list after form is displayed so that there is not a hanging delay for the user.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void UnrealDocTranslatorForm_Shown(object sender, EventArgs e)
        {
            Cursor.Current = Cursors.WaitCursor;
            RefreshP4Details();
            Cursor.Current = Cursors.Default;
        }

        private bool IsFileOpenedLocally(string DepotFileName)
        {
            bool IsOpenLocally = false;

            foreach (string CheckedOutFile in FilesCheckedOut)
            {
                if (CheckedOutFile.Equals(DepotFileName, StringComparison.OrdinalIgnoreCase))
                {
                    IsOpenLocally = true;
                }
            }
            return IsOpenLocally;
        }
        
        /// <summary>
        /// Populate or remove from the list of files those which have been created in INT based on NotInLangButtonOn state
        /// Also consider whether the user wishes to hide files for which the INT file is currently checked out by another user
        /// </summary>
        private void PopulateListWithNotInLang()
        {
            bool bUseFilter = !FilterTextBox.Text.Equals(Settings.Default.DefaultSearchBoxText);
            FileListView.BeginUpdate();
            if (NotInLangButtonOn)
            {
                //Add to list
                foreach (var LangFile in LangFileToCreateProcessingList)
                {
                    if ((!LangFile.CheckedOutByOthers || !_connectionDetails.UserSavedStateHideCheckedOut) && (!bUseFilter || LangFile.FileListShortName.IndexOf(FilterTextBox.Text, StringComparison.OrdinalIgnoreCase) >= 0))
                    {
                        var Item = new ListViewItem(LangFile.ChangeList.ToString());
                        Item.SubItems.Add(LangFile.FileListShortName);
                        Item.SubItems.Add(GetDescriptionForChangelist(LangFile.ChangeList, true));
                        Item.SubItems.Add(LangFile.ClientFile);
                        Item.SubItems.Add(LangFile.DepotFile);
                        Item.SubItems.Add(LangFile.FileState.ToString());
                        Item.Tag = GenerateTag(LangFile);

                        //Determine if the file is open locally
                        if (IsFileOpenedLocally(GetLangFileNameFromInt(LangFile.DepotFile)))
                        {
                            Item.ForeColor = Color.Red;
                        }
                        else
                        {
                            Item.ForeColor = Color.Black;
                        }

                        FileListView.Items.Add(Item);
                    }
                }
            }
            else
            {
                //Remove from list
                foreach (var FileToUpdate in LangFileToCreateProcessingList)
                {
                    RemoveRowFromFileListView(FileToUpdate);
                }
            }

            FileListView.EndUpdate();
            FileListView.Refresh();

            UpdateButtonText();
        }

        /// <summary>
        /// Populate or remove from the list of files those which are updated in INT since last update of the Language file based on UpdatedInIntButtonOn state
        /// Also consider whether the user wishes to hide files for which the INT file is currently checked out by another user
        /// </summary>
        private void PopulateListWithUpdatedInInt()
        {
            bool bUseFilter = !FilterTextBox.Text.Equals(Settings.Default.DefaultSearchBoxText);
            FileListView.BeginUpdate();
            if (UpdatedInIntButtonOn)
            {
                //Add to list
                foreach (var LangFile in LangFileToUpdateProcessingList)
                {
                    if ((!LangFile.CheckedOutByOthers || !_connectionDetails.UserSavedStateHideCheckedOut) && (!bUseFilter || LangFile.FileListShortName.IndexOf(FilterTextBox.Text, StringComparison.OrdinalIgnoreCase) >= 0))
                    {
                        var Item = new ListViewItem(LangFile.ChangeList.ToString());
                        Item.SubItems.Add(LangFile.FileListShortName);
                        Item.SubItems.Add(GetDescriptionForChangelist(LangFile.ChangeList,true));
                        Item.SubItems.Add(LangFile.ClientFile);
                        Item.SubItems.Add(LangFile.DepotFile);
                        Item.SubItems.Add(LangFile.FileState.ToString());
                        Item.Tag = GenerateTag(LangFile);

                        //Determine if the file is open locally
                        if (IsFileOpenedLocally(GetLangFileNameFromInt(LangFile.DepotFile)))
                        {
                            Item.ForeColor = Color.Red;
                        }
                        else
                        {
                            Item.ForeColor = Color.Black;
                        }

                        FileListView.Items.Add(Item);
                    }
                }
            }
            else
            {
                //Remove from list
                foreach (var FileToUpdate in LangFileToUpdateProcessingList)
                {
                    RemoveRowFromFileListView(FileToUpdate);
                }
            }

            FileListView.EndUpdate();
            FileListView.Refresh();

            UpdateButtonText();
        }

        private void PopulateListWithAlreadyTranslated()
        {
            bool bUseFilter = !FilterTextBox.Text.Equals(Settings.Default.DefaultSearchBoxText);
            FileListView.BeginUpdate();
            if (AlreadyTranslatedButtonOn)
            {
                //Add to list
                foreach (var LangFile in LangFileTranslatedProcessingList)
                {
                    if ((!LangFile.CheckedOutByOthers || !_connectionDetails.UserSavedStateHideCheckedOut) && (!bUseFilter || LangFile.FileListShortName.IndexOf(FilterTextBox.Text, StringComparison.OrdinalIgnoreCase) >= 0))
                    {
                        var Item = new ListViewItem(LangFile.ChangeList.ToString());
                        Item.SubItems.Add(LangFile.FileListShortName);
                        Item.SubItems.Add(GetDescriptionForChangelist(LangFile.ChangeList, true));
                        Item.SubItems.Add(LangFile.ClientFile);
                        Item.SubItems.Add(LangFile.DepotFile);
                        Item.SubItems.Add(LangFile.FileState.ToString());
                        Item.Tag = GenerateTag(LangFile);

                        //Determine if the file is open locally
                        if (IsFileOpenedLocally(GetLangFileNameFromInt(LangFile.DepotFile)))
                        {
                            Item.ForeColor = Color.Red;
                        }
                        else
                        {
                            Item.ForeColor = Color.Black;
                        }

                        FileListView.Items.Add(Item);
                    }
                }
            }
            else
            {
                //Remove from list
                foreach (var FileToUpdate in LangFileTranslatedProcessingList)
                {
                    RemoveRowFromFileListView(FileToUpdate);
                }
            }

            FileListView.EndUpdate();
            FileListView.Refresh();

            UpdateButtonText();
        }



        /// <summary>
        /// Event called when user clicks on the Not In X button, displays list of all files in INT where there is no matching file for the current selected language
        /// </summary>
        private void NotInLangButton_Click(object sender, EventArgs e)
        {
            NotInLangButtonOn = !NotInLangButtonOn;
            PopulateListWithNotInLang();
        }
 
        /// <summary>
        /// Event called when user clicks on the Updated INT button, displays list of all files in INT where the changelist number is greater than the matching current selected language file
        /// </summary>
        private void UpdatedInIntButton_Click(object sender, EventArgs e)
        {
            UpdatedInIntButtonOn = !UpdatedInIntButtonOn;
            PopulateListWithUpdatedInInt();
        }

        /// <summary>
        /// Event called when user clicks on the Already Translated button, displays list of all files in INT where the changelist number is greater than the matching current selected language file
        /// </summary>
        private void AlreadyTranslatedButton_Click(object sender, EventArgs e)
        {
            AlreadyTranslatedButtonOn = !AlreadyTranslatedButtonOn;
            PopulateListWithAlreadyTranslated();
        }

        /// <summary>
        /// Event called when the user clicks the P4 details button
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void P4DetailsButton_Click(object sender, EventArgs e)
        {
            GetConnectionDetails();
        }

        /// <summary>
        /// Error handling for P4 messages
        /// </summary>
        /// <param name="ErrorMessage"></param>
        /// <param name="Severity"></param>
        /// <returns></returns>
        private bool ReportP4Error(string ErrorMessage, ErrorSeverity Severity)
        {
            if (Severity == ErrorSeverity.E_FATAL || Severity == ErrorSeverity.E_UNKNOWN || Severity == ErrorSeverity.E_FAILED)
            {
                log.Error(ErrorMessage);
                return false;
            }
            else
            {
                return true;
            }
        }

        /// <summary>
        /// Event called on user clicking the Sync P4 button
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void SyncP4Button_Click(object sender, EventArgs e)
        {
            RefreshP4Details();
        }

        /// <summary>
        /// Event called on user entering into the FilterTextBox
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void FilerTextBox_Enter(object sender, EventArgs e)
        {
            //On entry into the box, if currently set to default value remove the text and set to black text
            if (FilterTextBox.Text.Equals(Settings.Default.DefaultSearchBoxText, StringComparison.OrdinalIgnoreCase))
            {
                FilterTextBox.ForeColor = Color.Black;
                FilterTextBox.Text = "";
            }
        }

        /// <summary>
        /// Handles setting filter text box to gray and placing default text in there if empty
        /// </summary>
        void DefaultFilterBoxSetup()
        {
            FilterTextBox.Text = Settings.Default.DefaultSearchBoxText;
            FilterTextBox.ForeColor = Color.LightGray;
        }
        
        /// <summary>
        /// Event called on user leaving the FilterTextBox
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void FilerTextBox_Leave(object sender, EventArgs e)
        {
            //On leaving the box, if empty set to gray and add
            if (String.IsNullOrWhiteSpace(FilterTextBox.Text))
            {
                DefaultFilterBoxSetup();
                FileListView.BeginUpdate();
                FileListView.Items.Clear();
                FileListView.EndUpdate();
                FileListView.Refresh();
                PopulateListWithNotInLang();
                PopulateListWithUpdatedInInt();
                PopulateListWithAlreadyTranslated();
            }
            else
            {
                //Need to filter the data in the list.
                //Clear the FileListView
                FileListView.BeginUpdate();
                FileListView.Items.Clear();
                FileListView.EndUpdate();
                FileListView.Refresh();
                PopulateListWithNotInLang();
                PopulateListWithUpdatedInInt();
                PopulateListWithAlreadyTranslated();

            }
        }

        /// <summary>
        /// If user presses escape in the filter box remove search string and leave text box
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void FilterTextBox_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Escape)
            {
                FilterTextBox.Text = "";
                //focus away from the text box on escape to force list update
                ButtonPanel.Focus();
            }
        }           


        

        

        // Implements the manual sorting of items by columns from http://msdn.microsoft.com/en-us/library/ms996467.aspx
        private void FileListView_ColumnClick(object sender, ColumnClickEventArgs e)
        {
            // Determine whether the column is the same as the last column clicked.
            if (e.Column != SortColumn)
            {
                // Set the sort column to the new column.
                SortColumn = e.Column;
                // Set the sort order to ascending by default.
                FileListView.Sorting = SortOrder.Ascending;
            }
            else
            {
                // Determine what the last sort order was and change it.
                if (FileListView.Sorting == SortOrder.Ascending)
                {
                    FileListView.Sorting = SortOrder.Descending;
                }
                else
                {
                    FileListView.Sorting = SortOrder.Ascending;
                }
            }

            // Call the sort method to manually sort.
            FileListView.Sort();
            // Set the ListViewItemSorter property to a new ListViewItemComparer
            // object.
            this.FileListView.ListViewItemSorter = new ListViewItemComparer(e.Column,FileListView.Sorting);
        }
        
        /// <summary>
        /// Display the dialog for getting P4 server connection details from the user
        /// </summary>
        private void GetConnectionDetails()
        {
            if (_connectionDetails != null)
            {
                _connectionDetails.ShowDialog();
                if (_connectionDetails.DialogResult == DialogResult.OK)
                {
                    CheckConnect();
                }
                //if connection details have changed refresh lists
                if (_connectionDetails.SettingsChanged)
                {
                    RefreshP4Details();
                    //Reset setting change, we've refreshed the data.
                    _connectionDetails.SettingsChanged = false;
                }
            }
        }

        /// <summary>
        /// On doubleclick of a list item perform translate on the file.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void FileListView_DoubleClick(object sender, EventArgs e)
        {
            translateToolStripMenuItem_Click(sender, e);
        }


        
        #endregion Form Events

        #region Unreal Doc Tool

        enum UdtRunMode
        {
            Normal,
            Preview,
            Log
        }

        private Thread currentUdtLoggingThread = null;

        private void UdtRun(string path, bool recursive = false, UdtRunMode mode = UdtRunMode.Normal, string lang = null)
        {
            path = new Uri(GetUnrealDocToolLocation()).MakeRelativeUri(new Uri(path)).ToString();

            if (currentUdtLoggingThread != null && currentUdtLoggingThread.IsAlive)
            {
                MessageBox.Show("Conversion in progress. You have to wait until previous finishes to run another.");
                return;
            }

            if (File.Exists(path) || Directory.Exists(path))
            {
                if (Directory.Exists(path) && recursive)
                {
                    path += "*";
                }

                // Relative path to UnrealDocTool, same folder.
                var startInfo = new ProcessStartInfo(GetUnrealDocToolLocation(),
                    String.Format("\"{0}\" " + GetParameters(mode), path) + (lang != null ? (" -lang=" + lang) : ""))
                                    {
                                        UseShellExecute = false,
                                        CreateNoWindow = true,
                                        RedirectStandardOutput = true,
                                        RedirectStandardError = true
                                    };

                var unrealDocToolProcess = Process.Start(startInfo);

                currentUdtLoggingThread = RunLoggingThread(unrealDocToolProcess);
            }
            else
            {
                log.Error(string.Format("Unable to convert, file is missing please translate first: {0}", path));
            }
        }

        private string GetUnrealDocToolLocation()
        {
            return Path.Combine(
                    P4Repository.Connection.Client.Root,
                    "UE4", "Engine", "Binaries", "DotNET", "UnrealDocTool.exe");
        }

        private static Thread RunLoggingThread(Process process)
        {
            ThreadStart start = () =>
                {
                    var writer = new StreamWriter(GetLogPath());

                    // Get the standard output to get logs from the files.
                    while (!process.HasExited)
                    {
                        var line = process.StandardOutput.ReadLine();
                        log.WriteToLog(line);
                        writer.WriteLine(line);
                    }

                    writer.Close();
                };

            var thread = new Thread(start);

            thread.Start();

            return thread;
        }

        private static string GetParameters(UdtRunMode mode)
        {
            switch (mode)
            {
                case UdtRunMode.Log:
                    return "-log";
                case UdtRunMode.Preview:
                    return "-p";
                case UdtRunMode.Normal:
                    return "";
                default:
                    throw new NotSupportedException("Not supported run mode.");
            }
        }

        private void LogAllFiles()
        {
            UdtRun(Path.Combine(
                    P4Repository.Connection.Client.Root,
                    "UE4", "Engine", "Documentation", "Source"),
                true, UdtRunMode.Log, _connectionDetails.UserSavedStatePreferredLanguage);
        }

        /// <summary>
        /// Run the UnrealDocTool to generate the HTML file for Filename
        /// </summary>
        /// <param name="fileName"></param>
        private void ConvertFile(string fileName, bool loggingMode = false)
        {
            UdtRun(fileName, false, loggingMode ? UdtRunMode.Log : UdtRunMode.Normal);
        }

        /// <summary>
        /// Run the UnrealDocTool to generate the preview HTML file for Filename
        /// </summary>
        /// <param name="fileName"></param>
        private void PreviewFile(string fileName)
        {
            UdtRun(fileName, false, UdtRunMode.Preview);
        }        
        #endregion Unreal Doc Tool

        #region Button Activation And Text Control

        /// <summary>
        /// Controls activation and text displayed in the three buttons at top of screen
        /// </summary>
        private void UpdateButtonTextNoData()
        {
            NotInLangButton.ResetBackColor();
            NotInLangButton.ResetForeColor();
            UpdatedInIntButton.ResetBackColor();
            UpdatedInIntButton.ResetForeColor();
            AlreadyTranslatedButton.ResetBackColor();
            AlreadyTranslatedButton.ResetForeColor();

            NotInLangButton.Text = "Use Refresh Lists";
            UpdatedInIntButton.Text = "Use Refresh Lists";
            AlreadyTranslatedButton.Text = "Use Refresh Lists";
            NotInLangButton.Enabled = false;
            UpdatedInIntButton.Enabled = false;
            AlreadyTranslatedButton.Enabled = false;
        }


        /// <summary>
        /// Controls activation and text displayed in the three buttons at top of screen
        /// </summary>
        private void UpdateButtonText()
        {
            //Check that the lists are populated.
            if (INTFileProcessingDictionary.Count > 0)
            {
                if (NotInLangButtonOn)
                {
                    NotInLangButton.BackColor = Color.Green;
                    NotInLangButton.ForeColor = Color.White;
                }
                else
                {
                    NotInLangButton.ResetBackColor();
                    NotInLangButton.ResetForeColor();
                }

                if (UpdatedInIntButtonOn)
                {
                    UpdatedInIntButton.BackColor = Color.Green;
                    UpdatedInIntButton.ForeColor = Color.White;
                }
                else
                {
                    UpdatedInIntButton.ResetBackColor();
                    UpdatedInIntButton.ResetForeColor();
                }

                if (AlreadyTranslatedButtonOn)
                {
                    AlreadyTranslatedButton.BackColor = Color.Green;
                    AlreadyTranslatedButton.ForeColor = Color.White;
                }
                else
                {
                    AlreadyTranslatedButton.ResetBackColor();
                    AlreadyTranslatedButton.ResetForeColor();
                }

                if (_connectionDetails.UserSavedStateHideCheckedOut) //Only count if values for checked out by other users are correct for current settings
                {
                    int LangFileToCreateCount = 0;
                    foreach (var LangFileToCreate in LangFileToCreateProcessingList)
                    {
                        if (!LangFileToCreate.CheckedOutByOthers)
                        {
                            ++LangFileToCreateCount;
                        }
                    }

                    int LangFileToUpdateCount = 0;
                    foreach (var LangFileToUpdate in LangFileToUpdateProcessingList)
                    {
                        if (!LangFileToUpdate.CheckedOutByOthers)
                        {
                            ++LangFileToUpdateCount;
                        }
                    }

                    int LangFileTranslatedCount = 0;
                    foreach (var LangFileAlreadyTranslated in LangFileTranslatedProcessingList)
                    {
                        if (!LangFileAlreadyTranslated.CheckedOutByOthers)
                        {
                            ++LangFileTranslatedCount;
                        }
                    }

                    if (LangFileToCreateCount > 0)
                    {
                        ButtonToSwitchOnAfterRefresh = DefaultOnButton.New;
                    }
                    else if (LangFileToUpdateCount > 0)
                    {
                        ButtonToSwitchOnAfterRefresh = DefaultOnButton.Changed;
                    }
                    else if (LangFileTranslatedCount > 0)
                    {
                        ButtonToSwitchOnAfterRefresh = DefaultOnButton.Translated;
                    }
                    else
                    {
                        ButtonToSwitchOnAfterRefresh = DefaultOnButton.None;
                    }
                    
                    NotInLangButton.Text = String.Format("New ({0})", LangFileToCreateCount);
                    UpdatedInIntButton.Text = String.Format("Updated ({0})", LangFileToUpdateCount);
                    AlreadyTranslatedButton.Text = String.Format("Translated ({0})", LangFileTranslatedCount);
                }
                else
                {
                    if (LangFileToCreateProcessingList.Count > 0)
                    {
                        ButtonToSwitchOnAfterRefresh = DefaultOnButton.New;
                    }
                    else if (LangFileToUpdateProcessingList.Count > 0)
                    {
                        ButtonToSwitchOnAfterRefresh = DefaultOnButton.Changed;
                    }
                    else if (LangFileTranslatedProcessingList.Count > 0)
                    {
                        ButtonToSwitchOnAfterRefresh = DefaultOnButton.Translated;
                    }
                    else
                    {
                        ButtonToSwitchOnAfterRefresh = DefaultOnButton.None;
                    } 
                    
                    NotInLangButton.Text = String.Format("New ({0})", LangFileToCreateProcessingList.Count);
                    UpdatedInIntButton.Text = String.Format("Updated ({0})", LangFileToUpdateProcessingList.Count);
                    AlreadyTranslatedButton.Text = String.Format("Translated ({0})", LangFileTranslatedProcessingList.Count);
                }
                NotInLangButton.Enabled = true;
                UpdatedInIntButton.Enabled = true;
                AlreadyTranslatedButton.Enabled = true;
            }
            else
            {
                UpdateButtonTextNoData();
            }
        }

        /// <summary>
        /// Function controls the operation buttons activation and text.
        /// </summary>
        private void MenuItemSettings()
        {
            translateToolStripMenuItem.Enabled = true;
            if (FileListView.SelectedItems.Count == 1)
            {
                //Preview and convert available if local file exists
                if ((new System.IO.FileInfo(GetLangFileNameFromInt(GetClientFile(FileListView.SelectedItems[0])))).Exists)
                {
                    previewToolStripMenuItem.Enabled = true;
                    convertToolStripMenuItem.Enabled = true;
                }
                else
                {
                    previewToolStripMenuItem.Enabled = false;
                    convertToolStripMenuItem.Enabled = false;
                }

                if (IsFileOpenedLocally(GetLangFileNameFromInt(GetDepotFile(FileListView.SelectedItems[0]))))
                {
                    revertToolStripMenuItem.Enabled = true;
                }
                else
                {
                    revertToolStripMenuItem.Enabled = false;
                }


                p4DetailsToolStripMenuItem.Enabled = true;
                openP4FolderToolStripMenuItem.Enabled = true;
                openLocalSourceFolderToolStripMenuItem.Enabled = true;
                checkoutINTFileToolStripMenuItem.Enabled = true;
                convertINTToolStripMenuItem.Enabled = true;
            }
            else
            {
                //Multiple rows selected can not really set the values here to on off with any meaning

                // for multiple rows we can not preview there is a chance the temp file will be deleted 
                // before it is loaded into the browser by the next preview task running
                previewToolStripMenuItem.Enabled = false;

                // for multiple rows we can convert fine
                convertToolStripMenuItem.Enabled = true;

                // allow revert to run, handle whether a file can be reverted in the revert operation
                revertToolStripMenuItem.Enabled = true;
                
                // Following tasks better on single items
                p4DetailsToolStripMenuItem.Enabled = false;
                openP4FolderToolStripMenuItem.Enabled = false;
                openLocalSourceFolderToolStripMenuItem.Enabled = false;
                checkoutINTFileToolStripMenuItem.Enabled = false;
                convertINTToolStripMenuItem.Enabled = false;
            }
        }
       
        /// <summary>
        /// Event called on right click on the list to handle the context menu
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void FileListViewContextMenu_Opening(object sender, CancelEventArgs e)
        {
            if (FileListView.SelectedItems.Count == 0)
            {
                e.Cancel = true;
            }
            else
            {
                //Set the menu items on or off
                MenuItemSettings();
            }
        }


        /// <summary>
        /// given an file name of form .INT.udn return the corresponding language file name .Lang.udn
        /// </summary>
        /// <param name="IntFileName"></param>
        /// <returns></returns>
        private string GetLangFileNameFromInt(string IntFileName)
        {
            return ParseINTFileName.Replace(IntFileName, String.Format("$1.{0}.udn", _connectionDetails.UserSavedStatePreferredLanguage));
        }

        /// <summary>
        /// Performs the action for a translate or diff operation based on the value of FullTranslate
        /// </summary>
        /// <param name="FullTranslate">If true will translate files and checkout the language file, otherwise just performs a diff operation</param>
        void TranslateOrDiff(bool FullTranslate)
        {
            foreach (ListViewItem SelectedRow in FileListView.SelectedItems)
            {
                if (GetState(SelectedRow).Equals(FileProcessingDetails.State.New.ToString()))
                {
                    //Copy file to Lang version only if file does not already exist.
                    string LangClientFileName = "";

                    //TempFileLocation or FullTranslate location
                    if (FullTranslate)
                    {
                        LangClientFileName = GetLangFileNameFromInt(GetClientFile(SelectedRow));
                    }
                    else
                    {
                        DirectoryInfo TempOutputDirectory = new DirectoryInfo(Path.Combine(System.IO.Path.GetTempPath(), "UnrealDocTranslator"));

                        if (!TempOutputDirectory.Exists)
                        {
                            TempOutputDirectory.Create();
                        }

                        LangClientFileName = Path.Combine(TempOutputDirectory.FullName,
                                                           (new System.IO.FileInfo(GetClientFile(SelectedRow))).Name);
                    }

                    if (!(new System.IO.FileInfo(GetClientFile(SelectedRow))).Exists)
                    {
                        log.Error(String.Format("Client file {0} does not exist, unable to create language file, try to Refresh P4 and synchronize", GetClientFile(SelectedRow)));
                        break;
                    }
                    else
                    {
                        // Does the local file exist? only create file once if not full translate
                        if (!(FullTranslate && (new System.IO.FileInfo(LangClientFileName)).Exists))
                        {
                            //Add Changelist number of INT file to top of lang file as metadata
                            string FileContents = "INTSourceChangelist:" + GetChangelist(SelectedRow) +
                                                    Environment.NewLine +
                                                    System.IO.File.ReadAllText(GetClientFile(SelectedRow), Encoding.UTF8);
                            System.IO.File.WriteAllText(LangClientFileName, FileContents, Encoding.UTF8);
                        }

                        // Add to P4 as file add
                        if (FullTranslate && CheckConnect())
                        {
                            using (Connection P4Connection = P4Repository.Connection)
                            {
                                FileSpec[] CurrentClientFile = new FileSpec[1];
                                CurrentClientFile[0] = new FileSpec(null, new ClientPath(LangClientFileName), null, null);

                                try
                                {
                                    P4Connection.Client.AddFiles(CurrentClientFile, null);
                                    SelectedRow.ForeColor = Color.Red;
                                    FilesCheckedOut.Add(GetLangFileNameFromInt(GetDepotFile(SelectedRow)));
                                }
                                catch (P4CommandTimeOutException Error)
                                {
                                    //do not continue
                                    log.Error(Error.Message);
                                    return;
                                }
                            }
                        }
                        string AraxisMergeComparePath = System.IO.Path.Combine(_connectionDetails.AraxisFolderName, "Compare.exe");
                        if ((new System.IO.FileInfo(AraxisMergeComparePath)).Exists)
                        {
                            ProcessStartInfo StartInfo = new ProcessStartInfo(AraxisMergeComparePath, String.Format("\"{0}\" \"{1}\"", GetClientFile(SelectedRow), LangClientFileName));
                            StartInfo.UseShellExecute = false;
                            StartInfo.CreateNoWindow = true;
                            StartInfo.RedirectStandardOutput = true;
                            StartInfo.RedirectStandardError = true;
                            System.Diagnostics.Process.Start(StartInfo);
                        }
                        else
                        {
                            log.Error("Unable to locate Araxis Merge compare, please use Change Settings to correct");
                            break;
                        }
                    }
                }
                else if (GetState(SelectedRow).Equals(FileProcessingDetails.State.Translated.ToString()))
                {
                    string LangFileProcessingDictionaryIndex = ParseINTFileName.Match(GetDepotFile(SelectedRow)).Groups["FileName"].Value.ToUpper();

                    FileProcessingDetails LangFileDetails;

                    if (!LangFileProcessingDictionary.TryGetValue(LangFileProcessingDictionaryIndex, out LangFileDetails))
                    {
                        log.Error("Can not find Lang file details for " + GetClientFile(SelectedRow));
                        break;
                    }

                    if (FullTranslate && CheckConnect())
                    {
                        using (Connection P4Connection = P4Repository.Connection)
                        {
                            //Set the language client file to edit
                            FileSpec[] CurrentClientFile = new FileSpec[1];
                            CurrentClientFile[0] = new FileSpec(null, new ClientPath(LangFileDetails.ClientFile), null, null);
                            try
                            {
                                P4Connection.Client.EditFiles(CurrentClientFile, null);
                                SelectedRow.ForeColor = Color.Red;
                                FilesCheckedOut.Add(GetLangFileNameFromInt(GetDepotFile(SelectedRow)));
                            }
                            catch (P4CommandTimeOutException Error)
                            {
                                //do not continue
                                log.Error(Error.Message);
                                return;
                            }
                        }
                    }

                    string AraxisMergeComparePath = System.IO.Path.Combine(_connectionDetails.AraxisFolderName, "Compare.exe");
                    if ((new System.IO.FileInfo(AraxisMergeComparePath)).Exists)
                    {
                        ProcessStartInfo StartInfo = new ProcessStartInfo(AraxisMergeComparePath, String.Format("\"{0}\" \"{1}\"", GetClientFile(SelectedRow), LangFileDetails.ClientFile));
                        StartInfo.UseShellExecute = false;
                        StartInfo.CreateNoWindow = true;
                        StartInfo.RedirectStandardOutput = true;
                        StartInfo.RedirectStandardError = true;
                        System.Diagnostics.Process.Start(StartInfo);
                    }
                    else
                    {
                        log.Error("Unable to locate Araxis Merge compare, please use Change Settings to correct");
                        break;
                    }
                }
                else if (GetState(SelectedRow).Equals(FileProcessingDetails.State.Delete.ToString()))
                {
                    //Should not be able to get here
                    log.Info("Not implemented for delete");
                }
                else if (GetState(SelectedRow).Equals(FileProcessingDetails.State.MoveTo.ToString()))
                {
                    //Should not be able to get here
                    log.Info("Not implemented for move");
                }
                else if (GetState(SelectedRow).Equals(FileProcessingDetails.State.Changed.ToString()))
                {
                    string LangFileProcessingDictionaryIndex = ParseINTFileName.Match(GetDepotFile(SelectedRow)).Groups["FileName"].Value.ToUpper();

                    FileProcessingDetails LangFileDetails;

                    if (!LangFileProcessingDictionary.TryGetValue(LangFileProcessingDictionaryIndex, out LangFileDetails))
                    {
                        log.Error("Can not find Lang file details for " + GetClientFile(SelectedRow));
                        break;
                    }


                    //Create temp file for diff to old INT file
                    string tempFileContent = "INTSourceChangelist:" + GetChangelist(SelectedRow) + Environment.NewLine;


                    if (CheckConnect())
                    {
                        using (Connection P4Connection = P4Repository.Connection)
                        {
                            FileSpec[] CurrentClientFile = new FileSpec[1];
                            if (FullTranslate)
                            {
                                //Set the language client file to edit
                                CurrentClientFile[0] = new FileSpec(null, new ClientPath(LangFileDetails.ClientFile), null, null);
                                try
                                {
                                    P4Connection.Client.EditFiles(CurrentClientFile, null);
                                    SelectedRow.ForeColor = Color.Red;
                                    FilesCheckedOut.Add(GetLangFileNameFromInt(GetDepotFile(SelectedRow)));
                                }
                                catch (P4CommandTimeOutException Error)
                                {
                                    //do not continue
                                    log.Error(Error.Message);
                                    return;
                                }
                            }

                            //Get the filespec of the int file for the current changelist number
                            CurrentClientFile[0] = GetIntFileSpecForFileChangelist(GetDepotFile(SelectedRow), LangFileDetails.ChangeList, true);

                            IList<string> listString = P4Repository.GetFileContents(CurrentClientFile, null);
                            if (listString.Count > 1)
                            {
                                tempFileContent += listString[1];
                            }
                        }
                    }
                    DirectoryInfo TempOutputDirectory = new DirectoryInfo(Path.Combine(System.IO.Path.GetTempPath(), "UnrealDocTranslator"));

                    if (!TempOutputDirectory.Exists)
                    {
                        TempOutputDirectory.Create();
                    }

                    string TempFileName = Path.Combine(TempOutputDirectory.FullName,
                                                       (new System.IO.FileInfo(GetClientFile(SelectedRow))).Name);

                    System.IO.File.WriteAllText(TempFileName, tempFileContent);

                    string AraxisMergeComparePath = System.IO.Path.Combine(_connectionDetails.AraxisFolderName, "Compare.exe");
                    if ((new System.IO.FileInfo(AraxisMergeComparePath)).Exists)
                    {
                        ProcessStartInfo StartInfo = new ProcessStartInfo(AraxisMergeComparePath, String.Format("/a3 /3 \"{0}\" \"{1}\" \"{2}\"", GetClientFile(SelectedRow), LangFileDetails.ClientFile, TempFileName));
                        StartInfo.UseShellExecute = false;
                        StartInfo.CreateNoWindow = true;
                        StartInfo.RedirectStandardOutput = true;
                        StartInfo.RedirectStandardError = true;
                        System.Diagnostics.Process.Start(StartInfo);
                    }
                    else
                    {
                        log.Error("Unable to locate Araxis Merge compare, please use Change Settings to correct");
                        break;
                    }
                }
            }
        }
        
        /// <summary>
        /// Event called when user clicks the Translate option from the menu
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void translateToolStripMenuItem_Click(object sender, EventArgs e)
        {
            TranslateOrDiff(true);
        }

        /// <summary>
        /// Generate preview of the language file.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void previewToolStripMenuItem_Click(object sender, EventArgs e)
        {
            foreach (ListViewItem SelectedRow in FileListView.SelectedItems)
            {
                PreviewFile(GetLangFileNameFromInt(GetClientFile(SelectedRow)));
            }
        }

        /// <summary>
        /// Run the Local int client file through the unrealdoctool in create html mode
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void convertINTToolStripMenuItem_Click(object sender, EventArgs e)
        {
            foreach (ListViewItem SelectedRow in FileListView.SelectedItems)
            {
                ConvertFile(GetClientFile(SelectedRow));
            }
        }

        /// <summary>
        /// Run the Local lang client file through the unrealdoctool in create html mode
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void convertToolStripMenuItem_Click(object sender, EventArgs e)
        {
            foreach (ListViewItem SelectedRow in FileListView.SelectedItems)
            {
                ConvertFile(GetLangFileNameFromInt(GetClientFile(SelectedRow)));
            }
        }

        /// <summary>
        /// Run the Local lang client file through the unrealdoctool in log mode
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void verifyToolStripMenuItem_Click(object sender, EventArgs e)
        {
            foreach (ListViewItem SelectedRow in FileListView.SelectedItems)
            {
                ConvertFile(GetLangFileNameFromInt(GetClientFile(SelectedRow)), true);
            }
        }

        /// <summary>
        /// Check out the INT file for this lang file
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void checkoutINTFileToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (CheckConnect())
            {
                using (Connection P4Connection = P4Repository.Connection)
                {
                    foreach (ListViewItem SelectedRow in FileListView.SelectedItems)
                    {
                        //Set the language client file to edit
                        FileSpec[] CurrentClientFile = new FileSpec[1];
                        CurrentClientFile[0] = new FileSpec(null, new ClientPath(GetClientFile(SelectedRow)), null, null);
                        try
                        {
                            P4Connection.Client.EditFiles(CurrentClientFile, null);
                        }
                        catch (P4CommandTimeOutException Error)
                        {
                            log.Error(Error.Message);
                            //do not continue
                            return;
                        }
                    }
                }
            }
        }

        /// <summary>
        /// Navigate P4 to the folder of this file
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void openP4FolderToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (P4V == null)
            {
                P4V = new System.Diagnostics.Process();
                P4V.StartInfo.FileName = "p4v.exe";
            }

            P4V.StartInfo.Arguments = String.Format("-s {0}", (GetDepotFile(FileListView.SelectedItems[0])));
            P4V.Start();
        }


        /// <summary>
        /// Open explorer window for the folder of the file
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void openLocalSourceFolderToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (WindowsExplorer == null)
            {
                WindowsExplorer = new System.Diagnostics.Process();
                WindowsExplorer.StartInfo.FileName = "explorer.exe";
            }

            WindowsExplorer.StartInfo.Arguments = (new System.IO.FileInfo(GetClientFile(FileListView.SelectedItems[0]))).DirectoryName;
            WindowsExplorer.Start();
        }

        /// <summary>
        /// Run a diff, similar to Translate without checkout.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void diffToolStripMenuItem_Click(object sender, EventArgs e)
        {
            TranslateOrDiff(false);
        }

        /// <summary>
        /// Selects all List View items
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void selectAllToolStripMenuItem_Click(object sender, EventArgs e)
        {
            foreach (ListViewItem Item in FileListView.Items)
            {
                Item.Selected = true;
            }
        }

        /// <summary>
        /// Revert the selected files.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void revertToolStripMenuItem_Click(object sender, EventArgs e)
        {
            List<string> FilesToRevert = new List<string>();

            foreach (ListViewItem SelectedRow in FileListView.SelectedItems)
            {
                //Only revert files that are opened.
                if (IsFileOpenedLocally(GetLangFileNameFromInt(GetDepotFile(SelectedRow))))
                {
                    FilesToRevert.Add(GetLangFileNameFromInt(GetDepotFile(SelectedRow)));
                    //assume we will complete the operation, on error reset the values to red
                    SelectedRow.ForeColor = Color.Black;
                }
            }

            // if there is a file to revert and we have a valid p4 connection
            if (FilesToRevert.Count > 0 && CheckConnect())
            {
                List<FileSpec> FileSpecsToRevert = new List<FileSpec>();
                foreach (string File in FilesToRevert)
                {
                    FileSpecsToRevert.Add(new FileSpec(new DepotPath(File), null, null, null));
                }

                using (Connection P4Connection = P4Repository.Connection)
                {
                    try
                    {
                        P4Connection.Client.RevertFiles(null, FileSpecsToRevert.ToArray());

                        //Remove the reverted files from the list of OpenedFiles.
                        foreach (string File in FilesToRevert)
                        {
                            FilesCheckedOut.Remove(File);
                        }
                    }
                    catch (P4Exception ex)
                    {
                        //On P4Exception set the rows back to red
                        foreach (ListViewItem SelectedRow in FileListView.SelectedItems)
                        {
                            //Only revert files that are opened.
                            if (IsFileOpenedLocally(GetLangFileNameFromInt(GetDepotFile(SelectedRow))))
                            {
                                //assume we will complete the operation, on error reset the values to red
                                SelectedRow.ForeColor = Color.Red;
                            }
                        }
                        log.Error("Error in reverting files:");
                        log.Error(ex.Message);
                    }
                }
            }
        }

        /// <summary>
        /// Submit all in the default changelist
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void submitAllToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (CheckConnect())
            {
                using (Connection P4Connection = P4Repository.Connection)
                {
                    try
                    {
                        //revert unchanged files.
                        List<FileSpec> FileSpecsToRevert = new List<FileSpec>();
                        foreach (string File in FilesCheckedOut)
                        {
                            FileSpecsToRevert.Add(new FileSpec(new DepotPath(File), null, null, null));
                        }

                        Options RevertOptions = new Options();

                        //Only files  that are open for edit or integrate and are unchanged or missing
                        RevertOptions.Add("-a", null);

                        P4Connection.Client.RevertFiles(RevertOptions, FileSpecsToRevert.ToArray());

                        // Submit remaining changelist
                        Options SubmitOptions = new Options();

                        //Add a description
                        SubmitOptions.Add("-d", String.Format(Settings.Default.SubmitChangelistDescription, _connectionDetails.UserSavedStatePreferredLanguage));

                        SubmitResults UpdatesInsertsSubmitResults = P4Connection.Client.SubmitFiles(SubmitOptions, null);

                        log.Info(String.Format("Updated and New files submitted in CL# {0}, Files affected:", UpdatesInsertsSubmitResults.ChangeIdAfterSubmit));
                        foreach (var SubmittedFile in UpdatesInsertsSubmitResults.Files)
                        {
                            log.Info(String.Format("\t{0}:\t{1}({2})", SubmittedFile.Action, SubmittedFile.File.DepotPath, SubmittedFile.File.Version));
                        }


                        //Clear checked out files.
                        FilesCheckedOut.Clear();

                        //Mark lines black text
                        foreach (ListViewItem line in FileListView.Items)
                        {
                            line.ForeColor = Color.Black;
                        }
                    }
                    catch (P4Exception ex)
                    {
                        //Swallow No files to submit error
                        if (ex.ErrorCode != 806427698)
                        {
                            log.Error("Error in submitting changelist:");
                            log.Error(ex.Message);
                        }
                    }
                }
            }
        }

        /// <summary>
        /// Get the details of the changelist for the selected row.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void p4DetailsToolStripMenuItem_Click(object sender, EventArgs e)
        {
            foreach (ListViewItem SelectedRow in FileListView.SelectedItems)
            {
                if (CheckConnect())
                {
                    int ChangeListNumber = 0;
                    if (Int32.TryParse(GetChangelist(SelectedRow), out ChangeListNumber))
                    {

                        Changelist ChangeListDetails = P4Repository.GetChangelist(ChangeListNumber);

                        log.Info(String.Format("Changelist #{0}, Modified on {1} by {2}, Files affected:", ChangeListDetails.Id, ChangeListDetails.ModifiedDate, ChangeListDetails.OwnerName));
                        foreach (var ChangeListDetailsFile in ChangeListDetails.Files)
                        {
                            log.Info(String.Format("\t{0}\t{1}", ChangeListDetailsFile.Action, ChangeListDetailsFile.DepotPath));
                        }
                    }
                    else
                    {
                        //Should not normally be able to get here.
                        log.Error(string.Format("Unable determine numberic ChangeList number from {0}", GetChangelist(SelectedRow)));
                    }
                }
            }
        }

        /// <summary>
        /// Handle key presses on the File List for the menu options
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void FileListView_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Control)
            {
                switch (e.KeyCode)
                {
                    case Keys.E:
                        translateToolStripMenuItem_Click(sender, e);
                        break;
                    case Keys.D:
                        diffToolStripMenuItem_Click(sender, e);
                        break;
                    case Keys.V:
                        previewToolStripMenuItem_Click(sender, e);
                        break;
                    case Keys.C:
                        convertToolStripMenuItem_Click(sender, e);
                        break;
                    case Keys.F:
                        verifyToolStripMenuItem_Click(sender, e);
                        break;
                    case Keys.A:
                        selectAllToolStripMenuItem_Click(sender, e);
                        break;
                    case Keys.S:
                        submitAllToolStripMenuItem_Click(sender, e);
                        break;
                    default:
                        //do nothing on unrecognized key
                        break;
                }
            }
            else if (e.KeyCode == Keys.F5)
            {
                RefreshP4Details();
            }
        }


        #endregion Button Activation And Text Control

        #region P4 List Control
        /// <summary>
        /// Get the description for a changelist
        /// </summary>
        /// <param name="ChangelistId">Changelist identifier to get the description for</param>
        /// <param name="StripReturns">whether to strip the line end characters and replace with a space</param>
        /// <returns>A description for the changelist provided</returns>
        private string GetDescriptionForChangelist(int ChangelistId, bool StripReturns)
        {
            //if we have already looked up this changelist description use that
            ChangelistDescription Description;
            if (ChangelistIdDescriptionMapping.TryGetValue(ChangelistId, out Description))
            {
                return Description.GetDescription(StripReturns);
            }
            else
            {
                return "Unable to get description from P4";
            }
        }

        
        /// <summary>
        /// Get the FileSpec for a depot file given a changelist
        /// </summary>
        /// <param name="depotFileName">The file name in current depot location</param>
        /// <param name="changelist">The language file's changelist to be used to find an older version of the int file</param>
        /// <param name="checkForRenameMove">Check if the file has changed names due to a move/rename</param>
        /// <returns>FileSpec which may have a different depotfilename than that passed into function if the file has been moved since changelist</returns>
        private FileSpec GetIntFileSpecForFileChangelist(string depotFileName, int changelist, bool checkForRenameMove)
        {
            if (!CheckConnect())
            {
                return null;
            }

            if (checkForRenameMove)
            {
                PathSpec HistoryPathSpec = new DepotPath(depotFileName) as PathSpec;

                FileSpec[] HistoryFiles = new FileSpec[1];
                HistoryFiles[0] = new FileSpec(HistoryPathSpec, VersionSpec.Head);

                Options Opts = new Options();
                Opts.Add("-i", "");
                IList<FileHistory> History = P4Repository.GetFileHistory(Opts, HistoryFiles);

                // walk through history until we find the first changelist older than the specified changelist
                foreach (FileHistory Item in History)
                {
                    if (Item.ChangelistId > changelist)
                    {
                        continue;
                    }

                    // use the depot filename at this revision
                    HistoryPathSpec = Item.DepotPath;
                    break;
                }

                return new FileSpec(HistoryPathSpec, new ChangelistIdVersion(changelist));           
            }
            else
            {
                return new FileSpec(new DepotPath(depotFileName) as PathSpec, new ChangelistIdVersion(changelist));
            }                   
        }

        /// <summary>
        /// Creates LangFileProcessingDictionary based on the currently selected language
        /// </summary>
        private void CreateLanguageFileDictionary()
        {
            //From the list of file stats set up the Lang dictionary
            log.Info("Processing language files");

            if (ListOfAllFileStats != null && !string.IsNullOrWhiteSpace(_connectionDetails.UserSavedStatePreferredLanguage))
            {
                LangFileProcessingDictionary.Clear();
                foreach (FileMetaData FileDetails in ListOfAllFileStats)
                {
                    if (FileDetails.DepotPath == null)
                    {
                        log.Warn("DepotPath missing for one of the files on language CreateLanguageFileDictionary");
                    }
                    else if (FileDetails.ClientPath == null)
                    {
                        log.Warn("ClientPath missing for DepotPath: " + FileDetails.DepotPath.ToString());
                    }
                    else
                    {
                        string PartFileName = ParseLangFileName(_connectionDetails.UserSavedStatePreferredLanguage).Match(FileDetails.DepotPath.ToString()).Groups["FileName"].Value.ToUpper();

                        if (!string.IsNullOrWhiteSpace(PartFileName) && !(FileDetails.HeadAction == FileAction.Delete || FileDetails.HeadAction == FileAction.MoveDelete))
                        {
                            //check files for INTSourceChangelist:
                            int ChangelistValue = ChooseHeadChangeOrFileChangeListNumber(FileDetails.HeadChange, FileDetails.ClientPath.ToString());

                            LangFileProcessingDictionary.Add(PartFileName, new FileProcessingDetails(FileDetails.DepotPath.ToString(), FileDetails.ClientPath.ToString(), ChangelistValue, FileProcessingDetails.State.NotDeterminedYet, false, _connectionDetails));
                        }
                    }
                }
            }

            //Setup list of files for creation
            log.Info("Generating list of missing language files");
            LangFileToCreateProcessingList.Clear();
            foreach (var INTFile in INTFileProcessingDictionary)
            {
                if (!LangFileProcessingDictionary.ContainsKey(INTFile.Key))
                {
                    LangFileToCreateProcessingList.Add(new FileProcessingDetails(INTFile.Value.DepotFile, INTFile.Value.ClientFile, INTFile.Value.ChangeList, FileProcessingDetails.State.New, INTFile.Value.CheckedOutByOthers, _connectionDetails));
                }
            }

            //Setup list of files for change and translated
            log.Info("Generating list of language files needing update or already translated");
            LangFileToUpdateProcessingList.Clear();
            LangFileTranslatedProcessingList.Clear();
            foreach (var INTFile in INTFileProcessingDictionary)
            {
                if (LangFileProcessingDictionary.ContainsKey(INTFile.Key))
                {
                    FileProcessingDetails LangFile;
                    if (LangFileProcessingDictionary.TryGetValue(INTFile.Key, out LangFile))
                    {
                        if (LangFile.ChangeList < INTFile.Value.ChangeList)
                        {
                            // Needs update
                            LangFileToUpdateProcessingList.Add(new FileProcessingDetails(INTFile.Value.DepotFile, INTFile.Value.ClientFile, INTFile.Value.ChangeList, FileProcessingDetails.State.Changed, INTFile.Value.CheckedOutByOthers, _connectionDetails));
                        }
                        else
                        {
                            // Already translated
                            LangFileTranslatedProcessingList.Add(new FileProcessingDetails(INTFile.Value.DepotFile, INTFile.Value.ClientFile, INTFile.Value.ChangeList, FileProcessingDetails.State.Translated, INTFile.Value.CheckedOutByOthers, _connectionDetails));
                        }
                    }
                }
            }

            //Setup list of files for move
            log.Info("Checking list of language files to be moved");
            LangFileToMoveProcessingList.Clear();
            foreach (var LangFile in LangFileProcessingDictionary)
            {
                FileProcessingDetails MovedFromFileDetails;
                if (INTMovedFileFromProcessingDictionary.TryGetValue(LangFile.Key, out MovedFromFileDetails))
                {

                    log.Info("Locating destination of language files that need to be moved - This is a long process");
                    var MovedToFileDetails = GetMovedToLocation(MovedFromFileDetails);

                    LangFileToMoveProcessingList.Add(new MoveFileProcessingDetails(LangFile.Value.DepotFile,
                                                                                   LangFile.Value.ClientFile,
                                                                                   GetLangFileNameFromInt(MovedToFileDetails.DepotFile),
                                                                                   GetLangFileNameFromInt(MovedToFileDetails.ClientFile),
                                                                                   MovedToFileDetails.ChangeList,
                                                                                   MovedToFileDetails.CheckedOutByOthers));
                }
            }

            //Setup list of files for delete
            log.Info("Generating list of language files needing to be deleted");
            LangFileToDeleteProcessingList.Clear();
            foreach (var LangFile in LangFileProcessingDictionary)
            {
                if (INTDeletedFileProcessingDictionary.ContainsKey(LangFile.Key))
                {
                    LangFileToDeleteProcessingList.Add(new FileProcessingDetails(LangFile.Value.DepotFile, LangFile.Value.ClientFile, LangFile.Value.ChangeList, FileProcessingDetails.State.Delete, false, _connectionDetails));
                }
            }
            //Update buttons to match counts
            UpdateButtonText();

        }

        private FileProcessingDetails GetMovedToLocation(FileProcessingDetails MovedFromFileDetails)
        {
            //Find the moved to location of moved from files, need to examine the changelist, do this in languages as it is quite slow so should only process those moves which we need to.
            var ListOfFilesInMovedFromChangeList =
                P4Repository.GetChangelist(MovedFromFileDetails.ChangeList).Files;

            //For the files that are MoveAdd
            foreach (var FileDetails in ListOfFilesInMovedFromChangeList)
            {
                if (FileDetails.Action == FileAction.MoveAdd)
                {
                    //This could be the file we are looking for.  Check the file history of this file.
                    FileSpec[] CurrentClientFile = new FileSpec[1];
                    CurrentClientFile[0] = new FileSpec(new DepotPath(FileDetails.DepotPath.ToString()), null);
                    IList<FileHistory> FileHistory = P4Repository.GetFileHistory(CurrentClientFile, null);

                    foreach (var SliceOfHistory in FileHistory)
                    {
                        if (SliceOfHistory.ChangelistId == MovedFromFileDetails.ChangeList)
                        {
                            foreach (var IntegrationSummary in SliceOfHistory.IntegrationSummaries)
                            {
                                if (IntegrationSummary.FromFile.DepotPath.ToString() == MovedFromFileDetails.DepotFile)
                                {
                                    //Does anyone else have this checked out?
                                    bool IsCheckedOutByOtherUser = FileDetails.OtherUsers == null ? false : true;

                                    //Found where it moves to initially, what if it moved again?
                                    //Would like to determine the last moved to location.
                                    if (FileHistory[0].Action == FileAction.MoveDelete)
                                    {
                                        return
                                            GetMovedToLocation(new FileProcessingDetails(
                                                                   FileDetails.DepotPath.ToString(),
                                                                   "",
                                                                   FileHistory[0].ChangelistId,
                                                                   FileProcessingDetails.State.MoveTo,
                                                                   IsCheckedOutByOtherUser, 
                                                                   _connectionDetails));
                                    }
                                    else
                                    {
                                        //Need the client path of this file to be able to rename the language file on move correctly.
                                        string ClientPath = "";

                                        FileProcessingDetails ClientFileDetails;

                                        //Last task on the file could be the MoveAdd, it could also be an update, or delete, find the ClientPath
                                        if (
                                            INTMovedFileToProcessingDictionary.TryGetValue(
                                                ParseINTFileName.Match(FileDetails.DepotPath.ToString()).Groups[
                                                    "FileName"].Value.ToUpper(), out ClientFileDetails))
                                        {
                                            ClientPath = ClientFileDetails.ClientFile;
                                        }
                                        else if (
                                            INTDeletedFileProcessingDictionary.TryGetValue(
                                                ParseINTFileName.Match(FileDetails.DepotPath.ToString()).Groups[
                                                    "FileName"].Value.ToUpper(), out ClientFileDetails))
                                        {
                                            ClientPath = ClientFileDetails.ClientFile;
                                        }
                                        else if (
                                            INTFileProcessingDictionary.TryGetValue(
                                                ParseINTFileName.Match(FileDetails.DepotPath.ToString()).Groups[
                                                    "FileName"].Value.ToUpper(), out ClientFileDetails))
                                        {
                                            ClientPath = ClientFileDetails.ClientFile;
                                        }

                                        return
                                            (new FileProcessingDetails(FileDetails.DepotPath.ToString(),
                                                                       ClientPath,
                                                                       MovedFromFileDetails.ChangeList,
                                                                       FileProcessingDetails.State.MoveTo,
                                                                       IsCheckedOutByOtherUser,
                                                                       _connectionDetails));
                                    }
                                }
                            }
                        }
                    }
                }
            }
            //Should not really get here unless P4 is corrupt.
            return (new FileProcessingDetails());
        }

        /// <summary>
        /// Gets the change list number to use when processing differences for this file
        /// </summary>
        /// <param name="ChangeListValue">Value from headchange of the P4 file</param>
        /// <param name="ClientFilePath">Path to the file on the client machine</param>
        /// <returns>If INTSourceChangelist returns a value from the file then return that, otherwise return ChangeList Value</returns>
        private int ChooseHeadChangeOrFileChangeListNumber(int ChangeListValue, string ClientFilePath)
        {
            if (System.IO.File.Exists(ClientFilePath))
            {
                string FileContents = System.IO.File.ReadAllText(ClientFilePath);

                string FileChangeListValue = ParseSourceChangelist.Match(FileContents).Groups["ChangeListValue"].Value;

                if (string.IsNullOrWhiteSpace(FileChangeListValue))
                {
                    return ChangeListValue;
                }
                else
                {
                    try
                    {
                        return (Convert.ToInt32(FileChangeListValue));
                    }
                    catch (Exception Error)
                    {
                        if (Error is FormatException || Error is OverflowException)
                        {
                            return ChangeListValue;
                        }
                        else
                        {
                            throw;
                        }
                    }
                }
            }
            return ChangeListValue;
        }

        /// <summary>
        /// Initialise P4 server connection
        /// </summary>
        private bool CheckConnect()
        {
            if (P4Repository != null)
            {
                //release any old connection
                P4Repository.Dispose();
            }

            try
            {
                var P4Server = new Server(new ServerAddress(_connectionDetails.ServerName));

                P4Repository = new Repository(P4Server);

                P4Repository.Connection.UserName = _connectionDetails.UserName;
                var P4Options = new Options();

                P4Repository.Connection.Client = new Client();

                P4Repository.Connection.Client.Name = _connectionDetails.WorkspaceName;

                P4Repository.Connection.Connect(P4Options);

                return true;
            }
            catch (Exception ex)
            {
                P4Repository = null;
                log.Error("Error in connecting to Perforce server:");
                log.Error(ex.Message);
                return false;
            }
        }


        /// <summary>
        /// List control from P4
        /// </summary>
        private void RefreshP4Details ()
        {
            //If any temporary files delete them
            DirectoryInfo TempOutputDirectory = new DirectoryInfo(Path.Combine(System.IO.Path.GetTempPath(), "UnrealDocTranslator"));

            if (TempOutputDirectory.Exists)
            {
                TempOutputDirectory.Delete(true);
            }

            try
            {
                Cursor.Current = Cursors.WaitCursor;

                if (CheckConnect())
                {
                    using (Connection P4Connection = P4Repository.Connection)
                    {
                        FileSpec[] HeadSourceUdnFiles = new FileSpec[1];
                        HeadSourceUdnFiles[0] = new FileSpec(
                            new DepotPath(_connectionDetails.DepotPath), null, null, VersionSpec.Head);

                        FileSpec[] OpenedFiles = new FileSpec[1];
                        OpenedFiles[0] = new FileSpec(
                            new DepotPath("..."), null, null, null);    // Make sure there are no files in any path open for edit in the default changelist
                                                                        // They will be unintentionally submitted otherwise.

                        try
                        {
                            var ListOfOpenedFileStats = P4Repository.GetOpenedFiles(OpenedFiles, null);

                            FilesCheckedOut.Clear();

                            //If there are any opened files 
                            if (ListOfOpenedFileStats != null)
                            {
                                foreach (var FileStat in ListOfOpenedFileStats)
                                {
                                    //if any of the files are in the default changelist stop processing.
                                    if (FileStat.ChangeId == 0)
                                    {
                                        log.Error("Your default Changelist is not empty. Please Submit All or Revert before you proceed.");
                                        return;
                                    }
                                }
                            }

                            log.Info("Sync P4 to get latest changes");
                            P4Repository.Connection.Client.SyncFiles(null, HeadSourceUdnFiles);

                            log.Info("Getting file metadata from p4");
                            ListOfAllFileStats = P4Repository.GetFileMetaData(null, HeadSourceUdnFiles);
                        
                        }
                        catch (P4Exception ErrorMessage)
                        {
                            log.Error(ErrorMessage.Message);
                            return;
                        }

                        INTFileProcessingDictionary.Clear();
                        INTMovedFileFromProcessingDictionary.Clear();
                        INTMovedFileToProcessingDictionary.Clear();
                        INTDeletedFileProcessingDictionary.Clear();

                        log.Info("Detecting status of INT files");

                        //From the list of file stats set up the INT dictionary
                        foreach (FileMetaData FileDetails in ListOfAllFileStats)
                        {
                            string PartFileName = FileDetails.DepotPath == null ? "" : ParseINTFileName.Match(FileDetails.DepotPath.ToString()).Groups["FileName"].Value.ToUpper();

                            if (!string.IsNullOrWhiteSpace(PartFileName))
                            {
                                //Does anyone else have this checked out?
                                bool IsCheckedOutByOtherUser = FileDetails.OtherUsers == null ? false : true;


                                //Depot file could be determined?
                                if (FileDetails.DepotPath == null)
                                {
                                    log.Warn("DepotPath missing for PartFileName: " + PartFileName);
                                }
                                else
                                {

                                    if (FileDetails.HeadAction == FileAction.Delete)
                                    {
                                        INTDeletedFileProcessingDictionary.Add(PartFileName,
                                                                               new FileProcessingDetails(
                                                                                   FileDetails.DepotPath.ToString(),
                                                                                   FileDetails.ClientPath == null ? "" : FileDetails.ClientPath.ToString(),
                                                                                   FileDetails.HeadChange,
                                                                                   FileProcessingDetails.State.Delete,
                                                                                   IsCheckedOutByOtherUser,
                                                                                   _connectionDetails));
                                    }
                                    else if (FileDetails.HeadAction == FileAction.MoveDelete)
                                    {
                                        INTMovedFileFromProcessingDictionary.Add(PartFileName,
                                                                                 new FileProcessingDetails(
                                                                                     FileDetails.DepotPath.ToString(),
                                                                                     FileDetails.ClientPath == null ? "" : FileDetails.ClientPath.ToString(),
                                                                                     FileDetails.HeadChange,
                                                                                     FileProcessingDetails.State.MoveFrom,
                                                                                     IsCheckedOutByOtherUser,
                                                                                     _connectionDetails));
                                    }
                                    else if (FileDetails.HeadAction == FileAction.MoveAdd)
                                    {
                                        if (FileDetails.ClientPath == null)
                                        {
                                            //log message
                                            log.Warn("ClientPath missing for DepotPath: " + FileDetails.DepotPath.ToString());
                                        }
                                        else
                                        {
                                            INTMovedFileToProcessingDictionary.Add(PartFileName,
                                                                                     new FileProcessingDetails(
                                                                                         FileDetails.DepotPath.ToString(),
                                                                                         FileDetails.ClientPath.ToString(),
                                                                                         FileDetails.HeadChange,
                                                                                         FileProcessingDetails.State.MoveTo,
                                                                                         IsCheckedOutByOtherUser,
                                                                                         _connectionDetails));

                                            // Even if the most recent perforce action is a MoveAdd, 
                                            // We need to add this to the file processing dictionary as well, so it can show up in the tool's UI
                                            INTFileProcessingDictionary.Add(PartFileName,
                                                                            new FileProcessingDetails(
                                                                                FileDetails.DepotPath.ToString(),
                                                                                FileDetails.ClientPath.ToString(),
                                                                                FileDetails.HeadChange,
                                                                                FileProcessingDetails.State.NotDeterminedYet,
                                                                                IsCheckedOutByOtherUser,
                                                                                _connectionDetails));
                                        }
                                    }
                                    else
                                    {
                                        if (FileDetails.ClientPath == null)
                                        {
                                            //log message
                                            log.Warn("ClientPath missing for DepotPath: " + FileDetails.DepotPath.ToString());
                                        }
                                        else
                                        {
                                            INTFileProcessingDictionary.Add(PartFileName,
                                                                            new FileProcessingDetails(
                                                                                FileDetails.DepotPath.ToString(),
                                                                                FileDetails.ClientPath.ToString(),
                                                                                FileDetails.HeadChange,
                                                                                FileProcessingDetails.State.NotDeterminedYet,
                                                                                IsCheckedOutByOtherUser,
                                                                                _connectionDetails));
                                        }
                                    }
                                }
                            }
                        }

                        CreateLanguageFileDictionary();
                    }
                }

                PerformDefaultActions();

                log.Info("Lookup the change list descriptions");

                BuildChangeListsDescriptionMapping();

                log.Info("Populating list  view");

                //Clear the FileListView
                FileListView.BeginUpdate();
                FileListView.Items.Clear();
                FileListView.EndUpdate();
                FileListView.Refresh();

                switch (ButtonToSwitchOnAfterRefresh)
                {
                    case DefaultOnButton.New:
                        NotInLangButtonOn = true;
                        UpdatedInIntButtonOn = false;
                        AlreadyTranslatedButtonOn = false;
                        break;
                    case DefaultOnButton.Changed:
                        NotInLangButtonOn = false;
                        UpdatedInIntButtonOn = true;
                        AlreadyTranslatedButtonOn = false;
                        break;
                    case DefaultOnButton.Translated:
                        NotInLangButtonOn = false;
                        UpdatedInIntButtonOn = false;
                        AlreadyTranslatedButtonOn = true;
                        break;
                    default:
                        NotInLangButtonOn = false;
                        UpdatedInIntButtonOn = false;
                        AlreadyTranslatedButtonOn = false;
                        break;
                }

                PopulateListWithNotInLang();
                PopulateListWithUpdatedInInt();
                PopulateListWithAlreadyTranslated();

                log.Info("Refresh or loading tasks completed");
            }
            catch (Exception ex)
            {
                log.Error("Error in refresh:");
                log.Error(ex.Message);
            }
            finally
            {
                Cursor.Current = Cursors.Default;
            }
        }

        private void BuildChangeListsDescriptionMapping()
        {
            //Build filespec list of all files to get changelists for
            List<FileSpec> ListOfFiles = new List<FileSpec>();

            foreach (var LangFile in LangFileToCreateProcessingList)
            {
                ListOfFiles.Add(new FileSpec(new DepotPath(LangFile.DepotFile), null, null, VersionSpec.Head));
            }
            foreach (var LangFile in LangFileToUpdateProcessingList)
            {
                ListOfFiles.Add(new FileSpec(new DepotPath(LangFile.DepotFile), null, null, VersionSpec.Head));
            }
            foreach (var LangFile in LangFileTranslatedProcessingList)
            {
                ListOfFiles.Add(new FileSpec(new DepotPath(LangFile.DepotFile), null, null, VersionSpec.Head));
            }

            if (CheckConnect())
            {
                Options options = new Options();

                //Extended descriptions
                options.Add("-l", null);

                IList<Changelist> Changelists = P4Repository.GetChangelists(options, ListOfFiles.ToArray());

                foreach (Changelist ChangelistDetails in Changelists)
                {
                    if (!ChangelistIdDescriptionMapping.ContainsKey(ChangelistDetails.Id))
                    {
                        ChangelistIdDescriptionMapping.Add(ChangelistDetails.Id, new ChangelistDescription(ChangelistDetails.Description));
                    }
                }
            }
        }

        /// <summary>
        /// Perform actions which can be automated, such as moving and deleting files.
        /// </summary>
        private void PerformDefaultActions()
        {
            if (CheckConnect())
            {
                using (Connection P4Connection = P4Repository.Connection)
                {
                    AddDeleteFilesToDefaultChangeList(P4Connection);
                    AddMoveFilesToDefaultChangeList(P4Connection);

                    //if there are files to move or delete submit them otherwise delete the changelist
                    try
                    {
                        Options SubmitOptions = new Options();

                        //Add a description
                        SubmitOptions.Add("-d", String.Format(Settings.Default.MoveDeleteChangelistDescription, _connectionDetails.UserSavedStatePreferredLanguage));

                        SubmitResults DeleteMoveSubmitResults = P4Connection.Client.SubmitFiles(SubmitOptions, null);

                        log.Info(String.Format("Delete and move files submitted in CL# {0}, Files affected:", DeleteMoveSubmitResults.ChangeIdAfterSubmit));
                        foreach (var SubmittedFile in DeleteMoveSubmitResults.Files)
                        {
                            log.Info(String.Format("\t{0}:\t{1}({2})", SubmittedFile.Action, SubmittedFile.File.DepotPath, SubmittedFile.File.Version));
                        }
                    }
                    catch (P4Exception ex)
                    {
                        //Swallow No files to submit error
                        if (ex.ErrorCode != 806427698)
                        {
                            log.Error("Error in submitting move delete changelist:");
                            log.Error(ex.Message);
                        }
                    }
                }
            }
        }

        /// <summary>
        /// Adds the files to be deleted to the default changelist
        /// </summary>
        private void AddDeleteFilesToDefaultChangeList(Connection P4Connection)
        {
            Cursor.Current = Cursors.WaitCursor;

            IList<FileSpec> FilesToDelete = new List<FileSpec>();

            //Delete local files and add to p4 as delete
            foreach (var LangFileToDelete in LangFileToDeleteProcessingList)
            {
                FileInfo ClientFileInfo = new System.IO.FileInfo(LangFileToDelete.ClientFile);
                FilesToDelete.Add(new FileSpec(null, new ClientPath(LangFileToDelete.ClientFile), null, null));
            }
            if (FilesToDelete.Count() > 0)
            {
                try
                {
                    P4Connection.Client.DeleteFiles(FilesToDelete, null);
                }
                catch (P4CommandTimeOutException Error)
                {
                    log.Error(Error.Message);
                    //do not continue
                    return;
                }
            }

            LangFileToDeleteProcessingList.Clear();
        }

        /// <summary>
        /// Add the move files to the default change list
        /// </summary>
        private void AddMoveFilesToDefaultChangeList(Connection P4Connection)
        {
            foreach (var LangFileMoveDetails in LangFileToMoveProcessingList)
            {
                //Check that the Move to file does not already exist.
                if (
                    LangFileProcessingDictionary.ContainsKey(
                        ParseLangFileName(_connectionDetails.UserSavedStatePreferredLanguage).Match(LangFileMoveDetails.ToDepotFile).Groups[
                            "FileName"].Value.ToUpper()))
                {
                    log.Error(
                        String.Format(
                            "Unable to move a file, destination already exists, manual fix needed.\nFrom:{0}\nTo:{1}",
                            LangFileMoveDetails.FromDepotFile, LangFileMoveDetails.ToDepotFile));
                }
                else
                {
                    FileSpec[] CurrentClientFile = new FileSpec[1];
                    CurrentClientFile[0] = new FileSpec(null, new ClientPath(LangFileMoveDetails.FromClientFile),
                                                        null, null);

                    try
                    {
                        P4Connection.Client.EditFiles(CurrentClientFile, null);
                        P4Connection.Client.MoveFiles(CurrentClientFile[0],
                                                        new FileSpec(null,
                                                                    new ClientPath(LangFileMoveDetails.ToClientFile),
                                                                    null, null),
                                                        null);
                    }
                    catch (P4CommandTimeOutException Error)
                    {
                        log.Error(Error.Message);
                        //do not continue
                        return;
                    }
                }
            }

            LangFileToMoveProcessingList.Clear();
        }


        /// <summary>
        /// Generate an unique tag which will allow searching the FileListView
        /// </summary>
        /// <param name="FileDetails"></param>
        /// <returns></returns>
        private string GenerateTag(FileProcessingDetails FileDetails)
        {
            return FileDetails.ChangeList.ToString() + FileDetails.ClientFile;
        }

        /// <summary>
        /// Remove a row from the FileListView
        /// </summary>
        /// <param name="ListTag"></param>
        /// <returns></returns>
        private void RemoveRowFromFileListView(FileProcessingDetails FileDetails)
        {
            string TagToFind = GenerateTag(FileDetails);

            for (int i = 0; i < FileListView.Items.Count; ++i)
            {
                if (FileListView.Items[i].Tag.Equals(TagToFind))
                {
                    FileListView.Items.RemoveAt(i);
                    break;
                }
            }
        }

        /// <summary>
        /// returns the changelist from a list view row
        /// </summary>
        /// <param name="RowOfData"></param>
        /// <returns></returns>
        private string GetChangelist(ListViewItem RowOfData)
        {
            return RowOfData.SubItems[0].Text;
        }

        //Item 1 = Short File Name

        //Item 2 = Description

        /// <summary>
        /// returns the client file name from a list view row
        /// </summary>
        /// <param name="RowOfData"></param>
        /// <returns></returns>
        private string GetClientFile(ListViewItem RowOfData)
        {
            return RowOfData.SubItems[3].Text;
        }

        /// <summary>
        /// returns the depot file name from a list view row
        /// </summary>
        /// <param name="RowOfData"></param>
        /// <returns></returns>
        private string GetDepotFile(ListViewItem RowOfData)
        {
            return RowOfData.SubItems[4].Text;
        }
        /// <summary>
        /// returns the the state of a list view row
        /// </summary>
        /// <param name="RowOfData"></param>
        /// <returns></returns>
        private string GetState(ListViewItem RowOfData)
        {
            return RowOfData.SubItems[5].Text;
        }

        #endregion P4 List Control

        private void VerifyAll_Click(object sender, EventArgs e)
        {
            LogAllFiles();
        }

        private void CopyLastLogToClipboard_Click(object sender, EventArgs e)
        {
            if (!File.Exists(GetLogPath()))
            {
                return;
            }

            try
            {
                Clipboard.SetText(File.ReadAllText(GetLogPath()));
            }
            catch (IOException)
            {
                // ignore io exception -- probably due to concurrent writing process
            }
        }

        private static string GetLogPath()
        {
            return Path.Combine(Path.GetTempPath(), "UnrealDocTranslator_log.txt");
        }
    }
}