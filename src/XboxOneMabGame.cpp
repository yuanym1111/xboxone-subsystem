/**
 * @file XboxOneMabGame.cpp
 * @Game status data.
 * @author jaydens
 * @13/11/2014
 */

#include "pch.h"

#include "XboxOneMabGame.h"
#include "XboxOneMabUtils.h"
#include "XboxOneMabUserInfo.h"
#include "XboxOneMabSessionManager.h"
#include "XboxOneMabStateManager.h"
#include "XboxOneMabStatisticsManager.h"

using namespace Concurrency;
using namespace Windows::Data;
using namespace Windows::Data::Json;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Microsoft::Xbox::Services;
using namespace Microsoft::Xbox::Services::Social;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace Windows::Xbox::Multiplayer;
using namespace Windows::Xbox::System;
using namespace Windows::Xbox::Networking;
using namespace XboxOneMabLib;

static XboxOneMabUserInfo* userInfo;


XboxOnePlayerState::XboxOnePlayerState(Platform::String^ displayName /*= ""*/)
{
	if (displayName->IsEmpty())
	{
		DisplayName = "Player";
	}
	else
	{
		DisplayName = displayName;
	}
	IsLocalPlayer = false;
	EnterLobby();
	m_isInactive = false;
	m_isInactive = true;
}



XboxOneMabGame::XboxOneMabGame() : m_gameState(E_GAME_NOT_START), m_isHost(false), m_have_accepted_invite(false)
{
	
}

XboxOneMabGame::~XboxOneMabGame()
{
}

void XboxOneMabGame::Initialize()
{

}

void XboxOneMabGame::SetOnlineUser()
{
	userInfo = XboxOneMabUserInfo::Get();

	Windows::Xbox::System::User^ user = userInfo->GetCurrentUser();

	if(user == nullptr){
		XboxOneMabUtils::LogComment(L"User is not login yet");
		return;
	}
	SetUser(userInfo->GetCurrentUser());
}
void XboxOneMabGame::SetMultiplayerSession( Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session )
{
    Concurrency::critical_section::scoped_lock lock(m_stateLock);
	m_multiplayerSession = session;
}

bool XboxOneMabGame::IsSlotFillingInProgress()
{
    Concurrency::critical_section::scoped_lock lock(m_stateLock);
    return m_isSlotFillInProgress;
}

void XboxOneMabGame::SetIsSlotFillingInProgress( bool isInProgress )
{
    Concurrency::critical_section::scoped_lock lock(m_stateLock);
    m_isSlotFillInProgress = isInProgress;
}

bool XboxOneMabGame::IsMatchmakingInProgress()
{
    Concurrency::critical_section::scoped_lock lock(m_stateLock);
    return m_isMatchmakingInProgress;
}

void XboxOneMabGame::SetMatchmakingInProgress( bool val )
{
    Concurrency::critical_section::scoped_lock lock(m_stateLock);
    m_isMatchmakingInProgress = val;
}

bool XboxOneMabGame::HasMatchTicketExpired()
{
    Concurrency::critical_section::scoped_lock lock(m_stateLock);
    return m_hasMatchTicketExpired;
}

void XboxOneMabGame::SetMatchTicketExpired( bool val )
{
    Concurrency::critical_section::scoped_lock lock(m_stateLock);
    m_hasMatchTicketExpired = val;
}

Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ XboxOneMabGame::GetMultiplayerSession()
{
    Concurrency::critical_section::scoped_lock lock(m_stateLock);
    return m_multiplayerSession;
}

void XboxOneMabGame::SetIsHost(bool isHost)
{
    Concurrency::critical_section::scoped_lock lock(m_stateLock);
    m_isHost = isHost;
}

bool XboxOneMabGame::IsArbiter()
{
    Concurrency::critical_section::scoped_lock lock(m_stateLock);
    return m_isArbiter;
}

void XboxOneMabGame::SetIsArbiter(bool isArbiter)
{
    Concurrency::critical_section::scoped_lock lock(m_stateLock);
    m_isArbiter = isArbiter;
}

