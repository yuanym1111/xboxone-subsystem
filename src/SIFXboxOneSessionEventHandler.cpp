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

#include <Precompiled.h>

// Me
#include "SIFXboxOneSessionEventHandler.h"

// XboxOne Mab
//#include <XboxOneMabSessionManager.h>
#include <XboxOneMabSystemNotificationHub.h>
//#include <XboxOneMabUserInfo.h>
//#include <XboxOneMabHostMigrationHelper.h>
#include "XboxOneMabMatchmaking.h"
#include "XboxOneMabVoiceManager.h"

// Mab
#include <MabLuaTypeDatabase.h>
#include <MabNamedValueList.h>
#include <MabNetworkManager.h>
#include <MabPeerGroupManager.h>
#include <MabNetOnlineClientDetails.h>
#include <MabUINode.h>
#include <xboxone/XboxOneMabNetTypes.h>

// SIF

#include "SIFRumbler.h"
#include "SIFGameLuaFunctions.h"
#include "SIFUIConstants.h"
#include "SIFWindowSystem.h"
#include "SIFPlayerProfileManager.h"
#include "SIFXboxOnePlayerProfileManager.h"
#include "SIFNetworkNonGameMessageHandler.h"
#include "SIFUnlockableManager.h"
//#include "SIFXboxOneLivePartyDelegate.h"
#include "SIFMatchmakingLuaFunctions.h"
#include "SIFGameLuaFunctions.h"

//class to include to avoid crash when change account inside game
#include "RUCareerModeManager.h"
#include "RUCompetitionCustomisationHelper.h"
#include "SIFGameWorld.h"
#include "RUGameEvents.h"
#include "SIFFlowConstants.h"
#include "SIFMenuLua.h"

// SURYA NOTE: This is temporary, change it to 4 or 8 when full multiplayer support has been implemented
const int MAX_PUBLIC_SLOT = NET_NUM_PLAYERS;
const int MAX_PRIVATE_SLOT = NET_NUM_PLAYERS;

SIFXboxOneSessionEventHandler::SIFXboxOneSessionEventHandler(
	MABMEM_HEAP _heap,
	XboxOneMabUISessionController* _session_controller,
	XboxOneMabMatchMaking* _matchmaking_controller,
	//XboxOneMabSystemNotificationHub* _system_notification_hub,
	//XboxOneMabHostMigrationHelper* host_migration_helper,
	MabNetworkManager* my_mgr,
	MabPeerGroupManager* peer_group_mgr,
	SIFNetworkNonGameMessageHandler* _non_game_message_handler) 
:	
	session_controller(_session_controller),
	matchmaking_controller(_matchmaking_controller),
	//host_migration_helper(host_migration_helper),
	network_manager(my_mgr),
	peer_group_mgr(peer_group_mgr),
	non_game_message_handler(_non_game_message_handler),
	readiness( SIFMMR_NOT_READY ),
	allow_disconnection_popups( false ),
	m_isOnline (true)
{
	session_controller->Attach(this);
	matchmaking_controller->Attach(this);
	//session_search_mgr->Attach(this);
	//system_notification_hub->Attach(this);
	//host_migration_helper->Attach(this);

	//session_controller->Attach(this);

	network_manager->OnNetworkSessionEvent.Add(this, &SIFXboxOneSessionEventHandler::OnNetworkSessionEvent);
	peer_group_mgr->PeerBecameHost.Add(this, &SIFXboxOneSessionEventHandler::OnPeerBecameHost);
	
	// Get the voice manager, attach
	SIFApplication::GetApplication()->GetVoiceManager()->OnVoiceEvent.Add(this, &SIFXboxOneSessionEventHandler::OnVoiceEvent);
	// Attach to the non game message handler (for game launch messages)
	non_game_message_handler->OnMessageReceived.Add(this, &SIFXboxOneSessionEventHandler::OnNonGameMessageReceived);

	// Give the network manager a callback to get it through this thing.
	network_manager->SetOnlineGameFullCallback( &SIFXboxOneSessionEventHandler::IsFullCallback, this );

	// Nowhere else to put this right now.
	//SIFApplication::GetApplication()->GetUnlockableManager()->Register( 
		//MabMemNew(MMHEAP_PERMANENT_DATA)SIFXboxOneLivePartyDelegate(*_session_controller));

}

