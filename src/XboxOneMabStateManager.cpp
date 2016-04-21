#include "pch.h"

#include "XboxOneMabStateManager.h"
#include "XboxOneMabUtils.h"
#include "XboxOneMabMatchMaking.h"
#include "XboxOneMabSessionManager.h"
#include "XboxOneMabUserInfo.h"
#include "XboxOneMabGame.h"
#include "XboxOneMabSessionManager.h"
#include "XboxOneMabVoiceManager.h"

#include "XboxOneMabUISessionController.h"
#include "XboxOneMabStatisticsManager.h"
#include "XboxOneMabAchievementManager.h"

using namespace Concurrency;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace Windows::Foundation;
using namespace Windows::Networking::Connectivity;
using namespace Windows::Xbox::System;
using namespace XboxOneMabLib;

XboxOneMabStateManager::XboxOneMabStateManager() :
	m_multiplayerActivationInfo(nullptr),m_systemEventsBound(false),IsGameSessionAvailable(false),IsGameSessionFull(false),IsGameHostMigrating(false),IsLobbyHostMigrating(false),PeerMapNeedsVerification(false)
{
	m_priorState = GameState::Reset;
	m_state = GameState::Reset;
	m_pendingState = GameState::Reset;
	m_gameType = NETWORK_NONE;
	m_ruMultiPlayerType = RU_FIFTEEN;
}

XboxOneMabStateManager::~XboxOneMabStateManager()
{
	if(!m_peerChangeQueue.empty()){
		m_peerChangeQueue.clear();
	}
	if(!m_peerMessageQueue.empty()){
		m_peerMessageQueue.clear();
	}
	XboxOneSessionManager->OnMultiplayerStateChanged.~function();
	XboxOneSessionManager->OnMatchmakingChanged.~function();
}

///////////////////////////////////////////////////////////////////////////////
//  Initialize
void XboxOneMabStateManager::Initialize()
{
	SwitchState(GameState::Initialize);
}

void XboxOneMabStateManager::Reset(){
	XboxOneGame->SetOnlineUser();
}

bool XboxOneMabStateManager::IsCurrentOrPendingState(XboxOneMabLib::GameState state)
{
	critical_section::scoped_lock lock(m_lock);
	return (m_state == state || m_pendingState == state);
}

///////////////////////////////////////////////////////////////////////////////
//  Switch to the next state, it will do the logic in the next update
void XboxOneMabStateManager::SwitchState(GameState newState)
{
	critical_section::scoped_lock lock(m_lock);

	if (newState == GameState::Reset)
	{
		m_priorState = m_state;
		m_state = GameState::Reset;
		m_pendingState = GameState::Initialize;
		return;
	}

	if (m_state == newState)
	{
		XboxOneMabUtils::LogComment(L"WARNING: Attempted to switch to current state.\n");
		return;
	}

	// Set the pending state so it will pick up on the next update
	m_pendingState = newState;
}

///////////////////////////////////////////////////////////////////////////////
//  Revert back to the old state
void XboxOneMabStateManager::RevertToPriorState()
{
	critical_section::scoped_lock lock(m_lock);
	XboxOneMabUtils::LogCommentFormat(L"RevertToPriorState %d -> %d\n", m_state, m_priorState);
	m_pendingState = m_priorState;
	m_priorState = m_state;
	m_state = m_pendingState;
}

///////////////////////////////////////////////////////////////////////////////
//  Reset session manager, leave the network
void XboxOneMabStateManager::ResetMultiplayer(bool leaveClean /* = false*/)
{
	create_task([this,leaveClean]()
	{
		XboxOneMabUtils::LogComment(L"Leave the game, reset multiplayer session\n");
		XboxOneSessionManager->LeaveMultiplayer(leaveClean);
		if(true)
			XboxOneVoice->RemoveUserFromChatChannel(0,XboxOneGame->GetCurrentUser());

		IsGameSessionFull = false;
		//	m_multiplayerActivationInfo = nullptr;
		m_peerChangeQueue.clear();
		m_peerMessageQueue.clear();
		m_gameType = NETWORK_NONE;
	}).wait();
}

///////////////////////////////////////////////////////////////////////////////////
// Call this function to permanently leave multiplayer session
void XboxOneMabStateManager::ResetMultiplayerAndGameAndSwitchState(XboxOneMabLib::GameState newState, bool leaveClean /* = true */)
{
	XboxOneMabUtils::LogComment(L"XboxOneMabStateManager::ResetMultiplayerAndGameAndSwitchState()\n");

	IsGameHostMigrating = false;
	IsLobbyHostMigrating = false;
	m_gameType = NETWORK_NONE;

	SwitchState(XboxOneMabLib::GameState::leavingMultiplayer);
	Update();

	create_task([this,newState, leaveClean]()
	{
		ResetMultiplayer(leaveClean);
		XboxOneMabNetGame->ResetGameplayData();
		SwitchState(newState);
	});
}

