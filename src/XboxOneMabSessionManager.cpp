/**
 * @file XboxOneMabSessionManagement.cpp
 * @Session Controller.
 * @author jaydens
 * @22/10/2014
 */

#include "pch.h"

#include "XboxOneMabSessionManager.h"
#include "XboxOneMabUtils.h"
#include "XboxOneMabGame.h"
#include "XboxOneMabUserInfo.h"
#include "XboxOneMabStateManager.h"
#include "XboxOneMabConnect.h"
#include "XboxOneMabVoiceManager.h"
#include "XboxOneMabUISessionController.h"
#include "MabLog.h"

using namespace Concurrency;
using namespace Windows::Data;
using namespace Windows::Data::Json;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Microsoft::Xbox::Services;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace Windows::Xbox::Multiplayer;
using namespace Windows::Xbox::Services;
using namespace Windows::Xbox::System;
using namespace Windows::Xbox::Networking;
using namespace Platform::Collections;
using namespace Microsoft::Xbox::Services::Matchmaking;
using namespace XboxOneMabLib;

void XboxOneMabSessionManager::Initalize( 
	Platform::String^ lobbySessionTemplate,
	Platform::String^ gameSessionTemplate,
	Platform::String^ matchmakingHopper)
{
	//Should check whether is initialized
	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::Initalize()\n");
	m_state = SessionManagerState::NotStarted;
	m_matchmakingState = MatchmakingState::None;
	m_gameSessionTemplate = gameSessionTemplate;
	m_lobbySessionTemplate = lobbySessionTemplate;
	m_hopperName = matchmakingHopper;
	m_serviceConfigId = XboxLiveConfiguration::PrimaryServiceConfigId;
	
	m_sessionChangeToken.Value = 0;
	m_subscriptionLostToken.Value = 0;
	//add the template to get address
	m_peerDeviceAssociationTemplate = SecureDeviceAssociationTemplate::GetTemplateByName("PeerTrafficUDP");

	if(m_peerDeviceAssociationTemplate)
	{
		// Listen to AssociationIncoming event
		TypedEventHandler<Windows::Xbox::Networking::SecureDeviceAssociationTemplate^, Windows::Xbox::Networking::SecureDeviceAssociationIncomingEventArgs^>^ associationIncomingEvent = 
			ref new TypedEventHandler<Windows::Xbox::Networking::SecureDeviceAssociationTemplate^, Windows::Xbox::Networking::SecureDeviceAssociationIncomingEventArgs^>(
			[this] (Windows::Xbox::Networking::SecureDeviceAssociationTemplate^ associationTemplate, Windows::Xbox::Networking::SecureDeviceAssociationIncomingEventArgs^ args)
		{
			OnAssociationIncoming( associationTemplate, args );
		});
		m_associationIncomingToken = m_peerDeviceAssociationTemplate->AssociationIncoming += associationIncomingEvent;
	}

	if(m_xblContext)
	{
		//Reset the session listener
		//StopListeningToMultiplayerEvents();
		//StartListeningToMultiplayerEvents();
	}
}

XboxOneMabSessionManager::XboxOneMabSessionManager()
{
	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::XboxOneMabSessionManager()\n");
	m_lobbySession = nullptr;
	m_gameSession = nullptr;
	m_xblContext = nullptr;
	m_matchTicketResponse = nullptr;

	m_remoteClient = nullptr;
	m_lastProcessedLobbyChange = 0;
	m_lastProcessedGameChange = 0;
	m_bLeavingSession = false;

	m_sessionID = GUID();

	if(m_peerDeviceAssociationTemplate)
	{
		m_peerDeviceAssociationTemplate->AssociationIncoming -= m_associationIncomingToken;
		m_peerDeviceAssociationTemplate = nullptr;
	}
}

XboxOneMabSessionManager::~XboxOneMabSessionManager()
{
}

Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ 
XboxOneMabSessionManager::GetCurrentSessionHelper(
    Microsoft::Xbox::Services::XboxLiveContext^ xboxLiveContext,
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session
    )
{
    if(session == nullptr)
    {
        //LogComment(L"GetCurrentSessionHelper: invalid session.");
        return nullptr;
    }
    return GetSessionHelper(xboxLiveContext, session->SessionReference);
}

Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ 
XboxOneMabSessionManager::GetSessionHelper(
    Microsoft::Xbox::Services::XboxLiveContext^ xboxLiveContext,
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ sessionRef
    )
{
    if(sessionRef == nullptr)
    {
        MABLOGDEBUG("GetSessionHelper: invalid sessionRef.");
        return nullptr;
    }

    auto asyncOp = xboxLiveContext->MultiplayerService->GetCurrentSessionAsync( sessionRef );

    Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ multiplayerSession;

    create_task(asyncOp)
    .then([&multiplayerSession] (task<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^> t)
    {
        multiplayerSession = t.get(); // if t.get() didn't throw, it succeeded
        //LogComment( L"GetCurrentSessionAsync: " + multiplayerSession->SessionReference->SessionName );
    })
    .wait();

    return multiplayerSession;
}

Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ 
XboxOneMabSessionManager::WriteSessionHelper(
    Microsoft::Xbox::Services::XboxLiveContext^ xboxLiveContext,
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ multiplayerSession,
    MultiplayerSessionWriteMode writeMode,
    HRESULT& hr
    )
{
    if (multiplayerSession == nullptr)
    {
        XboxOneMabUtils::LogComment(L"WriteSessionHelper: invalid multiplayerSession.");
        return nullptr;
    }
	MABLOGDEBUG("In session submitting, %s,%d", __FILE__,__LINE__);

    auto asyncOpWriteSessionAsync = xboxLiveContext->MultiplayerService->WriteSessionAsync(
        multiplayerSession, 
        writeMode
        );
	MABLOGDEBUG("In session submitting, %s,%d", __FILE__,__LINE__);
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ outputMultiplayerSession = nullptr;
	MABLOGDEBUG("In session submitting, %s,%d", __FILE__,__LINE__);
    create_task(asyncOpWriteSessionAsync)
    .then([&outputMultiplayerSession, &hr](task<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^> t)
    {
		try {
			MABLOGDEBUG("In session submitting, %s,%d", __FILE__,__LINE__);
			outputMultiplayerSession = t.get(); // if t.get() didn't throw, it succeeded
			MABLOGDEBUG("In session submitting, %s,%d", __FILE__,__LINE__);

			if( outputMultiplayerSession != nullptr && 
				outputMultiplayerSession->SessionReference != nullptr )
			{
				XboxOneMabUtils::LogComment( L"Write Session succeeded: " + outputMultiplayerSession->SessionReference->SessionName );
			}
		}
        catch (Platform::Exception^ ex)
        {
			XboxOneMabUtils::LogComment( L"Write Session failed! ");
            hr = ex->HResult;
            XboxOneMabUtils::LogCommentWithError( L"WriteSessionAsync failed", ex->HResult );
        }
    })
    .wait();

    return outputMultiplayerSession;
}

HRESULT XboxOneMabSessionManager::RegisterGameSessionCompareHelper( 
    Microsoft::Xbox::Services::XboxLiveContext^ xboxLiveContext,
	Windows::Xbox::Multiplayer::MultiplayerSessionReference^ sessionReference,
	Windows::Xbox::Multiplayer::MultiplayerSessionReference^ sessionReferenceComparand
    )
{
    if (sessionReference == nullptr)
    {
        MABLOGDEBUG("RegisterGameSessionCompareHelper: invalid sessionReference.");
        return E_FAIL;
    }
	if(sessionReferenceComparand != nullptr){
		HRESULT hr = S_OK;
		XboxOneMabUtils::LogCommentFormat( L"RegisterGameSessionCompareAsync \nsessionReference " , sessionReference->SessionName );

		IAsyncAction^ partyOperation = Party::RegisterGameSessionCompareAsync(
			xboxLiveContext->User,
			sessionReference,
			sessionReferenceComparand);

		create_task( partyOperation)
		.then( [&hr] (task<void> t)
		{
			t.get(); // if t.get() didn't throw, it succeeded

			// From this point on, Title is safe to be PLM'd.  If Title is in foreground when Game Session is set up, Title will get GameSessionReady.
			// If Title is not in foreground, User will receive Toast to launch the Title again. Title should check if there is Game Session in the Activation path.

			// Title should convey to user, "We are setting up your game. In meantime, go enjoy our non-multiplayer features such as Single Player or something else".

		})
		.wait();
	
		return hr;
	}else{
		HRESULT hr = S_OK;
		XboxOneMabUtils::LogCommentFormat( L"RegisterGameSessionCompareAsync \nsessionReference " , sessionReference->SessionName );

		IAsyncAction^ partyOperation = Party::RegisterGameSessionAsync(
			xboxLiveContext->User,
			sessionReference);

		create_task( partyOperation)
			.then( [&hr] (task<void> t)
		{
			t.get();

		})
			.wait();

		return hr;
	}
}

HRESULT XboxOneMabSessionManager::RegisterMatchSessionCompareHelper(
    Microsoft::Xbox::Services::XboxLiveContext^ xboxLiveContext,
	Windows::Xbox::Multiplayer::MultiplayerSessionReference^ sessionReference,
	Windows::Xbox::Multiplayer::MultiplayerSessionReference^ sessionReferenceComparand
    )
{
    if (sessionReference == nullptr)
    {
        //LogComment(L"RegisterMatchSessionCompareHelper: invalid multiplayerSession.");
        return E_FAIL;
    }

    HRESULT hr = S_OK;
    //LogComment( L"RegisterMatchSessionCompareAsync " + sessionReference->SessionName );

    IAsyncAction^ partyOperation = Party::RegisterMatchSessionCompareAsync(
        xboxLiveContext->User,
        sessionReference,
		sessionReferenceComparand);

    create_task( partyOperation)
    .then( [&hr] (task<void> t)
    {
        t.get(); // if t.get() didn't throw, it succeeded

        // From this point on, Title is safe to be PLM'd.  If Title is in foreground when Game Session is set up, Title will get GameSessionReady.
        // If Title is not in foreground, User will receive Toast to launch the Title again. Title should check if there is Game Session in the Activation path.

        // Title should convey to user, "We are setting up your game. In meantime, go enjoy our non-multiplayer features such as Single Player or something else".

    })
    .wait();

    return hr;
}

Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ XboxOneMabSessionManager::CreateMultiplayerSession()
{
	QualityOfService::PublishPrivatePayload(ref new Platform::Array<BYTE>(1));

    Platform::String^ sessionName;

    GUID sessionNameGUID;
    CoCreateGuid( &sessionNameGUID );
    Platform::Guid sessionGuidName = Platform::Guid( sessionNameGUID );
	sessionName = XboxOneMabUtils::RemoveBracesFromGuidString(sessionGuidName.ToString());

	XboxLiveContext^ xboxLiveContext = XboxOneMabUserInfo::Get()->GetCurrentXboxLiveContext();

    Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session = ref new Microsoft::Xbox::Services::Multiplayer::MultiplayerSession(
		xboxLiveContext,
        ref new Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference(
        GAME_SERVICE_CONFIG_ID,
        MATCH_SESSION_TEMPLATE_NAME,
        sessionName
        ),
        MAX_MEMBER_LIMIT, 
        false,
        MultiplayerSessionVisibility::Private,
        nullptr,
        nullptr
        );
	MABLOGDEBUG ("Creating Session %s,%d", __FILE__,__LINE__);

	return session;

}

