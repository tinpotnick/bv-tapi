
/*
    config.cpp, part of bv-tapi a SIP-TAPI bridge.
    Copyright (C) 2011  Nick Knight

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. 
*/

#include "sofia-bridge.h"
#import <msxml3.dll> raw_interfaces_only

#include "config.h"
#include "utilities.h"


#include <Shlobj.h>

static strstrmap config;
static strstrmap lines;

bool configLoaded = false;

std::string getFolderPath(void)
{
	char buf[MAX_PATH];
	SHGetFolderPath(0, CSIDL_SYSTEM, NULL, SHGFP_TYPE_DEFAULT, &buf[0]);

	return std::string(&buf[0]);
}


void loadConfig(void)
{

	if ( true ==configLoaded )
	{
		return;
	}
	configLoaded= true;

	HRESULT hr = CoInitialize(NULL);

	MSXML2::IXMLDOMDocumentPtr XMLDom;
	HRESULT retv;

	retv = XMLDom.CreateInstance("Microsoft.XMLDOM");

	/*
		XML config file with the format of:

		<?xml version="1.0" encoding="utf-8"?>
		<bv>
			<config>
				<reg_url></reg_url>
				<reg_secret></reg_secret>
				<debug_file></debug_file>
			</config>
			<xcapcache>
				<resource-lists xmlns="urn:ietf:params:xml:ns:resource-lists">
					<list name="babblevoice">
						<entry uri="sip:1000@omniis.babblevoice.com">
							<display-name>Nick Knight</display-name>
						</entry>
						<entry uri="sip:1001@omniis.babblevoice.com">
							<display-name>Spare</display-name>
						</entry>
					</list>
				</resource-lists>
			</xcapcache>
		</bv>

		There are two main sections, the config section which we only support reg_url, reg_secret, and debug_file
		if debug_file is empty no debugging happens.

		xcapcache is used to store lines - but is in xcap format so we can get all our lines frm an xcap server.
	*/

	VARIANT_BOOL isSucess;
	VARIANT filename;
	std::string reg_url;

	filename.vt = VT_BSTR;
#ifdef _DEBUG
	filename.bstrVal = _bstr_t("../bv_tapi.xml");
#else
	std::string tmp = getFolderPath() + "\\bv_tapi.xml";
	filename.bstrVal = _bstr_t(tmp.c_str());
#endif
	
	if ( S_OK != XMLDom->load(filename, &isSucess))
	{
		bv_logger("Failed to load config\n");
		return;
	}

	config.clear();
	lines.clear();

	bv_logger("reset config, config items %u, lines now at %u.\n", config.size(), lines.size());
	
	MSXML2::IXMLDOMNodePtr node;
	if ( S_OK == ( retv = XMLDom->selectSingleNode(_bstr_t("//bv/config/reg_url").GetBSTR(), &node) ) )
	{
		BSTR url;
		node->get_text(&url);
		reg_url = frombstr(_bstr_t(url));
		bv_logger("reg_url: %s\n", reg_url.c_str());
		config["reg_url"] = reg_url;
		SysFreeString(url);
	}

	if ( S_OK == ( retv = XMLDom->selectSingleNode(_bstr_t("//bv/config/reg_secret").GetBSTR(), &node) ) )
	{
		BSTR secret;
		node->get_text(&secret);
		bv_logger("reg_secret: %s\n", frombstr(_bstr_t(secret)).c_str());
		config["reg_secret"] = frombstr(_bstr_t(secret));
		SysFreeString(secret);
	}

	if ( S_OK == ( retv = XMLDom->selectSingleNode(_bstr_t("//bv/config/debug_file").GetBSTR(), &node) ) )
	{
		BSTR file;
		node->get_text(&file);
		bv_logger("debug_file: %s\n", frombstr(_bstr_t(file)).c_str());
		config["debug_file"] = frombstr(_bstr_t(file));
		SysFreeString(file);
	}

	// now load our lines/entries
	if ( S_OK == ( retv = XMLDom->selectSingleNode(_bstr_t("//bv/xcapcache/resource-lists/list"), &node) ) )
	{
		MSXML2::IXMLDOMNodePtr childnode;
		MSXML2::IXMLDOMNamedNodeMap* entry_attributes;
		MSXML2::IXMLDOMNodePtr entry_uri;
		if ( S_OK == node->get_firstChild(&childnode) )
		{
			BSTR dispname = NULL;
			BSTR uri = NULL;
			childnode->get_attributes(&entry_attributes);

			entry_attributes->getNamedItem(_bstr_t("uri"), &entry_uri);
			entry_uri->get_text(&uri);

			childnode->get_text(&dispname);

			if ( reg_url != frombstr(uri) )
			{
				bv_logger("Found line: %s\n", frombstr(dispname).c_str());
				lines.insert(strstrpair(frombstr(dispname), frombstr(uri)));
				bv_logger("Number of lines now at %u.\n", lines.size());
			}
			SysFreeString(dispname);
			SysFreeString(uri);

			MSXML2::IXMLDOMNodePtr nextchildnode;
			while ( S_OK == childnode->get_nextSibling(&nextchildnode) )
			{
				nextchildnode->get_attributes(&entry_attributes);

				entry_attributes->getNamedItem(_bstr_t("uri"), &entry_uri);
				entry_uri->get_text(&uri);

				nextchildnode->get_text(&dispname);

				if ( reg_url != frombstr(uri) )
				{
					bv_logger("Found line: %s\n", frombstr(dispname).c_str());
					lines.insert(strstrpair(frombstr(dispname), frombstr(uri)));
					bv_logger("Number of lines now at %u.\n", lines.size());
				}

				SysFreeString(dispname);
				SysFreeString(uri);

				childnode = nextchildnode;
			}
		}
	}
}


