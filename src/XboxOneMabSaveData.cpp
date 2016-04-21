
#include "pch.h"

#include "XboxOneMabSaveData.h"

#include "XboxOneMabSaveDevice.h"
#include "XboxOneMabUtils.h"
#include "XboxOneMabWinRTWrapperForNativeBuffer.h"
#include "XboxOneMabConnectedStorageManager.h"

using namespace XBOXONE_MAB_SAVE_ENUMS;

MABRUNTIMETYPE_IMP(XboxOneMabSaveData);
MABRUNTIMETYPE_IMP1(XboxOneMabSaveDataEnumerateFiles, XboxOneMabSaveData);

// in-file static functions

/// Get the full filename given the mounted drive name and the system filename.
static void GetFullFilenameFromMountedDriveName( const MabString& mounted_drive_name_in, const MabString& system_filename_in, MabString& full_filename_out )
{
	full_filename_out = system_filename_in;
}

// ----------------------------------------------------------------------------
// --------------- XboxOneMabSaveData ---------------------------------------
// ----------------------------------------------------------------------------

XboxOneMabSaveData::XboxOneMabSaveData( unsigned int unique_id, MABMEM_HEAP heap_id )
: 
heap( heap_id ), 
controller( 0 ), 
display_filename( "" ),
system_filename( "" ), 
device_id( 0 ), 
buffer( heap_id ), 
fixed_size( 0 ), 
file_size( 0 ), 
marked_for_deletion( false ),
complete_status(SD_INIT),
file_exists(false)
{
	Initialise( unique_id );
}

XboxOneMabSaveData::XboxOneMabSaveData( unsigned int unique_id, MABMEM_HEAP heap_id, int _controller, 
										const MabString& _display_filename, const MabString& _system_filename, 
										XboxOneMabSaveDevice* _device, MabStreamMemory* _buffer, MabUInt64 _fixed_size )
: 
heap( heap_id ), 
controller( _controller ), 
display_filename( _display_filename ), 
system_filename( _system_filename ), 
device_id( 0 ), 
fixed_size( _fixed_size ), 
file_size( 0 ), 
marked_for_deletion( false ),
file_exists(false)
{
	SetBuffer( _buffer );

	if( _device != NULL )
	{
		device_id = _device->GetDeviceID();
	}
	else
	{
		device_id = 0;
	}
	Initialise( unique_id );
}

XboxOneMabSaveData::XboxOneMabSaveData( MABMEM_HEAP heap_id, int _controller, const WCHAR* data_in )
:	
heap( heap_id ), 
controller( _controller ), 
buffer( heap_id ), 
fixed_size( 0 ), 
file_size( 0 ), 
marked_for_deletion( false ),
file_exists(false)
{
	MABUNUSED(data_in);
	// need this to convert szDisplayName from WCHAR format
	char tmp_display_name[ 512 ];

	device_id = 0;

	display_filename = MabString( tmp_display_name );
	system_filename = MabString( "EMPTY");
}

XboxOneMabSaveData::~XboxOneMabSaveData()
{
	buffer.Close();
}

void XboxOneMabSaveData::Initialise( unsigned int _id )
{
	MABASSERTMSG( display_filename.length() <= 512, "This display filename is too long!" );
	MABASSERTMSG( system_filename.length() <= 512, "This system filename is too long!" );

	id = _id;
	manager_message = XboxOneMabSaveMessage::INVALID_MESSAGE_TYPE;
	current_mode = NO_OP;
	save_write_mode = WRITE_MODE_ANY;
	mounted_drive_name = "";
	full_filename = system_filename;
}

void XboxOneMabSaveData::SetDisplayFilename( const MabString& _display_filename )
{
	MABASSERTMSG( _display_filename.length() <= 512, "This display filename is too long!" );
	display_filename = _display_filename;
}

void XboxOneMabSaveData::SetSystemFilename( const MabString& _system_filename )
{
	MABASSERTMSG( _system_filename.length() <= 512, "This system filename is too long!" );
	system_filename = _system_filename;
}