bool XboxOneMabSessionManager::IsPlayerHost(
    Platform::String^ xboxUserId,
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session
    )
{
    if( session == nullptr )
    {
        return false;
    }

    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^ hostMember = GetHostMember(session);
    if( hostMember != nullptr )
    {
		if( XboxOneMabUtils::IsStringEqualCaseInsenstive(xboxUserId, hostMember->XboxUserId ) )
        {
            return true;
        }
    }

    return false;
}


Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^ 
XboxOneMabSessionManager::GetHostMember( 
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session 
    )
{
    if (session == nullptr)
    {
        //LogComment(L"GetHostMember: invalid session.");
        return nullptr;
    }

    Platform::String^ hostDeviceToken = session->SessionProperties->HostDeviceToken;
    if(hostDeviceToken == nullptr)
    {
        return nullptr;
    }

    for each (MultiplayerSessionMember^ member in session->Members)
    {
		if( XboxOneMabUtils::IsStringEqualCaseInsenstive(member->DeviceToken, hostDeviceToken ) )
        {
            return member;
        }
    }

    return nullptr;
}

bool
XboxOneMabSessionManager::IsMemberTheHost( 
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session,
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^ member
    )
{
    if (session == nullptr)
    {
        XboxOneMabUtils::LogComment(L"IsMemberTheHost: invalid session.");
        return false;
    }

    Platform::String^ hostDeviceToken = session->SessionProperties->HostDeviceToken;
	if( XboxOneMabUtils::IsStringEqualCaseInsenstive(member->DeviceToken, hostDeviceToken) )
    {
        return true;
    }

    return false;
}

int
XboxOneMabSessionManager::GetEmptySlots(
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session
    )
{
    if(session == nullptr)
    {
        //LogComment(L"GetEmptySlots: invalid session.");
        return 0;
    }
    return session->SessionConstants->MaxMembersInSession - session->Members->Size;
}

Concurrency::task<void> XboxOneMabSessionManager::LeaveAllSessions()
{
	XboxOneMabUtils::LogComment( L"LeaveAllSessions" );

    Concurrency::task<void> result = create_task([]
    {
        IVectorView<User^>^ userList = User::Users;

        for(User^ user : userList)
        {
            XboxLiveContext^ xboxLiveContext = ref new XboxLiveContext(user);

            //leave sessions
            IAsyncOperation<IVectorView<MultiplayerSessionStates^>^>^ op = xboxLiveContext->MultiplayerService->GetSessionsAsync(
                GAME_SERVICE_CONFIG_ID,
                nullptr,
                xboxLiveContext->User->XboxUserId,
                nullptr,
                MultiplayerSessionVisibility::Any,
                0,
                false, // Don't include private sessions
                false, // Don't remove us from any "reserved" slots - those are invites to join party games
                false, // Don't include inactive sessions
                32
                );

            create_task(op)
            .then([xboxLiveContext] (task<IVectorView<MultiplayerSessionStates^>^> t)
            {

				IVectorView<MultiplayerSessionStates^>^ collection = t.get();
				for(unsigned i = 0; i < collection->Size; ++i)
				{
					MultiplayerSessionStates^ state = collection->GetAt(i);
					IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^>^ asyncOp = 
						xboxLiveContext->MultiplayerService->GetCurrentSessionAsync(state->SessionReference);

					create_task(asyncOp)
					.then([xboxLiveContext] (task<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^> t)    
					{

						auto multiplayerSession = t.get(); // if t.get() didn't throw, it succeeded

						//XboxOneMabUtils::LogCommentFormat( L"[SESSION] Found old session, now leaving: %s", multiplayerSession->SessionReference->SessionName->Data());
						//XboxOneMabUtils::LogCommentFormat( L"\tCorrelationId: %s", multiplayerSession->MultiplayerCorrelationId->Data());
						//XboxOneMabUtils::LogCommentFormat( L"\t# of Members: %d", multiplayerSession->Members->Size);
						//XboxOneMabUtils::LogCommentFormat( L"\tCustom JSON: %s", multiplayerSession->SessionProperties->SessionCustomPropertiesJson->Data());

						// Leave the session
						multiplayerSession->Leave();

						HRESULT hr = S_OK;
						multiplayerSession = XboxOneMabSessionManager::WriteSessionHelper(
							xboxLiveContext, 
							multiplayerSession,
							Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode::UpdateOrCreateNew,
							hr
							);
						if( multiplayerSession == nullptr )
						{
							;
							XboxOneMabUtils::LogComment( L"WriteSessionAsync failed" );
						}

					})
					.wait();
                }

            })
            .wait();
        }
    });

    return result;
}

bool XboxOneMabSessionManager::IsConsoleHost(
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session
    )
{
    if( session == nullptr )
    {
        return false;
    }


    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^ hostMember = GetHostMember(session);
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^ currentMember = GetCurrentUserMemberInSession(session);
    if( hostMember != nullptr && hostMember != nullptr )
    {
		if( XboxOneMabUtils::IsStringEqualCaseInsenstive(currentMember->DeviceToken, hostMember->DeviceToken) )
        {
            return true;
        }
    }

    return false;
}

Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^ 
XboxOneMabSessionManager::GetCurrentUserMemberInSession( 
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session
    )
{
    if( session == nullptr )
    {
        //LogComment(L"GetCurrentUserMemberInSession: invalid session.");
        return nullptr;
    }

	User^ user = XboxOneMabUserInfo::Get()->GetCurrentUser();

    for each (MultiplayerSessionMember^ member in session->Members)
    {
        if( XboxOneMabUtils::IsStringEqualCaseInsenstive(member->XboxUserId, user->XboxUserId) )
        {
            return member;
        }
    }

    return nullptr;
}

void XboxOneMabSessionManager::ListAllExistingSessions()
{
	User^ user = User::Users->GetAt(0);
	XboxLiveContext^ xboxLiveContext = ref new XboxLiveContext(user);
	MABLOGDEBUG("in ListAllExistingSessions, %s,%d", __FILE__, __LINE__);

	IAsyncOperation<IVectorView<MultiplayerSessionStates^>^>^ op = xboxLiveContext->MultiplayerService->GetSessionsAsync(
		GAME_SERVICE_CONFIG_ID,
		nullptr,
		xboxLiveContext->User->XboxUserId,
		nullptr,
		MultiplayerSessionVisibility::Any,
		0,
		false,
		false,
		false,
		32
		);

        create_task(op)
        .then([xboxLiveContext] (task<IVectorView<MultiplayerSessionStates^>^> t)
        {
            try
            {
                IVectorView<MultiplayerSessionStates^>^ collection = t.get();
				MABLOGDEBUG("in ListAllExistingSessions, %s,%d", __FILE__, __LINE__);
                for(unsigned i = 0; i < collection->Size; ++i)
                {
                    MultiplayerSessionStates^ state = collection->GetAt(i);
                    IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^>^ asyncOp = 
                        xboxLiveContext->MultiplayerService->GetCurrentSessionAsync(state->SessionReference);
					MABLOGDEBUG("in ListAllExistingSessions, %s,%d", __FILE__, __LINE__);

                    create_task(asyncOp)
                    .then([xboxLiveContext] (task<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^> t)    
                    {
                        try
                        {
                            auto multiplayerSession = t.get(); // if t.get() didn't throw, it succeeded

                            XboxOneMabUtils::LogCommentFormat( L"[SESSION] Found old session, now leaving: %s", multiplayerSession->SessionReference->SessionName->Data());
                            XboxOneMabUtils::LogCommentFormat( L"\tCorrelationId: %s", multiplayerSession->MultiplayerCorrelationId->Data());
                            XboxOneMabUtils::LogCommentFormat( L"\t# of Members: %d", multiplayerSession->Members->Size);
                            XboxOneMabUtils::LogCommentFormat( L"\tCustom JSON: %s", multiplayerSession->SessionProperties->SessionCustomPropertiesJson->Data());

                        }
                        catch (Platform::Exception^ ex)
                        {
							MABLOGDEBUG("in ListAllExistingSessions, %s,%d", __FILE__, __LINE__);
                            XboxOneMabUtils::LogCommentWithError( L"GetCurrentSessionAsync failed", ex->HResult );
                        }
                    })
                    .wait();
                }
            }
            catch (Platform::Exception^ ex)
            {
                XboxOneMabUtils::LogCommentWithError( L"GetSessionsAsync failed", ex->HResult );
            }
        })
        .wait();

}

bool XboxOneMabSessionManager::IsPlayerInSession(Platform::String^ xboxUserId, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session)
{
	if( session == nullptr )
	{
		 XboxOneMabUtils::LogCommentFormat(L"IsPlayerInSession: invalid session.");
		return false;
	}

	for (MultiplayerSessionMember^ member : session->Members)
	{
		if( XboxOneMabUtils::IsStringEqualCaseInsenstive(xboxUserId, member->XboxUserId ) )
		{
			return true;
		}
	}

	return false;
}

MultiplayerActivationInfo^ XboxOneMabSessionManager::GetMultiplayerActivationInfo(Windows::ApplicationModel::Activation::IProtocolActivatedEventArgs^ args)
{
	MultiplayerActivationInfo^ returnValue = nullptr;
	auto url = ref new Windows::Foundation::Uri(args->Uri->RawUri);

	Platform::String^ queryGet = args->Uri->Query;

	XboxOneMabUtils::LogCommentFormat(L"App activation urlHost: %ws\n", url->Host->Data());

	if (XboxOneMabUtils::IsStringEqualCaseInsenstive(url->Host, "inviteHandleAccept"))
	{
		// This is triggered off a game invite toast
		returnValue = ref new MultiplayerActivationInfo(
			url->QueryParsed->GetFirstValueByName("handle"),
			url->QueryParsed->GetFirstValueByName("invitedXuid")
			);
	}                        
	else if(XboxOneMabUtils::IsStringEqualCaseInsenstive(url->Host, "activityHandleJoin"))
	{
		// This is from someone joining your activity
		returnValue = ref new MultiplayerActivationInfo(
			url->QueryParsed->GetFirstValueByName("handle"),
			url->QueryParsed->GetFirstValueByName("joinerXuid"),
			url->QueryParsed->GetFirstValueByName("joineeXuid")
			);
	}

	return returnValue;
}

Platform::String^ XboxOneMabSessionManager::GenerateUniqueName()
{
	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::GenerateUniqueName()\n");

	GUID sessionNameGUID;
	CoCreateGuid( &sessionNameGUID );
	m_sessionID = sessionNameGUID;
	Platform::Guid sessionGuidName = Platform::Guid( sessionNameGUID );
	return XboxOneMabUtils::RemoveBracesFromGuidString(sessionGuidName.ToString());
}

void XboxOneMabSessionManager::SetXboxLiveContext(Microsoft::Xbox::Services::XboxLiveContext^ context)
{
	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::SetXboxLiveContext()\n");

	critical_section::scoped_lock lock(m_stateLock);

	if (m_xblContext != nullptr)
	{
		StopListeningToMultiplayerEvents();
	}

	m_state = SessionManagerState::NotStarted;
	m_xblContext = context;

	if (m_xblContext != nullptr)
	{
		StartListeningToMultiplayerEvents();
		m_state = SessionManagerState::Running;

#ifdef _DEBUG
		// This gives SUPER GREAT debug spew for all MPSD calls.
		m_xblContext->Settings->DiagnosticsTraceLevel = Microsoft::Xbox::Services::XboxServicesDiagnosticsTraceLevel::Verbose;
		m_xblContext->Settings->EnableServiceCallRoutedEvents = true;
		m_xblContext->Settings->ServiceCallRouted += ref new EventHandler<XboxServiceCallRoutedEventArgs^>([=] (Platform::Object^, XboxServiceCallRoutedEventArgs^ args)
		{
			if (args != nullptr)
			{
				XboxOneMabUtils::LogCommentFormat(L"%ws\n", args->FullResponseToString->Data());
			}
		});
#endif
	}
}