SIFXboxOneSessionEventHandler::~SIFXboxOneSessionEventHandler()
{
	network_manager->OnNetworkSessionEvent.Remove(this, &SIFXboxOneSessionEventHandler::OnNetworkSessionEvent);
	peer_group_mgr->PeerBecameHost.Remove(this, &SIFXboxOneSessionEventHandler::OnPeerBecameHost);
	SIFApplication::GetApplication()->GetVoiceManager()->OnVoiceEvent.Remove(this, &SIFXboxOneSessionEventHandler::OnVoiceEvent);
	non_game_message_handler->OnMessageReceived.Remove(this, &SIFXboxOneSessionEventHandler::OnNonGameMessageReceived);
	network_manager->SetOnlineGameFullCallback( NULL );

	//host_migration_helper->Detach(this);
	session_controller->Detach(this);
	matchmaking_controller->Detach(this);
	//session_search_mgr->Detach(this);
	//system_notification_hub->Detach(this);
}

void SIFXboxOneSessionEventHandler::Update( MabObservable< XboxOneMabUISessionControllerMessage >* source, const XboxOneMabUISessionControllerMessage& msg )
{
	if(msg.GetType() == XboxOneMabUISessionController::SESSION_EVENT_USER_LEFT)
	{
		bool isStarted = msg.GetResult();
		if(!isStarted)
		{
			SIFApplication::GetApplication()->GetWindowSystem()->LaunchPopUpByName("WWFailedSignin");
			return;
		}
		bool isIngame = SIFGameLuaFunctions::GAIsInGame();

		if(isIngame){
			MabNamedValueList event_parameters;
			event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, "xboxoneprofilechanged");
			SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
			SIFGameLuaFunctions::GAXboxOneLeaveAll();
			return;
		}
		MabNamedValueList event_parameters;
		event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, "on_user_lost");
		SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
		SIFApplication::GetApplication()->GetWindowSystem()->LaunchPopUpByName("WWLoseXboxOneUser");

	}

	if(msg.GetType() == XboxOneMabUISessionController::SESSION_EVENT_USER_ADDED)
	{
		//Met disconnected event from another peer
		MabNamedValueList event_parameters;
		event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, "xboxoneuseradded");
		SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
	}

	if(msg.GetType() == XboxOneMabUISessionController::SESSION_EVENT_JOIN_RESTRICTION)
	{
		SIFGameLuaFunctions::GAXboxOneLeaveAll();
		SIFApplication::GetApplication()->GetWindowSystem()->LaunchPopUpByName("MultiplayerPrivilegeNeeded");
	}

	// If the message is not to do with a multiplayer session, we don't care about it. 
	if (!msg.GetSessionMultiplayer()) return;

	XboxOneMabUISessionController::SESSION_EVENT msg_type = msg.GetType();

	if (msg.GetResult() && msg.GetType() == XboxOneMabUISessionController::SESSION_EVENT_CREATE)
	{	
		// Let the application know that the session has been created.
		// Notify the UI that we've been connected to the room.
			MabNamedValueList event_parameters;
			event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_SESSION_EVENT_CREATED);
			SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
	}
	else if (!msg.GetResult() && msg.GetType() == XboxOneMabUISessionController::SESSION_EVENT_CREATE)
	{
		MabNamedValueList event_parameters;
		event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_SESSION_EVENT_CREATE_FAILED);
		SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
	}

	if(msg.GetType() == XboxOneMabUISessionController::SESSION_EVENT_PEER_CONNECT) {
		MABLOGDEBUG("RECEIVED A SESSION_EVENT_PEER_CONNECT");

		// Let the application know that the session has been created.
		// Notify the UI that we've been connected to the room.
		// For quick match, the session is created at the same time peer connected with each other
		if(SIFMatchmakingLuaFunctions::IsQuickmatch()){
			MabNamedValueList event_parameters;
			event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_SESSION_EVENT_CREATED);
			SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
		}


		if (session_controller->IsHost())
		{
			// If we're the host, just go into the PRE_GAME state and start hosting
			session_controller->UpdateLocalClientInfo();
			const XboxOneMabNetOnlineClient& server =  session_controller->GetLocalClient();
#ifdef _DEBUG
			MABLOGDEBUG("I.m server. Server user xbox user id: %s",server.member_id.c_str());
			const XboxOneMabNetOnlineClient *client = reinterpret_cast<const XboxOneMabNetOnlineClient*>(msg.GetPayload());
			MABLOGDEBUG("Wait for client xbox user id: %s",client->member_id.c_str());
#endif
			network_manager->HostGame(NET_GAME_PORT, MabNetOnlineClientDetails(server));
		}
		else
		{
			MabNetAddress* address;
			const XboxOneMabNetOnlineClient *data = reinterpret_cast<const XboxOneMabNetOnlineClient*>(msg.GetPayload());
			//XboxOneMabNetOnlineClient
			//Todo:  Make sure the server is initilized before the client want to connected to

			address = new MabNetAddress();
			address->SetV6Address(data->local_address);
			session_controller->UpdateLocalClientInfo();
			const XboxOneMabNetOnlineClient& m_client  =  session_controller->GetLocalClient();
#ifdef _DEBUG
			MABLOGDEBUG("I'm client with user id: %s. Will connect to Server user xbox user id: %s",m_client.member_id.c_str(), data->member_id.c_str());

#endif
			network_manager->JoinGame(*address, NET_GAME_PORT, MabNetOnlineClientDetails(m_client));
//			SIFApplication::GetApplication()->GetVoiceManager()->RegisterClientXUID(data->member_id,0);
		}

	}

	if (msg.GetResult() && (msg.GetType() == XboxOneMabUISessionController::SESSION_EVENT_POST_JOIN
		|| msg.GetType() == XboxOneMabUISessionController::SESSION_EVENT_POST_LEAVE))
	{
		MabNamedValueList event_parameters;
		event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_USERLIST_CHANGED);
		SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
	}

	if (!msg.GetResult() && msg.GetType() == XboxOneMabUISessionController::SESSION_EVENT_POST_JOIN)
	{
		MabNamedValueList event_parameters;
		event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_SESSION_EVENT_JOIN_FAILED);
		SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
	}

	if (msg.GetType() == XboxOneMabUISessionController::SESSION_EVENT_ACCEPT_INVITE)
	{
		// Figure out who accepted the invite.
		//int controller_idx = session_controller->GetInviteController();
		//MABASSERT(controller_idx < XUSER_MAX_COUNT);
		int controller_idx = 0;

		// Throw a popup on screen if appropriate.
		//Todo: Check whether the session is ready for join

		//check whether we are inside a game
		if(SIFGameLuaFunctions::GAIsInGame()  )
		{
			//Do nothing but lauch the pop up window
			SIFApplication::GetApplication()->GetWindowSystem()->LaunchPopUpByName(SIF_MATCHMAKING_INVITE_RECEIVED_POPUP_NAME);	
		}
		else if (readiness == SIFMMR_USER_READY)
		{	
			//TODO: currently just put some delay until player data is loaded
			SIFGameLuaFunctions::GAXboxOneLeaveAll();

			if(SIFApplication::GetApplication()->GetMatchMakingManager()->isMatchStarted())
			{

				SIFApplication::GetApplication()->GetMatchMakingManager()->ResetMultiplayer();
				network_manager->LeaveGame(true);
			}

			Sleep(3000);
			SIFApplication::GetApplication()->GetWindowSystem()->LaunchPopUpByName(SIF_MATCHMAKING_INVITE_RECEIVED_POPUP_NAME);			
		}

		// Notify the UI.
		MabNamedValueList event_parameters;
		event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_INVITE_ACCEPTED);
		event_parameters.SetValue<int>(SIFUI_LUA_CONTROLLER_ID_PARAM, controller_idx);
		SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
	}
	if(msg.GetType() == XboxOneMabUISessionController::SESSION_EVENT_ACCEPT_GMAEMODE){

		int controller_idx = 0;
		// Notify the UI.
		MabNamedValueList event_parameters;

		const int* gamemode = reinterpret_cast<const int*>(msg.GetPayload());
		SIFApplication::GetApplication()->GetGameSettings()->game_settings.SetGameMode((GAME_MODE)*gamemode);

		event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_GAMEMODE_ACCEPTED);
		SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
	}

	if (msg.GetType() == XboxOneMabUISessionController::SESSION_EVENT_DELETE)
	{
		MabNamedValueList event_parameters;
		event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_SESSION_FINISHED);
		SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters); //?
		//SIFApplication::GetApplication()->GetVoiceManager()->EndSession();
	}

	if (msg.GetType() == XboxOneMabUISessionController::SESSION_EVENT_TERMINATED)
	{
		if (SIFRumbler::GetInstance())
		{
			SIFRumbler::GetInstance()->ClearRumbles();
			SIFRumbler::GetInstance()->PauseRumbles(true);
		}

		// Fire a popup onto screen - it's lower priority than a signout popup, and should hopefully be 
		// automagically dismissed on hitting main menu, if a signout triggered this scenario.
		/*if (allow_disconnection_popups)
		{
			MabUINode* active_popup = SIFApplication::GetApplication()->GetWindowSystem()->GetCurrentActivePopUp();
			if(!active_popup)
				SIFApplication::GetApplication()->GetWindowSystem()->LaunchPopUpByName( SIF_MATCHMAKING_GAME_DISCONNECT_POPUP_NAME );
		}*/

		// Just tell the network manager to "Leave Game" - that's the same as an unpredicted disconnect as far as we're concerned.
		network_manager->LeaveGame(true);
		//SIFApplication::GetApplication()->GetVoiceManager()->EndSession();
	}


	//trigger session search completed event