///////////////////////////////////////////////
// Update: main loop
void XboxOneMabStateManager::Update()
{
	auto priorState = GameState::Reset;
	auto state = GameState::Reset;
	auto pendingState = GameState::Reset;

	{
		critical_section::scoped_lock lock(m_lock);
		priorState = m_priorState;
		state = m_state;
		pendingState = m_pendingState;
	}

	if (pendingState != state)
	{
		{
			//Todo: redo the lock
			//The lock will meet some error when using the notify() function
			//as it will not return immediately and possible some changes will done
			//when switchstate called, redesign it later

			critical_section::scoped_lock lock(m_lock);
			XboxOneMabUtils::LogCommentFormat(L"SwitchState %d -> %d\n", state, pendingState);
			m_priorState = state;
			state = m_state = pendingState;


			switch (state)
			{
			case GameState::Initialize:
				InitializeEvents();
				break;
			case GameState::AcquireUser:
				validUserSignin();
				break;
			case GameState::VerifyPrivileges:
				VerifyCurrentUserPrivileges();


				break;
			case GameState::MultiplayerMenus:
				XboxOneMabNetGame->DeleteAllPlayerStates(); // delete any previous player states when returning here from the lobby or a multiplayer game			

				break;
			case GameState::CreateLobby:
				if (XboxOneMabUtils::CreatePeerNetwork())
				{
					CreateLobby();
				}
				else
				{
					RevertToPriorState();
				}
				break;
			case GameState::JoinLobby:
				if (XboxOneMabUtils::CreatePeerNetwork())
				{
					JoinLobby();
				}
				else
				{
					SwitchState(GameState::MultiplayerMenus);
				}
				break;
			case GameState::ConfirmUserSwitch:
				break;
			case GameState::Matchmaking:
				if (XboxOneMabUtils::CreatePeerNetwork())
				{
					BeginMatchmaking();
					//add current user to chat channel, default is 0
					if(true)
						XboxOneVoice->AddLocalUserToChatChannel();
				}
				else
				{
					RevertToPriorState();
				}
				break;
			case GameState::Lobby:

				break;
			case GameState::MultiplayerGame:
				
				break;
			case GameState::Suspended:

				break;
			default:
				break;
			}

			XboxOneMabUtils::LogCommentFormat(L"Finish SwitchState %d -> %d\n", state, pendingState);
		}
	}
}

////////////////////////////////////////////////////////////////////
// Entry point for all other manager
void XboxOneMabStateManager::InitializeEvents()
{
	//Perform all customerized initialization method for each manager
//	XboxOneSessionManager->Initalize(MPSD_LOBBY_TEMPLATE, MPSD_GAME_TEMPLATE, MATCH_HOPPER_NAME);
	if(XboxOneSessionManager->OnMultiplayerStateChanged == nullptr)
		XboxOneSessionManager->OnMultiplayerStateChanged = std::bind(&XboxOneMabStateManager::OnMultiplayerStateChanged, this,  std::placeholders::_1, std::placeholders::_2 );
	if(XboxOneSessionManager->OnMatchmakingChanged == nullptr)
		XboxOneSessionManager->OnMatchmakingChanged = std::bind(&XboxOneMabStateManager::OnMatchmakingStatusChanged, this, std::placeholders::_1);

	XboxOneMabUserInfo::Get()->Initialize();
	XboxOneGame->Initialize();
	XboxOneVoice->Initilize();
	create_task([this]()
	{
		Sleep(2000);
		SwitchState(GameState::MultiplayerMenus);
	});
}

bool XboxOneMabStateManager::SelectSessionMode(int gamemode /* =0 */, bool showTeamWindow /* = true*/)
{
	XboxOneMabUtils::LogCommentFormat(L"XboxOneMabStateManager::SelectSessionMode %d\n", gamemode);
	switch(gamemode)
	{
	case 0:
		XboxOneSessionManager->Initalize(MPSD_LOBBY_TEMPLATE, MPSD_GAME_TEMPLATE, MATCH_HOPPER_NAME);
		m_ruMultiPlayerType =RU_FIFTEEN;
		break;
	case 1:
		XboxOneSessionManager->Initalize(MPSD_LOBBY_TEMPLATE, MPSD_GAME_TEMPLATE_SEVEN, MATCH_HOPPER_NAME_SEVEN);
		m_ruMultiPlayerType =RU_SEVEN;
		break;
	default:
		break;
	}

	if(showTeamWindow)	
		XboxOneMessageController->Notify( XboxOneMabUISessionControllerMessage( XboxOneMabUISessionController::SESSION_EVENT_ACCEPT_GMAEMODE, true, true, &gamemode) );
	return true;
}
void XboxOneMabStateManager::VerifyCurrentUserPrivileges()
{
	XboxOneMabUtils::LogComment(L"XboxOneMabStateManager::VerifyCurrentUserPrivileges()\n");

	if(XboxOneMabUserInfo::Get()->GetCurrentUser() == nullptr || XboxOneMabUserInfo::Get()->GetCurrentUser()->IsSignedIn == false){
		SwitchState(GameState::MultiplayerMenus);
		return;
	}
	try{

		create_task(Windows::Xbox::ApplicationModel::Store::Product::CheckPrivilegeAsync(
			XboxOneGame->GetCurrentUser(),
			XPRIVILEGE_MULTIPLAYER_SESSIONS,
			false,
			nullptr)
			).then([this] (task<Windows::Xbox::ApplicationModel::Store::PrivilegeCheckResult> t)
		{
			try 
			{
				auto result = t.get();

				if (result != Windows::Xbox::ApplicationModel::Store::PrivilegeCheckResult::NoIssue)
				{
				
					XboxOneMabUtils::LogCommentFormat(L"   =PrivilegeCheckResult code: %d\n", result);
				}
				else
				{
					XboxOneMabUtils::LogComment(L"PrivilegeCheck succeeded\n");

					// If this was a protocol activation then it's time to join the lobby session to which we were invited.
					if (HasMultiplayerActivationInfo())
					{
						SwitchState(GameState::JoinLobby);
					}
					else
					{
						SwitchState(GameState::MultiplayerMenus);
					}
				}
			}
			catch(Platform::Exception^ ex)
			{
				XboxOneMabUtils::LogCommentFormat(L"Unable to verify multiplayer privileges",  XboxOneMabUtils::GetErrorStringForException(ex));
			}
		});
	}
	catch(Platform::Exception^ ex)
	{
		XboxOneMabUtils::LogCommentFormat(L"Unable to verify multiplayer privileges",  XboxOneMabUtils::GetErrorStringForException(ex));
	}
}