WRITE_MODE XboxOneMabSaveData::GetSaveWriteMode() const
{
	MABASSERTMSG( current_mode == SAVE_OP, "GetSaveWriteMode should not be called when the operation for this file isn't a write." );
	return save_write_mode;
}

// Start the save process.
bool XboxOneMabSaveData::SetSaveMode( WRITE_MODE write_mode_in )
{
	MABASSERTMSG( current_mode == NO_OP, "StartSave is being called before the previous operation is complete!" );
	if ( current_mode != NO_OP ) 
		return false;
	current_mode = SAVE_OP;
	save_write_mode = write_mode_in;
	return true;
}

// Start the load process.
bool XboxOneMabSaveData::SetLoadMode()
{
	MABASSERTMSG( ( current_mode == NO_OP || current_mode == READY_FOR_DELETE ), "StartLoad is being called before the previous operation is complete!" );
	if ( current_mode != NO_OP && current_mode != READY_FOR_DELETE ) 
		return false;
	current_mode = LOAD_OP;
	return true;
}

// Start the process of deleting this file off of the file system.
bool XboxOneMabSaveData::SetDeleteMode()
{
	MABASSERTMSG( ( current_mode == NO_OP || current_mode == READY_FOR_DELETE ), "StartDelete is being called before the previous operation is complete!" );
	if ( current_mode != NO_OP && current_mode != READY_FOR_DELETE ) 
		return false;
	current_mode = DELETE_OP;
	return true;
}

static XboxOneMabSaveMessage::MESSAGE_TYPE CreateFileHelper( const MabString& filename_as_str, const DWORD access_mode, const DWORD creation_mode, const DWORD flag, HANDLE& handle_out )
{
	XboxOneMabSaveMessage::MESSAGE_TYPE msg_out = XboxOneMabSaveMessage::SUCCESS_GENERIC;

	CREATEFILE2_EXTENDED_PARAMETERS params;
    ZeroMemory( &params, sizeof(params) );

    params.dwSize = sizeof( params );
    params.dwFileFlags = flag;

	handle_out = CreateFile2
		( 
		
		XboxOneMabUtils::ConvertCharStringToPlatformString(filename_as_str.c_str())->Data(),
															//< filename; set by previous FSMState
		access_mode,										//< GENERIC_WRITE or GENERIC_READ
		0,													//< sharing variable; at the moment we allow NO sharing of the file
															//<	with multiple write attempts, so that while this handle is open
															//<	we cannot open another handle
		creation_mode,										//< this specifies basically whether or not to create the
															//< file if it's not already there; so far we're saying no 
															//< for both writes and reads
		&params
		);

	if ( handle_out == INVALID_HANDLE_VALUE )
	{
		DWORD last_err = GetLastError();
		switch ( last_err )
		{
		case ERROR_PATH_NOT_FOUND: //< means the device has been removed since the device was mounted
			msg_out = XboxOneMabSaveMessage::FAIL_DEVICE_INVALID;
			break;
		case ERROR_FILE_NOT_FOUND:
		case ERROR_FILE_CORRUPT:
		case ERROR_DISK_CORRUPT:
			msg_out = XboxOneMabSaveMessage::FAIL_DATA_CORRUPT;
			break;
		default:
			MABBREAKMSG( "An invalid handle has yielded an unfamiliar error!" );
			msg_out = XboxOneMabSaveMessage::FAIL_GENERIC;
			break;
		}
		// Even though it's errored, we still need to close the handle --Is this known to be true??
		CloseHandle( handle_out );
	}

	return msg_out;
}


XboxOneMabSaveMessage::MESSAGE_TYPE XboxOneMabSaveData::PerformSave()
{
	DWORD number_of_bytes_written;

	const char* tmp_buffer = buffer.RawBuffer();
	//MABLOGDEBUG("%s", tmp_buffer);
	size_t buf_size = buffer.Size();

	BOOL ret_val = XboxOneMabConnectedStorageManager::Get()->WriteFileToConnectedStorage(system_filename.c_str(), (void*)tmp_buffer, buf_size);

	if( ret_val == FALSE )
	{
		const DWORD write_file_error = GetLastError();
		MABLOGDEBUG( "Error number: %u", write_file_error );

		manager_message = XboxOneMabSaveMessage::FAIL_GENERIC;
		return XboxOneMabSaveMessage::FAIL_GENERIC;
	}

	manager_message = XboxOneMabSaveMessage::SUCCESS_FILE_WRITE;
	return XboxOneMabSaveMessage::SUCCESS_FILE_WRITE;
}