bool XboxOneMabGame::IsMatchSession()
{
    Concurrency::critical_section::scoped_lock lock(m_stateLock);
    return m_isMatchSession;
}

void XboxOneMabGame::SetIsMatchSession(bool isMatchSession)
{
    Concurrency::critical_section::scoped_lock lock(m_stateLock);
    m_isMatchSession = isMatchSession;
}

Platform::String^ XboxOneMabGame::GetHopperName()
{
    Platform::String^ result;
    if (QOS_OPTION_TITLE_EVAL)
    {
        result = MATCH_HOPPER_TITLE_EVAL_HOPPER_NAME;
    }
    // QOS_OPTION_TITLE_EVAL and QOS_OPTION_TITLE_MEASURE all use MPSD service evaluation,
    // they also share the same hopper and session template. 
    // This is also the default template
    else
    {
        result = MATCH_HOPPER_AUTO_EVAL_HOPPER_NAME;
    }

    return result;
}

void XboxOneMabGame::SetUser(User^ user)
{
	m_user = user; 

	
	if (XboxOneSessionManager)
	{
		if (user == nullptr)
		{
			XboxOneSessionManager->SetXboxLiveContext(nullptr);
		}
	}
	

	if (XboxOneStatistics)
	{
		if (user == nullptr)
		{
			XboxOneStatistics->SetLiveContext(nullptr);
			XboxOneStatistics->SetUser(nullptr);
		}
		else
		{
			try{
			XboxOneStatistics->SetLiveContext(ref new Microsoft::Xbox::Services::XboxLiveContext(user));
			}
			catch(COMException^ ex)
			{
				XboxOneStatistics->SetLiveContext(nullptr);
			}
			XboxOneStatistics->SetUser(user);
		}
	}


	if (user != nullptr)
	{
		UpdateFriendXuidToNameMap();
	}
}


void XboxOneMabGame::SetCurrentGamePad(Windows::Xbox::Input::IGamepad^ gamePad)
{
	m_currentGamepad = gamePad;
}



void XboxOneMabGame::UpdateFriendSessionList()
{

	//Todo: set a timer here to avoid update each time

	if (XboxOneStateManager->GetMyState() != GameState::MultiplayerMenus)
	{
		return;
	}

	if (XboxOneSessionManager == nullptr)
	{
		XboxOneMabUtils::LogComment(L"XboxOneMabSessionSearchManager::UpdateFriendSessionList(): Session Manager is null\n");
		return;
	}

	if (XboxOneSessionManager->GetMyState() == SessionManagerState::NotStarted)
	{
		XboxOneMabUtils::LogComment(L"XboxOneMabSessionSearchManager::UpdateFriendSessionList(): Session Manager State == NotStarted\n");
		return;
	}

	auto currentUser = XboxOneGame->GetCurrentUser();
	if (currentUser == nullptr)
	{
		XboxOneMabUtils::LogComment(L"XboxOneMabSessionSearchManager::UpdateFriendSessionList(): Current User is null\n");
		return;
	}

	auto userContext = XboxOneSessionManager->GetMyContext();
	if (userContext == nullptr)
	{
		XboxOneMabUtils::LogComment(L"XboxOneMabSessionSearchManager::UpdateFriendSessionList(): UserContext is null\n");
		return;
	}

	MultiplayerService^ mpService = userContext->MultiplayerService;

	create_task(mpService->GetActivitiesForSocialGroupAsync(
		Windows::Xbox::Services::XboxLiveConfiguration::PrimaryServiceConfigId, 
		currentUser->XboxUserId, 
		Microsoft::Xbox::Services::Social::SocialGroupConstants::People)
		).then([this] (task<IVectorView<MultiplayerActivityDetails^>^> t)
	{
		try
		{
			auto activityDetails = t.get();
			UpdateFriendActivityDetails(activityDetails);
		}
		catch (Platform::Exception^ ex)
		{
			if (ex->HResult == 0x80190194)
			{
				XboxOneMabUtils::LogComment(L"XboxOneMabSessionSearchManager::UpdateFriendSessionList(): Friend activity lookup error - service configuration ID correct? (%ws)\n");
			}
			else
			{
				XboxOneMabUtils::LogComment(L"XboxOneMabSessionSearchManager::UpdateFriendSessionList(): Unable to retrieve friends' activities (%ws)\n");
			}
		}
	});
}





