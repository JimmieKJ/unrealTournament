// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class LeaveCommentWindow
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
			this.OkBtn = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.CommentTextBox = new System.Windows.Forms.TextBox();
			this.SuspendLayout();
			// 
			// OkBtn
			// 
			this.OkBtn.DialogResult = System.Windows.Forms.DialogResult.OK;
			this.OkBtn.Location = new System.Drawing.Point(422, 39);
			this.OkBtn.Name = "OkBtn";
			this.OkBtn.Size = new System.Drawing.Size(75, 23);
			this.OkBtn.TabIndex = 3;
			this.OkBtn.Text = "Ok";
			this.OkBtn.UseVisualStyleBackColor = true;
			// 
			// CancelBtn
			// 
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(341, 39);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(75, 23);
			this.CancelBtn.TabIndex = 2;
			this.CancelBtn.Text = "Cancel";
			this.CancelBtn.UseVisualStyleBackColor = true;
			// 
			// CommentTextBox
			// 
			this.CommentTextBox.Location = new System.Drawing.Point(13, 13);
			this.CommentTextBox.Name = "CommentTextBox";
			this.CommentTextBox.Size = new System.Drawing.Size(484, 20);
			this.CommentTextBox.TabIndex = 1;
			// 
			// LeaveCommentWindow
			// 
			this.AcceptButton = this.OkBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size(510, 73);
			this.Controls.Add(this.CommentTextBox);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.OkBtn);
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Name = "LeaveCommentWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Comment";
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.Button OkBtn;
		private System.Windows.Forms.Button CancelBtn;
		public System.Windows.Forms.TextBox CommentTextBox;
	}
}