// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using CommonUnrealMarkdown;
using System.Windows;
using System.Windows.Input;
using ContextMenu = System.Windows.Controls.ContextMenu;
using ListView = System.Windows.Controls.ListView;
using ListViewItem = System.Windows.Controls.ListViewItem;
using MenuItem = System.Windows.Controls.MenuItem;
using SelectionMode = System.Windows.Controls.SelectionMode;

namespace UnrealDocTool.GUI
{
    public class WPFLogger : LogFileLogger
    {
        public WPFLogger() : base()
        {
            listView = new ListView { MinHeight = 210 };
            
            var copyText = new MenuItem { Header = "Copy" };
            copyText.Click += OnCopy;

            var rc = new ContextMenu();
            rc.Items.Add(copyText);
            listView.ContextMenu = rc;

            listView.SelectionMode = SelectionMode.Multiple;
            listView.CommandBindings.Add(new CommandBinding(ApplicationCommands.Copy, OnCopy));

            listView.InputBindings.Add(new KeyBinding(ApplicationCommands.SelectAll,
                          new KeyGesture(Key.A, ModifierKeys.Control)));
            listView.CommandBindings.Add(new CommandBinding(ApplicationCommands.SelectAll, (sender, args) => listView.SelectAll()));
        }

        public override void WriteToLog(string text)
        {
            listView.Dispatcher.Invoke(new Action(
                () =>
                {
                    var item = new ListViewItem { Content = text };

                    listView.Items.Add(item);
                    listView.ScrollIntoView(item);
                }), null);

            base.WriteToLog(text);
        }

        public override void ClearLog()
        {
            listView.Items.Clear();
        }

        public FrameworkElement GetGUIElement()
        {
            return listView;
        }

        private void OnCopy(object sender, EventArgs args)
        {
            var logText = listView.Items.Cast<ListViewItem>().Where(item => item.IsSelected).Aggregate("", (current, item) => current + (item.Content.ToString() + "\n"));

            System.Windows.Clipboard.SetText(logText);
        }

        private readonly ListView listView;
    }
}
