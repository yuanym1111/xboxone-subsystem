#ifndef XBOX_ONE_MAB_CONNECT_H
#define XBOX_ONE_MAB_CONNECT_H

//#include "xboxone/XboxOneMabNetTypes.h"
#include "XboxOneMabSingleton.h"
#include "xboxone/XboxOneMabNetTypes.h"

class MabNetAddress;


namespace XboxOneMabLib {
// ----------------------------------------------------------------------------
// Definies
// ----------------------------------------------------------------------------

#define SECURE_DEVICE_ASSOCIATION_TEMPLATE_TCP            "TestTrafficTcp"
#define SECURE_DEVICE_ASSOCIATION_TEMPLATE_UDP            "TestTrafficUdp"
#define TEST_MESSAGE_SERVER                               "Message from Server"
#define TEST_MESSAGE_CLIENT                               "Message from Client"
#define BUFFER_SIZE                                       1024

ref class XboxOneMabConnectManager;

public enum class ConnectionStatus
{
	Disconnected,
	Pending,
	Connected,
	PostHandshake
};

public ref class XboxOneMabConnect sealed
{

internal:
	XboxOneMabConnect(Windows::Xbox::Networking::SecureDeviceAddress^ secureDeviceAddress, XboxOneMabConnectManager^ manager);

public:
	void Initialize();
	void GetPeersAddressesFromGameSessionAndConnect(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ multiplayerSession);



	uint8 GetConsoleId();
	void SetConsoleId(uint8 consoleId);

	Platform::String^ GetConsoleName();
	void SetConsoleName(Platform::String^ consoleName);

	Platform::Object^ GetCustomProperty();
	void SetCustomProperty(Platform::Object^ object);

	int GetNumberOfRetryAttempts();
	void SetNumberOfRetryAttempts(int retryAttempt);

	ConnectionStatus GetConnectionStatus();
	void SetConnectionStatus(ConnectionStatus status);

	bool GetAssocationFoundInTemplate();
	void SetAssocationFoundInTemplate(bool bFound);

	Windows::Xbox::Networking::SecureDeviceAssociation^ GetAssociation();
	void SetAssociation(Windows::Xbox::Networking::SecureDeviceAssociation^ association);

	bool IsInComingAssociation();
	void SetInComingAssociation(bool val);

	bool IsConnectionDestroying();
	void SetConnectionDestroying(bool val);

	bool IsConnectionInProgress();
	void SetConnectionInProgress(bool val);

	void SetHeartTimer( float timer );
	float GetHeartTimer();

	void CreateAndUsePeerAssociation(Windows::Xbox::Networking::SecureDeviceAddress^ peerAddress, Platform::String^ xboxUserId);

	Windows::Xbox::Networking::SecureDeviceAddress^ XboxOneMabConnect::GetSecureDeviceAddress();

private:

	Concurrency::critical_section m_stateLock;

	Windows::Xbox::Networking::SecureDeviceAddress^ m_secureDeviceAddress;
	Windows::Xbox::Networking::SecureDeviceAssociation^ m_association;
	Windows::Foundation::EventRegistrationToken m_associationStateChangeToken;

	Platform::WeakReference m_ConnectManager; // weak ref to XboxOneConnectManager^ 

	uint8 m_consoleId;
	Platform::String^ m_consoleName;
	Platform::Object^ m_customProperty;
	XboxOneMabConnect::ConnectionStatus m_connectionStatus;
	bool m_isInComingAssociation;
	bool m_isConnectionInProgress;
	bool m_isConnectionDestroying;
	bool m_assocationFoundInTemplate;
	int m_retryAttempts;
	float m_timerSinceLastAttempt;
	float m_heartTimer;

	XboxOneMabNetOnlineClient* m_onlineclient;

	void HandleAssociationChangedEvent(
		Windows::Xbox::Networking::SecureDeviceAssociation^ association, 
		Windows::Xbox::Networking::SecureDeviceAssociationStateChangedEventArgs^ args
		);

	void addOrUpdatePeer(XboxOneMabNetOnlineClient* peer);

	//Should moved to ConnectManager
	Windows::Xbox::Networking::SecureDeviceAssociationTemplate^   m_peerDeviceAssociationTemplate;
};

}
#endif