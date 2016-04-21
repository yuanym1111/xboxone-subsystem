/**
 * @file XboxOneMabPartyManager.h
 * @Party management class.
 * @author jaydens
 * @22/10/2014
 */

#ifndef XBOX_ONE_MAB_PARTY_MANAGER_H
#define XBOX_ONE_MAB_PARTY_MANAGER_H

#include "MabString.h"
#include "XboxOneMabSingleton.h"


#include <MabBuffer.h>
#include <MabNetIClientAddressProvider.h>
#include "XboxOneMabSubsystem.h"
#include "xboxone/XboxOneMabNetTypes.h"

class MabNetAddress;
class XboxOneMabSessionManagerMessage;
class XboxOneMabUserInfo;



//#include "MabMemSTLAllocation.h"

struct UserPartyAssociationMapping
{
    Windows::Foundation::Collections::IVectorView<Platform::String^>^ queriedXboxUserIds;
    Platform::String^ partyId;
 //   Windows::Xbox::Multiplayer::PartyView^ partyView;
};

class XboxOneMabPartyManager:public XboxOneMabSingleton<XboxOneMabPartyManager>,
	public XboxOneMabSubsystem
//	public MabObservable< XboxOneMabSessionManagerMessage >
{
public:
	XboxOneMabPartyManager();
	~XboxOneMabPartyManager();

	void Initialize();
    void SetPartyView( Windows::Xbox::Multiplayer::PartyView^ partyView );
    Windows::Xbox::Multiplayer::PartyView^ GetPartyView();
	void Initilize();
	bool IsInitilized(){return m_isintialized;}


	virtual bool Initialise(){return true;};
	virtual void CleanUp(){};
	virtual void Update() {};

	/// Observer messages from Xbox360UserManager
//	virtual void Update(MabObservable< XboxOneMabSystemNotificationMessage > *source, const XboxOneMabSystemNotificationMessage &msg);
	void DebugPrintPartyView();

	void RefreshPartyView();
	void CreateParty();
    void AddSponsoredUserToParty(Windows::Xbox::System::User^ user);
	void RemoveLocalUsersFromParty();
    void RegisterEventHandlers();
    void UnregisterEventHandlers();
    void UnregisterGamePlayersEventHandler();
    void RegisterGamePlayersChangedEventHandler();
    bool CanJoinParty();
    bool CanInvitePartyToMyGame( Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ multiplayerSession );
    bool IsGameSessionReadyEventTriggered();
    bool DoesPartySessionExist();
    void ClearGameSessionReadyEventTriggered();
    int GetActiveAndReservedMemberPartySize();
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ GetPartyGameSession();
    void FillOpenSlotsAsNecessary();
    bool DoesPartyAndSessionPlayersMatch(
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session
        );
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ RefreshPartyGameSession();

	//added functions to join another session
	void SetSessionHost(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session);

	 std::vector<UserPartyAssociationMapping> GetUserPartyAssociationMapping() {return m_userPartyAssociationMapping;}
	HRESULT GetFriendPartyAssociations();
	HRESULT JoinFriendsParty(Platform::String^ partyId);

	void JoinExistingSession(
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session,
		Microsoft::Xbox::Services::XboxLiveContext^ xboxLiveContext
		);

	void AddFriendToReserverdSession(Microsoft::Xbox::Services::XboxLiveContext^ xboxLiveContext,  Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session);

	void setSessionReference(Windows::Xbox::Multiplayer::MultiplayerSessionReference^ partyGameSession){m_partyGameSession = partyGameSession;};

private:
	bool m_isintialized;
    Concurrency::critical_section m_lock;
    bool m_isGameSessionReadyEventTriggered;
    bool m_isGamePlayerEventRegistered;
    std::wstring m_partyId;
	Windows::Xbox::Multiplayer::PartyView^ m_partyView;
    Windows::Xbox::Multiplayer::MultiplayerSessionReference^ m_partyGameSession;
    std::vector<UserPartyAssociationMapping> m_userPartyAssociationMapping;

    static void DebugPrintPartyView( Windows::Xbox::Multiplayer::PartyView^ partyView );
	static void DebugPrintSessionView(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ currentSession);

    void OnPartyStateChanged( Windows::Xbox::Multiplayer::PartyStateChangedEventArgs^ eventArgs );
    void OnPartyRosterChanged( Windows::Xbox::Multiplayer::PartyRosterChangedEventArgs^ eventArgs );
    void OnGamePlayersChanged( Windows::Xbox::Multiplayer::GamePlayersChangedEventArgs^ eventArgs );
    void OnPartyMatchStatusChanged( Windows::Xbox::Multiplayer::MatchStatusChangedEventArgs^ eventArgs );
    void OnGameSessionReady( Windows::Xbox::Multiplayer::GameSessionReadyEventArgs^ eventArgs );
    void AddAvailableGamePlayers(
        Windows::Foundation::Collections::IVectorView<Windows::Xbox::Multiplayer::GamePlayer^>^ availablePlayers, 
        int& remainingSlots, 
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ currentSession
        );

	Windows::Foundation::Collections::IVectorView<Windows::Xbox::Multiplayer::UserPartyAssociation^>^ GetUserPartyAssociations();
//	Windows::Xbox::Multiplayer::PartyView^ GetPartyViewByPartyId(Platform::String^ partyId);
	Windows::Foundation::Collections::IVectorView<Platform::String^>^ RequestSocialList();

    // Party/Session events.
    Windows::Foundation::EventRegistrationToken m_partyRosterChangedToken;
    Windows::Foundation::EventRegistrationToken m_partyStateChangedToken;
    Windows::Foundation::EventRegistrationToken m_partyGamePlayersChangedToken;
    Windows::Foundation::EventRegistrationToken m_partyMatchStatusChangedToken;
    Windows::Foundation::EventRegistrationToken m_partyGameSessionReadyToken;

};

#endif