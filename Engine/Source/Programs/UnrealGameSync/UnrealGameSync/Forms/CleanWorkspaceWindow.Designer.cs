// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class CleanWorkspaceWindow
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(CleanWorkspaceWindow));
			this.TreeView = new System.Windows.Forms.TreeView();
			this.DeleteBtn = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.label1 = new System.Windows.Forms.Label();
			this.SelectAllBtn = new System.Windows.Forms.Button();
			this.SuspendLayout();
			// 
			// TreeView
			// 
			this.TreeView.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.TreeView.Location = new System.Drawing.Point(12, 38);
			this.TreeView.MinimumSize = new System.Drawing.Size(680, 400);
			this.TreeView.Name = "TreeView";
			this.TreeView.Size = new System.Drawing.Size(685, 407);
			this.TreeView.TabIndex = 0;
			this.TreeView.DrawNode += new System.Windows.Forms.DrawTreeNodeEventHandler(this.TreeView_DrawNode);
			this.TreeView.NodeMouseClick += new System.Windows.Forms.TreeNodeMouseClickEventHandler(this.TreeView_NodeMouseClick);
			// 
			// DeleteBtn
			// 
			this.DeleteBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.DeleteBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.DeleteBtn.Location = new System.Drawing.Point(610, 451);
			this.DeleteBtn.Name = "DeleteBtn";
			this.DeleteBtn.Size = new System.Drawing.Size(87, 23);
			this.DeleteBtn.TabIndex = 1;
			this.DeleteBtn.Text = "Delete Files";
			this.DeleteBtn.UseVisualStyleBackColor = true;
			this.DeleteBtn.Click += new System.EventHandler(this.DeleteBtn_Click);
			// 
			// CancelBtn
			// 
			this.CancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(517, 451);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(87, 23);
			this.CancelBtn.TabIndex = 2;
			this.CancelBtn.Text = "Cancel";
			this.CancelBtn.UseVisualStyleBackColor = true;
			// 
			// label1
			// 
			this.label1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(12, 13);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(660, 13);
			this.label1.TabIndex = 3;
			this.label1.Text = "The files below are not tracked by Perforce; check any you wish to delete. Interm" +
    "ediate files that are safe to delete are checked by default.";
			// 
			// SelectAllBtn
			// 
			this.SelectAllBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.SelectAllBtn.Location = new System.Drawing.Point(12, 451);
			this.SelectAllBtn.Name = "SelectAllBtn";
			this.SelectAllBtn.Size = new System.Drawing.Size(107, 23);
			this.SelectAllBtn.TabIndex = 4;
			this.SelectAllBtn.Text = "Select All";
			this.SelectAllBtn.UseVisualStyleBackColor = true;
			this.SelectAllBtn.Click += new System.EventHandler(this.SelectAllBtn_Click);
			// 
			// CleanWorkspaceWindow
			// 
			this.AcceptButton = this.DeleteBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size(709, 482);
			this.ControlBox = false;
			this.Controls.Add(this.SelectAllBtn);
			this.Controls.Add(this.label1);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.DeleteBtn);
			this.Controls.Add(this.TreeView);
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.MinimumSize = new System.Drawing.Size(725, 520);
			this.Name = "CleanWorkspaceWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Clean Workspace";
			this.Load += new System.EventHandler(this.CleanWorkspaceWindow_Load);
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.TreeView TreeView;
		private System.Windows.Forms.Button DeleteBtn;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Button SelectAllBtn;
	}
}