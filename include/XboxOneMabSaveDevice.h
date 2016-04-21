
#ifndef XBOXONE_MAB_SAVE_DEVICE
#define XBOXONE_MAB_SAVE_DEVICE

#include <xdk.h>
#include <MabObserver.h>
#include <MabNonCopyable.h>
#include <MabRuntimeType.h>
#include <winnt.h>

// forward declaration of message class; defined in SIFXboxOneSerialMessage
class XboxOneMabSaveDeviceMessage;

class XboxOneMabSaveDevice :
	public MabObservable<XboxOneMabSaveDeviceMessage>
{
public:

	///Constructor -- 
	XboxOneMabSaveDevice( MabObserver<XboxOneMabSaveDeviceMessage>* _observer);

	/// Still has a public destructor
	virtual ~XboxOneMabSaveDevice();

	/// Begin the process of checking to ensure the device is valid
	///	NOTE: this method will Notify with a SUCCESS_DEVICE_VALID or FAIL_DEVICE_INVALID asynchronously
	bool StartGetIsValid();

	/// Gets called every frame when processing a GetIsValid command
	bool Update();

	/** @name accessor functions */
	/// Most of these functions have possible calls to ForceDeviceInfoPopulate, which can yield a DEVICE_NOT_CONNECTED notification,
	///	and as a consequence these functions can't be declared const.
	//@{

	int GetDeviceID() const { return device_id; };
	int GetType();
	ULONGLONG GetTotalSpace();
	bool GetRemainingSpace( ULONGLONG& space_out );
	const MabString& GetFriendlyName();

	//@}

	/** @name overloaded operators */
	//@{

	XboxOneMabSaveDevice& operator=( const XboxOneMabSaveDevice& that );
	inline bool operator==( const XboxOneMabSaveDevice& rhs ) { return device_id == rhs.device_id; }
	inline bool operator!=( const XboxOneMabSaveDevice& rhs ) { return device_id != rhs.device_id; }
	//@}

	/// Static error function that tests for device disconnection with all of MS's highly-varied error messages.
	static bool IsDeviceDisconnected( DWORD ret_val_in );

private:
	//< the observer that this class has attached
	MabObserver<XboxOneMabSaveDeviceMessage>* observer;

	//< the MS device_id of the device in question
	int device_id;

	//< the internal information about the device; populated by ForceDeviceInfoPopulate
	void* internal_info;

	//< the internal MabString representation of internal_info's friendly name
	MabString internal_friendly_name;

	//< a boolean that communicates whether this device was attached to the manager on creation
	bool was_attached;
	/// Copy Constructor -- should never be called
	XboxOneMabSaveDevice( const XboxOneMabSaveDevice& that );

	/// Internal function to be called by all constructors
	void Initialise();

	///	Populate this Device with information--should only be called when a Device has been successfully set. Note that
	///	this operation is synchronous.
	bool ForceDeviceInfoPopulate();

	/// @name Friend classes
	///	@{

	// needs to be friended b/c it needs direct r/w access to device_id
	friend class SIFXboxOneSaveFSMStateRaiseDevice; 
	friend class XboxOneMabSaveDataDevice;
	/// @}

};

#endif //#ifndef XBOXONE_MAB_SAVE_DEVICE