#ifndef XBOX_ONE_MAB_NETWORK_MANAGER_H
#define XBOX_ONE_MAB_NETWORK_MANAGER_H

enum class GameMessageType
{
	Unknown,
	GameStart,
	GameTurn,
	GameOver,
	GameLeft,
	LobbyUpdate,
	PowerUpSpawn,
	WorldSetup,
	WorldData,
	ShipSpawn,
	ShipInput,
	ShipData,
	ShipDeath
};

ref class GameNetworkMessage sealed
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

#endif