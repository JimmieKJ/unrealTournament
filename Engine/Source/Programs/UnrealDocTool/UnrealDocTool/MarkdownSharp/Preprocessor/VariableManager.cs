// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace MarkdownSharp.Preprocessor
{
    using System;
    using System.Collections.Generic;
    using System.Linq;
    using System.Text.RegularExpressions;
    using System.IO;

    using DotLiquid;

    using MarkdownSharp.Doxygen;
    using MarkdownSharp.EpicMarkdown;

    public class VariableManager
    {
        public static readonly Regex InLineVariableOrMetadata = new Regex(@"(?<=^[ ]{0,3}(\S.*)?)(?<!http://[^) ]+)((\%(?<pathWithColon>(?<path>[^%: \s]*?):)?(?<variableName>([_ \(\):a-zA-Z0-9\<\>*;]+?))\%))", RegexOptions.IgnorePatternWhitespace | RegexOptions.Multiline | RegexOptions.Compiled);
        private static readonly Regex VariableDefinitionPattern = new Regex(@"
                (
                    ^\[VAR:(?<variableName>[_a-zA-Z0-9]+?)\]$\n
                        (?<variableContent>(.|\n)*?)\n
                    ^\[/VAR(:\2)?\]$
                )
                |
                (
                    ^\[VAR:(?<variableName>[_a-zA-Z0-9]+?)\](?<variableContent>.*?)\[/VAR(:\2)?\]$
                )
            ", RegexOptions.Compiled | RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace);

        public string ParseVariablesDefinition(string text, string relPath, TransformationData data, PreprocessedTextLocationMap map)
        {
            return Preprocessor.Replace(VariableDefinitionPattern, text, everyMatch => ParseVariableDefinition(everyMatch, relPath, data), map);
        }

        private string ParseVariableDefinition(Match everyMatch, string relPath, TransformationData data)
        {
            var value = everyMatch.Groups["variableContent"].Value.Replace("<a name=\"MARKDOWNANCHORNOTUSEDELSEWHERE\"></a>", "");

            Add(everyMatch.Groups["variableName"].Value.ToLower(), value, data, relPath);

            return "";
        }

        public VariableManager(PreprocessedData data)
        {

        }

        public List<string> GetVariableNames()
        {
            return variableMap.Keys.ToList();
        }

        public void Add(string name, string content, TransformationData data, string variableFileFolderName = null)
        {
            if (!string.IsNullOrWhiteSpace(variableFileFolderName))
            {
                content = Markdown.ProcessRelativePaths(content, variableFileFolderName);
            }

            if (variableMap.ContainsKey(name))
            {
                data.ErrorList.Add(
                    new ErrorDetail(Language.Message("VariableRedefinition", name),
                        MessageClass.Info, "", "", 0, 0)
                );
            }
            else
            {
                variableMap.Add(name, content);
            }
        }

        public bool TryGet(string name, out string content)
        {
            if (variableMap.ContainsKey(name))
            {
                content = variableMap[name];
                return true;
            }

            content = "";
            return false;
        }

        private string ParseInLineVariableOrMetadata(Match match, FileInfo currentFile, FileInfo originalScope, TransformationData data, EMDocument doc, Stack<Tuple<string, string>> visitedVariables)
        {
            var pathToLinkedFile = match.Groups["path"].Value;
            var variableName = match.Groups["variableName"].Value.ToLower();
            var originalString = match.Value;
            string relativePathToLinkedFile = null;

            var outString = "";
            var isProblem = false;
            var changedLanguage = false;

            var errorId = 0;
            var OnErrorStringToUseForMatch = originalString;

            //ignore if the value is STOPPUBLISH STARTPUBLISH
            if (variableName.Equals("STOPPUBLISH") || variableName.Equals("STARTPUBLISH") || originalString.ToUpper() == "%ROOT%" || VariableManager.IsKeywordToSkip(originalString))
            {
                return originalString;
            }
            else
            {
                if (pathToLinkedFile.Equals("doxygen"))
                {
                    var excerptName = ("doxygen:" + match.Groups["variableName"].Value).Replace(':', 'x');

                    if (!doc.Excerpts.Exists(excerptName))
                    {
                        var content =
                            DoxygenHelper.GetSymbolDescriptionAndCatchExceptionIntoMarkdownErrors(
                                match.Groups["variableName"].Value,
                                match.Value,
                                DoxygenHelper.DescriptionType.Full,
                                data,
                                data.Markdown,
                                data.FoundDoxygenSymbols).Trim();

                        content = Normalizer.Normalize(content, null);

                        doc.Excerpts.AddExcerpt(
                            excerptName, new EMExcerpt(doc, 0, 0, content, data, excerptName == ""));
                    }

                    return string.Format("[INCLUDE:#{0}]", excerptName);
                }
                if (pathToLinkedFile.Equals("orig"))
                {
                    currentFile = originalScope;
                    relativePathToLinkedFile = data.CurrentFolderDetails.CurrentFolderFromMarkdownAsTopLeaf;
                }
                else if (!String.IsNullOrWhiteSpace(pathToLinkedFile))
                {
                    // Link other file and get metadata from there.
                    var languageForFile = data.CurrentFolderDetails.Language;
                    relativePathToLinkedFile = pathToLinkedFile;

                    var linkedFileDir = new DirectoryInfo(Path.Combine(Path.Combine(data.CurrentFolderDetails.AbsoluteMarkdownPath, pathToLinkedFile)));

                    if (!linkedFileDir.Exists || linkedFileDir.GetFiles(string.Format("*.{0}.udn", data.CurrentFolderDetails.Language)).Length == 0)
                    {
                        // if this is not an INT file check for the INT version.
                        if (data.CurrentFolderDetails.Language != "INT")
                        {
                            if (!linkedFileDir.Exists || linkedFileDir.GetFiles("*.INT.udn").Length == 0)
                            {
                                errorId = data.ErrorList.Count;
                                data.ErrorList.Add(
                                    Markdown.GenerateError(
                                        Language.Message(
                                            "BadLinkOrMissingMarkdownFileAndNoINTFile", pathToLinkedFile, variableName),
                                        MessageClass.Error,
                                        Markdown.Unescape(OnErrorStringToUseForMatch),
                                        errorId,
                                        data));

                                isProblem = true;
                            }
                            else
                            {
                                // Found int file
                                languageForFile = "INT";

                                // Raise info so that now we are allowing missing linked files to still allow processing of the file if INT file is there
                                errorId = data.ErrorList.Count;
                                data.ErrorList.Add(
                                    Markdown.GenerateError(
                                        Language.Message(
                                            "BadLinkOrMissingMarkdownFileINTUsed", pathToLinkedFile, variableName),
                                        MessageClass.Info,
                                        Markdown.Unescape(OnErrorStringToUseForMatch),
                                        errorId,
                                        data));

                                isProblem = true;

                                changedLanguage = true;
                            }
                        }
                        else
                        {
                            errorId = data.ErrorList.Count;
                            data.ErrorList.Add(
                                Markdown.GenerateError(
                                    Language.Message("BadLinkOrMissingMarkdownFile", pathToLinkedFile, variableName),
                                    MessageClass.Error,
                                    Markdown.Unescape(OnErrorStringToUseForMatch),
                                    errorId,
                                    data));

                            isProblem = true;
                            outString = OnErrorStringToUseForMatch;
                        }
                    }

                    if (linkedFileDir.Exists && linkedFileDir.GetFiles(string.Format("*.{0}.udn", languageForFile)).Length > 0)
                    {
                        currentFile = linkedFileDir.GetFiles(string.Format("*.{0}.udn", languageForFile))[0];
                    }
                }

                // Cycle detection mechanism. First check if there was request for that variable in the visited stack.
                var currentVisitedVariableTuple = new Tuple<string, string>(currentFile.FullName, variableName);
                if (visitedVariables != null && visitedVariables.Contains(currentVisitedVariableTuple))
                {
                    // cycle detected
                    visitedVariables.Push(currentVisitedVariableTuple);
                    string visitedVariablesMessage = string.Join(", ", visitedVariables.Select(
                            (v) =>
                            {
                                return string.Format("[{0}, {1}]", v.Item1, v.Item2);
                            }));
                    visitedVariables.Pop();

                    data.ErrorList.Add(
                        Markdown.GenerateError(Language.Message("CycleDetectedInVariableRefs", visitedVariablesMessage), MessageClass.Error,
                        "", data.ErrorList.Count, data));

                    return "";
                }

                if (!data.ProcessedDocumentCache.TryGetLinkedFileVariable(currentFile, variableName, out outString))
                {
					if (doc.PerformStrictConversion())
					{
						//error
						errorId = data.ErrorList.Count;
						data.ErrorList.Add(
							Markdown.GenerateError(
								Language.Message("VariableOrMetadataNotFoundInFile", variableName, currentFile.FullName),
								MessageClass.Error,
								OnErrorStringToUseForMatch,
								errorId,
								data));

						isProblem = true;
					}
                    outString = OnErrorStringToUseForMatch.Replace("%", "&#37;");
                }
                else
                {
                    // If there was no request for this variable then push it.
                    visitedVariables.Push(currentVisitedVariableTuple);

                    outString = InLineVariableOrMetadata.Replace(
                        outString,
                        everyMatch =>
                        ParseInLineVariableOrMetadata(everyMatch, currentFile, originalScope, data, doc, visitedVariables));

                    // After parsing inner string pop visited variable from the stack.
                    visitedVariables.Pop();
                }

                if (changedLanguage)
                {
                    //If we had to replace the language with INT then update the linkText to include link to image of flag
                    outString += Templates.ImageFrame.Render(
                        Hash.FromAnonymousObject(
                            new
                            {
                                imageClass = "languageinline",
                                imagePath = Path.Combine(
                                    data.CurrentFolderDetails.RelativeHTMLPath,
                                    "Include", "Images", "INT_flag.jpg")
                            }));
                }
            }

            if (data.Markdown.ThisIsPreview && isProblem)
            {
                return Templates.ErrorHighlight.Render(
                    Hash.FromAnonymousObject(
                        new
                        {
                            errorId = errorId,
                            errorText = outString
                        }));
            }

            return outString;
        }

        public string ReplaceVariables(EMDocument doc, string text, TransformationData data, PreprocessedTextLocationMap map = null)
        {
            text = EMTOCInline.ConvertVariableLikeTOCInlines(text, map);

            var stack = new Stack<Tuple<string, string>>();
            var currentFile = new FileInfo(doc.LocalPath);

            return map == null
                       ? InLineVariableOrMetadata.Replace(
                           text,
                           everyMatch =>
                           ParseInLineVariableOrMetadata(everyMatch, currentFile, currentFile, data, doc, stack))
                       : Preprocessor.Replace(
                           InLineVariableOrMetadata,
                           text,
                           everyMatch =>
                           ParseInLineVariableOrMetadata(everyMatch, currentFile, currentFile, data, doc, stack),
                           map);
        }

        readonly Dictionary<string, string> variableMap = new Dictionary<string, string>();

        private static readonly string[] KeywordsToSkip = new string[]
            {
                "H", "I", "M", "N", "P", "Q", "S", "T", "U", "X", "Y", // graphic icons
                "PURPLE", "YELLOW", "ORANGE", "RED", "PINK", "PURPLE", "TEAL", "NAVY", "BLUE", "AQUA", "LIME", "GREEN", "OLIVE", "MAROON", "BROWN", "BLACK", "GRAY", "SILVER", "WHITE", "ENDCOLOR" // colours
            };
        private static readonly Regex KeywordFromOrigPattern = new Regex("^%(?<keyword>.+)%$", RegexOptions.Compiled);
        public static bool IsKeywordToSkip(string orig)
        {
            var keyword = KeywordFromOrigPattern.Match(orig).Groups["keyword"].Value;

            return KeywordsToSkip.Any(t => t == keyword);
        }

        public static string CutoutVariableInclusions(string text, PreprocessedTextLocationMap map)
        {
            text = Preprocessor.EscapeChars(text, map);
            
            text = Preprocessor.Replace(InLineVariableOrMetadata, text, "", map);

            return Preprocessor.UnescapeChars(text, false, map);
        }
    }
}