XboxOneMabSaveMessage::MESSAGE_TYPE XboxOneMabSaveData::PerformLoad()
{
	UINT32 number_of_bytes_read = NULL;

	MABLOGDEBUG( "XboxOne: LoadingFile: %s", system_filename.c_str() );
	std::string str_name = std::string(system_filename.c_str());
	buffer.Reserve( XboxOneMabConnectedStorageManager::Get()->GetBlobSize(str_name) + 256 );
	buffer.Clear();
	// Lock the buffer--notify that an external application will be writing to it
	const char* tmp_buffer = buffer.Lock();
	size_t buf_size = buffer.Capacity();

	//XboxOneMabConnectedStorageManager::Get()->ListAllBlobsInConnectedStorage();
	BOOL ret_val = XboxOneMabConnectedStorageManager::Get()->ReadFileFromConnectedStorage( system_filename.c_str(), (void*)tmp_buffer, buf_size, number_of_bytes_read );
	MABASSERT( ret_val == TRUE );

	MABLOGDEBUG("File Size: %d", file_size);
	MABLOGDEBUG("Buffer Size: %d", buf_size);
	MABLOGDEBUG("number_of_bytes_read: %d", number_of_bytes_read);

	if ( number_of_bytes_read <= 0 )
	{
		return XboxOneMabSaveMessage::FAIL_FILE_DOES_NOT_EXIST;
	}

	buffer.Unlock( (size_t) number_of_bytes_read );
	
	manager_message = XboxOneMabSaveMessage::SUCCESS_FILE_READ;
	//Notify(XboxOneMabSaveMessage(manager_message, GetID(), nullptr));

	return XboxOneMabSaveMessage::SUCCESS_FILE_READ;
}

XboxOneMabSaveMessage::MESSAGE_TYPE XboxOneMabSaveData::PerformExistCheck()
{

	MABLOGDEBUG( "XboxOne: LoadingFile: %s", system_filename.c_str() );

	BOOL ret_val = XboxOneMabConnectedStorageManager::Get()->CheckFileExistenceInConnectedStorage( system_filename.c_str(), file_exists );

	MABLOGDEBUG("file_exist: %s", file_exists ? "yes" : "no");

	if ( file_exists == false )
	{
		return XboxOneMabSaveMessage::FAIL_FILE_DOES_NOT_EXIST;
	}

	manager_message = XboxOneMabSaveMessage::SUCCESS_FILE_EXISTS;
	return XboxOneMabSaveMessage::SUCCESS_FILE_EXISTS;
}

XboxOneMabSaveMessage::MESSAGE_TYPE XboxOneMabSaveData::PerformDelete(  )
{		

	MABLOGDEBUG( "XboxOne: Deleting File: %s", system_filename.c_str() );
	std::string str_name = std::string(system_filename.c_str());

	XboxOneMabConnectedStorageManager::Get()->ListAllBlobsInConnectedStorage();
	BOOL ret_val = XboxOneMabConnectedStorageManager::Get()->DeleteFileFromConnectedStorage( system_filename.c_str() );
	MABASSERT( ret_val == TRUE );
	
	return XboxOneMabSaveMessage::SUCCESS_FILE_DELETED;
}

XboxOneMabSaveMessage::MESSAGE_TYPE XboxOneMabSaveData::PerformContentCreate(  )
{
	return XboxOneMabSaveMessage::SUCCESS_GENERIC;
}

XboxOneMabSaveMessage::MESSAGE_TYPE XboxOneMabSaveData::PerformContentClose( )
{
	return XboxOneMabSaveMessage::SUCCESS_GENERIC;
}

