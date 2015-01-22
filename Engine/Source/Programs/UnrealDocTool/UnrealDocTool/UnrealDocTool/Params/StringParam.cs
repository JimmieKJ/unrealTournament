// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Windows;
using System.Windows.Controls;

namespace UnrealDocTool.Params
{
    public class StringParam : Param
    {
        public StringParam(
            string name,
            string wpfName,
            Func<string> descriptionGetter,
            string defaultValue,
            ParamsDescriptionGroup descGroup = null,
            Action<string> parsedAction = null)
            : base(name, wpfName, descriptionGetter, descGroup, parsedAction)
        {
            Value = defaultValue;
        }

        public string Value { get; private set; }

        public override void Parse(string param)
        {
            Value = param;
            base.Parse(param);
        }

        public override FrameworkElement GetGUIElement()
        {
            var box = new GroupBox();

            var textBox = new TextBox { Text = Value };

            textBox.TextChanged += (sender, args) =>
                { Value = textBox.Text; };

            box.Header = WpfName;
            box.Content = textBox;

            return box;
        }

        public static implicit operator string(StringParam p)
        {
            return p.Value;
        }
    }
}
