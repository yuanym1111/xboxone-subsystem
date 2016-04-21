/**
 * @file XboxOneMabPartyManagement.cpp
 * @Party controller.
 * @author jaydens
 * @22/10/2014
 */


#include "pch.h"

#include <collection.h>

#include "XboxOneMabPartyManager.h"
#include "XboxOneMabUserInfo.h"
#include "XboxOneMabGame.h"
#include "XboxOneMabUtils.h"
#include "XboxOneMabSessionManager.h"
#include "XboxOneMabConnect.h"
#include "XboxOneMabSystemNotificationHub.h"

using namespace Concurrency;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace Microsoft::Xbox::Services::Matchmaking;
using namespace Microsoft::Xbox::Services::Social;
using namespace Microsoft::Xbox::Services;
using namespace Windows::Foundation::Collections;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;
using namespace Windows::Storage;
using namespace Windows::System;
using namespace Windows::UI::Core;
using namespace Windows::Xbox::Multiplayer;
using namespace Windows::Xbox::Networking;
using namespace Windows::Xbox::System;

static XboxOneMabGame* gameData;
static XboxOneMabUserInfo* userInfo;
static XboxOneMabSessionManager* sessionManager;
//static XboxOneMabConnect* netConnector;

XboxOneMabPartyManager::XboxOneMabPartyManager() :
    m_isGameSessionReadyEventTriggered(false),
    m_isGamePlayerEventRegistered(false),
	m_isintialized(false)
{
	gameData = XboxOneMabGame::Get();
	userInfo = XboxOneMabUserInfo::Get();
	sessionManager = XboxOneMabSessionManager::Get();
//  put to the initialize function
//	RegisterEventHandlers();
}


void XboxOneMabPartyManager::Initialize()
{
	Initilize();
}

XboxOneMabPartyManager::~XboxOneMabPartyManager()
{
	UnregisterEventHandlers();
	UnregisterGamePlayersEventHandler();
}


void XboxOneMabPartyManager::DebugPrintPartyView( Windows::Xbox::Multiplayer::PartyView^ partyView )
{
	XboxOneMabUtils::LogComment( L"PartyView:" );
	if( partyView == nullptr )
	{
		XboxOneMabUtils::LogComment( L"     No party view" );
		return;
	}

	XboxOneMabUtils::LogComment( L"      GameSession: " + (partyView->GameSession ? partyView->GameSession->SessionName : L"NONE") );
	if( partyView->GameSession != nullptr )
	{
		XboxOneMabUtils::LogComment( L"        Template: " + partyView->GameSession->SessionTemplateName );
	}
	XboxOneMabUtils::LogComment( L"      MatchSession: " + (partyView->MatchSession ? partyView->MatchSession->SessionName : L"NONE") );
	if( partyView->MatchSession != nullptr )
	{
		XboxOneMabUtils::LogComment( L"        Template: " + partyView->MatchSession->SessionTemplateName );
	}

	XboxOneMabUtils::LogComment( L"        Members: " + partyView->Members->Size.ToString() );
	for( PartyMember^ member : partyView->Members )
	{
		XboxOneMabUtils::LogComment( L"      Member:" );
		XboxOneMabUtils::LogComment( L"          XboxUserID: " + member->XboxUserId );
		XboxOneMabUtils::LogCommentFormat( L"          IsLocalUser: %s", member->IsLocal ? L"TRUE" : L"FALSE" );
	}

	XboxOneMabUtils::LogComment( L"      ReservedMembers: " + partyView->ReservedMembers->Size.ToString() );
	for( Platform::String^ memberXuid : partyView->ReservedMembers )
	{
		XboxOneMabUtils::LogComment( L"      ReservedMember:" );
		XboxOneMabUtils::LogComment( L"          XboxUserID " + memberXuid );
	}

	XboxOneMabUtils::LogComment( L"      MembersGroupedByDevice: " + partyView->MembersGroupedByDevice->Size.ToString() );
	for( PartyMemberDeviceGroup^ partyMemberDeviceGroup : partyView->MembersGroupedByDevice )
	{
		XboxOneMabUtils::LogComment( L"        Device:" );
		for( PartyMember^ member : partyMemberDeviceGroup->Members )
		{
			XboxOneMabUtils::LogComment( L"          Member:" );
			XboxOneMabUtils::LogComment( L"            XboxUserID: " + member->XboxUserId );
			XboxOneMabUtils::LogCommentFormat( L"            IsLocalUser: %s", member->IsLocal ? L"TRUE" : L"FALSE" );
		}
	}
}

void XboxOneMabPartyManager::DebugPrintSessionView(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ currentSession)
{
	XboxOneMabUtils::LogComment( L"Party Session View:" );
	if( currentSession == nullptr )
	{
		XboxOneMabUtils::LogComment( L"     No session view" );
		return;
	}

	if( currentSession->Members != nullptr )
	{
		XboxOneMabUtils::LogComment( L"      Members: " + (currentSession->Members->Size.ToString()));

		for( MultiplayerSessionMember^ member : currentSession->Members )
		{
			XboxOneMabUtils::LogComment( L"      Member:" );
			XboxOneMabUtils::LogComment( L"          XboxUserID: " + member->XboxUserId );
			XboxOneMabUtils::LogComment( L"          DeviceToken: "+ member->DeviceToken );
		}
	}	
}



