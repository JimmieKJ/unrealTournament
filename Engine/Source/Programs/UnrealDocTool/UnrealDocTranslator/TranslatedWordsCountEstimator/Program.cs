// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.RegularExpressions;
using Perforce.P4;

namespace TranslatedWordsCountEstimator
{
    using System.IO;
    using System.Xml;

    class Program
    {
        private static readonly Regex ChangelistNumberPattern = new Regex(@"^INTSourceChangelist\:(?<number>\d+)\n", RegexOptions.Multiline | RegexOptions.Compiled);

        private const string DefaultDepotPath = "//depot/UE4";
        private const string DefaultDocumentationPath = "/Engine/Documentation/Source/....udn";

        private enum P4Setting
        {
            P4PORT,
            P4USER,
            P4CLIENT
        };

        static void Main(string[] args)
        {
            if (args.Length < 1)
            {
                Console.Out.WriteLine("Usage TranslatedWordsCountEstimator <report CSV file> [<p4 server address> <user name> <workspace name> [<password>]]");
                return;
            }

            try
            {
                var reportFilePath = args[0];
                var p4Port = GetP4Setting(args, 1, P4Setting.P4PORT);
                var p4User = GetP4Setting(args, 2, P4Setting.P4USER);
                var p4Workspace = GetP4Setting(args, 3, P4Setting.P4CLIENT);

                Console.Out.Write("Connecting to P4 ");
                var p4Server = new Server(new ServerAddress(p4Port));
                var p4Repository = new Repository(p4Server);
                var p4Connection = p4Repository.Connection;

                p4Connection.UserName = p4User;

                p4Connection.Client = new Client { Name = p4Workspace };

                p4Connection.Connect(null);

                if (args.Length >= 5)
                {
                    p4Connection.Login(args[4]);
                }

                Console.WriteLine("[done]");

                Console.Out.Write("Getting latest changelist: ");
                var latestChangelist = p4Repository.GetChangelists(new Options { { "-m", "1" } }).First();
                Console.Out.WriteLine(latestChangelist.Id);

                var currentDate = DateTime.Now;

                // detect depot based on EXE path
                string DepotPath = DefaultDepotPath;
                IList<FileMetaData> ExeMetaData = p4Repository.GetFileMetaData(new Options(), new FileSpec(null, new ClientPath(System.Reflection.Assembly.GetExecutingAssembly().GetModules()[0].FullyQualifiedName), null, null));
                if( ExeMetaData.Count > 0 ) 
                {
                    string Path = ExeMetaData[0].DepotPath.ToString();
                    int i = Path.IndexOf("/Engine/Binaries");
                    if (i != -1)
                    {
                        DepotPath = Path.Substring(0, i);
                    }
                }

                string DocumentationDepotPath = DepotPath + DefaultDocumentationPath;
                var filesSpec = new List<FileSpec> {
                                        new FileSpec(
                                            new DepotPath(DocumentationDepotPath),
                                            new ChangelistIdVersion(latestChangelist.Id))
                                    };

                Console.Out.Write("Synchronizing " + DocumentationDepotPath);
                p4Connection.Client.SyncFiles(filesSpec, null);
                Console.Out.WriteLine("[done]");

                Console.Out.Write("Getting files info ");
                var files = p4Repository.GetFileMetaData(filesSpec, null).Where(md => !ExcludedAction(md.HeadAction)).ToList();
                Console.Out.WriteLine("[done]");

                var locLangs = DetectLocalizationLanguages(files);
                Console.Out.WriteLine("Found languages: [{0}]", string.Join(", ", locLangs));

                Console.Out.Write("Counting total words: ");
                var intFiles = files.Where(f => f.DepotPath.Path.ToLower().EndsWith(".int.udn")).ToList();
                var intWordCount = new WordCountCache();
                var totalWords = (double) intFiles.Sum(intFile => intWordCount.Get(intFile));
                Console.Out.WriteLine(totalWords);

                var data = new Dictionary<string, double> { { "int", totalWords } };

                foreach (var lang in locLangs)
                {
                    var locFiles =
                        files.Where(f => f.DepotPath.Path.ToLower().EndsWith("." + lang.ToLower() + ".udn")).ToList();
                    var wordCount = totalWords;

                    Console.Out.Write("Calculating {0} language: ", lang, 0);

                    foreach (var intFile in intFiles)
                    {
                        var filteredLocFiles =
                            locFiles.Where(
                                file =>
                                file.DepotPath.Path.ToLower() == UDNFilesHelper.ChangeLanguage(intFile.DepotPath.Path, lang.ToUpper()).ToLower())
                                    .ToArray();

                        if (filteredLocFiles.Count() != 1)
                        {
                            wordCount -= intWordCount.Get(intFile);
                            continue;
                        }

                        var locFile = filteredLocFiles[0];

                        var match = ChangelistNumberPattern.Match(UDNFilesHelper.Get(locFile));
                        IList<DepotFileDiff> diffs = null;

                        DateTime? diffDate = null;

                        if (match.Success)
                        {
                            var cl = int.Parse(match.Groups["number"].Value);
                            if (intFile.HeadChange > cl)
                            {
                                diffDate = p4Repository.GetChangelist(cl).ModifiedDate;
                            }
                        }
                        else
                        {
                            if (intFile.HeadTime > locFile.HeadTime)
                            {
                                diffDate = locFile.HeadTime;
                            }
                        }

                        if (!diffDate.HasValue)
                        {
                            continue;
                        }

                        diffs = p4Repository.GetDepotFileDiffs(
                            string.Format("{0}@{1}", intFile.DepotPath.Path, diffDate.Value.ToString("yyyy\\/MM\\/dd\\:HH\\:mm\\:ss")),
                            new FileSpec(intFile.DepotPath, new ChangelistIdVersion(latestChangelist.Id)).ToString(),
                            new Options() { { "-d", "" }, { "-u", "" } });

                        if (diffs != null && diffs[0] != null && string.IsNullOrEmpty(diffs[0].LeftFile.DepotPath.Path))
                        {
                            diffs = p4Repository.GetDepotFileDiffs(
                                string.Format("{0}@{1}", TraceP4Location(p4Repository,
                                    new FileSpec(intFile.DepotPath, new ChangelistIdVersion(latestChangelist.Id)), diffDate.Value),
                                    diffDate.Value.ToString("yyyy\\/MM\\/dd\\:HH\\:mm\\:ss")),
                                new FileSpec(intFile.DepotPath, new ChangelistIdVersion(latestChangelist.Id)).ToString(),
                                new Options() { { "-d", "" }, { "-u", "" } });
                        }

                        if (diffs[0].Type == DiffType.Identical)
                        {
                            continue;
                        }

                        var lines = diffs[0].Diff.Split('\n');

                        var addedWords = UDNFilesHelper.GetWordCount(string.Join("\n", lines.Where(l => l.StartsWith(">"))));
                        var removedWords = UDNFilesHelper.GetWordCount(string.Join("\n", lines.Where(l => l.StartsWith("<"))));

                        wordCount -= addedWords;
                        wordCount -= 0.05 * removedWords;
                    }

                    Console.Out.Write("{1:0.00}\n", lang, wordCount);
                    data.Add(lang.ToLower(), wordCount);
                }

                p4Connection.Disconnect(null);

                var reportFile = new ReportFile(reportFilePath);
                reportFile.AppendData(currentDate, data);
                reportFile.Save();
            }
            catch (InvalidCastException e)
            {
                Console.Error.WriteLine("Unrecognized error: {0}\nCall stack: {1}\n", e.Message, e.StackTrace);
            }
        }

