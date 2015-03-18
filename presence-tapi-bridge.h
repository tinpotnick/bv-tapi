

#include <string>
#include <map>

#include "phone.h"

void handle_presence_event(std::string identity, 
						   std::string dialog_id, 
						   std::string direction, 
						   std::string state, 
						   std::string remote_display, 
						   std::string remote_identity);

typedef std::map<std::string, Phone> PhoneContainer;

int registerIntiatedCall(std::string phoneUri, std::string to, HTAPICALL hcall);
PhoneCall find_phonecall_by_tapi_call_id(HDRVCALL hdCall);

#if 0 
typedef std::list<PhoneCall *> PhoneCallList;

PhoneCall * find_phonecall_by_dialog_id(std::string id);
PhoneCall * find_phonecall_by_tapi_call_id(HDRVCALL id);
PhoneCall * find_phonecall_by_refered(std::string called_id);
PhoneCall * create_phone_call(std::string id, std::string identity);
PhoneCall * create_phone_call(void);
void destroy_phone_call(PhoneCall *call);

#endif