/*
Name: tapiastmanager.cpp
Copyright: Under the Mozilla Public License Version 1.1 or later
Author: Nick Knight
Date: 19/04/04 15:00
Description: ties the astmanager connection to the extra information required for TAPI
*/
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Asttapi.
 *
 * The Initial Developer of the Original Code is
 * Nick Knight.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): none
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 or later (the "GPL")
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


//we have to make sure we are thread safe - so need a mutex
#include "utilities.h"
#include "tapiastmanager.h"
#include "wavetsp.h"
#include <algorithm>
#include <cctype>

tapiAstManager::tapiAstManager(void)
{
	this->lineEvent = 0;
	this->listThread = NULL;
}

tapiAstManager::~tapiAstManager(void)
{
	this->dropConnection();

	if ( this->listThread != NULL )
	{
		switch (WaitForSingleObject( this->listThread, 50000))
		{
		case WAIT_OBJECT_0:
			//  The thread has terminated - do something
			TSPTRACE("apiAstManager::~tapiAstManager Thread terminated nicley\n");
			break;

		case WAIT_TIMEOUT:
			TerminateThread(this->listThread,0);
			TSPTRACE("apiAstManager::~tapiAstManager killed the thread\n");
			//  The timeout has elapsed but the thread is still running
			//  do something appropriate for a timeout
			break;
		}

		CloseHandle(this->listThread);
	}
}

//V0.0.7.0
void tapiAstManager::setSimpleDial(BOOL blSimpleDial)
{
	this->SimpleDial = blSimpleDial;
	return;
}
//V0.0.7.0

//V0.0.7.0
BOOL tapiAstManager::getSimpleDial(void)
{
	return this->SimpleDial;
}
//V0.0.7.0

//V0.0.7.0
void tapiAstManager::tapiSendCompletion(DWORD dwRequest, DWORD dwCompletion)
{
	if ( this->tapiCompletionProc != NULL)
	{
		TSPTRACE("tapiAstManager::tapiSendCompletion(%lu,%lu)\n", dwRequest, dwCompletion);
		this->tapiCompletionProc(dwRequest, dwCompletion);
	}
}
//V0.0.7.0


DWORD tapiAstManager::setTapiLine(HTAPILINE line)
{
	this->htLine = line;
	return 0;
}

HTAPILINE tapiAstManager::getTapiLine(void)
{
	return this->htLine;
}


