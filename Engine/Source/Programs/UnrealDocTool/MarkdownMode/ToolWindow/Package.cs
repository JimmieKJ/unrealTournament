// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.InteropServices;
using System.ComponentModel.Design;
using EnvDTE;
using MarkdownSharp;
using MarkdownSharp.Doxygen;
using Microsoft.Internal.VisualStudio.PlatformUI;
using Microsoft.VisualStudio.PlatformUI;
using Microsoft.VisualStudio.Shell.Interop;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio;
using System.Collections.Generic;
using MarkdownMode.Properties;
using System.Collections.ObjectModel;
using System.ComponentModel.Composition;
using System.IO;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.Editor;
using Microsoft.VisualStudio.ComponentModelHost;
using Microsoft.VisualStudio.Text;
using System.Linq;
using MarkdownMode.ToolWindow;
using Process = System.Diagnostics.Process;

namespace MarkdownMode
{
    using System.Windows;
    using System.Windows.Forms;

    using MarkdownMode.Perforce;

    using MarkdownSharp.EpicMarkdown;

    using MessageBox = System.Windows.Forms.MessageBox;

    [PackageRegistration(UseManagedResourcesOnly = true)]
    [InstalledProductRegistration("#110", "#112", "1.0")]
    [ProvideMenuResource(1000, 1)]
    [ProvideToolWindow(typeof(MarkdownPreviewToolWindow))]
    [Guid(GuidList.guidMarkdownPackagePkgString)]
    [ProvideOptionPage(typeof(MarkdownModeOptions), "MarkdownMode", "General", 0, 0, true)]
    public sealed class MarkdownPackage : Package
    {
        UDNDocRunningTableMonitor rdtm = null;
        internal UDNDocView CurrentUDNDocView { get { return UDNDocRunningTableMonitor.CurrentUDNDocView; }}


        public static MarkdownPackage ForceLoadPackage(IVsShell shell)
        {
            Guid packageGuid = new Guid(GuidList.guidMarkdownPackagePkgString);
            IVsPackage package;

            if (VSConstants.S_OK == shell.IsPackageLoaded(ref packageGuid, out package))
                return package as MarkdownPackage;
            else if (ErrorHandler.Succeeded(shell.LoadPackage(ref packageGuid, out package)))
                return package as MarkdownPackage;
            return null;
        }

        public MarkdownPackage()
        {
            Trace.WriteLine(MarkdownSharp.Language.Message("MarkdownPackageLoaded"));
        }

        private void ShowToolWindow(object sender, EventArgs e)
        {
            var window = GetToolWindow(true);

            if (window != null)
            {
                BufferIdleEventUtil.BufferChanged((object)CurrentUDNDocView.TextEditorView.TextBuffer);

                IVsWindowFrame windowFrame = (IVsWindowFrame)window.Frame;
                Microsoft.VisualStudio.ErrorHandler.ThrowOnFailure(windowFrame.Show());
            }
        }

        void OpenImageFolder(object sender, EventArgs e)
        {
            bool ThisIsASupportedLanguage = false;

            FileInfo SourceFileInfo = new FileInfo(CurrentUDNDocView.SourceFilePath);
            string CurrentLanguage = SourceFileInfo.Name.Substring(SourceFileInfo.Name.IndexOf(".udn") - 3, 3);

            foreach (string Language in Settings.SupportedLanguages)
            {
                if (CurrentLanguage == Language)
                {
                    ThisIsASupportedLanguage = true;
                    break;
                }
            }
            if (!ThisIsASupportedLanguage)
            {
                MessageBox.Show(MarkdownSharp.Language.Message("LanguageOfFileNotSupported", SourceFileInfo.Name));
            }


            if (CurrentLanguage.Equals("INT"))
            {
                DirectoryInfo ImageFileDirectory = new DirectoryInfo(Path.Combine(SourceFileInfo.Directory.FullName, Settings.Default.ImagesFolderName));
                if (!ImageFileDirectory.Exists)
                {
                    ImageFileDirectory.Create();
                }
                Process.Start(ImageFileDirectory.FullName);
            }
            else
            {
                DirectoryInfo ImageLanguageFileDirectory = new DirectoryInfo(Path.Combine(Path.Combine(SourceFileInfo.Directory.FullName, Settings.Default.ImagesFolderName), CurrentLanguage));
                if (!ImageLanguageFileDirectory.Exists)
                {
                    ImageLanguageFileDirectory.Create();
                }
                Process.Start(ImageLanguageFileDirectory.FullName);
            }
        }