void XboxOneMabPartyManager::RefreshPartyView()
{
	XboxOneMabUtils::LogComment(L"RefreshPartyView");
    PartyView^ partyView;

    IAsyncOperation<PartyView^>^ partyOperation = Party::GetPartyViewAsyncEx(Windows::Xbox::Multiplayer::PartyFlags::PartyViewDefault);
    create_task(partyOperation)
        .then([this, &partyView] (task<PartyView^> t)
    {
        partyView = t.get();

    }).wait();

	DebugPrintPartyView(partyView);
    SetPartyView(partyView);
}

PartyView^ XboxOneMabPartyManager::GetPartyView()
{
    Concurrency::critical_section::scoped_lock lock(m_lock);
    return m_partyView;
}

void XboxOneMabPartyManager::SetPartyView( PartyView^ partyView )
{
    Concurrency::critical_section::scoped_lock lock(m_lock);
    m_partyView = partyView;
    m_partyId = m_partyView != nullptr ? Party::PartyId->Data() : L"";

}

Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ XboxOneMabPartyManager::GetPartyGameSession() 
{
	XboxOneMabUtils::LogComment(L"Enter GetPartyGameSession");
    XboxLiveContext^ xboxLiveContext = XboxOneMabUserInfo::Get()->GetCurrentXboxLiveContext();
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ multiplayerSession = nullptr;

    PartyView^ partyView = GetPartyView();
    if( partyView != nullptr && partyView->GameSession != nullptr )
    {

		multiplayerSession = XboxOneMabSessionManager::GetSessionHelper( 
            xboxLiveContext, 
			XboxOneMabUtils::ConvertToMicrosoftXboxServicesMultiplayerSessionReference(partyView->GameSession) 
            );
		DebugPrintSessionView(multiplayerSession);
    }

	XboxOneMabUtils::LogComment(L"Exit GetPartyGameSession");

    return multiplayerSession;
}
void XboxOneMabPartyManager::CreateParty()
{
    // Manually add player to a party of one if no party exists.
	IVectorView<User^>^ localUsersToAdd = XboxOneMabUserInfo::Get()->GetMultiplayerUserList();
	MABLOGDEBUG ("Creating Session %s,%d", __FILE__,__LINE__);

    // Add only m_user to the new party of one.
    User^ user = XboxOneMabUserInfo::Get()->GetCurrentUser();
	XboxOneMabUtils::LogComment(L"user is " + user->XboxUserId);
    IAsyncAction^ addMemberOperation = Party::AddLocalUsersAsync( user, localUsersToAdd );
	localUsersToAdd->Size;
	XboxOneMabUtils::LogComment(L"Local user number is " +  (localUsersToAdd->Size));
	MABLOGDEBUG ("Creating Session %s,%d", __FILE__,__LINE__);

    create_task(addMemberOperation)
        .then([this] (task<void> t)
    {
		try {
			MABLOGDEBUG ("Creating Session %s,%d", __FILE__,__LINE__);
			t.get();
			MABLOGDEBUG ("Creating Session %s,%d", __FILE__,__LINE__);
			RefreshPartyView();
			MABLOGDEBUG ("Creating Session %s,%d", __FILE__,__LINE__);

		}
		catch (Platform::Exception^ ex)
		{
			MABLOGDEBUG ("Creating Session %s,%d", __FILE__,__LINE__);
			HRESULT hr = ex->HResult;
			Platform::String^ str = XboxOneMabUtils::FormatString(L" %s [0x%0.8x]", XboxOneMabUtils::ConvertHResultToErrorName(hr)->Data(), hr );
			XboxOneMabUtils::LogComment( L"System::Add local user failed"+ str +"||" + ex->Message);
		}		
    }).wait();
	MABLOGDEBUG ("Creating Session %s,%d", __FILE__,__LINE__);

}

void XboxOneMabPartyManager::AddSponsoredUserToParty( User^ sponsoredUser )
{
    Platform::Collections::Vector<User^>^ localUsersToAdd = ref new Platform::Collections::Vector<User^>();
    localUsersToAdd->Append(sponsoredUser);

    User^ currentUser = XboxOneMabUserInfo::Get()->GetCurrentUser();
    IAsyncAction^ addMemberOperation = Party::AddLocalUsersAsync( currentUser, localUsersToAdd->GetView() );

    create_task(addMemberOperation)
        .then([this] (task<void> t)
    {
        t.get();
        RefreshPartyView();

    }).wait();
}

void XboxOneMabPartyManager::RemoveLocalUsersFromParty()
{
	// Remove local players from a party.
	IVectorView<User^>^ localUsersToRemove = XboxOneMabUserInfo::Get()->GetMultiplayerUserList();
	IAsyncAction^ removeMemberOperation = Party::RemoveLocalUsersAsync(localUsersToRemove);

	create_task(removeMemberOperation)
		.then([this] (task<void> t)
	{
		t.get();
		RefreshPartyView();
	}).wait();
}

