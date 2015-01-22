// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.Windows.Forms;

namespace UnrealDocTranslator
{
    // Implements a TraceListener for a text box
    public class TraceListenerForTextBoxOutput : TraceListener
    {
        private TextBox OutputTextBox;

        public TraceListenerForTextBoxOutput(TextBox OutputTextBox)
        {
            this.OutputTextBox = OutputTextBox;
        }


        public override void Write(string OutputMessage)
        {
            OutputTextBox.BeginInvoke((Action)(() => OutputTextBox.AppendText(OutputMessage)));
        }

        public override void WriteLine(string OutputMessage)
        {
            Write(OutputMessage + Environment.NewLine);
        }
    }
}


