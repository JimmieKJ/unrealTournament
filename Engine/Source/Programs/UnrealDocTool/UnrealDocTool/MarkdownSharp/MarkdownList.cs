// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using DotLiquid;

namespace MarkdownSharp
{
    [LiquidType("Content", "HasError", "ErrorId")]
    public class MarkdownItemBase
    {
        public MarkdownItemBase(MarkdownListType type)
        {
            ErrorId = -1;
            Type = type;
        }

        public int ErrorId { get; set; }
        public bool HasError { get { return ErrorId > -1; } }
        public MarkdownListType Type { get; set; }
    }

    [LiquidType("Content", "HasError", "ErrorId")]
    public class MarkdownItem : MarkdownItemBase
    {
        public MarkdownItem(MarkdownListType type)
            : base(type)
        {
            
        }

        public string Content { get; set; }
    }

    [LiquidType("Content", "HasError", "ErrorId")]
    public class MarkdownOrderedItem : MarkdownItem
    {
        public MarkdownOrderedItem()
            : base(MarkdownListType.Ordered)
        {
            
        }
    }

    [LiquidType("Content", "HasError", "ErrorId")]
    public class MarkdownUnorderedItem : MarkdownItem
    {
        public MarkdownUnorderedItem()
            : base(MarkdownListType.Unordered)
        {

        }
    }

    [LiquidType("Definition", "Description", "HasError", "ErrorId")]
    public class MarkdownDefinitionItem : MarkdownItemBase
    {
        public MarkdownDefinitionItem()
            : base(MarkdownListType.Definition)
        {

        }

        public string Definition { get; set; }
        public string Description { get; set; }
    }

    public enum MarkdownListType
    {
        Definition,
        Unordered,
        Ordered
    }

    [LiquidType("IsOrdered", "IsUnordered", "IsDefinition", "Items")]
    public class MarkdownList
    {
        public MarkdownList()
        {
            Items = new List<MarkdownItemBase>();
        }

        public bool IsOrdered { get { return Type == MarkdownListType.Ordered; } }
        public bool IsUnordered { get { return Type == MarkdownListType.Unordered; } }
        public bool IsDefinition { get { return Type == MarkdownListType.Definition; } }

        public MarkdownListType Type { get; set; }
        public List<MarkdownItemBase> Items { get; private set; } 
    }
}
