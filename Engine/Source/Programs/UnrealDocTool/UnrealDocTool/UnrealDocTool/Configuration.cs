// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using MarkdownSharp;
using UnrealDocTool.Params;
using UnrealDocTool.Properties;
using System.Windows.Forms;

namespace UnrealDocTool
{
    using System.IO;

    public enum LogVerbosity
    {
        Info,
        Warn,
        Error
    }

    public enum LocalizationLanguage
    {
        INT
    }

    public enum OutputFormat
    {
        HTML,
        PDF
    }

    public class Configuration
    {
        public Configuration(string[] supportedAvailability, string publicAvailabilityFlag, string[] supportedLanguages, bool runningGUI)
        {
            ParamsManager = new ParamsManager();

            var groupDescBoth = new ParamsDescriptionGroup(() => Language.Message("ParamGroupDescBoth"));
            var groupDescDir = new ParamsDescriptionGroup(() => Language.Message("ParamGroupDescDir"));
            var groupDescFile = new ParamsDescriptionGroup(() => Language.Message("ParamGroupDescFile"));

            // looking for localization param, before any text display
            LocParam = new EnumParam<LocalizationLanguage>(
                "loc",
                "Localization language",
                () => Language.Message("ParamDescLoc", string.Join(", ", Language.AvailableLangIDs)),
                LocalizationLanguage.INT,
                groupDescBoth,
                arg =>
                {
                    // need to update it right after parsing to allow proper messages
                    Language.LangID = arg;
                });

            PublishFlagsParam = new MultipleChoiceParam(
                "publish",
                "Publish flags",
                supportedAvailability,
                Enumerable.Range(0, supportedAvailability.Length),
                () => Language.Message("ParamDescPublish", publicAvailabilityFlag, string.Join(", ", supportedAvailability)),
                groupDescBoth);
            

            OutputFormatParam = new EnumParam<OutputFormat>(
                "outputFormat",
                "Output format",
                () => Language.Message("ParamDescOutputFormat"),
                OutputFormat.HTML,
                groupDescBoth);
            

            LangParam = new MultipleChoiceParam(
                "lang",
                "Languages",
                supportedLanguages,
                Enumerable.Range(0, supportedLanguages.Length),
                () => Language.Message("ParamDescLang", string.Join(", ", supportedLanguages)),
                groupDescDir);

            LinksToAllLangs = new Flag(
                "linksToAllLangs", "Links to all langs", () => Language.Message("ParamDescLinksToAllLangs"), groupDescDir);

            if (runningGUI)
            {
                PublishFlagsParam.Parse(User.Default.SupportedAvailability);
                OutputFormatParam.Parse(User.Default.OutputFormat);
                LangParam.Parse(User.Default.SupportedLanguages);
            }

            TemplateParam = new StringParam(
                "t", "Default template", () => Language.Message("ParamDescT"), runningGUI ? User.Default.DefaultTemplate : Settings.Default.DefaultTemplate, groupDescBoth);
            OutputParam = new StringParam(
                "o", "Output directory", () => Language.Message("ParamDescO"), runningGUI ? User.Default.OutputDirectory : Settings.Default.OutputDirectory, groupDescBoth);
            SourceParam = new StringParam(
                "s", "Source directory", () => Language.Message("ParamDescS"), runningGUI ? User.Default.SourceDirectory : Settings.Default.SourceDirectory, groupDescBoth);
            LogVerbosityParam = new EnumParam<LogVerbosity>(
                "v", "Logging verbosity", () => Language.Message("ParamDescV"), LogVerbosity.Info, groupDescBoth);
            DoxygenCacheParam = new StringParam(
                "doxygenCache", "Doxygen cache location", () => Language.Message("ParamDescDoxygenCache"), "", groupDescBoth);
            RebuildDoxygenCacheParam = new StringParam(
                "rebuildDoxygenCache",
                "If you want to rebuild Doxygen cache put here doxygen exec path",
                () => Language.Message("ParamDescRebuildDoxygenCache"),
                "", groupDescBoth);

            var absoluteSourceDir = new DirectoryInfo(Settings.Default.SourceDirectory).FullName;
            
            PathPrefixParam = new PathParam(
                "pathPrefix", "Path prefix", () => Language.Message("ParamDescPathPrefix"),
                runningGUI ? User.Default.PathPrefix : absoluteSourceDir,
                runningGUI ? User.Default.SourceDirectory : absoluteSourceDir,
                null, groupDescBoth);

            if (runningGUI)
            {
                HelpFlag = new Flag("h", "", () => Language.Message("ParamDescH"), User.Default.HelpFlag);
                LogOnlyFlag = new Flag("log", "Log Only", () => Language.Message("ParamDescLog"), User.Default.LogOnlyFlag, groupDescBoth);
                PreviewFlag = new Flag("p", "Preview", () => Language.Message("ParamDescP"), User.Default.PreviewFlag, groupDescFile);
                CleanFlag = new Flag("clean", "Clean", () => Language.Message("ParamDescClean"), User.Default.CleanFlag, groupDescBoth);
                LogVerbosityParam.Parse(User.Default.LogVerbosityParam);
            }
            else
            {
                HelpFlag = new Flag("h", "", () => Language.Message("ParamDescH"));
                LogOnlyFlag = new Flag("log", "Log Only", () => Language.Message("ParamDescLog"), groupDescBoth);
                PreviewFlag = new Flag("p", "Preview", () => Language.Message("ParamDescP"),  groupDescFile);
                CleanFlag = new Flag("clean", "Clean", () => Language.Message("ParamDescClean"), groupDescBoth);
            }

            ParamsManager.Add(
                new List<Param>
                    {
                        LocParam,
                        PathPrefixParam,
                        PublishFlagsParam,
                        OutputFormatParam,
                        LangParam,
                        LinksToAllLangs,
                        TemplateParam,
                        OutputParam,
                        SourceParam,
                        LogVerbosityParam,
                        RebuildDoxygenCacheParam,
                        DoxygenCacheParam,
                        HelpFlag,
                        LogOnlyFlag,
                        PreviewFlag,
                        CleanFlag
                    });

            PathsSpec = new StringParam(
                "PathsSpecifier",
                "Semicolon separated documentation source file or directory paths.",
                () => Language.Message("PathSpecParamDesc"),
                runningGUI ? Path.GetFileName(User.Default.PathsSpec) : "",
                null);

            ParamsManager.SetMainParam(PathsSpec);
        }

        public MultipleChoiceParam PublishFlagsParam { get; private set; }
        public EnumParam<OutputFormat> OutputFormatParam { get; private set; }
        public EnumParam<LocalizationLanguage> LocParam { get; private set; }
        public MultipleChoiceParam LangParam { get; private set; }
        public Flag LinksToAllLangs { get; private set; }
        public StringParam TemplateParam { get; private set; }
        public StringParam OutputParam { get; private set; }
        public StringParam SourceParam { get; private set; }
        public PathParam PathPrefixParam { get; private set; }
        public EnumParam<LogVerbosity> LogVerbosityParam { get; private set; }
        public StringParam DoxygenCacheParam { get; private set; }
        public StringParam RebuildDoxygenCacheParam { get; private set; }
        public Flag HelpFlag { get; private set; }
        public Flag RecursiveFlag { get; private set; }
        public Flag LogOnlyFlag { get; private set; }
        public Flag PreviewFlag { get; private set; }
        public Flag CleanFlag { get; private set; }
        public StringParam PathsSpec { get; private set; }

        public ParamsManager ParamsManager { get; private set; }
    }

}
