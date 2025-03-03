//%LICENSE////////////////////////////////////////////////////////////////
//
// Licensed to The Open Group (TOG) under one or more contributor license
// agreements.  Refer to the OpenPegasusNOTICE.txt file distributed with
// this work for additional information regarding copyright ownership.
// Each contributor licenses this file to you under the OpenPegasus Open
// Source License; you may not use this file except in compliance with the
// License.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//////////////////////////////////////////////////////////////////////////
//
//%/////////////////////////////////////////////////////////////////////////////

#include <Pegasus/Common/PegasusAssert.h>
#include <Pegasus/Common/Config.h>
#include <Pegasus/Common/FileSystem.h>
#include <Pegasus/Common/XmlWriter.h>
#include <Pegasus/Common/XmlReader.h>
#include <Pegasus/Repository/CIMRepository.h>

PEGASUS_USING_PEGASUS;
PEGASUS_USING_STD;

String namespaceName;
static Boolean verbose = false;



Boolean ProcessClaseOrInstanceElement(CIMRepository& repository, XmlParser& parser)
{
	CIMClass cimClass, tmpClass;
	CIMQualifierDecl qualifierDecl;
	CIMInstance instance;

	if (XmlReader::getClassElement(parser, cimClass))
	{
		if (verbose)
		{
			cout << "Creating: class ";
			cout << cimClass.getClassName().getString() << endl;
		}

		try
		{
			repository.createClass(namespaceName, cimClass);
		}
		catch (CIMException &e)
		{
			if (e.getCode() != CIM_ERR_ALREADY_EXISTS)
			{
				throw;
			}
		}
		catch (Exception &e)
		{
			// Ignore if cimClass already exists
			if (e.getMessage() != cimClass.getClassName())
			{
				throw;
			}
		}
	}
	else if (XmlReader::getQualifierDeclElement(parser, qualifierDecl))
	{
		if (verbose)
		{
			cout << "Creating: qualifier ";
			cout << qualifierDecl.getName().getString() << endl;
		}

		try
		{
			repository.setQualifier(namespaceName, qualifierDecl);
		}
		catch (CIMException &e)
		{
			if (e.getCode() != CIM_ERR_ALREADY_EXISTS  && 
				e.getMessage() != qualifierDecl.getName())
			{
				throw;
			}
		}
		catch (Exception &e)
		{
			// Ignore if qualifierDecl already exists
			if (e.getMessage() != qualifierDecl.getName())
			{
				throw;
			}
		}
	}
	else if (XmlReader::getInstanceElement(parser, instance))
	{
		if (verbose)
		{
			cout << "Creating: instance ";
			cout << instance.getPath().toString() << endl;
		}

		try
		{
			repository.createInstance(namespaceName, instance);
		}
		catch (CIMException &e)
		{
			if (e.getCode() != CIM_ERR_ALREADY_EXISTS)
			{
				throw;
			}
		}
		catch (Exception &e)
		{
			// Ignore if qualifierDecl already exists
			if (e.getMessage() != instance.getPath())
			{
				throw;
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------------
// ProcessValueObjectElement()
//
//     <!ELEMENT VALUE.OBJECT (CLASS|INSTANCE)>
//
// ATTN: Nothing handled but CLASS.
//
//------------------------------------------------------------------------------

Boolean ProcessValueObjectElement(CIMRepository& repository, XmlParser& parser)
{
	XmlEntry entry;

	if (!XmlReader::testStartTag(parser, entry, "VALUE.OBJECT"))
	{
		return false;
	}
	Boolean ret = ProcessClaseOrInstanceElement(repository, parser);
	XmlReader::expectEndTag(parser, "VALUE.OBJECT");
	return ret;
}

//------------------------------------------------------------------------------
// ProcessDeclGroupElement()
//
//     <!ELEMENT DECLGROUP ((LOCALNAMESPACEPATH|NAMESPACEPATH)?,
//         QUALIFIER.DECLARATION*,VALUE.OBJECT*)>
//
// ATTN: Nothing handled but VALUE.OBJECT:
//
//------------------------------------------------------------------------------

Boolean ProcessDeclGroupElement(CIMRepository& repository, XmlParser& parser)
{
    XmlEntry entry;

    if (!XmlReader::testStartTag(parser, entry, "DECLGROUP"))
    {
        return false;
    }

    while (ProcessValueObjectElement(repository, parser))
        ;

    XmlReader::expectEndTag(parser, "DECLGROUP");

    return true;
}

//------------------------------------------------------------------------------
// ProcessDeclarationElement()
//
//     <!ELEMENT DECLARATION (DECLGROUP|DECLGROUP.WITHNAME|DECLGROUP.WITHPATH)*>
//
// ATTN: DECLGROUP.WITHNAME ande DECLGROUP.WITHPATH not handled:
//
//------------------------------------------------------------------------------

Boolean ProcessDeclarationElement(CIMRepository& repository, XmlParser& parser)
{
    XmlEntry entry;

    if (!XmlReader::testStartTag(parser, entry, "DECLARATION"))
    {
        return false;
    }

    while(ProcessDeclGroupElement(repository, parser))
        ;

    XmlReader::expectEndTag(parser, "DECLARATION");

    return true;
}

//------------------------------------------------------------------------------
// ProcessCimElement()
//
//     <!ELEMENT CIM (MESSAGE|DECLARATION)>
//     <!ATTLIST CIM
//         CIMVERSION CDATA #REQUIRED
//         DTDVERSION CDATA #REQUIRED>
//
// ATTN: only declarations are handled here!
//
//------------------------------------------------------------------------------

Boolean ProcessCimElement(CIMRepository& repository, XmlParser& parser)
{
    XmlEntry entry;

    if (!parser.next(entry) || entry.type != XmlEntry::XML_DECLARATION)
    {
		// throw XmlValidationError(parser.getLine(),
		//	"Expected XML declaration");
		parser.putBack(entry);
    }

    if (XmlReader::testStartTag(parser, entry, "CIM"))
    {
		String cimVersion;

		if (!entry.getAttributeValue("CIMVERSION", cimVersion))
		{
			throw XmlValidationError(parser.getLine(),
				"missing CIM.CIMVERSION attribute");
		}

		String dtdVersion;

		if (!entry.getAttributeValue("DTDVERSION", dtdVersion))
		{
			throw XmlValidationError(parser.getLine(),
				"missing CIM.DTDVERSION attribute");
		}

		if (!ProcessDeclarationElement(repository, parser))
		{
			throw XmlValidationError(parser.getLine(),
				"Expected DECLARATION element");
		}

		XmlReader::expectEndTag(parser, "CIM");
		return true;
    }

	return ProcessClaseOrInstanceElement(repository, parser);
}

//------------------------------------------------------------------------------
//
// _processFile()
//
//------------------------------------------------------------------------------

static void _processFile(CIMRepository &repository, const char* xmlFileName)
{
    // Create the parser:

    Buffer text;
    text.reserveCapacity(1024 * 1024);
    FileSystem::loadFileToMemory(text, xmlFileName);
    XmlParser parser((char*)text.getData());


	if (!ProcessCimElement(repository, parser))
	{
		cerr << "CIM root element missing" << endl;
		exit(1);
	}
}


static void _processDirectory(CIMRepository &repository, const char* dir) {
	Array<String> filenames;
	if (!FileSystem::glob(dir, "*.xml", filenames)) {
		cout << " read directory fail: " << dir << endl;
		return ;
	}


	for (; filenames.size() > 0;)
	{
		Array<String> remain;

		for (int i = 0; i < filenames.size(); i++)
		{
			try
			{
				_processFile(repository, filenames[i].getCString());
				cout << filenames[i] << " loaded." << endl;
			}
			catch (const Exception& e)
			{
				remain.append(filenames[i]);
			}
		}

		if (filenames.size() == remain.size()) {
			for (int i = 0; i < filenames.size(); i++)
			{
				try
				{
					_processFile(repository, filenames[i].getCString());
					cout << filenames[i] << " loaded." << endl;
				}
				catch (const Exception& e)
				{
					cerr << filenames[i] << ": " << e.getMessage() << endl;
				}
			}
			return;
		}
		filenames.clear();
		filenames.appendArray(remain);
	}
}
//------------------------------------------------------------------------------
//
// main()
//
//------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        cerr << "Usage: " << argv[0]
            << " repository-root namespace xmlfiles" << endl;
        exit(1);
    }

    verbose = getenv("PEGASUS_TEST_VERBOSE") ? true : false;

	String repositoryRoot = argv[1];
    namespaceName = argv[2];

	CIMRepository repository(repositoryRoot);
	try
	{
		repository.createNameSpace(namespaceName);
	}
	catch (Exception &e)
	{
		// Ignore if Namespace already exists
		if (e.getMessage() != namespaceName)
		{
			cerr << e.getMessage() << endl;
			return 1;
		}
	}

	for (int i = 3; i < argc; i++) 
	{
		try
		{
			if (FileSystem::isDirectory(argv[i])) {
				_processDirectory(repository, argv[i]);
			}
			else 
			{
				_processFile(repository, argv[i]);
			}
			cout << "file: " << argv[i] << endl;
		}
		catch (const Exception& e)
		{
			cerr << e.getMessage() << endl;
		}
	}

    return 0;
}