void saveConfig(void)
{
	HRESULT hr = CoInitialize(NULL);

	MSXML2::IXMLDOMDocumentPtr XMLDom;
	HRESULT retv;

	retv = XMLDom.CreateInstance("Microsoft.XMLDOM");

	MSXML2::IXMLDOMProcessingInstructionPtr pi;
	XMLDom->createProcessingInstruction(_bstr_t("xml"), _bstr_t(" version='1.0' encoding='ISO-8859-1'"), &pi );
	MSXML2::IXMLDOMNodePtr header;
	XMLDom->appendChild(pi, &header);

	MSXML2::IXMLDOMElementPtr el;
	MSXML2::IXMLDOMNodePtr bv;

	XMLDom->createElement(_bstr_t("bv"), &el);
	XMLDom->appendChild(el, &bv);


	MSXML2::IXMLDOMNodePtr confignode;
	XMLDom->createElement(_bstr_t("config"), &el);
	bv->appendChild(el, &confignode);

	strstrmap::iterator it;

	for ( it = config.begin(); it != config.end(); it ++)
	{
		MSXML2::IXMLDOMNodePtr reg_url;
		XMLDom->createElement(_bstr_t((*it).first.c_str()), &el);
		confignode->appendChild(el, &reg_url);
		reg_url->put_text(_bstr_t((*it).second.c_str()));
	}

	// //bv/xcapcapcache
	MSXML2::IXMLDOMNodePtr xcapcachenode;
	XMLDom->createElement(_bstr_t("xcapcache"), &el);
	bv->appendChild(el, &xcapcachenode);

	// //bv/xcapcapcache/resource-lists
	MSXML2::IXMLDOMNodePtr resource_list;
	XMLDom->createElement(_bstr_t("resource-lists"), &el);
	xcapcachenode->appendChild(el, &resource_list);

	// add our xmlns attribute...
	MSXML2::IXMLDOMAttribute* rlist_attributes;
	MSXML2::IXMLDOMNamedNodeMap*  rlist_atrrib_nodemap;
	MSXML2::IXMLDOMNode* atribbewnode;

	XMLDom->createAttribute(_bstr_t("xmlns"), &rlist_attributes);
	rlist_attributes->put_text(_bstr_t("urn:ietf:params:xml:ns:resource-lists"));

	resource_list->get_attributes(&rlist_atrrib_nodemap);
	rlist_atrrib_nodemap->setNamedItem(rlist_attributes, &atribbewnode);


	// //bv/xcapcapcache/resource-lists
	MSXML2::IXMLDOMNodePtr _list;
	XMLDom->createElement(_bstr_t("list"), &el);
	resource_list->appendChild(el, &_list);

	// add a name attribute
	MSXML2::IXMLDOMAttribute* list_attributes;
	MSXML2::IXMLDOMNamedNodeMap*  list_atrrib_nodemap;

	XMLDom->createAttribute(_bstr_t("name"), &list_attributes);
	list_attributes->put_text(_bstr_t("babblevoice"));

	_list->get_attributes(&list_atrrib_nodemap);
	list_atrrib_nodemap->setNamedItem(list_attributes, &atribbewnode);
	
	// Now for the entries
	for ( it = lines.begin(); it != lines.end(); it ++)
	{
		MSXML2::IXMLDOMNodePtr entry;
		XMLDom->createElement(_bstr_t("entry"), &el);
		_list->appendChild(el, &entry);

		// add uri
		MSXML2::IXMLDOMAttribute* entry_attributes;
		MSXML2::IXMLDOMNamedNodeMap*  entry_atrrib_nodemap;

		XMLDom->createAttribute(_bstr_t("uri"), &entry_attributes);
		entry_attributes->put_text(_bstr_t((*it).second.c_str()));
		
		entry->get_attributes(&entry_atrrib_nodemap);
		entry_atrrib_nodemap->setNamedItem(entry_attributes, &atribbewnode);
		

		//now add display-name
		MSXML2::IXMLDOMNodePtr disp_name;
		XMLDom->createElement(_bstr_t("display-name"), &el);
		entry->appendChild(el, &disp_name);

		disp_name->put_text(_bstr_t((*it).first.c_str()));
	}

	// Now save it
	VARIANT filename;
	filename.vt = VT_BSTR;
#ifdef _DEBUG
	filename.bstrVal = _bstr_t("../bv_tapi.xml");
#else
	std::string tmp = getFolderPath() + "\\bv_tapi.xml";
	filename.bstrVal = _bstr_t(tmp.c_str());
#endif


	XMLDom->save(filename);


}

