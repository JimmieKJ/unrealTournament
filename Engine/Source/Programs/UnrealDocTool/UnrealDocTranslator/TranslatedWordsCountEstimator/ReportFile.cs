// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using Microsoft.VisualBasic.FileIO;
using System.Collections.Generic;
using System.IO;
using System.Globalization;
using System.Linq;

namespace TranslatedWordsCountEstimator
{
    public class ReportFile
    {
        public ReportFile(string path)
        {
            fileInfo = new FileInfo(path);

            if (!fileInfo.Exists)
            {
                CreateReportFile(fileInfo);
            }

            var parser = new TextFieldParser(path) { Delimiters = new[] { "," } };

            if (!parser.EndOfData)
            {
                var headerFields = parser.ReadFields();
                var langList = new List<string>();

                // skip Date/Time and Word Count column headers
                for (var i = 2; i < headerFields.Length; ++i)
                {
                    var lang = headerFields[i];

                    langList.Add(lang);
                    langs.Add(lang);
                }

                while (!parser.EndOfData)
                {
                    rows.Add(new ReportRow(langList.ToArray(), parser.ReadFields()));
                }
            }

            parser.Close();
        }

        public void AppendData(DateTime dateTime, IDictionary<string, double> langValueMap)
        {
            var totalWords = langValueMap["int"];

            langValueMap.Remove("int");

            foreach (var lang in langValueMap.Keys)
            {
                langs.Add(lang);
            }

            rows.Add(new ReportRow(dateTime, totalWords, langValueMap));
        }

        public void Save()
        {
            var langList = langs.ToList();

            var stringRows = new List<string> { string.Join(",", new List<string>() { StdHeader }.Union(langList)) };
            stringRows.AddRange(rows.Select(r => r.ToString(langList)));

            File.WriteAllLines(fileInfo.FullName, stringRows);
        }

        private static void CreateReportFile(FileInfo fileInfo)
        {
            File.WriteAllText(fileInfo.FullName, StdHeader + "\n");
        }

        private readonly FileInfo fileInfo;
        private readonly HashSet<string> langs = new HashSet<string>();
        private readonly List<ReportRow> rows = new List<ReportRow>();

        private const string StdHeader = "Date/Time,Word Count";
    }

    internal class ReportRow
    {
        public ReportRow(string[] langs, string[] csvRow)
        {
            dateTime = DateTime.ParseExact(csvRow[0], DateTimeFormat, CultureInfo.InvariantCulture);
            totalWords = double.Parse(csvRow[1]);

            for (var i = 2; i < csvRow.Length; ++i)
            {
                values.Add(langs[i - 2], double.Parse(csvRow[i]));
            }
        }

        public ReportRow(DateTime dateTime, double totalWords, IDictionary<string, double> values)
        {
            this.dateTime = dateTime;
            this.totalWords = totalWords;

            foreach (var pair in values)
            {
                this.values.Add(pair.Key, pair.Value);
            }
        }

        public string ToString(IList<string> langs)
        {
            var fields = new List<string> { dateTime.ToString(DateTimeFormat), totalWords.ToString(CultureInfo.InvariantCulture) };
            fields.AddRange(langs.Select(l => values.ContainsKey(l) ? values[l].ToString(CultureInfo.InvariantCulture) : (0.0).ToString(CultureInfo.InvariantCulture)));

            return string.Join(",", fields);
        }

        private readonly DateTime dateTime;
        private readonly double totalWords;
        private readonly Dictionary<string, double> values = new Dictionary<string, double>();

        private const string DateTimeFormat = "yyyy.M.d-H.mm.ss";
    }
}
