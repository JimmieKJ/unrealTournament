// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Xml;
using System.Xml.XPath;
using System.Linq;
using System.Threading;
using DotLiquid;

namespace MarkdownSharp.Doxygen
{
    using System.Runtime.Serialization;
    using System.Security;
    using System.Text.RegularExpressions;

    using MarkdownSharp.FileSystemUtils;

    public class DoxygenSymbol
    {
        public string SymbolName { get; private set; }
        public string CompoundId { get; private set; }
        public string RefId { get; private set; }
        public string Kind { get; private set; }
        public bool IsOverloadedMember { get; private set; }
        public string SourceLocation
        {
            get
            {
                if (_sourceLocation == null)
                {
                    using (var reader = new StreamReader(GetCompoundFilePath()))
                    {
                        var doc = new XmlDocument();
                        doc.Load(reader);
                        var nav = doc.CreateNavigator();

                        if (!IsDoxygenMember)
                        {
                            var locationNode = nav.SelectSingleNode("/doxygen/compounddef/location");

                            Debug.Assert(locationNode != null, "locationNode != null");

                            _sourceLocation = locationNode.GetAttribute("file", "");
                        }
                        else
                        {
                            var nonQualifiedName = SymbolName.Substring(
                                SymbolName.LastIndexOf("::", StringComparison.Ordinal) + 2);

                            var locationNode = nav.SelectSingleNode(string.Format("/doxygen/compounddef/sectiondef/memberdef/name[text()='{0}']", nonQualifiedName));

                            Debug.Assert(locationNode != null, "locationNode != null");

                            locationNode.MoveToParent();
                            locationNode = locationNode.SelectSingleNode("location");

                            Debug.Assert(locationNode != null, "locationNode != null");

                            _sourceLocation = locationNode.GetAttribute("file", "");
                        }
                    }
                }

                return _sourceLocation;
            }
        }

        public DoxygenSymbol(string symbolName, string refId, string compoundId, string kind)
        {
            SymbolName = symbolName;
            CompoundId = compoundId;
            RefId = refId;
            Kind = kind;
            IsOverloadedMember = false;
        }

        public bool IsDoxygenMember
        {
            get
            {
                switch (Kind)
                {
                    case "variable":
                    case "function":
                    case "enum":
                    case "property":
                    case "enumvalue":
                    case "define":
                    case "typedef":
                        return true;
                }

                return false;
            }
        }

        public void SetOverloadedMemberFlag()
        {
            IsOverloadedMember = true;
        }

        public string GetSymbolDescription(DoxygenHelper.DescriptionType descriptionType, string overloadSpecification = null)
        {
            using (var reader = XmlReader.Create(new StreamReader(GetCompoundFilePath())))
            {
                var doc = new XmlDocument();
                doc.Load(reader);
                var nav = doc.CreateNavigator();


                if (descriptionType == DoxygenHelper.DescriptionType.Full)
                {
                    return
                        GetTranslatedNode(nav, DoxygenHelper.GetDescriptionTypeString(DoxygenHelper.DescriptionType.Brief), overloadSpecification)
                        + " " +
                        GetTranslatedNode(nav, DoxygenHelper.GetDescriptionTypeString(DoxygenHelper.DescriptionType.Detailed), overloadSpecification);
                }

                return GetTranslatedNode(nav, DoxygenHelper.GetDescriptionTypeString(descriptionType), overloadSpecification);
            }
        }