        private static PathSpec TraceP4Location(Repository p4Repository, FileSpec originalFile, DateTime date)
        {
            var history = p4Repository.GetFileHistory(
                new Options(), originalFile);

            foreach (var historyFile in history)
            {
                if (historyFile.Date < date)
                {
                    return historyFile.DepotPath;
                }
            }

            return null;
        }

        private static string GetP4Setting(string[] args, int argId, P4Setting settingType)
        {
            return args.Length > argId ? args[argId] : AutoDetectP4Setting(settingType);
        }

        private readonly static Dictionary<P4Setting, string> P4LastConnectionSettingsCache = new Dictionary<P4Setting, string>();

        private static string AutoDetectP4Setting(P4Setting type)
        {
            Console.Out.WriteLine("Auto detection of {0} from P4V settings.", type);

            string value;
            if (P4LastConnectionSettingsCache.TryGetValue(type, out value))
            {
                return value;
            }

            FillP4LastConnectionSettingsCache();

            return P4LastConnectionSettingsCache[type];
        }

        private static void FillP4LastConnectionSettingsCache()
        {
            var p4AppSettingsXmlDoc = new XmlDocument();

            try
            {
                p4AppSettingsXmlDoc.Load(
                    Path.Combine(
                        Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
                        ".p4qt",
                        "ApplicationSettings.xml"));
            }
            catch (FileNotFoundException e)
            {
                throw new InvalidOperationException("Could not find ApplicationSettings.xml for P4V.", e);
            }

            var nav = p4AppSettingsXmlDoc.CreateNavigator();

            var lastConnectionNode = nav.SelectSingleNode("/PropertyList/PropertyList[@varName='Connection']/String[@varName='LastConnection']");

            if (lastConnectionNode == null)
            {
                throw new InvalidOperationException("Cannot find LastConnection setting in P4 ApplicationSettings.");
            }

            var lastConnectionSettings =
                lastConnectionNode
                    .InnerXml.Split(',')
                    .Select(e => e.Trim())
                    .ToArray();

            P4LastConnectionSettingsCache[P4Setting.P4PORT] = lastConnectionSettings[0];
            P4LastConnectionSettingsCache[P4Setting.P4USER] = lastConnectionSettings[1];
            P4LastConnectionSettingsCache[P4Setting.P4CLIENT] = lastConnectionSettings[2];
        }

        private static string[] DetectLocalizationLanguages(IEnumerable<FileMetaData> files)
        {
            var langs = new HashSet<string>();

            foreach (var file in files)
            {
                string prefix;
                string lang;
                string postfix;

                UDNFilesHelper.SplitByLang(file.DepotPath.Path, out prefix, out lang, out postfix);

                if (!string.IsNullOrWhiteSpace(lang) && lang.ToUpper() != "INT" && !langs.Contains(lang.ToUpper()))
                {
                    langs.Add(lang.ToUpper());
                }
            }

            return langs.ToArray();
        }

        private static bool ExcludedAction(FileAction fileAction)
        {
            switch (fileAction)
            {
                case FileAction.Delete:
                case FileAction.DeleteFrom:
                case FileAction.DeleteInto:
                case FileAction.MoveDelete:
                case FileAction.Purge:
                    return true;
                default:
                    return false;
            }
        }
    }
}
