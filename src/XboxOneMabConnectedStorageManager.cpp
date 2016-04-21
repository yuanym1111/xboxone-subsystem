#include "pch.h"

#include "XboxOneMabConnectedStorageManager.h"
#include "XboxOneMabUserInfo.h"
#include "XboxOneMabWinRTWrapperForNativeBuffer.h"
#include <collection.h>
#include <ppltasks.h>
#include "XboxOneMabUtils.h"

using namespace Windows::Xbox::System;
using namespace Windows::Xbox::Storage;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox; 

XBOXONE_LIVE_FILE_IO_BUFFER::XBOXONE_LIVE_FILE_IO_BUFFER()
{
	raw_buffer = nullptr;
	buffer_size = 0;
}

XBOXONE_LIVE_FILE_IO_BUFFER::~XBOXONE_LIVE_FILE_IO_BUFFER()
{

}

void XBOXONE_LIVE_FILE_IO_BUFFER::clear()
{
	if(!fileName.empty())
		fileName.clear();
	raw_buffer = nullptr;
	buffer_size = 0;
	exist = false;
}

void XboxOneMabConnectedStorageManager::Initialize()
{
	m_fileBuffer = XBOXONE_LIVE_FILE_IO_BUFFER();
	m_currentOperationProcessing = false;
	m_blob_size = 0;
	m_blob_size_map.clear();
}

void XboxOneMabConnectedStorageManager::Refresh()
{
	User^ current_user = XboxOneUser->GetCurrentUser();
	if(current_user == nullptr)
		return;
	auto asyncOp = Windows::Xbox::Storage::ConnectedStorageSpace::GetForUserAsync( current_user );
    asyncOp->Completed = ref new Windows::Foundation::AsyncOperationCompletedHandler<Windows::Xbox::Storage::ConnectedStorageSpace^>( [this] ( Windows::Foundation::IAsyncOperation<Windows::Xbox::Storage::ConnectedStorageSpace^>^ operation, Windows::Foundation::AsyncStatus status )
    {
        try
        {
            if ( status == Windows::Foundation::AsyncStatus::Completed )
            {
                m_userSaveSpace = operation->GetResults(); 
               // m_gameStateContainer = m_userSaveSpace->CreateContainer( SAVE_GAME_CONTAINER_NAME );
                //StartLoadingPreviousGameSessionState();
            }
            else
            {
                XboxOneMabUtils::LogCommentFormat( L"\n ConnectedStorageSpace::GetForUserAsync has failed, Error code = 0x%x", operation->ErrorCode);
            }
        }
        catch( Platform::Exception^ ex )
        {
            //m_gameState = GameState::GAMESTATE_SHOW_ERROR_SCREEN;
            XboxOneMabUtils::LogCommentFormat( L"\n Exception thrown by ConnectedStorageSpace::GetForUserAsync; Error code = 0x%x, Message = %s", ex->HResult, ex->Message );
        }
	});
}