void XboxOneMabPartyManager::RegisterGamePlayersChangedEventHandler()
{
    // Listen to Party::GamePlayersChanged
    // Only the arbiter should act on this event and detect if any party members were added that should be pulled into the game.
    EventHandler<GamePlayersChangedEventArgs^>^ partyGamePlayersChanged = ref new EventHandler<GamePlayersChangedEventArgs^>(
        [this] (Platform::Object^, GamePlayersChangedEventArgs^ eventArgs)
    {
        OnGamePlayersChanged( eventArgs );
    });
    m_partyGamePlayersChangedToken = Party::GamePlayersChanged += partyGamePlayersChanged;
    m_isGamePlayerEventRegistered = true;
}

void XboxOneMabPartyManager::RegisterEventHandlers()
{
    // Listen to Party Roster Changed
    EventHandler<PartyRosterChangedEventArgs^>^ partyRosterChangedEvent = ref new EventHandler<PartyRosterChangedEventArgs^>(
        [this] (Platform::Object^, PartyRosterChangedEventArgs^ eventArgs)
    {
        OnPartyRosterChanged( eventArgs );
    });
    m_partyRosterChangedToken = Party::PartyRosterChanged += partyRosterChangedEvent;

    // Listen to Party::MatchStatusChanged
    EventHandler<MatchStatusChangedEventArgs^>^ partyMatchStatusChanged = ref new EventHandler<MatchStatusChangedEventArgs^>(
        [this] (Platform::Object^, MatchStatusChangedEventArgs^ eventArgs)
    {
        OnPartyMatchStatusChanged( eventArgs );
    });
    m_partyMatchStatusChangedToken = Party::MatchStatusChanged += partyMatchStatusChanged;

    // Listen to Party State Changed
    EventHandler<PartyStateChangedEventArgs^>^ partyStateChangedEvent = ref new EventHandler<PartyStateChangedEventArgs^>(
        [this] (Platform::Object^, PartyStateChangedEventArgs^ eventArgs)
    {
        OnPartyStateChanged( eventArgs );
    });
    m_partyStateChangedToken = Party::PartyStateChanged += partyStateChangedEvent;
    // Listen to Game Session Ready
    EventHandler<GameSessionReadyEventArgs^>^ gameSessionReadyEvent = ref new EventHandler<GameSessionReadyEventArgs^>(
        [this] (Platform::Object^, GameSessionReadyEventArgs^ eventArgs)
    {
        OnGameSessionReady(eventArgs);
    });
    m_partyGameSessionReadyToken = Party::GameSessionReady += gameSessionReadyEvent;
}

void XboxOneMabPartyManager::UnregisterEventHandlers()
{
    Party::PartyRosterChanged -= m_partyRosterChangedToken;
    Party::MatchStatusChanged -= m_partyMatchStatusChangedToken;
    Party::PartyStateChanged -= m_partyStateChangedToken;
    Party::GameSessionReady -= m_partyGameSessionReadyToken;
}

void XboxOneMabPartyManager::UnregisterGamePlayersEventHandler()
{
    if(m_isGamePlayerEventRegistered)
    {
        Party::GamePlayersChanged -= m_partyGamePlayersChangedToken;
    }
}

// This event will fire when :
// - A new Match Session registered to party, or
// - A new Game Session is registered to party (via RegisterGame call, or as a result of matchmaking)
void XboxOneMabPartyManager::OnPartyStateChanged( PartyStateChangedEventArgs^ eventArgs )
{
	MABLOGDEBUG("Enter OnPartyStateChanged.");
    RefreshPartyView();
    //Avoid to set nullptr
	if( XboxOneMabGame::Get()->IsHost()){
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session = RefreshPartyGameSession();
		if(session != nullptr)
			XboxOneMabGame::Get()->SetMultiplayerSession(session);
	}
}