// Main update loop; just let the fsm handle it.
bool XboxOneMabSaveData::Update()
{
	return true;
}

// A function that determines if two files are equal, with equality being able to be used-defined
bool XboxOneMabSaveData::Equals( const XboxOneMabSaveData& that, EQUALITY_TYPE eq_type ) const
{
	if ( eq_type & ID )
	{
		return ( id == that.id );
	}

	bool to_go_out = ( ( controller == that.controller ) &&	//< always check controller

			// either we've not turned on DEVICE or device == that.device; same for all the rest of the fields
			 ( ( !( eq_type & DEVICE ) ) || device_id == that.device_id ) &&
			 ( ( !( eq_type & DISPLAY_NAME ) ) || display_filename == that.display_filename ) &&
			 ( ( !( eq_type & SYSTEM_NAME ) ) || system_filename == that.system_filename ) );

	// if the function is already returning false or if we've selected not to compare the buffer, return the value as it exists
	if ( !to_go_out || !( eq_type & BUFFER ) ) return to_go_out;

	// We need to perform a buffer comparison. First check to see if they're the same size:
	if ( buffer.Size() != that.buffer.Size() ) return false;

	// ...otherwise we need to compare the contents. This obviously can get a little time-consuming.
	for ( size_t i = 0; i < buffer.Size(); ++i )
	{
		if ( ( *buffer )[ i ] != ( *( that.buffer ) )[ i ] ) return false;
	}
	return true;
}


void XboxOneMabSaveData::SetMountedDeviceName( const MabString& _mounted_device_name )
{
	MABLOGDEBUG("XboxOne: Setting mounted device name: %s", _mounted_device_name.c_str());
	mounted_drive_name = _mounted_device_name;
	GetFullFilenameFromMountedDriveName( mounted_drive_name, system_filename, full_filename );
}


// ----------------------------------------------------------------------------
// --------------- XboxOneMabSaveDataEnumerateFiles -------------------------
// ----------------------------------------------------------------------------

XboxOneMabSaveDataEnumerateFiles::XboxOneMabSaveDataEnumerateFiles(  unsigned int _device_id, MABMEM_HEAP heap_id  ) 
: XboxOneMabSaveData( _device_id, heap_id )
, found_files( NULL )
, enumerate_handle( NULL )
, save_files( NULL )
{
	manager_message = XboxOneMabSaveMessage::SUCCESS_FILES_ENUMERATED;
}

XboxOneMabSaveDataEnumerateFiles::~XboxOneMabSaveDataEnumerateFiles()
{
	if ( save_files != NULL ) 
	{
		MabMemArrayDelete( save_files );
		save_files = NULL;
	}

	if ( found_files != NULL ) 
	{
		found_files->clear();
		found_files = NULL;
	}
}

// Kick off the enumeration process
unsigned int XboxOneMabSaveDataEnumerateFiles::StartEnumerateFiles( MabVector<XboxOneMabSaveData*>& files_list_out )
{
	MABASSERTMSG( found_files == NULL, "StartEnumerateFiles being called a second time before the first is resolved!" );
	found_files = &files_list_out;

	return GetID();
}

XboxOneMabSaveMessage::MESSAGE_TYPE XboxOneMabSaveDataEnumerateFiles::CreateEnumerator( unsigned int _max_number_save_files )
{
	MABUNUSED(_max_number_save_files);
	return XboxOneMabSaveMessage::SUCCESS_GENERIC;
}

XboxOneMabSaveMessage::MESSAGE_TYPE XboxOneMabSaveDataEnumerateFiles::PerformEnumeration( )
{
	return XboxOneMabSaveMessage::FAIL_GENERIC;
}

XboxOneMabSaveMessage::MESSAGE_TYPE XboxOneMabSaveDataEnumerateFiles::RetrieveData( DWORD num_found_save_files )
{
	MABUNUSED(num_found_save_files);

	return XboxOneMabSaveMessage::SAVE_FILE_FOUND;
}

// Main update function
bool XboxOneMabSaveDataEnumerateFiles::Update()
{
	// let your parent's Update function do the work
	return XboxOneMabSaveData::Update();
}