        private bool _doxygenIsRebuilding = false;

        List<string> selectedItemPaths = new List<string>();

        static readonly EnvDTE80.DTE2 _applicationObject = GetGlobalService(typeof(DTE)) as EnvDTE80.DTE2;

        readonly UIHierarchy uih = _applicationObject.ToolWindows.SolutionExplorer;

        void RebuildDoxygen(object sender, EventArgs e)
        {
            Action<string, Exception> showExceptionBox = (reason, ex) =>
                {
                    MessageBox.Show(reason + " The error was: " + ex.Message);
                };

            DirectoryInfo currentSourceDir;

            currentSourceDir = new DirectoryInfo(Path.GetDirectoryName(CurrentUDNDocView.SourceFilePath));

            while (currentSourceDir != null && currentSourceDir.Name != "Source")
            {
                currentSourceDir = currentSourceDir.Parent;
            }

            Debug.Assert(currentSourceDir != null, "currentSourceDir != null");
            Debug.Assert(currentSourceDir.Parent != null, "currentSourceDir.Parent != null");
            Debug.Assert(currentSourceDir.Parent.Parent != null, "currentSourceDir.Parent.Parent != null");

            var engineDir = currentSourceDir.Parent.Parent;
            var sourceDir = engineDir.GetDirectories("Source")[0];

            _doxygenIsRebuilding = true;
            DoxygenHelper.DoxygenRebuildingFinished += DoxygenHelperOnDoxygenRebuildingFinished;
            DoxygenHelper.RebuildingDoxygenProgressChanged += DoxygenHelperOnRebuildingDoxygenProgressChanged;

            try
            {
                DoxygenHelper.DoxygenInputFilter = Path.Combine(
	  	                    engineDir.FullName, "Binaries", "DotNET", "UnrealDocToolDoxygenInputFilter.exe");
  	                DoxygenHelper.RebuildCache(sourceDir.FullName, null, true);
            }
            catch (Exception ex)
            {
                if (!(ex is DoxygenHelper.DoxygenExecMissing) && !(ex is DoxygenHelper.InvalidDoxygenPath))
                {
                    showExceptionBox("Could not rebuild Doxygen database. Unhandled exception.", ex);
                    throw;
                }

                showExceptionBox("Could not rebuild Doxygen database.", ex);
                DoxygenHelperOnDoxygenRebuildingFinished();
            }
        }

        private void DoxygenHelperOnRebuildingDoxygenProgressChanged(double progress)
        {
            var statusBar = (IVsStatusbar)GetService(typeof(SVsStatusbar));
            const uint range = 1000;
            const string label = "Rebuilding Doxygen cache...";

            uint cookie = 0;

            if (Math.Abs(progress - 1.0) < 0.00000001)
            {
                // Zeroing and clearing the progress bar.
                statusBar.Progress(ref cookie, 1, label, (uint)(range * 0), range);
                statusBar.Progress(ref cookie, 0, "", 0, 0);
            }
            else
            {
                statusBar.Progress(ref cookie, 1, label, (uint)(range * progress), range);
            }
        }

        void SetTextStyle(Object sender, EventArgs e, String decoration)
        {
            ITextBuffer _mybuffer = CurrentUDNDocView.TextEditorView.TextBuffer;
            ITextSelection _selection = CurrentUDNDocView.TextEditorView.Selection;

            var start = _selection.Start.Position.Position;
            var end = _selection.End.Position.Position;
            var preceedingCharacter = _mybuffer.CurrentSnapshot.GetText(start - 1, 1);

            if (preceedingCharacter != decoration.Substring(preceedingCharacter.Length - 1, 1))
            {
                if (_selection.Start.Position == _selection.End.Position)
                {
                    _mybuffer.Insert(_selection.Start.Position, decoration);
                }
                else
                {
                    _mybuffer.Insert(_selection.Start.Position, decoration);
                    _mybuffer.Insert(_selection.End.Position, decoration);
                }
                CurrentUDNDocView.TextEditorView.Selection.Select(
                    new SnapshotSpan(CurrentUDNDocView.TextEditorView.TextSnapshot, start + decoration.Length,
                        end - start), false);
            }
            else
            {
                _mybuffer.Delete(new Span(start - decoration.Length, decoration.Length));
                _mybuffer.Delete(new Span(end - decoration.Length, decoration.Length));
            }
        }