// OnSessionChanged 
//      Event handler for the session object changes.
//          - Retrieve current session
//          - Diff with the previous session
//          - Dispatch work based on change type
//
void XboxOneMabSessionManager::OnSessionChanged(Platform::Object^ object, Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivityMultiplayerSessionChangeEventArgs^ arg)
{
	critical_section::scoped_lock taskLock(m_taskLock);
	XboxOneMabUtils::LogCommentFormat(L"XboxOneMabSessionManager::OnSessionChanged(%d): %ws\n", static_cast<int>(arg->ChangeNumber),arg->SessionReference->SessionName->Data());
	
	XboxLiveContext^ xblContext = nullptr;
	{
		critical_section::scoped_lock lock(m_stateLock);
		xblContext = m_xblContext;
	}

	if (xblContext == nullptr)
	{
		return;
	}

	MultiplayerSession^ currentSession = nullptr;

	// Retrieve the session object referenced in the event args

	try{

		create_task(
			xblContext->MultiplayerService->GetCurrentSessionAsync(
			arg->SessionReference
			)
			).then([this, &currentSession](task<MultiplayerSession^> t)
		{
			try
			{
				currentSession = t.get();
			}
			catch(Platform::COMException^ ex)
			{
				XboxOneMabUtils::LogCommentFormat(L"Session Changed return null for the current session: %ws\n", XboxOneMabUtils::GetErrorStringForException(ex)->Data());
				currentSession = nullptr;
			}
		}
		).wait();

	}
	catch(Platform::COMException^ ex)
	{
		XboxOneMabUtils::LogCommentFormat(L"OnSessionChanged:: GetCurrentSessionAsync threw %ws\n", XboxOneMabUtils::GetErrorStringForException(ex)->Data());
		currentSession = nullptr;
	}
	if (currentSession != nullptr)
	{
		ProcessSessionChange(currentSession);
	}

}

void XboxOneMabSessionManager::OnSubscriptionLost(Platform::Object^ object, Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivityMultiplayerSubscriptionsLostEventArgs^ arg)
{

}

//  StartListeningToMultiplayerEvents
//      - It'll register the session token to the SessionStateChanged event
//
void XboxOneMabSessionManager::StartListeningToMultiplayerEvents()
{
	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::StartListeningToMultiplayerEvents()\n");
	//assert(m_xblContext != nullptr);

	if (m_xblContext == nullptr)
	{
		return;
	}

	if (!m_xblContext->RealTimeActivityService->MultiplayerSubscriptionsEnabled)
	{

		EventHandler<RealTimeActivity::RealTimeActivityMultiplayerSessionChangeEventArgs^>^ multiplayerSessionChanged = ref new EventHandler<RealTimeActivity::RealTimeActivityMultiplayerSessionChangeEventArgs^>(
			[this] (Platform::Object^ object, RealTimeActivity::RealTimeActivityMultiplayerSessionChangeEventArgs^ eventArgs)
		{
			OnSessionChanged( object,eventArgs );
		});
		m_sessionChangeToken = m_xblContext->RealTimeActivityService->MultiplayerSessionChanged += multiplayerSessionChanged;

		EventHandler<RealTimeActivity::RealTimeActivityMultiplayerSubscriptionsLostEventArgs^>^ multiplayerSubscriptionLost = ref new EventHandler<RealTimeActivity::RealTimeActivityMultiplayerSubscriptionsLostEventArgs^>(
			[this] (Platform::Object^ object, RealTimeActivity::RealTimeActivityMultiplayerSubscriptionsLostEventArgs^ eventArgs)
		{
			OnSubscriptionLost( object,eventArgs);
		});
		m_subscriptionLostToken = m_xblContext->RealTimeActivityService->MultiplayerSubscriptionsLost += multiplayerSubscriptionLost;

		m_xblContext->RealTimeActivityService->EnableMultiplayerSubscriptions();
	}
}

///////////////////////////////////////////////////////////////////////////////
//  StopListeningToMultiplayerEvents
//      - It'll deregister the token from the OnSessionStateChanged event
//      - It'll zero out the token value
//
void XboxOneMabSessionManager::StopListeningToMultiplayerEvents()
{
	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::StopListeningToMultiplayerEvents()\n");

	//assert(m_xblContext != nullptr);

	if (m_xblContext == nullptr)
	{
		return;
	}

	if (m_xblContext->RealTimeActivityService->MultiplayerSubscriptionsEnabled)
	{
		m_xblContext->RealTimeActivityService->MultiplayerSessionChanged -= m_sessionChangeToken;
		m_xblContext->RealTimeActivityService->MultiplayerSubscriptionsLost -= m_subscriptionLostToken;

		m_xblContext->RealTimeActivityService->DisableMultiplayerSubscriptions();
	}
}


//   ProcessSessionChange
//
//   Performs a check to see which session is changed
//   based on the session name
//
void XboxOneMabSessionManager::ProcessSessionChange(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session)
{
	MultiplayerSession^ currentSession = session;
	MultiplayerSession^ previousSession = nullptr;
	unsigned long long lastProcessedChange = 0;

	{
		critical_section::scoped_lock lock(m_processLock);

		XboxOneMabUtils::LogCommentFormat(L"ProcessSessionChange( %lu )\n", session->ChangeNumber);

		XboxLiveContext^ xblContext = nullptr;
		MultiplayerSession^ lobbySession = nullptr;
		MultiplayerSession^ gameSession = nullptr;

		{
			critical_section::scoped_lock lock(m_stateLock);

			xblContext = m_xblContext;
			lobbySession = m_lobbySession;
			gameSession = m_gameSession;
		}

		if (xblContext == nullptr)
		{
			return;
		}

		// See which session was changed
		if (lobbySession != nullptr &&
			XboxOneMabUtils::IsStringEqualCaseInsenstive(
			session->SessionReference->SessionName,
			lobbySession->SessionReference->SessionName))
		{
			XboxOneMabUtils::LogComment(L"LobbySession changed.\n");
			critical_section::scoped_lock lock(m_stateLock);

			previousSession = lobbySession;
			lastProcessedChange = m_lastProcessedLobbyChange;

			if (m_lobbySession == nullptr)
			{
				// The matched session is no longer valid so abort processing
				XboxOneMabUtils::LogComment(L"LobbySession is no longer valid.\n");
				return;
			}

			m_lobbySession = currentSession;
			m_lastProcessedLobbyChange = currentSession->ChangeNumber;
		}
		else if (gameSession != nullptr &&
			XboxOneMabUtils::IsStringEqualCaseInsenstive(
			session->SessionReference->SessionName,
			gameSession->SessionReference->SessionName))
		{
			XboxOneMabUtils::LogComment(L"GameSession changed.\n");
			critical_section::scoped_lock lock(m_stateLock);

			previousSession = gameSession;
			lastProcessedChange = m_lastProcessedGameChange;

			if (m_gameSession == nullptr)
			{
				// The matched session is no longer valid so abort processing
				XboxOneMabUtils::LogComment(L"GameSession is no longer valid.\n");
				return;
			}

			m_gameSession = currentSession;
			m_lastProcessedGameChange = currentSession->ChangeNumber;
		}
		else
		{
			XboxOneMabUtils::LogComment(L"Unable to match session changed event with an existing session.\n");
		}

		if (currentSession == nullptr || previousSession == nullptr)
		{
			XboxOneMabUtils::LogComment(L"An expected session is null and has probably been left.\n");
			//Todo: I'm leaving the game, do some clean up tasks here
			return;
		}
	}

	create_async([this, currentSession, previousSession, lastProcessedChange] ()
	{
		ProcessSessionDeltas(currentSession, previousSession, lastProcessedChange);
	});
}

//  ProcessSessionDeltas
//
//  Performs a diff between two sessions and preforms actions based on the
//  changed values.
//
void XboxOneMabSessionManager::ProcessSessionDeltas(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ currentSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ previousSession, unsigned long long lastChange)
{
	// Make sure we haven't seen this change before

	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::ProcessSessionDeltas()\n");

	if (currentSession->ChangeNumber < lastChange)
	{
		XboxOneMabUtils::LogCommentFormat(
			L"Change %lu has already been processed for session %ws, most recent: %lu\n",
			currentSession->ChangeNumber,
			currentSession->SessionReference->SessionName->Data(),
			lastChange
			);

		return;
	}

	// Perform a session diff
	auto diff = MultiplayerSession::CompareMultiplayerSessions(
		currentSession,
		previousSession
		);

	XboxOneMabUtils::LogCommentFormat(
		L"Diffing sessions { id: %ws, num: %d } vs. { id: %ws, num: %d } Diff Code : %d\n",
		previousSession->SessionReference->SessionName->Data(),
		previousSession->ChangeNumber,
		currentSession->SessionReference->SessionName->Data(),
		currentSession->ChangeNumber,
		(int)diff
		);

	if (IsMultiplayerSessionChangeTypeSet(diff, MultiplayerSessionChangeTypes::InitializationStateChange))
	{
		XboxOneMabUtils::LogComment(L"ProcessSessionDeltas: InitializationStateChange\n");
		auto eventArgs = HandleInitilizationStateChanged(currentSession);

		if (eventArgs != nullptr && OnMatchmakingChanged)
		{
			OnMatchmakingChanged(eventArgs);
		}
	}

	if (IsMultiplayerSessionChangeTypeSet(diff, MultiplayerSessionChangeTypes::MatchmakingStatusChange))
	{
		XboxOneMabUtils::LogComment(L"ProcessSessionDeltas: MatchmakingStatusChange\n");
		auto eventArgs = HandleMatchmakingStatusChanged(currentSession);

		if (eventArgs != nullptr && OnMatchmakingChanged)
		{
			OnMatchmakingChanged(eventArgs);
		}
	}

	if (IsMultiplayerSessionChangeTypeSet(diff, MultiplayerSessionChangeTypes::MemberListChange))
	{
		XboxOneMabUtils::LogCommentFormat(
			L"ProcessSessionDeltas: MemberListChanged from: %d, %d\n",
			previousSession->Members->Size,
			currentSession->Members->Size
			);
		if(OnMultiplayerStateChanged){
			if(previousSession->Members->Size == 2 && currentSession->Members->Size == 1)
			{
				OnMultiplayerStateChanged(currentSession, SessionState::MemberLeft);
				OnMultiplayerStateChanged(currentSession, SessionState::MembersChanged);
			}else{
				OnMultiplayerStateChanged(currentSession, SessionState::MembersChanged);
			}
		}
	}

	if (IsMultiplayerSessionChangeTypeSet(diff, MultiplayerSessionChangeTypes::SessionJoinabilityChange))
	{
		XboxOneMabUtils::LogCommentFormat(
			L"ProcessSessionDeltas: SessionJoinabilityChange from %d, %d\n",
			previousSession->SessionProperties->JoinRestriction,
			currentSession->SessionProperties->JoinRestriction
			);
		if(OnMultiplayerStateChanged)
			OnMultiplayerStateChanged(currentSession, SessionState::JoinabilityChanged);
	}

	if(IsMultiplayerSessionChangeTypeSet(diff, MultiplayerSessionChangeTypes::HostDeviceTokenChange))
	{
		XboxOneMabUtils::LogComment(L"ProcessSessionDeltas: HostDeviceTokenChange\n");
		if(OnMultiplayerStateChanged)
			OnMultiplayerStateChanged(currentSession, SessionState::HostTokenChanged);
	}
}

