// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.Drawing;
using System.Linq;
using System.Windows;
using System.Windows.Forms;
using System.Windows.Media;
using UnrealDocTool.Params;
using System.Windows.Controls;
using MarkdownSharp;
using System.Threading;
using UnrealDocTool.Properties;
using Application = System.Windows.Application;
using Button = System.Windows.Controls.Button;
using Color = System.Drawing.Color;
using GroupBox = System.Windows.Controls.GroupBox;
using HorizontalAlignment = System.Windows.HorizontalAlignment;
using MessageBox = System.Windows.MessageBox;
using Orientation = System.Windows.Controls.Orientation;
using Panel = System.Windows.Controls.Panel;
using TabControl = System.Windows.Forms.TabControl;


namespace UnrealDocTool.GUI
{
    public class GUI : Application
    {
        public void Init(Configuration config, WPFLogger logger)
        {
            this.config = config;
            this.logger = logger;
        }

        protected override void OnStartup(StartupEventArgs e)
        {
            mainWindow = new Window
            {
                Title = "Unreal Documentation Tool",
                Width = 1250,
                MinWidth = 650,
            };
            DefineMainLayout();
            CreateMainParamsPanel();
            AddParamsToPanel();
            AddLogger();
            mainGrid.Children.Add(mainParamsPanel);
            Grid.SetRow(mainParamsPanel, 0);
            mainWindow.Content = mainGrid;
            mainWindow.Show();
            base.OnStartup(e);
        }

        protected override void OnExit(ExitEventArgs e)
        {
            mainWindow.Close();
            base.OnExit(e);
        }

        private static void AddParamToPanel(Param param, Panel panel)
        {
            var element = AddToolTipToParam(param);

            panel.Children.Add(element);
        }

        private static FrameworkElement AddToolTipToParam(Param param)
        {
            var element = param.GetGUIElement();

            element.ToolTip = "toolTip";
            element.DataContext = param;
            element.ToolTipOpening +=
                (sender, args) => { element.ToolTip = ((sender as FrameworkElement).DataContext as Param).Description; };
            return element;
        }

        private void DefineMainLayout()
        {
            mainGrid = new Grid()
            {
                VerticalAlignment = VerticalAlignment.Stretch,
                HorizontalAlignment = HorizontalAlignment.Stretch,
            };
            var topPanel = new RowDefinition
            {
                Height = new GridLength(1, GridUnitType.Auto),
            };
            var splitterPanel = new RowDefinition
            {
                Height = new GridLength(1, GridUnitType.Auto),
            };
            var bottomPanel = new RowDefinition
            {
                Height = new GridLength(1, GridUnitType.Star),
            };
            mainGrid.RowDefinitions.Add(topPanel);
            mainGrid.RowDefinitions.Add(splitterPanel);
            mainGrid.RowDefinitions.Add(bottomPanel);
            var splitter = new GridSplitter()
            {
                ResizeDirection = GridResizeDirection.Rows,
                VerticalAlignment = VerticalAlignment.Stretch,
                HorizontalAlignment = HorizontalAlignment.Stretch,
                Height = 3,
                Background = new SolidColorBrush(Colors.Black),
                Margin = new Thickness(0),
            };
            mainGrid.Children.Add(splitter);
            Grid.SetRow(splitter, 1);
        }

        private void CreateMainParamsPanel()
        {
            mainParamsPanel = new Grid()
            {
                VerticalAlignment = VerticalAlignment.Stretch,
                HorizontalAlignment = HorizontalAlignment.Stretch,
                Margin = new Thickness(5),
            };
            var topContentRow1 = new RowDefinition
            {
                Height = new GridLength(1, GridUnitType.Auto)
            };
            var topContentRow2 = new RowDefinition
            {
                Height = new GridLength(1, GridUnitType.Auto)
            };
            var topContentRow3 = new RowDefinition
            {
                Height = new GridLength(1, GridUnitType.Auto)
            };
            mainParamsPanel.RowDefinitions.Add(topContentRow1);
            mainParamsPanel.RowDefinitions.Add(topContentRow2);
            mainParamsPanel.RowDefinitions.Add(topContentRow3);
        }