void XboxOneMabPartyManager::OnPartyRosterChanged( PartyRosterChangedEventArgs^ eventArgs )
{
	MABLOGDEBUG("Enter OnPartyRosterChanged.");




	//If I dont have a partyID yet, I need to join this party and session
	if(m_partyId.empty()){
		RefreshPartyView();

		//If we are not the host of this session, we can perform a session join automatically here
		//ONLY IT IS PUBLIC!! should be triggered somewhere else?
		if(!XboxOneMabGame::Get()->IsHost()){
			Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ sessionRef =
				XboxOneMabUtils::ConvertToMicrosoftXboxServicesMultiplayerSessionReference(eventArgs->PartyView->GameSession);
			 auto JoinSession = ref new Microsoft::Xbox::Services::Multiplayer::MultiplayerSession(XboxOneMabUserInfo::Get()->GetCurrentXboxLiveContext(), sessionRef);
			JoinExistingSession(JoinSession,userInfo->GetCurrentXboxLiveContext());
		}
	}


		
    std::wstring lastPartyId = m_partyId;

    Windows::Xbox::Multiplayer::MultiplayerSessionReference^ lastPartyGameSessionReference = m_partyGameSession;
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ lastPartyGameSession = XboxOneMabGame::Get()->GetMultiplayerSession();

    RefreshPartyView();
    std::wstring newPartyId = m_partyId;
    Windows::Xbox::Multiplayer::PartyView^ newParty = m_partyView;

	if(XboxOneMabGame::Get()->IsHost()){
	    Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ newGameSession = RefreshPartyGameSession();

//		XboxOneMabGame::Get()->HandleSessionChange(newGameSession);
	}
	    // Leave the prior game session IF
    // 1. We had a party and game session 
    // 2. The new party and game session are null or different than before
    if( !lastPartyId.empty() && lastPartyGameSession != nullptr )
    {
        if( (_wcsicmp( newPartyId.c_str(), lastPartyId.c_str()) != 0) ||
            (newParty == nullptr) ||
            (!XboxOneMabUtils::CompareWindowsXboxMultiplayerSessionReference(newParty->GameSession, lastPartyGameSessionReference)) )
        {

			MABLOGDEBUG("Session is different than before.");

            XboxLiveContext^ xboxLiveContext = userInfo->GetCurrentXboxLiveContext();
            if( xboxLiveContext != nullptr && lastPartyGameSession->Members->Size > 1)
            {
                lastPartyGameSession->Leave();
                HRESULT hr = S_OK;
				XboxOneMabSessionManager::WriteSessionHelper(
                    xboxLiveContext, 
                    lastPartyGameSession,
                    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode::UpdateExisting,
                    hr
                    );
            }
        }
//		netConnector->GetPeersAddressesFromGameSessionAndConnect(newGameSession);
    }




    if( eventArgs->RemovedMembers->Size > 0 )
    {
        if( XboxOneMabGame::Get()->IsMatchmakingInProgress() )
        {
            // If matchmaking is in progress and one of the Party member drops out, 
            // then cancel the pending match ticket and re-submit.
            bool bTicketExpired = false;
 //           XboxOneMabGame::Get()->ResubmitMatchTicket(bTicketExpired);
        }
    }

	MABLOGDEBUG("Exist OnPartyRosterChanged.");
}

void XboxOneMabPartyManager::OnPartyMatchStatusChanged( MatchStatusChangedEventArgs^ eventArgs )
{
    Windows::Xbox::Multiplayer::MatchStatus partyMatchStatus = eventArgs->MatchStatus;
    //LogCommentFormat( L"OnPartyMatchStatusChanged: Match status changed: %s", partyMatchStatus.ToString()->Data());
    if (partyMatchStatus == Windows::Xbox::Multiplayer::MatchStatus::Canceled)
    {
        //LogComment(L"Match Status was canceled");
        return;
    }

    if (XboxOneMabGame::Get()->IsSlotFillingInProgress())
    {
        XboxOneMabGame::Get()->SetIsSlotFillingInProgress(false);
        RefreshPartyView();
        FillOpenSlotsAsNecessary();
    }
    else
    {
        switch (partyMatchStatus)
        {
        case Windows::Xbox::Multiplayer::MatchStatus::Found:
            {
                //g_sampleInstance->GetGameData()->SetMatchmakingInProgress(false);
                //g_sampleInstance->OnMatchFound();
				;
            }
            break;
        case Windows::Xbox::Multiplayer::MatchStatus::Expired:
            {
                // You should resubmit the ticket if it expired.
                Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ sessionRef =
                    XboxOneMabUtils::ConvertToMicrosoftXboxServicesMultiplayerSessionReference(eventArgs->MatchSession);
                XboxLiveContext^ xboxLiveContext = XboxOneMabUserInfo::Get()->GetCurrentXboxLiveContext();
                //g_sampleInstance->HandleSessionChange(XboxOneMabSessionManager::GetSessionHelper(xboxLiveContext, sessionRef));

               // g_sampleInstance->GetGameData()->SetMatchmakingInProgress(false);
                //bool bTicketExpired = true;
                //g_sampleInstance->ResubmitMatchTicket(bTicketExpired);
            }
            break;

        default:
            break;
        }

    }
}

void XboxOneMabPartyManager::OnGameSessionReady( GameSessionReadyEventArgs^ eventArgs )
{
    MABLOGDEBUG( "Game session ready event triggered.");
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ sessionRef =
        XboxOneMabUtils::ConvertToMicrosoftXboxServicesMultiplayerSessionReference(eventArgs->GameSession);

    XboxLiveContext^ xboxLiveContext = XboxOneMabUserInfo::Get()->GetCurrentXboxLiveContext();
    IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^>^ asyncOp = xboxLiveContext->MultiplayerService->GetCurrentSessionAsync( sessionRef );
    create_task(asyncOp)
        .then([this] (task<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^> t)
    {
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session = t.get();
		m_partyGameSession =  XboxOneMabUtils::ConvertToWindowsXboxMultiplayerSessionReference(session->SessionReference);
		if (XboxOneMabGame::Get()->GetGameState() == ENUM_GAME_STATE::E_GAME_RUNNING)
        {
            // ignore when the host is just trying to fill slots in an existing game
//            XboxOneMabGame::Get()->HandleSessionChange( session );
            return;
        }
        m_isGameSessionReadyEventTriggered = true;
        //g_sampleInstance->HandleGameSessionReadyEventTriggered();
//
    }).wait();
}