        private string GetTranslatedNode(XPathNavigator nav, string nodeName, string overloadSpecification = null)
        {
            if (IsOverloadedMember)
            {
                if (overloadSpecification == null)
                {
                    throw new DoxygenHelper.DoxygenAmbiguousSymbolException(SymbolName, GetOverloadSpecificationOptions());
                }

                var nonQualifiedName = SymbolName.Substring(SymbolName.LastIndexOf("::", StringComparison.Ordinal) + 2);

                var members = nav.Select(string.Format("/doxygen/compounddef//memberdef/name[text()='{0}']", nonQualifiedName));

                while (members.MoveNext())
                {
                    var member = members.Current;

                    member.MoveToParent();

                    var templateTypeNodes = member.Select("templateparamlist/param/type");
                    var templateTypes = new List<string>();

                    while (templateTypeNodes.MoveNext())
                    {
                        templateTypes.Add(templateTypeNodes.Current.Value.Trim());
                    }

                    var argsstring = member.SelectSingleNode("argsstring").Value;

                    if (!overloadSpecification.Equals(GetOverloadSpecificationString(argsstring, templateTypes)))
                    {
                        continue;
                    }

                    var descNode = member.SelectSingleNode(nodeName);

                    Debug.Assert(descNode != null, "descNode != null");

                    return DoxygenToMarkdownTranslator.Translate(descNode, RefLinkGenerator).Trim();
                }

                throw new DoxygenHelper.DoxygenAmbiguousSymbolException(SymbolName, GetOverloadSpecificationOptions());
            }

            if (IsDoxygenMember)
            {
                var nonQualifiedName = SymbolName.Substring(SymbolName.LastIndexOf("::", StringComparison.Ordinal) + 2);

                nav = nav.SelectSingleNode(
                    string.Format("/doxygen/compounddef//memberdef/name[text()='{0}']", nonQualifiedName));

                Debug.Assert(nav != null, "nav != null");

                nav.MoveToParent();
            }
            else
            {
                nav = nav.SelectSingleNode("/doxygen/compounddef");
            }

            Debug.Assert(nav != null, "nav != null");

            var description = nav.SelectSingleNode(nodeName);

            Debug.Assert(description != null, "desc != null");

            return DoxygenToMarkdownTranslator.Translate(description, RefLinkGenerator).Trim();
        }

        private static string RefLinkGenerator(XmlNode refNode)
        {
            var symbol = DoxygenHelper.DoxygenDbCache.GetSymbolFromRefId(refNode.Attributes["refid"].Value);

            return string.Format("[API:{0}]", symbol.SymbolName);
        }

        private string GetCompoundFilePath()
        {
            // if there is an override
            var file = new FileInfo(Path.Combine(DoxygenHelper.DoxygenXmlPath, "tmp", "xml", CompoundId + ".xml"));

            return file.Exists ? file.FullName : Path.Combine(DoxygenHelper.DoxygenXmlPath, "xml", CompoundId + ".xml");
        }

        private string _sourceLocation;

        public List<string> GetOverloadSpecificationOptions()
        {
            var specs = new List<string>();

            using (var reader = XmlReader.Create(new StreamReader(GetCompoundFilePath())))
            {
                var doc = new XmlDocument();
                doc.Load(reader);
                var nav = doc.CreateNavigator();

                var nonQualifiedName = SymbolName.Substring(SymbolName.LastIndexOf("::", StringComparison.Ordinal) + 2);

                var members = nav.Select(string.Format("/doxygen/compounddef//memberdef/name[text()='{0}']", nonQualifiedName));

                while (members.MoveNext())
                {
                    var member = members.Current;

                    member.MoveToParent();

                    var templateTypeNodes = member.Select("templateparamlist/param/type");
                    var templateTypes = new List<string>();

                    while (templateTypeNodes.MoveNext())
                    {
                        templateTypes.Add(templateTypeNodes.Current.Value.Trim());
                    }

                    var argsstring = member.SelectSingleNode("argsstring").Value;

                    specs.Add(SymbolName + GetOverloadSpecificationString(argsstring, templateTypes));
                }
            }

            return specs;
        }

        private static string GetOverloadSpecificationString(string args, List<string> templateTypes)
        {
            return ((templateTypes.Count > 0 ? ("<" + string.Join(", ", templateTypes) + ">") : "") + args).Trim();
        }
    }

