namespace Tools.CrashReporter.RegisterPII
{
	partial class RegisterPII
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose( bool disposing )
		{
			if( disposing && ( components != null ) )
			{
				components.Dispose();
			}
			base.Dispose( disposing );
		}

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.TitleLabel = new System.Windows.Forms.Label();
			this.ContentLabel = new System.Windows.Forms.Label();
			this.MachineGUIDLabel = new System.Windows.Forms.Label();
			this.ExitLabel = new System.Windows.Forms.Label();
			this.SuspendLayout();
			// 
			// TitleLabel
			// 
			this.TitleLabel.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.TitleLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.TitleLabel.ForeColor = System.Drawing.Color.LightCyan;
			this.TitleLabel.Location = new System.Drawing.Point(12, 18);
			this.TitleLabel.Name = "TitleLabel";
			this.TitleLabel.Size = new System.Drawing.Size(456, 20);
			this.TitleLabel.TabIndex = 0;
			this.TitleLabel.Text = "Register PII - INTERNAL USE ONLY!";
			this.TitleLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// ContentLabel
			// 
			this.ContentLabel.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.ContentLabel.ForeColor = System.Drawing.Color.LightCyan;
			this.ContentLabel.Location = new System.Drawing.Point(16, 57);
			this.ContentLabel.Name = "ContentLabel";
			this.ContentLabel.Size = new System.Drawing.Size(452, 23);
			this.ContentLabel.TabIndex = 1;
			this.ContentLabel.Text = "Mapping user \'Anonymous\' and machine name \'Z0000\'";
			this.ContentLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// MachineGUIDLabel
			// 
			this.MachineGUIDLabel.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.MachineGUIDLabel.ForeColor = System.Drawing.Color.LightCyan;
			this.MachineGUIDLabel.Location = new System.Drawing.Point(16, 80);
			this.MachineGUIDLabel.Name = "MachineGUIDLabel";
			this.MachineGUIDLabel.Size = new System.Drawing.Size(452, 23);
			this.MachineGUIDLabel.TabIndex = 2;
			this.MachineGUIDLabel.Text = "to machine \'00000000-0000-0000-0000-000000000000\'";
			this.MachineGUIDLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// ExitLabel
			// 
			this.ExitLabel.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.ExitLabel.ForeColor = System.Drawing.Color.LightCyan;
			this.ExitLabel.Location = new System.Drawing.Point(13, 123);
			this.ExitLabel.Name = "ExitLabel";
			this.ExitLabel.Size = new System.Drawing.Size(452, 23);
			this.ExitLabel.TabIndex = 3;
			this.ExitLabel.Text = "Automatically exiting in 5 seconds";
			this.ExitLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// RegisterPII
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.BackColor = System.Drawing.Color.MidnightBlue;
			this.ClientSize = new System.Drawing.Size(480, 160);
			this.Controls.Add(this.ExitLabel);
			this.Controls.Add(this.MachineGUIDLabel);
			this.Controls.Add(this.ContentLabel);
			this.Controls.Add(this.TitleLabel);
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.None;
			this.Name = "RegisterPII";
			this.ShowIcon = false;
			this.ShowInTaskbar = false;
			this.Text = "Register PII - INTERNAL USE ONLY!";
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.Label TitleLabel;
		private System.Windows.Forms.Label ContentLabel;
		private System.Windows.Forms.Label MachineGUIDLabel;
		private System.Windows.Forms.Label ExitLabel;
	}
}