void XboxOneMabPartyManager::FillOpenSlotsAsNecessary()
{
#if 0
    auto gameData = XboxOneMabGame::Get()->GetGameData();

    if (gameData == nullptr)
        return;

    // if a slot fill is already in progress, nothing to do
    if (gameData->IsSlotFillingInProgress())
        return;

    // if this match is private, we won't try to fill slots
    if (!gameData->ShouldMatchFillSlots())
    {
        return;
    }

    // if this is not the arbiter, nothing to do
    if (!gameData->IsArbiter())
    {
        return;
    }
#endif
    // if player session has no open spots, nothing to do
    MultiplayerSession^ currentSession = RefreshPartyGameSession();
	int remainingSlots = XboxOneMabSessionManager::GetEmptySlots(currentSession);
    if (remainingSlots <= 0)
        return;

    //LogCommentFormat( L"Looking for %d more players", remainingSlots );

    // first, check to see if people can be pulled from engaged parties
    User^ actingUser = XboxOneMabUserInfo::Get()->GetCurrentUser();
    auto getAvailableGamePlayersTask = Party::GetAvailableGamePlayersAsync(actingUser);

    create_task(getAvailableGamePlayersTask)
        .then([] (task<IVectorView<GamePlayer^>^ > response)
    {
        //auto gameData = XboxOneMabGame::Get()->GetGameData();

        // Since this is not a sync task, reload this instead of passing the outer scope versions in
        User^ actingUser = XboxOneMabUserInfo::Get()->GetCurrentUser();
        MultiplayerSession^ currentSession ;//= gameData->GetMultiplayerSession();
        int remainingSlots = XboxOneMabSessionManager::GetEmptySlots(currentSession);
        IVectorView<GamePlayer^>^ availablePlayers = response.get();

        // iterate through available members, add & PullPending.
        //LogCommentFormat( L"Looking for players in related parties.  There are %d available players to fill %d slots.", availablePlayers->Size, remainingSlots );
        //AddAvailableGamePlayers(availablePlayers, remainingSlots, currentSession);

        // if all slots have been filled, no need to look for more.
        if (remainingSlots <= 0)
        {
            //LogComment( L"All remaining slots have been filled; will not look for more random players" );
            //gameData->SetIsSlotFillingInProgress(false);
            return;
        }

        //gameData->SetIsSlotFillingInProgress(true);

        // if there are still open slots, send out a preserved ticket using our game session to find more people
        //LogCommentFormat( L"Still need to find %d players, sending out a match ticket", remainingSlots );
        //HRESULT hr = XboxOneMabGame::Get()->RegisterMatchSessionCompareAsync();
#if 0
        if( FAILED(hr) )
        {
            // TODO: TEMP FIX ONLY: Register crashes is a known bug in QFE2.  This is to prevent a retreat to title for running game sessions
            if (XboxOneMabGame::Get()->GetGameData()->IsSlotFillingInProgress()) 
            {
                XboxOneMabGame::Get()->GetGameData()->SetIsSlotFillingInProgress(false);
                XboxOneMabGame::Get()->ResetGameWithText("FillOpenSlotsAsNecessary: RegisterMatchSession failed. Exiting");
                return;
            }
        }
        //hr = g_sampleInstance->CreateMatchTicket(PreserveSessionMode::Always);
        if (FAILED(hr) )
        {
            //g_sampleInstance->ResetGameWithText( L"FillOpenSlotsAsNecessary: CreateMatchTicket failed. Exiting");
            return;
        }
#endif

    });

}

void XboxOneMabPartyManager::OnGamePlayersChanged( GamePlayersChangedEventArgs^ eventArgs )
{
    MABLOGDEBUG( "OnGamePlayersChanged");
    RefreshPartyView();

    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ sessionRef =
		XboxOneMabUtils::ConvertToMicrosoftXboxServicesMultiplayerSessionReference(eventArgs->GameSession);

    if(sessionRef == nullptr)
    {
        MABLOGDEBUG("OnGamePlayersChanged: invalid sessionRef.");
        return;
    }

	XboxLiveContext^ xboxLiveContext = XboxOneMabUserInfo::Get()->GetCurrentXboxLiveContext();
    IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^>^ asyncOp = xboxLiveContext->MultiplayerService->GetCurrentSessionAsync( sessionRef );
    create_task(asyncOp)
        .then([this, &eventArgs, &xboxLiveContext] (task<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^> t)
    {

        Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ currentSession = t.get();
 //       XboxOneMabGame::Get()->HandleSessionChange(currentSession);

        // Check if current user is still the host/arbiter according to mpsd document.
        // If he is no longer the arbiter, unregister GamePlayersChanged event.
		if(XboxOneMabSessionManager::IsConsoleHost(currentSession) == false)
        {
            UnregisterGamePlayersEventHandler();
            return;
        }

        int remainingSlots = XboxOneMabSessionManager::GetEmptySlots(currentSession);
        if(remainingSlots == 0)
        {
            return;
        }

        AddAvailableGamePlayers(eventArgs->AvailablePlayers, remainingSlots, currentSession);

    }).wait();

    FillOpenSlotsAsNecessary();
}