bool XboxOneMabConnectedStorageManager::WriteFileToConnectedStorage(const char* file_name, void* buffer, UINT32 size)
{
    Concurrency::critical_section::scoped_lock lock(m_lock);

	User^ current_user = XboxOneUser->GetCurrentUser();
	if(current_user == nullptr)
		return false;
	auto asyncOp = Windows::Xbox::Storage::ConnectedStorageSpace::GetForUserAsync( current_user );

	XboxOneMabUtils::LogComment(L"Current user is " + current_user->DisplayInfo->Gamertag);
	m_fileBuffer.clear();
	std::string s_str = std::string(file_name);
	std::wstring w_str = std::wstring(s_str.begin(), s_str.end());
	m_fileBuffer.fileName = std::wstring(s_str.begin(), s_str.end());
	m_fileBuffer.raw_buffer = buffer;
	m_fileBuffer.buffer_size = size;
	m_currentOperationProcessing = true;

	//MABLOGDEBUG("blob size is %d, %s,%d", m_blob_size_map[s_str], __FILE__, __LINE__);

    asyncOp->Completed = ref new Windows::Foundation::AsyncOperationCompletedHandler<Windows::Xbox::Storage::ConnectedStorageSpace^>( [this, s_str] ( Windows::Foundation::IAsyncOperation<Windows::Xbox::Storage::ConnectedStorageSpace^>^ operation, Windows::Foundation::AsyncStatus status )
    {
        try
        {
            if ( status == Windows::Foundation::AsyncStatus::Completed )
            {
                m_userSaveSpace = operation->GetResults(); 
                m_fileContainer = m_userSaveSpace->CreateContainer( RC3_XBOXLIVE_CONTAINER_NAME );
				if (m_fileContainer != nullptr) 
				{
					auto blobsToUpdate = ref new Platform::Collections::Map<Platform::String^,  Windows::Storage::Streams::IBuffer^>();
					Windows::Storage::Streams::IBuffer^ fileWriteBuffer =  RUGBY_CHALLENGE_XBOXONE_MAB::WinRTWrapperForNativeBuffer::Create( (BYTE *)(m_fileBuffer.raw_buffer), m_fileBuffer.buffer_size ); 
					Platform::String^ buffer_name = ref new Platform::String(m_fileBuffer.fileName.c_str());
					blobsToUpdate->Insert( buffer_name, fileWriteBuffer );

					auto asyncOp = m_fileContainer->SubmitUpdatesAsync( blobsToUpdate->GetView(), nullptr ); 

					asyncOp->Completed = ref new Windows::Foundation::AsyncActionCompletedHandler( [this, s_str] ( Windows::Foundation::IAsyncAction^ operation, Windows::Foundation::AsyncStatus status ) 
					{
						//On file saved
						if (status == Windows::Foundation::AsyncStatus::Completed)
						{
							m_blob_size = m_fileBuffer.buffer_size;
							m_blob_size_map[s_str] =  m_blob_size;
						}
						m_currentOperationProcessing = false;

					});
				}

            }

        }
        catch( Platform::Exception^ ex )
        {
            //m_gameState = GameState::GAMESTATE_SHOW_ERROR_SCREEN;
            XboxOneMabUtils::LogCommentFormat( L"\n Exception thrown by ConnectedStorageSpace::GetForUserAsync; Error code = 0x%x, Message = %s", ex->HResult, ex->Message );
        }
	});

	while(m_currentOperationProcessing) {
		SwitchToThread();
	}
	return true;
}

bool XboxOneMabConnectedStorageManager::DeleteFileFromConnectedStorage(const char* file_name)
{
    Concurrency::critical_section::scoped_lock lock(m_lock);

	User^ current_user = XboxOneUser->GetCurrentUser();
	if(current_user == nullptr)
		return false;
	auto asyncOp = Windows::Xbox::Storage::ConnectedStorageSpace::GetForUserAsync( current_user );

	XboxOneMabUtils::LogComment(L"Current user is " + current_user->DisplayInfo->Gamertag);
	m_fileBuffer.clear();
	std::string s_str = std::string(file_name);
	std::wstring w_str = std::wstring(s_str.begin(), s_str.end());
	m_fileBuffer.fileName = std::wstring(s_str.begin(), s_str.end());
	m_fileBuffer.raw_buffer = nullptr;
	m_fileBuffer.buffer_size = 0;
	m_currentOperationProcessing = true;

	//MABLOGDEBUG("blob size is %d, %s,%d", m_blob_size_map[s_str], __FILE__, __LINE__);

    asyncOp->Completed = ref new Windows::Foundation::AsyncOperationCompletedHandler<Windows::Xbox::Storage::ConnectedStorageSpace^>( [this, s_str] ( Windows::Foundation::IAsyncOperation<Windows::Xbox::Storage::ConnectedStorageSpace^>^ operation, Windows::Foundation::AsyncStatus status )
    {
        try
        {
            if ( status == Windows::Foundation::AsyncStatus::Completed )
            {
                m_userSaveSpace = operation->GetResults(); 
                m_fileContainer = m_userSaveSpace->CreateContainer( RC3_XBOXLIVE_CONTAINER_NAME );
				if (m_fileContainer != nullptr) 
				{
					//auto blobsToDelete = ref new Windows::Foundation::Collections::IVectorView<Platform::String^>;
					IVector<Platform::String^>^ blobsToDelete;
					blobsToDelete = ref new Platform::Collections::Vector<Platform::String^>(5);
					//Windows::Storage::Streams::IBuffer^ fileWriteBuffer =  RUGBY_CHALLENGE_XBOXONE_MAB::WinRTWrapperForNativeBuffer::Create( (BYTE *)(m_fileBuffer.raw_buffer), m_fileBuffer.buffer_size ); 
					Platform::String^ buffer_name = ref new Platform::String(m_fileBuffer.fileName.c_str());
					//blobsToDelete->Insert( buffer_name, fileWriteBuffer );
					blobsToDelete->Append(buffer_name);

					auto asyncOp = m_fileContainer->SubmitUpdatesAsync(nullptr, blobsToDelete->GetView() ); 

					asyncOp->Completed = ref new Windows::Foundation::AsyncActionCompletedHandler( [this, s_str] ( Windows::Foundation::IAsyncAction^ operation, Windows::Foundation::AsyncStatus status ) 
					{
						//On file deleted
						if (status == Windows::Foundation::AsyncStatus::Completed)
						{
							m_blob_size = m_fileBuffer.buffer_size;
							m_blob_size_map[s_str] =  m_blob_size;
						}
						m_currentOperationProcessing = false;

					});
				}

            }
            else
            {
                XboxOneMabUtils::LogCommentFormat( L"\n ConnectedStorageSpace::GetForUserAsync has failed, Error code = 0x%x", operation->ErrorCode );
            }
        }
        catch( Platform::Exception^ ex )
        {
            //m_gameState = GameState::GAMESTATE_SHOW_ERROR_SCREEN;
            XboxOneMabUtils::LogCommentFormat( L"\n Exception thrown by ConnectedStorageSpace::GetForUserAsync; Error code = 0x%x, Message = %s", ex->HResult, ex->Message );
        }
	});

	while(m_currentOperationProcessing) {
		SwitchToThread();
	}
	return true;
}