//#ifdef _DEBUG
	if (msg.GetType() == XboxOneMabUISessionController::SESSION_SEARCH_SEARCH_COMPLETED)
	{
		MabNamedValueList event_parameters;
		event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_SEARCHING_COMPLETED);
		event_parameters.SetValue<int>(SIF_MATCHMAKING_SEARCHING_NUM_RESULTS, 0);
		SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
	}

	if(msg.GetType() == XboxOneMabUISessionController::SESSION_EVENT_GAME_FULL)
	{
		SIFApplication::GetApplication()->GetWindowSystem()->LaunchPopUpByName("JoinGameFullOccurred");
	}
	//If the invitation url is null or can't get the match session url
	if(msg.GetType() == XboxOneMabUISessionController::SESSION_EVENT_GAME_NOMATCHINFO || msg.GetType() ==  XboxOneMabMatchMaking::XBOXONE_NETWORK_MATCH_ERROR)
	{
		SIFApplication::GetApplication()->GetWindowSystem()->LaunchPopUpByName("JoinErrorOccurred");
	}
	if(msg.GetType() == XboxOneMabUISessionController::SESSION_EVENT_OPPONENT_LEFT)
	{
		//Met disconnected event from another peer
		MabNamedValueList event_parameters;
		event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_HOSTMIGRATION_EVENT_OCCURRED);
