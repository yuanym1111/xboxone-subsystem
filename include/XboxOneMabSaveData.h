/**
 * @file XboxOneMabSaveData.h
 * @Xbox One save manager data class.
 * @author jaydens
 * @15/04/2015
 */
#ifndef XBOXONE_MAB_SAVE_DATA_H
#define XBOXONE_MAB_SAVE_DATA_H

#include <MabStreamMemory.h>

#include "XboxOneMabSaveMessage.h"		//< All Data classes have one to send back to the Manager at the end of their lives
#include "XboxOneMabSaveEnums.h"		//< Needed b/c of the enums
#include "XboxOneMabSaveDevice.h"

/// Forward declarations, needed for pointers
class XboxOneMabSaveDataDevice;
class XboxOneMabSaveDataMessage;

/// This is a list of all the classes that this file contains and a few words about their intention:
///
///	* XboxOneMabSaveData: The SerialData parent class;
/// * XboxOneMabSaveDataDevice: The SerialData subclass that deals specifically with
///		raising the device pane and device selection;
///	* XboxOneMabSaveDataEnumerateFiles: The SerialData subclass that deals specifically
///		with enumeration of save files;
///
/// @author Jayden Sun
///
class XboxOneMabSaveData 
	: public MabNonCopyable
	, public MabRuntimeType
	, public MabObservable<XboxOneMabSaveDataMessage>
{
	MABRUNTIMETYPE_HEADER(XboxOneMabSaveData);
public:

	/// An enumeration communicating the different types of save/load operations
	enum OPERATING_MODE
	{
		NO_OP = 0,
		SAVE_OP,
		LOAD_OP,
		DELETE_OP,
		EXIST_CHECK_OP,

		NUM_OPERATING_MODES,

		READY_FOR_DELETE,			//< Indicates that this operation should be removed from the Update queue
	};

	enum SD_OPERATION_STATUS
	{
		SD_INIT = 0,
		SD_SUCCESS,
		SD_FAIL
	};

	/** @name Construction/Destruction operations*/
	//@{
	/// The most common constructor; it's assumed you don't have all the information that you need at the time that this file
	///	is constructed. Use the accessor functions to set all applicable values later
	XboxOneMabSaveData( unsigned int unique_id, MABMEM_HEAP heap_id );

	/// The constructor for when everything is known at the time of construction
	XboxOneMabSaveData( unsigned int unique_id, MABMEM_HEAP heap_id, int _controller, const MabString& _display_filename, const MabString& _system_filename, XboxOneMabSaveDevice* _device, MabStreamMemory* _buffer, MabUInt64 _fixed_size = 0 );

	/// A constructor that fills out some of the fields by using an XCONTENT_DATA
	XboxOneMabSaveData( MABMEM_HEAP heap_id, int _controller, const WCHAR* data_in );

	/// Destructor; note that this is an IMMEDIATE destruction; if there is any chance that this save file hasn't fully moved out of its
	///	FSM--and FSMs only call their Exit code the frame after a Transition is commanded--then consider calling MarkForDeletion instead
	virtual ~XboxOneMabSaveData();

	//@}


	/** @name Accessor functions */
	//@{
	//inline SIFXboxOneSaveManager* const GetManager() const { MABBREAKMSG("TODO: We no longer store ther manager, get it elsewhere"); return NULL; }

	inline unsigned int GetID() const { return id; }
	inline XboxOneMabSaveMessage::MESSAGE_TYPE GetManagerMessage() const { return manager_message; }
	inline void SetManagerMessage( XboxOneMabSaveMessage::MESSAGE_TYPE msg ) { manager_message = msg; }

	void SetBuffer( MabStreamMemory* _buffer ) { if( _buffer != NULL ) buffer = *_buffer; }
	void SetDevice( XboxOneMabSaveDevice* const _device ) { if( _device != NULL ) device_id = _device->GetDeviceID(); }
	// TODO: Make these functions a little smarter in terms of what they're doing and how it impacts the FSM path that
	//	we go through--setting the system filename, for example, has the potential to add space against a fixed-size save and might
	//	require a new call to XContentCreateEx
	void SetController( int _controller ) { controller = _controller; }
	void SetMountedDeviceName( const MabString& _mounted_device_name );
	void SetDisplayFilename( const MabString& _display_filename );
	void SetSystemFilename( const MabString& _system_filename );
	void SetFixedSize( MabUInt64 _fixed_size ) { fixed_size = _fixed_size; }

	/// Can't return const MabStreamMemory& b/c even read operations on MabStreamMemory are non-const,
	///	and also we have the potential to resize the buffer post-system read
	MabStreamMemory* GetBuffer() { return &buffer; }
	int const GetDevice() const { return device_id; }
	int GetController() const { return controller; }
	const MabString& GetDisplayFilename() const { return display_filename; }
	const MabString& GetSystemFilename() const { return system_filename; }
	MabUInt64 GetFixedSize() const { return fixed_size; }
	OPERATING_MODE GetCurrentOperation() const { return current_mode; }
	XBOXONE_MAB_SAVE_ENUMS::WRITE_MODE GetSaveWriteMode() const;
	bool GetIsMarkedForDeletion() const { return marked_for_deletion; };

	/// This function is one exception to the bland accessors, because it's overridden by the
	///	GetFileSize function of the SerialDataDevice class, which returns the size of the file
	///	that's been REQUESTED. That's why we have the cast here to the unsigned version of this data
	///	(which should be safe because file_size should never be negative.
	virtual MabUInt64 InternalGetFileSize() const { return (MabUInt64) file_size; };

	//@}

	/// A function that determines if two files are equal, with equality being able to be used-defined
	///	@param that		the save_data we're comparing against
	///	@param eq_type	this field communicates the type of "equals" operation being performed. Most of the qualifiers
	///					in the enum can be |'d together; although ID can be |'d, it trumps all the rest of the qualifiers
	///					and only the ID will be checked. NOTE: The Controller (which represents the player's profile)
	///					will ALWAYS be checked, in all cases excpet ID, where it's implied.
	///					This defaults to device, display_name, and system_name.
	bool Equals( const XboxOneMabSaveData& that, XBOXONE_MAB_SAVE_ENUMS::EQUALITY_TYPE eq_type = XBOXONE_MAB_SAVE_ENUMS::EQUALITY_DEFAULT ) const;

	/// Performs an ID equality operation
	bool operator==( const XboxOneMabSaveData& that ) { return Equals( that, XBOXONE_MAB_SAVE_ENUMS::ID ); }

	///	A function that will delete this structure after the class's FSM's update is called once more, so that an FSM has a chance to call
	///	its Update (and presumably Exit) functionality before its surrounding class is deleted
	void MarkForDeletion() { marked_for_deletion = true; };

	/** @name Major functions */
	//@{

	/// \brief Starts the save process for this file. Subsequent calls to Update will move this file through the save process.
	///	@param	write_mode			Used to specify write new or overwrite operations
	/// This function is meant to be called ONLY by SIFXboxOneSaveManager::StartSave
	bool SetSaveMode( XBOXONE_MAB_SAVE_ENUMS::WRITE_MODE write_mode );

	/// \brief Starts the load process for this file. Subsequent calls to Update will move this file through the load process.
	/// This function is meant to be called ONLY by SIFXboxOneSaveManager::StartLoad
	bool SetLoadMode();

	/// Starts the process of handling an existence check for this file.
	bool SetExistCheckMode();

	///	Starts the delete process for this file
	bool SetDeleteMode();

	void SetOperationStatus(SD_OPERATION_STATUS status) { complete_status = status; };
	SD_OPERATION_STATUS GetOperationStatus() { return complete_status; };

	/// Should be called every frame the client wishes to move this file through its states; will generally be handled by the Manager
	virtual bool Update();

	bool IsFileExists() { return file_exists; }
	void SetFileExists( bool flag) { file_exists = flag; }

	//@}

protected:

	/** Helper functions **/
	//@{

	/// Gets the name of the full filestring
	const MabString& GetFullFilename() const { return full_filename; }

	/// Gets the dynamically-generated name of the mounted drive created by XContentCreate
	const MabString& GetMountedDriveName() const { return mounted_drive_name; }

	/// Calls 

	/// A method used to take a given save file and use its fields to fill a given XCONTENT_DATA structure.
	///	XCONTENT_DATAs are used by various SDK functions and represent one of the the XDK's internal representations
	///	of a save file.
	///	@param data_out			This is the data structure that you want to fill
	/// @alt_device				The device to use if you don't want to use this save file's internal device
	/// @alt_display_filename	The display filename to use if you don't want to use this save file's internal display filename
	/// @alt_system_filename	The system filename to use if you don't want to use this save file's internal system filename
	//void FillXContentData( XCONTENT_DATA& data_out, const XboxOneMabSaveDevice* const alt_device = NULL, 
	//						const MabString* const alt_display_filename = NULL, const MabString* const alt_system_filename = NULL ) const;

	/// Handler for incoming data messages
	//virtual void Update( MabObservable<XboxOneMabSaveDataMessage>* source, const XboxOneMabSaveDataMessage& msg );

	//@}

	///Memory Heap
	MABMEM_HEAP heap;

	/// ID assigned to this save file
	unsigned int id;

	/// the internal FSM that this save file is going to be monitoring
	//SIFXboxOneSerialFSM fsm;

	/// the message that should be sent back to the Manager when the most recent save/load operation is complete
	XboxOneMabSaveMessage::MESSAGE_TYPE manager_message;

	/// The size of the content you wish to mount on one device--if you're only using one save game, this will be 
	///	the size of that one save; if you have the potential to write three saves, then use that total size. This
	///	field is used to indicate whether you wish to use XContentCreateEx or XContentCreate. If you wish to use
	///	XContentCreate set this field to 0.
	MabUInt64 fixed_size;

	/// This is the size of the file as determined through the course of a load operation
	LONGLONG file_size;

	OPERATING_MODE current_mode;
	XBOXONE_MAB_SAVE_ENUMS::WRITE_MODE save_write_mode;

	bool file_exists;

	/** @name fields that are used more or less directly by the XDK */
	//@{

	MabStreamMemory buffer;
	int device_id;
	int controller;
	MabString display_filename;
	MabString system_filename;

	//@}

private:

	/// Private, unimplemented copy-constructor -- this represenation of save files should never be copied
	XboxOneMabSaveData( const XboxOneMabSaveData& that );

	/// The name that this save file has had its drive mapped to
	MabString mounted_drive_name;

	/// Full name of the file path--dependent on mounted_drive_name
	MabString full_filename;

	/// The flag for when a Data struct is in-progress and has been marked for deletion
	bool marked_for_deletion;

	void Initialise( unsigned int _id );

	SD_OPERATION_STATUS complete_status;

	/// Calls the XContentCreate Functionality on the current save data
	/// @return the flag from the XContent functions correlated messages

	XboxOneMabSaveMessage::MESSAGE_TYPE PerformSave( );
	XboxOneMabSaveMessage::MESSAGE_TYPE PerformLoad( );
	XboxOneMabSaveMessage::MESSAGE_TYPE PerformDelete();
	XboxOneMabSaveMessage::MESSAGE_TYPE PerformExistCheck();
	XboxOneMabSaveMessage::MESSAGE_TYPE PerformContentCreate();
	XboxOneMabSaveMessage::MESSAGE_TYPE PerformContentClose();

	/// Friended classes
	///	The reason we have a bunch of friended classes is because there are a number of things that the Manager and the FSM need to do to
	///	the Data that ordinary clients won't have to do. Although nominally friending hurts encapsulation, in this case we're trimming
	///	down our public interface.
	///@{

	friend class SIFXboxOneSaveManager;

	///@}
};