        void PublishSelectedDocuments(Object sender, EventArgs e, bool recursive)
        {
            var selectedItems = (Array)uih.SelectedItems;
            var argString = this.PublishFlags.Count > 0 ? " -loc=INT -lang=INT -publish=" + string.Join(",", this.PublishFlags) : " -loc=INT -lang=INT";

            foreach (UIHierarchyItem selectedItem in selectedItems)
            {
                var projectItem = selectedItem.Object as ProjectItem;
                var pathString = projectItem.Properties.Item("FullPath").Value.ToString();
                var startInfo = new ProcessStartInfo();
                startInfo.CreateNoWindow = true;
                startInfo.UseShellExecute = false;
                startInfo.FileName = GetOptions().UnrealDocToolPath + "UnrealDocTool.exe";
                startInfo.WindowStyle = ProcessWindowStyle.Hidden;
                startInfo.Arguments = pathString + argString;
                if (recursive)
                {
                    var position = pathString.LastIndexOf('\\');
                    if (position >= 0)
                    {
                        pathString = pathString.Substring(0, position+1);
                    };
                    startInfo.Arguments = pathString + argString + " -r";
                    startInfo.CreateNoWindow = false;
                }
                
                using (Process exeProcess = Process.Start(startInfo))
                {
                    exeProcess.WaitForExit();
                }
            }
        }

        private void DoxygenHelperOnDoxygenRebuildingFinished()
        {
            _doxygenIsRebuilding = false;
            DoxygenHelper.DoxygenRebuildingFinished -= DoxygenHelperOnDoxygenRebuildingFinished;
            DoxygenHelper.RebuildingDoxygenProgressChanged -= DoxygenHelperOnRebuildingDoxygenProgressChanged;

            DoxygenHelper.InvalidateDbCache();
          
            var shell = GetService(typeof(SVsUIShell)) as IVsUIShell;

            Debug.Assert(shell != null, "shell != null");

            shell.UpdateCommandUI(1);
        }

        private MarkdownModeOptions GetOptions()
        {
            return (MarkdownModeOptions) GetDialogPage(typeof (MarkdownModeOptions));
        }

        internal MarkdownPreviewToolWindow GetToolWindow(bool create)
        {
            var window = this.FindToolWindow(typeof(MarkdownPreviewToolWindow), 0, create);
            if ((null == window) || (null == window.Frame))
            {
                return null;
            }

            return window as MarkdownPreviewToolWindow;
        }

        void NavigateTo(SnapshotPoint point)
        {
            CurrentUDNDocView.TextEditorView.Selection.Clear();
            CurrentUDNDocView.TextEditorView.Caret.MoveTo(point);

            CurrentUDNDocView.TextEditorView.ViewScroller.EnsureSpanVisible(
                CurrentUDNDocView.TextEditorView.Selection.StreamSelectionSpan.SnapshotSpan, EnsureSpanVisibleOptions.AlwaysCenter);

            CurrentUDNDocView.TextEditorView.VisualElement.Focus();
        }


        void NavigateToComboSelected(object sender, EventArgs e)
        {
            if (e == null)
            {
                throw new ArgumentException(MarkdownSharp.Language.Message("EventArgsShouldNotBeNull"));
            }

            OleMenuCmdEventArgs eventArgs = e as OleMenuCmdEventArgs;

            if (eventArgs == null)
            {
                throw new ArgumentException(MarkdownSharp.Language.Message("EventArgsShouldBeOfTypeOleMenuCmdEventArgs"));
            }

            if (eventArgs.InValue != null)
            {
                ITextSnapshot snapshot = CurrentUDNDocView.TextEditorView.TextSnapshot;

                int selectedIndex = (int)eventArgs.InValue;

                if(selectedIndex == 0)
                {
                    NavigateTo(new SnapshotPoint(snapshot, 0));
                }
                else
                {
                    selectedIndex--;

                    if (selectedIndex >= CurrentUDNDocView.NavigateToComboData.Count)
                    {
                        Debug.Fail(MarkdownSharp.Language.Message("ItemInTheComboWasSelectedThatIsntAValidSelection"));
                        return;
                    }

                    NavigateTo(CurrentUDNDocView.NavigateToComboData.GetSection(selectedIndex).GetStartPoint(snapshot));
                }
            }
            else if (eventArgs.OutValue != null)
            {
                Marshal.GetNativeVariantForObject(CurrentUDNDocView.NavigateToComboData.GetCurrentItemValue(), eventArgs.OutValue);
            }
            else
            {
                throw new InvalidOperationException(MarkdownSharp.Language.Message("ShouldNeverBeHere"));
            }
        }

