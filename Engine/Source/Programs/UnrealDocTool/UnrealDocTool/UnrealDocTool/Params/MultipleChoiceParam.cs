// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Controls;
using System.Windows;
using MarkdownSharp;

namespace UnrealDocTool.Params
{
    public class MultipleChoiceParam : Param
    {
        public MultipleChoiceParam(
            string name,
            string wpfName,
            IEnumerable<string> options,
            IEnumerable<int> defaultOptions,
            Func<string> descriptionGetter,
            ParamsDescriptionGroup descGroup = null)
            : base(name, wpfName, descriptionGetter, descGroup)
        {
            this.options = options.ToArray();

            foreach (var defaultOption in defaultOptions)
            {
                chosenOptions.Add(defaultOption);
            }
        }

        public override void Parse(string param)
        {
            var textOptions = param.Split(',').Select((t) => t.ToLower());

            var currentChosenOptions = new HashSet<int>();

            foreach (var textOption in textOptions)
            {
                var validOption = false;

                for (var i = 0; i < options.Length; ++i)
                {
                    if (textOption != options[i].ToLower())
                    {
                        continue;
                    }

                    currentChosenOptions.Add(i);
                    validOption = true;
                    break;
                }

                if (!validOption)
                {
                    throw new ParsingErrorException(this, Language.Message("NotSupportedOption", textOption, string.Join(", ", options)));
                }
            }

            chosenOptions = currentChosenOptions;

            base.Parse(param);
        }

        public override FrameworkElement GetGUIElement()
        {
            var group = new GroupBox();

            group.Header = WpfName;

            var stackPanel = new StackPanel { Orientation = Orientation.Horizontal };

            for (var i = 0; i < options.Length; ++i)
            {
                var checkBox = new CheckBox
                                 {
                                     Content = options[i],
                                     DataContext = i,
                                     IsChecked = chosenOptions.Contains(i)
                                 };
                checkBox.Margin = new Thickness(4, 3, 4, 3);
                checkBox.Checked += (sender, args) =>
                    { chosenOptions.Add((int)(sender as CheckBox).DataContext); };
                checkBox.Unchecked += (sender, args) =>
                    { chosenOptions.Remove((int)(sender as CheckBox).DataContext); };

                stackPanel.Children.Add(checkBox);
            }

            group.Content = stackPanel;

            return group;
        }

        public override string GetExample()
        {
            return "[-" + Name + "=(subset of " + string.Join(",", options) + ")]";
        }

        public string[] ChosenStringOptions
        {
            get
            {
                return chosenOptions.Select((i) => options[i]).ToArray();
            }
        }

        private HashSet<int> chosenOptions = new HashSet<int>();
        private readonly string[] options;
    }
}