    public class DoxygenDbCache : IDisposable
    {
        public void ReloadFromDoxygenXmlDirectory()
        {
            var indexPath = Path.Combine(DoxygenHelper.DoxygenXmlPath, "xml", "index.xml");
            var triggerChange = _symbolMap.Count > 0;
            
            if (!File.Exists(indexPath))
            {
                throw new DoxygenHelper.InvalidDoxygenPath(Path.Combine(DoxygenHelper.DoxygenXmlPath, "xml"));
            }

            Clear();

            try
            {
                _directoryPath = Path.Combine(DoxygenHelper.DoxygenXmlPath, "xml");

                using (var reader = XmlReader.Create(new StreamReader(indexPath)))
                {
                    var doc = new XmlDocument();
                    doc.Load(reader);
                    var nav = doc.CreateNavigator();

                    var compoundsIterator = nav.Select("/doxygenindex/compound");

                    while (compoundsIterator.MoveNext())
                    {
                        var compound = compoundsIterator.Current;

                        switch (compound.GetAttribute("kind", ""))
                        {
                            case "struct":
                            case "class":
                            case "union":
                            case "namespace":
                            case "file":
                                LoadFromXmlCompound(compound);
                                break;
                            case "category":
                            case "page":
                            case "dir":
                                // ignore compound
                                break;
                            default:
                                //throw new InvalidOperationException(Language.Message("DoxygenCompoundKindNotSupported", compound.GetAttribute("kind", "")));
                                break;
                        }
                    }
                }
            }
            catch (Exception)
            {
                // If something went wrong clear symbol info.
                Clear();
                throw;
            }
            finally
            {
                if (triggerChange)
                {
                    DoxygenHelper.TriggerTrackedFilesChangedEvent();
                }
            }
        }
        
        public string GetSymbolDescription(string path, DoxygenHelper.DescriptionType descriptionType)
        {
            var symbol = GetSymbolFromPath(path);

            return symbol.GetSymbolDescription(descriptionType);
        }
        
        public string GetSymbolLocation(string path)
        {
            return _symbolMap[path].SourceLocation;
        }
        
        public void EnableTracking(string location)
        {
            _sourceFileTracking.EnableFileTracking(location);
        }
        
        public void DisableTracking(string location)
        {
            _sourceFileTracking.DisableFileTracking(location);
        }
        
        public DoxygenSymbol GetSymbolFromRefId(string refId)
        {
            if (!_refIdToSymbolMap.ContainsKey(refId))
            {
                return null;
            }

            return _refIdToSymbolMap[refId];
        }

        public DoxygenSymbol GetSymbolFromPath(string path)
        {
            if (!_symbolMap.ContainsKey(path))
            {
                throw new DoxygenHelper.SymbolNotFoundInDoxygenXmlFile(path);
            }

            var symbol = _symbolMap[path];

            return symbol;
        }
        
        public void Dispose()
        {
            Clear();
        }

        class SourceFileTracking
        {
            public void EnableFileTracking(string location)
            {
                if (_trackedLocations.ContainsKey(location))
                {
                    return;
                }

                var watcher = new FileWatcher(location);
                watcher.Changed += (sender, args) =>
                {
                    DoxygenHelper.RunDoxygen(new string[] { location },
                                                Path.Combine(DoxygenHelper.DoxygenXmlPath, "tmp"));
                    DoxygenHelper.TriggerTrackedFilesChangedEvent();
                };

                _trackedLocations[location] = watcher;
            }

            public void DisableFileTracking(string location)
            {
                if (!_trackedLocations.ContainsKey(location))
                {
                    return;
                }

                _trackedLocations[location].Dispose();

                _trackedLocations.Remove(location);
            }

            public void DisableAllTracking()
            {
                foreach (var location in _trackedLocations)
                {
                    location.Value.Dispose();
                }

                _trackedLocations.Clear();
            }

            private readonly Dictionary<string, FileWatcher> _trackedLocations = new Dictionary<string, FileWatcher>();
        }

        private void Clear()
        {
            _sourceFileTracking.DisableAllTracking();
            _symbolMap.Clear();
            _refIdToSymbolMap.Clear();
        }
        
