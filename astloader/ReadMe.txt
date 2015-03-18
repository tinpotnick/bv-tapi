/////////////Notes for inbound Asterisk Manager interface/////////////////////////
Asterisk Call Manager/1.0
Action: Login
UserName: nick
Secret: *****

Response: Success
Message: Authentication accepted

Event: Newchannel
Channel: Zap/2-1
State: Ring
CallerID: 07766088671
Uniqueid: asterisk-5765-1112631218.1134

Event: Newexten
Channel: Zap/2-1
Context: isdn
Extension: 200151
Priority: 1
Application: Wait
AppData: 1
Uniqueid: asterisk-5765-1112631218.1134

Event: Newexten
Channel: Zap/2-1
Context: isdn
Extension: 200151
Priority: 2
Application: Dial
AppData: Sip/nick|60|trg
Uniqueid: asterisk-5765-1112631218.1134

Event: Newchannel
Channel: SIP/nick-f39b
State: Down
CallerID: <unknown>
Uniqueid: asterisk-5765-1112631220.1135

Event: Newstate
Channel: Zap/2-1
State: Ringing
CallerID: 07766088671
Uniqueid: asterisk-5765-1112631218.1134

Event: Newchannel
Channel: SIP/nick-f39b
State: Ringing
CallerID: 07766088671
Uniqueid: asterisk-5765-1112631220.1135

Event: Hangup
Channel: SIP/nick-f39b
Uniqueid: asterisk-5765-1112631220.1135
Cause: 16

Event: Newexten
Channel: Zap/2-1
Context: isdn
Extension: 200151
Priority: 3
Application: VoiceMail
AppData: u101
Uniqueid: asterisk-5765-1112631218.1134

Event: Newstate
Channel: Zap/2-1
State: Up
CallerID: 07766088671
Uniqueid: asterisk-5765-1112631218.1134

Event: Hangup
Channel: Zap/2-1
Uniqueid: asterisk-5765-1112631218.1134
Cause: 16
