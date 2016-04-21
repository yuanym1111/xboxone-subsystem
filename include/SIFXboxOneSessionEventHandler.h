/*--------------------------------------------------------------
|        Copyright (C) 1997-2010 by Prodigy Design Ltd         |
|		              Sidhe Interactive (TM)		           |
|                      All rights Reserved                     |
|--------------------------------------------------------------|
|                                                              |
|  Unauthorised use or distribution of this file or any        |
|  portion of it may result in severe civil and criminal       |
|  Penalties and will be prosecuted to the full extent of the  |
|  law.                                                        |
|                                                              |
---------------------------------------------------------------*/

#ifndef SIF_XboxOne_SESSION_EVENT_HANDLER_H
#define SIF_XboxOne_SESSION_EVENT_HANDLER_H

#include "SIFNetworkConstants.h"
#include "SIFOnlineConstants.h"
//#include <XboxOneMabOnlineConstants.h>
#include <MabNetTypes.h>
#include "XboxOneMabMatchmaking.h"
#include "XboxOneMabUISessionController.h"
#include "MabPeerGroupManager.h"
#include "XboxOneMabVoiceManager.h"


class XboxOneMabMatchmaking;
class XboxOneMabSessionSearchManager;
class XboxOneMabSystemNotificationHub;
class XboxOneMabSessionManagerMessage;
class XboxOneMabEventProvider;
class XboxOneMabSessionSearchManagerMessage;
class XboxOneMabSystemNotificationMessage;
//class XboxOneMabHostMigrationHelper;
//class XboxOneMabHostMigrationHelperMessage;
class MabNetworkManager;
class MabNetOnlineClientDetails;
class MabPeerGroupManager;
class SIFNetworkNonGameMessageHandler;
class XboxOneMabMatchMakingMessage;

// HAX: Temporary class to deal with this. Need to figure out what I actually
// want the QoS stuff to hold.
struct SIFXboxOneQoSData
{
	MabString xbox_user_id;
	//char gamertag[XUSER_NAME_SIZE];
	DWORD game_type;
	DWORD game_mode;
};

/**
 * \class SIFXboxOneSessionEventHandler
 * This handler deals with session/session search related events for multiplayer.
 * @author Jayden Sun
 */
class SIFXboxOneSessionEventHandler :	
	public MabObserver< XboxOneMabUISessionControllerMessage >,
	public MabObserver< XboxOneMabMatchMakingMessage >
{
public:
	SIFXboxOneSessionEventHandler( 
		MABMEM_HEAP _heap,
		XboxOneMabUISessionController* session_controller,
		XboxOneMabMatchMaking* matchmaking_controller,
		MabNetworkManager* my_mgr,
		MabPeerGroupManager* peer_group_mgr,
		SIFNetworkNonGameMessageHandler* non_game_message_handler);

	virtual ~SIFXboxOneSessionEventHandler();

	virtual void Update( MabObservable< XboxOneMabUISessionControllerMessage >* source, const XboxOneMabUISessionControllerMessage& msg );
	virtual void Update(MabObservable<XboxOneMabMatchMakingMessage>* source, const XboxOneMabMatchMakingMessage &msg);

	SIFXboxOneQoSData GetQoSInfoForResult( int idx ) const;

	// Query whether this should really live here. For now it does though, as there's no other logical place.
	inline void SetMatchmakingReadiness( SIF_MATCHMAKING_READINESS readiness ) { this->readiness = readiness; }
	// Sets if the session event handler should launch disconnection popups when it detects a detection.
	inline void SetAllowDisconnectionPopup( bool is_allowed )	{ allow_disconnection_popups = is_allowed; }

	virtual void OnSuspending();
	virtual void OnResumed(Platform::Object^ sender = nullptr, Platform::Object^ args = nullptr);
private:
	// Delegates for various events we care about.
	void OnNetworkSessionEvent(MabNetworkManagerEvent, MabNodeId, const MabNetOnlineClientDetails*);
	void OnNonGameMessageReceived(MabNodeId, SIFNetworkMessageTypes);

	void OnVoiceEvent(XboxOneMabVoiceEvent event_type, unsigned int controller_id, MabString& relevant_xuid);
	void OnInGameChanged(bool);
	void OnPeerBecameHost(MabNodeId);

	void OnPeerLeft(MabNodeId node_id = 0);

	// Helper method to start QoS listening on the host.
	void StartQOSListener();

	static bool IsFullCallback(void* thisptr);

	XboxOneMabUISessionController* session_controller;
	XboxOneMabSystemNotificationHub* system_notification_hub;
	MabNetworkManager* network_manager;
	MabPeerGroupManager* peer_group_mgr;
	SIFNetworkNonGameMessageHandler* non_game_message_handler;
	bool allow_disconnection_popups;
	XboxOneMabMatchMaking* matchmaking_controller;
	bool m_isOnline;
	SIF_MATCHMAKING_READINESS readiness;
};


#endif