        private void LoadFromXmlCompound(XPathNavigator compound)
        {
            var refId = compound.GetAttribute("refid", "");
            var compoundName = compound.SelectSingleNode("name").Value;

            AddSymbol(compoundName, refId, refId, compound.GetAttribute("kind", ""));

            var members = compound.Select("member");

            while (members.MoveNext())
            {
                var member = members.Current;
                var name = member.SelectSingleNode("name").Value;
                var memberKind = member.GetAttribute("kind", "");
                var memberRefId = member.GetAttribute("refid", "");

                switch (memberKind)
                {
                    case "variable":
                    case "function":
                    case "enum":
                    case "property":
                    case "enumvalue":
                    case "define":
                    case "typedef":
                        AddSymbol(compoundName + "::" + name, memberRefId, refId, memberKind);
                        break;
                    case "friend":
                        // ignore friend member
                        break;
                    default:
                        throw new InvalidOperationException(Language.Message("DoxygenMemberKindNotSupported", memberKind));
                }
            }
        }
        
        private void AddSymbol(string name, string refId, string compoundId, string kind)
        {
            var symbol = new DoxygenSymbol(name, refId, compoundId, kind);
            _refIdToSymbolMap[refId] = symbol;

            if (_symbolMap.ContainsKey(name))
            {
                symbol = _symbolMap[name];
                symbol.SetOverloadedMemberFlag();
            }
            else
            {
                _symbolMap[name] = symbol;
            }
        }



        private readonly SourceFileTracking _sourceFileTracking = new SourceFileTracking();
        private readonly Dictionary<string, DoxygenSymbol> _refIdToSymbolMap = new Dictionary<string, DoxygenSymbol>();

        private readonly Dictionary<string, DoxygenSymbol> _symbolMap = new Dictionary<string, DoxygenSymbol>();
        private string _directoryPath;
    }

    /// <summary>
    /// Class that provides functions to parse doxygen XML elements into markdown text.
    /// </summary>
    public static class DoxygenHelper
    {
        public enum DescriptionType
        {
            Detailed,
            Brief,
            Full
        }

        [Serializable]
        public class DoxygenExecMissing : Exception
        {
            public DoxygenExecMissing()
                : base(Language.Message("DoxygenExecMissing"))
            {
                
            }

            public DoxygenExecMissing(SerializationInfo info, StreamingContext context)
                : base(info, context)
            {
                
            }
        }

        [Serializable]
        public class InvalidDoxygenPath : Exception
        {
            public InvalidDoxygenPath()
                : this(null)
            {
            }

            public InvalidDoxygenPath(string path)
                : base(Language.Message("InvalidDoxygenPath", path))
            {
            }

            public InvalidDoxygenPath(SerializationInfo info, StreamingContext context)
                : base(info, context)
            {
            }
        }

        [Serializable]
        public class DoxygenAmbiguousSymbolException : Exception
        {
            public DoxygenAmbiguousSymbolException(string path, List<string> specOptions)
                : base(Language.Message("DoxygenAmbiguousSymbol", path, "\n\t" + string.Join("\n\t", specOptions)))
            {
            }

            public DoxygenAmbiguousSymbolException()
            {
            }

            public DoxygenAmbiguousSymbolException(SerializationInfo info, StreamingContext context)
                : base(info, context)
            {
            }
        }

        public class SymbolNotFoundInDoxygenXmlFile : Exception
        {
            public override string Message
            {
                get { return Language.Message("SymbolNotFoundInDoxygenCache", _path); }
            }

            public SymbolNotFoundInDoxygenXmlFile(string path)
            {
                _path = path;
            }

            public SymbolNotFoundInDoxygenXmlFile(XPathNavigator node, string relativePath)
            {
                var path = "";

                do
                {
                    path = Path.PathSeparator + node.Name + path;
                } while (node.MoveToParent());

                _path = path + Path.PathSeparator + relativePath;
            }

            private readonly string _path;
        }

        public static FileInfo DoxygenExec { get; set; }
        