//		SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
//		OnPeerLeft();
	}
//#endif


}

void SIFXboxOneSessionEventHandler::Update(MabObservable<XboxOneMabMatchMakingMessage>*, const XboxOneMabMatchMakingMessage &msg)
{
	XboxOneMabMatchMaking::XBOXONE_NETWORK_EVENT type = msg.GetType();

	switch(type) {
	case XboxOneMabMatchMaking::XBOXONE_NETWORK_DISCONNECTED:
	case XboxOneMabMatchMaking::XBOXONE_NETWORK_DOWN:
		if(session_controller && network_manager->GetNumPeers() > 0 && (session_controller->IsSessionActive()
			&& !session_controller->IsSessionDying() || session_controller->IsMatchMakingStarted()))
		{			
			if( allow_disconnection_popups )
			{
				MabUINode* active_popup = SIFApplication::GetApplication()->GetWindowSystem()->GetCurrentActivePopUp();
				if(!active_popup || active_popup->GetName() != SIF_MATCHMAKING_LIVE_CONNECTION_LOST_POPUP_NAME )
				{
					if(active_popup)
						SIFApplication::GetApplication()->GetWindowSystem()->DismissPopUpByName(active_popup->GetName().c_str());
					SIFApplication::GetApplication()->GetWindowSystem()->LaunchPopUpByName(SIF_MATCHMAKING_LIVE_CONNECTION_LOST_POPUP_NAME);
				}
			}

//			MabNamedValueList event_parameters;
//			event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_DISCONNECTED);
//			SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
//			network_manager->LeaveGame(true);
//			network_manager->StopTransport();
//			SIFMatchmakingLuaFunctions::SafeReturnToMainMenu();
		}
		m_isOnline = false;
		break;
	case XboxOneMabMatchMaking::XBOXONE_NETWORK_CONNECTED:
	case XboxOneMabMatchMaking::XBOXONE_NETWORK_RESTORE:
		m_isOnline = true;
//		session_controller->OnNetworkStatusChanged(true);
		break;
	default:
		break;
	}
}

