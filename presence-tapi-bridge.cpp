
#include <string>
#include <algorithm>

#include <winsock2.h>
#include <tspi.h>

#include "presence-tapi-bridge.h"
#include "phonecall.h"
#include "utilities.h"
#include "tapi_lines.h"

#include "sofia-bridge.h"

extern Mutex *g_CallMut;

PhoneContainer Phones;

////////////////////////////////////////////////////////////////////////////////
// Function handle_presence_event
//
// Called by our sofia parser in resopnce to a notify event - if we 
// parsed ok - which means we got a dialog event.
//
// identity: who we are watching (i.e. sip:1001@omniis.babblevoice.com
// dialog_id: unique id representing this conversation, in fact on babblevoice
// this is the uuid of the so could be used to grab the call recording.
// direction: initiator or recipient
// state: Trying, Proceeding, Early, Confirmed, Terminated (RFC 4235 3.7.1 see the state machine)
//
//                +----------+            +----------+
//                |          | 1xx-notag  |          |
//                |          |----------->|          |
//                |  Trying  |            |Proceeding|-----+
//                |          |---+  +-----|          |     |
//                |          |   |  |     |          |     |
//                +----------+   |  |     +----------+     |
//                     |   |     |  |          |           |
//                     |   |     |  |          |           |
//                     +<--C-----C--+          |1xx-tag    |
//                     |   |     |             |           |
//            cancelled|   |     |             V           |
//             rejected|   |     |1xx-tag +----------+     |
//                     |   |     +------->|          |     |2xx
//                     |   |              |          |     |
//                     +<--C--------------|  Early   |-----C---+ 1xx-tag
//                     |   |   replaced   |          |     |   | w/new tag
//                     |   |              |          |<----C---+ (new FSM
//                     |   |              +----------+     |      instance
//                     |   |   2xx             |           |      created)
//                     |   +----------------+  |           |
//                     |                    |  |2xx        |
//                     |                    |  |           |
//                    V                    V  V           |
//                +----------+            +----------+     |
//                |          |            |          |     |
//                |          |            |          |     |
//                |Terminated|<-----------| Confirmed|<----+
//                |          |  error     |          |
//                |          |  timeout   |          |
//                +----------+  replaced  +----------+
//                              local-bye   |      ^
//                              remote-bye  |      |
//                                          |      |
//                                          +------+
//                                           2xx w. new tag
//                                            (new FSM instance
//                                             created)
//
//                               Figure 3
// remote_display: the remote party display string (i.e. the phone number)
// remote_identity: the uri of the remote party (i.e. if we called 3 it might 
// be sip:3@omniis.babblevoice.com)
//
////////////////////////////////////////////////////////////////////////////////
void handle_presence_event(std::string identity, 
						   std::string dialog_id, 
						   std::string direction, 
						   std::string state, 
						   std::string remote_display, 
						   std::string remote_identity)
{
	std::transform(state.begin(), state.end(), state.begin(), ::tolower);
	std::transform(direction.begin(), direction.end(), direction.begin(), ::tolower);
	std::transform(identity.begin(), identity.end(), identity.begin(), ::tolower);
	std::transform(direction.begin(), direction.end(), direction.begin(), ::tolower);
	std::transform(remote_display.begin(), remote_display.end(), remote_display.begin(), ::tolower);

	bv_logger("handle_presence_event for %s\n", identity.c_str());

	if ( "" == dialog_id ) return;

	Phones[identity].UpdateState(identity, 
								   dialog_id, 
								   direction, 
								   state, 
								   remote_display, 
								   remote_identity);

}



/////////////// new version of functions....

// This will create a new phone call registration which
// wil have a blank id waiting to be populated - also marked 
// as refered by us...
int registerIntiatedCall(std::string phoneUri, std::string to, HTAPICALL hcall)
{
	std::string normuri = getnormalizedsipuri(phoneUri);
	std::string normto = getnormalizedsipuri(to);
	bv_logger("registerIntiatedCall: phoneUri %s, to %s, hcall %i\n", normuri.c_str(), normto.c_str(), (int)hcall);
	return Phones[normuri].MarkAsRefered(normuri, normto, hcall);
}

PhoneCall find_phonecall_by_tapi_call_id(HDRVCALL hdCall)
{
	PhoneContainer::iterator it;
	PhoneCallContainer calls;
	PhoneCallContainer::iterator callit;

	for ( it = Phones.begin(); it != Phones.end(); it++ )
	{
		calls = it->second.GetPhoneCalls();
		for ( callit = calls.begin(); callit != calls.end(); callit ++)
		{
			bv_logger("find_phonecall_by_tapi_call_id: %i == %i\n", callit->second.getUID(), (int)hdCall);
			if ( callit->second.getUID() == (int)hdCall )
			{
				return callit->second;
			}
		}
	}

	bv_logger("find_phonecall_by_tapi_call_id: Call not found, returning\n");
	PhoneCall empty;
	return empty;
}


