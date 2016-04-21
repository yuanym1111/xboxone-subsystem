#ifndef XBOXONE_MAB_CONNECTED_STORAGE_MANAGER_H
#define XBOXONE_MAB_CONNECTED_STORAGE_MANAGER_H

#pragma once

#include "XboxOneMabSingleton.h"

struct XBOXONE_LIVE_FILE_IO_BUFFER{
public:
	XBOXONE_LIVE_FILE_IO_BUFFER();
	~XBOXONE_LIVE_FILE_IO_BUFFER();

	std::wstring fileName;
	void* raw_buffer;
	UINT32 buffer_size;
	bool exist;

	void clear();
};

#define RC3_XBOXLIVE_CONTAINER_NAME L"RUGBY_CHALLENGE_ONLINE_STORAGE"

class XboxOneMabConnectedStorageManager:public XboxOneMabSingleton<XboxOneMabConnectedStorageManager>
{
public:

	void Initialize();
	void Refresh();
	bool WriteFileToConnectedStorage(const char* file_name, void* buffer, UINT32 size);
	bool DeleteFileFromConnectedStorage(const char* file_name);
	bool ReadFileFromConnectedStorage(const char* file_name, void* buffer, UINT32 size, UINT32 &read_size);
	bool CheckFileExistenceInConnectedStorage(const char* file_name, bool &exist);
	bool ListAllBlobsInConnectedStorage();
	void SyncOnDemand();

	void OnLoadBlobComplete();
	XBOXONE_LIVE_FILE_IO_BUFFER m_fileBuffer;

	unsigned int GetBlobSize(std::string name) { return m_blob_size_map[name];}
private:
    Windows::Xbox::Storage::ConnectedStorageSpace^ m_userSaveSpace;
    std::map <std::string, Windows::Xbox::Storage::ConnectedStorageContainer^> m_ContainerMap;
    Windows::Xbox::Storage::ConnectedStorageContainer^ m_fileContainer;
	bool m_currentOperationProcessing;
	bool m_currentOperationProcessing_2nd;
	bool m_currentOperationProcessing_3rd;
    Concurrency::critical_section m_lock;

	unsigned int m_blob_size;
	std::map <std::string, unsigned int> m_blob_size_map;
};
#endif