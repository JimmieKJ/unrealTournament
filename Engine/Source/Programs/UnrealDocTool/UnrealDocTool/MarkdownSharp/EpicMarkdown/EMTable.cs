// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown
{
    using System.Text.RegularExpressions;

    using DotLiquid;

    using MarkdownSharp.EpicMarkdown.Parsers;
    using MarkdownSharp.EpicMarkdown.Traversing;
    using MarkdownSharp.Preprocessor;

    class EMTable : EMElement
    {
        public EMTable(EMDocument doc, EMElementOrigin origin, EMElement parent, Match match, TransformationData data)
            : base(doc, origin, parent)
        {
            var tableAlignment = RemoveTrailingWhitespaceAndNewLines(match.Groups[3].Value, null);
            
            bool useRowHeader;

            optionalCaption = data.Markdown.RunSpanGamut(GetOptionalCaption(match.Groups[0].Value), data);
            tableHeaderRows = GetHeaders(doc, origin, this, data, match.Groups[2].Index - match.Index, match.Groups[2].Value, out useRowHeader);
            tableColumnAlignments = new List<Hash>();
            tableRows = GetRows(this, data, match.Groups[5].Index - match.Index, match.Groups[5].Value, GetAlignments(tableAlignment, tableColumnAlignments), useRowHeader, !String.IsNullOrWhiteSpace(match.Groups[2].Value));
        }

        public override string GetClassificationString()
        {
            return "markdown.tableofdata";
        }

        public static EMRegexParser GetParser()
        {
            return new EMRegexParser(TablePattern, CreateTable);
        }

        private static EMElement CreateTable(Match match, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            return new EMTable(doc, origin, parent, match, data);
        }

        public class TableCellInformation : ITraversable, IHTMLRenderable
        {
            public TableCellInformation(
                int colSpanCount = 1,
                string alignment = null,
                bool isRowHeader = false,
                bool isDummy = false,
                int rowSpanCount = 1,
                bool isRowSpanColumn = false)
            {
                IsRowHeader = isRowHeader;
                ColSpanCount = colSpanCount;
                RowSpanCount = rowSpanCount;
                IsRowSpanColumn = isRowSpanColumn;
                Alignment = alignment;
                IsDummy = isDummy;
                Content = null;
            }

            public TableCellInformation(
                EMDocument doc,
                EMElementOrigin origin,
                EMElement parent,
                TransformationData data,
                int colSpanCount = 1,
                string alignment = null,
                bool isRowHeader = false,
                bool isDummy = false,
                int rowSpanCount = 1,
                bool isRowSpanColumn = false)
                : this(colSpanCount, alignment, isRowHeader, isDummy, rowSpanCount, isRowSpanColumn)
            {
                Content = new EMSpanElements(doc, origin, parent);
                Content.Parse(0, origin.Text, data);
            }

            public bool IsRowHeader { get; private set; }
            public string Alignment { get; private set; }
            public int RowSpanCount { get; set; }
            public bool IsRowSpanColumn { get; private set; }

            public int ColSpanCount { get; private set; }
            public bool IsDummy { get; private set; }
            public EMSpanElements Content { get; private set; }

            public void ClearContent()
            {
                IsDummy = true;
                Content.Clear();
            }

            public void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
            {
                builder.Append(
                    Templates.TableCell.Render(
                        Hash.FromAnonymousObject(
                            new
                                {
                                    hasContent = (Content != null),
                                    isRowHeader = IsRowHeader,
                                    rowSpanCount = RowSpanCount,
                                    colSpanCount = ColSpanCount,
                                    alignment = Alignment,
                                    content = (Content != null) ? Content.GetInnerHTML(includesStack, data) : null
                                })));
            }

            public void ForEachWithContext<TContext>(TContext context) where TContext : ITraversingContext
            {
                context.PreChildrenVisit(this);

                if (Content != null)
                {
                    Content.ForEachWithContext(context);
                }

                context.PostChildrenVisit(this);
            }
        }

        public class TableRowInformation : ITraversable, IHTMLRenderable
        {
            public TableRowInformation()
            {
                Cells = new List<TableCellInformation>();
                HasError = false;
                ErrorId = -1;
            }

            public List<TableCellInformation> Cells { get; private set; }
            public bool HasError { get; set; }
            public int ErrorId { get; set; }

            public void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
            {
                var cells = new StringBuilder();

                foreach (var cell in Cells)
                {
                    cell.AppendHTML(cells, includesStack, data);
                }

                builder.Append(Templates.TableRow.Render(
                        Hash.FromAnonymousObject(new { hasError = HasError, errorId = ErrorId, cells = cells.ToString() })));
            }

            public void ForEachWithContext<TContext>(TContext context) where TContext : ITraversingContext
            {
                context.PreChildrenVisit(this);

                foreach (var cell in Cells)
                {
                    cell.ForEachWithContext(context);
                }

                context.PostChildrenVisit(this);
            }
        }

        private static List<TableRowInformation> GetRows(
            EMElement parent,
            TransformationData data,
            int tableDataOffset,
            string tableData,
            string[] alignments,
            bool useRowHeader,
            bool hasHeader)
        {
            // Add body data
            var tableRows = new List<TableRowInformation>();

            var map = new PreprocessedTextLocationMap();
            tableData = RemoveTrailingWhitespaceAndNewLines(tableData, map);

            var dataRows = Regex.Split(tableData, @"\n");

            var rowOffset = 0;

            for (var i = 0; i < dataRows.Count(); ++i)
            {
                var dataRow = dataRows[i];

                // Parse table into List of List of CellInformation
                var count = 0;

                var dataColumns = Regex.Matches(dataRow, @"([^\|]+)([\|]*)");

                var tableRow = new TableRowInformation();

                // Check to see if someone has left an empty row incorrectly formatted, which we can fix.
                if (dataColumns.Count == 0)
                {
                    var fixedDataRow = dataRow;
                    for (var j = dataRow.Length; j < alignments.Length + 1; ++j)
                    {
                        fixedDataRow += '|';
                    }
                    fixedDataRow = fixedDataRow.Insert(1, " ");

                    dataColumns = Regex.Matches(fixedDataRow, @"([^\|]+)([\|]*)");
                }

                foreach (Match dataColumn in dataColumns)
                {
                    int addToOffset;
                    var cell = Normalizer.LeadingWhitespaceRemove(dataColumn.Groups[1].Value, out addToOffset);
                    cell = Normalizer.TrailingWhitespaceRemove(cell);
                    
                    var columnSpanLength = 1;
                    var isRowHeader = (dataColumns.Count == 1 && i == 0 && !hasHeader);

                    if (Regex.Match(dataColumn.Groups[2].Value, @"(\|{2,})").Success)
                    {
                        columnSpanLength = Regex.Match(dataColumn.Groups[2].Value, @"(\|{2,})").Length;
                    }

                    // @UE3 ensure that the index into alignments is not greater than the amount expected. (Users may accidentally add extra || to lines)
                    string alignment = null;
                    if (count >= alignments.Length)
                    {
                        // Report only 1 error per row per run to avoid duplicates
                        if (!tableRow.HasError)
                        {
                            var errorCount = data.ErrorList.Count;
                            tableRow.ErrorId = errorCount;
                            tableRow.HasError = true;
                            data.ErrorList.Add(
                                Markdown.GenerateError(
                                    Language.Message(
                                        "MoreColumnsThanAllignmentsInOneOfTheTableRowsColumnsIgnored",
                                        Markdown.Unescape(dataRows[i])),
                                    MessageClass.Error,
                                    Markdown.Unescape(dataRows[i]),
                                    errorCount,
                                    data));
                        }
                    }
                    else
                    {
                        alignment = alignments[count];
                    }

                    var cellInformation = (cell.Trim() != "^")
                                              ? new TableCellInformation(
                                                    parent.Document,
                                                    new EMElementOrigin(tableDataOffset + map.GetOriginalPosition(addToOffset + rowOffset + dataColumn.Groups[1].Index, PositionRounding.Down), cell),
                                                    parent, data, columnSpanLength, alignment, isRowHeader)
                                              : new TableCellInformation(columnSpanLength, alignment, isRowHeader, false, 1, true);

                    tableRow.Cells.Add(cellInformation);

                    // Make parsing table easier later, if ColumnSpan > 1 create a blank column for each over one
                    for (var dummyColumnCount = 1; dummyColumnCount < columnSpanLength; ++dummyColumnCount)
                    {
                        tableRow.Cells.Add(new TableCellInformation(0));
                    }

                    // @UE3 this was not in original specification, handles correct alignment of a subsequent column after a long column.
                    count += columnSpanLength;
                }
                // If count is zero check that someone has just not placed a space in the first column indicating it should be empty.

                if (count < alignments.Length)
                {
                    var errorCount = data.ErrorList.Count;
                    tableRow.ErrorId = errorCount;
                    tableRow.HasError = true;
                    data.ErrorList.Add(
                        Markdown.GenerateError(
                            Language.Message(
                                "LessColumnsThanExpectedInOneOfTheTableRowsEmptyCellsInserted", Markdown.Unescape(dataRows[i])),
                            MessageClass.Error,
                            Markdown.Unescape(dataRows[i]),
                            errorCount,
                            data));
                }

                // If count is less than total expected for a rows data add dummy column and raise error
                for (var dummyColumnCount = count; dummyColumnCount < alignments.Length; ++dummyColumnCount)
                {
                    tableRow.Cells.Add(new TableCellInformation(1, null, false, true));
                }

                tableRows.Add(tableRow);

                rowOffset += dataRow.Length + 1;
            }

            // Now work out rowspans based on Content and character ^
            // Work down rows in columns

            for (var columnNumber = 0; columnNumber < alignments.Length; ++columnNumber)
            {
                var firstNonSpanRow = -1;
                for (var rowNumber = 0; rowNumber < tableRows.Count; ++rowNumber)
                {
                    if (tableRows[rowNumber].Cells[columnNumber].IsRowSpanColumn) //Found a rowspan column
                    {
                        if (rowNumber == 0)
                        {
                            var errorCount = data.ErrorList.Count;
                            tableRows[rowNumber].ErrorId = errorCount;
                            tableRows[rowNumber].HasError = true;
                            data.ErrorList.Add(
                                Markdown.GenerateError(
                                    Language.Message(
                                        "ColumnCannotBeSetToSpanWithSpecialSymbolInTheFirstRowOfATable", dataRows[0]),
                                    MessageClass.Error,
                                    dataRows[0],
                                    errorCount,
                                    data));
                        }
                        else
                        {
                            if (firstNonSpanRow < 0)
                            {
                                //is this the first detected row for this span?
                                firstNonSpanRow = rowNumber - 1;

                                //Row span above this 1 now should include itself
                                tableRows[firstNonSpanRow].Cells[columnNumber].RowSpanCount = 1;
                            }

                            //Increment the Row above first_row detected by 1
                            tableRows[firstNonSpanRow].Cells[columnNumber].RowSpanCount += 1;
                        }
                    }
                    else if (firstNonSpanRow >= 0)
                    {
                        //This row is not a rowspan but we had one previously so clear for any other rowspans in the column
                        firstNonSpanRow = -1;
                    }
                }
            }

            return tableRows;
        }

        private static List<TableRowInformation> GetHeaders(EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data, int headerOffset, string tableHeader, out bool useRowHeader)
        {
            // Add headers
            // May be multiple lines, may have columns spanning
            // May also have no header if we are intending the 1st column to be the header

            var map = new PreprocessedTextLocationMap();
            tableHeader = RemoveTrailingWhitespaceAndNewLines(tableHeader, map);

            List<TableRowInformation> templateHeaderRows = null;
            useRowHeader = true;

            if (!String.IsNullOrWhiteSpace(tableHeader))
            {
                templateHeaderRows = new List<TableRowInformation>();

                var headerRowOffset = 0;

                var headerRows = Regex.Split(tableHeader, @"\n");
                foreach (var headerRow in headerRows)
                {
                    var count = 0;

                    var headerColumns = Regex.Matches(headerRow, @"[ ]?([^\|]+)[ ]?([\|]*)");
                    var row = new TableRowInformation();

                    foreach (Match headerColumn in headerColumns)
                    {
                        var cellGroup = headerColumn.Groups[1];
                        var columnSpanLength = 1;

                        if (Regex.Match(headerColumn.Groups[2].Value, @"(\|{2,})").Success)
                        {
                            columnSpanLength = Regex.Match(headerColumn.Groups[2].Value, @"(\|{2,})").Length;
                        }

                        var cell = new TableCellInformation(doc, new EMElementOrigin(headerOffset + map.GetOriginalPosition(headerRowOffset + cellGroup.Index, PositionRounding.Down), cellGroup.Value), parent, data, columnSpanLength, null, true);

                        if (count == 0)
                        {
                            useRowHeader = !Regex.Match(cell.ToString(), @"(\S)").Success;
                        }

                        count++;

                        row.Cells.Add(cell);
                    }

                    headerRowOffset += headerRow.Length + 1;
                    templateHeaderRows.Add(row);
                }
            }

            return templateHeaderRows;
        }

        private static string[] GetAlignments(string tableAlignment, List<Hash> templateColumnAlignments)
        {
            // Reading alignment from header underline.
            var alignmentColumns = Regex.Matches(tableAlignment, @"([^\\n|.]+)([\|]*)");
            var alignments = new string[alignmentColumns.Count];

            var columnIndex = 0;

            foreach (Match alignmentColumn in alignmentColumns)
            {
                var cell = alignmentColumn.Groups[1].Value.Trim();
                var alignment = "";

                //Check alignment based on position of colons
                if (Regex.Match(cell, @":-+:").Success)
                {
                    alignment = "center";
                }
                else if (Regex.Match(cell, @"-+:").Success)
                {
                    alignment = "right";
                }
                else if (Regex.Match(cell, @":-+").Success)
                {
                    alignment = "left";
                }

                alignments[columnIndex] = alignment;
                ++columnIndex;

                templateColumnAlignments.Add(Hash.FromAnonymousObject(
                    new
                    {
                        extended = Regex.Match(alignmentColumn.Groups[2].Value, @"(\|{2,})").Success,
                        align = alignment
                    }));
            }

            return alignments;
        }

        private static string GetOptionalCaption(string text)
        {
            // Just match first caption front or trailing. Ignore secondary captions
            return Regex.Match(text, @"^[ ]{0,3}\[([^\]]*?)\]",RegexOptions.Multiline).Groups[1].Value;
        }

        private static readonly Regex TrailingWhitespaceAndNewLinesPattern = new Regex(@"[ ]*[\n]+", RegexOptions.Compiled);
        private static readonly Regex TrailingReturnPattern = new Regex(@"\n$", RegexOptions.Compiled);

        private static string RemoveTrailingWhitespaceAndNewLines(string text, PreprocessedTextLocationMap map)
        {
            return Preprocessor.Replace(
                TrailingReturnPattern,
                Preprocessor.Replace(TrailingWhitespaceAndNewLinesPattern, text, "\n", map),
                "", map);
        }

        private static readonly Regex TablePattern = new Regex(@"^([ ]{0,3}\[[^\]]*?\][^\n]*\n)?^((?:[ ]{0,3}[\S][^\||\n]*?\|{1,}[^\n]*?\n)*?)^([ ]{0,3}(\|+[ ]*)?[:]?-{2,}[^\|]*?\|+[^\n]*?\n)^((?:[ ]{0,3}[^\n]*?\|{1,}[^\n]*?(\n|\z))+)(^[ ]{0,3}\[[^\]]*?\](\n|\z))?", RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        private readonly string optionalCaption;
        private readonly List<Hash> tableColumnAlignments;
        private readonly List<TableRowInformation> tableHeaderRows;
        private readonly List<TableRowInformation> tableRows;

        public override void ForEachWithContext<T>(T context)
        {
            context.PreChildrenVisit(this);

            if (tableHeaderRows != null)
            {
                foreach (var headerRow in tableHeaderRows)
                {
                    headerRow.ForEachWithContext(context);
                }
            }

            foreach (var row in tableRows)
            {
                row.ForEachWithContext(context);
            }

            context.PostChildrenVisit(this);
        }

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            var headers = new StringBuilder();

            if (tableHeaderRows != null)
            {
                foreach (var headerRow in tableHeaderRows)
                {
                    headerRow.AppendHTML(headers, includesStack, data);
                }
            }

            var rows = new StringBuilder();

            foreach (var row in tableRows)
            {
                row.AppendHTML(rows, includesStack, data);
            }

            builder.Append(Templates.Table.Render(Hash.FromAnonymousObject(
                new
                {
                    caption = optionalCaption,
                    columnAlignments = tableColumnAlignments,
                    headers = tableHeaderRows != null ? headers.ToString() : null,
                    rows = rows.ToString()
                })));
        }
    }
}
