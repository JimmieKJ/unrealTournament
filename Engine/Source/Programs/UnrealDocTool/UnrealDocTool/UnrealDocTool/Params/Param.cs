// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Windows;
using System.Windows.Controls;

namespace UnrealDocTool.Params
{
    public delegate void AfterParsingHandler(string arg);

    public class Param
    {
        public Param(
            string name,
            string wpfName,
            Func<string> descriptionGetter,
            ParamsDescriptionGroup descGroup = null,
            Action<string> parsedAction = null)
        {
            Name = name;
            WpfName = wpfName;
            DescriptionGroup = descGroup;

            this.descriptionGetter = descriptionGetter;

            if (parsedAction != null)
            {
                AfterParsing += (param) => parsedAction(param);
            }
        }

        public virtual void Parse(string param)
        {
            TriggerAfterParsingEvent(param);
        }

        public virtual string GetExample()
        {
            return "[-" + Name + "=value]";
        }

        public virtual FrameworkElement GetGUIElement()
        {
            var label = new Label { Content = WpfName };

            return label;
        }

        protected void TriggerAfterParsingEvent(string data)
        {
            if (AfterParsing != null)
            {
                AfterParsing(data);
            }
        }

        public string Name { get; private set; }
        public ParamsDescriptionGroup DescriptionGroup { get; private set; }
        public event AfterParsingHandler AfterParsing;
        public string WpfName { get; private set; }
        public string Description
        {
            get
            {
                return this.descriptionGetter();
            }
        }

        private readonly Func<string> descriptionGetter;
    }
}