MatchmakingArgs^ XboxOneMabSessionManager::HandleInitilizationStateChanged(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ newSession)
{
	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::HandleInitilizationStateChanged()\n");
	if (m_matchmakingState != MatchmakingState::Found)
	{
		return nullptr;
	}

	MatchmakingArgs^ retVal = nullptr;

	XboxOneMabUtils::LogCommentFormat(
		L"Session->InitializationStage = %d, Session->InitializingEpisode = %d\n",
		newSession->InitializationStage,
		newSession->InitializingEpisode
		);

	// Check if we need to run QoS
	if (newSession->InitializingEpisode == 1)
	{
		// Determine which step we're on
		switch (newSession->InitializationStage)
		{
		case MultiplayerInitializationStage::None:
			XboxOneMabUtils::LogComment(L"InitializationStage::None\n");
			break;

		case MultiplayerInitializationStage::Unknown:
			XboxOneMabUtils::LogComment(L"InitializationStage::Unknown\n");
			break;

		case MultiplayerInitializationStage::Joining:
			XboxOneMabUtils::LogComment(L"InitializationStage::Joining\n");
			break;

		case MultiplayerInitializationStage::Measuring:
			XboxOneMabUtils::LogComment(L"InitializationStage::Measuring\n" );
			BeginQoSMeasureAndUpload(newSession);
			break;

		case MultiplayerInitializationStage::Evaluating:
			XboxOneMabUtils::LogComment(L"InitializationStage::Evaluation\n");
			// The sample template specifies automatic evaluation so we do nothing here
			break;

		case MultiplayerInitializationStage::Failed:
			XboxOneMabUtils::LogComment(L"InitializationStage::Failed\n");
			ResubmitTicketSession();
			break;

		default:
			XboxOneMabUtils::LogCommentFormat(L"Unhandled stage: %d\n", static_cast<int>(newSession->InitializationStage));
		}
	}
	else
	{
		if (newSession->CurrentUser->InitializationFailureCause != MultiplayerMeasurementFailure::None)
		{
			XboxOneMabUtils::LogComment(L"QoS failed.  Resubmitting ticket.\n");
			ResubmitTicketSession();
		}
		else
		{
			XboxOneMabUtils::LogComment(L"QoS succeeded.\n" );
			critical_section::scoped_lock lock(m_stateLock);

			m_matchmakingState = MatchmakingState::Completed;
			m_state = SessionManagerState::Running;
			retVal = ref new MatchmakingArgs(MatchmakingResult::MatchFound);
		}
	}

	return retVal;
}

void XboxOneMabSessionManager::LeaveMultiplayer(bool leaveCleanly)
{
	XboxOneMabUtils::LogCommentFormat(L"XboxOneMabSessionManager::LeaveMultiplayer : %i",leaveCleanly);
	if (leaveCleanly == true)
	{
		// Try to leave cleanly
		try{
//			StopListeningToMultiplayerEvents();
			LeaveGameSession();
			LeaveLobbySession();
		}catch(Platform::COMException^ ex)
		{
			m_lobbySession = nullptr;
			m_gameSession = nullptr;
			m_state = SessionManagerState::NotStarted;
			m_sessionID = GUID();
			m_lastProcessedGameChange = 0;
			m_lastProcessedLobbyChange = 0;
			XboxOneMabUtils::LogComment(L"Met error when leaving multiplayer");
		}
	}
	else
	{
		critical_section::scoped_lock lock(m_stateLock);

		// Just toss everything away, usually because we lost the network
		m_lobbySession = nullptr;
		m_gameSession = nullptr;
		m_sessionID = GUID();
		m_lastProcessedGameChange = 0;
		m_lastProcessedLobbyChange = 0;
		m_state = SessionManagerState::NotStarted;
	}
}

void XboxOneMabSessionManager::LeaveGameSession()
{

	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::LeaveGameSession() Start to leave Game Session");

	MultiplayerSession^ gameSession = nullptr;
	XboxLiveContext^ xblContext = nullptr;

	{
		critical_section::scoped_lock lock(m_stateLock);

		xblContext = m_xblContext;
		gameSession = m_gameSession;
		m_gameSession = nullptr;
		m_sessionID = GUID();
		m_lastProcessedGameChange = 0;
	}

	if (gameSession && xblContext)
	{
		m_bLeavingSession = true;

		// Leave the session and update MPSD

		gameSession->Leave();

		// Ignore the results since we've left as far as we're concerned
		Microsoft::Xbox::Services::Multiplayer::WriteSessionResult^ result = WriteSession(
			xblContext,
			gameSession,
			MultiplayerSessionWriteMode::UpdateExisting
			);

		if (result == nullptr || result->Succeeded == false)
		{
			gameSession = nullptr;
		}
		XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::LeaveGameSession() success leave Game Session");
	}
}

void XboxOneMabSessionManager::LeaveLobbySession()
{
	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::LeaveLobbySession() Start to leave Lobby Session");
	MultiplayerSession^ lobbySession = nullptr;
	XboxLiveContext^ xblContext = nullptr;

	{
		critical_section::scoped_lock lock(m_stateLock);

		xblContext = m_xblContext;
		lobbySession = m_lobbySession;
		m_lobbySession = nullptr;
		m_lastProcessedLobbyChange = 0;
	}

	if (lobbySession && xblContext)
	{
		m_bLeavingSession = true;

		// Leave the session and update MPSD
		lobbySession->Leave();

		// Ignore the results since we've left as far as we're concerned
		Microsoft::Xbox::Services::Multiplayer::WriteSessionResult^ result = WriteSession(
			xblContext,
			lobbySession,
			MultiplayerSessionWriteMode::UpdateExisting
			);

		if (result == nullptr || result->Succeeded == false)
		{
			lobbySession = nullptr;
		}

		XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::LeaveLobbySession() success leave Lobby Session");
	}
}

Microsoft::Xbox::Services::Multiplayer::WriteSessionResult^ XboxOneMabSessionManager::WriteSession(Microsoft::Xbox::Services::XboxLiveContext^ context, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode writeMode)
{

	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::WriteSession()\n");
	WriteSessionResult^ result = nullptr;

	if(context == nullptr)
		return nullptr;

	try
	{
		create_task(
			context->MultiplayerService->TryWriteSessionAsync(
			session,
			writeMode
			)
			).then([&result](WriteSessionResult^ response)
		{

			try
			{
				result = response;
			}
			catch(Platform::COMException^ ex)
			{
				XboxOneMabUtils::LogCommentFormat(L"Exception happened to write the session:%ws ",  XboxOneMabUtils::GetErrorStringForException(ex)->Data());
			}
			catch( Platform::Exception^ cex)
			{
				XboxOneMabUtils::LogCommentFormat(L"Exception happened to write the session:%ws ",  XboxOneMabUtils::GetErrorStringForException(cex)->Data());
			}

		}
		).wait();
	}
	catch(Platform::Exception^ ex)
	{
		XboxOneMabUtils::LogCommentFormat(L"Exception happened to create write session async call:%ws ",  XboxOneMabUtils::GetErrorStringForException(ex)->Data());
	}

	return result;
}

Microsoft::Xbox::Services::Multiplayer::WriteSessionResult^ XboxOneMabSessionManager::UpdateAndProcessSession(Microsoft::Xbox::Services::XboxLiveContext^ context, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode writeMode)
{

	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::UpdateAndProcessSession()\n");

	auto result = WriteSession(
		context,
		session,
		writeMode
		);

	if(result == nullptr){
		return result;
	}

	if (result->Succeeded)
	{
		ProcessSessionChange(result->Session);
	}

	return result;
}



Windows::Foundation::IAsyncAction^ XboxOneMabSessionManager::JoinMultiplayerAsync(MultiplayerActivationInfo^ activationInfo)
{
	MABASSERT(activationInfo != nullptr);

	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::JoinMultiplayerAsync()\n");

	return create_async([this, activationInfo]()
	{
		XboxLiveContext^ xblContext = m_xblContext;

		MABASSERT(xblContext != nullptr);

		MultiplayerSession^ session = nullptr;
		Platform::Exception^ exception = nullptr;

		// Retrieve the lobby session using the activity handle
		create_task(
			xblContext->MultiplayerService->GetCurrentSessionByHandleAsync(
			activationInfo->HandleId
			)
			).then([&session, &exception] (task<MultiplayerSession^> t)
		{
			try
			{
				session = t.get();
				//Try to get game type from the session
				Platform::String^ sessionCustomProperty =
					session->SessionProperties->SessionCustomPropertiesJson;


				if(sessionCustomProperty == nullptr)
					return;

				Json::JsonObject^ jsonObj = ref new Json::JsonObject();

#ifdef _DEBUG
				IMapView<Platform::String^ , IJsonValue^ >^ jsonmap = jsonObj->GetView();
				 
				std::for_each( begin(jsonmap), end(jsonmap),[]( IKeyValuePair<Platform::String^, IJsonValue^>^entity )
				{
					MABLOGDEBUG("key is: %ws, value is: %ws",entity->Key->Data(),entity->Value->GetString()->Data());
				});
#endif

				if(Json::JsonObject::TryParse(sessionCustomProperty, &jsonObj) == false)
					return;

				if(!jsonObj->HasKey(L"ONLINEMODE"))
					throw ref new Platform::COMException(E_ACCESSDENIED,L"GAME MODE IS WRONG");
				Platform::String^ ruOnlineMode = jsonObj->GetNamedString("ONLINEMODE");
				if(!XboxOneMabUtils::IsStringEqualCaseInsenstive(ruOnlineMode,ONLINE_PRIVATE_MATCH))
				{
					throw ref new Platform::COMException(E_ACCESSDENIED,L"GAME MODE IS WRONG");
				}


				if(!jsonObj->HasKey(L"RUGAMEMODE"))
					return;

				Platform::String^ ruGameMode =
					jsonObj->GetNamedString(L"RUGAMEMODE");
				MABASSERT(ruGameMode != nullptr);
				MABLOGDEBUG("RU GameMode is: %ws",ruGameMode->Data());				

				XboxOneMabUtils::LogComment(L"Set game mode from accepting an invitation\n");

				if(XboxOneMabUtils::IsStringEqualCaseInsenstive(ruGameMode,RUGBY_SEVEN_TYPE)){
					XboxOneStateManager->SelectSessionMode(1);
				}else if(XboxOneMabUtils::IsStringEqualCaseInsenstive(ruGameMode,RUGBY_FIFTEEN_TYPE)){
					XboxOneStateManager->SelectSessionMode(0);
				}

			}
			catch(Platform::COMException^ ex)
			{
				exception = ex;
			}
		}
		).wait();

		// Failure to get the session is fatal
		if (exception != nullptr)
		{
			LeaveMultiplayer(false);
			throw ref new Platform::FailureException( XboxOneMabUtils::GetErrorStringForException(exception) );
			return;
		}

		// Check if the session is full
		// NB: Session is lobby
		MABLOGDEBUG("session size : %d, MaxMembersInSession : %d ", session->Members->Size, session->SessionConstants->MaxMembersInSession);

		if (session->Members->Size >= MAX_GAME_PLAYERS)
		{
			XboxOneMabUtils::LogComment(L"Session is full.\n");
			LeaveMultiplayer(false);
			//Send back a message to pop up UI message
			XboxOneMessageController->Notify( XboxOneMabUISessionControllerMessage( XboxOneMabUISessionController::SESSION_EVENT_GAME_FULL, true, true) );
			throw ref new Platform::FailureException(L"Session is full");
		}

		// Join the session and mark ourselves as active
		session->Join(nullptr);
		session->SetCurrentUserStatus(MultiplayerSessionMemberStatus::Active);
		session->SetCurrentUserSecureDeviceAddressBase64(SecureDeviceAddress::GetLocal()->GetBase64String());

		// Set RTA event types to be notified of
		session->SetSessionChangeSubscription(
			MultiplayerSessionChangeTypes::SessionJoinabilityChange |
			MultiplayerSessionChangeTypes::MemberListChange |
			MultiplayerSessionChangeTypes::MatchmakingStatusChange |
			MultiplayerSessionChangeTypes::InitializationStateChange |
			MultiplayerSessionChangeTypes::HostDeviceTokenChange
			);

		{
			critical_section::scoped_lock lock(m_stateLock);
			m_lobbySession = session;
		}

		// Submit it to MPSD
		auto result = UpdateAndProcessSession(
			xblContext,
			session,
			MultiplayerSessionWriteMode::UpdateExisting
			);

		// Failure to join the lobby session is fatal
		if (result == nullptr || result->Succeeded == false)
		{
			throw ref new Platform::FailureException(result->Details);
		}

		// Publish the lobby session as the user's activity
		xblContext->MultiplayerService->SetActivityAsync(result->Session->SessionReference);

		// Notify listeners that state has been established
		if(OnMultiplayerStateChanged)
			OnMultiplayerStateChanged(result->Session, SessionState::None);
	});
}