void XboxOneMabPartyManager::Initilize()
{
	if(!IsInitilized()){
		RegisterGamePlayersChangedEventHandler();
		RegisterEventHandlers();
	}
	m_isintialized = true;
}

Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ XboxOneMabPartyManager::RefreshPartyGameSession() 
{

	 MABLOGDEBUG( "Enter RefreshPartyGameSession.");

    XboxLiveContext^ xboxLiveContext = userInfo->GetCurrentXboxLiveContext();
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ multiplayerSession = nullptr;

    PartyView^ partyView = GetPartyView();
    m_partyGameSession = partyView != nullptr ? partyView->GameSession : nullptr;

    if( m_partyGameSession != nullptr )
    {
		multiplayerSession = XboxOneMabSessionManager::GetSessionHelper( 
            xboxLiveContext, 
            XboxOneMabUtils::ConvertToMicrosoftXboxServicesMultiplayerSessionReference(m_partyGameSession) 
            );
    }

    return multiplayerSession;
}


void XboxOneMabPartyManager::JoinExistingSession(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session, Microsoft::Xbox::Services::XboxLiveContext^ xboxLiveContext)
{
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ sessionRef = session->SessionReference;
	Platform::String^ localSecureDeviceAddress = SecureDeviceAddress::GetLocal()->GetBase64String();

	DebugPrintSessionView(session);

	int attempts = 0;
	while( true )
	{
		session->Join( nullptr, true);
		session->SetCurrentUserStatus( MultiplayerSessionMemberStatus::Active );
		session->SetCurrentUserSecureDeviceAddressBase64( localSecureDeviceAddress );

		HRESULT hr = S_OK;
		session = XboxOneMabSessionManager::WriteSessionHelper(
			xboxLiveContext, 
			session,
			Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode::SynchronizedUpdate,
			hr
			);
		if( FAILED(hr) && session == nullptr )
		{
			if (  hr == HTTP_E_STATUS_FORBIDDEN )
			{
				 MABLOGDEBUG( "Game session join failed.");
			}

			// Failure is expected when doing SynchronizedUpdate.  
			// Failure means that we didn't have the latest copy of the session when we tried to write it
			// Someone else must have joined since we last got a copy
			// Get a new copy and try again
			session = XboxOneMabSessionManager::GetSessionHelper( xboxLiveContext, sessionRef );

			if (session->Members->Size > MAX_SESSION_MEMBERS)
			{
				// If the session is full, then give the user an option to stay or leave the session. 
				// Titles can also keep the user engaged by taking him to the lobby, 
				// MP loadout screen, SinglePlayer mode, etc based on game design.
				MABLOGDEBUG( "Game session member already fulled.");
				return;
			}

			attempts++;
			if( attempts > 5 )
			{
				// Either very active session, or something odd happening 
				break;
			}
		}
		else
		{
			break;
		}
	}

	SetSessionHost( session );

}

void XboxOneMabPartyManager::SetSessionHost(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session)
{
	if ( session == nullptr )
	{
		return;
	}

	int attempts = 0;
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ sessionRef = session->SessionReference;
	XboxLiveContext^ xboxLiveContext = XboxOneMabUserInfo::Get()->GetCurrentXboxLiveContext();
	while( true )
	{
		MultiplayerSessionMember^ hostMember = XboxOneMabSessionManager::GetHostMember( session );
		if( hostMember == nullptr ||
			hostMember->Status == MultiplayerSessionMemberStatus::Inactive ||
			hostMember->Status == MultiplayerSessionMemberStatus::Reserved )
		{
			// Make myself new host and attempt write it synchronized
			MultiplayerSessionMember^ currentMember = XboxOneMabSessionManager::GetCurrentUserMemberInSession(session);
			session->SetHostDeviceToken( currentMember->DeviceToken );

			HRESULT hr = S_OK;
			session = XboxOneMabSessionManager::WriteSessionHelper(
				xboxLiveContext, 
				session,
				Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode::SynchronizedUpdate,
				hr
				);
			if( FAILED(hr) && session == nullptr )
			{
				// Failure is expected when doing SynchronizedUpdate.
				// Failure means that we didn't have the latest copy of the session when we tried to write it
				// Someone else must have joined since we last got a copy
				// Get a new copy and try again
				session = XboxOneMabSessionManager::GetSessionHelper( xboxLiveContext, sessionRef );

				attempts++;
				if( attempts > 5 )
				{
					// Either very active session, or something odd happening 
					break;
				}
			}
			else
			{
				// I'm now the host and arbiter
				XboxOneMabGame::Get()->SetIsHost(true);
				XboxOneMabGame::Get()->SetIsArbiter(true);

				FillOpenSlotsAsNecessary();
				break;
			}
		}
		else
		{
			// If you were joining a session that you were a host and arbiter, set IsHost and IsArbiter = true.
			if(XboxOneMabUtils::IsStringEqualCaseInsenstive(hostMember->XboxUserId, XboxOneMabUserInfo::Get()->GetCurrentUser()->XboxUserId))
			{
				XboxOneMabGame::Get()->SetIsHost(true);
				XboxOneMabGame::Get()->SetIsArbiter(true);
			}
			break;
		}
	}

//	XboxOneMabGame::Get()->OnSessionChanged( session );
}


