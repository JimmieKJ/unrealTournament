// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using MarkdownSharp;
using System.Collections.Generic;
namespace MarkdownMode.Properties {
    
    
    // This class allows you to handle specific events on the settings class:
    //  The SettingChanging event is raised before a setting's value is changed.
    //  The PropertyChanged event is raised after a setting's value is changed.
    //  The SettingsLoaded event is raised after the setting values are loaded.
    //  The SettingsSaving event is raised before the setting values are saved.
    internal sealed partial class Settings {
        static public System.Drawing.Color DefaultImageFillColor;
        static public string[] SupportedLanguages;
        static public string[] SupportedLanguageLabels;
        static public string[] MetadataErrorIfMissing;
        static public string[] MetadataInfoIfMissing;
        static public List<string> SupportedAvailabilities;

        public Settings() {
            // // To add event handlers for saving and changing settings, uncomment the lines below:
            //
            // this.SettingChanging += this.SettingChangingEventHandler;
            //
            // this.SettingsSaving += this.SettingsSavingEventHandler;
            //
            DefaultImageFillColor = ImageConversion.GetColorFromHexString(DefaultImageFillColorText);
            SupportedLanguages = SupportedLanguagesString.Split(',');
            SupportedLanguageLabels = SupportedLanguageLabelsString.Split(',');
            MetadataErrorIfMissing = MetadataErrorIfMissingString.Split(',');
            MetadataInfoIfMissing = MetadataInfoIfMissingString.Split(',');
            SupportedAvailabilities = new List<string>(SupportedAvailabilitiesString.Split(','));
        }
        
        private void SettingChangingEventHandler(object sender, System.Configuration.SettingChangingEventArgs e) {
            // Add code to handle the SettingChangingEvent event here.
        }
        
        private void SettingsSavingEventHandler(object sender, System.ComponentModel.CancelEventArgs e) {
            // Add code to handle the SettingsSaving event here.
        }
    }
}
