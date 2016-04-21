#ifndef _XBOXONE_MAB_VOICE_MANAGER_H
#define _XBOXONE_MAB_VOICE_MANAGER_H

#include <MabCoreLib.h>
#include <MabMemLib.h>
#include "XboxOneMabSingleton.h"
#include <collection.h>
#include <ppltasks.h>

//Network Part
#include <MabEvent.h>
#include <xboxone/MabSecureSocketUDP.h>
#include <MabNetMessageHandler.h>
#include "MabNetOnlineClientDetails.h"
#include "XboxOneMabNetTypes.h"

class MabNetworkManager;

// constants
static const DWORD MAX_VOICE_BUFFER_TIME = 200;  // 200ms
static const DWORD XBOXONE_MAX_VOICE_USERS = 8;
static const DWORD MABXBOXONE_NETWORK = 6;

static const DWORD MAX_CHAT_PACKET_SIZE = 800;

// Type Definitions
typedef MabSet< MabString > XUIDSet;
typedef MabMap<MabString, bool> XUIDMuteMap;
typedef MabSet< ULONGLONG > TargetConsoleSet;


enum XboxOneMabVoiceEvent
{
	XBONE_USER_SPEAKING_CHANGED_LOCAL,
	XBONE_USER_SPEAKING_CHANGED_REMOTE,
	XBONE_USER_MUTING_CHANGED,
	XBONE_USER_USERS_CHANGED
};


class XboxOneMabVoiceManager : public XboxOneMabSingleton<XboxOneMabVoiceManager>,	public MabNetMessageHandler
{

private:

	struct RemoteClientUserData
	{
		RemoteClientUserData(){
			m_ismuted = false;
		};
		void SetClientData(const XboxOneMabNetOnlineClient& client){m_clientDetail = client;}
		void SetMuted(bool isMuted){m_ismuted = isMuted;}
		MabString getXboxUserid(MabString& xboxusertag){return m_clientDetail.game_tag;}
		bool m_ismuted ;
		XboxOneMabNetOnlineClient m_clientDetail;
	};

public:

	typedef MabArray<byte,MAX_CHAT_PACKET_SIZE> ChatDataPacket;

	XboxOneMabVoiceManager();
	~XboxOneMabVoiceManager();


	static const int MAX_NUM_CHANNELS = 1; //only 1 local talker possible
	static const DWORD MAX_REMOTE_TALKERS = 8; 

	void SetNetworkManager(MabNetworkManager* new_network_manager);
	void RemoveHandler();

	virtual bool Initilize(__in bool combineCaptureBuffersIntoSinglePacketEnabled = true,
		__in bool useKinectAsCaptureSource  = false,
		__in bool applySoundEffectToCapture  = false,
		__in bool applySoundEffectToRender  = false);

	/// Register all handler already, Should we need an update any more?
	void Update();

	// Session start and end.
	void StartSession() {session_started = true;}
	void EndSession( void );

	void RegisterClient( MabNodeId node_id );
	void RegisterClientXUID(const MabString& xuid, MabNodeId node_id);
	void UnregisterClient( MabNodeId node_id );

	/// Loopback mode
	void ToggleLoopbackMode  ( unsigned int controller_num );

	/// Test for talking
	bool IsTalking( const char* name );

	/// Checks if headset present
	bool IsHeadsetPresent( const char* name );

	/// returns true if the given client needs to send voice data
	bool NeedToSend( const XboxOneMabNetOnlineClient &client );

	/// Retrieve outgoing voice data
	WORD GetVoiceData( const XboxOneMabNetOnlineClient & client, BYTE* buffer, WORD buffer_size );

	/// Submit incoming voice data
	void SubmitVoiceData( const XboxOneMabNetOnlineClient & client, const BYTE* buffer, WORD buffer_size );

	/// Reevaluate mutelists
	void ProcessMutelists();

	MabString GetGamerTag( MabNodeId node_id, unsigned int controller_num );
	MabString GetGamerTag( const XboxOneMabNetOnlineClient* client, unsigned int controller_num );
	MabString GetXUIDFromGamerTag( const char* gamertag );

	/// Wrapper for the mute list query
	bool QueryMute(unsigned int local_player_num, const char* name);
	bool QueryMute(unsigned int local_player_num, MabString user_id);

	/// Reciprocally mute a player, where necessary
	void ProcessMute( unsigned int controller_num, MabString xuid, bool value  );
	/// Set the value of a remote mute, or add one if the value does not exist.  This will also trigger a ProcessMute call.
	void SetRemoteMute( unsigned int controller_num, MabString xuid, bool value );

	void SendMuteMessage( unsigned int controller_num, MabString xuid, bool value );
	void SendHeadsetMessage( unsigned int controller_num, bool value );
	void SendLocalUser( unsigned int controller_num, MabNodeId node_id = NODE_ALL );