        public static DoxygenDbCache DoxygenDbCache
        {
            get
            {
                if (_doxygenDbCache == null)
                {
                    _doxygenDbCache = new DoxygenDbCache();
                    try
                    {
                        _doxygenDbCache.ReloadFromDoxygenXmlDirectory();
                    }
                    catch (Exception)
                    {
                        // if something went wrong, don't remember the cache
                        _doxygenDbCache.Dispose();
                        _doxygenDbCache = null;
                        throw;
                    }
                }

                return _doxygenDbCache;
            }
        }
        
        public static string DoxygenXmlPath
        {
            get
            {
                if (_doxygenXmlPath == null)
                {
                    throw new InvalidDoxygenPath("null");
                }

                return _doxygenXmlPath;
            }
            set
            {
                if (_doxygenXmlPath != value)
                {
                    _doxygenXmlPath = value;

                    InvalidateDbCache();
                }
            }
        }

        public static string DoxygenInputFilter { get; set; }

        public delegate void RebuildingDoxygenProgressHandler(double progress);
        public delegate void DoxygenRebuildingFinishedHandler();
        public delegate void TrackedFilesChangedHandler();

        public static event RebuildingDoxygenProgressHandler RebuildingDoxygenProgressChanged;
        public static event DoxygenRebuildingFinishedHandler DoxygenRebuildingFinished;
        public static event TrackedFilesChangedHandler TrackedFilesChanged;

        public static void RunDoxygen(IEnumerable<string> inputs, string outputDir, DataReceivedEventHandler stdoutDataReveiver = null)
        {
            var outputDirInfo = new DirectoryInfo(outputDir);

            if (!outputDirInfo.Exists)
            {
                outputDirInfo.Create();
            }

            var configPath = Path.Combine(outputDir, "doxygen.config");

            var template =
                new DoxygenConfig(
                    string.Join(" \\" + Environment.NewLine, inputs.Select(input => "\"" + input + "\"")),
                    "\"" + outputDir + "\"");

            File.WriteAllText(configPath, template.TransformText());

            var doxygenProcess = new Process
            {
                StartInfo =
                {
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    FileName = DoxygenExec.FullName,
                    Arguments = configPath,
                    CreateNoWindow = true
                },

                EnableRaisingEvents = true
            };

            // The progress estimation is not accurate. It divides the process into 3 equal
            // parts: first parsing input files, second building group list and third finished.
            // Each of these parts may last variable amount of time. It can be improved, but
            // it will require some research and implementation. The += 0.0000333 on every
            // stdout newline gives the impression of progress. If it is not enough, please
            // TTP it.
            double lastProgress = 0.0;

            doxygenProcess.OutputDataReceived += (sender, args) =>
                {
                    if (args.Data == null)
                    {
                        ProgressChanged(1.0);
                        return;
                    }

                    switch (args.Data)
                    {
                        case "Building group list...":
                            lastProgress = 1.0 / 3.0;
                            break;
                        case "Generating XML output...":
                            lastProgress = 2.0 / 3.0;
                            break;
                        case "finished...":
                            // when it's finished then return
                            ProgressChanged(1.0);
                            return;
                        default:
                            lastProgress += 0.0000333;
                            break;
                    }

                    if (lastProgress > 0.0 && lastProgress < 1.0)
                    {
                        ProgressChanged(lastProgress);
                    }

                    if (lastProgress >= 1.0)
                    {
                        // almost finished
                        ProgressChanged(0.9999999);
                    }
                };

            if (stdoutDataReveiver != null)
            {
                doxygenProcess.OutputDataReceived += stdoutDataReveiver;
            }

            if (doxygenProcess.Start())
            {
                doxygenProcess.BeginOutputReadLine();
                doxygenProcess.WaitForExit();
            }
        }

