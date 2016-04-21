/**
 * @file XboxOneMabUtils.h
 * @Xbox One utils class.
 * @author jaydens
 * @05/11/2014
 */

#pragma once

#include "pch.h"
#include <stdio.h>
#include "collection.h"
#include <synchapi.h>

class XboxOneMabUtils
{
public:
    static Windows::Xbox::Multiplayer::MultiplayerSessionReference^
    ConvertToWindowsXboxMultiplayerSessionReference(
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ sessionRef
        );

    static Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^
    ConvertToMicrosoftXboxServicesMultiplayerSessionReference(
        Windows::Xbox::Multiplayer::MultiplayerSessionReference^ sessionRef
        );

    static Platform::String^
    RemoveBracesFromGuidString(
        __in Platform::String^ guidString
        );

    static Platform::String^ FormatString( LPCWSTR strMsg, ... );

    static void AddNewStringToVector( WCHAR* string, Platform::Collections::Vector<Platform::String^>^ vector );

    static Platform::String^ GetBase64String(Windows::Storage::Streams::IBuffer^ buffer);

    static double GetNamedNumberWithValue( Windows::Data::Json::JsonObject^ json , Platform::String^ name, double defaultValue );

    static Platform::String^ ConvertHResultToErrorName( HRESULT hr );
    static Platform::String^ GetErrorString( HRESULT hr );

    static Windows::Foundation::DateTime GetCurrentTime();
    static double GetTimeBetweenInSeconds(Windows::Foundation::DateTime dt1, Windows::Foundation::DateTime dt2);
    static Platform::String^ DateTimeToString( __in Windows::Foundation::DateTime dateTime );
    static Windows::Foundation::DateTime StringToDateTime( Platform::String^ string );

    static bool IsStringEqualCaseInsenstive( Platform::String^ xboxUserId1, Platform::String^ xboxUserId2 );
	static void LogComment(Platform::String^ str);
	static void LogCommentWithError( Platform::String^ strText, HRESULT hr );
	static void LogCommentFormat( LPCWSTR strMsg, ... );
	static bool CompareWindowsXboxMultiplayerSessionReference(
				Windows::Xbox::Multiplayer::MultiplayerSessionReference^ sessionA,
				Windows::Xbox::Multiplayer::MultiplayerSessionReference^ sessionB
				);

	static Platform::String^ GetErrorStringForException(Platform::Exception^ ex);
	static bool CreatePeerNetwork(bool becomeServer = false);
	static bool EstablishPeerNetwork(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session);
	static Platform::String^ FormatUsername(Windows::Xbox::System::User^ user);

	static Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerQualityOfServiceMeasurements^>^>^
		MeasureQosForSessionAsync(
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session
		);
	static void ConvertPlatformStringToCharArray(Platform::String^ string, char* outputstring);

	static void ConvertPlatformStringToMabString(Platform::String^ string, MabString& outputMabString);

	static Platform::String^ ConvertMabStringToPlatformString(const MabString* string);

	static Platform::String^ ConvertCharStringToPlatformString(const char*);

	static Platform::String^ PrintSecureDeviceAssociation(Windows::Xbox::Networking::SecureDeviceAssociation^ association);

	//for stream convert
	static void ConvertPlatformIBufferToBytes( __in Windows::Storage::Streams::IBuffer^ buffer, __out byte** ppOut );
	static Windows::Storage::Streams::IBuffer^ ConvertByteArrayToPlatformIBuffer(__in Platform::Array<byte>^ array);
	
	template <class T>
	Windows::Foundation::Collections::IVectorView<T>^ ConvertMabVectorToVecterView(__in MabVector<T>);

	//Some Old function names, not use it anymore
	//static Platform::Array<byte>^ BufferToArray(__in Windows::Storage::Streams::IBuffer^ buffer);
	//static Windows::Storage::Streams::IBuffer^ ArrayToBuffer(__in Platform::Array<byte>^ array);


	//For type to object convert
	static Platform::Object^ IntToPlatformObject(__in int val);
	static bool AreSecureDeviceAddressesEqual( Windows::Xbox::Networking::SecureDeviceAssociation^ secureDeviceAssociation1, Windows::Xbox::Networking::SecureDeviceAssociation^ secureDeviceAssociation2);

	static void GetUserInfoFromXUIDs(__in Windows::Foundation::Collections::IVectorView<Platform::String^>^ XUIDs,__out Platform::Collections::Vector<Microsoft::Xbox::Services::Social::XboxUserProfile^>^ userInfo);

	static bool VerifyStringResult(__in Platform::String^ userString);
private:
	typedef std::map<std::wstring, std::map<Windows::Xbox::Networking::QualityOfServiceMetric, uint32>> QosMap;
    static wchar_t GetValueDigit(int value);
    static int GetDigitValue(wchar_t digit);
	static   QosMap MeasureQosForSession(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session);
	static Platform::String^ PrintSocketAddress(_In_ UINT32 sockaddrSize,_In_ const SOCKADDR* sockaddr);
};