        void NavigateToComboGetList(object sender, EventArgs e)
        {
            if (e == null)
            {
                throw new ArgumentException(MarkdownSharp.Language.Message("EventArgsShouldNotBeNull"));
            }

            OleMenuCmdEventArgs eventArgs = e as OleMenuCmdEventArgs;

            if (eventArgs == null)
            {
                throw new ArgumentException(MarkdownSharp.Language.Message("EventArgsShouldBeOfTypeOleMenuCmdEventArgs"));
            }

            Marshal.GetNativeVariantForObject(CurrentUDNDocView.NavigateToComboData.GetCurrentItemsSet(), eventArgs.OutValue);
        }

        void PublishOrPreviewPage(object sender, EventArgs e, string flagArgs)
        {
            if (!_applicationObject.ActiveDocument.Saved)
            {
                _applicationObject.ActiveDocument.Save();
            }
            var argString = this.PublishFlags.Count > 0 ? " -loc=INT -lang=INT -publish=" + string.Join(",", this.PublishFlags) : " -loc=INT -lang=INT";
            
            var startInfo = new ProcessStartInfo
            {
                CreateNoWindow = true,
                UseShellExecute = false,
                FileName = GetOptions().UnrealDocToolPath + "UnrealDocTool.exe",
                WindowStyle = ProcessWindowStyle.Hidden,
                Arguments = CurrentUDNDocView.SourceFilePath + argString + flagArgs
            };
            using (var exeProcess = Process.Start(startInfo))
            {
                exeProcess.WaitForExit();
            }
        }

        void PublishRecursive(object sender, EventArgs e)
        {
            if (!_applicationObject.ActiveDocument.Saved)
            {
                _applicationObject.ActiveDocument.Save();
            }
            
            var filePath = CurrentUDNDocView.SourceFilePath;
            var position = filePath.LastIndexOf('\\');
            var argString = this.PublishFlags.Count > 0 ? " -loc=INT -lang=INT -publish=" + string.Join(",", this.PublishFlags) : " -loc=INT -lang=INT";
            if (position >= 0)
            {
                filePath = filePath.Substring(0, position+1);
            }
            var startInfo = new ProcessStartInfo
            {
                CreateNoWindow = false,
                UseShellExecute = false,
                FileName = GetOptions().UnrealDocToolPath + "UnrealDocTool.exe",
                WindowStyle = ProcessWindowStyle.Hidden
            };

            startInfo.Arguments = filePath + argString + " -r";

            using (var exeProcess = Process.Start(startInfo))
            {
                exeProcess.WaitForExit();
            }
        }

        void PublishCommand(object sender, EventArgs e, string PublishFlag)
        {
            if (this.PublishFlags.Contains(PublishFlag))
            {
                this.PublishFlags.Remove(PublishFlag);
            }
            else
            {
                this.PublishFlags.Add(PublishFlag);
            }
            Settings.Default.LastAvailability = string.Join(",", this.PublishFlags);
            Settings.Default.Save();
        }

        void PublishCommandBeforeQueryStatus(object sender, EventArgs e, OleMenuCommand cmd, string PublishFlag)
        {
            var command = sender as OleMenuCommand;

            cmd.Checked = this.PublishFlags.Contains(PublishFlag);
            command.Text = PublishFlag;
        }

        #region Package Members

