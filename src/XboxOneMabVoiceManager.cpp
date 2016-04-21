#include "pch.h"
#include "XboxOneMabVoiceManager.h"

#include <MabEvent.h>
#include <xboxone/MabSecureSocketUDP.h>
#include <MabNetMessageHandler.h>
#include "XboxOneMabUtils.h"
#include "XboxOneMabUserInfo.h"
#include "XboxOneMabSessionManager.h"

#include <MabNetworkManager.h>
#include <MabNetMessageDatabase.h>
#include <MabNetMessage.h>
#include <MabNetSerialiser.h>

using namespace Concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::Networking;
using namespace Windows::Xbox::System;

static const MabNetMessageType XBOXONE_VOICE_MSG = MNPG_XBOXONEMAB;
static const MabNetMessageType XBOXONE_MUTE_MSG = XBOXONE_VOICE_MSG + 1;
const char MUTE_VALUE[] = "mute_value";
const char DATA_LEN[] = "data_len";
const char DATA_BUF[] = "data_buf";
const char MUTE_SOURCE[] = "mute_source";


MAB_USER_TYPE_REFLECTION(XboxOneMabVoiceManager::ChatDataPacket,4001);
MABNET_SERIALISER_POD(XboxOneMabVoiceManager::ChatDataPacket);
MABNET_SERIALISER_HEADER(unsigned char);
MABNET_SERIALISER_HEADER(unsigned int);

XboxOneMabVoiceManager::XboxOneMabVoiceManager() 
{
	m_chatManager = ref new Microsoft::Xbox::GameChat::ChatManager(); 
	m_remoteMuted = false;
}



void XboxOneMabVoiceManager::RemoveHandler()
{
	if( m_chatManager != nullptr )
	{
#ifdef _DEBUG
		m_chatManager->OnDebugMessage -= m_tokenOnDebugMessage;
#endif
		m_chatManager->OnOutgoingChatPacketReady -= m_tokenOnOutgoingChatPacketReady;
		m_chatManager->OnCompareUniqueConsoleIdentifiers -= m_tokenOnCompareUniqueConsoleIdentifiers;
		Windows::ApplicationModel::Core::CoreApplication::ResourceAvailabilityChanged -= m_tokenResourceAvailabilityChanged;
		if( m_chatManager->ChatSettings->PreEncodeCallbackEnabled )
		{
			m_chatManager->OnPreEncodeAudioBuffer -= m_tokenOnPreEncodeAudioBuffer;
		}
		if( m_chatManager->ChatSettings->PostDecodeCallbackEnabled )
		{
			m_chatManager->OnPostDecodeAudioBuffer -= m_tokenOnPostDecodeAudioBuffer;
		}
		m_chatManager = nullptr;
	}
	if(network_manager != nullptr)
	{
		network_manager->RemoveMessageHandler(XBOXONE_VOICE_MSG);
		network_manager->RemoveMessageHandler(XBOXONE_MUTE_MSG);
	}
}


XboxOneMabVoiceManager::~XboxOneMabVoiceManager()
{
	RemoveHandler();
}


void XboxOneMabVoiceManager::SetNetworkManager(MabNetworkManager* _network_manager)
{
	if( m_chatManager != nullptr )
	{
		network_manager = _network_manager;
		network_manager->GetDatabase()->RegisterMessage(XBOXONE_VOICE_MSG)	
			.AddVariable<uint32_t>(DATA_LEN)
			.AddVariable<ChatDataPacket>(DATA_BUF);
		network_manager->RegisterMessageHandler(XBOXONE_VOICE_MSG, this);

		network_manager->GetDatabase()->RegisterMessage(XBOXONE_MUTE_MSG)
			.AddVariable<bool>(MUTE_VALUE);
//			.AddVariable<MabString>(MUTE_SOURCE);
		network_manager->RegisterMessageHandler(XBOXONE_MUTE_MSG, this);
	}
}