/// \class XboxOneMabSaveDataDevice
/// A Data subclass that is meant to represent the Device Select operation. 
///		It's implemented as a XboxOneMabSaveData (to make it
///		possible to include on the list of save/load operations to be processed by the Manager), although many of its operations 
///		should not ever be called--StartSave, StartLoad, etc.
///	@author Mark Barrett
class XboxOneMabSaveDataDevice
	:	public XboxOneMabSaveData
	,	public MabObservable< XboxOneMabSaveDataMessage >
{
	MABRUNTIMETYPE_HEADER(XboxOneMabSaveDataDevice);

public:
	/// Constructor will immediately begin attempting to raise the device selector pane
	/// @param _manager			The manager for this save file.
	///	@param controller_id	The controller that's commanded the device raise
	/// @param flags_in			The argument--if any--that you wish to pass to XShowDeviceSelectorUI. If you leave this
	///							field as its default, XCONTENTFLAG_NONE, then the FSM underlying this code will handle all
	///							necessary size-checking for you. (This sequence is explained in the comments for 
	///							SIFXboxOneSerialFSMStateRaiseDevice.) If you set this flag to either of the other values, it's
	///							assumed you're doing something other than the basic "select a location for my save game"
	///							device operation, and will NOT perform the size-checking.
	///							Possible values:
	///								XCONTENTFLAG_FORCE_SHOW_UI: forces the UI to come up even if there's only one device available
	///								XCONTENTFLAG_MANAGESTORAGE: will skip past the sub-UI that allows the player to manage overfilled
	///															devices even if the selected device doesn't have enough space.
	///	@param file_size		The size of the intended save data; needs to be accurate for TCRs.
	///							NOTE: If you are raising this for a load operation--for instance, if
	///							the player has save games on multiple devices and you're asking the player
	///							to choose between them, set this field to 0. ONLY for **save** operations
	///							should this field be set to the size of the file.
	XboxOneMabSaveDataDevice(  unsigned int _id, MABMEM_HEAP heap_id, int controller_id, MabUInt64 file_size, DWORD flags_in );
	virtual ~XboxOneMabSaveDataDevice() {};

	/// Returns the flags that the user has specified
	WCHAR GetFlags() const { return flags; };

	/// Returns the file size of the requested save file
	virtual MabUInt64 InternalGetFileSize() const { return device_needed_file_size; };

	/// Main update function
	virtual bool Update();

	/// Handler for when the message signaling that the device is selected is fired
	//virtual void Update( MabObservable< XboxOneMabSaveDataMessage >* source, const XboxOneMabSaveDataMessage& msg );

	/// Function to call to raise the device, if it fails or succeeds it will send a XboxOneMabSaveDataMessage out.
	/// The manager can then call this
	void RaiseDevice( MabObserver<XboxOneMabSaveDeviceMessage>* _observer );

private:
	/// Method to help expedite device-selector pane raising
	///	@overlapped_ptr_out	A reference-to-pointer to the overlapped structure that's returned by the XShowDeviceSelectorUI 
	///							function; the original argument can be set to NULL
	///	@return				Returns true iff there the device-selector pane was successfully raised
	bool DeviceRaiseHelper(  );

	/// The device that the player has selected at the end of being raised
	XboxOneMabSaveDevice* selected_device;

	/// The internal flag for when the raise has occured.
	bool raised_device;

	/// The flags associated with this call to the raise function
	DWORD flags;

	/// The file size associated with this call to the raise function
	MabUInt64 device_needed_file_size;
};