        /// <summary>
        /// Initialization of the package; this method is called right after the package is sited, so this is the place
        /// where you can put all the initialization code that rely on services provided by VisualStudio.
        /// </summary>
        protected override void Initialize()
        {
            Trace.WriteLine (string.Format(CultureInfo.CurrentCulture, "Entering Initialize() of: {0}", this.ToString()));
            base.Initialize();

            foreach (string publishFlag in Settings.SupportedAvailabilities)
            {
                PublishFlags.Add(publishFlag);
            }

            // Add our command handlers for menu (commands must exist in the .vsct file)
            var mcs = GetService(typeof(IMenuCommandService)) as IMenuCommandService;
            if (mcs != null)
            {
                var rdt = GetService(typeof(SVsRunningDocumentTable)) as IVsRunningDocumentTable;
                var ms = GetService(typeof(SVsShellMonitorSelection)) as IVsMonitorSelection;

                var componentModel = GetService(typeof(SComponentModel)) as IComponentModel;

                var shell = GetService(typeof(SVsUIShell)) as IVsUIShell;
                shell.UpdateCommandUI(1);

                rdtm = new UDNDocRunningTableMonitor(rdt, ms, componentModel.GetService<IVsEditorAdaptersFactoryService>(), shell, this);

                uint cookie;
                rdt.AdviseRunningDocTableEvents(rdtm, out cookie);

				// Create the command for the tool window
                CommandID publishRecursiveCmdID = new CommandID(GuidList.guidMarkdownPackageCmdSet, PkgCmdId.CmdidMarkdownPublishRecursiveCtxt);
                OleMenuCommand publishRecursiveSlnExpCommand = new OleMenuCommand(new EventHandler((sender, e) => PublishSelectedDocuments(sender, e,true)), publishRecursiveCmdID);
                AddSolutionExplorerCommand(mcs, publishRecursiveSlnExpCommand, rdtm);
                
                CommandID publishCommandID = new CommandID(GuidList.guidMarkdownPackageCmdSet, PkgCmdId.cmdidMarkdownPublishPagesCtxt);
                OleMenuCommand publishCommand = new OleMenuCommand(new EventHandler((sender, e) => PublishSelectedDocuments(sender, e, false)), publishCommandID);
                AddSolutionExplorerCommand(mcs, publishCommand, rdtm);

                CommandID publishPreviewCommandID = new CommandID(GuidList.guidMarkdownPackageCmdSet,PkgCmdId.cmdidMarkdownPreviewPage);
                OleMenuCommand publishPreviewCommand = new OleMenuCommand(new EventHandler((sender, e) => PublishOrPreviewPage(sender, e, " -p")), publishPreviewCommandID);
                AddCommandWithEnableFlag(mcs, publishPreviewCommand, rdtm);

                CommandID publishPageCommandID = new CommandID(GuidList.guidMarkdownPackageCmdSet,PkgCmdId.cmdidMarkdownPublishPage);
                OleMenuCommand publishPageCommand = new OleMenuCommand(new EventHandler((sender,e) => PublishOrPreviewPage(sender,e,"")), publishPageCommandID);
                AddCommandWithEnableFlag(mcs, publishPageCommand, rdtm);

                CommandID publishRecursiveCommandID = new CommandID(GuidList.guidMarkdownPackageCmdSet,PkgCmdId.cmdidMarkdownPublishRecursive);
                OleMenuCommand publishRecursiveCommand = new OleMenuCommand(new EventHandler(PublishRecursive),publishRecursiveCommandID );
                AddCommandWithEnableFlag(mcs,publishRecursiveCommand,rdtm);

                CommandID SetTextToBoldCommandID = new CommandID(GuidList.guidMarkdownPackageCmdSet, PkgCmdId.cmdidMarkdownBoldText);
                OleMenuCommand SetTextToBoldCommand = new OleMenuCommand(new EventHandler((sender, e) => SetTextStyle(sender, e, "**")), SetTextToBoldCommandID);
                AddCommandWithEnableFlag(mcs, SetTextToBoldCommand, rdtm);

                CommandID SetTextToItalicsCommandID = new CommandID(GuidList.guidMarkdownPackageCmdSet, PkgCmdId.cmdidMarkdownItalicText);
                OleMenuCommand SetTextToItalicCommand = new OleMenuCommand(new EventHandler((sender, e) => SetTextStyle(sender, e, "_")), SetTextToItalicsCommandID);
                AddCommandWithEnableFlag(mcs, SetTextToItalicCommand, rdtm);

                CommandID SetTextToCodeCommandID = new CommandID(GuidList.guidMarkdownPackageCmdSet, PkgCmdId.cmdidMarkdownCodeText);
                OleMenuCommand SetTextToCodeCommand = new OleMenuCommand(new EventHandler((sender, e) => SetTextStyle(sender, e, "`")), SetTextToCodeCommandID);
                AddCommandWithEnableFlag(mcs, SetTextToCodeCommand, rdtm);

                // Create the command for the tool window
                var showPreviewWindowCommandId = new CommandID(GuidList.guidMarkdownPackageCmdSet, PkgCmdId.cmdidMarkdownPreviewWindow);
                var showPreviewWindowCommand = new OleMenuCommand(ShowToolWindow, showPreviewWindowCommandId);
                AddCommandWithEnableFlag(mcs, showPreviewWindowCommand, rdtm);

                var openImagesCommandId = new CommandID(GuidList.guidMarkdownPackageCmdSet, PkgCmdId.cmdidMarkdownOpenImagesFolder);
                var openImagesCommand = new OleMenuCommand(OpenImageFolder, openImagesCommandId);
                AddCommandWithEnableFlag(mcs, openImagesCommand, rdtm);

                var navigateToComboCommandId = new CommandID(GuidList.guidMarkdownPackageCmdSet, PkgCmdId.cmdidMarkdownNavigateToCombo);
                var navigateToCombo = new OleMenuCommand(NavigateToComboSelected, navigateToComboCommandId);
                AddCommandWithEnableFlag(mcs, navigateToCombo, rdtm);

                var navigateToComboGetListCommandId = new CommandID(GuidList.guidMarkdownPackageCmdSet, PkgCmdId.cmdidMarkdownNavigateToComboGetList);
                var navigateToComboGetListCmd = new OleMenuCommand(NavigateToComboGetList, navigateToComboGetListCommandId);
                AddCommandWithEnableFlag(mcs, navigateToComboGetListCmd, rdtm);

                InitializePublishFlagButtons(mcs, rdtm);

                var rebuildDoxygenCommandId = new CommandID(GuidList.guidMarkdownPackageCmdSet, PkgCmdId.cmdidMarkdownRebuildDoxygenDatabase);
                var rebuildDoxygenCommand = new OleMenuCommand(RebuildDoxygen, rebuildDoxygenCommandId);
                AddCommandWithEnableFlag(mcs, rebuildDoxygenCommand, rdtm);

                var checkP4ImagesCommandId = new CommandID(GuidList.guidMarkdownPackageCmdSet, PkgCmdId.cmdidMarkdownCheckP4Images);
                var checkP4ImagesCommand = new OleMenuCommand(CheckP4Images, checkP4ImagesCommandId);
                AddCommandWithEnableFlag(mcs, checkP4ImagesCommand, rdtm);

                var disableParsingCommandId = new CommandID(GuidList.guidMarkdownPackageCmdSet, PkgCmdId.cmdidMarkdownDisableParsing);
                var disableParsingCommand = new OleMenuCommand(DisableDocumentParsing, disableParsingCommandId);
                AddCommandWithEnableFlag(mcs, disableParsingCommand, rdtm);

                var disableHighlightingCommandId = new CommandID(GuidList.guidMarkdownPackageCmdSet, PkgCmdId.cmdidMarkdownDisableHighlighting);
                var disableHighlightingCommand = new OleMenuCommand(DisableHighlightingMode, disableHighlightingCommandId);
                AddCommandWithEnableFlag(mcs, disableHighlightingCommand, rdtm);
            }

            Action reloadSettings = () =>
                {
                    string newDoxygenXmlPath = null;

                    if (!string.IsNullOrEmpty(GetOptions().DoxygenXmlCachePath))
                    {
                        newDoxygenXmlPath = GetOptions().DoxygenXmlCachePath;
                    }

                    DoxygenHelper.DoxygenXmlPath = newDoxygenXmlPath;


                    FileInfo newDoxygenExec = null;

                    if (!string.IsNullOrEmpty(GetOptions().DoxygenBinaryPath))
                    {
                        var doxygenExec = new FileInfo(GetOptions().DoxygenBinaryPath);
                        if (doxygenExec.Exists)
                        {
                            newDoxygenExec = doxygenExec;
                        }
                    }

                    DoxygenHelper.DoxygenExec = newDoxygenExec;
                };

            Action loadLastPublishingOptions = () =>
            {
                if (!Settings.Default.FirstRun)
                {
                    PublishFlags.Clear();
                    if (Settings.Default.LastAvailability != "")
                    {
                        var lastSettings = Settings.Default.LastAvailability.Split(',');
                        foreach (string lastSetting in lastSettings)
                        {
                            PublishFlags.Add(lastSetting);
                        }
                    }
                }
                else
                {
                    PublishFlags.Clear();
                    Settings.Default.FirstRun = false;
                    Settings.Default.Save();
                }
                
            };

            GetOptions().Changed += () => reloadSettings();
            reloadSettings();
            loadLastPublishingOptions();
        }

