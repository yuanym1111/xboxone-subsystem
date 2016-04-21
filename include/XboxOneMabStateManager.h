#ifndef XBOX_ONE_MAB_STATE_MANAGER_H
#define XBOX_ONE_MAB_STATE_MANAGER_H

#include "XboxOneMabSingleton.h"
#include "XboxOneMabNetworkManager.h"
#include "XboxOneMabEventHandler.h"
#include <functional>
#include <memory>
#include <concurrent_queue.h>

typedef std::function<void()> MatchmakingCanceled;

class XboxOneMabStateManager :public XboxOneMabSingleton<XboxOneMabStateManager>
{
public:
	XboxOneMabStateManager();
	~XboxOneMabStateManager();

	void Initialize();
	void Update();
	void Reset();

	inline XboxOneMabLib::GameState GetMyState() { return m_state; };

	inline bool IsInLobby() {return m_state == XboxOneMabLib::GameState::Lobby;};
	inline bool IsInGame() { return m_state == XboxOneMabLib::GameState::MultiplayerGame;}
	inline bool IsQuickMatch() {return m_gameType == NETWORK_QUICK_MATCH;}
	inline bool IsPrivateMatch() {return m_gameType == NETWORK_PRIVATE_MATCH;}
	inline int GetRUGameType() {return m_ruMultiPlayerType;}

	MatchmakingCanceled OnMatchmakingCanceled;

	Concurrency::concurrent_queue<XboxOneMabLib::PeerChange> m_peerChangeQueue;
	Concurrency::concurrent_queue<XboxOneMabLib::PeerMessage> m_peerMessageQueue;

    bool IsGameSessionAvailable;
	bool IsGameSessionFull;
	bool IsGameHostMigrating;
	bool IsLobbyHostMigrating;
	bool PeerMapNeedsVerification;

	void ResetMultiplayer(bool leaveClean = false);
	void ResetMultiplayerAndGameAndSwitchState(XboxOneMabLib::GameState state = XboxOneMabLib::GameState::Reset, bool leaveClean = true);

	/************************************************************************/
	/*            Start Lobby/Game Session logic                            */
	/************************************************************************/
	void InitializeEvents();
	bool SelectSessionMode(int gamemode = 0, bool showTeamWindow = true);
	void VerifyCurrentUserPrivileges();
	void CreateLobby();
	void JoinLobby();
	void BeginMatchmaking();
	void CreateGame();
	void JoinGame();

	void CancelMatchmaking();
	void ProcessJoinGameFromStart(XboxOneMabLib::MultiplayerActivationInfo^ info);
	
	bool CanlaunchByInvitation();

	//Main logic part to state machine works, make it public for testing purpose
	void SwitchState(XboxOneMabLib::GameState newState);

	//Event Handlers
	void OnMultiplayerStateChanged(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session, XboxOneMabLib::SessionState arg);
	void OnMatchmakingStatusChanged(XboxOneMabLib::MatchmakingArgs^ args);
	void OnControllerPairingChanged(Windows::Xbox::Input::ControllerPairingChangedEventArgs^ args);

	void validUserSignin();

	void OnSuspending();

	void OnResumed();

private:

	void RevertToPriorState();
	bool IsCurrentOrPendingState(XboxOneMabLib::GameState state);

    bool HasMultiplayerActivationInfo() { return m_multiplayerActivationInfo != nullptr; }

	void UpdateGameSessionFullStatusAndOpenOrCloseLobbyIfHost(bool forceSet = false);
	XboxOneMabLib::MultiplayerActivationInfo^ m_multiplayerActivationInfo;
	Concurrency::critical_section m_lock;
	bool m_systemEventsBound;
	XboxOneMabLib::GameState m_state;
	XboxOneMabLib::GameState m_pendingState;
	XboxOneMabLib::GameState m_priorState;
	XboxOneMabLib::GameState m_resumedState;
	enum GameMode{NETWORK_NONE,NETWORK_QUICK_MATCH,NETWORK_PRIVATE_MATCH} m_gameType;
	enum GameType{RU_FIFTEEN, RU_SEVEN} m_ruMultiPlayerType;
};


#endif