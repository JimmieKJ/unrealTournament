// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Security;
using System.Text;
using System.Windows.Forms;
using Microsoft.Win32;
using UnrealDocTranslator.Properties;
using System.Text.RegularExpressions;

namespace UnrealDocTranslator
{
    public partial class AppSettings : Form
    {
        public string ServerName { get; set; }
        public string UserName { get; set; }
        public string WorkspaceName { get; set; }
        public string DepotPath { get; set; }
        public string UserSavedStatePreferredLanguage { get; set; }
        public string AraxisFolderName { get; set; }
        public bool UserSavedStateHideCheckedOut { get; set; }
        public bool SettingsChanged { get; set; }
        public int ShortFileNameStartPosition { get; set; }
       
        private RegistryKey _currentUserRegistryKey = Registry.CurrentUser;

        public AppSettings()
        {
            LoadFromRegistry();
            InitializeComponent();
            ServerTextBox.Text = ServerName;
            UserTextBox.Text = UserName;
            WorkspaceTextBox.Text = WorkspaceName;
            DepotPathTextBox.Text = DepotPath;
            AraxisFolderPathTextBox.Text = AraxisFolderName;
            HideIntCheckedOutCheckbox.Checked = UserSavedStateHideCheckedOut;
            ShortFileNameStartPosition = CalculateShortFileNameStartPosition();
            PopulateTranslationLanguage();
            SettingsChanged = false;
        }

        private int CalculateShortFileNameStartPosition()
        {
            int CalculatedStartPosition = DepotPath.Length - Settings.Default.DepotPathMustEndWith.Length;
            //Check does the DepotPath end in / before ... if not add 1 to the result to exclude that from the list name?
            return Regex.IsMatch(DepotPath, @"/\.\.\.") ? CalculatedStartPosition : CalculatedStartPosition + 1;
        }

        /// <summary>
        /// Sets up the combo box for current language selection from the settings file app.config
        /// </summary>
        private void PopulateTranslationLanguage()
        {
            foreach (string LanguageName in Settings.Default.TranslationLanguages.Split(','))
            {
                TranslationLanguageComboBox.Items.Add(LanguageName.Trim());
            }

            //Set to saved state preferred language
            if (!String.IsNullOrWhiteSpace(UserSavedStatePreferredLanguage))
            {
                var IndexOfUserPreferredLanguage = TranslationLanguageComboBox.Items.IndexOf(UserSavedStatePreferredLanguage);
                TranslationLanguageComboBox.SelectedItem = TranslationLanguageComboBox.Items[IndexOfUserPreferredLanguage];
            }
        }