////////////////////////////////////////////////////////////////////
void XboxOneMabStateManager::CreateLobby()
{
	XboxOneMabUtils::LogComment(L"XboxOneMabStateManager::CreateLobby()\n");

	if (!XboxOneMabNetGame->IsNetworkOnline())
	{
		XboxOneMabUtils::LogComment(L"XboxOneMabStateManager::CreateLobby() return, network is down.\n");
		return;
	}
	auto leavePreExistingLobbyTask = create_task([]()
	{
		if (XboxOneSessionManager->GetLobbySession() != nullptr)
		{
			XboxOneSessionManager->LeaveLobbySession();
		}
	});

	leavePreExistingLobbyTask.then([this]()
	{
		try
		{
			create_task(XboxOneSessionManager->CreateLobbySessionAsync()
				).then([this](task<void> t)
			{
				try
				{
					// Observe any exceptions
					t.get();
					//Change the state, before it go to create Game session, or it will met problem
					SwitchState(GameState::Lobby);

					if(true)
						XboxOneVoice->AddLocalUserToChatChannel();
					wait(1000);
					CreateGame();
				}
				catch(Platform::Exception^ ex)
				{
					XboxOneMabUtils::LogCommentFormat(L"Error creating lobby: %ws", XboxOneMabUtils::GetErrorStringForException(ex)->Data());
				}
			});
		}
		catch (Platform::Exception^ ex)
		{
			XboxOneMabUtils::LogCommentFormat(L"Error calling create lobby : %ws", XboxOneMabUtils::GetErrorStringForException(ex)->Data());
		}
	});
}


////////////////////////////////////////////////////////////////////
//    Create the game, and I will become the host of this match
void XboxOneMabStateManager::CreateGame()
{
	XboxOneMabUtils::LogComment(L"XboxOneMabStateManager::CreateGame()\n");

	try
	{
		create_task(XboxOneSessionManager->CreateGameSessionFromLobbyAsync()
			).then([this](task<void> t)
		{
			try
			{
				// Observe any exceptions
				t.get();
				XboxOneMabUISessionController::Get()->onHostSession();
				IsGameSessionAvailable = true;
				m_gameType = NETWORK_PRIVATE_MATCH;
				try
				{
					// Advertise the game session in the lobby session properties so others can join by reading the property
					MabMap<MabString,MabString> game_params;
					if(m_ruMultiPlayerType == RU_SEVEN)
						game_params["RUGAMEMODE"] = RUGBY_SEVEN_TYPE;
					else if(m_ruMultiPlayerType == RU_FIFTEEN)
						game_params["RUGAMEMODE"] = RUGBY_FIFTEEN_TYPE;
					game_params["ONLINEMODE"] = ONLINE_PRIVATE_MATCH;
					XboxOneSessionManager->AdvertiseGameSession(&game_params);
				}
				catch (Platform::Exception^ ex)
				{
					//TODO: retry the advertisement
					XboxOneMabUtils::LogCommentFormat(L"Error calling AdvertiseGameSession (%ws)\n", XboxOneMabUtils::GetErrorStringForException(ex)->Data());
					ResetMultiplayer(true);
					XboxOneMessageController->Notify(XboxOneMabUISessionControllerMessage( 
						XboxOneMabUISessionController::SESSION_EVENT_CREATE, false, true, NULL));
				}
			}
			catch(Platform::Exception^ ex)
			{
				XboxOneMabUtils::LogCommentFormat(L"Error creating game from lobby: %ws" , XboxOneMabUtils::GetErrorStringForException(ex)->Data() );
				ResetMultiplayer(true);
				XboxOneMessageController->Notify(XboxOneMabUISessionControllerMessage( 
					XboxOneMabUISessionController::SESSION_EVENT_CREATE, false, true, NULL));
			}
		});
	}
	catch (Platform::Exception^ ex)
	{
		XboxOneMabUtils::LogCommentFormat(L"Error calling create game session from lobby: %ws" , XboxOneMabUtils::GetErrorStringForException(ex)->Data() );
		ResetMultiplayer(false);
		XboxOneMessageController->Notify(XboxOneMabUISessionControllerMessage( 
			XboxOneMabUISessionController::SESSION_EVENT_CREATE, false, true, NULL));
	}
}


