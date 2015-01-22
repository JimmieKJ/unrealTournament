// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Forms;
using System.Windows.Forms.VisualStyles;
using MarkdownSharp;
using System.Linq;
using UnrealDocTool.Properties;
using ComboBox = System.Windows.Controls.ComboBox;
using GroupBox = System.Windows.Controls.GroupBox;
using Orientation = System.Windows.Controls.Orientation;
using RadioButton = System.Windows.Controls.RadioButton;

namespace UnrealDocTool.Params
{
    public class EnumParam<TEnum> : Param
    {
        public EnumParam(
            string name,
            string wpfName,
            Func<string> descriptionGetter,
            TEnum defaultOption,
            ParamsDescriptionGroup descGroup = null,
            Action<string> afterParsing = null)
            : base(name, wpfName, descriptionGetter, descGroup, afterParsing)
        {
            // Have to check it in run-time because for some reason C# disallows Enum as generic
            // type constraint (public class EnumParam<TEnum> : Param where TEnum : Enum).
            if (!(defaultOption is Enum))
            {
                throw new ArgumentException("Only enums are allowed as EnumParam type.");
            }

            foreach (var field in typeof(TEnum).GetFields())
            {
                if (field.Name.Equals("value__"))
                {
                    continue;
                }

                options.Add(field.Name, (TEnum) field.GetRawConstantValue());
            }
        }

        public override void Parse(string param)
        {
            var validOption = false;

            foreach (var pair in options.Where(pair => param.ToLower() == pair.Key.ToLower()))
            {
                ChosenOption = pair.Value;
                validOption = true;
                break;
            }

            if (!validOption)
            {
                throw new ParsingErrorException(this, Language.Message("NotSupportedOption", param, string.Join(", ", options.Keys)));
            }

            base.Parse(param);
        }

        public override FrameworkElement GetGUIElement()
        {
            var group = new GroupBox();

            group.Header = WpfName;

            var stackPanel = new StackPanel { Orientation = Orientation.Vertical };

            var comboBox = new ComboBox();
            
            
            //Convert to a combo box -------------
            foreach (var optionPair in options)
            {
                var comboItem = new ComboBoxItem()
                {
                    Content = optionPair.Key,
                    DataContext = optionPair,
                };
                comboBox.Items.Add(comboItem);
                comboItem.Selected += (sender, args) =>
                {
                    var optPair = (KeyValuePair<string, TEnum>) (sender as ComboBoxItem).DataContext;
                    ChosenOption = optPair.Value;
                    TriggerAfterParsingEvent(optPair.Key);
                };
            }
            comboBox.SelectedIndex = ChosenOption.GetHashCode();
            stackPanel.Children.Add(comboBox);
            //Combo box end ----------------------
            group.Content = stackPanel;

            return group;
        }

        public override string GetExample()
        {
            return "[-" + Name + "=" + string.Join("|", options.Keys) + "]";
        }

        public static implicit operator TEnum(EnumParam<TEnum> p)
        {
            return p.ChosenOption;
        }

        public TEnum ChosenOption { get; private set; }

        private readonly Dictionary<string, TEnum> options = new Dictionary<string, TEnum>();
    }
}
