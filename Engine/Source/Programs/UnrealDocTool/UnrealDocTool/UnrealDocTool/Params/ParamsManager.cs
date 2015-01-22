// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Text;
using MarkdownSharp;
using System.Linq;
using System;
using System.Text.RegularExpressions;

namespace UnrealDocTool.Params
{
    public delegate void ParsingErrorHandler(Param sender, string message);

    public class ParsingErrorException : Exception
    {
        public ParsingErrorException(Param sender, string message)
            : base(message)
        {
            Sender = sender;
        }

        public Param Sender { get; private set; }
    }

    public class ParamsManager
    {
        public event ParsingErrorHandler ParsingError;

        public void Add(IEnumerable<Param> parametersToAdd)
        {
            foreach (var param in parametersToAdd)
            {
                Add(param);
            }
        }

        public void Add(Param param)
        {
            parameters.Add(param);

            var groupHash = (param.DescriptionGroup != null) ? param.DescriptionGroup.GetHashCode() : 0;
            
            if (!groupParamsMap.ContainsKey(groupHash))
            {
                if (groupHash != 0)
                {
                    groups.Add(param.DescriptionGroup);
                }

                groupParamsMap[groupHash] = new List<Param>();
            }

            groupParamsMap[groupHash].Add(param);
        }

        public string GetHelp()
        {
            var sb = new StringBuilder();

            sb.Append(Language.Message("Usage") + "\n");
            sb.Append(
                "    UnrealDocTool <" + mainParam.Name + "> " + string.Join(" ", parameters.Select(p => p.GetExample())) + "\n\n");
            WrapWithIndentAt(sb, 2, 80);

            sb.Append("    " + mainParam.Description + "\n\n");
            WrapWithIndentAt(sb, 1, 80);

            foreach (var group in groups)
            {
                sb.Append("    " + group.Description + "\n");
                WrapWithIndentAt(sb, 1, 80);

                foreach (var param in groupParamsMap[group.GetHashCode()])
                {
                    sb.Append("        -" + param.Name + ((param is Flag) ? " " : "= ") + param.Description + "\n\n");
                    WrapWithIndentAt(sb, 3, 80);
                }

                sb.Append("\n\n");
            }

            return sb.ToString();
        }

        public void SetMainParam(Param param)
        {
            mainParam = param;
        }
        
        public void Parse(string[] args)
        {
            try
            {
                var mainParamParsed = false;

                foreach (var arg in args)
                {
                    var match = ParamParser.Match(arg);

                    if (match.Success)
                    {
                        var name = match.Groups["name"].Value;
                        var content = match.Groups["content"].Value;
                        var paramFound = false;

                        foreach (var param in parameters.Where(param => param.Name == name))
                        {
                            paramFound = true;

                            if (string.IsNullOrWhiteSpace(content) && !(param is Flag))
                            {
                                throw new ParsingErrorException(
                                    param, Language.Message("ParamNotAFlagNeedsValue", param.Name));
                            }

                            param.Parse(content);
                            break;
                        }

                        if (!paramFound)
                        {
                            throw new ParsingErrorException(null, Language.Message("UnknownParamName", name));
                        }
                    }
                    else
                    {
                        mainParamParsed = true;
                        mainParam.Parse(arg);
                    }
                }

                if (!mainParamParsed)
                {
                    throw new ParsingErrorException(mainParam, Language.Message("RequiredParameter", mainParam.Name));
                }
            }
            catch (ParsingErrorException e)
            {
                if (ParsingError != null)
                {
                    ParsingError(e.Sender, e.Message);
                }
                else
                {
                    throw;
                }
            }
        }

        public List<Param> GetParams()
        {
            return parameters;
        }

        public Param GetMainParam()
        {
            return mainParam;
        }

        public List<ParamsDescriptionGroup> GetGroups()
        {
            return groups;
        }

        public List<Param> GetParamsByGroup(ParamsDescriptionGroup group)
        {
            return groupParamsMap[group.GetHashCode()];
        }

        private static void WrapWithIndentAt(StringBuilder sb, int indent, int wrapAt)
        {
            if (wrapAt <= indent * 4)
            {
                throw new ArgumentOutOfRangeException("The indent times 4 has to be smaller than wrapAt.");
            }

            var indentString = new string(' ', indent * 4);

            while (true)
            {
                var data = sb.ToString().TrimEnd();

                var newLineIndex = data.LastIndexOf("\n", System.StringComparison.Ordinal);

                if (data.Length - newLineIndex < wrapAt)
                {
                    break;
                }

                var insertion = data.LastIndexOf(" ", newLineIndex + wrapAt, System.StringComparison.Ordinal);
                sb.Remove(insertion, 1);
                sb.Insert(insertion, "\n" + indentString);
            }
        }

        private Param mainParam = null;
        private readonly static Regex ParamParser = new Regex(@"^-(?<name>[_a-zA-Z][_a-zA-Z0-9]*)(?:=(?<content>.+))?$", RegexOptions.Compiled);
        private readonly List<Param> parameters = new List<Param>();
        private readonly Dictionary<int, List<Param>> groupParamsMap = new Dictionary<int, List<Param>>();
        private readonly List<ParamsDescriptionGroup> groups = new List<ParamsDescriptionGroup>();
    }
}