//Should move to session manager, hot here

void XboxOneMabPartyManager::AddFriendToReserverdSession(Microsoft::Xbox::Services::XboxLiveContext^ xboxLiveContext,  Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session)
{
#if 1
	if( xboxLiveContext == nullptr )
	{
		throw ref new Platform::InvalidArgumentException(L"A valid User is required");
	}

	int startIndex = 0;
	int maxItems = 10;

	auto pAsyncOp = xboxLiveContext->SocialService->GetSocialRelationshipsAsync(
		SocialRelationship::All,
		startIndex,
		maxItems
	);


	create_task( pAsyncOp )
		.then( [this, maxItems, &session] (task<XboxSocialRelationshipResult^> resultTask)
	{

		int itemCount = 0;
		try
		{
			XboxSocialRelationshipResult^ lastResult = resultTask.get();

			MABLOGDEBUG("GetSocialRelationshipsAsync returned %u entries.",
				lastResult->TotalCount);

			if( lastResult != nullptr && lastResult->Items != nullptr )
			{
				IVectorView<XboxSocialRelationship^>^ list = lastResult->Items;

				for( UINT index = 0; index < list->Size; index++ )
				{
					XboxSocialRelationship^ person = list->GetAt(index);

					if (person != nullptr)
					{
						MABLOGDEBUG( "Person [%d]: XUID: %s IsFavorite: %s IsFollowingCaller: %s", 
							itemCount++,
							person->XboxUserId->Data(),
							person->IsFavorite.ToString()->Data(),
							person->IsFollowingCaller.ToString()->Data()
							);
						session->AddMemberReservation(person->XboxUserId,nullptr,false);
					}
				}
			}

			while (lastResult != nullptr)
			{
				if (lastResult->Items->Size == 0)
				{
					MABLOGDEBUG("Reached the end of the paged result"); 
					break;
				}

				create_task(lastResult->GetNextAsync(maxItems))
					.then([this, &lastResult, &itemCount, &session](XboxSocialRelationshipResult^ result)
				{
					lastResult = result;
					for( UINT index = 0; index < lastResult->Items->Size; index++ )
					{
						XboxSocialRelationship ^ person = lastResult->Items->GetAt(index);
						if (person != nullptr)
						{
							MABLOGDEBUG( "Person [%d]: XUID: %s IsFavorite: %s IsFollowingCaller: %s",
								itemCount++,
								person->XboxUserId->Data(),
								person->IsFavorite.ToString()->Data(),
								person->IsFollowingCaller.ToString()->Data()
								);
							session->AddMemberReservation(person->XboxUserId,nullptr,true);
						}
					}
				}).wait();
			}
		}	
		catch (Platform::Exception^ ex)
		{
			if (ex->HResult == INET_E_DATA_NOT_AVAILABLE)
			{
				// we hit the end of the list
				MABLOGDEBUG("Reach the end");
			}
			else
			{
				MABLOGDEBUG( "Error calling GetSocialAsync function: 0x%0.8x", ex->HResult);
			}
		}
	}).wait();
	DebugPrintSessionView(session);
#endif
}

HRESULT XboxOneMabPartyManager::GetFriendPartyAssociations()
{
	m_userPartyAssociationMapping.clear();

	IVectorView<UserPartyAssociation^>^ userPartyAssociations = GetUserPartyAssociations();
	if (userPartyAssociations == nullptr)
	{
		return E_FAIL;
	}

	for (UserPartyAssociation^ iUserPartyAssociaton : userPartyAssociations )
	{
		UserPartyAssociationMapping userAssociation;
		userAssociation.partyId = iUserPartyAssociaton->PartyId;
//		userAssociation.partyView = GetPartyViewByPartyId(iUserPartyAssociaton->PartyId);
		userAssociation.queriedXboxUserIds = iUserPartyAssociaton->QueriedXboxUserIds;
		m_userPartyAssociationMapping.push_back(userAssociation);
	}

	return S_OK;
}

Windows::Foundation::Collections::IVectorView<Windows::Xbox::Multiplayer::UserPartyAssociation^>^ XboxOneMabPartyManager::GetUserPartyAssociations()
{
	XboxLiveContext^ xboxLiveContext = XboxOneMabUserInfo::Get()->GetCurrentXboxLiveContext();


	Platform::Collections::Vector<Platform::String^>^ friendsList = ref new Platform::Collections::Vector<Platform::String^>();
	friendsList->Append(XboxOneMabUtils::FormatString(L"2535472221661319"));
	IVectorView<Platform::String^ >^ targetXuids = friendsList->GetView();
	IVectorView<UserPartyAssociation^>^ userPartyAssociations = nullptr;

	IAsyncOperation<IVectorView<UserPartyAssociation^>^>^ asyncOp = Party::GetUserPartyAssociationsAsync(
		XboxOneMabUserInfo::Get()->GetCurrentUser(),
		targetXuids);
	create_task(asyncOp)
		.then([this, &userPartyAssociations] (task<IVectorView<UserPartyAssociation^>^> t)
	{
		try
		{
			userPartyAssociations = t.get();
		}
		catch ( Platform::Exception^ ex )
		{
			MABLOGDEBUG( "GetUserPartyAssociationsAsync failed %s", ex->HResult  );
		}
	}).wait();

	return userPartyAssociations;
}

