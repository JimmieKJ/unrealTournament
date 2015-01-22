namespace UnrealDocTranslator
{
    partial class UnrealDocTranslatorForm
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
            this.components = new System.ComponentModel.Container();
            this.NotInLangButton = new System.Windows.Forms.Button();
            this.UpdatedInIntButton = new System.Windows.Forms.Button();
            this.ButtonPanel = new System.Windows.Forms.Panel();
            this.AlreadyTranslatedButton = new System.Windows.Forms.Button();
            this.FilterTextBox = new System.Windows.Forms.TextBox();
            this.SettingsButtonPanel = new System.Windows.Forms.Panel();
            this.P4DetailsButton = new System.Windows.Forms.Button();
            this.SyncP4Button = new System.Windows.Forms.Button();
            this.FileListView = new System.Windows.Forms.ListView();
            this.ChangelistColumnHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.FileColumnHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.DescriptionColumnHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.FileListViewContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.translateToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.diffToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.previewToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.convertToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.verifyToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.revertToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.selectAllToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.submitAllToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.openLocalSourceFolderToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.p4DetailsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.openP4FolderToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.checkoutINTFileToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.convertINTToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.HorizontalSplitContainer = new System.Windows.Forms.SplitContainer();
            this.CopyLastLogButton = new System.Windows.Forms.Button();
            this.VerifyAllButton = new System.Windows.Forms.Button();
            this.FilterPanel = new System.Windows.Forms.Panel();
            this.LogTextBox = new System.Windows.Forms.TextBox();
            this.ButtonTooltip = new System.Windows.Forms.ToolTip(this.components);
            this.ButtonPanel.SuspendLayout();
            this.SettingsButtonPanel.SuspendLayout();
            this.FileListViewContextMenu.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.HorizontalSplitContainer)).BeginInit();
            this.HorizontalSplitContainer.Panel1.SuspendLayout();
            this.HorizontalSplitContainer.Panel2.SuspendLayout();
            this.HorizontalSplitContainer.SuspendLayout();
            this.FilterPanel.SuspendLayout();
            this.SuspendLayout();
            // 
            // NotInLangButton
            // 
            this.NotInLangButton.Location = new System.Drawing.Point(6, 7);
            this.NotInLangButton.Name = "NotInLangButton";
            this.NotInLangButton.Size = new System.Drawing.Size(96, 23);
            this.NotInLangButton.TabIndex = 1;
            this.NotInLangButton.Text = "Select Language";
            this.NotInLangButton.UseVisualStyleBackColor = true;
            this.NotInLangButton.Click += new System.EventHandler(this.NotInLangButton_Click);
            // 
            // UpdatedInIntButton
            // 
            this.UpdatedInIntButton.Location = new System.Drawing.Point(108, 7);
            this.UpdatedInIntButton.Name = "UpdatedInIntButton";
            this.UpdatedInIntButton.Size = new System.Drawing.Size(96, 23);
            this.UpdatedInIntButton.TabIndex = 2;
            this.UpdatedInIntButton.Text = "Select Language";
            this.UpdatedInIntButton.UseVisualStyleBackColor = true;
            this.UpdatedInIntButton.Click += new System.EventHandler(this.UpdatedInIntButton_Click);
            // 
            // ButtonPanel
            // 
            this.ButtonPanel.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
            this.ButtonPanel.Controls.Add(this.AlreadyTranslatedButton);
            this.ButtonPanel.Controls.Add(this.UpdatedInIntButton);
            this.ButtonPanel.Controls.Add(this.NotInLangButton);
            this.ButtonPanel.Location = new System.Drawing.Point(3, 3);
            this.ButtonPanel.Name = "ButtonPanel";
            this.ButtonPanel.Size = new System.Drawing.Size(315, 39);
            this.ButtonPanel.TabIndex = 7;
            // 
            // AlreadyTranslatedButton
            // 
            this.AlreadyTranslatedButton.Location = new System.Drawing.Point(210, 7);
            this.AlreadyTranslatedButton.Name = "AlreadyTranslatedButton";
            this.AlreadyTranslatedButton.Size = new System.Drawing.Size(96, 23);
            this.AlreadyTranslatedButton.TabIndex = 3;
            this.AlreadyTranslatedButton.Text = "Select Language";
            this.AlreadyTranslatedButton.UseVisualStyleBackColor = true;
            this.AlreadyTranslatedButton.Click += new System.EventHandler(this.AlreadyTranslatedButton_Click);
            // 
            // FilterTextBox
            // 
            this.FilterTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.FilterTextBox.Location = new System.Drawing.Point(8, 9);
            this.FilterTextBox.Name = "FilterTextBox";
            this.FilterTextBox.Size = new System.Drawing.Size(494, 20);
            this.FilterTextBox.TabIndex = 4;
            this.FilterTextBox.Enter += new System.EventHandler(this.FilerTextBox_Enter);
            this.FilterTextBox.KeyDown += new System.Windows.Forms.KeyEventHandler(this.FilterTextBox_KeyDown);
            this.FilterTextBox.Leave += new System.EventHandler(this.FilerTextBox_Leave);
            // 
            // SettingsButtonPanel
            // 
            this.SettingsButtonPanel.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.SettingsButtonPanel.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
            this.SettingsButtonPanel.Controls.Add(this.P4DetailsButton);
            this.SettingsButtonPanel.Controls.Add(this.SyncP4Button);
            this.SettingsButtonPanel.Location = new System.Drawing.Point(836, 3);
            this.SettingsButtonPanel.MaximumSize = new System.Drawing.Size(71, 39);
            this.SettingsButtonPanel.MinimumSize = new System.Drawing.Size(71, 39);
            this.SettingsButtonPanel.Name = "SettingsButtonPanel";
            this.SettingsButtonPanel.Size = new System.Drawing.Size(71, 39);
            this.SettingsButtonPanel.TabIndex = 21;
            // 
            // P4DetailsButton
            // 
            this.P4DetailsButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.P4DetailsButton.Image = global::UnrealDocTranslator.Properties.Resources.Config;
            this.P4DetailsButton.Location = new System.Drawing.Point(38, 7);
            this.P4DetailsButton.Name = "P4DetailsButton";
            this.P4DetailsButton.Size = new System.Drawing.Size(23, 23);
            this.P4DetailsButton.TabIndex = 6;
            this.ButtonTooltip.SetToolTip(this.P4DetailsButton, "Change Options");
            this.P4DetailsButton.UseVisualStyleBackColor = true;
            this.P4DetailsButton.Click += new System.EventHandler(this.P4DetailsButton_Click);
            // 
            // SyncP4Button
            // 
            this.SyncP4Button.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.SyncP4Button.Image = global::UnrealDocTranslator.Properties.Resources.RefreshButton;
            this.SyncP4Button.Location = new System.Drawing.Point(8, 7);
            this.SyncP4Button.Name = "SyncP4Button";
            this.SyncP4Button.Size = new System.Drawing.Size(23, 23);
            this.SyncP4Button.TabIndex = 5;
            this.ButtonTooltip.SetToolTip(this.SyncP4Button, "Refresh list from P4 (F5)");
            this.SyncP4Button.UseVisualStyleBackColor = true;
            this.SyncP4Button.Click += new System.EventHandler(this.SyncP4Button_Click);
            // 
            // FileListView
            // 
            this.FileListView.AllowColumnReorder = true;
            this.FileListView.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.FileListView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.ChangelistColumnHeader,
            this.FileColumnHeader,
            this.DescriptionColumnHeader});
            this.FileListView.ContextMenuStrip = this.FileListViewContextMenu;
            this.FileListView.HideSelection = false;
            this.FileListView.Location = new System.Drawing.Point(3, 48);
            this.FileListView.Name = "FileListView";
            this.FileListView.Size = new System.Drawing.Size(904, 310);
            this.FileListView.Sorting = System.Windows.Forms.SortOrder.Descending;
            this.FileListView.TabIndex = 7;
            this.FileListView.UseCompatibleStateImageBehavior = false;
            this.FileListView.View = System.Windows.Forms.View.Details;
            this.FileListView.ColumnClick += new System.Windows.Forms.ColumnClickEventHandler(this.FileListView_ColumnClick);
            this.FileListView.DoubleClick += new System.EventHandler(this.FileListView_DoubleClick);
            this.FileListView.KeyDown += new System.Windows.Forms.KeyEventHandler(this.FileListView_KeyDown);
            // 
            // ChangelistColumnHeader
            // 
            this.ChangelistColumnHeader.Text = "Changelist";
            this.ChangelistColumnHeader.Width = 71;
            // 
            // FileColumnHeader
            // 
            this.FileColumnHeader.Text = "File";
            this.FileColumnHeader.Width = 300;
            // 
            // DescriptionColumnHeader
            // 
            this.DescriptionColumnHeader.Text = "Description";
            this.DescriptionColumnHeader.Width = 450;
            // 
            // FileListViewContextMenu
            // 
            this.FileListViewContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.translateToolStripMenuItem,
            this.diffToolStripMenuItem,
            this.previewToolStripMenuItem,
            this.convertToolStripMenuItem,
            this.verifyToolStripMenuItem,
            this.revertToolStripMenuItem,
            this.selectAllToolStripMenuItem,
            this.submitAllToolStripMenuItem,
            this.openLocalSourceFolderToolStripMenuItem,
            this.p4DetailsToolStripMenuItem,
            this.openP4FolderToolStripMenuItem,
            this.checkoutINTFileToolStripMenuItem,
            this.convertINTToolStripMenuItem});
            this.FileListViewContextMenu.Name = "FileListViewContextMenu";
            this.FileListViewContextMenu.Size = new System.Drawing.Size(197, 290);
            this.FileListViewContextMenu.Opening += new System.ComponentModel.CancelEventHandler(this.FileListViewContextMenu_Opening);
            // 
            // translateToolStripMenuItem
            // 
            this.translateToolStripMenuItem.Name = "translateToolStripMenuItem";
            this.translateToolStripMenuItem.Size = new System.Drawing.Size(196, 22);
            this.translateToolStripMenuItem.Text = "Translate         (Ctrl + E)";
            this.translateToolStripMenuItem.ToolTipText = "Add file to default changelist, display araxis merge of changes";
            this.translateToolStripMenuItem.Click += new System.EventHandler(this.translateToolStripMenuItem_Click);
            // 
            // diffToolStripMenuItem
            // 
            this.diffToolStripMenuItem.Name = "diffToolStripMenuItem";
            this.diffToolStripMenuItem.Size = new System.Drawing.Size(196, 22);
            this.diffToolStripMenuItem.Text = "Diff                  (Ctrl + D)";
            this.diffToolStripMenuItem.ToolTipText = "Display araxis merge without adding to a P4 changelist.";
            this.diffToolStripMenuItem.Click += new System.EventHandler(this.diffToolStripMenuItem_Click);
            // 
            // previewToolStripMenuItem
            // 
            this.previewToolStripMenuItem.Name = "previewToolStripMenuItem";
            this.previewToolStripMenuItem.Size = new System.Drawing.Size(196, 22);
            this.previewToolStripMenuItem.Text = "Preview           (Ctrl + V)";
            this.previewToolStripMenuItem.ToolTipText = "Create HTML preview of the language udn file";
            this.previewToolStripMenuItem.Click += new System.EventHandler(this.previewToolStripMenuItem_Click);
            // 
            // convertToolStripMenuItem
            // 
            this.convertToolStripMenuItem.Name = "convertToolStripMenuItem";
            this.convertToolStripMenuItem.Size = new System.Drawing.Size(196, 22);
            this.convertToolStripMenuItem.Text = "Convert          (Ctrl + C)";
            this.convertToolStripMenuItem.ToolTipText = "Generate the HTML file from the language udn file";
            this.convertToolStripMenuItem.Click += new System.EventHandler(this.convertToolStripMenuItem_Click);
            // 
            // verifyToolStripMenuItem
            // 
            this.verifyToolStripMenuItem.Name = "verifyToolStripMenuItem";
            this.verifyToolStripMenuItem.Size = new System.Drawing.Size(196, 22);
            this.verifyToolStripMenuItem.Text = "Verify              (Ctrl + F)";
            this.verifyToolStripMenuItem.ToolTipText = "Verify generation of HTML file from the language udn file";
            this.verifyToolStripMenuItem.Click += new System.EventHandler(this.verifyToolStripMenuItem_Click);
            // 
            // revertToolStripMenuItem
            // 
            this.revertToolStripMenuItem.Name = "revertToolStripMenuItem";
            this.revertToolStripMenuItem.Size = new System.Drawing.Size(196, 22);
            this.revertToolStripMenuItem.Text = "Revert";
            this.revertToolStripMenuItem.ToolTipText = "Revert all files.";
            this.revertToolStripMenuItem.Click += new System.EventHandler(this.revertToolStripMenuItem_Click);
            // 
            // selectAllToolStripMenuItem
            // 
            this.selectAllToolStripMenuItem.Name = "selectAllToolStripMenuItem";
            this.selectAllToolStripMenuItem.Size = new System.Drawing.Size(196, 22);
            this.selectAllToolStripMenuItem.Text = "Select All        (Ctrl + A)";
            this.selectAllToolStripMenuItem.ToolTipText = "Select all files in the list";
            this.selectAllToolStripMenuItem.Click += new System.EventHandler(this.selectAllToolStripMenuItem_Click);
            // 
            // submitAllToolStripMenuItem
            // 
            this.submitAllToolStripMenuItem.Name = "submitAllToolStripMenuItem";
            this.submitAllToolStripMenuItem.Size = new System.Drawing.Size(196, 22);
            this.submitAllToolStripMenuItem.Text = "Submit All      (Ctrl + S)";
            this.submitAllToolStripMenuItem.ToolTipText = "Submit the default changelist";
            this.submitAllToolStripMenuItem.Click += new System.EventHandler(this.submitAllToolStripMenuItem_Click);
            // 
            // openLocalSourceFolderToolStripMenuItem
            // 
            this.openLocalSourceFolderToolStripMenuItem.Name = "openLocalSourceFolderToolStripMenuItem";
            this.openLocalSourceFolderToolStripMenuItem.Size = new System.Drawing.Size(196, 22);
            this.openLocalSourceFolderToolStripMenuItem.Text = "Find in Explorer";
            this.openLocalSourceFolderToolStripMenuItem.ToolTipText = "Start an instance of windows explorer and navigate to the containing folder";
            this.openLocalSourceFolderToolStripMenuItem.Click += new System.EventHandler(this.openLocalSourceFolderToolStripMenuItem_Click);
            // 
            // p4DetailsToolStripMenuItem
            // 
            this.p4DetailsToolStripMenuItem.Name = "p4DetailsToolStripMenuItem";
            this.p4DetailsToolStripMenuItem.Size = new System.Drawing.Size(196, 22);
            this.p4DetailsToolStripMenuItem.Text = "P4 Details";
            this.p4DetailsToolStripMenuItem.ToolTipText = "Display the changelist details of the selected row.";
            this.p4DetailsToolStripMenuItem.Click += new System.EventHandler(this.p4DetailsToolStripMenuItem_Click);
            // 
            // openP4FolderToolStripMenuItem
            // 
            this.openP4FolderToolStripMenuItem.Name = "openP4FolderToolStripMenuItem";
            this.openP4FolderToolStripMenuItem.Size = new System.Drawing.Size(196, 22);
            this.openP4FolderToolStripMenuItem.Text = "Find in P4";
            this.openP4FolderToolStripMenuItem.ToolTipText = "Start an instance of P4 and navigate to the containing folder";
            this.openP4FolderToolStripMenuItem.Click += new System.EventHandler(this.openP4FolderToolStripMenuItem_Click);
            // 
            // checkoutINTFileToolStripMenuItem
            // 
            this.checkoutINTFileToolStripMenuItem.Name = "checkoutINTFileToolStripMenuItem";
            this.checkoutINTFileToolStripMenuItem.Size = new System.Drawing.Size(196, 22);
            this.checkoutINTFileToolStripMenuItem.Text = "Checkout INT file";
            this.checkoutINTFileToolStripMenuItem.ToolTipText = "Checkout the INT file from P4 and add to default changelist";
            this.checkoutINTFileToolStripMenuItem.Click += new System.EventHandler(this.checkoutINTFileToolStripMenuItem_Click);
            // 
            // convertINTToolStripMenuItem
            // 
            this.convertINTToolStripMenuItem.Name = "convertINTToolStripMenuItem";
            this.convertINTToolStripMenuItem.Size = new System.Drawing.Size(196, 22);
            this.convertINTToolStripMenuItem.Text = "Convert INT";
            this.convertINTToolStripMenuItem.ToolTipText = "Generate the HTML file from the INT.udn file";
            this.convertINTToolStripMenuItem.Click += new System.EventHandler(this.convertINTToolStripMenuItem_Click);
            // 
            // HorizontalSplitContainer
            // 
            this.HorizontalSplitContainer.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.HorizontalSplitContainer.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
            this.HorizontalSplitContainer.Location = new System.Drawing.Point(3, 4);
            this.HorizontalSplitContainer.Name = "HorizontalSplitContainer";
            this.HorizontalSplitContainer.Orientation = System.Windows.Forms.Orientation.Horizontal;
            // 
            // HorizontalSplitContainer.Panel1
            // 
            this.HorizontalSplitContainer.Panel1.Controls.Add(this.CopyLastLogButton);
            this.HorizontalSplitContainer.Panel1.Controls.Add(this.VerifyAllButton);
            this.HorizontalSplitContainer.Panel1.Controls.Add(this.FilterPanel);
            this.HorizontalSplitContainer.Panel1.Controls.Add(this.ButtonPanel);
            this.HorizontalSplitContainer.Panel1.Controls.Add(this.FileListView);
            this.HorizontalSplitContainer.Panel1.Controls.Add(this.SettingsButtonPanel);
            // 
            // HorizontalSplitContainer.Panel2
            // 
            this.HorizontalSplitContainer.Panel2.Controls.Add(this.LogTextBox);
            this.HorizontalSplitContainer.Size = new System.Drawing.Size(914, 519);
            this.HorizontalSplitContainer.SplitterDistance = 393;
            this.HorizontalSplitContainer.TabIndex = 24;
            // 
            // CopyLastLogButton
            // 
            this.CopyLastLogButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.CopyLastLogButton.Location = new System.Drawing.Point(749, 364);
            this.CopyLastLogButton.Name = "CopyLastLogButton";
            this.CopyLastLogButton.Size = new System.Drawing.Size(158, 23);
            this.CopyLastLogButton.TabIndex = 25;
            this.CopyLastLogButton.Text = "Copy Last Log To Clipboard";
            this.CopyLastLogButton.UseVisualStyleBackColor = true;
            this.CopyLastLogButton.Click += new System.EventHandler(this.CopyLastLogToClipboard_Click);
            // 
            // VerifyAllButton
            // 
            this.VerifyAllButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.VerifyAllButton.Location = new System.Drawing.Point(3, 364);
            this.VerifyAllButton.Name = "VerifyAllButton";
            this.VerifyAllButton.Size = new System.Drawing.Size(75, 23);
            this.VerifyAllButton.TabIndex = 24;
            this.VerifyAllButton.Text = "Verify All";
            this.VerifyAllButton.UseVisualStyleBackColor = true;
            this.VerifyAllButton.Click += new System.EventHandler(this.VerifyAll_Click);
            // 
            // FilterPanel
            // 
            this.FilterPanel.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.FilterPanel.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
            this.FilterPanel.Controls.Add(this.FilterTextBox);
            this.FilterPanel.Location = new System.Drawing.Point(320, 3);
            this.FilterPanel.Name = "FilterPanel";
            this.FilterPanel.Size = new System.Drawing.Size(513, 39);
            this.FilterPanel.TabIndex = 23;
            // 
            // LogTextBox
            // 
            this.LogTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.LogTextBox.Location = new System.Drawing.Point(3, 4);
            this.LogTextBox.Multiline = true;
            this.LogTextBox.Name = "LogTextBox";
            this.LogTextBox.ReadOnly = true;
            this.LogTextBox.ScrollBars = System.Windows.Forms.ScrollBars.Both;
            this.LogTextBox.Size = new System.Drawing.Size(904, 112);
            this.LogTextBox.TabIndex = 0;
            this.LogTextBox.WordWrap = false;
            // 
            // UnrealDocTranslatorForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(920, 527);
            this.Controls.Add(this.HorizontalSplitContainer);
            this.MinimumSize = new System.Drawing.Size(865, 565);
            this.Name = "UnrealDocTranslatorForm";
            this.Text = "UnrealDocTranslator";
            this.Shown += new System.EventHandler(this.UnrealDocTranslatorForm_Shown);
            this.ButtonPanel.ResumeLayout(false);
            this.SettingsButtonPanel.ResumeLayout(false);
            this.FileListViewContextMenu.ResumeLayout(false);
            this.HorizontalSplitContainer.Panel1.ResumeLayout(false);
            this.HorizontalSplitContainer.Panel2.ResumeLayout(false);
            this.HorizontalSplitContainer.Panel2.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.HorizontalSplitContainer)).EndInit();
            this.HorizontalSplitContainer.ResumeLayout(false);
            this.FilterPanel.ResumeLayout(false);
            this.FilterPanel.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Button NotInLangButton;
        private System.Windows.Forms.Button UpdatedInIntButton;
        private System.Windows.Forms.Panel ButtonPanel;
        private System.Windows.Forms.Button P4DetailsButton;
        private System.Windows.Forms.Button SyncP4Button;
        private System.Windows.Forms.Panel SettingsButtonPanel;
        private System.Windows.Forms.ListView FileListView;
        private System.Windows.Forms.ColumnHeader ChangelistColumnHeader;
        private System.Windows.Forms.ColumnHeader FileColumnHeader;
        private System.Windows.Forms.ColumnHeader DescriptionColumnHeader;
        private System.Windows.Forms.Button AlreadyTranslatedButton;
        private System.Windows.Forms.TextBox FilterTextBox;
        private System.Windows.Forms.SplitContainer HorizontalSplitContainer;
        private System.Windows.Forms.ContextMenuStrip FileListViewContextMenu;
        private System.Windows.Forms.ToolStripMenuItem translateToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem previewToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem convertToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem verifyToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem checkoutINTFileToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem convertINTToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem openP4FolderToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem openLocalSourceFolderToolStripMenuItem;
        private System.Windows.Forms.Panel FilterPanel;
        private System.Windows.Forms.TextBox LogTextBox;
        private System.Windows.Forms.ToolStripMenuItem diffToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem revertToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem selectAllToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem submitAllToolStripMenuItem;
        private System.Windows.Forms.ToolTip ButtonTooltip;
        private System.Windows.Forms.ToolStripMenuItem p4DetailsToolStripMenuItem;
        private System.Windows.Forms.Button CopyLastLogButton;
        private System.Windows.Forms.Button VerifyAllButton;
    }
}