        public static void RebuildCache(string sourcePath, DataReceivedEventHandler stdoutDataReveiver = null, bool backgroundThread = false)
        {
            lock (_rebuildingInProgress)
            {
                if ((bool) _rebuildingInProgress)
                {
                    throw new InvalidOperationException(Language.Message("WaitUntilTheEarlierDoxygenRebuild"));
                }
            }

            var sourceDir = new DirectoryInfo(sourcePath);
            var outputDir = new DirectoryInfo(DoxygenXmlPath);
            DirectoryInfo randomTmpDir = null;

            if (outputDir.Parent == null)
            {
                throw new ArgumentException(Language.Message("DoxygenOutputDirShouldHaveAParentDir"));
            }

            while (randomTmpDir == null || randomTmpDir.Exists)
            {
                randomTmpDir = new DirectoryInfo(outputDir.Parent.FullName + Guid.NewGuid());
            }

            if (!sourceDir.Exists)
            {
                throw new ArgumentException(Language.Message("BadParameters"));
            }

            var foldersToParse = new string[] {"Runtime", "Developer", "Editor"};

            if (DoxygenExec == null || !DoxygenExec.Exists)
            {
                throw new DoxygenExecMissing();
            }

            Action runDoxygen = () =>
                {
                    lock (_rebuildingInProgress)
                    {
                        _rebuildingInProgress = true;
                    }

                    RunDoxygen(
                        foldersToParse.Select(folder => Path.Combine(sourceDir.FullName, folder)),
                        randomTmpDir.FullName,
                        stdoutDataReveiver);

                    // Try 5 times with 0.5 second sleep to let the os refresh after deletion.
                    if (outputDir.Exists)
                    {
                        try
                        {
                            RetryIfException(
                                () => outputDir.Delete(true),
                                e => e is IOException || e is UnauthorizedAccessException || e is SecurityException);
                        }
                        catch (DirectoryNotFoundException)
                        {
                            // This shouldn't happen (because I'm checking if the dir exists).
                            // Nevertheless it happened during debugging session, so to be safe,
                            // we just assume that dir is deleted then and ignore the exception.
                        }
                    }

                    RetryIfException(() => randomTmpDir.MoveTo(outputDir.FullName), e => e is IOException);

                    lock (_rebuildingInProgress)
                    {
                        _rebuildCacheThread = null;
                        _rebuildingInProgress = false;
                    }

                    if (DoxygenRebuildingFinished != null)
                    {
                        DoxygenRebuildingFinished();
                    }
                };

            if (backgroundThread)
            {
                _rebuildCacheThread = new Thread(new ThreadStart(runDoxygen));
                _rebuildCacheThread.Start();
            }
            else
            {
                runDoxygen();
            }
        }

        private static void RetryIfException(Action action, Func<Exception, bool> exceptionPredicate, int retryCount = 5, int timeOut = 500)
        {
            var tryId = 0;

            while (true)
            {
                try
                {
                    ++tryId;

                    action();
                    break;
                }
                catch (Exception e)
                {
                    if (!exceptionPredicate(e))
                    {
                        throw;
                    }

                    if (tryId == retryCount)
                    {
                        // rethrow after retryCount attempts
                        throw;
                    }

                    Thread.Sleep(timeOut);
                }
            }
        }

        public static void SetTrackedSymbols(HashSet<string> symbolPaths)
        {
            if (DoxygenExec == null)
            {
                return;
            }

            // obtain symbols locations set
            var symbolsLocations = new HashSet<string>();
            symbolPaths.Select(symbol => DoxygenDbCache.GetSymbolLocation(symbol)).ToList().ForEach(
                location =>
                {
                    if (!symbolsLocations.Contains(location))
                    {
                        symbolsLocations.Add(location);
                    }
                });


            var locationsToDisableTracking = new HashSet<string>(_trackedLocations);
            locationsToDisableTracking.ExceptWith(symbolsLocations);

            var locationsToEnableTracking = new HashSet<string>(symbolsLocations);
            locationsToEnableTracking.ExceptWith(_trackedLocations);

            _trackedLocations = symbolsLocations;

            foreach (var locationToDisable in locationsToDisableTracking)
            {
                DoxygenDbCache.DisableTracking(locationToDisable);
            }

            foreach (var locationToEnable in locationsToEnableTracking)
            {
                DoxygenDbCache.EnableTracking(locationToEnable);
            }
        }
        