void XboxOneMabStateManager::JoinLobby()
{
	XboxOneMabUtils::LogComment(L"XboxOneMabStateManager::JoinLobby()\n");

	try
	{
		create_task(XboxOneSessionManager->JoinMultiplayerAsync(
			m_multiplayerActivationInfo)
			).then([this] (task<void> t)
		{
			try
			{
				// Observe any exceptions
				t.get();
				XboxOneVoice->AddLocalUserToChatChannel();
				SwitchState(GameState::Lobby);
				JoinGame();
			}
			catch (Platform::Exception^ ex)
			{
				XboxOneMabUtils::LogCommentFormat(L"Unable to join lobby : %ws" , XboxOneMabUtils::GetErrorStringForException(ex)->Data() );
				ResetMultiplayer(true);
				XboxOneMessageController->Notify(XboxOneMabUISessionControllerMessage( 
					XboxOneMabUISessionController::SESSION_EVENT_POST_JOIN, false, true, NULL));
			}

			m_multiplayerActivationInfo = nullptr;
			XboxOneMessageController->ConsumeInvitation();
		});
	}
	catch(Platform::Exception^ ex)
	{
		m_multiplayerActivationInfo = nullptr;
		XboxOneMessageController->ConsumeInvitation();
		XboxOneMabUtils::LogCommentFormat(L"Error calling join lobby  : %ws" , XboxOneMabUtils::GetErrorStringForException(ex)->Data() );
		ResetMultiplayer(false);
		XboxOneMessageController->Notify(XboxOneMabUISessionControllerMessage( 
			XboxOneMabUISessionController::SESSION_EVENT_POST_JOIN, false, true, NULL));
	}
}

void XboxOneMabStateManager::BeginMatchmaking()
{
	XboxOneMabUtils::LogComment(L"XboxOneMabStateManager::BeginMatchmaking()\n");

	create_task(XboxOneSessionManager->BeginSmartMatchAsync()
		).then([this] (task<void> t)
	{
		bool matchmakingStarted = false;
		try
		{
			t.get();

			if (XboxOneSessionManager->GetMyState() == SessionManagerState::Matchmaking)
			{
				matchmakingStarted = true;
				m_gameType = NETWORK_QUICK_MATCH;
				// close the session to joins during matchmaking (assuming the matchmaking hasn't been canceled)
				auto lobbySession = XboxOneSessionManager->GetLobbySession();
				auto xblContext = XboxOneSessionManager->GetMyContext();
				if (lobbySession != nullptr && xblContext != nullptr)
				{
					XboxOneSessionManager->SetLobbySessionJoinRestriction(MultiplayerSessionJoinRestriction::Local);
				}
			}
			else
			{

				XboxOneMabUtils::LogComment("Failed to start matchmaking");
				XboxOneSessionManager->LeaveMultiplayer(false);
				XboxOneMessageController->OnMatchMakingFailed();
			}
		}
		catch(Platform::Exception^ ex)
		{
			XboxOneMabUtils::LogCommentFormat(L"Error starting matchmaking: %ws", XboxOneMabUtils::GetErrorStringForException(ex)->Data());
			XboxOneMessageController->OnMatchMakingFailed();
		}

		if (!matchmakingStarted)
		{
			if (m_state == GameState::Matchmaking)
			{
				RevertToPriorState();
			}

			if (OnMatchmakingCanceled)
			{
				OnMatchmakingCanceled();
			}
		}
	});
}

void XboxOneMabStateManager::JoinGame()
{
	XboxOneMabUtils::LogComment(L"XboxOneMabStateManager::JoinGame()\n");

	Platform::String^ gameSessionUri = XboxOneSessionManager->GetAdvertisedGameSessionUri();
	if (gameSessionUri == nullptr)
	{
		XboxOneMabUtils::LogComment("Game session info not available for joining");

		XboxOneMessageController->Notify(XboxOneMabUISessionControllerMessage( 
			XboxOneMabUISessionController::SESSION_EVENT_GAME_NOMATCHINFO, true, true, NULL));

		return;
	}

	try
	{
		create_task(XboxOneSessionManager->JoinGameSessionAsync(gameSessionUri)
			).then([this](task<void> t)
		{
			try
			{
				// Observe any exceptions
				t.get();

				bool success = XboxOneMabUtils::EstablishPeerNetwork(XboxOneSessionManager->GetGameSession());
				if (!success)
				{
					return;
				}

				IsGameSessionAvailable = true;
				m_gameType = NETWORK_PRIVATE_MATCH;
			}
			catch(Platform::Exception^ ex)
			{
				XboxOneMabUtils::LogCommentFormat(L"Error joining game from lobby  : %ws" , XboxOneMabUtils::GetErrorStringForException(ex)->Data() );
				XboxOneMessageController->Notify(XboxOneMabUISessionControllerMessage( 
					XboxOneMabUISessionController::SESSION_EVENT_POST_JOIN, false, true, NULL));
				ResetMultiplayer();
			}
		});
	}
	catch (Platform::Exception^ ex)
	{
		XboxOneMabUtils::LogCommentFormat(L"Error calling join game session from lobby  : %ws" , XboxOneMabUtils::GetErrorStringForException(ex)->Data() );
		XboxOneMessageController->Notify(XboxOneMabUISessionControllerMessage( 
			XboxOneMabUISessionController::SESSION_EVENT_POST_JOIN, false, true, NULL));
		ResetMultiplayer(false);
	}
}