bool XboxOneMabVoiceManager::Initilize(__in bool combineCaptureBuffersIntoSinglePacket,
									   __in bool useKinectAsCaptureSource,
									   __in bool applySoundEffectsToCapturedAudio,
									   __in bool applySoundEffectsToChatRenderedAudio)
{
	XboxOneMabUtils::LogComment(L"XboxOneMabVoiceManager::Initilize");
	if(m_chatManager == nullptr)
		return false;
	m_chatManager->ChatSettings->CombineCaptureBuffersIntoSinglePacket = combineCaptureBuffersIntoSinglePacket; // if unset, it defaults to TRUE
	m_chatManager->ChatSettings->UseKinectAsCaptureSource = useKinectAsCaptureSource; // if unset, it defaults to FALSE
	m_chatManager->ChatSettings->PreEncodeCallbackEnabled = applySoundEffectsToCapturedAudio; // if unset, it defaults to FALSE
	m_chatManager->ChatSettings->PostDecodeCallbackEnabled = applySoundEffectsToChatRenderedAudio; // if unset, it defaults to FALSE


	if( applySoundEffectsToCapturedAudio )
	{
		m_tokenOnPreEncodeAudioBuffer = m_chatManager->OnPreEncodeAudioBuffer += ref new Microsoft::Xbox::GameChat::ProcessAudioBufferHandler( [ this ](
			__in Windows::Storage::Streams::IBuffer^ preEncodedRawBuffer, 
			__in Windows::Xbox::Chat::IFormat^ audioFormat,
			__in Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::GameChat::ChatUser^>^ chatUsers
			)
		{            
			Windows::Storage::Streams::IBuffer^ rawBufferWithAudioEffects;

			// Using a std::weak_ptr instead of 'this' to avoid dangling pointer if caller class is released.
			// Simply unregistering the callback in the destructor isn't enough to prevent a dangling pointer
			if( this != nullptr )
			{
				rawBufferWithAudioEffects = ApplyPreEncodeChatSoundEffects(
					preEncodedRawBuffer, 
					audioFormat, 
					chatUsers
					);
			}
			return rawBufferWithAudioEffects;
		});
	}

	//not list here
	//Todo:added those condition if needed

	// Upon enter constrained mode, mute everyone.  
	// Upon leaving constrained mode, unmute everyone who was previously muted.
	m_tokenResourceAvailabilityChanged = Windows::ApplicationModel::Core::CoreApplication::ResourceAvailabilityChanged += 
		ref new EventHandler< Platform::Object^ >( [this] (Platform::Object^, Platform::Object^ )
	{
		// Using a std::weak_ptr instead of 'this' to avoid dangling pointer if caller class is released.
		// Simply unregistering the callback in the destructor isn't enough to prevent a dangling pointer
		if( this != nullptr )
		{
			if (Windows::ApplicationModel::Core::CoreApplication::ResourceAvailability == Windows::ApplicationModel::Core::ResourceAvailability::Constrained)
			{
				if( m_chatManager != nullptr )
				{
					m_chatManager->MuteAllUsersFromAllChannels();
				}
			}
			else if(Windows::ApplicationModel::Core::CoreApplication::ResourceAvailability == Windows::ApplicationModel::Core::ResourceAvailability::Full)
			{
				if( m_chatManager != nullptr )
				{
					m_chatManager->UnmuteAllUsersFromAllChannels();

					// The title should remember who was muted so when the Resume even occurs
					// to avoid unmuting users who has been previously muted.  Simply re-mute them here
				}
			}
		}
	});

#ifdef _DEBUG
	m_tokenOnDebugMessage = m_chatManager->OnDebugMessage += 
		ref new Windows::Foundation::EventHandler<Microsoft::Xbox::GameChat::DebugMessageEventArgs^>(
		[this] ( Platform::Object^, Microsoft::Xbox::GameChat::DebugMessageEventArgs^ args )
	{
		// Using a std::weak_ptr instead of 'this' to avoid dangling pointer if caller class is released.
		// Simply unregistering the callback in the destructor isn't enough to prevent a dangling pointer
		if( this != nullptr )
		{
			OnDebugMessageReceived(args);
		}
	});
#endif
	m_tokenOnOutgoingChatPacketReady = m_chatManager->OnOutgoingChatPacketReady += 
		ref new Windows::Foundation::EventHandler<Microsoft::Xbox::GameChat::ChatPacketEventArgs^>( 
		[this] ( Platform::Object^, Microsoft::Xbox::GameChat::ChatPacketEventArgs^ args )
	{
		// Using a std::weak_ptr instead of 'this' to avoid dangling pointer if caller class is released.
		// Simply unregistering the callback in the destructor isn't enough to prevent a dangling pointer
		if( this != nullptr )
		{
			OnOutgoingChatPacketReady(args);
		}
	});

	m_tokenOnCompareUniqueConsoleIdentifiers = m_chatManager->OnCompareUniqueConsoleIdentifiers += 
		ref new Microsoft::Xbox::GameChat::CompareUniqueConsoleIdentifiersHandler( 
		[this] ( Platform::Object^ obj1, Platform::Object^ obj2 ) 
	{ 
		// Using a std::weak_ptr instead of 'this' to avoid dangling pointer if caller class is released.
		// Simply unregistering the callback in the destructor isn't enough to prevent a dangling pointer
		if( this )
		{
			return CompareUniqueConsoleIdentifiers(obj1, obj2); 
		}
		else
		{
			return false;
		}
	});

	return true;
}