Windows::Foundation::IAsyncAction^ XboxOneMabSessionManager::JoinGameSessionAsync(Platform::String^ gameSessionUri)
{
	return create_async([this, gameSessionUri]()
	{
		XboxLiveContext^ xblContext = m_xblContext;

		MABASSERT(xblContext != nullptr);

		MultiplayerSession^ session = nullptr;
		Platform::Exception^ exception = nullptr;

		// Retrieve the target session using the activity handle
		create_task(
			xblContext->MultiplayerService->GetCurrentSessionAsync(
				Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference::ParseFromUriPath(
					gameSessionUri
					)
				)
			).then([&session, &exception] (task<MultiplayerSession^> t)
		{
			try
			{
				session = t.get();
			}
			catch(Platform::Exception^ ex)
			{
				exception = ex;
			}
		}
		).wait();

		// Failure to get the session is fatal
		if (exception != nullptr)
		{
			XboxOneMabUtils::LogCommentFormat(L"Failed to get session by handle: %ws\n", XboxOneMabUtils::GetErrorStringForException(exception)->Data());
			throw ref new Platform::FailureException(XboxOneMabUtils::GetErrorStringForException(exception));
		}

		// Check if the session is full
		if (session->Members->Size >= session->SessionConstants->MaxMembersInSession)
		{
			XboxOneMabUtils::LogComment(L"Session is full.\n");
			throw ref new Platform::FailureException("Session is full");
		}

		// Join the session and mark ourselves as active
		session->Join(nullptr);
		session->SetCurrentUserStatus(MultiplayerSessionMemberStatus::Active);
		session->SetCurrentUserSecureDeviceAddressBase64(SecureDeviceAddress::GetLocal()->GetBase64String());

		// Set RTA event types to be notified of
		session->SetSessionChangeSubscription(
			MultiplayerSessionChangeTypes::SessionJoinabilityChange |
			MultiplayerSessionChangeTypes::MemberListChange |
			MultiplayerSessionChangeTypes::MatchmakingStatusChange |
			MultiplayerSessionChangeTypes::InitializationStateChange |
			MultiplayerSessionChangeTypes::HostDeviceTokenChange
			);

		{
			critical_section::scoped_lock lock(m_stateLock);
			m_gameSession = session;
		}

		// Submit it to MPSD
		auto result = UpdateAndProcessSession(
			xblContext,
			session,
			MultiplayerSessionWriteMode::UpdateExisting
			);

		// Failure to join the session is fatal
		if (result->Succeeded == false)
		{
			throw ref new Platform::FailureException(result->Details);
		}

		// Notify listeners that state has been established
		if(OnMultiplayerStateChanged)
			OnMultiplayerStateChanged(result->Session, SessionState::None);
	});
}



///////////////////////////////////////////////////////////////////////////////
//  CreateLobbySessionAsync()
//
//    - Creates and joins a new lobby session
//    - Sets the host device token
//    - Sets the user activity