/************************************************************************/
/*   Event Handler registered here and also change the game state       */
/************************************************************************/


////////////////////////////////////////////////////////////////////
//   Register callback function to Session Manager
void XboxOneMabStateManager::OnMultiplayerStateChanged(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session, XboxOneMabLib::SessionState arg)
{
	auto isLobbySession = XboxOneSessionManager->IsLobbySession(session);
	XboxOneMabUtils::LogCommentFormat(L"StateManager::OnMultiplayerStateChanged(%ws, %d)\n", isLobbySession ? L"LobbySession" : L"GameSession", arg);

	if (arg == SessionState::Disconnect && XboxOneSessionManager->GetMyState() != SessionManagerState::NotStarted)
	{
		if (!IsCurrentOrPendingState(GameState::MultiplayerMenus))
		{
			ResetMultiplayerAndGameAndSwitchState(GameState::MultiplayerMenus);
		}
		if(!isLobbySession)
			XboxOneMessageController->Notify(XboxOneMabUISessionControllerMessage( 
				XboxOneMabUISessionController::SESSION_EVENT_TERMINATED, true, true, NULL));

		return;
	}
	if (arg == SessionState::HostTokenChanged)
	{
		// The HostDeviceToken has changed, refresh the session to make sure we have the latest

		 XboxOneSessionManager->RefreshSessionsAsync();

		if(!isLobbySession){
//			bool success = XboxOneMabUtils::EstablishPeerNetwork(XboxOneSessionManager->GetGameSession());
		}

	}else if (arg == SessionState::MemberLeft){
		XboxOneMessageController->Notify(XboxOneMabUISessionControllerMessage( 
			XboxOneMabUISessionController::SESSION_EVENT_OPPONENT_LEFT, true, true, NULL));
	}else if (arg == SessionState::MembersChanged)
	{
		try{
			if (!isLobbySession)
			{
				// it is possible to not receive a peer removed event from the peer network when a peer loses connectivity
				// so in response to this MembersChanged event, let's flag the peer map for verification on next state update
				// (we only care about changes to the game session because that's the session in which we have established a peer network)
				PeerMapNeedsVerification = true;

				// see if we should open or close the lobby session because of a newly not full or full game session
				UpdateGameSessionFullStatusAndOpenOrCloseLobbyIfHost();
			}

			XboxOneMabUtils::LogCommentFormat(L"Session HostDeviceToken: %ws\n", session->SessionProperties->HostDeviceToken->Data());

			if(session->Members->Size < 2)
			{
				MABLOGDEBUG("Do Something for session member less than 2!");
				return;
			}

			auto hostTokenIsEmpty = session->SessionProperties->HostDeviceToken->IsEmpty();
			auto isSlot0User = session->Members->GetAt(0)->XboxUserId->Equals(session->CurrentUser->XboxUserId);
			auto isNotSessionHost = !XboxOneMabUtils::IsStringEqualCaseInsenstive(session->SessionProperties->HostDeviceToken, session->CurrentUser->DeviceToken);

			//The session is joined by my friend.
	#if 1
			if(isSlot0User && !isNotSessionHost && !isLobbySession && session->Members->Size == 2)
			{
				bool success = XboxOneMabUtils::EstablishPeerNetwork(XboxOneSessionManager->GetGameSession());
				if (!success)
				{
					return;
				}
			}
	#endif
			// Check if we should migrate host
			if (isSlot0User && (hostTokenIsEmpty || isNotSessionHost))
			{
				XboxOneMabUtils::LogCommentFormat(L"Host Migration for %ws\n", isLobbySession ? L"LobbySession" : L"GameSession");
				if (isLobbySession)
				{
					IsLobbyHostMigrating = true;
				}
				else
				{
					IsGameHostMigrating = true;
				}

				create_task(XboxOneSessionManager->HandleHostMigration(session)
					).then([this, isLobbySession] (task<HostMigrationResultArgs^> t)
				{
					try
					{
						HostMigrationResultArgs^ result = t.get();

						switch (result->Result)
						{
						case HostMigrationResult::MigrationSuccess:
							XboxOneMabUtils::LogComment(L"HostMigrationResult = MigrationSuccess\n");
							if (isLobbySession)
							{
								UpdateGameSessionFullStatusAndOpenOrCloseLobbyIfHost();
							}
							break;
						case HostMigrationResult::NoAction:
							XboxOneMabUtils::LogComment(L"HostMigrationResult = NoAction\n");
							break;
						case HostMigrationResult::SessionLeft:
							XboxOneMabUtils::LogComment(L"HostMigrationResult = SessionLeft\n");
							if (!IsCurrentOrPendingState(GameState::MultiplayerMenus))
							{
								//Send message to terminate the process and return back to the menu screen
								ResetMultiplayerAndGameAndSwitchState(GameState::MultiplayerMenus);
							}
							break;
						case HostMigrationResult::MigrateFailed:
							XboxOneMabUtils::LogComment(L"HostMigrationResult = MigrateFailed\n");
							if (!IsCurrentOrPendingState(GameState::MultiplayerMenus))
							{
								//Send message to terminate the process and return back to the menu screen
								ResetMultiplayerAndGameAndSwitchState(GameState::MultiplayerMenus);
							}
							break;
						default:
							XboxOneMabUtils::LogCommentFormat(L"Unknown HostMigrationResult (%d)\n", result->Result);
							assert(false);
							if (!IsCurrentOrPendingState(GameState::MultiplayerMenus))
							{
								//Error Happened!! Still return back to the first step
								ResetMultiplayerAndGameAndSwitchState(GameState::MultiplayerMenus);
							}
							break;
						}
					}
					catch(Platform::Exception^ ex)
					{
						if (!IsCurrentOrPendingState(GameState::MultiplayerMenus))
						{
							ResetMultiplayerAndGameAndSwitchState(GameState::MultiplayerMenus);
						}
					}

					if (isLobbySession)
					{
						IsLobbyHostMigrating = false;
					}
					else
					{
						IsGameHostMigrating = false;
					}
				});
			}
		}catch(Platform::Exception^ ex)
		{
			MABLOGDEBUG("Session member changed got exception");
		}
	}
}