DWORD tapiAstManager::processMessages(void)
{
	std::string strData;

	//make sure we are logged in
	//V0.0.7.0
	TSPTRACE("tapiAstManager::processMessages astConnect() and login() next\n");
	//V0.0.7.0
	this->astConnect();
	this->login();

	//TODO perhaps some checking on the login - although the connection is 
	//dropped by Asterisk if the login is not accepted
	astEvent *ev = 0;

	while ( (ev = this->waitForMessage()) != NULL )
	{
		if ( ev->uniqueid == "" )
		{
			delete ev;
			ev = 0;
			continue;
		}
		try
		{
			TSPTRACE("Unique ID: %s\n",ev->uniqueid.c_str());
			TSPTRACE("TAPI data: %s\n",ev->tapidata.c_str());

			//First off find the associated call object
			astTspGlue tmp;

			
			//A unique ID is the channel name - we are only interested in the leg 
			//to our phone, this needs to be mapped onto a line, so the first message 
			//which we must be given from Asterisk is LINE_NEWCALL which should be 
			//followed with teh line name i.e. LINE_NEWCALL line_1
			//When we receive this we signal the TAPI app, and also make
			//a note of the channel so that we can track it
			
			if ( ev->tapidata.find("LINE_NEWCALL ") != -1 )
			{
				std::string line_name;
				line_name = ev->tapidata.substr(sizeof("LINE_NEWCALL"), ev->tapidata.length());
				//LINE_NEWCALL command takes on the format
				//
				// LINE_NEWCALL line_ident&secon_ident....
				//
				// This way we can group lines together - such as a queue of
				// some form

				stringList line_identifiers = splitString(line_name, '&');
				stringList::iterator line_it = line_identifiers.begin();

				while ( line_it != line_identifiers.end() )
				{
					if ( this->lineName == (*line_it) )
					{
						break;
					}
					line_it++;
				}

				if ( line_it == line_identifiers.end() )
				{
					delete ev;
					ev = 0;
					continue;
				}

				//We may have received this indication in two situations
				// 1. Where TAPI has initiated a call 
				// 2. Where a new call has been initiated by other means - i.e. terminated
				//
				// in the case of 1, we need to block it here as we shouldn't inidcate this
				// we check for this by checking if we know about the call about it already
				tspMut.Lock();
				mapCall::iterator it = trackCalls.find(ev->uniqueid);
				tspMut.Unlock();

				if ( it == trackCalls.end() )
				{
					TSPTRACE("New call detected %s uniqueID\n",ev->uniqueid.c_str() );

					tmp.setTapiLine(this->htLine);
					tmp.setLineEvent(this->lineEvent);
					tmp.signalTapiNewCall();
					tmp.setUniqueID(ev->uniqueid);

					tspMut.Lock();    //ensure we are thread safe again.
					trackCalls[ev->uniqueid] = tmp;
					tspMut.Unlock();
				}

				delete ev;
				ev = 0;
				continue;
			}

			
			// we can now filter signals based on the line identifiers
			// a line identifier should look like
			//
			// [line_ident] LINE_LINEDEVSTATE LINEDEVSTATE_RINGING
			// 
			// of course it can be left out.
			// we can also use the '!' to exclude
			//
			// so if our line identifier is 'bob' then
			// [bob] LINE_LINEDEVSTATE LINEDEVSTATE_RINGING
			// but
			// [!bob] LINE_LINEDEVSTATE LINEDEVSTATE_RINGING
			// wouldn't

			DWORD st_lineid, en_lineid;
			if ( (st_lineid = ev->tapidata.find('[')) != -1 )
			{
				// We have the option to signal specific lines identifiers.
				if ( (en_lineid = ev->tapidata.find(']')) != -1 )
				{
					// BUG Using substr wrong
					//std::string lineid = ev->tapidata.substr(st_lineid+1, ev->tapidata.length() - st_lineid - en_lineid - 1);
					std::string lineid = ev->tapidata.substr(st_lineid+1, en_lineid-st_lineid-1);
					TSPTRACE("Line id looking for is %s\n", lineid.c_str());

					//Chop of the line id part...
					ev->tapidata = ev->tapidata.substr(en_lineid+2, ev->tapidata.length());
					TSPTRACE("Remaining command is %s\n", ev->tapidata.c_str());

					stringList line_identifiers = splitString(lineid, '&');
					stringList::iterator line_it = line_identifiers.begin();
					
					//Now we follow our rules
					// 1. By default if it gets here we are signalled
					// 2. If it becomes specific i.e. [bart&lisa] and we are holmer
					//    then we ignore it
					// 3. If we are holmer then [!holmer] would leave us out, but include
					//    everyone else
					bool specific = false;
					bool notus = false;
					while ( line_it != line_identifiers.end() )
					{
						std::string line_id_item = (*line_it);
						if ( line_id_item[0] == '!' )
						{
							std::string negate = line_id_item.substr(1,line_id_item.length());
							TSPTRACE("Not allowing %s\n", negate.c_str());
							if ( negate == this->lineName )
							{
								notus = true;
								break;
							}
						}
						else if ( line_id_item[0] == '~' )
						{
							std::string newunqid = line_id_item.substr(1,line_id_item.length());
							TSPTRACE("Pretending event came from ID %s\n", newunqid.c_str());
							ev->uniqueid = newunqid;
						}
						else
						{
							specific = true;
							TSPTRACE("Looking for line %s, found %s\n", line_id_item.c_str(), this->lineName.c_str());
							if ( line_id_item == this->lineName )
							{
								break;
							}
						}
						line_it++;
					}
					
					if ( notus == true )
					{
						delete ev;
						ev = 0;
						continue;
					}
					// If we have found not found a match and it is specific.
					if ( specific == true && line_it == line_identifiers.end())
					{
						delete ev;
						ev = 0;
						continue;
					}
					//If we get here it is probably for us.
				}
			}

			//are already interested in this channel - i.e.
			//already have this unique id stored
			TSPTRACE("Number of entries in trackCalls %i\n",trackCalls.size());

			//now make sure our iterator is correct.
			tspMut.Lock();
			mapCall::iterator it = trackCalls.find(ev->uniqueid);
			tspMut.Unlock();

			if ( it == trackCalls.end() )
			{
				TSPTRACE("Not a channel we are interested in\n");
				delete ev;
				ev = 0;
				continue;
			}

			//We may not have set the channel name against the call
			//so we should check here
			(*it).second.setChannelName(ev->channel);


			//At this point we can search for the TAPI messages we wish to process.
			
			//LINE_LINEDEVSTATE
			if ( ev->tapidata.find("LINE_LINEDEVSTATE ") == 0 )
			{
				if ( ev->tapidata.find(" LINEDEVSTATE_RINGING") != -1 )
				{
					(*it).second.signalEvent(LINE_LINEDEVSTATE, LINEDEVSTATE_RINGING,0,0);
				}
			}

			//LINE_CALLSTATE
			if ( ev->tapidata.find("LINE_CALLSTATE ") == 0 )
			{
				if ( ev->tapidata.find(" LINECALLSTATE_OFFERING") != -1 )
				{
					(*it).second.signalEvent(LINE_CALLSTATE, LINECALLSTATE_OFFERING,0,0);
				}
				if ( ev->tapidata.find(" LINECALLSTATE_IDLE") != -1 )
				{
					(*it).second.signalEvent(LINE_CALLSTATE, LINECALLSTATE_IDLE,0,0);
					//When we drop the call we also remove it from the list which we are
					//monitoring
					tspMut.Lock();
					trackCalls.erase(it);
					tspMut.Unlock();
				}
				if ( ev->tapidata.find(" LINECALLSTATE_CONNECTED") != -1 )
				{
					(*it).second.signalEvent(LINE_CALLSTATE, LINECALLSTATE_CONNECTED,0,0);
				}
				if ( ev->tapidata.find(" LINECALLSTATE_DIALING") != -1 )
				{
					(*it).second.signalEvent(LINE_CALLSTATE, LINECALLSTATE_DIALING,0,0);
				}
				if ( ev->tapidata.find(" LINECALLSTATE_DISCONNECTED") != -1 )
				{
					(*it).second.signalEvent(LINE_CALLSTATE, LINECALLSTATE_DISCONNECTED,0,0);
				}
				if ( ev->tapidata.find(" LINECALLSTATE_UNKNOWN") != -1 )
				{
					(*it).second.signalEvent(LINE_CALLSTATE, LINECALLSTATE_UNKNOWN,0,0);
				}
				if ( ev->tapidata.find(" LINECALLSTATE_ACCEPTED") != -1 )
				{
					(*it).second.signalEvent(LINE_CALLSTATE, LINECALLSTATE_ACCEPTED,0,0);
				}
				if ( ev->tapidata.find(" LINECALLSTATE_ONHOLDPENDTRANSFER") != -1 )
				{
					(*it).second.signalEvent(LINE_CALLSTATE, LINECALLSTATE_ONHOLDPENDTRANSFER,0,0);
				}
				if ( ev->tapidata.find(" LINECALLSTATE_ONHOLDPENDCONF") != -1 )
				{
					(*it).second.signalEvent(LINE_CALLSTATE, LINECALLSTATE_ONHOLDPENDCONF,0,0);
				}
				if ( ev->tapidata.find(" LINECALLSTATE_CONFERENCED") != -1 )
				{
					(*it).second.signalEvent(LINE_CALLSTATE, LINECALLSTATE_CONFERENCED,0,0);
				}
				if ( ev->tapidata.find(" LINECALLSTATE_ONHOLD") != -1 )
				{
					(*it).second.signalEvent(LINE_CALLSTATE, LINECALLSTATE_ONHOLD,0,0);
				}
				if ( ev->tapidata.find(" LINECALLSTATE_PROCEEDING") != -1 )
				{
					(*it).second.signalEvent(LINE_CALLSTATE, LINECALLSTATE_PROCEEDING,0,0);
				}
				if ( ev->tapidata.find(" LINECALLSTATE_SPECIALINFO") != -1 )
				{
					(*it).second.signalEvent(LINE_CALLSTATE, LINECALLSTATE_SPECIALINFO,0,0);
				}
				if ( ev->tapidata.find(" LINECALLSTATE_BUSY") != -1 )
				{
					(*it).second.signalEvent(LINE_CALLSTATE, LINECALLSTATE_BUSY,0,0);
				}
				if ( ev->tapidata.find(" LINECALLSTATE_RINGBACK") != -1 )
				{
					(*it).second.signalEvent(LINE_CALLSTATE, LINECALLSTATE_RINGBACK,0,0);
				}
				if ( ev->tapidata.find(" LINECALLSTATE_DIALTONE") != -1 )
				{
					(*it).second.signalEvent(LINE_CALLSTATE, LINECALLSTATE_DIALTONE,0,0);
				}
				if ( ev->tapidata.find(" LINECALLSTATE_ACCEPTED") != -1 )
				{
					(*it).second.signalEvent(LINE_CALLSTATE, LINECALLSTATE_ACCEPTED,0,0);
				}

			}

			//LINE_CALLINFO
			if ( ev->tapidata.find("LINE_CALLINFO ") == 0 )
			{
				if ( ev->tapidata.find(" LINECALLINFOSTATE_CALLERID") != -1 )
				{
					(*it).second.signalEvent(LINE_CALLINFO, LINECALLINFOSTATE_CALLERID,0,0);
				}
			}

			//This area takes on a slightly different view, it takes 
			//all of the data we can possibly set
			if ( ev->tapidata.find("SET ") == 0 )
			{
				int value_pos;

				if ( ( value_pos = ev->tapidata.find(" CALLERID ") ) != -1 )
				{
					//This should be in the format containing numbers only
					(*it).second.setCallerID(ev->tapidata.substr(value_pos+sizeof(" CALLERID") , ev->tapidata.length()));
				}
				if ( ( value_pos = ev->tapidata.find(" CALLERIDNAME ") ) != -1 )
				{
					//This will grab all of the name
					(*it).second.setCallerIDName(ev->tapidata.substr(value_pos+sizeof(" CALLERIDNAME") , ev->tapidata.length()));
				}
				if ( ( value_pos = ev->tapidata.find(" CALLEDID ") ) != -1 )
				{
					//Number only
					(*it).second.setCalledID(ev->tapidata.substr(value_pos+sizeof(" CALLEDID") , ev->tapidata.length()));
				}
				if ( ( value_pos = ev->tapidata.find(" CALLEDIDNAME ") ) != -1 )
				{
					//All of the text
					(*it).second.setCalledIDName(ev->tapidata.substr(value_pos+sizeof(" CALLEDIDNAME") , ev->tapidata.length()));
				}
				if ( ( value_pos = ev->tapidata.find(" CONNECTEDID ") ) != -1 )
				{
					//Number only
					(*it).second.setConnectedID(ev->tapidata.substr(value_pos+sizeof(" CONNECTEDID") , ev->tapidata.length()));
				}
				if ( ( value_pos = ev->tapidata.find(" CONNECTEDIDNAME ") ) != -1 )
				{
					//All of the text
					(*it).second.setConnectedIDName(ev->tapidata.substr(value_pos+sizeof(" CONNECTEDIDNAME") , ev->tapidata.length()));
				}

				if ( ev->tapidata.find(" CALLORIGIN ") != -1 )
				{
					if ( ev->tapidata.find("LINECALLORIGIN_OUTBOUND") )
					{
						(*it).second.setOrigin(LINECALLORIGIN_OUTBOUND);
					}
					if ( ev->tapidata.find("LINECALLORIGIN_INTERNAL") )
					{
						(*it).second.setOrigin(LINECALLORIGIN_INTERNAL);
					}
					if ( ev->tapidata.find("LINECALLORIGIN_EXTERNAL") )
					{
						(*it).second.setOrigin(LINECALLORIGIN_EXTERNAL);
					}
					if ( ev->tapidata.find("LINECALLORIGIN_UNKNOWN") )
					{
						(*it).second.setOrigin(LINECALLORIGIN_UNKNOWN);
					}
					if ( ev->tapidata.find("LINECALLORIGIN_UNAVAIL") )
					{
						(*it).second.setOrigin(LINECALLORIGIN_UNAVAIL);
					}
					if ( ev->tapidata.find("LINECALLORIGIN_CONFERENCE") )
					{
						(*it).second.setOrigin(LINECALLORIGIN_CONFERENCE);
					}
					if ( ev->tapidata.find("LINECALLORIGIN_INBOUND") )
					{
						(*it).second.setOrigin(LINECALLORIGIN_INBOUND);
					}
				}
			}
			

			delete ev;
			ev = 0;

		}
		catch(...)
		{
			TSPTRACE("tapiAstManager::processMessages exception\n");

			if ( ev )
			{
				if ( ev )
				{
					delete ev;
					ev = 0;
				}
				tspMut.Unlock();
			}

		}

	}

	TSPTRACE("completing tapiAstManager::processMessages\n");
	return 0;
}