HRESULT XboxOneMabPartyManager::JoinFriendsParty(Platform::String^ partyId)
{

	HRESULT hr = S_OK;
#if 0
	IAsyncAction^ joinPartyOperation = Party::JoinPartyByIdAsync(
		g_sampleInstance->GetUserController()->GetCurrentUser(),
		partyId);

	create_task(joinPartyOperation)
		.then([this, &hr] (task<void> t)
	{
		try
		{
			t.get();
			LogComment(L"Successfully joined friend's party");
		}
		catch (Platform::Exception^ ex)
		{
			// Note: The join action fails if the party is full, joined by invitation only, 
			// or an attempt is made to join a user who does not follow back as a friend. 
			// The resulting error code is MP_E_PARTY_JOIN_FAILED [0x87cc000b].

			hr = ex->HResult;
			LogCommentWithError( L"JoinPartyByIdAsync failed to join", ex->HResult );
		}
	}).wait();
#endif

#if 1
	GetFriendPartyAssociations();
	Platform::String^ demopartyId = m_userPartyAssociationMapping[0].partyId;
	IAsyncAction^ joinPartyOperation = Party::JoinPartyByIdAsync(
		XboxOneMabUserInfo::Get()->GetCurrentUser(),
		demopartyId);

	create_task(joinPartyOperation)
		.then([this, &hr] (task<void> t)
	{
		try
		{
			t.get();
			MABLOGDEBUG("Successfully joined friend's party");
		}
		catch (Platform::Exception^ ex)
		{
			// Note: The join action fails if the party is full, joined by invitation only, 
			// or an attempt is made to join a user who does not follow back as a friend. 
			// The resulting error code is MP_E_PARTY_JOIN_FAILED [0x87cc000b].

			hr = ex->HResult;
			MABLOGDEBUG( "JoinPartyByIdAsync failed to join", ex->HResult );
		}
	}).wait();
#endif



	return hr;
}

void XboxOneMabPartyManager::AddAvailableGamePlayers(Windows::Foundation::Collections::IVectorView<Windows::Xbox::Multiplayer::GamePlayer^>^ availablePlayers, int& remainingSlots, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ currentSession)
{
	bool bNewMembersAdded = false;
	for (Windows::Xbox::Multiplayer::GamePlayer^ player : availablePlayers)
	{
		// Use the timeout based on what you think your title
		if( remainingSlots <= 0 )
		{
			MABLOGDEBUG( "All remaining slots have been filled; will stop adding more players in related party sessions" );
			break;
		}

		if( XboxOneMabUtils::GetTimeBetweenInSeconds(player->LastInvitedTime, XboxOneMabUtils::GetCurrentTime()) < LAST_INVITED_TIME_TIMEOUT )
		{
			MABLOGDEBUG( "Possible user just exited; skipping join request" );
			continue;
		}

		if( XboxOneMabSessionManager::IsPlayerInSession(player->XboxUserId, currentSession))
		{
			MABLOGDEBUG( "Player is already in session; skipping join request" );
			continue;
		}

		// For invites and join-in-progress flow QoS should be disabled to avoid auto-removal of an invited/joining player 
		// if the QoS metrics are not sufficient. In this scenario, a friend explicitly joined the user's game party 
		// and the intent is to attempt a connection regardless of the connection quality.

		// Note: Titles can still show an error about connection quality at a later point 
		// when the gameplay is starting/the actual connection is established.

		bool initializeRequested = false;

		currentSession->AddMemberReservation( player->XboxUserId, nullptr, initializeRequested );
		MABLOGDEBUG( "Member added: %s", player->XboxUserId->ToString() );
		bNewMembersAdded = true;
		remainingSlots--;
	}

	if(bNewMembersAdded)
	{
		XboxLiveContext^ xboxLiveContext = XboxOneMabUserInfo::Get()->GetCurrentXboxLiveContext();
		MABLOGDEBUG( "New members found and added from related parties" );
		HRESULT hr = S_OK;
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ outputSession = XboxOneMabSessionManager::WriteSessionHelper(
			xboxLiveContext, 
			currentSession,
			Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode::UpdateExisting,
			hr
			);

		if(outputSession != nullptr)
		{
			Windows::Xbox::Multiplayer::MultiplayerSessionReference^ convertedSessionRef =
				XboxOneMabUtils::ConvertToWindowsXboxMultiplayerSessionReference(currentSession->SessionReference);
			User^ actingUser = XboxOneMabUserInfo::Get()->GetCurrentUser();
			Party::PullReservedPlayersAsync(actingUser, convertedSessionRef);

//			XboxOneMabGame::Get()->HandleSessionChange( outputSession );
		}
	}
}