////////////////////////////////////////////////////////////////////
//   Register callback function to Session Manager
void XboxOneMabStateManager::OnMatchmakingStatusChanged(XboxOneMabLib::MatchmakingArgs^ args)
{
	XboxOneMabUtils::LogComment(L"XboxOneMabStateManager::OnMatchmakingStatusChanged()\n");

	switch (args->Result)
	{
	case MatchmakingResult::MatchFound:
		XboxOneMabUtils::LogComment(L"MatchmakingResult = MatchFound\n");
		if (m_state == GameState::Matchmaking)
		{
			//!!Set the first one as the owner of the game Session
			bool success = XboxOneMabUtils::EstablishPeerNetwork(XboxOneSessionManager->GetGameSession());
//			bool success = XboxOneSessionManager->GenerateMabXboxOneClient(XboxOneSessionManager->GetGameSession());
			if (success)
			{


				//Try send message here,and test 

				create_task([]()
				{
					XboxOneMessageController->Notify(XboxOneMabUISessionControllerMessage( 
						XboxOneMabUISessionController::SESSION_SEARCH_SEARCH_COMPLETED, true, true, NULL));
				});


				// Advertise the game session in the lobby session properties so others can join by reading the property
				try
				{
					MultiplayerSession^ gameSession = XboxOneSessionManager->GetGameSession();
					Platform::String^ xbox_user_id = XboxOneGame->GetCurrentUser()->XboxUserId;

					XboxOneMabUtils::LogComment(L"Advertising game session\n");

					MabMap<MabString,MabString> game_params;
					if(m_ruMultiPlayerType == RU_SEVEN)
						game_params["RUGAMEMODE"] = RUGBY_SEVEN_TYPE;
					else if(m_ruMultiPlayerType == RU_FIFTEEN)
						game_params["RUGAMEMODE"] = RUGBY_FIFTEEN_TYPE;

					game_params["ONLINEMODE"] = ONLINE_QUICK_MATCH;

					XboxOneSessionManager->AdvertiseGameSession(&game_params);

				}
				catch (Platform::Exception^ ex)
				{
					//TODO: retry the advertisement
					XboxOneMabUtils::LogCommentFormat(L"Error calling AdvertiseGameSession (%ws)\n", XboxOneMabUtils::GetErrorStringForException(ex)->Data());
				}

				IsGameSessionAvailable = true;

				// Reopen lobby session to joins, unless game session is full
				UpdateGameSessionFullStatusAndOpenOrCloseLobbyIfHost(true);

				SwitchState(GameState::Lobby);
			}
			else
			{
				CancelMatchmaking();
			}
		}
		break;

	case MatchmakingResult::MatchFailed:
		if (m_state == GameState::Matchmaking)
		{
			XboxOneMabUtils::LogComment(L"MatchmakingResult = MatchFailed. Retrying...\n");

			//try to become the server
			XboxOneMabUtils::CreatePeerNetwork(true);
			BeginMatchmaking();
		}
		else
		{
			XboxOneMabUtils::LogCommentFormat(L"MatchmakingResult = MatchFailed. NOT Retrying (m_state = %d)...\n", m_state);
		}
		break;

	case MatchmakingResult::MatchRefreshing:
		if (m_state == GameState::Matchmaking)
		{
			XboxOneMabUtils::LogComment(L"MatchmakingResult = MatchRefreshing. Retrying...\n");

			//Do not notice.
			XboxOneMabUtils::CreatePeerNetwork(false);
			BeginMatchmaking();
		}
		else
		{
			XboxOneMabUtils::LogCommentFormat(L"MatchmakingResult = MatchRefreshing. NOT Retrying (m_state = %d)...\n", m_state);
		}
		break;
	default:
		XboxOneMabUtils::LogCommentFormat(L"Unexpected MatchmakingResult:: %s", args->Result.ToString() );
		if (m_state == GameState::Matchmaking)
		{
			CancelMatchmaking();
		}
		break;
	}
}



