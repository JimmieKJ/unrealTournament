// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace MarkdownMode.Perforce
{
    using System.Collections.Generic;
    using System.Windows;
    using System.Windows.Controls;

    using global::Perforce.P4;

    /// <summary>
    /// Interaction logic for AddImagesToChangelistWindow.xaml
    /// </summary>
    public partial class AddImagesToChangelistWindow
    {
        private const int ComboboxOptionMaxLength = 50;

        private readonly Window window;
        private readonly List<string> missingDepotFiles;
        private readonly Dictionary<int, Changelist> changelists; 

        public AddImagesToChangelistWindow(Window window, List<string> missingDepotFiles)
        {
            this.window = window;
            this.missingDepotFiles = missingDepotFiles;

            changelists = PerforceHelper.GetPendingChangelists();

            InitializeComponent();

            InitializeChangelistComboBoxOptions();
        }

        private void InitializeChangelistComboBoxOptions()
        {
            changelistDescriptionTextBox.IsEnabled = false;
            changelistDescriptionTextBox.Text = "";

            changelistComboBox.Items.Add(new ComboBoxItem { Content = "default", DataContext = -1, IsSelected = true });
            changelistComboBox.Items.Add(new ComboBoxItem { Content = "new", DataContext = -2 });

            foreach (var changelistPair in changelists)
            {
                var changelist = changelistPair.Value;

                var name = changelist.Description.Replace("\n", "");
                name = (name.Length <= ComboboxOptionMaxLength)
                               ? name
                               : name.Substring(0, ComboboxOptionMaxLength - 4) + " ...";

                changelistComboBox.Items.Add(new ComboBoxItem { Content = name, DataContext = changelist.Id });
            }

            changelistComboBox.SelectionChanged += (sender, args) =>
                {
                    var item = args.AddedItems[0] as ComboBoxItem;
                    var clId = (int) item.DataContext;

                    if (clId == -1)
                    {
                        changelistDescriptionTextBox.IsEnabled = false;
                        changelistDescriptionTextBox.Text = "";

                        return;
                    }

                    changelistDescriptionTextBox.IsEnabled = true;
                    changelistDescriptionTextBox.Text =
                        clId == -2
                            ? MarkdownSharp.Language.Message("MissingImagesNewChangelistDesc")
                            : changelists[clId].Description;
                };
        }

        public static void ShowAddImagesToChangelistWindow(List<string> files)
        {
            var window = new Window { Width = 300, Height = 300 };
            window.Content = new AddImagesToChangelistWindow(window, files);

            window.Show();
        }

        private void CancelButtonClick(object sender, RoutedEventArgs e)
        {
            window.Close();
        }

        private void OkButtonClick(object sender, RoutedEventArgs e)
        {
            CheckoutFiles();

            window.Close();
        }

        private void CheckoutFiles()
        {
            var clId = (int)(changelistComboBox.SelectedItem as ComboBoxItem).DataContext;

            if (clId == -2)
            {
                clId = PerforceHelper.CreateNewChangelist();
            }

            PerforceHelper.AddFilesToChangelist(clId, missingDepotFiles);
        }

    }
}