        public static string GetDescriptionTypeString(DescriptionType descriptionType)
        {
            switch (descriptionType)
            {
                case DescriptionType.Brief:
                    return "briefdescription";
                case DescriptionType.Detailed:
                    return "detaileddescription";
                default:
                    throw new ArgumentException("Unsupported description type.");
            }
        }
        
        public static void InvalidateDbCache()
        {
            if (_doxygenDbCache == null)
            {
                return;
            }

            _doxygenDbCache.Dispose();
            _doxygenDbCache = null;
        }
        
        public static void TriggerTrackedFilesChangedEvent()
        {
            if (TrackedFilesChanged != null)
            {
                TrackedFilesChanged();
            }
        }

        private static readonly Regex SymbolPathPattern = new Regex(@"^(?<symbolPath>[\w:]+)(?<overloadSpecification>.*)$");
        
        public static string GetSymbolDescriptionAndCatchExceptionIntoMarkdownErrors(string path, string originalUserEntry, DescriptionType descriptionType,
                                                         TransformationData transformationDataForThisRun, Markdown parser, HashSet<string> foundSymbolPaths = null)
        {
            var outputText = "";
            var isError = false;
            var isInfo = false;
            var errorId = 0;

            try
            {
                var match = SymbolPathPattern.Match(path);

                if (!match.Success)
                {
                    throw new InvalidDoxygenPath(path);
                }

                var symbolPath = match.Groups["symbolPath"].Value;
                var overloadSpec = match.Groups["overloadSpecification"].Value;

                var symbol = DoxygenDbCache.GetSymbolFromPath(symbolPath);

                if (foundSymbolPaths != null)
                {
                    foundSymbolPaths.Add(symbolPath);
                }

                if (symbol.IsOverloadedMember)
                {
                    if (string.IsNullOrWhiteSpace(overloadSpec))
                    {
                        throw new DoxygenAmbiguousSymbolException(path, symbol.GetOverloadSpecificationOptions());
                    }

                    outputText = symbol.GetSymbolDescription(descriptionType, overloadSpec);
                }
                else
                {
                    outputText = symbol.GetSymbolDescription(descriptionType);
                }
            }
            catch (Exception e)
            {
                errorId = transformationDataForThisRun.ErrorList.Count;
                transformationDataForThisRun.ErrorList.Add(
                    Markdown.GenerateError(
                        Language.Message("DoxygenQueryError", e.Message),
                        MessageClass.Error, originalUserEntry, errorId, transformationDataForThisRun));

                isError = true;

                // rethrowing if not known exception
                if (!(e is InvalidDoxygenPath) && !(e is SymbolNotFoundInDoxygenXmlFile) && !(e is DoxygenAmbiguousSymbolException))
                {
                    throw;
                }
            }

            if (parser.ThisIsPreview && (isInfo || isError))
            {
                return Templates.ErrorHighlight.Render(Hash.FromAnonymousObject(
                    new
                    {
                        errorId = errorId,
                        errorText = outputText
                    }));
            }

            return outputText;
        }

        
        
        private static void ProgressChanged(double progress)
        {
            if (RebuildingDoxygenProgressChanged != null)
            {
                RebuildingDoxygenProgressChanged(progress);
            }
        }

        private static DoxygenDbCache _doxygenDbCache;
        private static Thread _rebuildCacheThread;
        private static object _rebuildingInProgress = false; // object type, because locks requiring reference objects
        private static HashSet<string> _trackedLocations = new HashSet<string>();
        private static string _doxygenXmlPath = "";
    }

    partial class DoxygenConfig
    {
        public DoxygenConfig(string input, string outputDir)
        {
            this.input = input;
            this.outputDir = outputDir;
            this.inputFilter = DoxygenHelper.DoxygenInputFilter;

            if (inputFilter == null)
            {
                throw new InvalidOperationException("Doxygen input filter is missing.");
            }
        }

        private readonly string input;
        private readonly string outputDir;
        private readonly string inputFilter;
    }

}
