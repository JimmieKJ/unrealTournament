// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Forms;
using MarkdownSharp;
using System.IO;

using GroupBox = System.Windows.Controls.GroupBox;
using TextBox = System.Windows.Controls.TextBox;
using Button = System.Windows.Controls.Button;

namespace UnrealDocTool.Params
{
    public class PathParam : StringParam
    {
        public PathParam(
            string name,
            string wpfName,
            Func<string> descriptionGetter,
            string defaultValue,
            string defaultPathToBrowse = null,
            Action<string> parsedAction = null,
            ParamsDescriptionGroup descGroup = null)
            : base(name, wpfName, descriptionGetter, defaultValue, descGroup, parsedAction)
        {
            this.defaultPathToBrowse = defaultPathToBrowse;
        }

        public override FrameworkElement GetGUIElement()
        {
            var box = base.GetGUIElement() as GroupBox;
            var textBox = box.Content as TextBox;
            box.Content = null;
            textBox.Margin = new Thickness(0,0,5,0);
            var grid = new Grid();
            
            var browseButton = new Button();

            browseButton.Click += (sender, args) =>
                {
                    var dlg = new FolderBrowserDialog();

                    var dir = new DirectoryInfo(defaultPathToBrowse);

                    if (dir.Exists)
                    {
                        dlg.SelectedPath = dir.FullName;
                    }

                    var result = dlg.ShowDialog();

                    if (result == DialogResult.OK)
                    {
                        textBox.Text = dlg.SelectedPath;
                    }
                };

            browseButton.Content = Language.Message("Browse");

            Language.Loaded += () =>
                { browseButton.Content = Language.Message("Browse"); };

            var col0 = new ColumnDefinition { Width = new GridLength(100, GridUnitType.Star) };
            var col1 = new ColumnDefinition { Width = new GridLength(100) };

            grid.ColumnDefinitions.Add(col0);
            grid.ColumnDefinitions.Add(col1);

            grid.Children.Add(textBox);

            Grid.SetRow(textBox, 0);
            Grid.SetColumn(textBox, 0);

            grid.Children.Add(browseButton);

            Grid.SetRow(browseButton, 0);
            Grid.SetColumn(browseButton, 1);

            box.Header = WpfName;
            box.Content = grid;

            return box;
        }

        private readonly string defaultPathToBrowse;
    }
}
