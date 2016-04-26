// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class BuildStepWindow
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(BuildStepWindow));
			this.label1 = new System.Windows.Forms.Label();
			this.DescriptionTextBox = new System.Windows.Forms.TextBox();
			this.label2 = new System.Windows.Forms.Label();
			this.DurationTextBox = new System.Windows.Forms.TextBox();
			this.groupBox1 = new System.Windows.Forms.GroupBox();
			this.label9 = new System.Windows.Forms.Label();
			this.label8 = new System.Windows.Forms.Label();
			this.CompileTargetComboBox = new System.Windows.Forms.ComboBox();
			this.CompileArgumentsTextBox = new System.Windows.Forms.TextBox();
			this.CompileConfigComboBox = new System.Windows.Forms.ComboBox();
			this.CompilePlatformComboBox = new System.Windows.Forms.ComboBox();
			this.CompileRadioButton = new System.Windows.Forms.RadioButton();
			this.label7 = new System.Windows.Forms.Label();
			this.label6 = new System.Windows.Forms.Label();
			this.label5 = new System.Windows.Forms.Label();
			this.OtherUseLogWindowCheckBox = new System.Windows.Forms.CheckBox();
			this.CookFileNameButton = new System.Windows.Forms.Button();
			this.OtherArgumentsTextBox = new System.Windows.Forms.TextBox();
			this.OtherFileNameButton = new System.Windows.Forms.Button();
			this.OtherFileNameTextBox = new System.Windows.Forms.TextBox();
			this.CookFileNameTextBox = new System.Windows.Forms.TextBox();
			this.OtherRadioButton = new System.Windows.Forms.RadioButton();
			this.CookRadioButton = new System.Windows.Forms.RadioButton();
			this.StatusTextTextBox = new System.Windows.Forms.TextBox();
			this.label4 = new System.Windows.Forms.Label();
			this.OkButton = new System.Windows.Forms.Button();
			this.NewCancelButton = new System.Windows.Forms.Button();
			this.groupBox3 = new System.Windows.Forms.GroupBox();
			this.groupBox4 = new System.Windows.Forms.GroupBox();
			this.VariablesButton = new System.Windows.Forms.Button();
			this.groupBox1.SuspendLayout();
			this.groupBox3.SuspendLayout();
			this.groupBox4.SuspendLayout();
			this.SuspendLayout();
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(15, 18);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(63, 13);
			this.label1.TabIndex = 0;
			this.label1.Text = "Description:";
			// 
			// DescriptionTextBox
			// 
			this.DescriptionTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.DescriptionTextBox.Location = new System.Drawing.Point(84, 15);
			this.DescriptionTextBox.Name = "DescriptionTextBox";
			this.DescriptionTextBox.Size = new System.Drawing.Size(548, 20);
			this.DescriptionTextBox.TabIndex = 0;
			// 
			// label2
			// 
			this.label2.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.label2.AutoSize = true;
			this.label2.Location = new System.Drawing.Point(379, 44);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(156, 13);
			this.label2.TabIndex = 2;
			this.label2.Text = "Approximate Duration (minutes):";
			// 
			// DurationTextBox
			// 
			this.DurationTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.DurationTextBox.Location = new System.Drawing.Point(541, 41);
			this.DurationTextBox.Name = "DurationTextBox";
			this.DurationTextBox.Size = new System.Drawing.Size(91, 20);
			this.DurationTextBox.TabIndex = 2;
			// 
			// groupBox1
			// 
			this.groupBox1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox1.Controls.Add(this.label9);
			this.groupBox1.Controls.Add(this.label8);
			this.groupBox1.Controls.Add(this.CompileTargetComboBox);
			this.groupBox1.Controls.Add(this.CompileArgumentsTextBox);
			this.groupBox1.Controls.Add(this.CompileConfigComboBox);
			this.groupBox1.Controls.Add(this.CompilePlatformComboBox);
			this.groupBox1.Location = new System.Drawing.Point(12, 76);
			this.groupBox1.Name = "groupBox1";
			this.groupBox1.Size = new System.Drawing.Size(625, 95);
			this.groupBox1.TabIndex = 4;
			this.groupBox1.TabStop = false;
			// 
			// label9
			// 
			this.label9.AutoSize = true;
			this.label9.Location = new System.Drawing.Point(24, 57);
			this.label9.Name = "label9";
			this.label9.Size = new System.Drawing.Size(60, 13);
			this.label9.TabIndex = 38;
			this.label9.Text = "Arguments:";
			// 
			// label8
			// 
			this.label8.AutoSize = true;
			this.label8.Location = new System.Drawing.Point(24, 33);
			this.label8.Name = "label8";
			this.label8.Size = new System.Drawing.Size(41, 13);
			this.label8.TabIndex = 37;
			this.label8.Text = "Target:";
			// 
			// CompileTargetComboBox
			// 
			this.CompileTargetComboBox.FormattingEnabled = true;
			this.CompileTargetComboBox.Location = new System.Drawing.Point(92, 30);
			this.CompileTargetComboBox.Name = "CompileTargetComboBox";
			this.CompileTargetComboBox.Size = new System.Drawing.Size(163, 21);
			this.CompileTargetComboBox.TabIndex = 0;
			// 
			// CompileArgumentsTextBox
			// 
			this.CompileArgumentsTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.CompileArgumentsTextBox.Location = new System.Drawing.Point(92, 54);
			this.CompileArgumentsTextBox.Name = "CompileArgumentsTextBox";
			this.CompileArgumentsTextBox.Size = new System.Drawing.Size(502, 20);
			this.CompileArgumentsTextBox.TabIndex = 3;
			// 
			// CompileConfigComboBox
			// 
			this.CompileConfigComboBox.FormattingEnabled = true;
			this.CompileConfigComboBox.Items.AddRange(new object[] {
            "Debug",
            "DebugGame",
            "Development",
            "Test",
            "Shipping"});
			this.CompileConfigComboBox.Location = new System.Drawing.Point(430, 30);
			this.CompileConfigComboBox.Name = "CompileConfigComboBox";
			this.CompileConfigComboBox.Size = new System.Drawing.Size(163, 21);
			this.CompileConfigComboBox.TabIndex = 2;
			this.CompileConfigComboBox.Text = "Debug";
			// 
			// CompilePlatformComboBox
			// 
			this.CompilePlatformComboBox.FormattingEnabled = true;
			this.CompilePlatformComboBox.Items.AddRange(new object[] {
            "Win32",
            "Win64",
            "Linux",
            "Android",
            "iOS",
            "Mac",
            "HTML5",
            "PS4",
            "XboxOne"});
			this.CompilePlatformComboBox.Location = new System.Drawing.Point(261, 30);
			this.CompilePlatformComboBox.Name = "CompilePlatformComboBox";
			this.CompilePlatformComboBox.Size = new System.Drawing.Size(163, 21);
			this.CompilePlatformComboBox.TabIndex = 1;
			this.CompilePlatformComboBox.Text = "Win64";
			// 
			// CompileRadioButton
			// 
			this.CompileRadioButton.AutoSize = true;
			this.CompileRadioButton.Location = new System.Drawing.Point(22, 74);
			this.CompileRadioButton.Name = "CompileRadioButton";
			this.CompileRadioButton.Size = new System.Drawing.Size(62, 17);
			this.CompileRadioButton.TabIndex = 3;
			this.CompileRadioButton.TabStop = true;
			this.CompileRadioButton.Text = "Compile";
			this.CompileRadioButton.UseVisualStyleBackColor = true;
			this.CompileRadioButton.CheckedChanged += new System.EventHandler(this.CompileRadioButton_CheckedChanged);
			// 
			// label7
			// 
			this.label7.AutoSize = true;
			this.label7.Location = new System.Drawing.Point(24, 36);
			this.label7.Name = "label7";
			this.label7.Size = new System.Drawing.Size(39, 13);
			this.label7.TabIndex = 36;
			this.label7.Text = "Profile:";
			// 
			// label6
			// 
			this.label6.AutoSize = true;
			this.label6.Location = new System.Drawing.Point(24, 34);
			this.label6.Name = "label6";
			this.label6.Size = new System.Drawing.Size(63, 13);
			this.label6.TabIndex = 35;
			this.label6.Text = "Executable:";
			// 
			// label5
			// 
			this.label5.AutoSize = true;
			this.label5.Location = new System.Drawing.Point(24, 58);
			this.label5.Name = "label5";
			this.label5.Size = new System.Drawing.Size(60, 13);
			this.label5.TabIndex = 34;
			this.label5.Text = "Arguments:";
			// 
			// OtherUseLogWindowCheckBox
			// 
			this.OtherUseLogWindowCheckBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.OtherUseLogWindowCheckBox.AutoSize = true;
			this.OtherUseLogWindowCheckBox.Checked = true;
			this.OtherUseLogWindowCheckBox.CheckState = System.Windows.Forms.CheckState.Checked;
			this.OtherUseLogWindowCheckBox.Location = new System.Drawing.Point(486, 57);
			this.OtherUseLogWindowCheckBox.Name = "OtherUseLogWindowCheckBox";
			this.OtherUseLogWindowCheckBox.Size = new System.Drawing.Size(108, 17);
			this.OtherUseLogWindowCheckBox.TabIndex = 3;
			this.OtherUseLogWindowCheckBox.Text = "Use Log Window";
			this.OtherUseLogWindowCheckBox.UseVisualStyleBackColor = true;
			// 
			// CookFileNameButton
			// 
			this.CookFileNameButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.CookFileNameButton.Location = new System.Drawing.Point(568, 32);
			this.CookFileNameButton.Name = "CookFileNameButton";
			this.CookFileNameButton.Size = new System.Drawing.Size(26, 20);
			this.CookFileNameButton.TabIndex = 1;
			this.CookFileNameButton.Text = "...";
			this.CookFileNameButton.UseVisualStyleBackColor = true;
			this.CookFileNameButton.Click += new System.EventHandler(this.CookFileNameButton_Click);
			// 
			// OtherArgumentsTextBox
			// 
			this.OtherArgumentsTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.OtherArgumentsTextBox.Location = new System.Drawing.Point(92, 55);
			this.OtherArgumentsTextBox.Name = "OtherArgumentsTextBox";
			this.OtherArgumentsTextBox.Size = new System.Drawing.Size(388, 20);
			this.OtherArgumentsTextBox.TabIndex = 2;
			// 
			// OtherFileNameButton
			// 
			this.OtherFileNameButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.OtherFileNameButton.Location = new System.Drawing.Point(568, 31);
			this.OtherFileNameButton.Name = "OtherFileNameButton";
			this.OtherFileNameButton.Size = new System.Drawing.Size(26, 20);
			this.OtherFileNameButton.TabIndex = 1;
			this.OtherFileNameButton.Text = "...";
			this.OtherFileNameButton.UseVisualStyleBackColor = true;
			this.OtherFileNameButton.Click += new System.EventHandler(this.OtherFileNameButton_Click);
			// 
			// OtherFileNameTextBox
			// 
			this.OtherFileNameTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.OtherFileNameTextBox.Location = new System.Drawing.Point(92, 31);
			this.OtherFileNameTextBox.Name = "OtherFileNameTextBox";
			this.OtherFileNameTextBox.Size = new System.Drawing.Size(470, 20);
			this.OtherFileNameTextBox.TabIndex = 0;
			// 
			// CookFileNameTextBox
			// 
			this.CookFileNameTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.CookFileNameTextBox.Location = new System.Drawing.Point(92, 33);
			this.CookFileNameTextBox.Name = "CookFileNameTextBox";
			this.CookFileNameTextBox.Size = new System.Drawing.Size(470, 20);
			this.CookFileNameTextBox.TabIndex = 0;
			// 
			// OtherRadioButton
			// 
			this.OtherRadioButton.AutoSize = true;
			this.OtherRadioButton.Location = new System.Drawing.Point(22, 262);
			this.OtherRadioButton.Name = "OtherRadioButton";
			this.OtherRadioButton.Size = new System.Drawing.Size(51, 17);
			this.OtherRadioButton.TabIndex = 7;
			this.OtherRadioButton.TabStop = true;
			this.OtherRadioButton.Text = "Other";
			this.OtherRadioButton.UseVisualStyleBackColor = true;
			this.OtherRadioButton.CheckedChanged += new System.EventHandler(this.OtherRadioButton_CheckedChanged);
			// 
			// CookRadioButton
			// 
			this.CookRadioButton.AutoSize = true;
			this.CookRadioButton.Location = new System.Drawing.Point(22, 175);
			this.CookRadioButton.Name = "CookRadioButton";
			this.CookRadioButton.Size = new System.Drawing.Size(50, 17);
			this.CookRadioButton.TabIndex = 5;
			this.CookRadioButton.TabStop = true;
			this.CookRadioButton.Text = "Cook";
			this.CookRadioButton.UseVisualStyleBackColor = true;
			this.CookRadioButton.CheckedChanged += new System.EventHandler(this.CookRadioButton_CheckedChanged);
			// 
			// StatusTextTextBox
			// 
			this.StatusTextTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.StatusTextTextBox.Location = new System.Drawing.Point(84, 41);
			this.StatusTextTextBox.Name = "StatusTextTextBox";
			this.StatusTextTextBox.Size = new System.Drawing.Size(289, 20);
			this.StatusTextTextBox.TabIndex = 1;
			// 
			// label4
			// 
			this.label4.AutoSize = true;
			this.label4.Location = new System.Drawing.Point(15, 44);
			this.label4.Name = "label4";
			this.label4.Size = new System.Drawing.Size(64, 13);
			this.label4.TabIndex = 4;
			this.label4.Text = "Status Text:";
			// 
			// OkButton
			// 
			this.OkButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.OkButton.Location = new System.Drawing.Point(562, 372);
			this.OkButton.Name = "OkButton";
			this.OkButton.Size = new System.Drawing.Size(75, 23);
			this.OkButton.TabIndex = 10;
			this.OkButton.Text = "Ok";
			this.OkButton.UseVisualStyleBackColor = true;
			this.OkButton.Click += new System.EventHandler(this.OkButton_Click);
			// 
			// NewCancelButton
			// 
			this.NewCancelButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.NewCancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.NewCancelButton.Location = new System.Drawing.Point(481, 372);
			this.NewCancelButton.Name = "NewCancelButton";
			this.NewCancelButton.Size = new System.Drawing.Size(75, 23);
			this.NewCancelButton.TabIndex = 9;
			this.NewCancelButton.Text = "Cancel";
			this.NewCancelButton.UseVisualStyleBackColor = true;
			this.NewCancelButton.Click += new System.EventHandler(this.NewCancelButton_Click);
			// 
			// groupBox3
			// 
			this.groupBox3.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox3.Controls.Add(this.CookFileNameButton);
			this.groupBox3.Controls.Add(this.label7);
			this.groupBox3.Controls.Add(this.CookFileNameTextBox);
			this.groupBox3.Location = new System.Drawing.Point(12, 177);
			this.groupBox3.Name = "groupBox3";
			this.groupBox3.Size = new System.Drawing.Size(625, 80);
			this.groupBox3.TabIndex = 6;
			this.groupBox3.TabStop = false;
			// 
			// groupBox4
			// 
			this.groupBox4.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox4.Controls.Add(this.OtherUseLogWindowCheckBox);
			this.groupBox4.Controls.Add(this.OtherFileNameTextBox);
			this.groupBox4.Controls.Add(this.label6);
			this.groupBox4.Controls.Add(this.OtherFileNameButton);
			this.groupBox4.Controls.Add(this.label5);
			this.groupBox4.Controls.Add(this.OtherArgumentsTextBox);
			this.groupBox4.Location = new System.Drawing.Point(12, 263);
			this.groupBox4.Name = "groupBox4";
			this.groupBox4.Size = new System.Drawing.Size(625, 99);
			this.groupBox4.TabIndex = 8;
			this.groupBox4.TabStop = false;
			// 
			// VariablesButton
			// 
			this.VariablesButton.Location = new System.Drawing.Point(9, 372);
			this.VariablesButton.Name = "VariablesButton";
			this.VariablesButton.Size = new System.Drawing.Size(75, 23);
			this.VariablesButton.TabIndex = 13;
			this.VariablesButton.Text = "Variables";
			this.VariablesButton.UseVisualStyleBackColor = true;
			this.VariablesButton.Click += new System.EventHandler(this.VariablesButton_Click);
			// 
			// BuildStepWindow
			// 
			this.AcceptButton = this.OkButton;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.NewCancelButton;
			this.ClientSize = new System.Drawing.Size(654, 407);
			this.Controls.Add(this.VariablesButton);
			this.Controls.Add(this.CookRadioButton);
			this.Controls.Add(this.OtherRadioButton);
			this.Controls.Add(this.StatusTextTextBox);
			this.Controls.Add(this.groupBox4);
			this.Controls.Add(this.label4);
			this.Controls.Add(this.groupBox3);
			this.Controls.Add(this.CompileRadioButton);
			this.Controls.Add(this.DurationTextBox);
			this.Controls.Add(this.NewCancelButton);
			this.Controls.Add(this.label1);
			this.Controls.Add(this.DescriptionTextBox);
			this.Controls.Add(this.OkButton);
			this.Controls.Add(this.label2);
			this.Controls.Add(this.groupBox1);
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.MaximizeBox = false;
			this.MaximumSize = new System.Drawing.Size(32768, 445);
			this.MinimizeBox = false;
			this.MinimumSize = new System.Drawing.Size(670, 445);
			this.Name = "BuildStepWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Edit Build Step";
			this.Load += new System.EventHandler(this.BuildTaskWindow_Load);
			this.groupBox1.ResumeLayout(false);
			this.groupBox1.PerformLayout();
			this.groupBox3.ResumeLayout(false);
			this.groupBox3.PerformLayout();
			this.groupBox4.ResumeLayout(false);
			this.groupBox4.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.TextBox DescriptionTextBox;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.TextBox DurationTextBox;
		private System.Windows.Forms.GroupBox groupBox1;
		private System.Windows.Forms.TextBox OtherArgumentsTextBox;
		private System.Windows.Forms.Button OtherFileNameButton;
		private System.Windows.Forms.TextBox OtherFileNameTextBox;
		private System.Windows.Forms.TextBox CookFileNameTextBox;
		private System.Windows.Forms.RadioButton OtherRadioButton;
		private System.Windows.Forms.RadioButton CookRadioButton;
		private System.Windows.Forms.TextBox CompileArgumentsTextBox;
		private System.Windows.Forms.ComboBox CompileConfigComboBox;
		private System.Windows.Forms.ComboBox CompilePlatformComboBox;
		private System.Windows.Forms.RadioButton CompileRadioButton;
		private System.Windows.Forms.Button OkButton;
		private System.Windows.Forms.Button NewCancelButton;
		private System.Windows.Forms.ComboBox CompileTargetComboBox;
		private System.Windows.Forms.Button CookFileNameButton;
		private System.Windows.Forms.Label label6;
		private System.Windows.Forms.Label label5;
		private System.Windows.Forms.CheckBox OtherUseLogWindowCheckBox;
		private System.Windows.Forms.TextBox StatusTextTextBox;
		private System.Windows.Forms.Label label4;
		private System.Windows.Forms.Label label9;
		private System.Windows.Forms.Label label8;
		private System.Windows.Forms.Label label7;
		private System.Windows.Forms.GroupBox groupBox3;
		private System.Windows.Forms.GroupBox groupBox4;
		private System.Windows.Forms.Button VariablesButton;
	}
}