	void AddLocalUserToChatChannel(__in Windows::Xbox::System::IUser^ user = nullptr);
	void RemoveUserFromChatChannel(__in uint8 channelIndex,__in Windows::Xbox::System::IUser^ user);

	/// Processes inbound network packets
	virtual void ProcessNetMessage( MabNodeId source_connection, const MabNetMessage& msg );

	/// Observer messages from Xbox360UserManager
	//void Update(MabObservable<Xbox360MabSystemNotificationMessage> *source, const Xbox360MabSystemNotificationMessage &msg);

	/// Rather than add yet another observer implementation, I'll just add a single simple event type.
	MabEvent<XboxOneMabVoiceEvent, unsigned int, MabString&> OnVoiceEvent;



	/// <summary>
	/// This delegate handler is called prior to encoding captured audio buffer.
	/// This allows titles to apply sound effects to the capture stream
	/// </summary>
	Windows::Storage::Streams::IBuffer^ ApplyPreEncodeChatSoundEffects(
		__in Windows::Storage::Streams::IBuffer^ preEncodedRawBuffer, 
		__in Windows::Xbox::Chat::IFormat^ audioFormat,
		__in Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::GameChat::ChatUser^>^ chatUsers
		);
	/// <summary>
	/// This delegate handler is called after decoding a remote audio buffer.
	/// This allows titles to apply sound effects to a remote user's audio mix
	/// </summary>
	Windows::Storage::Streams::IBuffer^ ApplyPostDecodeChatSoundEffects(
		__in Windows::Storage::Streams::IBuffer^ postDecodedRawBuffer, 
		__in Windows::Xbox::Chat::IFormat^ audioFormat,
		__in Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::GameChat::ChatUser^>^ chatUsers
		);

	/// <summary>
	/// Handles when a debug message is received.  Send this to the UI and OutputDebugString.  Games should integrate with their existing log system.
	/// </summary>
	/// <param name="args">Contains the debug message to log</param>
	void OnDebugMessageReceived( 
		__in Microsoft::Xbox::GameChat::DebugMessageEventArgs^ args 
		);

	/// <summary>
	/// Send the chat packet to all connected consoles
	///
	/// To integrate the Chat DLL in your game, change the following code to use your game's network layer.
	/// You will need to isolate chat messages to be unique from the rest of you game's other message types.
	/// When args->SendPacketToAllConnectedConsoles is true, your game should send the chat message to each connected console using the game's network layer.
	/// It should send the chat message with Reliable UDP if args->SendReliable is true.
	/// It should send the chat message in order (if that feature is available) if args->SendInOrder is true
	/// </summary>
	/// <param name="args">Describes the packet to send</param>
	void OnOutgoingChatPacketReady( 
		__in Microsoft::Xbox::GameChat::ChatPacketEventArgs^ args 
		);

	void OnUserAudioDeviceAdded( Windows::Xbox::System::AudioDeviceAddedEventArgs^ eventArgs);
	void OnUserRemoved(Windows::Xbox::System::UserRemovedEventArgs^ eventArgs);


	/// <summary>
	/// Helper function to get specific ChatUser by xboxUserId
	/// </summary>
	bool CompareUniqueConsoleIdentifiers( 
		__in Platform::Object^ uniqueRemoteConsoleIdentifier1, 
		__in Platform::Object^ uniqueRemoteConsoleIdentifier2 
		);

	Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::GameChat::ChatUser^>^ GetChatUsers();

	//
	//  Notice: the userid will be gametag for display purpose
	Microsoft::Xbox::GameChat::ChatUser^ GetChatUserByXboxUserId(
		__in MabString& xboxUserId
		);
	/// <summary>
	/// Helper function to get specific ChatUser by xboxUserId
	/// </summary>
	Microsoft::Xbox::GameChat::ChatUser^ GetChatUserByXboxUserId(
		__in Platform::String^ xboxUserId
		);

	MabNodeId GetMabNodeIDByXboxUserId(MabString& xboxuserid);

private:
	Concurrency::critical_section m_lock;
	Microsoft::Xbox::GameChat::ChatManager^ m_chatManager;
	Windows::Foundation::EventRegistrationToken m_tokenOnDebugMessage;
	Windows::Foundation::EventRegistrationToken m_tokenOnOutgoingChatPacketReady;
	Windows::Foundation::EventRegistrationToken m_tokenOnCompareUniqueConsoleIdentifiers;
	Windows::Foundation::EventRegistrationToken m_tokenResourceAvailabilityChanged;
	Windows::Foundation::EventRegistrationToken m_tokenOnPreEncodeAudioBuffer;
	Windows::Foundation::EventRegistrationToken m_tokenOnPostDecodeAudioBuffer;

	MABMEM_HEAP heap;
	MabNetworkManager* network_manager;
	// Remembers the status of the session
	bool session_started;

	bool m_remoteMuted;
	// voice data waiting to be sent for each client
	typedef MabMap< MabNodeId, RemoteClientUserData > RemoteClientDataMap;
	RemoteClientDataMap remote_client_map;

};	


#endif