bool XboxOneMabConnectedStorageManager::ReadFileFromConnectedStorage(const char* file_name, void* buffer, UINT32 size, UINT32 &read_size)
{
    Concurrency::critical_section::scoped_lock lock(m_lock);

	User^ current_user = XboxOneUser->GetCurrentUser();
	if(current_user == nullptr)
		return false;
	auto asyncOp = Windows::Xbox::Storage::ConnectedStorageSpace::GetForUserAsync( current_user );

	XboxOneMabUtils::LogComment(L"in ReadFileFromConnectedStorage Current user is " + current_user->DisplayInfo->Gamertag);
	m_fileBuffer.clear();
	std::string s_str = std::string(file_name);
	std::wstring w_str = std::wstring(s_str.begin(), s_str.end());
	m_fileBuffer.fileName = std::wstring(s_str.begin(), s_str.end());
	m_fileBuffer.raw_buffer = buffer;
	m_currentOperationProcessing = true;
	m_currentOperationProcessing_2nd = true;

	MABLOGDEBUG("blob size is %d, %s,%d", m_blob_size_map[s_str], __FILE__, __LINE__);
	m_fileBuffer.buffer_size = m_blob_size_map[s_str];

    asyncOp->Completed = ref new Windows::Foundation::AsyncOperationCompletedHandler<Windows::Xbox::Storage::ConnectedStorageSpace^>( [this, s_str] ( Windows::Foundation::IAsyncOperation<Windows::Xbox::Storage::ConnectedStorageSpace^>^ operation, Windows::Foundation::AsyncStatus status )
    {
        try
        {
            if ( status == Windows::Foundation::AsyncStatus::Completed )
            {
                m_userSaveSpace = operation->GetResults(); 
                m_fileContainer = m_userSaveSpace->CreateContainer(RC3_XBOXLIVE_CONTAINER_NAME);
				if (m_fileContainer != nullptr) 
				{
					auto blobsToRead = ref new Platform::Collections::Map<Platform::String^,  Windows::Storage::Streams::IBuffer^>();
					Windows::Storage::Streams::IBuffer^ fileReadBuffer =  RUGBY_CHALLENGE_XBOXONE_MAB::WinRTWrapperForNativeBuffer::Create( (BYTE *)(m_fileBuffer.raw_buffer), m_blob_size_map[s_str] ); 
					Platform::String^ buffer_name = ref new Platform::String(m_fileBuffer.fileName.c_str());
					blobsToRead->Insert( buffer_name, fileReadBuffer );

					auto asyncOp = m_fileContainer->ReadAsync( blobsToRead->GetView()); 	

					asyncOp->Completed = ref new Windows::Foundation::AsyncActionCompletedHandler( [this] ( Windows::Foundation::IAsyncAction^ operation, Windows::Foundation::AsyncStatus status ) 
					{
						//return a file size here.
						OnLoadBlobComplete();
						m_currentOperationProcessing_2nd = false;
					});

					while(m_currentOperationProcessing_2nd) {
						SwitchToThread();
					}
					m_currentOperationProcessing = false;
					//fileReadBuffer->
				}

            }
            else
            {
                XboxOneMabUtils::LogCommentFormat( L"\n ConnectedStorageSpace::GetForUserAsync has failed, Error code = 0x%x", operation->ErrorCode );
            }
        }
        catch( Platform::Exception^ ex )
        {
            //m_gameState = GameState::GAMESTATE_SHOW_ERROR_SCREEN;
            XboxOneMabUtils::LogCommentFormat( L"\n Exception thrown by ConnectedStorageSpace::GetForUserAsync; Error code = 0x%x, Message = %s", ex->HResult, ex->Message );
        }
	});

	while(m_currentOperationProcessing) {
		SwitchToThread();
	}
	MABLOGDEBUG("m_fileBuffer.buffer_size is %d, %s,%d", m_fileBuffer.buffer_size, __FILE__, __LINE__);
	read_size = m_fileBuffer.buffer_size;
	return true;
}