std::string getConfigValue(std::string key)
{
	if ( false == configLoaded )
	{
		loadConfig();
	}

	return config[key];
}

bool setConfigValue(std::string key, std::string value)
{
	config[key] = value;
	return true;
}

bool setProxyServer(std::string key, std::string value)
{
	if (value != "")
	{
		#define SIP_TEST_PROXY value;
	}
}


strstrmap getLines(void)
{
	if ( false == configLoaded )
	{
		loadConfig();
	}

	return lines;
}

strstrpair getLineByIndex(int pos)
{
	strstrmap::iterator it = lines.begin();

	for ( int i = 0; i < pos; i++ )
	{
		it++;

		if ( it == lines.end() )
		{
			break;
		}
	}
		
	return *it;
}

bool addLine(std::string name, std::string url)
{

	configLoaded = true;

	strstrmap::iterator it;

	for ( it  = lines.begin(); it != lines.end(); it++ )
	{
		if ((*it).second == url)
		{
			// duplicate
			return false;
		}
	}

	// cool, go for it.
	lines.insert(strstrpair(name, url));
	return true;
}



void removeLine(std::string name)
{
	strstrmap::iterator it;

	for ( it  = lines.begin(); it != lines.end(); it++ )
	{
		if ((*it).first == name)
		{
			lines.erase(it);
			return;
		}
	}
}