        private List<EnvDTE.Window> ActiveUDNDocuments()
        {
            List<EnvDTE.Window> UDNDocs = new List<EnvDTE.Window>();

            foreach (EnvDTE.Window window in _applicationObject.Windows)
            {
                if (window.Kind == "Document" && window.Document.Name.EndsWith(".udn"))
                {
                    UDNDocs.Add(window);
                }
            }
            return UDNDocs;
        }

        private List<EnvDTE.Window> UnsavedUDNDocuments()
        {
            List<EnvDTE.Window> UnsavedUDNDocs = new List<EnvDTE.Window>();
            foreach (EnvDTE.Window window in ActiveUDNDocuments())
            {
                if (!window.Document.Saved)
                {
                    UnsavedUDNDocs.Add(window);
                }
            }
            return UnsavedUDNDocs;
        }

        private void ReloadDocuments(List<EnvDTE.Window> doclist)
        {
            var activeDoc = _applicationObject.ActiveDocument.Name;
            foreach (EnvDTE.Window window in doclist)
            {
                window.SetFocus();
                SetFileAttributes(window);
                ITextBuffer _buffer = CurrentUDNDocView.TextEditorView.TextBuffer;
                ITextSnapshot _snap = CurrentUDNDocView.TextEditorView.TextSnapshot;
                var text = _snap.GetText(new Span(0, _snap.Length));
                _buffer.Delete(new Span(0, _snap.Length));
                _buffer.Insert(0, text);
                window.Document.Save();
            }
            SetLastActiveWindow(activeDoc);
        }
        
