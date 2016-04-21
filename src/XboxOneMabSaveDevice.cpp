#include "pch.h"

#include "XboxOneMabSaveDevice.h"
#include "XboxOneMabSaveMessage.h"
//#include "SIFXboxOneSaveFSM.h"
//#include "SIFXboxOneSerialManager.h"

XboxOneMabSaveDevice::XboxOneMabSaveDevice( MabObserver<XboxOneMabSaveDeviceMessage>* _observer )
	: observer( _observer ), was_attached( false ), device_id(0)
{
	Initialise();
}

XboxOneMabSaveDevice::XboxOneMabSaveDevice( const XboxOneMabSaveDevice& that )
{
	device_id = that.device_id;
	internal_info = that.internal_info;
	internal_friendly_name = that.internal_friendly_name;
	was_attached = false;
	Initialise();
}

/// Gets operator
XboxOneMabSaveDevice& XboxOneMabSaveDevice::operator=( const XboxOneMabSaveDevice& that )
{
	// Now begin copying
	device_id = that.device_id;
	internal_info = that.internal_info;
	internal_friendly_name = that.internal_friendly_name;
	Initialise();
	return *this;
}

void XboxOneMabSaveDevice::Initialise()
{
	/// Always attach any Device to the SerialManager so it can update its internal device list
	/// NOTE: The exception is ALL_DEVICES; that's not a real device.
	/// FURTHER NOTE: There's also the TestSuite's JUNK_DEVICE, which also shouldn't be attached.
	///	Leaving this code here even though ENABLE_SIF_SAVE_LOAD_TESTING is outre, b/c we'll need something
	///	like it at some point.
#ifdef ENABLE_SIF_SAVE_LOAD_TESTING
	if ( device_id != XCONTENTDEVICE_ANY && device_id != SIFXboxOneSaveTestSuite::JUNK_DEVICE_ID )
#else
	if ( !was_attached && observer && device_id != 0 ) 
#endif
	{
		Attach( observer );
		was_attached = true;
	}
}

XboxOneMabSaveDevice::~XboxOneMabSaveDevice()
{
	if( was_attached )
		Detach( observer );
}

/// Begin the process of checking to ensure the device is valid
///	@param is_valid		reflects whether or not this device is valid
///	NOTE: this method will return a SUCCESS_DEVICE_VALID or FAIL_DEVICE_INVALID (provided nothing else goes wrong)
///	in addition to setting the value of is_valid. Additionally, the Manager will update both
///	player_selected_devices and its internal device_list should those prove necessary.
bool XboxOneMabSaveDevice::StartGetIsValid()
{
	// device_valid_overlapped can actually be not-NULL provided the validity check has been started by someone else
	//MABASSERTMSG( device_valid_overlapped == NULL, "StartGetIsValid is being called before a previous IsValid request has been finished!" );
	MABASSERT( device_id != 0 );
	return false;
}

bool XboxOneMabSaveDevice::Update()
{
	return true;
}

bool XboxOneMabSaveDevice::ForceDeviceInfoPopulate()
{
	return true;
}

int XboxOneMabSaveDevice::GetType() 
{
	return 0; 
}

ULONGLONG XboxOneMabSaveDevice::GetTotalSpace() 
{ 
	return 0; 
}
bool XboxOneMabSaveDevice::GetRemainingSpace( ULONGLONG& space_out ) 
{
	MABUNUSED(space_out);
	return 0;
}

const MabString& XboxOneMabSaveDevice::GetFriendlyName()
{
	return internal_friendly_name;
}

// Static error function that tests for device disconnection with all of MS's highly-varied error messages.
bool XboxOneMabSaveDevice::IsDeviceDisconnected( DWORD ret_val_in )
{
	return(	ret_val_in == ERROR_ACCESS_DENIED 
			|| ret_val_in == ERROR_NOT_READY 
			|| ret_val_in == ERROR_MEDIA_CHANGED 
			|| ret_val_in == ERROR_DEVICE_NOT_CONNECTED
			|| ret_val_in == ERROR_DEVICE_REMOVED);
}