void SIFXboxOneSessionEventHandler::OnNetworkSessionEvent(MabNetworkManagerEvent event_type, MabNodeId node_id, const MabNetOnlineClientDetails* client)
{
	switch (event_type)
	{
	case MNMET_CLIENT_JOINED:
		session_controller->AddPlayersToSession(client->GetDetails());

#ifdef _DEBUG
		MABLOGDEBUG("Received peer xuid: %s and node id: %d",client->GetDetails().member_id.c_str(), node_id);
#endif
		//host_migration_helper->RegisterClient(client->GetDetails(), node_id);
		{
			MabNamedValueList event_parameters;
			event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_USERLIST_CHANGED);
//			SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
		}
		SIFApplication::GetApplication()->GetVoiceManager()->RegisterClientXUID(client->GetDetails().member_id, node_id);
		break;
	case MNMET_CLIENT_LEFT:
		// If the session is not active, some error condition has probably caused this, and we shouldn't try to fix anything.
		MABLOGDEBUG("Peer left for xuid: %s and node id: %d",client->GetDetails().member_id.c_str(), node_id);
		if (session_controller->IsSessionActive()
			&& !session_controller->IsSessionDying()
			&& client)
		{
			//Todo: Will be a session join again, wait for host migration event
			OnPeerLeft(node_id);

		}
		break;
	case MNMET_LEAVING_GAME:
		// Just leave the session. When that's completed we'll delete it.
		// If the session is no longer active or is dying, we don't need to do this.
		if (session_controller->IsSessionActive()
			&& !session_controller->IsSessionDying() || session_controller->IsMatchMakingStarted())
		{
		//	session_controller->RemovePlayersFromSession( session_controller->GetLocalClient());
			SIFApplication::GetApplication()->GetVoiceManager()->UnregisterClient(node_id);
			SIFApplication::GetApplication()->GetMatchMakingManager()->leave(m_isOnline);
		}
		break;
	case MNMET_JOIN_ATTEMPT_FAILED_FULL:
		if (session_controller->IsSessionActive()
			&& !session_controller->IsSessionDying())
		{
			MabNamedValueList event_parameters;
			event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_SESSION_EVENT_JOIN_FAILED_FULL);
			SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
		}
		break;
	case MNMET_JOIN_ATTEMPT_FAILED_OTHER:
		if (session_controller->IsSessionActive()
			&& !session_controller->IsSessionDying())
		{
			MabNamedValueList event_parameters;
			event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_SESSION_EVENT_JOIN_FAILED);
			SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
		}
		break;
	case MNMET_JOIN_ATTEMPT_FAILED_VERSION_MISMATCH:
		if (session_controller->IsSessionActive()
			&& !session_controller->IsSessionDying())
		{
			MabNamedValueList event_parameters;
			event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_SESSION_EVENT_VERSION_MISMATCH);
			SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
		}
		break;
	case MMET_CLIENT_TIMEOUT:
		if (session_controller->IsSessionActive()
			&& !session_controller->IsSessionDying())
		{
			MabNamedValueList event_parameters;
			event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_CONNECTION_TIMEOUT);
			SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
		}
		break;
	case MMET_CLIENT_RECONNECTED:
		if (session_controller->IsSessionActive()
			&& !session_controller->IsSessionDying())
		{
			MabNamedValueList event_parameters;
			event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_CONNECTION_RECONNNCTED);
			SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
		}
		break;
	default:
		break;
	}
}