bool XboxOneMabConnectedStorageManager::CheckFileExistenceInConnectedStorage(const char* file_name, bool &exist)
{
    Concurrency::critical_section::scoped_lock lock(m_lock);

	User^ current_user = XboxOneUser->GetCurrentUser();
	if(current_user == nullptr)
	{
		exist = false;
		return true;
	}
	auto asyncOp = Windows::Xbox::Storage::ConnectedStorageSpace::GetForUserAsync( current_user );

	XboxOneMabUtils::LogComment(L"Current user is " + current_user->DisplayInfo->Gamertag);

	m_fileBuffer.clear();
	std::string s_str = std::string(file_name);
	std::wstring w_str = std::wstring(s_str.begin(), s_str.end());
	m_fileBuffer.fileName = std::wstring(s_str.begin(), s_str.end());
	m_fileBuffer.raw_buffer = nullptr;
	m_fileBuffer.buffer_size = 0;
	m_fileBuffer.exist = false;
	m_currentOperationProcessing = true;

    asyncOp->Completed = ref new Windows::Foundation::AsyncOperationCompletedHandler<Windows::Xbox::Storage::ConnectedStorageSpace^>( [this, s_str] ( Windows::Foundation::IAsyncOperation<Windows::Xbox::Storage::ConnectedStorageSpace^>^ operation, Windows::Foundation::AsyncStatus status )
    {
        try
        {
            if ( status == Windows::Foundation::AsyncStatus::Completed )
            {
                m_userSaveSpace = operation->GetResults(); 
                m_fileContainer = m_userSaveSpace->CreateContainer(RC3_XBOXLIVE_CONTAINER_NAME);
				if (m_fileContainer != nullptr) 
				{
					auto blobsToQuery = ref new Platform::Collections::Map<Platform::String^,  Windows::Storage::Streams::IBuffer^>();
					Platform::String^ buffer_name = ref new Platform::String(m_fileBuffer.fileName.c_str());

					auto myBlobInfo = m_fileContainer->CreateBlobInfoQuery( buffer_name); 	
					auto asyncOp = myBlobInfo->GetBlobInfoAsync();

					asyncOp->Completed = ref new Windows::Foundation::AsyncOperationCompletedHandler<IVectorView<Storage::BlobInfo>^>( [this, s_str] ( Windows::Foundation::IAsyncOperation<IVectorView<Storage::BlobInfo>^>^ operation, Windows::Foundation::AsyncStatus status )
					{
						//return a file size here.
						if ( status == Windows::Foundation::AsyncStatus::Completed )
						{
							IVectorView<Storage::BlobInfo>^ results  = operation->GetResults();
							if( results->Size > 0 ) 
							{
								OnLoadBlobComplete();
								m_fileBuffer.exist = true;
								for (int i = 0; i < results->Size; i++)
								{
									XboxOneMabUtils::LogCommentFormat(L"Blob info No. %d, name = %S, size = %d.", i, results->GetAt(i).Name->Data(), results->GetAt(i).Size );
								}
								m_blob_size = results->GetAt(0).Size;
								m_blob_size_map[s_str] = m_blob_size;
								m_fileBuffer.buffer_size = m_blob_size;
							}
						}
						m_currentOperationProcessing = false;

					});

					while(m_currentOperationProcessing) {
						SwitchToThread();
					}
				}

            }
            else
            {
                XboxOneMabUtils::LogCommentFormat( L"\n ConnectedStorageSpace::GetForUserAsync has failed, Error code = 0x%x", operation->ErrorCode );
            }
        }
        catch( Platform::Exception^ ex )
        {
            //m_gameState = GameState::GAMESTATE_SHOW_ERROR_SCREEN;
            XboxOneMabUtils::LogCommentFormat( L"\n Exception thrown by ConnectedStorageSpace::GetForUserAsync; Error code = 0x%x, Message = %s", ex->HResult, ex->Message );
        }
	});

	while(m_currentOperationProcessing) {
		SwitchToThread();
	}
	exist = m_fileBuffer.exist;
	return true;
}

