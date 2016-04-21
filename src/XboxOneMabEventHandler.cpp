#include "pch.h"

#include "XboxOneMabEventHandler.h"

using namespace Windows::Foundation;
using namespace Windows::Xbox::Networking;
using namespace Windows::Networking::Connectivity;
using namespace Concurrency;
using namespace XboxOneMabLib;

namespace XboxOneMabLib {


	GameNetworkMessage::GameNetworkMessage()
	{
		_type = GameMessageType::Unknown;
		_data = nullptr;
	}

	GameNetworkMessage::~GameNetworkMessage()
	{
	}

	GameNetworkMessage::GameNetworkMessage(GameMessageType type, unsigned data)
	{
		_type = type;
		_data = ref new Platform::Array<unsigned char>(reinterpret_cast<unsigned char *>(&data), sizeof(data));
	}

	GameNetworkMessage::GameNetworkMessage(GameMessageType type, Platform::String^ data)
	{
		_type = type;
		_data = ref new Platform::Array<unsigned char>(data->Length() * sizeof(wchar_t));
		CopyMemory(_data->Data, data->Data(), _data->Length);
	}

	GameNetworkMessage::GameNetworkMessage(GameMessageType type, Platform::Array<unsigned char>^ data)
	{
		_type = type;
		_data = data;
	}

	GameNetworkMessage::GameNetworkMessage(const Platform::Array<unsigned char>^ data)
	{
		if (data->Length < (sizeof(GameMessageType) + sizeof(unsigned char)))
		{
			throw Platform::Exception::CreateException(E_INVALID_PROTOCOL_FORMAT, L"Invalid message length");
		}

		CopyMemory(&_type, data->Data, sizeof(GameMessageType));

		_data = ref new Platform::Array<unsigned char>(
			reinterpret_cast<unsigned char *>(data->Data + sizeof(GameMessageType)),
			data->Length - sizeof(GameMessageType)
			);
	}

	Platform::Array<unsigned char>^ 
		GameNetworkMessage::Serialize()
	{
		if (_type == GameMessageType::Unknown || _data == nullptr || _data->Length == 0)
		{
			throw Platform::Exception::CreateException(E_INVALID_PROTOCOL_FORMAT, L"Invalid GameMessage");
		}

		unsigned length = sizeof(GameMessageType) + _data->Length;
		unsigned char *buffer = new unsigned char[length];

		CopyMemory(buffer, &_type, sizeof(GameMessageType));
		CopyMemory(buffer + sizeof(GameMessageType), _data->Data, _data->Length);

		Platform::Array<unsigned char>^ packet = ref new Platform::Array<unsigned char>(buffer, length);

		delete [] buffer;

		return packet;
	}

	Platform::String^
		GameNetworkMessage::StringValue::get()
	{
		if (_data != nullptr && _data->Length > 0)
		{
			return ref new Platform::String(reinterpret_cast<wchar_t *>(_data->Data), _data->Length / sizeof(wchar_t));
		}

		return nullptr;
	}

	unsigned
		GameNetworkMessage::UnsignedValue::get()
	{
		if (_data != nullptr && _data->Length == sizeof(unsigned))
		{
			return *(reinterpret_cast<unsigned *>(_data->Data));
		}

		return 0;
	}


}