#if 0
DWORD tapiAstManager::addCall(astTspGlue call)
{
	BEGIN_PARAM_TABLE("TSPI_lineMakeCall")
		DWORD_IN_ENTRY(0)
	END_PARAM_TABLE()

	//we insert into th elist of possible calls
	//we have no defining link between this one and 
	//another... what to do!
	call.setTapiLine(this->htLine);
	call.setLineEvent(this->lineEvent);
	tspMut.Lock();
	trackCalls[""] = call;
	tspMut.Unlock();

	return EPILOG(0);
}
#endif

DWORD tapiAstManager::setLineEvent(LINEEVENT callBack)
{
	this->lineEvent = callBack;
	return 0;
}

astTspGlue * tapiAstManager::findCall(HDRVCALL tspCallID)
{
	tspMut.Lock();
	mapCall::iterator it;

	for ( it = this->trackCalls.begin() ; it != this->trackCalls.end() ; it ++ )
	{
		if ( (*it).second.getTspiCallID() == tspCallID )
		{
			tspMut.Unlock();
			return &(*it).second;
		}

	}

	tspMut.Unlock();
	return NULL;
}

DWORD tapiAstManager::getNumCalls(void)
{
	return this->trackCalls.size();
}
DWORD tapiAstManager::dropCall(HDRVCALL tspiID)
{
	astTspGlue *ourCall;

	TSPTRACE("tapiAstManager::dropCall called\n");

	if ( ( ourCall = this->findCall(tspiID)) != NULL )
	{
		TSPTRACE("tapiAstManager::dropCall call found, sending Hangup command\n");
		this->dropChannel(ourCall->getChannelName());
	}
	return 0;
}