Windows::Foundation::IAsyncAction^ XboxOneMabSessionManager::CreateLobbySessionAsync()
{
	// It is invalid to call this with a lobby already created
	MABASSERT(m_lobbySession == nullptr);

	return create_async([this]()
	{
		Platform::String^ lobbyTemplate = nullptr;
		XboxLiveContext^ xblContext = nullptr;

		{
			critical_section::scoped_lock lock(m_stateLock);

			lobbyTemplate = m_lobbySessionTemplate;
			xblContext = m_xblContext;
		}

		MABASSERT(xblContext != nullptr);
		MABASSERT(lobbyTemplate != nullptr);

		try
		{
			auto result = CreateLobbySession();

			// Failure to create the lobby session is fatal
			if (result->Succeeded == false)
			{
				throw ref new Platform::FailureException(result->Details);
			}

			// Creating a new lobby sets the current user as the host
			create_task(
				TryBecomeLobbyHostAsync()
				).wait();

			// Publish the lobby session as the user's activity
			xblContext->MultiplayerService->SetActivityAsync(result->Session->SessionReference);

			// Notify listeners that state has been established
			if(OnMultiplayerStateChanged)
				OnMultiplayerStateChanged(result->Session, SessionState::None);
		}
		catch(Platform::Exception^ ex)
		{
			XboxOneMabUtils::LogCommentFormat(L"CeateLobbySessionAsync flow failed: %ws\n", XboxOneMabUtils::GetErrorStringForException(ex)->Data());
		}
	});
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////
//    - Move the current lobby users into the game session.  If no game sessions exists
Windows::Foundation::IAsyncAction^ XboxOneMabSessionManager::CreateGameSessionFromLobbyAsync()
{
	MultiplayerSession^ lobbySession = nullptr;

	{
		critical_section::scoped_lock lock(m_stateLock);
		lobbySession = m_lobbySession;
	}

	MABASSERT(lobbySession != nullptr);

	auto xuids = ref new Vector<Platform::String^>();

	for (auto member : lobbySession->Members)
	{
		// The local user is implicitly added so we don't need to reserve space for him
		if (!member->IsCurrentUser)
		{
			xuids->Append(member->XboxUserId);
		}
	}

	return CreateGameSessionForUsersAsync(xuids->GetView());
}




//////////////////////////////////////////////////////////////////////////////////////////////////////////
//      Creates a new MultiplayerSession to be used for the lobby
Microsoft::Xbox::Services::Multiplayer::WriteSessionResult^ XboxOneMabSessionManager::CreateLobbySession()
{
	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::CreateLobbySession()\n");
	Platform::String^ lobbyTemplate = nullptr;
	XboxLiveContext^ xblContext = nullptr;

	{
		critical_section::scoped_lock lock(m_stateLock);

		lobbyTemplate = m_lobbySessionTemplate;
		xblContext = m_xblContext;
	}

	MABASSERT(xblContext != nullptr);
	MABASSERT(lobbyTemplate != nullptr);

	auto session = CreateNewDefaultSession(lobbyTemplate);

	{
		critical_section::scoped_lock lock(m_stateLock);
		m_lobbySession = session;
	}

	// Submit it to MPSD
	return UpdateAndProcessSession(
		xblContext,
		session,
		MultiplayerSessionWriteMode::CreateNew
		);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
//      Creates a new MultiplayerSession to be used for the game
Microsoft::Xbox::Services::Multiplayer::WriteSessionResult^ XboxOneMabSessionManager::CreateGameSession(Windows::Foundation::Collections::IVectorView<Platform::String^>^ xuids)
{
	Platform::String^ gameTemplate = nullptr;
	XboxLiveContext^ xblContext = nullptr;

	{
		critical_section::scoped_lock lock(m_stateLock);

		gameTemplate = m_gameSessionTemplate;
		xblContext = m_xblContext;
	}

	MABASSERT(xblContext != nullptr);
	MABASSERT(gameTemplate != nullptr);

	auto session = CreateNewDefaultSession(gameTemplate);

	// Add reservations for the accepted lobby members
	for (auto xuid : xuids)
	{
		session->AddMemberReservation(xuid, nullptr);
	}

	{
		critical_section::scoped_lock lock(m_stateLock);
		m_gameSession = session;
	}

	// Submit it to MPSD
	return UpdateAndProcessSession(
		xblContext,
		session,
		MultiplayerSessionWriteMode::CreateNew
		);
}



Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ XboxOneMabSessionManager::CreateNewDefaultSession(Platform::String^ templateName)
{
	XboxLiveContext^ xblContext = nullptr;

	{
		critical_section::scoped_lock lock(m_stateLock);
		xblContext = m_xblContext;
	}

	MABASSERT(xblContext != nullptr);

	auto sessionName = GenerateUniqueName();

	// Create new Session Reference
	auto sessionRef  = ref new Multiplayer::MultiplayerSessionReference(
		m_serviceConfigId, // Service Config ID of title from XDP
		templateName,      // MPSD Template Name
		sessionName        // Generated session name
		);

	// Create a new Multiplayer Session object
	auto session = ref new MultiplayerSession(
		xblContext,                         // XBL Context
		sessionRef,                         // Session reference
		0,                                  // Max members; 0 = template value
		false,                              // Not supported, always false
		MultiplayerSessionVisibility::Open, // Allow others to join
		nullptr,                            // Initial users
		nullptr                             // Custom constants
		);

	// Join the session and mark ourselves as active
	session->Join(nullptr, true);
	session->SetCurrentUserStatus(MultiplayerSessionMemberStatus::Active);
	session->SetCurrentUserSecureDeviceAddressBase64(SecureDeviceAddress::GetLocal()->GetBase64String());

	// Set RTA event types to be notified of
	session->SetSessionChangeSubscription(
		MultiplayerSessionChangeTypes::SessionJoinabilityChange |
		MultiplayerSessionChangeTypes::MemberListChange |
		MultiplayerSessionChangeTypes::MatchmakingStatusChange |
		MultiplayerSessionChangeTypes::InitializationStateChange |
		MultiplayerSessionChangeTypes::HostDeviceTokenChange
		);

	return session;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//    - Move the specified users into the game session.  If no game sessions exists
Windows::Foundation::IAsyncAction^ XboxOneMabSessionManager::CreateGameSessionForUsersAsync(Windows::Foundation::Collections::IVectorView<Platform::String^>^ xuids)
{
	return create_async([this, xuids]()
	{
		auto gameSession = CreateGameSession(xuids);

		if (gameSession == nullptr || gameSession->Succeeded == false)
		{
			throw ref new Platform::FailureException("Fatal Error to create teh sesion to join game");
			//Generate possible message to UI
		}

		// Creating a new game session sets the current user as the host
		create_task(
			TryBecomeGameHostAsync()
			).wait();
	});
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
//      Update the lobby session's HostDeviceToken to that of the current user.
Windows::Foundation::IAsyncOperation<bool>^ XboxOneMabSessionManager::TryBecomeLobbyHostAsync()
{
	return create_async([this]() -> bool
	{
		critical_section::scoped_lock taskLock(m_taskLock);

		MultiplayerSession^ lobbySession = nullptr;
		XboxLiveContext^ context = nullptr;

		{
			critical_section::scoped_lock lock(m_stateLock);

			lobbySession = m_lobbySession;
			context = m_xblContext;
		}

		MABASSERT(context != nullptr);

		// Try to update the lobby session
		if (lobbySession != nullptr)
		{
			lobbySession->SetHostDeviceToken(lobbySession->CurrentUser->DeviceToken);

			auto result = UpdateAndProcessSession(
				context,
				lobbySession,
				MultiplayerSessionWriteMode::UpdateExisting
				);

			return result->Succeeded;
		}

		return false;
	});
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////
//      Update the game session's HostDeviceToken to that of the current user.
Windows::Foundation::IAsyncOperation<bool>^ XboxOneMabSessionManager::TryBecomeGameHostAsync()
{
	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::TryBecomeGameHostAsync()\n");
	return create_async([this]() -> bool
	{
		critical_section::scoped_lock taskLock(m_taskLock);

		MultiplayerSession^ gameSession = nullptr;
		XboxLiveContext^ context = nullptr;

		{
			critical_section::scoped_lock lock(m_stateLock);

			gameSession = m_gameSession;
			context = m_xblContext;
		}

		MABASSERT(context != nullptr);

		// Try to update the game session
		if (gameSession != nullptr)
		{
			gameSession->SetHostDeviceToken(gameSession->CurrentUser->DeviceToken);

			auto result = UpdateAndProcessSession(
				context,
				gameSession,
				MultiplayerSessionWriteMode::UpdateExisting
				);

			return result->Succeeded;
		}

		return false;
	});
}

void XboxOneMabSessionManager::BeginQoSMeasureAndUpload(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ newSession)
{
	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::BeginQoSMeasureAndUpload()\n");

	// Do the QoS measurements and then attach the results to the session
	// and submit it back to the service for evaluation.
	create_task(
		XboxOneMabUtils::MeasureQosForSessionAsync(
		newSession
		)
	).then([this, newSession](IVectorView<MultiplayerQualityOfServiceMeasurements^>^ measurements)
	{
		XboxLiveContext^ xblContext = nullptr;

		{
			critical_section::scoped_lock lock(m_stateLock);
			xblContext = m_xblContext;
		}

		MABASSERT(xblContext != nullptr);

		// Figure out which session needs the numbers and send them.
		// Failures in the update are ignored here because they will trigger a qos failure down the line.
		if (IsLobbySession(newSession))
		{
			MultiplayerSession^ lobbySession = nullptr;

			// Get the current lobby session
			{
				critical_section::scoped_lock lock(m_stateLock);
				lobbySession = m_lobbySession;
			}

			MABASSERT(lobbySession != nullptr);

			// Attach the measurements
			lobbySession->SetCurrentUserQualityOfServiceMeasurements(measurements);

			// Update the session
			UpdateAndProcessSession(
				xblContext,
				lobbySession,
				MultiplayerSessionWriteMode::UpdateExisting
				);
		}
		else if (IsGameSession(newSession))
		{
			MultiplayerSession^ gameSession = nullptr;

			// Get the current game session
			{
				critical_section::scoped_lock lock(m_stateLock);
				gameSession = m_gameSession;
			}

			MABASSERT(gameSession != nullptr);

			// Attach the measurements
			gameSession->SetCurrentUserQualityOfServiceMeasurements(measurements);

			// Update the session
			UpdateAndProcessSession(
				xblContext,
				gameSession,
				MultiplayerSessionWriteMode::UpdateExisting
				);
		}
	});
}



///////////////////////////////////////////////////////////////////////////////
//  HandleMatchmakingStateChanged
//       Watches for matchmaking state changed events
//           - Notifies listeners in case of failure so they can retry
//           - If the matchmaking is found, call the function to join

XboxOneMabLib::MatchmakingArgs^ XboxOneMabSessionManager::HandleMatchmakingStatusChanged(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ newSession)
{
	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::HandleMatchmakingStateChanged()\n");

	if (newSession == nullptr)
	{
		XboxOneMabUtils::LogComment(L"Specified Session is null\n");
		return nullptr;
	}

	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ targetSessionRef = nullptr;
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ sessionRef = nullptr;

	MatchmakingArgs^ retVal = nullptr;
	MatchmakingStatus status = newSession->MatchmakingServer->Status;

	XboxOneMabUtils::LogCommentFormat(L"MatchmakingServer->Status = %d\n", static_cast<int>(status));

	switch (status)
	{
	case MatchmakingStatus::Canceled:
		XboxOneMabUtils::LogComment(L"Session cancelled\n");
		{
			critical_section::scoped_lock lock(m_stateLock);

			m_matchmakingState = MatchmakingState::Failed;
			m_state = SessionManagerState::Running;
		}

		retVal = ref new MatchmakingArgs(MatchmakingResult::MatchFailed, L"Matchmaking request cancelled");
		break;
	case MatchmakingStatus::Expired:
		XboxOneMabUtils::LogComment(L"Session expired\n");
		{
			critical_section::scoped_lock lock(m_stateLock);

			m_matchmakingState = MatchmakingState::Failed;
			m_state = SessionManagerState::Running;
		}

		retVal = ref new MatchmakingArgs(MatchmakingResult::MatchRefreshing, L"Matchmaking request expired");
		break;

	case MatchmakingStatus::Found:
		HandleMatchSessionFound(newSession);
		break;

	default:
		XboxOneMabUtils::LogCommentFormat(L"Unhandled status value: %d\n", status);
	}

	return retVal;
}

///////////////////////////////////////////////////////////////////////////////
//  HandleMatchSessionFound
//
//    - Extracts the session from the target session reference
//    - Joins the session
//
void XboxOneMabSessionManager::HandleMatchSessionFound(MultiplayerSession^ newSession)
{
	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::HandleMatchSessionFound()\n");

	create_async([this, newSession]()
	{
		XboxLiveContext^ xblContext = m_xblContext;

		auto targetSessionRef = newSession->MatchmakingServer->TargetSessionRef;
		auto sessionRef  = ref new Multiplayer::MultiplayerSessionReference(
			targetSessionRef->ServiceConfigurationId,
			targetSessionRef->SessionTemplateName,
			targetSessionRef->SessionName
			);

		XboxOneMabUtils::LogCommentFormat(
			L"XboxOneMabSessionManager::HandleMatchmakingStateChanged() Target Session Ref is %ws\n",
			sessionRef->SessionName->Data()
			);

		MultiplayerSession^ gameSession = nullptr;

		create_task(
			xblContext->MultiplayerService->GetCurrentSessionAsync(
			sessionRef
			)
			).then([this, &gameSession](MultiplayerSession^ session)
		{
			critical_section::scoped_lock lock(m_stateLock);
			gameSession = session;
			m_gameSession = gameSession;
		}
		).wait();

		gameSession->SetCurrentUserStatus(MultiplayerSessionMemberStatus::Active);
		gameSession->SetCurrentUserSecureDeviceAddressBase64(SecureDeviceAddress::GetLocal()->GetBase64String());

		gameSession->SetSessionChangeSubscription(
			MultiplayerSessionChangeTypes::SessionJoinabilityChange |
			MultiplayerSessionChangeTypes::MemberListChange |
			MultiplayerSessionChangeTypes::MatchmakingStatusChange |
			MultiplayerSessionChangeTypes::InitializationStateChange |
			MultiplayerSessionChangeTypes::HostDeviceTokenChange
			);

		{
			critical_section::scoped_lock lock(m_stateLock);

			m_matchmakingState = MatchmakingState::Found;
			m_state = SessionManagerState::Running;
		}

		auto result = UpdateAndProcessSession(
			xblContext,
			gameSession,
			MultiplayerSessionWriteMode::UpdateExisting
			);

		if (result->Succeeded == false)
		{
			critical_section::scoped_lock lock(m_stateLock);

			m_matchmakingState = MatchmakingState::Failed;
			throw ref new Platform::FailureException(result->Details);
		}
	});
}



///////////////////////////////////////////////////////
// Push the Game session uri to lobby session
// enable others to join
void XboxOneMabSessionManager::AdvertiseGameSession(MabMap<MabString,MabString>* customerParams /*= NULL*/)
{
	critical_section::scoped_lock taskLock(m_taskLock);

	XboxLiveContext^ xblContext = nullptr;
	MultiplayerSession^ lobbySession = nullptr;
	MultiplayerSession^ gameSession = nullptr;

	{
		critical_section::scoped_lock lock(m_stateLock);

		xblContext = m_xblContext;
		lobbySession = m_lobbySession;
		gameSession = m_gameSession;
	}

	MABASSERT(xblContext != nullptr);
	MABASSERT(lobbySession != nullptr);
	MABASSERT(gameSession != nullptr);

	if(gameSession == nullptr)
	{
		MABLOGDEBUG("Error: game session is NULL!");
		return;
	}


	lobbySession->SetSessionCustomPropertyJson(
		L"GameSessionUri",
		gameSession->SessionReference->ToUriPath()
		);

	//Here we need to insert game type of game session which it will become
	//Add customer params here
	if(customerParams){
		for ( MabMap< MabString, MabString >::iterator params_iter = customerParams->begin(); params_iter != customerParams->end(); params_iter++)
		{
			lobbySession->SetSessionCustomPropertyJson(
				XboxOneMabUtils::ConvertMabStringToPlatformString(&params_iter->first),
				XboxOneMabUtils::ConvertMabStringToPlatformString(&params_iter->second)
				);
		}
	}


	auto result = UpdateAndProcessSession(
		xblContext,
		lobbySession,
		MultiplayerSessionWriteMode::UpdateExisting
		);

	if (result->Succeeded == false)
	{
		throw ref new Platform::FailureException(result->Details);
	}
}

///////////////////////////////////////////////////////
// Get the game session uri to join

Platform::String^ XboxOneMabSessionManager::GetAdvertisedGameSessionUri()
{
	XboxOneMabUtils::LogComment("XboxOneMabSessionManager::GetAdvertisedGameSessionUri()");
	MultiplayerSession^ lobbySession = nullptr;

	{
		critical_section::scoped_lock lock(m_stateLock);
		lobbySession = m_lobbySession;
	}

	MABASSERT(lobbySession != nullptr);

	auto jsonBlob = lobbySession->SessionProperties->SessionCustomPropertiesJson;

	if (jsonBlob != nullptr)
	{
		auto jsonObj = ref new JsonObject();

		if (JsonObject::TryParse(jsonBlob, &jsonObj))
		{
			if (jsonObj->HasKey(L"GameSessionUri"))
			{
				return jsonObj->GetNamedString(L"GameSessionUri");
			}
		}
	}

	return nullptr;
}


bool XboxOneMabSessionManager::IsLobbySession(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session)
{
	critical_section::scoped_lock lock(m_stateLock);

	if (session != nullptr && m_lobbySession != nullptr)
	{
		return XboxOneMabUtils::IsStringEqualCaseInsenstive(
			session->SessionReference->SessionName,
			m_lobbySession->SessionReference->SessionName
			);
	}

	return false;
}

bool XboxOneMabSessionManager::IsGameSession(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session)
{
	critical_section::scoped_lock lock(m_stateLock);

	if (session != nullptr && m_gameSession != nullptr)
	{
		return XboxOneMabUtils::IsStringEqualCaseInsenstive(
			session->SessionReference->SessionName,
			m_gameSession->SessionReference->SessionName
			);
	}

	return false;
}

void XboxOneMabSessionManager::SetLobbySessionJoinRestriction(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionJoinRestriction joinRestriction)
{
	critical_section::scoped_lock taskLock(m_taskLock);
	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::SetLobbySessionJoinRestriction\n");

	MultiplayerSession^ lobbySession = nullptr;
	XboxLiveContext^ xblContext = nullptr;

	{
		critical_section::scoped_lock lock(m_stateLock);

		lobbySession = m_lobbySession;
		xblContext = m_xblContext;
	}

	if(lobbySession == nullptr || xblContext == nullptr)
		return;

	lobbySession->SessionProperties->JoinRestriction = joinRestriction;

	UpdateAndProcessSession(
		xblContext,
		lobbySession,
		MultiplayerSessionWriteMode::UpdateExisting
		);
}

/////////////////////////////////////////////////////////////////////////////////
//   Begin smartmatch call
//    - Submits the current lobby session for matchmaking
//    - A lobby session is implicitly created if one doesn't exist.
Windows::Foundation::IAsyncAction^ XboxOneMabSessionManager::BeginSmartMatchAsync()
{
	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::BeginSmartMatchAsync()\n");

	return create_async([this]()
	{
		Platform::String^ lobbyTemplate = nullptr;
		Platform::String^ hopperName = nullptr;
		XboxLiveContext^ xblContext = nullptr;
		MultiplayerSession^ lobbySession = nullptr;

		{
			critical_section::scoped_lock lock(m_stateLock);

			lobbyTemplate = m_lobbySessionTemplate;
			hopperName = m_hopperName;
			xblContext = m_xblContext;
			lobbySession = m_lobbySession;
		}

		MABASSERT(xblContext != nullptr);
		MABASSERT(lobbyTemplate != nullptr);
		MABASSERT(hopperName != nullptr);

		try
		{
			if (lobbySession == nullptr)
			{
				MABLOGDEBUG("Create new lobby session");
				// First we need to establish a lobby session
				auto result = CreateLobbySession();

				// Failure to create the lobby session is fatal
				if(result == nullptr)
				{
					throw ref new Platform::FailureException("Fatal Error For Session!");
				}

				if (result->Succeeded == false)
				{
					throw ref new Platform::FailureException(result->Details);
				}

				lobbySession = result->Session;

				// Creating a new lobby sets the current user as the host
				create_task(
					TryBecomeLobbyHostAsync()
					).wait();

				// Publish the lobby session as the user's activity
				xblContext->MultiplayerService->SetActivityAsync(result->Session->SessionReference);
			}

			TimeSpan matchTimeOut;
			matchTimeOut.Duration = 60 * TICKS_PER_SECOND;

			// Submit the lobby session to matchmaking
			create_task(
				xblContext->MatchmakingService->CreateMatchTicketAsync(
				lobbySession->SessionReference,
				lobbySession->SessionReference->ServiceConfigurationId,
				hopperName,
				matchTimeOut,
				Matchmaking::PreserveSessionMode::Never,
				nullptr
				)
				).then([this](task<CreateMatchTicketResponse^> response)
			{
				try
				{
					XboxOneMabUtils::LogComment(L"CreateMatchTicketAsync completed.\n");

					critical_section::scoped_lock lock(m_stateLock);

					// Matchmaking has begun successfully
					m_matchTicketResponse = response.get();

//					XboxOneMabUtils::LogComment( m_matchTicketResponse->MatchTicketId );
//					XboxOneMabUtils::LogComment( m_matchTicketResponse->EstimatedWaitTime.Duration.ToString() );


					m_matchmakingState = MatchmakingState::Searching;
					m_state = SessionManagerState::Matchmaking;

					return;
				}
				catch(Platform::Exception^ ex)
				{
					XboxOneMabUtils::LogCommentFormat(L"CreateMatchTicketAsync threw %ws\n", XboxOneMabUtils::GetErrorStringForException(ex)->Data());

					critical_section::scoped_lock lock(m_stateLock);

					// Failed to start matchmaking
					m_state = SessionManagerState::Running;
					m_matchmakingState = MatchmakingState::Failed;
				}
			}
			).wait();
		}
		catch(Platform::Exception^ ex)
		{
			XboxOneMabUtils::LogCommentFormat(L"CreateMatchTicketAsync threw %ws\n", XboxOneMabUtils::GetErrorStringForException(ex)->Data());

			critical_section::scoped_lock lock(m_stateLock);

			// Failed to start matchmaking
			m_matchmakingState = MatchmakingState::Failed;
			m_state = SessionManagerState::Running;
		}
	});
}

IAsyncAction^ XboxOneMabSessionManager::RefreshSessionsAsync()
{
	return create_async([this]()
	{
		critical_section::scoped_lock taskLock(m_taskLock);
		XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::RefreshSessionsAsync()\n");

		MultiplayerSession^ gameSession = nullptr;
		MultiplayerSession^ lobbySession = nullptr;
		XboxLiveContext^ xblContext = nullptr;

		{
			critical_section::scoped_lock lock(m_stateLock);

			gameSession = m_gameSession;
			lobbySession = m_lobbySession;
			xblContext = m_xblContext;
		}

		MABASSERT(xblContext != nullptr);

		std::vector<task<void>> taskList;

		if (gameSession != nullptr)
		{
			taskList.push_back(
				create_task(
				m_xblContext->MultiplayerService->GetCurrentSessionAsync(
				gameSession->SessionReference
				)
				).then([this, gameSession](MultiplayerSession^ session)
			{
				if (session != nullptr)
				{
					ProcessSessionChange(session);
				}
			})
				);
		}

		if (lobbySession != nullptr)
		{
			taskList.push_back(
				create_task(
				m_xblContext->MultiplayerService->GetCurrentSessionAsync(
				lobbySession->SessionReference
				)
				).then([this, lobbySession](MultiplayerSession^ session)
			{
				if (session != nullptr)
				{
					ProcessSessionChange(session);
				}
			})
				);
		}

		// Wait for refreshes to complete
		when_all(taskList.begin(), taskList.end()).wait();
	});
}

IAsyncOperation<HostMigrationResultArgs^>^ XboxOneMabSessionManager::HandleHostMigration(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session)
{
	XboxOneMabUtils::LogCommentFormat(L"XboxOneMabSessionManager::HandleHostMigration(%ws)\n", session->SessionReference->SessionName->Data());
	IAsyncOperation<HostMigrationResultArgs^>^ operation = nullptr;

	auto isGameSession = IsGameSession(session);
	auto isLobbySession = IsLobbySession(session);

	{
		critical_section::scoped_lock lock(m_stateLock);

		// If host migration is already running for the specified session
		// just return the stored action instead of starting another.
		if (isGameSession)
		{
			operation = m_gameMigrationOperation;
		}
		else if (isLobbySession)
		{
			operation = m_lobbyMigrationOperation;
		}
		else
		{
			XboxOneMabUtils::LogComment(L"Supplied session does not match the current lobby or game session.\n");
			return create_async([] () -> HostMigrationResultArgs^
			{
				return ref new HostMigrationResultArgs(HostMigrationResult::NoAction);
			});
		}
	}

	if (operation == nullptr)
	{
		operation = create_async([this, session, isLobbySession] () -> HostMigrationResultArgs^
		{
			critical_section::scoped_lock taskLock(m_taskLock);

			XboxLiveContext^ xblContext = nullptr;
			MultiplayerSession^ theSession = nullptr;
			HostMigrationResultArgs^ resultArgs = nullptr;

			{
				critical_section::scoped_lock lock(m_stateLock);

				xblContext = m_xblContext;

				if (isLobbySession)
				{
					theSession = m_lobbySession;
				}
				else
				{
					theSession = m_gameSession;
				}
			}

			MABASSERT(xblContext != nullptr);

			if (theSession == nullptr)
			{
				// If we're not in a current session we don't need to care about host
				XboxOneMabUtils::LogComment(L"The session is no longer valid.\n");
				resultArgs = ref new HostMigrationResultArgs(HostMigrationResult::SessionLeft);
			}
			else if (theSession->CurrentUser == nullptr)
			{
				// If there is no current user then we've already left it ourselves
				XboxOneMabUtils::LogComment(L"We are no longer in the session.\n");
				resultArgs = ref new HostMigrationResultArgs(HostMigrationResult::SessionLeft);
			}
			else 
			{
				if (theSession->Members->GetAt(0)->XboxUserId->Equals(theSession->CurrentUser->XboxUserId))
				{
					// Whoever is in the first position in the session member list is the host
					theSession->SetHostDeviceToken(theSession->CurrentUser->DeviceToken);

					auto result = UpdateAndProcessSession(
						xblContext,
						theSession,
						MultiplayerSessionWriteMode::UpdateExisting
						);

					if (result->Succeeded)
					{
						resultArgs = ref new HostMigrationResultArgs(HostMigrationResult::MigrationSuccess);
					}
					else
					{
						resultArgs = ref new HostMigrationResultArgs(HostMigrationResult::MigrateFailed);
					}
				}
				else
				{
					// We're not supposed to be the host so we can just wait for the session changed notification
					resultArgs = ref new HostMigrationResultArgs(HostMigrationResult::NoAction);
				}
			}

			// Clear out the stored operation reference
			{
				critical_section::scoped_lock lock(m_stateLock);

				if (isLobbySession)
				{
					m_lobbyMigrationOperation = nullptr;
				}
				else
				{
					m_gameMigrationOperation = nullptr;
				}
			}

			return resultArgs;
		});
	}

	// Store the operation reference in case this method is called again while the
	// operation is still in progress
	{
		critical_section::scoped_lock lock(m_stateLock);

		if (isLobbySession)
		{
			m_lobbyMigrationOperation = operation;
		}
		else
		{
			m_gameMigrationOperation = operation;
		}
	}

	return operation;
}

bool XboxOneMabSessionManager::GenerateMabXboxOneClient(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session)
{
	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::GenerateMabXboxOneClient()\n");

	MABASSERT(session != nullptr);

	if (session != nullptr)
	{
		if (XboxOneStateManager->GetMyState() == GameState::Matchmaking)
		{
			MABASSERT(IsGameSession(session));
			// Become host if we're the first in the game session
			if (session->Members->GetAt(0)->XboxUserId->Equals(session->CurrentUser->XboxUserId))
			{
				TryBecomeGameHostAsync();
			}
		}

		// Connect to everyone else in the session
		// The game session should be only 2
		for (auto member : session->Members)
		{
			if (!member->IsCurrentUser)
			{
				Platform::String^ gamertag = member->Gamertag;
				Platform::String^ peerName = member->XboxUserId;
				Platform::String^ secureDeviceAddress = member->SecureDeviceAddressBase64;
				SecureDeviceAddress^ hostAddr = SecureDeviceAddress::FromBase64String(secureDeviceAddress);
				
				IAsyncOperation<SecureDeviceAssociation^>^ asyncOp =
					 m_peerDeviceAssociationTemplate->CreateAssociationAsync(
					 hostAddr,
					 Windows::Xbox::Networking::CreateSecureDeviceAssociationBehavior::Default
					 );
				 create_task(asyncOp).then([this, peerName, gamertag]  (SecureDeviceAssociation^ association)
				 {
					 SOCKADDR_STORAGE remoteSocketAddress;
					 Platform::ArrayReference<BYTE> remoteSocketAddressBytes(
						 (BYTE*) &remoteSocketAddress,
						 sizeof( remoteSocketAddress )
						 );
					 association->GetRemoteSocketAddressBytes(
						 remoteSocketAddressBytes
						 );

					 //Here we get the remote address for the server
					 SOCKADDR_IN6* remote_address = (SOCKADDR_IN6*)&remoteSocketAddress;
					 m_remoteClient = new XboxOneMabNetOnlineClient();
					 std::string narrow_xuid(peerName->Begin(), peerName->End());
					 m_remoteClient->member_id = narrow_xuid.c_str();
					 std::string narrow_gametag(gamertag->Begin(), gamertag->End());
					  m_remoteClient->game_tag = narrow_gametag.c_str();
					 m_remoteClient->local_address.saddrV6 = *remote_address;
//					 m_remoteClient->secureDeviceAssociation = association;
					 AddAssociation(m_remoteClient->member_id,association);
					 //stub

				 }).wait();



				 //break here for get only the first one
				break;
			}
		}
	}
	else
	{
		return false;
	}

	return true;
}

bool XboxOneMabSessionManager::GetConnectedMabXboxOneClientDetails(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session, XboxOneMabNetOnlineClient* host)
{
	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::GetHostMabXboxOneClientDetails()\n");

	MABASSERT(session != nullptr);

	if (session != nullptr)
	{
		if (XboxOneStateManager->GetMyState() == GameState::Matchmaking)
		{
			MABASSERT(IsGameSession(session));
		}

		// Connect to everyone else in the session
		// The game session should be only 2
		for (auto member : session->Members)
		{
	
			if(member->IsCurrentUser) continue;		

			Platform::String^ gamertag = member->Gamertag;
			Platform::String^ peerName = member->XboxUserId;
			Platform::String^ secureDeviceAddress = member->SecureDeviceAddressBase64;
			Platform::String^ xbox_user_id = member->XboxUserId;
			SecureDeviceAddress^ hostAddr = SecureDeviceAddress::FromBase64String(secureDeviceAddress);
				
			IAsyncOperation<SecureDeviceAssociation^>^ asyncOp =
					m_peerDeviceAssociationTemplate->CreateAssociationAsync(
					hostAddr,
					Windows::Xbox::Networking::CreateSecureDeviceAssociationBehavior::Default
					);
				create_task(asyncOp).then([this, peerName, host, gamertag]  (SecureDeviceAssociation^ association)
				{
					SOCKADDR_STORAGE remoteSocketAddress;
					Platform::ArrayReference<BYTE> remoteSocketAddressBytes(
						(BYTE*) &remoteSocketAddress,
						sizeof( remoteSocketAddress )
						);
					association->GetRemoteSocketAddressBytes(
						remoteSocketAddressBytes
						);
#ifdef _DEBUG
					XboxOneMabUtils::PrintSecureDeviceAssociation(association);
#endif

					//Here we get the remote address for the server
					SOCKADDR_IN6* remote_address = (SOCKADDR_IN6*)&remoteSocketAddress;

					std::string narrow_xuid(peerName->Begin(), peerName->End());
					host->member_id = narrow_xuid.c_str();
					std::string narrow_gametag(gamertag->Begin(), gamertag->End());
					host->game_tag = narrow_gametag.c_str();

//					std::string narrow(peerName->Begin(), peerName->End());
//					strncpy(host->member_id, narrow.c_str(), sizeof(host->member_id));
//					host->member_id[sizeof(host->member_id) - 1] = 0;
					host->local_address.saddrV6 = *remote_address;
//					host->secureDeviceAssociation = association;
					AddAssociation(host->member_id,association);

				}).wait();

		}
	}
	else
	{
		return false;
	}

	return true;
}

bool XboxOneMabSessionManager::GetLocalMabXboxOneClientDetails(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session, XboxOneMabNetOnlineClient* local_client)
{
	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::GetLocalMabXboxOneClientDetails()\n");

	MABASSERT(session != nullptr);

	if (session != nullptr)
	{
		if (XboxOneStateManager->GetMyState() == GameState::Matchmaking)
		{
			MABASSERT(IsGameSession(session));
		}

		// Connect to everyone else in the session
		// The game session should be only 2
		for (auto member : session->Members)
		{

			if(member->IsCurrentUser) continue;

			Platform::String^ secureDeviceAddress = member->SecureDeviceAddressBase64;
			SecureDeviceAddress^ hostAddr = SecureDeviceAddress::FromBase64String(secureDeviceAddress);
				
			IAsyncOperation<SecureDeviceAssociation^>^ asyncOp =
					m_peerDeviceAssociationTemplate->CreateAssociationAsync(
					hostAddr,
					Windows::Xbox::Networking::CreateSecureDeviceAssociationBehavior::Default
					);
				create_task(asyncOp).then([this, local_client]  (SecureDeviceAssociation^ association)
				{
					SOCKADDR_STORAGE remoteSocketAddress;
					Platform::ArrayReference<BYTE> remoteSocketAddressBytes(
						(BYTE*) &remoteSocketAddress,
						sizeof( remoteSocketAddress )
						);
					association->GetLocalSocketAddressBytes(
						remoteSocketAddressBytes
						);
					//Here we get the remote address for the server

					SOCKADDR_IN6* remote_address = (SOCKADDR_IN6*)&remoteSocketAddress;
					local_client->local_address.saddrV6 = *remote_address;
//					local_client->secureDeviceAssociation = association;
					AddAssociation(local_client->member_id,association);
				}).wait();

		}
	}
	else
	{
		return false;
	}

	return true;
}
bool XboxOneMabSessionManager::ResubmitTicketSession()
{
	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::ResubmitTicketSession()\n");

	MultiplayerSession^ lobbySession = nullptr;
	XboxLiveContext^ xblContext = nullptr;

	{
		critical_section::scoped_lock lock(m_stateLock);

		xblContext = m_xblContext;
		lobbySession = m_lobbySession;
	}

	if (xblContext == nullptr || lobbySession == nullptr)
	{
		// multiplayer may have just been torn down
		return false;
	}

	// Best effort to leave the failed session
	LeaveGameSession();

	// Tell MPSD to try the match again
	lobbySession->SetMatchmakingResubmit(true);

	auto result = UpdateAndProcessSession(
		xblContext,
		lobbySession,
		MultiplayerSessionWriteMode::UpdateExisting
		);

	return result->Succeeded;
}

bool XboxOneMabSessionManager::IsLobbyHost()
{
	critical_section::scoped_lock lock(m_stateLock);

	if (m_lobbySession != nullptr)
	{
		return XboxOneMabUtils::IsStringEqualCaseInsenstive(
			m_lobbySession->SessionProperties->HostDeviceToken,
			m_lobbySession->CurrentUser->DeviceToken
			);
	}

	return false;
}

bool XboxOneMabSessionManager::IsGameHost()
{
	critical_section::scoped_lock lock(m_stateLock);
	//Hack to make it works!
	//We always assume the first one in the game session is the server
	//Todo: Put the code to TryBecomeGameHost functionm
	if (m_gameSession != nullptr)
	{
		if (m_gameSession->Members->GetAt(0)->XboxUserId->Equals(m_gameSession->CurrentUser->XboxUserId))
			return true;
		else
			return false;
		/*
		return XboxOneMabUtils::IsStringEqualCaseInsenstive(
			m_gameSession->SessionProperties->HostDeviceToken,
			m_gameSession->CurrentUser->DeviceToken
			);
			*/
	}

	return false;
}

IAsyncAction^ XboxOneMabSessionManager::CancelMatchmakingAsync()
{
	XboxOneMabUtils::LogComment(L"XboxOneMabSessionManager::CancelMatchmaking()\n");

	return create_async([this] ()
	{
		XboxLiveContext^ xblContext = nullptr;
		CreateMatchTicketResponse^ matchResponse = nullptr;
		Platform::String^ hopperName = nullptr;
		SessionManagerState state = SessionManagerState::NotStarted;

		{
			critical_section::scoped_lock lock(m_stateLock);

			xblContext = m_xblContext;
			matchResponse = m_matchTicketResponse;
			hopperName = m_hopperName;
			state = m_state;

			if (matchResponse == nullptr)
			{
				// this method may have been called very early, so just return state to Running, if we were Matchmaking, and return early
				if (state == SessionManagerState::Matchmaking)
				{
					m_state = SessionManagerState::Running;
				}
				return;
			}
		}

		if (xblContext == nullptr)
		{
			// context may have just been cleared due to some interruption, so return early
			return;
		}

		MABASSERT(hopperName != nullptr);

		if (state == SessionManagerState::Matchmaking)
		{
			create_task(
				xblContext->MatchmakingService->DeleteMatchTicketAsync( 
				m_serviceConfigId,
				hopperName,
				matchResponse->MatchTicketId
				)
				).wait();

			{
				critical_section::scoped_lock lock(m_stateLock);

				m_matchTicketResponse = nullptr;
				m_state = SessionManagerState::Running;
			}
		}
	});
}

Windows::Xbox::Networking::SecureDeviceAssociation^ XboxOneMabSessionManager::GetAssociation(MabString xboxid)
{
	MABLOGDEBUG("XboxOneMabSessionManager::GetAssociation for id: %s",xboxid.c_str());
	MabMap< MabString, Windows::Xbox::Networking::SecureDeviceAssociation^ >::const_iterator found_iter = m_SecureDeviceMap.find( xboxid );

	return( found_iter != m_SecureDeviceMap.end() ) ? found_iter->second : nullptr;
}

void XboxOneMabSessionManager::AddAssociation(MabString xboxid, Windows::Xbox::Networking::SecureDeviceAssociation^ deviceAsso)
{
	MABLOGDEBUG("XboxOneMabSessionManager::AddAssociation for id: %s",xboxid.c_str());
	m_SecureDeviceMap[ xboxid ] = deviceAsso;
}

void XboxOneMabSessionManager::OnAssociationIncoming(Windows::Xbox::Networking::SecureDeviceAssociationTemplate^ associationTemplate, Windows::Xbox::Networking::SecureDeviceAssociationIncomingEventArgs^ args)
{
	m_testRemoteSecureAssociation = args->Association;
}

Windows::Xbox::Networking::SecureDeviceAssociation^ XboxOneMabSessionManager::GetRemoteAssociation()
{
	return m_testRemoteSecureAssociation;
}

MabString XboxOneMabSessionManager::GetSessionUserGametag(const MabString& xuid)
{
	auto session = GetGameSession();
	if(session == nullptr)
	{
		return MabString("");
	}

	for (auto member : session->Members)
	{
		Platform::String^ peerName = member->XboxUserId;
		std::string narrow_xuid(peerName->Begin(), peerName->End());
		MabString peerxuid = narrow_xuid.c_str();
		if(peerxuid == xuid)
		{
			Platform::String^ peerGameTag = member->Gamertag; 
			std::string narrow_peerGameTag(peerGameTag->Begin(), peerGameTag->End());
			MabString peertag = narrow_peerGameTag.c_str();
			return peertag;
		}
	}
	return MabString("");
}

