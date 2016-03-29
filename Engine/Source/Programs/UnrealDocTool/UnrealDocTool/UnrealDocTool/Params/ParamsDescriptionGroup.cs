// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;

namespace UnrealDocTool.Params
{
    public class ParamsDescriptionGroup
    {
        public ParamsDescriptionGroup(Func<string> descriptionGetter)
        {
            this.descriptionGetter = descriptionGetter;
        }

        public string Description
        {
            get
            {
                return descriptionGetter();
            }
        }

        private readonly Func<string> descriptionGetter;
    }
}
