// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace UnrealDocTool.Params
{
    public class Flag : Param
    {
        public Flag(
            string name,
            string wpfName,
            Func<string> descriptionGetter,
            ParamsDescriptionGroup descGroup = null)
            : this(name, wpfName, descriptionGetter, false, descGroup)
        {
        }

        public Flag(
            string name,
            string wpfName,
            Func<string> descriptionGetter,
            bool defaultValue,
            ParamsDescriptionGroup descGroup = null)
            : base(name, wpfName, descriptionGetter, descGroup)
        {
            Value = defaultValue;
        }

        public override string GetExample()
        {
            return "[-" + Name + "]";
        }

        public override void Parse(string param)
        {
            Value = !NegationWordList.Any(negationWord => param.ToLower().Equals(negationWord));

            base.Parse(param);
        }

        public override FrameworkElement GetGUIElement()
        {
            var chkbox = new CheckBox { Content = WpfName, IsChecked = Value };
            chkbox.Margin = new Thickness(10, 25, 10, 0);
            chkbox.Checked += (sender, args) =>
                { Value = true; };

            chkbox.Unchecked += (sender, args) =>
                { Value = false; };

            return chkbox;
        }

        public bool Value { get; private set; }

        public static implicit operator bool(Flag f)
        {
            return f.Value;
        }

        static readonly string[] NegationWordList = new string[] { "false", "n", "no" };
    }
}