void XboxOneMabVoiceManager::OnOutgoingChatPacketReady(__in Microsoft::Xbox::GameChat::ChatPacketEventArgs^ args)
{
	//Send message to peers

	Windows::Storage::Streams::IBuffer^ outsendBuffer = args->PacketBuffer;

	uint32_t packetSize = outsendBuffer->Length;

	BYTE* byteBufferPointer;
	XboxOneMabUtils::ConvertPlatformIBufferToBytes(outsendBuffer, &byteBufferPointer);

	XboxOneMabUtils::LogCommentFormat(L"Try to send a block of voice data. Length is %d",outsendBuffer->Length);

	ChatDataPacket pkt;
	pkt.assign(byteBufferPointer,packetSize);

	MabNetMessage message(network_manager->GetDatabase(), XBOXONE_VOICE_MSG);
	message.PackVariable(DATA_LEN,packetSize);
	message.PackVariable(DATA_BUF,pkt);

	if (args->SendPacketToAllConnectedConsoles)
	{
		int totalPeers = network_manager->GetNumPeers();
		for(int i = 0; i < totalPeers; i ++)
		{
			MabNodeId peer_id = network_manager->GetPeerNodeId(i);

			// Don't bother if this person is muted
			RemoteClientDataMap::iterator itr = remote_client_map.find( peer_id );
			if(itr != remote_client_map.end() && itr->second.m_ismuted != true){
				network_manager->SendMessage(peer_id, message, STREAM_UNRELIABLE);
				XboxOneMabUtils::LogCommentFormat(L"network send to %d for chat message length is: %d",peer_id,packetSize );
			}
		}

	}else{
		//It will send first message to join the chat session
		XboxOneMabUtils::LogComment("Try to send to remote device association");
		Windows::Xbox::Networking::SecureDeviceAssociation^ targetAssociation = dynamic_cast<Windows::Xbox::Networking::SecureDeviceAssociation^>(args->UniqueTargetConsoleIdentifier);
		if (targetAssociation != nullptr)
		{
			XboxOneMabUtils::LogComment("successfully sent to remote device association!");
			int totalPeers = network_manager->GetNumPeers();
			for(int i = 0; i < totalPeers; i ++)
			{
				MabNodeId peer_id = network_manager->GetPeerNodeId(i);

				// Don't bother if this person is muted
				RemoteClientDataMap::iterator itr = remote_client_map.find( peer_id );
				if(itr != remote_client_map.end() && itr->second.m_ismuted != true)

					network_manager->SendMessage(peer_id, message, STREAM_UNRELIABLE);
				XboxOneMabUtils::LogCommentFormat(L"network send to %d for chat message length is: %d",peer_id,packetSize );
			}
		}
	}

}