DWORD tapiAstManager::handleResponce(bool sucsess, std::string message, std::string uniqueid, DWORD actionid)
{
	BEGIN_PARAM_TABLE("tapiAstManager::handleResponce")
		DWORD_IN_ENTRY(sucsess)
		DWORD_IN_ENTRY(this->tapiRequest)
		DWORD_IN_ENTRY(actionid)
	END_PARAM_TABLE()

	TSPTRACE("tapiAstManager::handleResponce message:%s\n", message.c_str());
	TSPTRACE("tapiAstManager::handleResponce uniqueid:%s\n", uniqueid.c_str());

	std::transform(message.begin(), message.end(), message.begin(), tolower);
	
	if ( message.find("originate") != -1 )
	{
		if ( sucsess == true )
		{
			//Store and wait for further messages
			//BUG: Asterisk 1.2 no longer returns uniqueid with the response
			//Adding code to check for that and await response in the OriginateSuccess
			//or OriginateFailure events
			if (uniqueid.length() != 0)
			{
				if (actionid != 0 && this->originated.getActionID() != actionid)
				{
					TSPTRACE("tapiAstManager::handleResponce received originate but improper actionid\n");
					return 0;
				}

				//V0.0.7.0

				//tspMut.Lock();
				//this->originated.setUniqueID(uniqueid);
				//trackCalls[uniqueid] = this->originated;
				//tspMut.Unlock();
				//TSPTRACE("tapiAstManager::handleResponce LINE_REPLY\n");
				//if ( this->tapiCompletionProc != 0 )
				//{
				//	TSPTRACE("tapiAstManager::handleResponce /PASS\n");
				//	this->tapiCompletionProc(this->tapiRequest, 0);
				//}

				TSPTRACE("tapiAstManager::handleResponce LINE_REPLY\n");
				//If using SimpleDial, inform TAPI caller the call has completed
				//and leave Asterisk to get on with it. This will not actually happen
				//until the caller answers their phone (which then causes Asterisk
				//to place the call to the callee)
				if ( this->SimpleDial )
				{
					if ( this->tapiCompletionProc != 0 )
					{
						TSPTRACE("tapiAstManager::handleResponce /PASS\n");
						this->tapiCompletionProc(this->tapiRequest, 0);
					}
					TSPTRACE("tapiAstManager::handleResponce SimpleDial Signaling call end\n");
					this->originated.signalEvent(LINE_CALLSTATE, LINECALLSTATE_IDLE,0,0);
					this->dropConnection();
				}
				else
				{
					tspMut.Lock();
					this->originated.setUniqueID(uniqueid);
					trackCalls[uniqueid] = this->originated;
					tspMut.Unlock();
					if ( this->tapiCompletionProc != 0 )
					{
						TSPTRACE("tapiAstManager::handleResponce /PASS\n");
						this->tapiCompletionProc(this->tapiRequest, 0);
					}
				}
			}

			//V0.0.7.0

		}
		else
		{
			//Indicate the call has been dropped
			//this->originated.signalEvent(LINE_CALLSTATE, LINECALLSTATE_DISCONNECTED, LINEDISCONNECTMODE_CANCELLED ,0);
			//this->originated.signalEvent(LINE_CALLSTATE, LINECALLSTATE_IDLE,0,0);
			TSPTRACE("tapiAstManager::handleResponce LINE_REPLY\n");
			if ( this->tapiCompletionProc != 0 )
			{
				TSPTRACE("tapiAstManager::handleResponce /FAIL\n");
				this->tapiCompletionProc(this->tapiRequest, -1);
			}
		}
	}
	return 0;
}