/// A "static" class--on a per-signed-in-user basis--meant to represent the Enumerate Files operation. 
///		It's implemented as a XboxOneMabSaveData (to make it
///		possible to include on the list of save/load operations to be processed by the Manager), although many of its operations 
///		should not ever be called--StartSave, StartLoad, etc.
class XboxOneMabSaveDataEnumerateFiles
	: public XboxOneMabSaveData
{
	MABRUNTIMETYPE_HEADER(XboxOneMabSaveDataEnumerateFiles);
public:

	/// This is the initial maximum number of files sent to XEnumerate; if this number of files is returned by the function,
	///	this number will be multiplied by NUM_SAVE_FILES_FACTOR, the array deleted, and XEnumerate called again.
	static const unsigned int NUM_SAVE_FILES_INITIAL = 5;

	/// The number to multiply NUM_SAVE_FILES_INITIAL by in case of XEnumerate returning a maximum number of files
	static const unsigned int NUM_SAVE_FILES_FACTOR = 5;

	XboxOneMabSaveDataEnumerateFiles( unsigned int _device_id, MABMEM_HEAP heap_id );
	virtual ~XboxOneMabSaveDataEnumerateFiles();

	/// See above and Manager::StartEnumerateFiles for a description of how this function works
	unsigned int StartEnumerateFiles( MabVector<XboxOneMabSaveData*>& files_list_out );

	/// Returns true if an EnumerateFiles call is currently in-progress
	bool GetIsInProgress() const { return found_files != NULL; };

	/// Main update function
	virtual bool Update();

	// Handler for DataMessages
	//virtual void Update( MabObservable< XboxOneMabSaveDataMessage >* source, const XboxOneMabSaveDataMessage& msg );

	// These functions must be called in order, an enumurator must exist before an enumeration can occur and the enumeration must 
	// have finished before trying to retrieve the results
	XboxOneMabSaveMessage::MESSAGE_TYPE CreateEnumerator( unsigned int _max_number_save_files );
	XboxOneMabSaveMessage::MESSAGE_TYPE PerformEnumeration();
	XboxOneMabSaveMessage::MESSAGE_TYPE RetrieveData( DWORD num_found_save_files );

private:
	/// A ptr to the list of files that you've found as a result of this enumerate
	MabVector< XboxOneMabSaveData* >* found_files;
	/// A system-required handle to the enumeration
	HANDLE enumerate_handle;
	/// the number of save files discovered and the size of all of them; returned by system procedures
	DWORD save_files_size;
	/// The max number of files to attmept to retrieve
	unsigned int max_number_save_files;
	/// The array of save files that is a consequence of the enumerate. Has the potential to change size,
	///	so is only a pointer
	DWORD* save_files;
};

#endif //#ifndef XBOXONE_MAB_SAVE_DATA_H