void XboxOneMabVoiceManager::ProcessNetMessage(MabNodeId source_connection, const MabNetMessage& msg)
{
	XboxOneMabUtils::LogCommentFormat( L" XboxOneMabVoiceManager::ProcessNetMessage: %d", msg.GetType() );
	if(msg.GetType() == XBOXONE_VOICE_MSG)
	{

		uint32_t len;
		ChatDataPacket pkt;

		msg.UnPackVariable(DATA_LEN,len);
		msg.UnPackVariable(DATA_BUF,pkt);

		Windows::Storage::Streams::IBuffer^ receivedBuffer = ref new Windows::Storage::Streams::Buffer( len );

		receivedBuffer->Length = len;
		BYTE* recvBufferBytes = nullptr;

		XboxOneMabUtils::ConvertPlatformIBufferToBytes(receivedBuffer, &recvBufferBytes);
		memcpy_s(recvBufferBytes, receivedBuffer->Length, pkt.array, len);
		try{
			if( m_chatManager != nullptr && len > 0 )
			{
				MabString xboxid = network_manager->GetPeerInfo(source_connection)->GetDetails().member_id;
				Windows::Xbox::Networking::SecureDeviceAssociation^ deviceAssociation = XboxOneSessionManager->GetAssociation(xboxid);
//				Windows::Xbox::Networking::SecureDeviceAssociation^ deviceAssociation = XboxOneSessionManager->GetRemoteAssociation();
				if(deviceAssociation != nullptr){
					MABLOGDEBUG("Process voice data from xbox user id: %s",xboxid.c_str());
					Microsoft::Xbox::GameChat::ChatMessageType chatMessageType = m_chatManager->ProcessIncomingChatMessage(receivedBuffer, deviceAssociation);
				}
			}
		}catch(Platform::COMException^ ex){
			XboxOneMabUtils::LogCommentFormat(L"Got Exception from voice manager : %d",ex->GetType());
		}
	}

	// Handle muting messages
	else if(msg.GetType() == XBOXONE_MUTE_MSG)
	{
		RemoteClientDataMap::iterator itr = remote_client_map.find( source_connection );
		if(itr == remote_client_map.end()) return;

		RemoteClientUserData& remote_client = itr->second;
		MabString mute_source;
		bool remoteMute;
		msg.UnPackVariable(MUTE_VALUE, remoteMute);
//		msg.UnPackVariable(MUTE_SOURCE, mute_source);
//		remote_client.m_ismuted = remoteMute;
		m_remoteMuted = remoteMute;
		MABLOGDEBUG("Receive remote xuid %s for state is %d",remote_client.m_clientDetail.member_id,remote_client.m_ismuted);
//		ProcessMute(0, remote_client.m_clientDetail.member_id, remote_client.m_ismuted);
		OnVoiceEvent(XboxOneMabVoiceEvent::XBONE_USER_MUTING_CHANGED,0,GetGamerTag(network_manager->GetLocalNodeId(),0));
	}
}

void XboxOneMabVoiceManager::Update()
{
	//Handle whether if incoming message arrived
}

void XboxOneMabVoiceManager::SendMuteMessage(unsigned int controller_num, MabString xuid, bool value)
{
	 //Send message as UDP packet
	MABUNUSED(controller_num);
	MabString m_userid;
	XboxOneMabUtils::ConvertPlatformStringToMabString(XboxOneUser->GetCurrentUser()->XboxUserId,m_userid);
	MabNetMessage msg( network_manager->GetDatabase(), XBOXONE_MUTE_MSG );
	msg.PackVariable(MUTE_VALUE, value);
//	msg.PackVariable(MUTE_SOURCE, m_userid);

	bool message_sent = false;

	for (RemoteClientDataMap::iterator rc_it = remote_client_map.begin(); 
		rc_it != remote_client_map.end() && !message_sent; ++rc_it)
	{
		RemoteClientUserData& remote_user = rc_it->second;
		if(remote_user.m_clientDetail.member_id == xuid){
			{
				network_manager->SendMessage(rc_it->first, msg, MNSI_SYSTEM);
				message_sent = true;
				break;
			}
		}
	}
}


