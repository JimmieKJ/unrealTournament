// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace AutoReporter
{
    partial class CrashURLDlg
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
            this.label1 = new System.Windows.Forms.Label();
            this.CrashURLClose_Button = new System.Windows.Forms.Button();
            this.CrashURL_linkLabel = new System.Windows.Forms.LinkLabel();
            this.label2 = new System.Windows.Forms.Label();
            this.SuspendLayout();
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(12, 9);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(126, 13);
            this.label1.TabIndex = 0;
            this.label1.Text = "Your crash report URL is:";
            // 
            // CrashURLClose_Button
            // 
            this.CrashURLClose_Button.Location = new System.Drawing.Point(113, 93);
            this.CrashURLClose_Button.Name = "CrashURLClose_Button";
            this.CrashURLClose_Button.Size = new System.Drawing.Size(75, 23);
            this.CrashURLClose_Button.TabIndex = 2;
            this.CrashURLClose_Button.Text = "&Close";
            this.CrashURLClose_Button.UseVisualStyleBackColor = true;
            this.CrashURLClose_Button.Click += new System.EventHandler(this.CrashURLClose_Button_Click);
            // 
            // CrashURL_linkLabel
            // 
            this.CrashURL_linkLabel.AutoSize = true;
            this.CrashURL_linkLabel.Location = new System.Drawing.Point(12, 35);
            this.CrashURL_linkLabel.Name = "CrashURL_linkLabel";
            this.CrashURL_linkLabel.Size = new System.Drawing.Size(60, 13);
            this.CrashURL_linkLabel.TabIndex = 3;
            this.CrashURL_linkLabel.TabStop = true;
            this.CrashURL_linkLabel.Text = "<NO URL>";
            this.CrashURL_linkLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.CrashURL_linkLabel_LinkClicked);
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(12, 61);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(271, 13);
            this.label2.TabIndex = 4;
            this.label2.Text = "(This is also copied into the clipboard for simple pasting.)";
            // 
            // CrashURLDlg
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(301, 128);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.CrashURL_linkLabel);
            this.Controls.Add(this.CrashURLClose_Button);
            this.Controls.Add(this.label1);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedToolWindow;
            this.Name = "CrashURLDlg";
            this.Text = "Crash Report";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Button CrashURLClose_Button;
        public System.Windows.Forms.LinkLabel CrashURL_linkLabel;
        private System.Windows.Forms.Label label2;
    }
}