void XboxOneMabStateManager::UpdateGameSessionFullStatusAndOpenOrCloseLobbyIfHost(bool forceSet)
{
	XboxOneMabUtils::LogCommentFormat (L"XboxOneMabStateManager::UpdateGameSessionFullStatusAndOpenOrCloseLobbyIfHost(%ws)\n", forceSet.ToString()->Data());

	auto lobbySession = XboxOneSessionManager->GetLobbySession();
	if (lobbySession == nullptr)
	{
		IsGameSessionFull = false;
		return;
	}

	unsigned int gameSessionSize = 0;
	unsigned int gameSessionMaxSize = 1;
	auto gameSession =XboxOneSessionManager->GetGameSession();

	if (gameSession != nullptr)
	{
		gameSessionSize = gameSession->Members->Size;
		gameSessionMaxSize = gameSession->SessionConstants->MaxMembersInSession;
	}
	else
	{
		forceSet = true;
	}

	// check the game session member size to see if we need to close or open the lobby session
	IsGameSessionFull = (gameSessionSize >= gameSessionMaxSize);

	auto lobbyJoinRestriction = lobbySession->SessionProperties->JoinRestriction;
	if (lobbyJoinRestriction == MultiplayerSessionJoinRestriction::Local)
	{
		XboxOneMabUtils::LogComment(L"Current Lobby Session Join Restriction: Local\n");
	}
	else if (lobbyJoinRestriction == MultiplayerSessionJoinRestriction::None)
	{
		XboxOneMabUtils::LogComment(L"Current Lobby Session Join Restriction: None\n");
	}
	else
	{
		XboxOneMabUtils::LogCommentFormat(L"Current Lobby Session Join Restriction: %d\n", lobbyJoinRestriction);
	}

	XboxOneMabUtils::LogCommentFormat(L"Currently IsLobbyHost: %ws\n", XboxOneSessionManager->IsLobbyHost() ? L"true" : L"false");

	if (IsGameSessionFull)
	{
		if (forceSet || XboxOneSessionManager->IsLobbyHost())
		{
			XboxOneMabUtils::LogComment(L"Setting lobby session join restriction to Local\n");
			XboxOneSessionManager->SetLobbySessionJoinRestriction(MultiplayerSessionJoinRestriction::Local);
		}
	}
	else
	{
		if (forceSet || (XboxOneSessionManager->IsLobbyHost() && lobbyJoinRestriction == MultiplayerSessionJoinRestriction::Local))
		{
			XboxOneMabUtils::LogComment(L"Setting lobby session join restriction to None\n");
			XboxOneSessionManager->SetLobbySessionJoinRestriction(MultiplayerSessionJoinRestriction::None);
		}
	}

}

void XboxOneMabStateManager::CancelMatchmaking()
{
	XboxOneMabUtils::LogComment(L"XboxOneMabStateManager::CancelMatchmaking()\n");

	create_task(XboxOneSessionManager->CancelMatchmakingAsync()
		).then([this] (task<void> t)
	{
		try
		{
			// Observe any exceptions
			t.get();
		}
		catch(Platform::Exception^ ex)
		{
			XboxOneMabUtils::LogComment("Error canceling matchmaking");
		}

		ResetMultiplayer(true);
	});

	SwitchState(GameState::MultiplayerMenus);

	if (IsCurrentOrPendingState(GameState::Matchmaking))
	{
		RevertToPriorState();
	}

	if (OnMatchmakingCanceled)
	{
		OnMatchmakingCanceled();
	}
}




void XboxOneMabStateManager::OnControllerPairingChanged(Windows::Xbox::Input::ControllerPairingChangedEventArgs^ args)
{
	XboxOneMabUtils::LogCommentFormat(L"OnControllerPairingChanged(%llu) from %ws to %ws\n", 
		args->Controller->Id, 
		XboxOneMabUtils::FormatUsername(args->PreviousUser)->Data(), 
		XboxOneMabUtils::FormatUsername(args->User)->Data());

	if (XboxOneGame->GetCurrentUser() != nullptr 
		&& args->Controller->Type->Equals(L"Windows.Xbox.Input.Gamepad"))
	{
		if (args->User != nullptr && args->User->Id == XboxOneGame->GetCurrentUser()->Id)
		{
			XboxOneMabUtils::LogComment(L"   Updating gamepad\n");
			auto gamepad = dynamic_cast<Windows::Xbox::Input::IGamepad^>(args->Controller);
			XboxOneGame->SetCurrentGamePad(gamepad);

			// update controlling player index in game and in any game screens
			// Send Message outside

		}

	}
}