void XboxOneMabVoiceManager::ProcessMute(unsigned int controller_num, MabString xbuserid, bool value)
{
	MABUNUSED(controller_num);
	Microsoft::Xbox::GameChat::ChatUser^ chatUser = GetChatUserByXboxUserId(xbuserid);
	if (chatUser)
	{
		if(value == true)
			m_chatManager->MuteUserFromAllChannels(chatUser);
		else
			m_chatManager->UnmuteUserFromAllChannels(chatUser);
		for(RemoteClientDataMap::iterator itr = remote_client_map.begin();itr != remote_client_map.end(); ++itr)
		{
			RemoteClientUserData& m_client = itr->second;
			if(m_client.m_clientDetail.member_id == xbuserid){
				MABLOGDEBUG("set mute for xbid: %s, state %d",xbuserid.c_str(),value);
				m_client.m_ismuted = value;
			}
		}
	}
}


void XboxOneMabVoiceManager::SetRemoteMute(unsigned int controller_num, MabString gametag, bool value)
{
	MABUNUSED(controller_num);

	MabString xuid;

	for(RemoteClientDataMap::iterator itr = remote_client_map.begin();itr != remote_client_map.end(); ++itr)
	{
		RemoteClientUserData& m_client = itr->second;
		if(m_client.m_clientDetail.game_tag == gametag){
			xuid = m_client.m_clientDetail.member_id;
		}
	}
	if(!xuid.empty()){
		ProcessMute(controller_num,xuid,value);
		SendMuteMessage(controller_num,xuid,value);
		OnVoiceEvent(XboxOneMabVoiceEvent::XBONE_USER_MUTING_CHANGED,controller_num,xuid);
	}
}

bool XboxOneMabVoiceManager::QueryMute(unsigned int local_player_num, const char* name)
{
	XboxOneMabUtils::LogCommentFormat(L"query the mute state %s", name);

	for(RemoteClientDataMap::iterator itr = remote_client_map.begin();itr != remote_client_map.end(); ++itr)
	{
		RemoteClientUserData& m_client = itr->second;
		if(m_client.m_clientDetail.game_tag == MabString(name)){
			MABLOGDEBUG("xuid %s get mute state is: %d",m_client.m_clientDetail.game_tag.c_str(),m_client.m_ismuted);
			return m_client.m_ismuted;
		}
	}
	//Local user show default to unmuted

	return m_remoteMuted;
}

bool XboxOneMabVoiceManager::QueryMute(unsigned int local_player_num, MabString user_id)
{
	return QueryMute(local_player_num,user_id.c_str());
}


Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::GameChat::ChatUser^>^ XboxOneMabVoiceManager::GetChatUsers()
{
	if( m_chatManager != nullptr )
	{
		return m_chatManager->GetChatUsers();
	}
	return nullptr;
}

Microsoft::Xbox::GameChat::ChatUser^ XboxOneMabVoiceManager::GetChatUserByXboxUserId(__in MabString& xbuserid)
{
	 Platform::String^ xuid = XboxOneMabUtils::ConvertMabStringToPlatformString(&xbuserid);
	 return GetChatUserByXboxUserId(xuid);
}


Microsoft::Xbox::GameChat::ChatUser^ XboxOneMabVoiceManager::GetChatUserByXboxUserId(__in Platform::String^ xbusertag)
{
	Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::GameChat::ChatUser^>^ chatUsers = GetChatUsers();
	for each (Microsoft::Xbox::GameChat::ChatUser^ chatUser in chatUsers)
	{
		XboxOneMabUtils::LogCommentFormat(L"chat user to check for xbuid: %ws",chatUser->XboxUserId->Data());
		if (chatUser != nullptr && 
			XboxOneMabUtils::IsStringEqualCaseInsenstive(chatUser->XboxUserId, xbusertag) )
		{
			return chatUser;
		}
	}

	return nullptr;
}

