#ifndef XBOX_ONE_MAB_CONNECT_MANAGER_H
#define XBOX_ONE_MAB_CONNECT_MANAGER_H

#include "XboxOneMabConnect.h"
#include "XboxOneMabSingleton.h"
#include "xboxone/MabSecureSocketUDP.h"

class XboxOneMabNetOnlineClient;
class MabNetworkManager;

namespace XboxOneMabLib {

public ref class XboxOneMabConnectManager sealed
{
internal:
	XboxOneMabConnectManager(
		uint8 localConsoleId, 
		Platform::String^ secureDeviceAssociationTemplateName, 
		Platform::String^ localConsoleName
	);
public:
	
	Platform::String^ GetLocalConsoleDisplayName();
	Platform::String^ GetLocalConsoleName();
	void SetLocalConsoleName(Platform::String^ consoleName);


	void OnAssociationChange( 
		Windows::Xbox::Networking::SecureDeviceAssociationStateChangedEventArgs^ args, 
		Windows::Xbox::Networking::SecureDeviceAssociation^ association );

	Windows::Xbox::Networking::SecureDeviceAssociationTemplate^ GetSecureDeviceAssociationTemplate();

	void ConnectToAddress(Windows::Xbox::Networking::SecureDeviceAddress^ secureDeviceAddress, Platform::String^ debugName );


	Windows::Foundation::Collections::IVectorView<XboxOneMabConnect^>^ GetConnections();

	Windows::Foundation::Collections::IVectorView<XboxOneMabConnect^>^ GetConnectionsByType(ConnectionStatus type);

	XboxOneMabConnect^ GetConnectionFromAssociation(Windows::Xbox::Networking::SecureDeviceAssociation^ association);

	XboxOneMabConnect^ GetConnectionFromSecureDeviceAddress(Windows::Xbox::Networking::SecureDeviceAddress^ address);

	XboxOneMabConnect^ GetConnectionFromConsoleId(uint8 consoleId);

	void DestroyConnection( XboxOneMabConnect^ connection); 

	void DisconectFromAddress( Windows::Xbox::Networking::SecureDeviceAddress^ address); 

	void DestroyAndDisconnectAll(); 

	void Shutdown();

	//Event handlers





private:
	Concurrency::critical_section m_connectionsLock;

	Platform::String^ m_localConsoleName;
	Windows::Xbox::Networking::SecureDeviceAssociationTemplate^ m_associationTemplate;
	std::vector<XboxOneMabConnect^> m_connections;



	void Initialize(uint8 localConsoleId);

	XboxOneMabConnect^ AddConnection( 
		Windows::Xbox::Networking::SecureDeviceAddress^ secureDeviceAddress,
		Windows::Xbox::Networking::SecureDeviceAssociation^ secureDeviceAssociation,
		bool inComingAssociation,
		ConnectionStatus connectionStatus    
		);

	bool AreSecureDeviceAddressesEqual(
		Windows::Xbox::Networking::SecureDeviceAddress^ secureDeviceAddress1,
		Windows::Xbox::Networking::SecureDeviceAddress^ secureDeviceAddress2
		);

	void DeleteConnection(XboxOneMabConnect^ connection);
	void DestroyAllTemplateAssociations();
	bool DoesConnectionExistInList(Windows::Foundation::Collections::IVectorView<XboxOneMabConnect^>^ list, XboxOneMabConnect^ connection);

	//Event for register

	Windows::Foundation::EventRegistrationToken m_associationIncomingToken;


	void OnAssociationIncoming( 
		Windows::Xbox::Networking::SecureDeviceAssociationTemplate^ associationTemplate, 
		Windows::Xbox::Networking::SecureDeviceAssociationIncomingEventArgs^ args 
		);

};

}
#endif