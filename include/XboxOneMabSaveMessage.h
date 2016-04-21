/**
 * @file XboxOneMabSaveMessage.h
 * @Xbox One save manager message class.
 * @author jaydens
 * @15/04/2015
 */

#ifndef XBOXONE_MAB_SAVE_MESSAGE
#define XBOXONE_MAB_SAVE_MESSAGE

/// \class XboxOneMabSaveMessage
/// A class describing all serial messages that get passed from Serial sub-class to serial sub-class
///
///	@author Jayden Sun
///
class XboxOneMabSaveMessage : public MabNonCopyable
{
public:

	// Can be bitwise OR'd together
	enum MESSAGE_TYPE
	{
		// Types that can be XOR'd together as being valid, expected responses by Manager; can only be successes. All
		//	failures are assumed to be unique. We assume there won't be more than eight of these
		SUCCESS_GENERIC						= 0x0001,	//< Successful operation; no further info
		SUCCESS_FILE_EXISTS					= 0x0002,	//< Successful operation; noting that file exists
		SUCCESS_FILE_DOES_NOT_EXIST			= 0x0004,	//< Successful operation; noting that file dne
		SUCCESS_PLAYER_SELECTED_DEVICE		= 0x0008,	//< Successful operation; player selected a device 
		FAIL_PLAYER_DID_NOT_SELECT_DEVICE	= 0x0010,	//< Failed operation; player opted not to select a device
		
		// Types that are unique and shouldn't be XOR'd

		SUCCESS_DEVICE_VALID				= 0x0100,	//< Successful operation; noting that device is valid
		SUCCESS_FILES_ENUMERATED,						//< Successful operation; all files matching criteria are enumerated
		SUCCESS_FILE_WRITE,								//< Successful operation; player successfully performed a write
		SUCCESS_FILE_READ,								//< Successful operation; player successfully performed a read
		SUCCESS_FILE_DELETED,							//< Successful operation; file successfully deleted
		SUCCESS_USER_IS_AUTHOR,							//< Successful operation; the signed-in user is the creator of this file
		SUCCESS_DEVICE_RAISE,							//< Successful operation; the device has been raised and is being displayed

		FAIL_GENERIC,									//< Failed operation; no further info
		FAIL_FILE_EXISTS,								//< Failed operation; the file exists
		FAIL_FILE_DOES_NOT_EXIST,						//< Failed operation; the file does not exist
		FAIL_DEVICE_INVALID,							//< Failed operation; the specified device is invalid
		FAIL_DATA_CORRUPT,								//< Failed operation; the data is corrupt
		FAIL_INSUFFICIENT_SPACE,						//< Failed operation; there's not enough space on the specified device
		FAIL_USER_NOT_AUTHOR,							//< Failed operation; the signed-in user is not the creator of this file
		FAIL_SIZE_MISMATCH,								//< Failed operation; the number of bytes read was not the number of bytes expected
		FAIL_DEVICE_RAISE,								//< Failed operation; the device has not been raised and we will need to try again
		FAIL_ENUMERATE_FAILED,							//< Failed operation; no further info
		
		OPERATION_COMPLETE,								//< Generic message that indicates a given Data operation is complete
														//<	and hence should be removed from Manager's list of processed operations
		DEVICE_FOUND,									//< Generic message stating that a device has been found. Called when a
														//< Device is constructed to alert the Manager that it should know about
														//<	this Device and put it in its internal Device list.
		SAVE_FILE_FOUND,								//< Generic message stating that a save file has been found and should be added
														//< to the Manager's list of save files.
		FILE_SIZE_FOUND,								//< File size determined (file size will be sent in the message's State)
		SET_MOUNTED_DRIVE_NAME,							//< Command to the Data class to set its mounted drive name (result of ContentCreate)
		UNSET_MOUNTED_DRIVE_NAME,						//< Command to the Data class to clear its mounted drive name (result of ContentClose)
		SET_FULL_FILENAME,								//< Command to the Data class to set its full filename (result of CreateFile)
		UNSET_FULL_FILENAME,							//< Command to the Data class to clear its full filename (result of CloseHandle)

		INVALID_MESSAGE_TYPE				= 0x0000
	};

	XboxOneMabSaveMessage( const MESSAGE_TYPE _type, unsigned int _id, void* const _state = NULL )
		: type( _type ), id( _id ), state( _state ) {};
	virtual ~XboxOneMabSaveMessage() {};

	const MESSAGE_TYPE GetType() const { return type; };
	unsigned int GetID() const { return id; };
	void* const GetState() const { return state; };

private:
	/// The type of this message
	const MESSAGE_TYPE type;

	/// The id of the save game this message is associated with
	const unsigned int id;

	/// Any additional state information, if that's required.
	void* const state;
};

/// This is the message that is delivered from XboxOneMabSaveData or its underlying FSM, usually to SIFXboxOneSaveManager
class XboxOneMabSaveDataMessage
	: public XboxOneMabSaveMessage
{
public:
	XboxOneMabSaveDataMessage( const MESSAGE_TYPE _type, unsigned int _id, void* const _state = NULL )
		:	XboxOneMabSaveMessage( _type, _id, _state ) {};
	virtual ~XboxOneMabSaveDataMessage() {};
};

#endif //#ifndef XboxOne_MAB_SAVE_MESSAGE