MabNodeId XboxOneMabVoiceManager::GetMabNodeIDByXboxUserId(MabString& xboxuserid)
{
	for(RemoteClientDataMap::iterator itr = remote_client_map.begin();itr != remote_client_map.end(); ++itr)
	{
		if(itr->second.m_clientDetail.member_id == xboxuserid)
			return itr->first;
	}
	return NULL;
}

void XboxOneMabVoiceManager::RegisterClient(MabNodeId node_id)
{
	XboxOneMabUtils::LogCommentFormat(L"XboxOneMabVoiceManager::RegisterClient for node id: %d",node_id);
	RemoteClientUserData& data = remote_client_map[ node_id ];
	data.SetClientData( network_manager->GetPeerInfo(node_id)->GetDetails());
	data.SetMuted(false);

	const MabString xuid = data.m_clientDetail.member_id;

	RegisterClientXUID(xuid, node_id);

}

void XboxOneMabVoiceManager::RegisterClientXUID(const MabString& xuid,MabNodeId node_id)
{
	MABLOGDEBUG("XboxOneMabVoiceManager::RegisterClient for xuid: %s",xuid.c_str());
	if( m_chatManager != nullptr && !xuid.empty())
	{

		RemoteClientUserData& data = remote_client_map[ node_id ];
		data.SetClientData( network_manager->GetPeerInfo(node_id)->GetDetails());
		data.SetMuted(false);

		try{
			Windows::Xbox::Networking::SecureDeviceAssociation^ remoteClientAssociation = XboxOneSessionManager->GetAssociation(xuid);
			if(remoteClientAssociation != nullptr){
				XboxOneMabUtils::LogComment("Add remote console to chat manager");
				m_chatManager->HandleNewRemoteConsole(remoteClientAssociation);
				OnVoiceEvent(XboxOneMabVoiceEvent::XBONE_USER_USERS_CHANGED,0,MabString(xuid));
			}
		}catch(Platform::Exception^ ex)
		{
			XboxOneMabUtils::LogCommentFormat(L"Met errors at HandleNewRemoteConsole for: %ws",ex->Message->Data());
		}
	}
}

void XboxOneMabVoiceManager::AddLocalUserToChatChannel(__in Windows::Xbox::System::IUser^ user /* = nullptr*/)
{
	XboxOneMabUtils::LogComment(L"XboxOneMabVoiceManager::AddLocalUserToChatChannel");
	Windows::Xbox::System::IUser^ currentLocalUser;
	if(user == nullptr){
		currentLocalUser =  XboxOneMabUserInfo::Get()->GetCurrentUser();
	}else{
		currentLocalUser = user;
	}

	if( m_chatManager != nullptr && currentLocalUser != nullptr )
	{
		uint8 channelIndex = (uint8)0;
		auto asyncOp = m_chatManager->AddLocalUserToChatChannelAsync( channelIndex, currentLocalUser );
		concurrency::create_task( asyncOp )
			.then( [this] ( concurrency::task<void> t )
		{
			// Error handling
			try
			{
				t.get();                
			}
			catch ( Platform::Exception^ ex )
			{
				XboxOneMabUtils::LogCommentFormat( L"AddLocalUserToChatChannelAsync failed: $d", ex->HResult );
			}
		});
	}
}


void XboxOneMabVoiceManager::RemoveUserFromChatChannel(__in uint8 channelIndex,__in Windows::Xbox::System::IUser^ user)
{
	if( m_chatManager != nullptr )
	{
		// This is helper function waits for the task to complete so shouldn't be called from the UI thread 
		// Remove the .wait() and return the result of concurrency::create_task() if you want to call it from the UI thread 
		// and chain PPL tasks together

		auto asyncOp = m_chatManager->RemoveLocalUserFromChatChannelAsync( channelIndex, user );
		concurrency::create_task( asyncOp ).then( [this] ( concurrency::task<void> t )
		{
			// Error handling
			try
			{
				t.get();
			}
			catch ( Platform::Exception^ ex )
			{
				XboxOneMabUtils::LogCommentFormat( L"RemoveLocalUserFromChatChannelAsync failed : %d", ex->HResult );
			}
		});
	}
}



