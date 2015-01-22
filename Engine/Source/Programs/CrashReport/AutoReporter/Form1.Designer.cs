// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace AutoReporter
{
    partial class Form1
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Form1));
            this.button1 = new System.Windows.Forms.Button();
            this.checkbox1 = new System.Windows.Forms.CheckBox();
            this.checkbox2 = new System.Windows.Forms.CheckBox();
            this.checkbox3 = new System.Windows.Forms.CheckBox();
            this.checkbox4 = new System.Windows.Forms.CheckBox();
            this.checkbox5 = new System.Windows.Forms.CheckBox();
            this.checkbox6 = new System.Windows.Forms.CheckBox();
            this.checkbox7 = new System.Windows.Forms.CheckBox();
            this.textBox1 = new System.Windows.Forms.TextBox();
            this.label1 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.label4 = new System.Windows.Forms.Label();
            this.textBox2 = new System.Windows.Forms.TextBox();
            this.AppErrorNotifyInfo = new System.Windows.Forms.NotifyIcon(this.components);
            this.textBox3 = new System.Windows.Forms.TextBox();
            this.ButtonPanel = new System.Windows.Forms.Panel();
            this.MainPanel = new System.Windows.Forms.Panel();
            this.ButtonPanel.SuspendLayout();
            this.MainPanel.SuspendLayout();
            this.SuspendLayout();
            // 
            // button1
            // 
            this.button1.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.button1.Location = new System.Drawing.Point(602, 3);
            this.button1.Name = "button1";
            this.button1.Size = new System.Drawing.Size(75, 23);
            this.button1.TabIndex = 2;
            this.button1.Text = "OK";
            this.button1.UseVisualStyleBackColor = true;
            this.button1.Click += new System.EventHandler(this.button1_Click);
            // 
            // checkbox1
            // 
            this.checkbox1.Checked = true;
            this.checkbox1.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkbox1.Location = new System.Drawing.Point(91, 164);
            this.checkbox1.Name = "checkbox1";
            this.checkbox1.Size = new System.Drawing.Size(85, 18);
            this.checkbox1.TabIndex = 3;
            this.checkbox1.Text = "Functions";
            this.checkbox1.Click += new System.EventHandler(this.checkbox_Click);
            // 
            // checkbox2
            // 
            this.checkbox2.Location = new System.Drawing.Point(176, 164);
            this.checkbox2.Name = "checkbox2";
            this.checkbox2.Size = new System.Drawing.Size(85, 18);
            this.checkbox2.TabIndex = 4;
            this.checkbox2.Text = "Addresses";
            this.checkbox2.Click += new System.EventHandler(this.checkbox_Click);
            // 
            // checkbox3
            // 
            this.checkbox3.Checked = true;
            this.checkbox3.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkbox3.Location = new System.Drawing.Point(267, 164);
            this.checkbox3.Name = "checkbox3";
            this.checkbox3.Size = new System.Drawing.Size(85, 18);
            this.checkbox3.TabIndex = 5;
            this.checkbox3.Text = "Files:Lines";
            this.checkbox3.Click += new System.EventHandler(this.checkbox_Click);
            // 
            // checkbox4
            // 
            this.checkbox4.Location = new System.Drawing.Point(358, 164);
            this.checkbox4.Name = "checkbox4";
            this.checkbox4.Size = new System.Drawing.Size(85, 18);
            this.checkbox4.TabIndex = 6;
            this.checkbox4.Text = "Modules";
            this.checkbox4.Click += new System.EventHandler(this.checkbox_Click);
            // 
            // checkbox5
            // 
            this.checkbox5.Checked = true;
            this.checkbox5.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkbox5.Location = new System.Drawing.Point(431, 164);
            this.checkbox5.Name = "checkbox5";
            this.checkbox5.Size = new System.Drawing.Size(85, 18);
            this.checkbox5.TabIndex = 7;
            this.checkbox5.Text = "Shortnames";
            this.checkbox5.Click += new System.EventHandler(this.checkbox_Click);
            // 
            // checkbox6
            // 
            this.checkbox6.Location = new System.Drawing.Point(516, 164);
            this.checkbox6.Name = "checkbox6";
            this.checkbox6.Size = new System.Drawing.Size(85, 18);
            this.checkbox6.TabIndex = 8;
            this.checkbox6.Text = "Unknowns";
            this.checkbox6.Click += new System.EventHandler(this.checkbox_Click);
            // 
            // checkbox7
            // 
            this.checkbox7.Location = new System.Drawing.Point(607, 164);
            this.checkbox7.Name = "checkbox7";
            this.checkbox7.Size = new System.Drawing.Size(85, 18);
            this.checkbox7.TabIndex = 9;
            this.checkbox7.Text = "Original";
            this.checkbox7.Click += new System.EventHandler(this.checkbox_Click);
            // 
            // textBox1
            // 
            this.textBox1.Location = new System.Drawing.Point(12, 86);
            this.textBox1.MaxLength = 1024;
            this.textBox1.Multiline = true;
            this.textBox1.Name = "textBox1";
            this.textBox1.Size = new System.Drawing.Size(665, 57);
            this.textBox1.TabIndex = 1;
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Font = new System.Drawing.Font("Microsoft Sans Serif", 8F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label1.Location = new System.Drawing.Point(12, 70);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(416, 13);
            this.label1.TabIndex = 13;
            this.label1.Text = "Please describe in detail what you were doing when the crash occurred:";
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label2.Location = new System.Drawing.Point(12, 166);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(65, 13);
            this.label2.TabIndex = 12;
            this.label2.Text = "CallStack:";
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label4.Location = new System.Drawing.Point(12, 11);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(236, 13);
            this.label4.TabIndex = 11;
            this.label4.Text = "Please provide a summary for this crash:";
            // 
            // textBox2
            // 
            this.textBox2.Location = new System.Drawing.Point(12, 27);
            this.textBox2.Name = "textBox2";
            this.textBox2.Size = new System.Drawing.Size(371, 20);
            this.textBox2.TabIndex = 0;
            // 
            // AppErrorNotifyInfo
            // 
            this.AppErrorNotifyInfo.BalloonTipIcon = System.Windows.Forms.ToolTipIcon.Warning;
            this.AppErrorNotifyInfo.BalloonTipTitle = "Unreal AutoReporter";
            this.AppErrorNotifyInfo.Icon = ((System.Drawing.Icon)(resources.GetObject("AppErrorNotifyInfo.Icon")));
            this.AppErrorNotifyInfo.Visible = true;
            // 
            // textBox3
            // 
            this.textBox3.Location = new System.Drawing.Point(12, 182);
            this.textBox3.Multiline = true;
            this.textBox3.Name = "textBox3";
            this.textBox3.ReadOnly = true;
            this.textBox3.ScrollBars = System.Windows.Forms.ScrollBars.Both;
            this.textBox3.Size = new System.Drawing.Size(665, 38);
            this.textBox3.TabIndex = 10;
            this.textBox3.Text = "hello worldhello worldhello worldhello worldhello worldhello world";
            this.textBox3.WordWrap = false;
            // 
            // ButtonPanel
            // 
            this.ButtonPanel.BackColor = System.Drawing.SystemColors.Control;
            this.ButtonPanel.Controls.Add(this.button1);
            this.ButtonPanel.Dock = System.Windows.Forms.DockStyle.Bottom;
            this.ButtonPanel.Location = new System.Drawing.Point(0, 226);
            this.ButtonPanel.Name = "ButtonPanel";
            this.ButtonPanel.Size = new System.Drawing.Size(689, 31);
            this.ButtonPanel.TabIndex = 14;
            // 
            // MainPanel
            // 
            this.MainPanel.Controls.Add(this.checkbox7);
            this.MainPanel.Controls.Add(this.textBox1);
            this.MainPanel.Controls.Add(this.textBox2);
            this.MainPanel.Controls.Add(this.label1);
            this.MainPanel.Controls.Add(this.label4);
            this.MainPanel.Controls.Add(this.checkbox5);
            this.MainPanel.Controls.Add(this.checkbox6);
            this.MainPanel.Controls.Add(this.checkbox4);
            this.MainPanel.Controls.Add(this.checkbox3);
            this.MainPanel.Controls.Add(this.checkbox2);
            this.MainPanel.Controls.Add(this.checkbox1);
            this.MainPanel.Controls.Add(this.textBox3);
            this.MainPanel.Controls.Add(this.label2);
            this.MainPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this.MainPanel.Location = new System.Drawing.Point(0, 0);
            this.MainPanel.Name = "MainPanel";
            this.MainPanel.Size = new System.Drawing.Size(689, 257);
            this.MainPanel.TabIndex = 15;
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(689, 257);
            this.ControlBox = false;
            this.Controls.Add(this.ButtonPanel);
            this.Controls.Add(this.MainPanel);
            this.Name = "Form1";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = ".";
            this.TopMost = true;
            this.Resize += new System.EventHandler(this.form1_Resize);
            this.ButtonPanel.ResumeLayout(false);
            this.MainPanel.ResumeLayout(false);
            this.MainPanel.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Button button1;
        private System.Windows.Forms.CheckBox checkbox1;
        private System.Windows.Forms.CheckBox checkbox2;
        private System.Windows.Forms.CheckBox checkbox3;
        private System.Windows.Forms.CheckBox checkbox4;
        private System.Windows.Forms.CheckBox checkbox5;
        private System.Windows.Forms.CheckBox checkbox6;
        private System.Windows.Forms.CheckBox checkbox7;
		private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.TextBox textBox1;
        private System.Windows.Forms.TextBox textBox2;
        private System.Windows.Forms.TextBox textBox3;
		private System.Windows.Forms.NotifyIcon AppErrorNotifyInfo;
		private System.Windows.Forms.Panel ButtonPanel;
		private System.Windows.Forms.Panel MainPanel;
    }
}