        private static FileAttributes RemoveAttribute(FileAttributes attributes, FileAttributes attributesToRemove)
        {
            return attributes & ~attributesToRemove;
        }

        private void SetFileAttributes(EnvDTE.Window window)
        {
            //In the event a file is set to read only in the project, we don't want to interrupt the process with messages to the user.
            if (window.Document.ReadOnly)
            {
                var fullpath = window.Document.Path + window.Document.Name;
                var attributes = File.GetAttributes(fullpath);
                if ((attributes & FileAttributes.ReadOnly) == FileAttributes.ReadOnly)
                {
                    attributes = RemoveAttribute(attributes, FileAttributes.ReadOnly);
                    File.SetAttributes(fullpath, attributes);
                }
            }
        }

        private static void DisableDocumentParsing(object sender, EventArgs e)
        {
            Settings.Default.DisableAllParsing = !Settings.Default.DisableAllParsing;
            Settings.Default.Save();
            UDNDocRunningTableMonitor.CurrentUDNDocView.ParsingResultsCache.TriggerAfterParsingEvent();
        }

        private static void DisableHighlightingMode(object sender, EventArgs e)
        {
            Settings.Default.DisableHighlighting = !Settings.Default.DisableHighlighting;
            Settings.Default.Save();
            UDNDocRunningTableMonitor.CurrentUDNDocView.ParsingResultsCache.TriggerAfterParsingEvent();
        }

        private void SetLastActiveWindow(string activeDoc)
        {
            foreach (EnvDTE.Window window in _applicationObject.Windows)
            {
                if (window.Kind == "Document" && window.Document.Name == activeDoc)
                {
                    window.SetFocus();
                }
            }
        }
        
        private void CheckP4Images(object sender, EventArgs e)
        {
            var doc = UDNDocRunningTableMonitor.CurrentUDNDocView.CurrentEMDocument;

            if (doc == null)
            {
                return;
            }

            var images = doc.GetUsedImagesPaths();

            try
            {
                PerforceHelper.AddToDepotIfNotExisting(images);
            }
            catch (PerforceLastConnectionSettingsAutoDetectionException exception)
            {
                MessageBox.Show(
                    MarkdownSharp.Language.Message("CouldNotAutodetectPerforceLastConnectionSettings", exception.Message),
                    MarkdownSharp.Language.Message("Error"),
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error,
                    MessageBoxDefaultButton.Button1);
            }
        }

        #endregion

        public MarkdownPreviewToolWindow GetMarkdownPreviewToolWindow(bool create)
        {
            return GetToolWindow(create);
        }

