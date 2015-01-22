// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealDocTranslator
{
    partial class AppSettings
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.ServerLabel = new System.Windows.Forms.Label();
            this.ServerTextBox = new System.Windows.Forms.TextBox();
            this.UserTextBox = new System.Windows.Forms.TextBox();
            this.UserLabel = new System.Windows.Forms.Label();
            this.WorkspaceTextBox = new System.Windows.Forms.TextBox();
            this.WorkspaceLabel = new System.Windows.Forms.Label();
            this.OKButton = new System.Windows.Forms.Button();
            this.P4Settings = new System.Windows.Forms.GroupBox();
            this.AraxisMergeDirectory = new System.Windows.Forms.GroupBox();
            this.ChangeAraxisMergeFolderButton = new System.Windows.Forms.Button();
            this.AraxisFolderPathTextBox = new System.Windows.Forms.TextBox();
            this.AraxisFolderBrowserDialog = new System.Windows.Forms.FolderBrowserDialog();
            this.TranslationLanguageLabel = new System.Windows.Forms.Label();
            this.TranslationLanguageComboBox = new System.Windows.Forms.ComboBox();
            this.ApplicationSettings = new System.Windows.Forms.GroupBox();
            this.HideIntCheckedOutCheckbox = new System.Windows.Forms.CheckBox();
            this.DepotPathTextBox = new System.Windows.Forms.TextBox();
            this.DepotPathLabel = new System.Windows.Forms.Label();
            this.P4Settings.SuspendLayout();
            this.AraxisMergeDirectory.SuspendLayout();
            this.ApplicationSettings.SuspendLayout();
            this.SuspendLayout();
            // 
            // ServerLabel
            // 
            this.ServerLabel.AutoSize = true;
            this.ServerLabel.Location = new System.Drawing.Point(15, 22);
            this.ServerLabel.Name = "ServerLabel";
            this.ServerLabel.Size = new System.Drawing.Size(41, 13);
            this.ServerLabel.TabIndex = 0;
            this.ServerLabel.Text = "Server:";
            // 
            // ServerTextBox
            // 
            this.ServerTextBox.Location = new System.Drawing.Point(86, 19);
            this.ServerTextBox.Name = "ServerTextBox";
            this.ServerTextBox.Size = new System.Drawing.Size(221, 20);
            this.ServerTextBox.TabIndex = 1;
            // 
            // UserTextBox
            // 
            this.UserTextBox.Location = new System.Drawing.Point(86, 45);
            this.UserTextBox.Name = "UserTextBox";
            this.UserTextBox.Size = new System.Drawing.Size(221, 20);
            this.UserTextBox.TabIndex = 3;
            // 
            // UserLabel
            // 
            this.UserLabel.AutoSize = true;
            this.UserLabel.Location = new System.Drawing.Point(15, 48);
            this.UserLabel.Name = "UserLabel";
            this.UserLabel.Size = new System.Drawing.Size(32, 13);
            this.UserLabel.TabIndex = 2;
            this.UserLabel.Text = "User:";
            // 
            // WorkspaceTextBox
            // 
            this.WorkspaceTextBox.Location = new System.Drawing.Point(86, 71);
            this.WorkspaceTextBox.Name = "WorkspaceTextBox";
            this.WorkspaceTextBox.Size = new System.Drawing.Size(221, 20);
            this.WorkspaceTextBox.TabIndex = 5;
            // 
            // WorkspaceLabel
            // 
            this.WorkspaceLabel.AutoSize = true;
            this.WorkspaceLabel.Location = new System.Drawing.Point(15, 74);
            this.WorkspaceLabel.Name = "WorkspaceLabel";
            this.WorkspaceLabel.Size = new System.Drawing.Size(65, 13);
            this.WorkspaceLabel.TabIndex = 4;
            this.WorkspaceLabel.Text = "Workspace:";
            // 
            // OKButton
            // 
            this.OKButton.DialogResult = System.Windows.Forms.DialogResult.OK;
            this.OKButton.Location = new System.Drawing.Point(246, 278);
            this.OKButton.Name = "OKButton";
            this.OKButton.Size = new System.Drawing.Size(75, 23);
            this.OKButton.TabIndex = 6;
            this.OKButton.Text = "OK";
            this.OKButton.UseVisualStyleBackColor = true;
            this.OKButton.Click += new System.EventHandler(this.OKButton_Click);
            // 
            // P4Settings
            // 
            this.P4Settings.Controls.Add(this.DepotPathTextBox);
            this.P4Settings.Controls.Add(this.DepotPathLabel);
            this.P4Settings.Controls.Add(this.UserTextBox);
            this.P4Settings.Controls.Add(this.ServerLabel);
            this.P4Settings.Controls.Add(this.WorkspaceTextBox);
            this.P4Settings.Controls.Add(this.ServerTextBox);
            this.P4Settings.Controls.Add(this.WorkspaceLabel);
            this.P4Settings.Controls.Add(this.UserLabel);
            this.P4Settings.Location = new System.Drawing.Point(7, 7);
            this.P4Settings.Name = "P4Settings";
            this.P4Settings.Size = new System.Drawing.Size(314, 127);
            this.P4Settings.TabIndex = 7;
            this.P4Settings.TabStop = false;
            this.P4Settings.Text = "P4 Settings";
            // 
            // AraxisMergeDirectory
            // 
            this.AraxisMergeDirectory.Controls.Add(this.ChangeAraxisMergeFolderButton);
            this.AraxisMergeDirectory.Controls.Add(this.AraxisFolderPathTextBox);
            this.AraxisMergeDirectory.Location = new System.Drawing.Point(7, 139);
            this.AraxisMergeDirectory.Name = "AraxisMergeDirectory";
            this.AraxisMergeDirectory.Size = new System.Drawing.Size(314, 50);
            this.AraxisMergeDirectory.TabIndex = 8;
            this.AraxisMergeDirectory.TabStop = false;
            this.AraxisMergeDirectory.Text = "Araxis Merge Directory";
            // 
            // ChangeAraxisMergeFolderButton
            // 
            this.ChangeAraxisMergeFolderButton.Location = new System.Drawing.Point(254, 19);
            this.ChangeAraxisMergeFolderButton.Name = "ChangeAraxisMergeFolderButton";
            this.ChangeAraxisMergeFolderButton.Size = new System.Drawing.Size(53, 23);
            this.ChangeAraxisMergeFolderButton.TabIndex = 9;
            this.ChangeAraxisMergeFolderButton.Text = "Change";
            this.ChangeAraxisMergeFolderButton.UseVisualStyleBackColor = true;
            this.ChangeAraxisMergeFolderButton.Click += new System.EventHandler(this.ChangeAraxisMergeFolderButton_Click);
            // 
            // AraxisFolderPathTextBox
            // 
            this.AraxisFolderPathTextBox.Location = new System.Drawing.Point(6, 20);
            this.AraxisFolderPathTextBox.Name = "AraxisFolderPathTextBox";
            this.AraxisFolderPathTextBox.ReadOnly = true;
            this.AraxisFolderPathTextBox.Size = new System.Drawing.Size(246, 20);
            this.AraxisFolderPathTextBox.TabIndex = 6;
            // 
            // TranslationLanguageLabel
            // 
            this.TranslationLanguageLabel.AutoSize = true;
            this.TranslationLanguageLabel.Location = new System.Drawing.Point(15, 25);
            this.TranslationLanguageLabel.Name = "TranslationLanguageLabel";
            this.TranslationLanguageLabel.Size = new System.Drawing.Size(113, 13);
            this.TranslationLanguageLabel.TabIndex = 1;
            this.TranslationLanguageLabel.Text = "Translation Language:";
            // 
            // TranslationLanguageComboBox
            // 
            this.TranslationLanguageComboBox.FormattingEnabled = true;
            this.TranslationLanguageComboBox.Location = new System.Drawing.Point(134, 22);
            this.TranslationLanguageComboBox.Name = "TranslationLanguageComboBox";
            this.TranslationLanguageComboBox.Size = new System.Drawing.Size(173, 21);
            this.TranslationLanguageComboBox.TabIndex = 0;
            this.TranslationLanguageComboBox.SelectedIndexChanged += new System.EventHandler(this.TranslationLanguageComboBox_SelectedIndexChanged);
            // 
            // ApplicationSettings
            // 
            this.ApplicationSettings.Controls.Add(this.HideIntCheckedOutCheckbox);
            this.ApplicationSettings.Controls.Add(this.TranslationLanguageComboBox);
            this.ApplicationSettings.Controls.Add(this.TranslationLanguageLabel);
            this.ApplicationSettings.Location = new System.Drawing.Point(7, 196);
            this.ApplicationSettings.Name = "ApplicationSettings";
            this.ApplicationSettings.Size = new System.Drawing.Size(314, 71);
            this.ApplicationSettings.TabIndex = 10;
            this.ApplicationSettings.TabStop = false;
            this.ApplicationSettings.Text = "Application Settings";
            // 
            // HideIntCheckedOutCheckbox
            // 
            this.HideIntCheckedOutCheckbox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.HideIntCheckedOutCheckbox.AutoSize = true;
            this.HideIntCheckedOutCheckbox.CheckAlign = System.Drawing.ContentAlignment.MiddleRight;
            this.HideIntCheckedOutCheckbox.Location = new System.Drawing.Point(15, 48);
            this.HideIntCheckedOutCheckbox.Name = "HideIntCheckedOutCheckbox";
            this.HideIntCheckedOutCheckbox.Size = new System.Drawing.Size(133, 17);
            this.HideIntCheckedOutCheckbox.TabIndex = 9;
            this.HideIntCheckedOutCheckbox.Text = "Hide Checked-out INT";
            this.HideIntCheckedOutCheckbox.UseVisualStyleBackColor = true;
            this.HideIntCheckedOutCheckbox.CheckedChanged += new System.EventHandler(this.HideIntCheckedOutCheckbox_CheckedChanged);
            // 
            // DepotPathTextBox
            // 
            this.DepotPathTextBox.Location = new System.Drawing.Point(86, 97);
            this.DepotPathTextBox.Name = "DepotPathTextBox";
            this.DepotPathTextBox.Size = new System.Drawing.Size(221, 20);
            this.DepotPathTextBox.TabIndex = 7;
            // 
            // DepotPathLabel
            // 
            this.DepotPathLabel.AutoSize = true;
            this.DepotPathLabel.Location = new System.Drawing.Point(15, 100);
            this.DepotPathLabel.Name = "DepotPathLabel";
            this.DepotPathLabel.Size = new System.Drawing.Size(64, 13);
            this.DepotPathLabel.TabIndex = 6;
            this.DepotPathLabel.Text = "Depot Path:";
            // 
            // AppSettings
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(331, 308);
            this.Controls.Add(this.ApplicationSettings);
            this.Controls.Add(this.AraxisMergeDirectory);
            this.Controls.Add(this.P4Settings);
            this.Controls.Add(this.OKButton);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.Fixed3D;
            this.Name = "AppSettings";
            this.Text = "Settings";
            this.P4Settings.ResumeLayout(false);
            this.P4Settings.PerformLayout();
            this.AraxisMergeDirectory.ResumeLayout(false);
            this.AraxisMergeDirectory.PerformLayout();
            this.ApplicationSettings.ResumeLayout(false);
            this.ApplicationSettings.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Label ServerLabel;
        private System.Windows.Forms.TextBox ServerTextBox;
        private System.Windows.Forms.TextBox UserTextBox;
        private System.Windows.Forms.Label UserLabel;
        private System.Windows.Forms.TextBox WorkspaceTextBox;
        private System.Windows.Forms.Label WorkspaceLabel;
        private System.Windows.Forms.Button OKButton;
        private System.Windows.Forms.GroupBox P4Settings;
        private System.Windows.Forms.GroupBox AraxisMergeDirectory;
        private System.Windows.Forms.Button ChangeAraxisMergeFolderButton;
        private System.Windows.Forms.TextBox AraxisFolderPathTextBox;
        private System.Windows.Forms.FolderBrowserDialog AraxisFolderBrowserDialog;
        private System.Windows.Forms.Label TranslationLanguageLabel;
        private System.Windows.Forms.ComboBox TranslationLanguageComboBox;
        private System.Windows.Forms.GroupBox ApplicationSettings;
        private System.Windows.Forms.CheckBox HideIntCheckedOutCheckbox;
        private System.Windows.Forms.TextBox DepotPathTextBox;
        private System.Windows.Forms.Label DepotPathLabel;
    }
}