// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace AutoReporter
{
    public partial class CrashURLDlg : Form
    {
        public CrashURLDlg()
        {
            InitializeComponent();
        }

        private void CrashURLClose_Button_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void CrashURL_linkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            try
            {
                LinkLabel LinkLab = (LinkLabel)sender;
                if (LinkLab != null)
                {
                    System.Diagnostics.Process.Start(LinkLab.Text);
                }
            }
            finally
            {
            }
        }
    }
}
