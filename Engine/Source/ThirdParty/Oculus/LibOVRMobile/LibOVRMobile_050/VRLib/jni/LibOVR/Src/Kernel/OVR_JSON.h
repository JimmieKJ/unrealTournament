/************************************************************************************

PublicHeader:   None
Filename    :   OVR_JSON.h
Content     :   JSON format reader and writer
Created     :   April 9, 2013
Author      :   Brant Lewis
Notes       :

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_JSON_H
#define OVR_JSON_H

#include "OVR_RefCount.h"
#include "OVR_String.h"
#include "OVR_List.h"

namespace OVR {  

// JSONItemType describes the type of JSON item, specifying the type of
// data that can be obtained from it.
enum JSONItemType
{
    JSON_None      = 0,
    JSON_Null      = 1,
    JSON_Bool      = 2,
    JSON_Number    = 3,
    JSON_String    = 4,
    JSON_Array     = 5,
    JSON_Object    = 6
};

//-----------------------------------------------------------------------------
// ***** JSON

// JSON object represents a JSON node that can be either a root of the JSON tree
// or a child item. Every node has a type that describes what it is.
// New JSON trees are typically loaded with JSON::Load or created with JSON::Parse.

class JSON : public RefCountBase<JSON>, public ListNode<JSON>
{
protected:
    List<JSON>      Children;

public:
    JSONItemType    Type;       // Type of this JSON node.
    String          Name;       // Name part of the {Name, Value} pair in a parent object.
    String          Value;
    double          dValue;

public:
    ~JSON();

    // *** Creation of NEW JSON objects

    static JSON*    CreateObject()               { return new JSON(JSON_Object);}
    static JSON*    CreateNull()                 { return new JSON(JSON_Null); }
    static JSON*    CreateArray()                { return new JSON(JSON_Array); }
    static JSON*    CreateBool(bool b)           { return createHelper(JSON_Bool, b ? 1.0 : 0.0); }
    static JSON*    CreateNumber(double num)     { return createHelper(JSON_Number, num); }
    static JSON*    CreateString(const char *s)  { return createHelper(JSON_String, 0.0, s); }

    // Creates a new JSON object from parsing the given string.
    // Returns a null pointer and fills in *perror in case of parse error.
    static JSON*    Parse(const char* buff, const char** perror = 0);

    // Loads and parses a JSON object from a file.
    // Returns a null pointer and fills in *perror in case of parse error.
    static JSON*    Load(const char* path, const char** perror = 0);

    // Saves a JSON object to a file.
    bool            Save(const char* path);

    // *** Object Member Access

    // Child item access functions
    void            AddItem(const char *string, JSON* item);
    void            AddBoolItem(const char* name, bool b)            { AddItem(name, CreateBool(b)); }
    void            AddNumberItem(const char* name, double n)        { AddItem(name, CreateNumber(n)); }
    void            AddStringItem(const char* name, const char* s)   { AddItem(name, CreateString(s)); }

	// These provide access to child items of the list.
    bool            HasItems() const         { return Children.IsEmpty(); }
    // Returns first/last child item, or null if child list is empty.
    JSON*           GetFirstItem()           { return (!Children.IsEmpty()) ? Children.GetFirst() : 0; }
    JSON*           GetLastItem()            { return (!Children.IsEmpty()) ? Children.GetLast() : 0; }

    // Counts the number of items in the object; these methods are inefficient.
    unsigned        GetItemCount() const;
    JSON*           GetItemByIndex(unsigned i);
    JSON*           GetItemByName(const char* name);

    // Returns next item in a list of children; 0 if no more items exist.
    JSON*           GetNextItem(JSON* item)  { return Children.IsNull(item->pNext) ? 0 : item->pNext; }
    JSON*           GetPrevItem(JSON* item)  { return Children.IsNull(item->pPrev) ? 0 : item->pPrev; }

	// Value access with range checking where possible.
	// Using the JsonReader class is recommended instead of using these.
	bool			GetBoolValue() const;
	SInt32			GetInt32Value() const;
	SInt64			GetInt64Value() const;
	float			GetFloatValue() const;
	double			GetDoubleValue() const;
	const String &	GetStringValue() const;

	// *** Array Element Access

    // Add new elements to the end of array.
    void            AddArrayElement(JSON *item);
    void            AddArrayBool(bool b)			{ AddArrayElement(CreateBool(b)); }
    void            AddArrayNumber(double n)        { AddArrayElement(CreateNumber(n)); }
    void            AddArrayString(const char* s)   { AddArrayElement(CreateString(s)); }

    // Accessed array elements; these methods are inefficient.
    int             GetArraySize();
    double          GetArrayNumber(int index);
    const char*     GetArrayString(int index);

    char*           PrintValue(int depth, bool fmt);

protected:
    JSON(JSONItemType itemType = JSON_Object);

    static JSON*    createHelper(JSONItemType itemType, double dval, const char* strVal = 0);

    // JSON Parsing helper functions.
    const char*     parseValue(const char *buff, const char** perror);
    const char*     parseNumber(const char *num);
    const char*     parseArray(const char* value, const char** perror);
    const char*     parseObject(const char* value, const char** perror);
    const char*     parseString(const char* str, const char** perror);

    char*           PrintObject(int depth, bool fmt);
    char*           PrintArray(int depth, bool fmt);

	friend class JsonReader;
};

//-----------------------------------------------------------------------------
// ***** JsonReader

// Fast JSON reader. This reads one JSON node at a time.
//
// When used appropriately this class should result in easy to read, const
// correct code with maximum variable localization. Reading the JSON data with
// this class is not only fast but also safe in that variables or objects are
// either skipped or default initialized if the JSON file contains different
// data than expected.
//
// To be completely safe and to support backward / forward compatibility the
// JSON object names will have to be verified. This class will try to verify
// the object names with minimal effort. If the children of a node are read
// in the order they appear in the JSON file then using this class results in
// only one string comparison per child. Only if the children are read out
// of order, all children may have to be iterated to match a child name.
// This should, however, only happen when the code that writes out the JSON
// file has been changed without updating the code that reads back the data.
// Either way this class will do the right thing as long as the JSON tree is
// considered const and is not changed underneath this class.
//
// This is an example of how this class can be used to load a simplified indexed
// triangle model:
//
//	JSON * json = JSON::Load( "filename.json" );
//	const JsonReader model( json );
//	if ( model.IsObject() )
//	{
//		const JsonReader vertices( model.GetChildByName( "vertices" ) );
//		if ( vertices.IsArray() )
//		{
//			while ( !vertices.IsAtEndOfArray() )
//			{
//				const JsonReader vertex( vertices.GetNextArrayElement() );
//				if ( vertex.IsObject() )
//				{
//					const float x = vertex.GetChildFloatByName( "x", 0.0f );
//					const float y = vertex.GetChildFloatByName( "y", 0.0f );
//					const float z = vertex.GetChildFloatByName( "z", 0.0f );
//				}
//			}
//		}
//		const JsonReader indices( model.GetChildByName( "indices" ) );
//		if ( indices.IsArray() )
//		{
//			while ( !indices.IsAtEndOfArray() )
//			{
//				const int index = indices.GetNextArrayInt32( 0 );
//			}
//		}
//	}
//	json->Release();

class JsonReader
{
public:
					JsonReader( const JSON * json ) :
						Parent( json ),
						Child( json != NULL ? json->Children.GetFirst() : NULL ) {}

	bool			IsValid() const { return Parent != NULL; }
	bool			IsObject() const { return Parent != NULL && Parent->Type == JSON_Object; }
	bool			IsArray() const { return Parent != NULL && Parent->Type == JSON_Array; }
	bool			IsEndOfArray() const { OVR_ASSERT( Parent != NULL ); return Parent->Children.IsNull( Child ); }

	const JSON *	GetChildByName( const char * childName ) const;

	bool			GetChildBoolByName( const char * childName, const bool defaultValue = false ) const;
	SInt32			GetChildInt32ByName( const char * childName, const SInt32 defaultValue = 0 ) const;
	SInt64			GetChildInt64ByName( const char * childName, const SInt64 defaultValue = 0 ) const;
	float			GetChildFloatByName( const char * childName, const float defaultValue = 0.0f ) const;
	double			GetChildDoubleByName( const char * childName, const double defaultValue = 0.0 ) const;
	const String	GetChildStringByName( const char * childName, const String & defaultValue = String( "" ) ) const;

	const JSON *	GetNextArrayElement() const;

	bool			GetNextArrayBool( const bool defaultValue = false ) const;
	SInt32			GetNextArrayInt32( const SInt32 defaultValue = 0 ) const;
	SInt64			GetNextArrayInt64( const SInt64 defaultValue = 0 ) const;
	float			GetNextArrayFloat( const float defaultValue = 0.0f ) const;
	double			GetNextArrayDouble( const double defaultValue = 0.0 ) const;
	const String	GetNextArrayString( const String & defaultValue = String( "" ) ) const;

private:
	const JSON *			Parent;
	mutable const JSON *	Child;		// cached child pointer
};

}

#endif