void XboxOneMabStateManager::ProcessJoinGameFromStart(XboxOneMabLib::MultiplayerActivationInfo^ info)
{
	XboxOneMabUtils::LogCommentFormat(L"XboxOneMabStateManager::ProcessJoinGameFromStart()");
	m_multiplayerActivationInfo = info;

	//Need some mark here to handle if the game is processed by the invitation
	//No other 
	XboxOneMessageController->AcceptInvitation();

	if (m_state == GameState::Reset || XboxOneGame->GetCurrentUser() == nullptr)
	{
		// The game is starting from a cold activation or no user has been chosen so 
		// we'll store the activation info to be picked up when we move into the game main menu
	}
	else
	{
		// A protocol activation occured during normal operation.  This means
		// that an invitation was acted upon by someone on the console.
		XboxOneMabUtils::LogCommentFormat(L"Current User Id: %ws",XboxOneGame->GetCurrentUser()->XboxUserId->Data());

		create_task([this, info]()
		{
			try{
				if (XboxOneGame->GetCurrentUser()->XboxUserId->Equals(info->TargetXuid))
				{

					MultiplayerSession^ gameSession = XboxOneSessionManager->GetGameSession();
					if(gameSession != nullptr)
					{
						if(m_gameType == NETWORK_QUICK_MATCH)
							CancelMatchmaking();
						if(m_gameType == NETWORK_PRIVATE_MATCH)
							ResetMultiplayerAndGameAndSwitchState(GameState::MultiplayerMenus);
						XboxOneMessageController->Notify(XboxOneMabUISessionControllerMessage( 
							XboxOneMabUISessionController::SESSION_EVENT_TERMINATED, true, true, NULL));
						wait(2000);
					}
					// The current user is the target user so send the message to join the lobby session
					if(!XboxOneMabMatchMaking::Get()->CheckMultiplayerPrivilege())
					{
						XboxOneMessageController->Notify( XboxOneMabUISessionControllerMessage( XboxOneMabUISessionController::SESSION_EVENT_JOIN_RESTRICTION, true, true) );
						return;
					}
					XboxOneMessageController->Notify( XboxOneMabUISessionControllerMessage( XboxOneMabUISessionController::SESSION_EVENT_ACCEPT_INVITE, true, true) );
					//Put a mark that we accept an invitation
				}
				else
				{
					// An activation for a different user was selected so we need to confirm
					//TODO: IMPLEMENT
					SwitchState(GameState::ConfirmUserSwitch);
				}
			}catch(Platform::COMException^ ex)
			{
				//Accept invitation failed
			}
		});
	}
}

bool XboxOneMabStateManager::CanlaunchByInvitation(){
	XboxOneMabUtils::LogCommentFormat(L"XboxOneMabStateManager::CanlaunchByInvitation()");

	if(m_multiplayerActivationInfo != nullptr){
		if( m_state != GameState::AcceptInvitation && XboxOneGame->GetCurrentUser()->XboxUserId->Equals(m_multiplayerActivationInfo->TargetXuid)){
			//Immediately change the state to prevent the invite box be set twice
			//Temporary fix the bug created by profileload script
			SwitchState(GameState::AcceptInvitation);
			Update();

			create_task([this]()
			{
				try{
					XboxOneMabUtils::LogCommentFormat(L"XboxOneMabStateManager::switchtomultiplayermenus()");
					if(!XboxOneMabMatchMaking::Get()->CheckMultiplayerPrivilege())
					{
						XboxOneMessageController->Notify( XboxOneMabUISessionControllerMessage( XboxOneMabUISessionController::SESSION_EVENT_JOIN_RESTRICTION, true, true) );
						return;
					}

					//Leave current session if already inside one
					MultiplayerSession^ gameSession = XboxOneSessionManager->GetGameSession();
					if(gameSession != nullptr)
					{
						if(m_gameType == NETWORK_QUICK_MATCH)
							CancelMatchmaking();
						if(m_gameType == NETWORK_PRIVATE_MATCH)
							ResetMultiplayerAndGameAndSwitchState(GameState::MultiplayerMenus);
						XboxOneMessageController->Notify(XboxOneMabUISessionControllerMessage( 
							XboxOneMabUISessionController::SESSION_EVENT_TERMINATED, true, true, NULL));
						wait(2000);
					}

					XboxOneMessageController->Notify( XboxOneMabUISessionControllerMessage( XboxOneMabUISessionController::SESSION_EVENT_ACCEPT_INVITE, true, true) );
				}catch(Platform::COMException^ ex)
				{
					//Accept invitation failed
				}
			});
			return true;
		}else{
			XboxOneMabUtils::LogComment(L"Current User is not the invitee or the game state machine is not initilized correctly");
			return false;
		}
	}else{
		return false;
	}
}

void XboxOneMabStateManager::validUserSignin()
{
	if(XboxOneMabUserInfo::Get()->IsSuspending()){

	}else if(XboxOneMabUserInfo::Get()->GetCurrentUser() == nullptr || XboxOneMabUserInfo::Get()->GetCurrentUser()->IsSignedIn == false) {
		XboxOneMessageController->onUserBecomeNull();
	}
	create_task([this]()
	{
		Sleep(2000);
		SwitchState(GameState::MultiplayerMenus);
	});

}

void XboxOneMabStateManager::OnSuspending()
{
	XboxOneSessionManager->OnMultiplayerStateChanged.~function();
	XboxOneSessionManager->OnMatchmakingChanged.~function();
	m_resumedState = m_state;
	SwitchState(GameState::Suspended);
}

void XboxOneMabStateManager::OnResumed()
{
	if(XboxOneSessionManager->OnMultiplayerStateChanged == nullptr)
		XboxOneSessionManager->OnMultiplayerStateChanged = std::bind(&XboxOneMabStateManager::OnMultiplayerStateChanged, this,  std::placeholders::_1, std::placeholders::_2 );
	if(XboxOneSessionManager->OnMatchmakingChanged == nullptr)
		XboxOneSessionManager->OnMatchmakingChanged = std::bind(&XboxOneMabStateManager::OnMatchmakingStatusChanged, this, std::placeholders::_1);

	m_state = m_resumedState;
}