SIFXboxOneQoSData SIFXboxOneSessionEventHandler::GetQoSInfoForResult( int idx ) const
{
	//const XNQOSINFO* qos_info = session_search_mgr->GetQoSInfo( idx );
	SIFXboxOneQoSData qos_data;
#if 0
	if(qos_info && qos_info->pbData) memcpy( &qos_data, qos_info->pbData, sizeof(qos_data) );
	else memset(&qos_data, 0, sizeof(qos_data));
#endif
	return qos_data;
}

void SIFXboxOneSessionEventHandler::OnNonGameMessageReceived( MabNodeId, SIFNetworkMessageTypes message_type )
{
	switch (message_type)
	{
	case GAME_LAUNCH:
		// Just in case our target window wants to do something with this, we notify it.
		{
			//session_controller->StartSession();		
			MabNamedValueList event_parameters;
			event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_GAME_START);
			SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
		}
		break;
	default:
		break;
	}
}

void SIFXboxOneSessionEventHandler::OnInGameChanged( bool in_game )
{
	if (in_game)
	{
		bool private_game = (session_controller->GetFilledPrivateSlots() != 0);
		unsigned int num_public_slots = session_controller->GetFilledPublicSlots() + NUM_HUMAN_PLAYERS_PER_TEAM;
		unsigned int num_private_slots = session_controller->GetFilledPrivateSlots() + NUM_HUMAN_PLAYERS_PER_TEAM;
		//session_controller->ModifySession(private_game ? NULL : &num_public_slots, private_game ? &num_private_slots : NULL);		
	}
	else
	{
		unsigned int num_public_slots = session_controller->GetFilledPublicSlots();
		unsigned int num_private_slots = session_controller->GetFilledPrivateSlots();
		//session_controller->ModifySession(&num_public_slots, &num_private_slots);	
	}
}

void SIFXboxOneSessionEventHandler::OnPeerBecameHost( MabNodeId node_id )
{
#if 0
	// Someone has become the host - let's start migrating.
	if (const MabNetOnlineClientDetails* client_details = network_manager->GetPeerInfo(node_id))
		host_migration_helper->BeginHostMigration( client_details->GetDetails() );		
	else
		MABBREAKMSG( "WE'RE NOT CONNECTED TO THE PEER SPECIFIED, HOW CAN WE MIGRATE TO THEM?!" );
#endif
}

bool SIFXboxOneSessionEventHandler::IsFullCallback( void* thisptr )
{
	SIFXboxOneSessionEventHandler* event_handler = (SIFXboxOneSessionEventHandler*)thisptr;
	return event_handler->session_controller->GetFilledSlots() == event_handler->session_controller->GetTotalSlots();
}

void SIFXboxOneSessionEventHandler::StartQOSListener()
{
	MabBuffer buffer;
	// start QoS
	SIFXboxOneQoSData qos_data;
	MabString name;
	size_t owner_controller = (size_t)session_controller->GetSessionOwner();
#if 0
	SIFApplication::GetApplication()->GetXboxOneUserInfo()->GetUserXUID(owner_controller, qos_data.xuid);
	SIFApplication::GetApplication()->GetXboxOneUserInfo()->GetUserGamertag(owner_controller, name);
	strcpy_s( qos_data.gamertag, XUSER_NAME_SIZE, name.c_str() );
	qos_data.game_type = session_controller->GetSessionGameType();
	qos_data.game_mode = session_controller->GetSessionGameMode();
	buffer.PushBack( (char*)&qos_data, sizeof(qos_data) );
	session_search_mgr->StartQoSListener( session_controller->GetSessionInfo(), buffer );
#endif
}

