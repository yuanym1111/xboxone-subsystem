#ifndef XBOX_ONE_MAB_MULTIPLAYER_MANAGER_H
#define XBOX_ONE_MAB_MULTIPLAYER_MANAGER_H

#include "XboxOneMabSingleton.h"

using namespace Windows::Xbox::Multiplayer;

namespace XboxOneMabLib {
	
public enum class MatchmakingState
{
	None,
	Searching,
	Found,
	Completed,
	Failed
};

public enum class MatchmakingResult
{
	MatchFound,
	LookInProgress,
	MatchFailed,
	MatchRefreshing
};

public enum class SessionState 
{
	HostTokenChanged,
	Disconnect,
	MembersChanged,
	JoinabilityChanged,
	MemberLeft,
	None
};

public enum class HostMigrationResult
{
	MigrationSuccess,
	MigrateFailed,
	NoAction,
	SessionLeft
};

public enum class SessionManagerState
{
	NotStarted,
	Running,
	Matchmaking
};

public ref class HostMigrationResultArgs sealed
{
public:
	HostMigrationResultArgs(HostMigrationResult result) :
		_result(result)
	{
	}

	property HostMigrationResult Result
	{
		HostMigrationResult get() { return _result; }
	}

private:
	HostMigrationResult _result;
};


public ref class MatchmakingArgs sealed
{
public:
	MatchmakingArgs(MatchmakingResult result, Platform::String^ errorString):
		_errorString(errorString),
		_result(result)
	{
	}

	MatchmakingArgs(MatchmakingResult result):
		_errorString(L""),
		_result(result)
	{
	}

public:

	property MatchmakingResult Result
	{
		MatchmakingResult get() { return _result; }
	}

	property Platform::String^ ErrorString
	{
		Platform::String^ get() { return _errorString; }
	}

private:
	Platform::String^   _errorString;
	MatchmakingResult   _result;
};




public ref class MultiplayerActivationInfo sealed
{
public:
	MultiplayerActivationInfo(Platform::String^ handleId, Platform::String^ targetXuid)
	{
		HandleId = handleId;
		TargetXuid = targetXuid;
	}

	MultiplayerActivationInfo(Platform::String^ handleId, Platform::String^ targetXuid, Platform::String^ joineeXuid)
	{
		HandleId = handleId;
		TargetXuid = targetXuid;
		JoineeXuid = joineeXuid;
	}

	property Platform::String^ JoineeXuid
	{
		Platform::String^ get() { return m_joineeXuid; }
		void set(Platform::String^ value) { m_joineeXuid = value; }
	}

	property Platform::String^ HandleId
	{
		Platform::String^ get() { return m_handleId; }
		void set(Platform::String^ value) { m_handleId = value; }
	}

	property Platform::String^ TargetXuid
	{
		Platform::String^ get() { return m_targetXuid; }
		void set(Platform::String^ value) 
		{ 
			m_targetXuid = value;
			m_targetUser = nullptr;
			
			auto users = Windows::Xbox::System::User::Users;
			for ( UINT i = 0; i < users->Size; ++i )
			{
				auto pUser = users->GetAt( i );
				if (pUser->XboxUserId->Equals(m_targetXuid))
				{
					m_targetUser = pUser;
					break;
				}
			}  
			
		}
	}

	property Windows::Xbox::System::User^ TargetUser
	{
		Windows::Xbox::System::User^ get() { return m_targetUser; }
	}

private:
	Platform::String^            m_handleId;
	Platform::String^            m_targetXuid;
	Platform::String^            m_joineeXuid;
	Windows::Xbox::System::User^ m_targetUser;
};

public enum class GameMessageType
{
	Unknown,
	GameStart,
	GameTurn,
	GameOver,
	GameLeft,
	LobbyUpdate,
	PowerUpSpawn
};

public ref class GameNetworkMessage sealed
{
internal:
	GameNetworkMessage();
	GameNetworkMessage(GameMessageType type, unsigned data);
	GameNetworkMessage(GameMessageType type, Platform::String^ data);
	GameNetworkMessage(GameMessageType type, Platform::Array<unsigned char>^ data);
	GameNetworkMessage(const Platform::Array<unsigned char>^ data);

	property GameMessageType MessageType
	{
		GameMessageType get() { return _type; }
		void set(GameMessageType type) { _type = type; }
	}

	property Platform::Array<unsigned char>^ RawData
	{
		Platform::Array<unsigned char>^ get() { return _data; }
		void set(Platform::Array<unsigned char>^ data) { _data = data; }
	}

	property Platform::String^ StringValue { Platform::String^ get(); }
	property unsigned UnsignedValue { unsigned get(); }

	Platform::Array<unsigned char>^ Serialize();

private:
	~GameNetworkMessage();

private:
	GameMessageType _type;
	Platform::Array<unsigned char>^ _data;
};


public ref class XboxOneMabSessionRef sealed : IMultiplayerSessionReference
{
public:

	XboxOneMabSessionRef(Platform::String^ _configurationID,Platform::String^ _sessionName,Platform::String^ _templateID){
		configurationID = _configurationID;
		sessionName = _sessionName;
		templateID = _templateID;
	}

	property Platform::String^ ServiceConfigurationId {
		virtual Platform::String^ get (){return configurationID;}
		void set(Platform::String^ data){configurationID = data;}
	}
	property Platform::String^ SessionName {
		virtual Platform::String^ get () { return sessionName;}
		void set(Platform::String^ data){sessionName = data;}
	}
	property Platform::String^ SessionTemplateName {
		virtual Platform::String^ get () { return templateID;}
		void set(Platform::String^ data){templateID = data;}
	}
private:
	~XboxOneMabSessionRef(){};

private:
	Platform::String^ configurationID;
	Platform::String^ sessionName;
	Platform::String^ templateID;

};


public enum class GameState
{
	Reset,
	Initialize,
	AcquireUser,
	VerifyPrivileges,
	MultiplayerMenus,
	CreateLobby,
	JoinLobby,
	ConfirmUserSwitch,
	Matchmaking,
	Lobby,
	MultiplayerGame,
	Suspended,
	leavingMultiplayer,
	AcceptInvitation
};

struct PeerChange
{
	PeerChange() :
		isAdded(true),
		xuid("")
	{}

	PeerChange(bool isAdded, Platform::String^ xuid) :
		isAdded(isAdded),
		xuid(xuid)
	{}

	bool isAdded;
	Platform::String^ xuid;
};

struct PeerMessage
{
	PeerMessage() :
		xuid(""),
		message(nullptr)
	{}

	PeerMessage(Platform::String^ xuid, GameNetworkMessage^ message) :
		xuid(xuid),
		message(message)
	{}

	Platform::String^ xuid;
	GameNetworkMessage^ message;
};
}

#endif