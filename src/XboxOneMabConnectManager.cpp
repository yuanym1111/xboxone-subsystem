#include "pch.h"

#include "XboxOneMabConnectManager.h"
#include "XboxOneMabConnect.h"
#include "XboxOneMabUtils.h"

using namespace Concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::Networking;

using namespace XboxOneMabLib;

namespace XboxOneMabLib {

XboxOneMabConnectManager::XboxOneMabConnectManager(uint8 localConsoleId, Platform::String^ secureDeviceAssociationTemplateName, Platform::String^ localConsoleName):
	m_localConsoleName(localConsoleName)
{
	m_associationTemplate = Windows::Xbox::Networking::SecureDeviceAssociationTemplate::GetTemplateByName( secureDeviceAssociationTemplateName );

	Initialize(localConsoleId);
//	RegisterMeshPacketEventHandlers();
}



void XboxOneMabConnectManager::Initialize(uint8 localConsoleId)
{
	MABUNUSED(localConsoleId);
	m_connections.clear();

	if(m_associationTemplate != nullptr)
	{
		DestroyAllTemplateAssociations();

		// Listen to AssociationIncoming event
		TypedEventHandler<Windows::Xbox::Networking::SecureDeviceAssociationTemplate^, Windows::Xbox::Networking::SecureDeviceAssociationIncomingEventArgs^>^ associationIncomingEvent = 
			ref new TypedEventHandler<Windows::Xbox::Networking::SecureDeviceAssociationTemplate^, Windows::Xbox::Networking::SecureDeviceAssociationIncomingEventArgs^>(
			[this] (Windows::Xbox::Networking::SecureDeviceAssociationTemplate^ associationTemplate, Windows::Xbox::Networking::SecureDeviceAssociationIncomingEventArgs^ args)
		{
			OnAssociationIncoming( associationTemplate, args );
		});
		m_associationIncomingToken = m_associationTemplate->AssociationIncoming += associationIncomingEvent;
	}
	else
	{
		XboxOneMabUtils::LogComment("Association template is NULL!");
		return;
	}

	unsigned short sin6_port = htons(m_associationTemplate->AcceptorSocketDescription->BoundPortRangeLower);
	MABUNUSED(sin6_port);
	//
	//Todo: Register UDP Socket Layer here
}

Platform::String^ XboxOneMabConnectManager::GetLocalConsoleDisplayName()
{
	return XboxOneMabUtils::FormatString(L"%s ", GetLocalConsoleName()->Data());
}

Platform::String^ XboxOneMabConnectManager::GetLocalConsoleName()
{
	Platform::String^ localConsoleName = L"Console";
	{
		Concurrency::critical_section::scoped_lock lock(m_connectionsLock);
		localConsoleName = m_localConsoleName;
	}

	return localConsoleName;
}

void XboxOneMabConnectManager::SetLocalConsoleName(Platform::String^ consoleName)
{
	Concurrency::critical_section::scoped_lock lock(m_connectionsLock);
	m_localConsoleName = consoleName;
}

Windows::Xbox::Networking::SecureDeviceAssociationTemplate^ XboxOneMabConnectManager::GetSecureDeviceAssociationTemplate()
{
	return m_associationTemplate;
}


void XboxOneMabConnectManager::OnAssociationChange( Windows::Xbox::Networking::SecureDeviceAssociationStateChangedEventArgs^ args, Windows::Xbox::Networking::SecureDeviceAssociation^ association )
{
	// Update this mesh member with the latest information
	switch(args->NewState)
	{
	case Windows::Xbox::Networking::SecureDeviceAssociationState::DestroyingLocal:
	case Windows::Xbox::Networking::SecureDeviceAssociationState::DestroyingRemote:
	case Windows::Xbox::Networking::SecureDeviceAssociationState::Invalid:
		{
			bool bFound = false;
			{
				
				XboxOneMabConnect^ connection = GetConnectionFromAssociation(association);
				if(connection != nullptr)
				{
					bFound = true;
					// let the user know that this connection is being disconnected.
					//LogCommentFormat( L"Remote console is disconnecting %s", meshConnection->GetConsoleName()->Data() );
					//LogComment( Utils::GetThreadDescription(L"THREAD: Remote console disconnecting") );

					connection->SetConnectionDestroying(true);
					connection->SetConnectionStatus(ConnectionStatus::Disconnected);
//					OnDisconnected(this, connection);
				}
				
			}

			if (!bFound)
			{
				//LogComment(L"Association Change event fired for the wrong association.");
			}
		}
	default:
		break;
	}
}

XboxOneMabConnect^ XboxOneMabConnectManager::GetConnectionFromAssociation(Windows::Xbox::Networking::SecureDeviceAssociation^ association)
{
	if(association == nullptr)
	{
		return nullptr;
	}

	SecureDeviceAddress^ remoteSecureDeviceAddress = association->RemoteSecureDeviceAddress;
	return GetConnectionFromSecureDeviceAddress(remoteSecureDeviceAddress);
}

XboxOneMabConnect^ XboxOneMabConnectManager::GetConnectionFromSecureDeviceAddress(Windows::Xbox::Networking::SecureDeviceAddress^ address)
{
	if(address == nullptr)
	{
		return nullptr;
	}

	Windows::Foundation::Collections::IVectorView<XboxOneMabConnect^>^ allConnections = GetConnections();
	for each (XboxOneMabConnect^ meshConnection in allConnections)
	{
		SecureDeviceAddress^ remoteSecureDeviceAddress = meshConnection->GetSecureDeviceAddress();
		if(AreSecureDeviceAddressesEqual(remoteSecureDeviceAddress, address))
		{
			return meshConnection;
		}
	}
	return nullptr;
}

bool XboxOneMabConnectManager::AreSecureDeviceAddressesEqual( Windows::Xbox::Networking::SecureDeviceAddress^ secureDeviceAddress1, Windows::Xbox::Networking::SecureDeviceAddress^ secureDeviceAddress2 )
{
	if( secureDeviceAddress1 != nullptr && 
		secureDeviceAddress2 != nullptr &&
		secureDeviceAddress1->Compare(secureDeviceAddress2) == 0 )
	{
		return true;
	}
	return false;
}

XboxOneMabConnect^ XboxOneMabConnectManager::GetConnectionFromConsoleId(uint8 consoleId)
{
	Windows::Foundation::Collections::IVectorView<XboxOneMabConnect^>^ allConnections = GetConnections();
	for each (XboxOneMabConnect^ meshConnection in allConnections)
	{
		uint8 meshConsoleId = meshConnection->GetConsoleId();
		if( meshConsoleId == consoleId )
		{
			return meshConnection;
		}
	}
	return nullptr;
}

void XboxOneMabConnectManager::DeleteConnection(XboxOneMabConnect^ connection)
{
	if (connection == nullptr)
	{
		XboxOneMabUtils::LogComment(L"Cannot pass a nullptr for the member to DeleteConnection");
		throw ref new InvalidArgumentException(L"Cannot pass a nullptr for the member to DeleteConnection");
	}

	bool found = false;
	{
		Concurrency::critical_section::scoped_lock lock(m_connectionsLock);

		auto iter = m_connections.begin();
		for( ; iter != m_connections.end(); iter++ )
		{
			if ((*iter) == connection)
			{
				found = true;
				break;
			}
		}

		if (found)
		{
			m_connections.erase(iter);
		}
	}
}

void XboxOneMabConnectManager::DestroyConnection( XboxOneMabConnect^ connection )
{
	if(connection == nullptr)
	{
		return;
	}

	SecureDeviceAssociation^ meshAssociation = connection->GetAssociation();
	if(meshAssociation != nullptr)
	{
		if(false == connection->IsConnectionDestroying())
		{
			connection->SetConnectionDestroying(true);
			IAsyncAction^ asyncOp = meshAssociation->DestroyAsync();
			create_task(asyncOp)
				.then([this] (task<void> t)
			{
				try
				{
					t.get(); // if t.get fails, it will throw.
				}
				catch (Platform::COMException^ ex)
				{
					XboxOneMabUtils::LogCommentFormat( L"MeshManager::DestroyConnection - DestroyAsync failed %s", XboxOneMabUtils::GetErrorString(ex->HResult)->Data());
				}
			}).wait();
		}
		connection->SetAssociation(nullptr);
	}

	DeleteConnection(connection);
}

void XboxOneMabConnectManager::DisconectFromAddress( Windows::Xbox::Networking::SecureDeviceAddress^ address )
{
	XboxOneMabConnect^ connection = GetConnectionFromSecureDeviceAddress(address);
	DestroyConnection(connection);
}

void XboxOneMabConnectManager::DestroyAndDisconnectAll()
{
	Windows::Foundation::Collections::IVectorView<XboxOneMabConnect^>^ allConnections = GetConnections();
	for each (XboxOneMabConnect^ meshConnection in allConnections)
	{
		DestroyConnection(meshConnection);
	}

	{
		Concurrency::critical_section::scoped_lock lock(m_connectionsLock);
		m_connections.clear();
	}

}

void XboxOneMabConnectManager::DestroyAllTemplateAssociations()
{
	if(GetSecureDeviceAssociationTemplate() != nullptr)
	{
		Windows::Foundation::Collections::IVectorView<Windows::Xbox::Networking::SecureDeviceAssociation^>^ associations = GetSecureDeviceAssociationTemplate()->Associations;
		if(associations == nullptr || associations->Size == 0)
		{
			return;
		}

		XboxOneMabUtils::LogCommentFormat(L"We have %d associations on ConnectManager::Initialize", associations->Size );
		for each (Windows::Xbox::Networking::SecureDeviceAssociation^ associationInTemplate in associations)
		{
			IAsyncAction^ asyncOp = associationInTemplate->DestroyAsync();
			create_task(asyncOp)
				.then([this] (task<void> t)
			{
				try
				{
					t.get(); // if t.get fails, it will throw.
				}
				catch (Platform::COMException^ ex)
				{
					XboxOneMabUtils::LogCommentFormat( L"ConnectManager::DestroyAllTemplateAssociations - DestroyAsync failed %s", XboxOneMabUtils::GetErrorString(ex->HResult)->Data());
				}
			}).wait();
		}
	}
}

void XboxOneMabConnectManager::Shutdown()
{
	XboxOneMabUtils::LogComment( L"MeshManager::Shutdown");

	
	if( m_associationTemplate != nullptr )
	{
		m_associationTemplate->AssociationIncoming -= m_associationIncomingToken;
		m_associationTemplate = nullptr;
	}

	//Other subsystem should be closed here
}

void XboxOneMabConnectManager::ConnectToAddress(Windows::Xbox::Networking::SecureDeviceAddress^ secureDeviceAddress, Platform::String^ debugName)
{
	if (secureDeviceAddress == nullptr)
	{
		XboxOneMabUtils::LogComment(L"Cannot pass a nullptr for the address to MeshManager::ConnectTo");
		throw ref new InvalidArgumentException(L"Cannot pass a nullptr for the address to MeshManager::ConnectTo");
	}

	if(AreSecureDeviceAddressesEqual(SecureDeviceAddress::GetLocal(), secureDeviceAddress))
	{
		// Don't try to connect to local console.
		return;
	}

	XboxOneMabUtils::LogComment( "ConnectToAddress: Attempting to connect to remote console: " + debugName);
	XboxOneMabConnect^ newConnected = AddConnection(secureDeviceAddress, nullptr, false, ConnectionStatus::Disconnected);
	if( newConnected != nullptr )
	{
		// If this connection was new, then set the console name to be debug name.  It will change once the handshake is done
		newConnected->SetConsoleName(debugName);
	}
}

Windows::Foundation::Collections::IVectorView<XboxOneMabConnect^>^ XboxOneMabConnectManager::GetConnections()
{
	Concurrency::critical_section::scoped_lock lock(m_connectionsLock);
	auto v = ref new Platform::Collections::Vector<XboxOneMabConnect^>(m_connections);
	return v->GetView();
}

Windows::Foundation::Collections::IVectorView<XboxOneMabConnect^>^ XboxOneMabConnectManager::GetConnectionsByType(ConnectionStatus type)
{
	auto associationsByType = ref new Platform::Collections::Vector<XboxOneMabConnect^>();

	Windows::Foundation::Collections::IVectorView<XboxOneMabConnect^>^ allConnections = GetConnections();
	for each (XboxOneMabConnect^ meshConnection in allConnections)
	{
		if( meshConnection->GetConnectionStatus() == type )
		{
			associationsByType->Append(meshConnection);
		}
	}
	return associationsByType->GetView();
}

XboxOneMabConnect^ XboxOneMabConnectManager::AddConnection(Windows::Xbox::Networking::SecureDeviceAddress^ secureDeviceAddress, Windows::Xbox::Networking::SecureDeviceAssociation^ secureDeviceAssociation, bool inComingAssociation, ConnectionStatus connectionStatus)
{
	if(secureDeviceAddress == nullptr)
	{
		return nullptr;
	}

	if(GetConnectionFromSecureDeviceAddress(secureDeviceAddress) != nullptr)
	{
		// The secure device address already exists.
		return nullptr;
	}

	XboxOneMabConnect^ mewConnection = ref new XboxOneMabConnect(secureDeviceAddress, this);
	mewConnection->SetInComingAssociation(inComingAssociation);
	mewConnection->SetConnectionStatus(connectionStatus);
	mewConnection->SetAssociation(secureDeviceAssociation);

	// Add him to the list of mesh connections.
	{
		Concurrency::critical_section::scoped_lock lock(m_connectionsLock);
		m_connections.push_back(mewConnection);
	}

	return mewConnection;
}


void XboxOneMabConnectManager::OnAssociationIncoming(Windows::Xbox::Networking::SecureDeviceAssociationTemplate^ associationTemplate, Windows::Xbox::Networking::SecureDeviceAssociationIncomingEventArgs^ args)
{

}

}