//XboxOne system call to suspend the xboxone mab system
void SIFXboxOneSessionEventHandler::OnSuspending()
{
	//Todo: do nothing until there is crash reported
	SIFApplication::GetApplication()->GetMatchMakingManager()->OnSuspending();
	if(SIFApplication::GetApplication()->GetMatchMakingManager()->isInLobby() || SIFApplication::GetApplication()->GetMatchMakingManager()->isMatchStarted()){
		SIFApplication::GetApplication()->GetNetworkManager()->LeaveGame(true);
	}
	if(SIFApplication::GetApplication()->GetMatchMakingManager()->isInLobby()){
		SIFMatchmakingLuaFunctions::SafeReturnToMainMenu();
		SIFMenuLua* menu_lua = SIFApplication::GetApplication()->GetWindowSystem()->GetLuaInterpreter();
		if(menu_lua){
			MabNamedValueList event_parameters;
			menu_lua->CallWithResult("MainMenu.DefaultMainMenuSelection", NULL, event_parameters);
		}
	}
	SetAllowDisconnectionPopup(false);
}

//XboxOne system call to resume the xboxone mab system
void SIFXboxOneSessionEventHandler::OnResumed(Platform::Object^ sender /*= nullptr*/, Platform::Object^ args /*= nullptr*/)
{
	SIFApplication::GetApplication()->GetMatchMakingManager()->OnResumed();
}

void SIFXboxOneSessionEventHandler::OnPeerLeft(MabNodeId node_id)
{
//	network_manager->DisconnectPeer( node_id );
	//Host migration will happended inside leaving game logic, need to notify the UI user list changed
	//Only need to stop the transport layer and wait for another request

	//Fixed the crash, Should handled by message get
	//SIFApplication::GetApplication()->GetVoiceManager()->UnregisterClient(node_id);
	//Todo: if this is a quick match and match is not finished, resubmit the ticket if possible
	//Stop the transport layer to prepare for another hostgame call when someone else join me
	//TODO: Should we do that here? 
	MabNamedValueList event_parameters;
	event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_USERLIST_CHANGED);
	SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);

	if(SIFApplication::GetApplication()->GetMatchMakingManager()->isInLobby())
	{
		if(SIFMatchmakingLuaFunctions::IsQuickmatch() && SIFApplication::GetApplication()->GetMatchMakingManager()->IsQuickMatch() &&  m_isOnline == true){
			//If this is a quick match, fully leave the current game and resubmit the quick match request

			//Todo: If I'm still in lobby screen, to this
			network_manager->StopTransport();
			SIFApplication::GetApplication()->GetMatchMakingManager()->XboxOneStateLauchQuickMatch(SIFApplication::GetApplication()->GetGameSettings()->game_settings.gameMode, false);
		}else{
			if(!session_controller->IsHost())
			{
				MabNamedValueList event_parameters;
				event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_HOSTMIGRATION_EVENT_OCCURRED);
				SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
			}
		}
		//For xboxone quick match there is no server and client, should be removed here


	}else{
		SIFApplication::GetApplication()->GetMatchMakingManager()->ResetMultiplayer();
	}
}

void SIFXboxOneSessionEventHandler::OnVoiceEvent(XboxOneMabVoiceEvent event_type, unsigned int controller_id, MabString& relevant_xuid)
{
	// Inform UI that something... anything has happened Voice chat side.
	MabNamedValueList event_parameters;
	event_parameters.SetValue<const char*>(SIFUI_LUA_SYSTEM_EVENT_PARAM, SIF_MATCHMAKING_VOICE_USER_EVENT_NAME);
	SIFApplication::GetApplication()->GetWindowSystem()->OnSystemEvent(event_parameters);
}