        private void AddParamsToPanel()
        {
            var pathParamPanel = new StackPanel();
            AddParamToPanel(config.ParamsManager.GetMainParam(), pathParamPanel);
            mainParamsPanel.Children.Add(pathParamPanel);
            Grid.SetRow(pathParamPanel, 0);

            var tabbedOptionsPanel = new System.Windows.Controls.TabControl
            {
                Margin = new Thickness(0, 5, 0, 5)
            };

            foreach (var group in config.ParamsManager.GetGroups())
            {
                var paramGroup = new TabItem() { Header = group.Description };

                var groupParams = config.ParamsManager.GetParamsByGroup(group).Where(p => !string.IsNullOrWhiteSpace(p.WpfName));

                var stackPanel = new WrapPanel();

                foreach (var param in groupParams)
                {
                    var newPanel = new WrapPanel();
                    newPanel.Margin = new Thickness(2, 0, 2, 0);
                    AddParamToPanel(param, newPanel);
                    stackPanel.Children.Add(newPanel);
                }
                paramGroup.Content = stackPanel;
                tabbedOptionsPanel.Items.Add(paramGroup);
            }
            mainParamsPanel.Children.Add(tabbedOptionsPanel);
            Grid.SetRow(tabbedOptionsPanel, 1);

            runButton = new Button { Content = Language.Message("Run")};
            Language.Loaded += () => { runButton.Content = Language.Message("Run");};
            runButton.Click += RunButtonOnClick;
            resetButton = new Button { Content = "Restore Defaults" };
            resetButton.Click += ResetButtonOnClick;

            var buttonPanel = new Grid();
            var colDefinition1 = new ColumnDefinition()
            {
                Width = new GridLength(1, GridUnitType.Star)
            };
            var colDefinition2 = new ColumnDefinition()
            {
                Width = new GridLength(1, GridUnitType.Star)
            };
            buttonPanel.ColumnDefinitions.Add(colDefinition1);
            buttonPanel.ColumnDefinitions.Add(colDefinition2);
            buttonPanel.Children.Add(runButton);
            Grid.SetColumn(runButton,0);
            buttonPanel.Children.Add(resetButton);
            Grid.SetColumn(runButton, 1);
            Grid.SetRow(buttonPanel, 2);
            mainParamsPanel.Children.Add(buttonPanel);
        }

        private void AddLogger()
        {
            mainGrid.Children.Add(logger.GetGUIElement());
            Grid.SetRow(logger.GetGUIElement(),3);
        }

        private void ResetButtonOnClick(object sender, RoutedEventArgs routedEventArgs)
        {
            //cheat a little bit here
            User.Default.Reset();
            Process.Start(System.Windows.Forms.Application.StartupPath + "\\UnrealDocTool.exe");
            Process.GetCurrentProcess().Kill();
        }

        private void RunButtonOnClick(object sender, RoutedEventArgs routedEventArgs)
        {
            logger.ClearLog();

            var ts = new ThreadStart(
                () =>
                {
                    try
                    {
                        UnrealDocTool.RunUDNConversion(config, logger);
                    }
                    catch (ParsingErrorException e)
                    {
                        MessageBox.Show(e.Message, "Parsing error", MessageBoxButton.OK, MessageBoxImage.Exclamation);
                    }

                    runButton.Dispatcher.Invoke(new Action(() => { runButton.IsEnabled = true; }));
                });

            var converterThread = new Thread(ts);
            converterThread.SetApartmentState(ApartmentState.STA);
            converterThread.Start();

            runButton.IsEnabled = false;
        }

        private Grid mainGrid;
        private Grid mainParamsPanel;
        private Window mainWindow;
        private Configuration config;
        private WPFLogger logger;
        private Button runButton;
        private Button resetButton;
    }
}
