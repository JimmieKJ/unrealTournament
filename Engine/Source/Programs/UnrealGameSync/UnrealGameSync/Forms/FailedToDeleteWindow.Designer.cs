// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class FailedToDeleteWindow
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
			this.FileList = new System.Windows.Forms.TextBox();
			this.OkBtn = new System.Windows.Forms.Button();
			this.label1 = new System.Windows.Forms.Label();
			this.SuspendLayout();
			// 
			// FileList
			// 
			this.FileList.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.FileList.Location = new System.Drawing.Point(13, 37);
			this.FileList.Multiline = true;
			this.FileList.Name = "FileList";
			this.FileList.ReadOnly = true;
			this.FileList.Size = new System.Drawing.Size(715, 170);
			this.FileList.TabIndex = 0;
			// 
			// OkBtn
			// 
			this.OkBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.OkBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.OkBtn.Location = new System.Drawing.Point(644, 217);
			this.OkBtn.Name = "OkBtn";
			this.OkBtn.Size = new System.Drawing.Size(84, 23);
			this.OkBtn.TabIndex = 1;
			this.OkBtn.Text = "Ok";
			this.OkBtn.UseVisualStyleBackColor = true;
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(13, 13);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(194, 13);
			this.label1.TabIndex = 2;
			this.label1.Text = "The following files could not be deleted:";
			// 
			// FailedToDeleteWindow
			// 
			this.AcceptButton = this.OkBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.OkBtn;
			this.ClientSize = new System.Drawing.Size(740, 252);
			this.ControlBox = false;
			this.Controls.Add(this.label1);
			this.Controls.Add(this.OkBtn);
			this.Controls.Add(this.FileList);
			this.Name = "FailedToDeleteWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Failed to Delete Files";
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.Button OkBtn;
		private System.Windows.Forms.Label label1;
		public System.Windows.Forms.TextBox FileList;
	}
}