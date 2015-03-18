/*
    PhoneCall.cpp, part of bv-tapi a SIP-TAPI bridge.
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

#include <time.h>

#include <winsock2.h>
#include <tspi.h>
#include <tapi.h>

#include "PhoneCall.h"
#include "utilities.h"
#include "sofia-bridge.h"
#include "tapi_lines.h"


static int ouridcount = 0;

////////////////////////////////////////////////////////////////////////////////
// Function PhoneCall
//
// Default constructor
//
////////////////////////////////////////////////////////////////////////////////
PhoneCall::PhoneCall(void)
{
	this->ringCount = 0;
	this->callCreation = time(0);
	this->Origin = LINECALLORIGIN_UNAVAIL;
	this->refered = false;
	
	this->newcallSignalled = false;
	this->dialtoneSignalled = false;
	this->connectedSignalled = false;

	this->dialingSignaled = false;
	this->proceedingSignaled = false;

	this->offeringSignaled = false;
	this->ringingSignaled = false;
	this->idleSignaled = false;

	// never can be zero - error
	if ( 0 == ouridcount ) ouridcount++;
	this->uid = ouridcount++;
}


////////////////////////////////////////////////////////////////////////////////
// Function ~PhoneCall
//
// Destructor
//
////////////////////////////////////////////////////////////////////////////////
PhoneCall::~PhoneCall(void)
{
}

////////////////////////////////////////////////////////////////////////////////
// Function PhoneCall
//
// Extra constructor
//
////////////////////////////////////////////////////////////////////////////////
void PhoneCall::init(std::string CallID, std::string identity)
{
	this->CallID = CallID;
	this->identity = identity;
	this->ringCount = 0;
	this->callCreation = time(0);
	this->refered = false;

}

void PhoneCall::init(std::string CallID, std::string identity, HTAPICALL call)
{
	this->CallID = CallID;
	this->identity = identity;
	this->ringCount = 0;
	this->callCreation = time(0);

	tapi_lines_list llist = get_line_list();
	tapi_lines_list::iterator it;

	for ( it = llist.begin(); it != llist.end(); it++ )
	{
		if ( this->identity == (*it)->get_subscribed_uri() )
		{
			HTAPILINE line = (*it)->get_tapi_line();
			LINEEVENT ev = (*it)->get_line_event();

			this->tapi.push_back(tapi_info(line, call, ev));
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// Function lineEvent
//
// gateway between event and however multiple lines are monitoring us.
//
////////////////////////////////////////////////////////////////////////////////
void PhoneCall::lineEvent(DWORD p1, DWORD_PTR p2, DWORD_PTR p3, DWORD_PTR p4)
{
	tapi_info_list::iterator it;

	bv_logger("PhoneCall::lineEvent: Sending line event to %i line events\n", this->tapi.size());
	for ( it = this->tapi.begin(); it != this->tapi.end(); it++ )
	{
		LINEEVENT ev = (*it).get_line_event();
		if ( ev )
		{
			bv_logger("PhoneCall::lineEvent: Sending line event to 0x%x for call 0x%x\n", (*it).get_line(), (*it).get_call());
			ev((*it).get_line(), (*it).get_call(), p1, p2, p3, p4);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// Function getCallAge
//
// Returns the age of the call
//
////////////////////////////////////////////////////////////////////////////////
time_t PhoneCall::getCallAge(void)
{
	return time(0) - this->callCreation;
}


////////////////////////////////////////////////////////////////////////////////
// Function signalTapiNewCall
//
// This is sent to TAPI whenever a new call is received which TAPI doesn't know 
// about. We in return receive an identifier for the call.
//
////////////////////////////////////////////////////////////////////////////////
void PhoneCall::signalTapiNewCall(void)
{
	if ( true == this->newcallSignalled ) return;
	this->newcallSignalled = true;

	bv_logger("LINE_NEWCALL\n");
	
	// Get all of teh line ids we need to use
	tapi_lines_list llist = get_line_list();
	tapi_lines_list::iterator it;

	for ( it = llist.begin(); it != llist.end(); it++ )
	{
		if ( this->identity == (*it)->get_subscribed_uri() )
		{
			HTAPILINE line = (*it)->get_tapi_line();
			LINEEVENT ev = (*it)->get_line_event();
			HTAPICALL call;

			// Send the event
			if ( ev )
			{
				ev( line,
					0,
					LINE_NEWCALL,
					(DWORD_PTR)this->uid,
					(DWORD_PTR)&call,
					0);

				this->tapi.push_back(tapi_info(line, call, ev));
			}
			//The tapi call returned could be a value NULL, this indicates TAPI cannot
			//handle messages coming in so we should decide what to do with it.

			//When this function returns tapiCall will be filled with the correct value
		}
	}

	

	this->CurrentTapiState = LINECALLSTATE_UNKNOWN;
	GetSystemTime(&CurrentStateTime);
}

////////////////////////////////////////////////////////////////////////////////
// Function signalTapiDialing
//
// Sends a signal that the device is dialing
//
////////////////////////////////////////////////////////////////////////////////
void PhoneCall::signalTapiDialtone(void)
{
	if ( true == this->dialtoneSignalled ) return;
	this->dialtoneSignalled = true;

	bv_logger("LINECALLSTATE_DIALTONE\n");
	this->lineEvent(
		LINE_CALLSTATE,
		LINECALLSTATE_DIALTONE,
		0,
		0);

	this->CurrentTapiState = LINECALLSTATE_DIALTONE;
	GetSystemTime(&CurrentStateTime);
}

////////////////////////////////////////////////////////////////////////////////
// Function signalTapiDialing
//
// Sends a signal that the device is dialing
//
////////////////////////////////////////////////////////////////////////////////
void PhoneCall::signalTapiDialing(void)
{
	if ( true == this->dialingSignaled ) return;
	this->dialingSignaled = true;

	bv_logger("LINECALLSTATE_DIALING\n");
	this->lineEvent(
		LINE_CALLSTATE,
		LINECALLSTATE_DIALING,
		0,
		0);

	this->CurrentTapiState = LINECALLSTATE_DIALING;
	GetSystemTime(&CurrentStateTime);
}

////////////////////////////////////////////////////////////////////////////////
// Function signalTapiDialing
//
// Sends a signal that the device is dialing
//
////////////////////////////////////////////////////////////////////////////////
void PhoneCall::signalTapiProceeding(void)
{
	if ( true == this->proceedingSignaled ) return;
	this->proceedingSignaled = true;

	bv_logger("LINECALLSTATE_PROCEEDING\n");
	this->lineEvent(
		LINE_CALLSTATE,
		LINECALLSTATE_PROCEEDING,
		0,
		0);


	this->CurrentTapiState = LINECALLSTATE_PROCEEDING;
	GetSystemTime(&CurrentStateTime);
}

////////////////////////////////////////////////////////////////////////////////
// Function signalTapiOffering
//
// Indicates to TAPI that this call is ringing, this shouldn't be called unless
// we have a valid HtspiCall.
//
////////////////////////////////////////////////////////////////////////////////
void PhoneCall::signalTapiOffering(void)
{

	if ( true == this->offeringSignaled ) return;
	this->offeringSignaled = true;

	bv_logger("LINECALLSTATE_OFFERING\n");
	//If the line is ringing then it isn't a call the line is ringing
	//not the call
	this->lineEvent(
		LINE_CALLSTATE,
		LINECALLSTATE_OFFERING,
		0,
		0);


	this->CurrentTapiState = LINECALLSTATE_OFFERING;
	GetSystemTime(&CurrentStateTime);
}


////////////////////////////////////////////////////////////////////////////////
// Function signalTapiRinging
//
// Indicates to TAPI that this call is ringing, this shouldn't be called unless
// we have a valid HtspiCall.
//
////////////////////////////////////////////////////////////////////////////////
void PhoneCall::signalTapiRinging(void)
{

	if ( true == this->ringingSignaled ) return;
	this->ringingSignaled = true;

	bv_logger("LINEDEVSTATE_RINGING\n");

	this->lineEvent(
		LINE_CALLSTATE,
		LINECALLSTATE_ACCEPTED,
		0,
		0);
	//this->ringCount);


	this->CurrentTapiState = LINECALLSTATE_OFFERING;
	GetSystemTime(&CurrentStateTime);
	//this->ringCount++;
}


////////////////////////////////////////////////////////////////////////////////
// Function signalTapiConnected
//
// Sends signal to Tapi that a call has been connected.
//
////////////////////////////////////////////////////////////////////////////////
void PhoneCall::signalTapiConnected(void)
{
	if ( true == this->connectedSignalled ) return;
	this->connectedSignalled = true;

	bv_logger("LINECALLSTATE_CONNECTED\n");
	this->lineEvent(
		LINE_CALLSTATE,
		LINECALLSTATE_CONNECTED,
		0,
		0);

	this->CurrentTapiState = LINECALLSTATE_CONNECTED;
	GetSystemTime(&CurrentStateTime);
}

////////////////////////////////////////////////////////////////////////////////
// Function signalTapiIdle
//
// Sends signal to Tapi that a call has been dropped.
//
////////////////////////////////////////////////////////////////////////////////
void PhoneCall::signalTapiIdle(void)
{
	if ( true == this->idleSignaled ) return;
	this->idleSignaled = true;

	bv_logger("LINECALLSTATE_IDLE\n");
	this->lineEvent(
		LINE_CALLSTATE,
		LINECALLSTATE_IDLE,
		0,
		0);


	this->CurrentTapiState = LINECALLSTATE_IDLE;
	GetSystemTime(&CurrentStateTime);
}



////////////////////////////////////////////////////////////////////////////////
// Function signalTapiCallerID
//
// This sets the member variables to the correct variables, and signals TAPI
// so that it will then call back down to us to get the relevant information.
//
////////////////////////////////////////////////////////////////////////////////
void PhoneCall::signalTapiCallerID(void)
{

	bv_logger("LINECALLSTATE_CALLERID\n");
	//TO test - I am not sure how permanent the info for this
	//needs to be made - and whether it is in the correct place.
	this->lineEvent(
		LINE_CALLINFO,
		LINECALLINFOSTATE_CALLERID,
		0,
		0);

}

////////////////////////////////////////////////////////////////////////////////
// Function signalTapiCalledID
//
// This sets the member variables to the correct variables, and signals TAPI
// so that it will then call back down to us to get the relevant information.
//
////////////////////////////////////////////////////////////////////////////////
void PhoneCall::signalTapiCalledID(void)
{
	this->calledID = calledID;

	bv_logger("LINECALLSTATE_CALLERID\n");
	//TO test - I am not sure how permanent the info for this
	//needs to be made - and whether it is in the correct place.
	this->lineEvent(
		LINE_CALLINFO,
		LINECALLINFOSTATE_CALLEDID,
		0,
		0);

}

////////////////////////////////////////////////////////////////////////////////
// Function getOrigin
//
// Should return an appropriate LINECALLORIGIN_ Constants
// TODO
//
////////////////////////////////////////////////////////////////////////////////
DWORD PhoneCall::getOrigin(void)
{
	return 0;
}


////////////////////////////////////////////////////////////////////////////////
// Function markAsRefered
//
// Mark the call as being refered. When we get an inidication of another
// call this one will be replaced accordingly.
//
////////////////////////////////////////////////////////////////////////////////
void PhoneCall::markAsRefered(std::string identity, std::string calledID, HTAPICALL hcall)
{
	if ( "" != calledID )
	{
		this->calledID = calledID;
	}

	this->identity = identity;

	this->refered = true;

	// set up our lines...
	tapi_lines_list llist = get_line_list();
	tapi_lines_list::iterator it;

	for ( it = llist.begin(); it != llist.end(); it++ )
	{
		bv_logger("PhoneCall::markAsRefered: %s == %s\n", this->identity.c_str(), (*it)->get_subscribed_uri().c_str());
		if ( this->identity == (*it)->get_subscribed_uri() )
		{
			HTAPILINE line = (*it)->get_tapi_line();
			LINEEVENT ev = (*it)->get_line_event();

			this->tapi.push_back(tapi_info(line, hcall, ev));
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// Function isRefered
//
// Has this call been marked as refered?
//
////////////////////////////////////////////////////////////////////////////////
bool PhoneCall::isRefered(void)
{
	return this->refered;
}

////////////////////////////////////////////////////////////////////////////////
// Function UpdateState
//
// Update the state of the call and signal our TAPI lines accordingly.
//
////////////////////////////////////////////////////////////////////////////////
void PhoneCall::UpdateState(std::string identity, 
						   std::string dialog_id, 
						   std::string direction, 
						   std::string state, 
						   std::string remote_display, 
						   std::string remote_identity)
{

	if ( "" == this->identity )
	{
		this->CallID = dialog_id;
		this->identity = identity;
		this->ringCount = 0;
		this->callCreation = time(0);
	}

	if ( true == this->refered && "early" == state )
	{
		this->callerID = getuserfromsipuri(remote_identity);

		bv_logger("PhoneCall::UpdateState: found refered call\n");

		this->Origin = LINECALLORIGIN_OUTBOUND;
		this->signalTapiNewCall();
		this->signalTapiOffering();
		this->signalTapiRinging();
		return;
	}

	if ( "early" == state && "recipient" == direction )
	{
		this->callerID = getuserfromsipuri(remote_identity);
		this->calledID = getuserfromsipuri(identity);

		this->Origin = LINECALLORIGIN_INBOUND;
		this->signalTapiNewCall();
		this->signalTapiOffering();
		this->signalTapiRinging();
		return;
	}
	
	if ( "early" == state && "recipient" != direction )
	{
		this->callerID = getuserfromsipuri(identity);
		this->calledID = getuserfromsipuri(remote_identity);

		this->Origin = LINECALLORIGIN_OUTBOUND;
		this->signalTapiNewCall();
		this->signalTapiDialing();
		this->signalTapiProceeding();
		
		return;
	}

	if ( "confirmed" == state )
	{
		this->signalTapiCallerID();
		this->signalTapiConnected();

		return;
	}

	if ( "terminated" == state )
	{
		this->signalTapiIdle();

		return;
	}


}