void XboxOneMabGame::UpdateFriendXuidToNameMap()
{
	XboxOneMabUtils::LogComment(L"XboxOneMabGame::UpdateFriendXuidToNameMap()\n");
	return;
	if (XboxOneSessionManager == nullptr)
		return;

	auto userContext = XboxOneSessionManager->GetMyContext();
	if (userContext != nullptr)
	{
		create_task(userContext->ProfileService->GetUserProfilesForSocialGroupAsync(SocialGroupConstants::People)).then
			([this] (task<IVectorView<XboxUserProfile^>^> t)
		{
			try
			{
				auto friendProfiles = t.get();

				critical_section::scoped_lock lock(m_friendMapLock);

				FriendXuidToNameMap.clear();
				for (const auto& friendProfile : friendProfiles)
				{
					FriendXuidToNameMap.insert(std::pair<Platform::String^,Platform::String^>(friendProfile->XboxUserId,friendProfile->GameDisplayName));
				}
			}
			catch (Platform::Exception^ ex)
			{
				XboxOneMabUtils::LogCommentFormat(L"NetGame::UpdateFriendXuidToNameMap(): GetUserProfilesForSocialGroupAsync() failed (%ws)\n", XboxOneMabUtils::GetErrorStringForException(ex)->Data());
			}
		});
	}
}

void XboxOneMabGame::UpdateFriendActivityDetails(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails ^>^ details)
{
	bool friendCollectionNeedsUpdate = false;
	{
		// lock so we can clear the handle list and update the menu entries
		// Todo: Put the lock here
		if (XboxOneStateManager->GetMyState() != GameState::MultiplayerMenus)
		{
			return;
		}

		int friendSessionCount = 0;

		// Update menus with any joinable friend sessions
		for (const auto& activity : details)
		{
			Platform::String^ playerXuid = activity->OwnerXboxUserId;
			Platform::String^ friendName = "";

			{
				// lock while we try to look up the friend's display name for this player xuid
				critical_section::scoped_lock lock(m_friendMapLock);
				if (FriendXuidToNameMap.count(playerXuid))
				{
					friendName = FriendXuidToNameMap[playerXuid];
				}
				else
				{
					friendCollectionNeedsUpdate = true;
					continue;
				}
			}

			XboxOneMabUtils::LogCommentFormat(L"Found activity for %ws (%ws): Closed: %ws; JoinRestriction: %d; TitleId: %u; Visibility: %d\n",
				friendName->Data(),
				playerXuid->Data(),
				activity->Closed.ToString()->Data(),
				activity->JoinRestriction,
				activity->TitleId,
				activity->Visibility);

			bool joinableSession = activity->TitleId.ToString() == Windows::Xbox::Services::XboxLiveConfiguration::TitleId 
				&& activity->MembersCount < activity->MaxMembersCount
				&& !activity->Closed
				&& activity->JoinRestriction != MultiplayerSessionJoinRestriction::Local
				&& activity->Visibility == MultiplayerSessionVisibility::Open;

			if (joinableSession)
			{

				m_friendSessionHandles.push_back(ref new MultiplayerActivationInfo(activity->HandleId, m_user->XboxUserId, playerXuid));
				friendSessionCount++;
				XboxOneMabUtils::LogCommentFormat(L"Added Entry: %ws, TitleId: %u;\n", friendName->Data(), activity->TitleId);
			}

			if (friendSessionCount >= MAX_PLAYER_SESSION_ENTRIES)
				break;
		}

		// Clear any unused menu labels
		if (friendSessionCount == 0)
		{
			//There is no session to join
		}
		
	}

	if (friendCollectionNeedsUpdate)
	{
		UpdateFriendXuidToNameMap();
	}
}