        public bool AraxisMergeDirectoryIsValid()
        {
            //Check the araxisfolder
            bool DirectoryOk = false;
            try
            {
                var AraxisDirInfo = new System.IO.DirectoryInfo(AraxisFolderPathTextBox.Text);

                DirectoryOk = AraxisDirInfo.GetFiles("compare.exe").Count() > 0;
            }
            catch (Exception Error)
            {
                //Directory was not found issue, ignore as we are going to pick it up below
                if (
                    !(Error is ArgumentNullException || Error is SecurityException || Error is ArgumentException ||
                      Error is PathTooLongException || Error is DirectoryNotFoundException))
                {
                    MessageBox.Show(Error.Message); ;
                }
            }

            if (DirectoryOk)
            {
                AraxisFolderPathTextBox.ResetBackColor();
            }
            else
            {
                MessageBox.Show("The araxis merge directory is incorrect, please locate the folder containing compare.exe", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                AraxisFolderPathTextBox.BackColor = Color.Red;
            }
            return DirectoryOk;
        }

        public bool PreferredLanguageIsValid()
        {
            bool ReturnValue = false;
            if (String.IsNullOrWhiteSpace(TranslationLanguageComboBox.Text))
            {
                MessageBox.Show("Please select a language", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                TranslationLanguageComboBox.BackColor = Color.Red;
            }
            else
            {
                TranslationLanguageComboBox.ResetBackColor();
                ReturnValue = true;
            }
            return ReturnValue;
        }

        public bool DepotPathIsValid()
        {
            bool ReturnValue = false;
            if (String.IsNullOrWhiteSpace(DepotPathTextBox.Text) || !DepotPathTextBox.Text.StartsWith(Settings.Default.DepotPathMustStartWith, StringComparison.OrdinalIgnoreCase) || !DepotPathTextBox.Text.EndsWith(Settings.Default.DepotPathMustEndWith, StringComparison.OrdinalIgnoreCase))
            {
                MessageBox.Show(String.Format("Depot path must be specified, must start with {0}, and end with {1}",Settings.Default.DepotPathMustStartWith,Settings.Default.DepotPathMustEndWith), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                DepotPathTextBox.BackColor = Color.Red;
            }
            else
            {
                DepotPathTextBox.ResetBackColor();
                ReturnValue = true;
            }
            return ReturnValue;
        }
       
        private void OKButton_Click(object sender, EventArgs e)
        {
            ServerName = ServerTextBox.Text;
            UserName = UserTextBox.Text;
            WorkspaceName = WorkspaceTextBox.Text;
            DepotPath = DepotPathTextBox.Text;
            ShortFileNameStartPosition = CalculateShortFileNameStartPosition();

            bool CanContinue = AraxisMergeDirectoryIsValid() && PreferredLanguageIsValid() && DepotPathIsValid();
            
            AraxisFolderName = AraxisFolderPathTextBox.Text;
            if (CanContinue)
            {
                SaveToRegistry();
                Hide();
            }
            else
            {
                // Cancel close dialog event
                DialogResult = DialogResult.None;
            }
        }

        private void LoadFromRegistry()
        {
            using (RegistryKey OpenRegistry = _currentUserRegistryKey.OpenSubKey("Software\\Epic Games\\UnrealDocTranslator"))
            {
                if (OpenRegistry == null)
                {
                    ServerName = "";
                    UserName = "";
                    WorkspaceName = "";
                    DepotPath = Settings.Default.DefaultDepotPath;
                    AraxisFolderName = Settings.Default.DefaultAraxisPath;
                    UserSavedStatePreferredLanguage = "";
                    UserSavedStateHideCheckedOut = false;
                    return;
                }
                ServerName = (string)OpenRegistry.GetValue("ServerName","");
                UserName = (string)OpenRegistry.GetValue("UserName","");
                WorkspaceName = (string)OpenRegistry.GetValue("WorkspaceName","");
                DepotPath = (string)OpenRegistry.GetValue("DepotPath", Settings.Default.DefaultDepotPath);
                AraxisFolderName = (string)OpenRegistry.GetValue("AraxisFolderName",Settings.Default.DefaultAraxisPath);
                UserSavedStatePreferredLanguage = (string)OpenRegistry.GetValue("UserSavedStatePreferredLanguage","");
                UserSavedStateHideCheckedOut = (bool)String.Equals((string)OpenRegistry.GetValue("UserSavedStateHideCheckedOut", "False"), "False", StringComparison.OrdinalIgnoreCase) ? false : true;
            }
        }

        
        private void SaveToRegistry()
        {
            using (var OpenRegistry=_currentUserRegistryKey.OpenSubKey("Software",true))
            {
                using (var OpenSubRegistry = OpenRegistry.CreateSubKey("Epic Games\\UnrealDocTranslator"))
                {
                    OpenSubRegistry.SetValue("ServerName", ServerName);
                    OpenSubRegistry.SetValue("UserName", UserName);
                    OpenSubRegistry.SetValue("WorkspaceName", WorkspaceName);
                    OpenSubRegistry.SetValue("DepotPath", DepotPath);
                    OpenSubRegistry.SetValue("AraxisFolderName", AraxisFolderName);
                    OpenSubRegistry.SetValue("UserSavedStatePreferredLanguage", UserSavedStatePreferredLanguage);
                    OpenSubRegistry.SetValue("UserSavedStateHideCheckedOut", UserSavedStateHideCheckedOut);
                }
            }
        }

        internal void SaveStatePreferredLanguage(string Language)
        {
            UserSavedStatePreferredLanguage = Language;
            using (var OpenRegistry = _currentUserRegistryKey.OpenSubKey("Software", true))
            {
                using (var OpenSubRegistry = OpenRegistry.CreateSubKey("Epic Games\\UnrealDocTranslator"))
                {
                    OpenSubRegistry.SetValue("UserSavedStatePreferredLanguage", UserSavedStatePreferredLanguage);
                }
            }
        }

        internal void SaveStateHideCheckedOut(bool HideCheckedOut)
        {
            UserSavedStateHideCheckedOut = HideCheckedOut;
            using (var OpenRegistry = _currentUserRegistryKey.OpenSubKey("Software", true))
            {
                using (var OpenSubRegistry = OpenRegistry.CreateSubKey("Epic Games\\UnrealDocTranslator"))
                {
                    OpenSubRegistry.SetValue("UserSavedStateHideCheckedOut", UserSavedStateHideCheckedOut);
                }
            }
        }

        private void ChangeAraxisMergeFolderButton_Click(object sender, EventArgs e)
        {
            AraxisFolderBrowserDialog.RootFolder = System.Environment.SpecialFolder.MyComputer;
            AraxisFolderBrowserDialog.SelectedPath = AraxisFolderPathTextBox.Text;
            AraxisFolderBrowserDialog.ShowNewFolderButton = false;
            if (AraxisFolderBrowserDialog.ShowDialog() == DialogResult.OK)
            {
                AraxisFolderPathTextBox.Text = AraxisFolderBrowserDialog.SelectedPath;
            }
            AraxisMergeDirectoryIsValid();
        }

        private void TranslationLanguageComboBox_SelectedIndexChanged(object sender, EventArgs e)
        {
            //Save users choice for future runs
            SettingsChanged = true;
            UserSavedStatePreferredLanguage = TranslationLanguageComboBox.Text;
            SaveStatePreferredLanguage(UserSavedStatePreferredLanguage);
        }

        private void HideIntCheckedOutCheckbox_CheckedChanged(object sender, EventArgs e)
        {
            //Save users choice for future runs
            SettingsChanged = true;
            UserSavedStateHideCheckedOut = HideIntCheckedOutCheckbox.Checked;
            SaveStateHideCheckedOut(UserSavedStateHideCheckedOut);
        }
    }
}