bool XboxOneMabConnectedStorageManager::ListAllBlobsInConnectedStorage()
{
    Concurrency::critical_section::scoped_lock lock(m_lock);

	User^ current_user = XboxOneUser->GetCurrentUser();
	if(current_user == nullptr)
	{
		return true;
	}
	auto asyncOp = Windows::Xbox::Storage::ConnectedStorageSpace::GetForUserAsync( current_user );

	XboxOneMabUtils::LogComment(L"Current user is " + current_user->DisplayInfo->Gamertag);

	m_currentOperationProcessing = true;

    asyncOp->Completed = ref new Windows::Foundation::AsyncOperationCompletedHandler<Windows::Xbox::Storage::ConnectedStorageSpace^>( [this] ( Windows::Foundation::IAsyncOperation<Windows::Xbox::Storage::ConnectedStorageSpace^>^ operation, Windows::Foundation::AsyncStatus status )
    {
        try
        {
            if ( status == Windows::Foundation::AsyncStatus::Completed )
            {
                m_userSaveSpace = operation->GetResults(); 
                m_fileContainer = m_userSaveSpace->CreateContainer(RC3_XBOXLIVE_CONTAINER_NAME);
				if (m_fileContainer != nullptr) 
				{
					auto blobsToQuery = ref new Platform::Collections::Map<Platform::String^,  Windows::Storage::Streams::IBuffer^>();

					auto myBlobInfo = m_fileContainer->CreateBlobInfoQuery( nullptr); 	
					auto asyncOp = myBlobInfo->GetBlobInfoAsync();

					asyncOp->Completed = ref new Windows::Foundation::AsyncOperationCompletedHandler<IVectorView<Storage::BlobInfo>^>( [this] ( Windows::Foundation::IAsyncOperation<IVectorView<Storage::BlobInfo>^>^ operation, Windows::Foundation::AsyncStatus status )
					{
						//return a file size here.
						if ( status == Windows::Foundation::AsyncStatus::Completed )
						{
							IVectorView<Storage::BlobInfo>^ results  = operation->GetResults();
							if( results->Size > 0 ) 
							{
								for (int i = 0; i < results->Size; i++)
								{
									XboxOneMabUtils::LogComment(L"Blob info No. "+ i + L" name = " + results->GetAt(i).Name + ", size = " + results->GetAt(i).Size );
								}
							}
						}
						m_currentOperationProcessing = false;

					});

					while(m_currentOperationProcessing) {
						SwitchToThread();
					}
				}

            }
            else
            {
                XboxOneMabUtils::LogCommentFormat( L"\n ConnectedStorageSpace::GetForUserAsync has failed, Error code = 0x%x", operation->ErrorCode );
            }
        }
        catch( Platform::Exception^ ex )
        {
            //m_gameState = GameState::GAMESTATE_SHOW_ERROR_SCREEN;
            XboxOneMabUtils::LogCommentFormat( L"\n Exception thrown by ConnectedStorageSpace::GetForUserAsync; Error code = 0x%x, Message = %s", ex->HResult, ex->Message );
        }
	});

	while(m_currentOperationProcessing) {
		SwitchToThread();
	}
	return true;
}

void XboxOneMabConnectedStorageManager::OnLoadBlobComplete()
{

}

void XboxOneMabConnectedStorageManager::SyncOnDemand()
{
    Concurrency::critical_section::scoped_lock lock(m_lock);

	User^ current_user = XboxOneUser->GetCurrentUser();
	if(current_user == nullptr)
	{
		return;
	}
	auto asyncOp = Windows::Xbox::Storage::ConnectedStorageSpace::GetSyncOnDemandForUserAsync( current_user );

	XboxOneMabUtils::LogComment(L"Current user is " + current_user->DisplayInfo->Gamertag);

	m_currentOperationProcessing = true;

    asyncOp->Completed = ref new Windows::Foundation::AsyncOperationCompletedHandler<Windows::Xbox::Storage::ConnectedStorageSpace^>( [this] ( Windows::Foundation::IAsyncOperation<Windows::Xbox::Storage::ConnectedStorageSpace^>^ operation, Windows::Foundation::AsyncStatus status )
    {
        if ( status == Windows::Foundation::AsyncStatus::Completed )
		{
			XboxOneMabUtils::LogComment(L"Sync by current user compleated.");
		}

	});
}