        public ObservableCollection<string> PublishFlags = new ObservableCollection<string>();

        void AddSolutionExplorerCommand(IMenuCommandService mcs, OleMenuCommand command, UDNDocRunningTableMonitor rdtm)
        {
            command.BeforeQueryStatus += (sender, args) =>
            {
                var cmd = sender as OleMenuCommand;
                if (cmd != null)
                {
                    var selectedItems = (Array)uih.SelectedItems;
                    if (null != selectedItems)
                    {
                        foreach (UIHierarchyItem selectedItem in selectedItems)
                        {
                            var projectItem = selectedItem.Object as ProjectItem;
                            var pathString = projectItem.Properties.Item("FullPath").Value.ToString();
                            if (!pathString.EndsWith(".udn"))
                            {
                                cmd.Visible = false;
                                break;
                            }
                            cmd.Visible = true;
                        }
                    }
                }
            };
            mcs.AddCommand(command);
        }

        void AddCommandWithEnableFlag(IMenuCommandService mcs, OleMenuCommand command, UDNDocRunningTableMonitor rdtm)
        {
            command.BeforeQueryStatus += new EventHandler((sender, args) =>
            {
                var cmd = sender as OleMenuCommand;
                cmd.Enabled = (cmd.CommandID.ID == PkgCmdId.cmdidMarkdownRebuildDoxygenDatabase)
                                  ? (!_doxygenIsRebuilding && rdtm.CommandsEnabled)
                                  : rdtm.CommandsEnabled;
                cmd.Visible = rdtm.CommandsEnabled;
                if (cmd.CommandID.ID == PkgCmdId.cmdidMarkdownDisableParsing)
                {
                    cmd.Checked = Settings.Default.DisableAllParsing;
                }
                if (cmd.CommandID.ID == PkgCmdId.cmdidMarkdownDisableHighlighting)
                {
                    cmd.Enabled = !Settings.Default.DisableAllParsing;  //Turn off the highlighting button when full parsing is disabled
                    cmd.Checked = Settings.Default.DisableHighlighting;
                }
                if (cmd.CommandID.ID == PkgCmdId.cmdidMarkdownPreviewWindow)
                {
                    cmd.Enabled = !Settings.Default.DisableAllParsing;
                }
                
            });

            mcs.AddCommand(command);
        }

        void InitializePublishFlagButton(IMenuCommandService mcs, UDNDocRunningTableMonitor rdtm, Guid pkgGuid, int cmdId, string PublishFlag)
        {
            CommandID PublishFlagCommandID = new CommandID(pkgGuid, cmdId);

            // public is necessary so we don't want to attach changing event
            OleMenuCommand PublishFlagCommand;

            if (PublishFlag == Settings.Default.PublicAvailabilitiesString.Trim())
            {
                PublishFlagCommand = new OleMenuCommand((sender, args) => { }, PublishFlagCommandID);
                PublishFlagCommand.BeforeQueryStatus += (sender, args) =>
                {
                    PublishFlagCommand.Checked = true;
                    PublishFlagCommand.Text = PublishFlag;
                };
            }
            else
            {
                PublishFlagCommand = new OleMenuCommand((sender, args) => PublishCommand(sender, args, PublishFlag), PublishFlagCommandID);
                PublishFlagCommand.BeforeQueryStatus += (sender, args) => PublishCommandBeforeQueryStatus(sender, args, PublishFlagCommand, PublishFlag);
            }

            AddCommandWithEnableFlag(mcs, PublishFlagCommand, rdtm);
        }

        void InitializePublishFlagButtons(IMenuCommandService mcs, UDNDocRunningTableMonitor rdtm)
        {
            //InitializePublishFlagButton(mcs, rdtm, GuidList.guidMarkdownPackageCmdSet, PkgCmdId.cmdidMarkdownPublishList, .Trim());

            var availabilityStrings = Settings.Default.SupportedAvailabilitiesString.Split(',').ToList();
            availabilityStrings.Insert(0, Settings.Default.PublicAvailabilitiesString);

            int i = 0;
            foreach (var availabilityString in availabilityStrings)
            {
                InitializePublishFlagButton(mcs, rdtm, GuidList.guidMarkdownPackageCmdSet, PkgCmdId.cmdidMarkdownPublishList + i + 1, availabilityStrings[i].Trim());
                ++i;
            }
        }
    }
}