void XboxOneMabVoiceManager::UnregisterClient( MabNodeId node_id)
{
	// remove from map
	RemoteClientDataMap::iterator itr = remote_client_map.find( node_id );
	if ( itr != remote_client_map.end() )
	{
		if( m_chatManager != nullptr )
		{
			const MabString xuid = itr->second.m_clientDetail.member_id;
			Windows::Xbox::Networking::SecureDeviceAssociation^ remoteClientAssociation = XboxOneSessionManager->GetAssociation(xuid);

			try{
				auto asyncOp = m_chatManager->RemoveRemoteConsoleAsync( remoteClientAssociation );
				concurrency::create_task( asyncOp ).then( [this, &itr, &xuid] ( concurrency::task<void> t )
				{
					// Error handling
					try
					{
						remote_client_map.erase( itr );
						t.get();
						OnVoiceEvent(XboxOneMabVoiceEvent::XBONE_USER_USERS_CHANGED,0,MabString(xuid));
					}
					catch ( Platform::Exception^ ex )
					{
						XboxOneMabUtils::LogCommentFormat( L"RemoveRemoteConsoleAsync failed : %d", ex->HResult );
					}
				});
			}catch( Platform::Exception^ ex )
			{
				XboxOneMabUtils::LogCommentFormat( L"RemoveRemoteConsoleAsync failed : %d", ex->HResult );
			}
		}
//		remote_client_map.erase( itr );
	}
}


Windows::Storage::Streams::IBuffer^ XboxOneMabVoiceManager::ApplyPreEncodeChatSoundEffects(__in Windows::Storage::Streams::IBuffer^ preEncodedRawBuffer, __in Windows::Xbox::Chat::IFormat^ audioFormat, __in Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::GameChat::ChatUser^>^ chatUsers)
{
	return nullptr;
}

void XboxOneMabVoiceManager::OnDebugMessageReceived(__in Microsoft::Xbox::GameChat::DebugMessageEventArgs^ args)
{
	// To integrate the Chat DLL in your game, 
	// change this to false and remove the LogComment calls, 
	// or integrate with your game's own UI/debug message logging system
	bool outputToUI = true; 

	if( outputToUI )
	{
		if (args->ErrorCode == S_OK )
		{
			XboxOneMabUtils::LogCommentFormat(L"GameChat: %ws", args->Message->Data());
		}
		else
		{
			XboxOneMabUtils::LogCommentFormat(L"GameChat: %ws with error code: %d", args->Message->Data(), args->ErrorCode);
		}
	}
	else
	{
		// The string appear in the Visual Studio Output window 
		OutputDebugString( args->Message->Data() );
	}
}