void tapiAstManager::setAsyncComp(ASYNC_COMPLETION pfnCompletionProc)
{
	BEGIN_PARAM_TABLE("tapiAstManager::setAsyncComp")
		DWORD_IN_ENTRY(pfnCompletionProc)
	END_PARAM_TABLE()

	this->tapiCompletionProc = pfnCompletionProc;
}


DWORD tapiAstManager::originate(std::string destination, HTAPICALL call, DRV_REQUESTID requestID)
{
	BEGIN_PARAM_TABLE("tapiAstManager::originate")
		DWORD_IN_ENTRY(call)
		DWORD_IN_ENTRY(requestID)
	END_PARAM_TABLE()

	TSPTRACE("destination:%s\n", destination.c_str());

	tspMut.Lock();
	//By default we store what we dialed...
	this->originated.setCalledID(destination);
	this->originated.setTapiLine(this->htLine);
	this->originated.setLineEvent(this->lineEvent);
	this->originated.setActionID(requestID);
	//tmp.signalTapiNewCall();
	this->originated.setTapiCall(call);
	this->tapiRequest = requestID;
	tspMut.Unlock();

	//perform the origination
	astManager::originate(destination, requestID);
	//store our awaiting call info

	return 0;
}


void tapiAstManager::setThreadHandle(HANDLE thHandl)
{
	this->listThread = thHandl;
}