bool XboxOneMabVoiceManager::CompareUniqueConsoleIdentifiers(__in Platform::Object^ uniqueRemoteConsoleIdentifier1, __in Platform::Object^ uniqueRemoteConsoleIdentifier2)
{
	if (uniqueRemoteConsoleIdentifier1 == nullptr || uniqueRemoteConsoleIdentifier2 == nullptr)
	{
		return false;
	}

//	int sda1 = PlatformObjectToInt(uniqueRemoteConsoleIdentifier1);
//	int sda2 = PlatformObjectToInt(uniqueRemoteConsoleIdentifier2);
//	return sda1 == sda2;
	// uniqueRemoteConsoleIdentifier is a Platform::Object^ and can be cast or unboxed to most types. 
	// What exactly you use doesn't matter, but optimally it would be something that uniquely identifies a console on in the session. 
	// A Windows::Xbox::Networking::SecureDeviceAssociation^ is perfect to use if you have access to it.

	// This is how you would convert a Platform::Object to an int 
	// int consoleId = PlatformOjectToInt(args->TargetUniqueConsoleIdentifier);
	// And then compare as normal

	// In this example, the uniqueRemoteConsoleIdentifier is a Windows::Xbox::Networking::SecureDeviceAssociation, 
	// so we compare using the RemoteSecureDeviceAddress using the helper function AreSecureDeviceAddressesEqual
	   Windows::Xbox::Networking::SecureDeviceAssociation^ sda1 = dynamic_cast<Windows::Xbox::Networking::SecureDeviceAssociation^>(uniqueRemoteConsoleIdentifier1);
	   Windows::Xbox::Networking::SecureDeviceAssociation^ sda2 = dynamic_cast<Windows::Xbox::Networking::SecureDeviceAssociation^>(uniqueRemoteConsoleIdentifier2);
	   return XboxOneMabUtils::AreSecureDeviceAddressesEqual(sda1, sda2);
}

MabString XboxOneMabVoiceManager::GetGamerTag(MabNodeId node_id, unsigned int controller_num)
{
	//Todo: How to get remote user controller?
	if(controller_num > 0)
	{
		return MabString();
	}
	if(network_manager)
	{
		if(node_id == network_manager->GetLocalNodeId()){
			MabString m_gameName;
#ifdef _DEBUG
			MabString m_gameDisplayName;
			int iresult;
			XboxOneMabUtils::ConvertPlatformStringToMabString(XboxOneUser->GetCurrentUser()->DisplayInfo->GameDisplayName,m_gameDisplayName);
//			IsTextUnicode(m_gameDisplayName,m_gameDisplayName.size(),&iresult);
#endif
			XboxOneMabUtils::ConvertPlatformStringToMabString(XboxOneUser->GetCurrentUser()->DisplayInfo->Gamertag,m_gameName);
			return m_gameName;
		}

		RemoteClientDataMap::iterator itr = remote_client_map.find( node_id );
		if ( itr != remote_client_map.end() )
		{
			if( m_chatManager != nullptr )
			{
				MabString gametag = itr->second.m_clientDetail.game_tag;
				return gametag;
			}
		}
	}
	
	return MabString();
}

MabString XboxOneMabVoiceManager::GetGamerTag(const XboxOneMabNetOnlineClient* client, unsigned int controller_num)
{
	const MabString gametag = client->game_tag;
	return gametag;
}

void XboxOneMabVoiceManager::OnUserAudioDeviceAdded(AudioDeviceAddedEventArgs^ eventArgs)
{
	User^ user = eventArgs->User;
	if( user != nullptr )
	{
		AddLocalUserToChatChannel( 
			user 
			);
	}
}

void XboxOneMabVoiceManager::OnUserRemoved(UserRemovedEventArgs^ eventArgs)
{
	User^ user = eventArgs->User;
	if( user != nullptr )
	{
		RemoveUserFromChatChannel( 
			0,  //Channel ID
			user 
			);
	}
}

MabString XboxOneMabVoiceManager::GetXUIDFromGamerTag(const char* gamertag)
{
	for(RemoteClientDataMap::iterator itr = remote_client_map.begin();itr != remote_client_map.end(); ++itr)
	{
		RemoteClientUserData& m_client = itr->second;
		if(m_client.m_clientDetail.game_tag == MabString(gamertag)){
			return m_client.m_clientDetail.member_id;
		}
	}

	//would also be myself
	MabString self_Gametag;
	XboxOneMabUtils::ConvertPlatformStringToMabString(XboxOneMabUserInfo::Get()->GetCurrentUser()->DisplayInfo->Gamertag,self_Gametag);

	if(self_Gametag == MabString(gamertag) )
	{
		MabString self_userid;
		XboxOneMabUtils::ConvertPlatformStringToMabString(XboxOneMabUserInfo::Get()->GetCurrentUser()->XboxUserId,self_userid);
		return self_userid;
	}
	return MabString("");
}
