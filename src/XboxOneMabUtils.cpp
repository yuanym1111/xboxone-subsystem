//// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//// PARTICULAR PURPOSE.
////
//// Copyright (c) Microsoft Corporation. All rights reserved
#include "pch.h"
#include "XboxOneMabUtils.h"
#include "XboxOneMabStateManager.h"
#include "XboxOneMabSessionManager.h"
#include "XboxOneMabUISessionController.h"
#include "XboxOneMabUserInfo.h"

#ifndef BUILD_RETAIL
#include "debugapi.h"
#endif

// for conversion from seconds (int) to TimeSpan (perhaps use _XTIME_TICKS_PER_TIME_T instead)
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::Networking;
using namespace Concurrency;

Windows::Xbox::Multiplayer::MultiplayerSessionReference^
XboxOneMabUtils::ConvertToWindowsXboxMultiplayerSessionReference(
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ sessionRef
    )
{
    return ref new Windows::Xbox::Multiplayer::MultiplayerSessionReference(
        sessionRef->SessionName,
        sessionRef->ServiceConfigurationId,
        sessionRef->SessionTemplateName
        );
}

Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^
XboxOneMabUtils::ConvertToMicrosoftXboxServicesMultiplayerSessionReference(
    Windows::Xbox::Multiplayer::MultiplayerSessionReference^ sessionRef
    )
{
    return ref new Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference(
        sessionRef->ServiceConfigurationId,
        sessionRef->SessionTemplateName,
        sessionRef->SessionName
        );
}

Platform::String^ XboxOneMabUtils::FormatString( LPCWSTR strMsg, ... )
{
    WCHAR strBuffer[2048];

    va_list args;
    va_start(args, strMsg);
    _vsnwprintf_s( strBuffer, 2048, _TRUNCATE, strMsg, args );
    strBuffer[2047] = L'\0';

    va_end(args);

    Platform::String^ str = ref new Platform::String(strBuffer);
    return str;
}

Platform::String^
XboxOneMabUtils::RemoveBracesFromGuidString(
    __in Platform::String^ guid
    )
{
    std::wstring strGuid = guid->ToString()->Data();

    if(strGuid.length() > 0 && strGuid[0] == L'{')
    {
        // Remove the {
        strGuid.erase(0, 1);
    }

    if(strGuid.length() > 0 && strGuid[strGuid.length() - 1] == L'}')
    {
        // Remove the }
        strGuid.erase(strGuid.end() - 1, strGuid.end());
    }

    return ref new Platform::String(strGuid.c_str());
}

void XboxOneMabUtils::AddNewStringToVector( WCHAR* string, Platform::Collections::Vector<Platform::String^>^ vector )
{
    if ( vector->Size == 0 )
    {
        vector->Append( ref new Platform::String( string ) );
    }
    else if ( !vector->GetAt( vector->Size-1)->Equals( ref new Platform::String( string )) )
    {
        vector->Append( ref new Platform::String( string ) );
    }
}

double XboxOneMabUtils::GetNamedNumberWithValue( 
    Windows::Data::Json::JsonObject^ json, 
    Platform::String^ name, 
    double defaultValue
    )
{
    if( !json->HasKey(name) )
    {
        return defaultValue;
    }
    else
    {
        return json->GetNamedNumber(name);
    }
}

Platform::String^ XboxOneMabUtils::ConvertHResultToErrorName( HRESULT hr )
{
    switch( hr )
    {
        // Generic errors
        case S_OK: return L"S_OK";
        case S_FALSE: return L"S_FALSE";
        case E_OUTOFMEMORY: return L"E_OUTOFMEMORY";
        case E_ACCESSDENIED: return L"E_ACCESSDENIED";
        case E_INVALIDARG: return L"E_INVALIDARG";
        case E_UNEXPECTED: return L"E_UNEXPECTED";
        case E_ABORT: return L"E_ABORT";
        case E_FAIL: return L"E_FAIL";
        case E_NOTIMPL: return L"E_NOTIMPL";
        case E_ILLEGAL_METHOD_CALL: return L"E_ILLEGAL_METHOD_CALL";

        // Authentication specific errors
        case 0x87DD0003: return L"AM_E_XASD_UNEXPECTED";
        case 0x87DD0004: return L"AM_E_XASU_UNEXPECTED";
        case 0x87DD0005: return L"AM_E_XAST_UNEXPECTED";
        case 0x87DD0006: return L"AM_E_XSTS_UNEXPECTED";
        case 0x87DD0007: return L"AM_E_XDEVICE_UNEXPECTED";
        case 0x87DD0008: return L"AM_E_DEVMODE_NOT_AUTHORIZED";
        case 0x87DD0009: return L"AM_E_NOT_AUTHORIZED";
        case 0x87DD000A: return L"AM_E_FORBIDDEN";
        case 0x87DD000B: return L"AM_E_UNKNOWN_TARGET";
        case 0x87DD000C: return L"AM_E_INVALID_NSAL_DATA";
        case 0x87DD000D: return L"AM_E_TITLE_NOT_AUTHENTICATED";
        case 0x87DD000E: return L"AM_E_TITLE_NOT_AUTHORIZED";
        case 0x87DD000F: return L"AM_E_DEVICE_NOT_AUTHENTICATED";
        case 0x87DD0010: return L"AM_E_INVALID_USER_INDEX";
        case 0x87DD0011: return L"AM_E_USER_HASH_MISSING";
        case 0x87DD0012: return L"AM_E_ACTOR_NOT_SPECIFIED";
        case 0x87DD0013: return L"AM_E_USER_NOT_FOUND";
        case 0x87DD0014: return L"AM_E_INVALID_SUBTOKEN";
        case 0x87DD0015: return L"AM_E_INVALID_ENVIRONMENT";
        case 0x87DD0016: return L"AM_E_XASD_TIMEOUT";
        case 0x87DD0017: return L"AM_E_XASU_TIMEOUT";
        case 0x87DD0018: return L"AM_E_XAST_TIMEOUT";
        case 0x87DD0019: return L"AM_E_XSTS_TIMEOUT";
        case 0x8015DC00: return L"XO_E_DEVMODE_NOT_AUTHORIZED";
        case 0x8015DC01: return L"XO_E_SYSTEM_UPDATE_REQUIRED";
        case 0x8015DC02: return L"XO_E_CONTENT_UPDATE_REQUIRED";
        case 0x8015DC03: return L"XO_E_ENFORCEMENT_BAN";
        case 0x8015DC04: return L"XO_E_THIRD_PARTY_BAN";
        case 0x8015DC05: return L"XO_E_ACCOUNT_PARENTALLY_RESTRICTED";
        case 0x8015DC06: return L"XO_E_DEVICE_SUBSCRIPTION_NOT_ACTIVATED";
        case 0x8015DC08: return L"XO_E_ACCOUNT_BILLING_MAINTENANCE_REQUIRED";
        case 0x8015DC09: return L"XO_E_ACCOUNT_CREATION_REQUIRED";
        case 0x8015DC0A: return L"XO_E_ACCOUNT_TERMS_OF_USE_NOT_ACCEPTED";
        case 0x8015DC0B: return L"XO_E_ACCOUNT_COUNTRY_NOT_AUTHORIZED";
        case 0x8015DC0C: return L"XO_E_ACCOUNT_AGE_VERIFICATION_REQUIRED";
        case 0x8015DC0D: return L"XO_E_ACCOUNT_CURFEW";
        case 0x8015DC0E: return L"XO_E_ACCOUNT_ZEST_MAINTENANCE_REQUIRED";
        case 0x8015DC0F: return L"XO_E_ACCOUNT_CSV_TRANSITION_REQUIRED";
        case 0x8015DC10: return L"XO_E_ACCOUNT_MAINTENANCE_REQUIRED";
        case 0x8015DC11: return L"XO_E_ACCOUNT_TYPE_NOT_ALLOWED";
        case 0x8015DC12: return L"XO_E_CONTENT_ISOLATION";
        case 0x8015DC13: return L"XO_E_ACCOUNT_NAME_CHANGE_REQUIRED";
        case 0x8015DC14: return L"XO_E_DEVICE_CHALLENGE_REQUIRED";
        case 0x8015DC20: return L"XO_E_EXPIRED_DEVICE_TOKEN";
        case 0x8015DC21: return L"XO_E_EXPIRED_TITLE_TOKEN";
        case 0x8015DC22: return L"XO_E_EXPIRED_USER_TOKEN";
        case 0x8015DC23: return L"XO_E_INVALID_DEVICE_TOKEN";
        case 0x8015DC24: return L"XO_E_INVALID_TITLE_TOKEN";
        case 0x8015DC25: return L"XO_E_INVALID_USER_TOKEN";

        // winsock errors
        case MAKE_HRESULT(1,7,WSAEWOULDBLOCK) : return L"WSAEWOULDBLOCK";
        case MAKE_HRESULT(1,7,WSAEINPROGRESS) : return L"WSAEINPROGRESS";
        case MAKE_HRESULT(1,7,WSAEALREADY) : return L"WSAEALREADY";
        case MAKE_HRESULT(1,7,WSAENOTSOCK) : return L"WSAENOTSOCK";
        case MAKE_HRESULT(1,7,WSAEDESTADDRREQ) : return L"WSAEDESTADDRREQ";
        case MAKE_HRESULT(1,7,WSAEMSGSIZE) : return L"WSAEMSGSIZE";
        case MAKE_HRESULT(1,7,WSAEPROTOTYPE) : return L"WSAEPROTOTYPE";
        case MAKE_HRESULT(1,7,WSAENOPROTOOPT) : return L"WSAENOPROTOOPT";
        case MAKE_HRESULT(1,7,WSAEPROTONOSUPPORT) : return L"WSAEPROTONOSUPPORT";
        case MAKE_HRESULT(1,7,WSAESOCKTNOSUPPORT) : return L"WSAESOCKTNOSUPPORT";
        case MAKE_HRESULT(1,7,WSAEOPNOTSUPP) : return L"WSAEOPNOTSUPP";
        case MAKE_HRESULT(1,7,WSAEPFNOSUPPORT) : return L"WSAEPFNOSUPPORT";
        case MAKE_HRESULT(1,7,WSAEAFNOSUPPORT) : return L"WSAEAFNOSUPPORT";
        case MAKE_HRESULT(1,7,WSAEADDRINUSE) : return L"WSAEADDRINUSE";
        case MAKE_HRESULT(1,7,WSAEADDRNOTAVAIL) : return L"WSAEADDRNOTAVAIL";
        case MAKE_HRESULT(1,7,WSAENETDOWN) : return L"WSAENETDOWN";
        case MAKE_HRESULT(1,7,WSAENETUNREACH) : return L"WSAENETUNREACH";
        case MAKE_HRESULT(1,7,WSAENETRESET) : return L"WSAENETRESET";
        case MAKE_HRESULT(1,7,WSAECONNABORTED) : return L"WSAECONNABORTED";
        case MAKE_HRESULT(1,7,WSAECONNRESET) : return L"WSAECONNRESET";
        case MAKE_HRESULT(1,7,WSAENOBUFS) : return L"WSAENOBUFS";
        case MAKE_HRESULT(1,7,WSAEISCONN) : return L"WSAEISCONN";
        case MAKE_HRESULT(1,7,WSAENOTCONN) : return L"WSAENOTCONN";
        case MAKE_HRESULT(1,7,WSAESHUTDOWN) : return L"WSAESHUTDOWN";
        case MAKE_HRESULT(1,7,WSAETOOMANYREFS) : return L"WSAETOOMANYREFS";
        case MAKE_HRESULT(1,7,WSAETIMEDOUT) : return L"WSAETIMEDOUT";
        case MAKE_HRESULT(1,7,WSAECONNREFUSED) : return L"WSAECONNREFUSED";
        case MAKE_HRESULT(1,7,WSAELOOP) : return L"WSAELOOP";
        case MAKE_HRESULT(1,7,WSAENAMETOOLONG) : return L"WSAENAMETOOLONG";
        case MAKE_HRESULT(1,7,WSAEHOSTDOWN) : return L"WSAEHOSTDOWN";
        case MAKE_HRESULT(1,7,WSAEHOSTUNREACH) : return L"WSAEHOSTUNREACH";
        case MAKE_HRESULT(1,7,WSAENOTEMPTY) : return L"WSAENOTEMPTY";
        case MAKE_HRESULT(1,7,WSAEPROCLIM) : return L"WSAEPROCLIM";
        case MAKE_HRESULT(1,7,WSAEUSERS) : return L"WSAEUSERS";
        case MAKE_HRESULT(1,7,WSAEDQUOT) : return L"WSAEDQUOT";
        case MAKE_HRESULT(1,7,WSAESTALE) : return L"WSAESTALE";
        case MAKE_HRESULT(1,7,WSAEREMOTE) : return L"WSAEREMOTE";
        case MAKE_HRESULT(1,7,WSASYSNOTREADY) : return L"WSASYSNOTREADY";
        case MAKE_HRESULT(1,7,WSAVERNOTSUPPORTED) : return L"WSAVERNOTSUPPORTED";
        case MAKE_HRESULT(1,7,WSANOTINITIALISED) : return L"WSANOTINITIALISED";
        case MAKE_HRESULT(1,7,WSAEDISCON) : return L"WSAEDISCON";
        case MAKE_HRESULT(1,7,WSAENOMORE) : return L"WSAENOMORE";
        case MAKE_HRESULT(1,7,WSAECANCELLED) : return L"WSAECANCELLED";
        case MAKE_HRESULT(1,7,WSAEINVALIDPROCTABLE) : return L"WSAEINVALIDPROCTABLE";
        case MAKE_HRESULT(1,7,WSAEINVALIDPROVIDER) : return L"WSAEINVALIDPROVIDER";
        case MAKE_HRESULT(1,7,WSAEPROVIDERFAILEDINIT) : return L"WSAEPROVIDERFAILEDINIT";
        case MAKE_HRESULT(1,7,WSASYSCALLFAILURE) : return L"WSASYSCALLFAILURE";
        case MAKE_HRESULT(1,7,WSASERVICE_NOT_FOUND) : return L"WSASERVICE_NOT_FOUND";
        case MAKE_HRESULT(1,7,WSATYPE_NOT_FOUND) : return L"WSATYPE_NOT_FOUND";
        case MAKE_HRESULT(1,7,WSA_E_NO_MORE) : return L"WSA_E_NO_MORE";
        case MAKE_HRESULT(1,7,WSA_E_CANCELLED) : return L"WSA_E_CANCELLED";
        case MAKE_HRESULT(1,7,WSAEREFUSED) : return L"WSAEREFUSED";
        case MAKE_HRESULT(1,7,WSAHOST_NOT_FOUND) : return L"WSAHOST_NOT_FOUND";
        case MAKE_HRESULT(1,7,WSATRY_AGAIN) : return L"WSATRY_AGAIN";
        case MAKE_HRESULT(1,7,WSANO_RECOVERY) : return L"WSANO_RECOVERY";
        case MAKE_HRESULT(1,7,WSANO_DATA) : return L"WSANO_DATA";
        case MAKE_HRESULT(1,7,WSA_QOS_RECEIVERS) : return L"WSA_QOS_RECEIVERS";
        case MAKE_HRESULT(1,7,WSA_QOS_SENDERS) : return L"WSA_QOS_SENDERS";
        case MAKE_HRESULT(1,7,WSA_QOS_NO_SENDERS) : return L"WSA_QOS_NO_SENDERS";
        case MAKE_HRESULT(1,7,WSA_QOS_NO_RECEIVERS) : return L"WSA_QOS_NO_RECEIVERS";
        case MAKE_HRESULT(1,7,WSA_QOS_REQUEST_CONFIRMED) : return L"WSA_QOS_REQUEST_CONFIRMED";
        case MAKE_HRESULT(1,7,WSA_QOS_ADMISSION_FAILURE) : return L"WSA_QOS_ADMISSION_FAILURE";
        case MAKE_HRESULT(1,7,WSA_QOS_POLICY_FAILURE) : return L"WSA_QOS_POLICY_FAILURE";
        case MAKE_HRESULT(1,7,WSA_QOS_BAD_STYLE) : return L"WSA_QOS_BAD_STYLE";
        case MAKE_HRESULT(1,7,WSA_QOS_BAD_OBJECT) : return L"WSA_QOS_BAD_OBJECT";
        case MAKE_HRESULT(1,7,WSA_QOS_TRAFFIC_CTRL_ERROR) : return L"WSA_QOS_TRAFFIC_CTRL_ERROR";
        case MAKE_HRESULT(1,7,WSA_QOS_GENERIC_ERROR) : return L"WSA_QOS_GENERIC_ERROR";
        case MAKE_HRESULT(1,7,WSA_QOS_ESERVICETYPE) : return L"WSA_QOS_ESERVICETYPE";
        case MAKE_HRESULT(1,7,WSA_QOS_EFLOWSPEC) : return L"WSA_QOS_EFLOWSPEC";
        case MAKE_HRESULT(1,7,WSA_QOS_EPROVSPECBUF) : return L"WSA_QOS_EPROVSPECBUF";
        case MAKE_HRESULT(1,7,WSA_QOS_EFILTERSTYLE) : return L"WSA_QOS_EFILTERSTYLE";
        case MAKE_HRESULT(1,7,WSA_QOS_EFILTERTYPE) : return L"WSA_QOS_EFILTERTYPE";
        case MAKE_HRESULT(1,7,WSA_QOS_EFILTERCOUNT) : return L"WSA_QOS_EFILTERCOUNT";
        case MAKE_HRESULT(1,7,WSA_QOS_EOBJLENGTH) : return L"WSA_QOS_EOBJLENGTH";
        case MAKE_HRESULT(1,7,WSA_QOS_EFLOWCOUNT) : return L"WSA_QOS_EFLOWCOUNT";
        case MAKE_HRESULT(1,7,WSA_QOS_EUNKOWNPSOBJ) : return L"WSA_QOS_EUNKOWNPSOBJ";
        case MAKE_HRESULT(1,7,WSA_QOS_EPOLICYOBJ) : return L"WSA_QOS_EPOLICYOBJ";
        case MAKE_HRESULT(1,7,WSA_QOS_EFLOWDESC) : return L"WSA_QOS_EFLOWDESC";
        case MAKE_HRESULT(1,7,WSA_QOS_EPSFLOWSPEC) : return L"WSA_QOS_EPSFLOWSPEC";
        case MAKE_HRESULT(1,7,WSA_QOS_EPSFILTERSPEC) : return L"WSA_QOS_EPSFILTERSPEC";
        case MAKE_HRESULT(1,7,WSA_QOS_ESDMODEOBJ) : return L"WSA_QOS_ESDMODEOBJ";
        case MAKE_HRESULT(1,7,WSA_QOS_ESHAPERATEOBJ) : return L"WSA_QOS_ESHAPERATEOBJ";
        case MAKE_HRESULT(1,7,WSA_QOS_RESERVED_PETYPE) : return L"WSA_QOS_RESERVED_PETYPE";

        // HTTP specific errors
        case WEB_E_UNSUPPORTED_FORMAT: return L"WEB_E_UNSUPPORTED_FORMAT";
        case WEB_E_INVALID_XML: return L"WEB_E_INVALID_XML";
        case WEB_E_MISSING_REQUIRED_ELEMENT: return L"WEB_E_MISSING_REQUIRED_ELEMENT";
        case WEB_E_MISSING_REQUIRED_ATTRIBUTE: return L"WEB_E_MISSING_REQUIRED_ATTRIBUTE";
        case WEB_E_UNEXPECTED_CONTENT: return L"WEB_E_UNEXPECTED_CONTENT";
        case WEB_E_RESOURCE_TOO_LARGE: return L"WEB_E_RESOURCE_TOO_LARGE";
        case WEB_E_INVALID_JSON_STRING: return L"WEB_E_INVALID_JSON_STRING";
        case WEB_E_INVALID_JSON_NUMBER: return L"WEB_E_INVALID_JSON_NUMBER";
        case WEB_E_JSON_VALUE_NOT_FOUND: return L"WEB_E_JSON_VALUE_NOT_FOUND";
        case HTTP_E_STATUS_UNEXPECTED: return L"HTTP_E_STATUS_UNEXPECTED";
        case HTTP_E_STATUS_UNEXPECTED_REDIRECTION: return L"HTTP_E_STATUS_UNEXPECTED_REDIRECTION";
        case HTTP_E_STATUS_UNEXPECTED_CLIENT_ERROR: return L"HTTP_E_STATUS_UNEXPECTED_CLIENT_ERROR";
        case HTTP_E_STATUS_UNEXPECTED_SERVER_ERROR: return L"HTTP_E_STATUS_UNEXPECTED_SERVER_ERROR";
        case HTTP_E_STATUS_AMBIGUOUS: return L"HTTP_E_STATUS_AMBIGUOUS";
        case HTTP_E_STATUS_MOVED: return L"HTTP_E_STATUS_MOVED";
        case HTTP_E_STATUS_REDIRECT: return L"HTTP_E_STATUS_REDIRECT";
        case HTTP_E_STATUS_REDIRECT_METHOD: return L"HTTP_E_STATUS_REDIRECT_METHOD";
        case HTTP_E_STATUS_NOT_MODIFIED: return L"HTTP_E_STATUS_NOT_MODIFIED";
        case HTTP_E_STATUS_USE_PROXY: return L"HTTP_E_STATUS_USE_PROXY";
        case HTTP_E_STATUS_REDIRECT_KEEP_VERB: return L"HTTP_E_STATUS_REDIRECT_KEEP_VERB";
        case HTTP_E_STATUS_BAD_REQUEST: return L"HTTP_E_STATUS_BAD_REQUEST";
        case HTTP_E_STATUS_DENIED: return L"HTTP_E_STATUS_DENIED";
        case HTTP_E_STATUS_PAYMENT_REQ: return L"HTTP_E_STATUS_PAYMENT_REQ";
        case HTTP_E_STATUS_FORBIDDEN: return L"HTTP_E_STATUS_FORBIDDEN";
        case HTTP_E_STATUS_NOT_FOUND: return L"HTTP_E_STATUS_NOT_FOUND";
        case HTTP_E_STATUS_BAD_METHOD: return L"HTTP_E_STATUS_BAD_METHOD";
        case HTTP_E_STATUS_NONE_ACCEPTABLE: return L"HTTP_E_STATUS_NONE_ACCEPTABLE";
        case HTTP_E_STATUS_PROXY_AUTH_REQ: return L"HTTP_E_STATUS_PROXY_AUTH_REQ";
        case HTTP_E_STATUS_REQUEST_TIMEOUT: return L"HTTP_E_STATUS_REQUEST_TIMEOUT";
        case HTTP_E_STATUS_CONFLICT: return L"HTTP_E_STATUS_CONFLICT";
        case HTTP_E_STATUS_GONE: return L"HTTP_E_STATUS_GONE";
        case HTTP_E_STATUS_LENGTH_REQUIRED: return L"HTTP_E_STATUS_LENGTH_REQUIRED";
        case HTTP_E_STATUS_PRECOND_FAILED: return L"HTTP_E_STATUS_PRECOND_FAILED";
        case HTTP_E_STATUS_REQUEST_TOO_LARGE: return L"HTTP_E_STATUS_REQUEST_TOO_LARGE";
        case HTTP_E_STATUS_URI_TOO_LONG: return L"HTTP_E_STATUS_URI_TOO_LONG";
        case HTTP_E_STATUS_UNSUPPORTED_MEDIA: return L"HTTP_E_STATUS_UNSUPPORTED_MEDIA";
        case HTTP_E_STATUS_RANGE_NOT_SATISFIABLE: return L"HTTP_E_STATUS_RANGE_NOT_SATISFIABLE";
        case HTTP_E_STATUS_EXPECTATION_FAILED: return L"HTTP_E_STATUS_EXPECTATION_FAILED";
        case HTTP_E_STATUS_SERVER_ERROR: return L"HTTP_E_STATUS_SERVER_ERROR";
        case HTTP_E_STATUS_NOT_SUPPORTED: return L"HTTP_E_STATUS_NOT_SUPPORTED";
        case HTTP_E_STATUS_BAD_GATEWAY: return L"HTTP_E_STATUS_BAD_GATEWAY";
        case HTTP_E_STATUS_SERVICE_UNAVAIL: return L"HTTP_E_STATUS_SERVICE_UNAVAIL";
        case HTTP_E_STATUS_GATEWAY_TIMEOUT: return L"HTTP_E_STATUS_GATEWAY_TIMEOUT";
        case HTTP_E_STATUS_VERSION_NOT_SUP: return L"HTTP_E_STATUS_VERSION_NOT_SUP";

        // WinINet specific errors
        case INET_E_INVALID_URL: return L"INET_E_INVALID_URL";
        case INET_E_NO_SESSION: return L"INET_E_NO_SESSION";
        case INET_E_CANNOT_CONNECT: return L"INET_E_CANNOT_CONNECT";
        case INET_E_RESOURCE_NOT_FOUND: return L"INET_E_RESOURCE_NOT_FOUND";
        case INET_E_OBJECT_NOT_FOUND: return L"INET_E_OBJECT_NOT_FOUND";
        case INET_E_DATA_NOT_AVAILABLE: return L"INET_E_DATA_NOT_AVAILABLE";
        case INET_E_DOWNLOAD_FAILURE: return L"INET_E_DOWNLOAD_FAILURE";
        case INET_E_AUTHENTICATION_REQUIRED: return L"INET_E_AUTHENTICATION_REQUIRED";
        case INET_E_NO_VALID_MEDIA: return L"INET_E_NO_VALID_MEDIA";
        case INET_E_CONNECTION_TIMEOUT: return L"INET_E_CONNECTION_TIMEOUT";
        case INET_E_INVALID_REQUEST: return L"INET_E_INVALID_REQUEST";
        case INET_E_UNKNOWN_PROTOCOL: return L"INET_E_UNKNOWN_PROTOCOL";
        case INET_E_SECURITY_PROBLEM: return L"INET_E_SECURITY_PROBLEM";
        case INET_E_CANNOT_LOAD_DATA: return L"INET_E_CANNOT_LOAD_DATA";
        case INET_E_CANNOT_INSTANTIATE_OBJECT: return L"INET_E_CANNOT_INSTANTIATE_OBJECT";
        case INET_E_INVALID_CERTIFICATE: return L"INET_E_INVALID_CERTIFICATE";
        case INET_E_REDIRECT_FAILED: return L"INET_E_REDIRECT_FAILED";
        case INET_E_REDIRECT_TO_DIR: return L"INET_E_REDIRECT_TO_DIR";
    }

    return L"";
}

Platform::String^ XboxOneMabUtils::GetErrorString( HRESULT hr )
{
    Platform::String^ str = XboxOneMabUtils::FormatString(L" %s [0x%0.8x]", ConvertHResultToErrorName(hr)->Data(), hr );
    return str;
}

Windows::Foundation::DateTime 
XboxOneMabUtils::GetCurrentTime() 
{
    ULARGE_INTEGER uInt;
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uInt.LowPart = ft.dwLowDateTime;
    uInt.HighPart = ft.dwHighDateTime;

    Windows::Foundation::DateTime time;
    time.UniversalTime = uInt.QuadPart;
    return time;
}

double 
XboxOneMabUtils::GetTimeBetweenInSeconds(Windows::Foundation::DateTime dt1, Windows::Foundation::DateTime dt2)
{
    const uint64 tickPerSecond = 10000000i64;
    uint64 deltaTime = dt2.UniversalTime - dt1.UniversalTime;
    return (double)deltaTime / (double)tickPerSecond;
}
bool XboxOneMabUtils::IsStringEqualCaseInsenstive( Platform::String^ val1, Platform::String^ val2 )
{
    return ( _wcsicmp(val1->Data(), val2->Data()) == 0 );
}

void XboxOneMabUtils::LogComment(Platform::String^ str)
{

#ifndef BUILD_RETAIL
	OutputDebugString(str->Data());
	OutputDebugString(L"\r\n");
#endif
}

void XboxOneMabUtils::LogCommentWithError( Platform::String^ strText, HRESULT hr )
{
#ifndef BUILD_RETAIL
	OutputDebugString( strText->Data() + hr);
#endif
}

void XboxOneMabUtils::LogCommentFormat( LPCWSTR strMsg, ... )
{
#ifndef BUILD_RETAIL
    WCHAR strBuffer[2048];

    va_list args;
    va_start(args, strMsg);
    _vsnwprintf_s( strBuffer, 2048, _TRUNCATE, strMsg, args );
    strBuffer[2047] = L'\0';

    va_end(args);

    LogComment(ref new Platform::String(strBuffer));
#endif
}

bool XboxOneMabUtils::CompareWindowsXboxMultiplayerSessionReference(
    Windows::Xbox::Multiplayer::MultiplayerSessionReference^ sessionA,
    Windows::Xbox::Multiplayer::MultiplayerSessionReference^ sessionB
    )
{

#ifndef BUILD_RETAIL
	OutputDebugString(sessionA->SessionName->Data());
	OutputDebugString(sessionB->SessionName->Data());
	OutputDebugString(sessionA->SessionTemplateName->Data());
	OutputDebugString(sessionB->SessionTemplateName->Data());
	OutputDebugString(sessionA->ServiceConfigurationId->Data());
	OutputDebugString(sessionB->ServiceConfigurationId->Data());
#endif

    if (sessionA != nullptr && sessionB != nullptr)
    {
        return IsStringEqualCaseInsenstive(sessionA->SessionName, sessionB->SessionName) &&
            IsStringEqualCaseInsenstive(sessionA->SessionTemplateName, sessionB->SessionTemplateName) &&
            IsStringEqualCaseInsenstive(sessionA->ServiceConfigurationId, sessionB->ServiceConfigurationId);
    }

    // This will handle cases when one is null and the other isn't
    return sessionA == sessionB;
}

Platform::String^ XboxOneMabUtils::GetErrorStringForException(Platform::Exception^ ex)
{
	if (ex == nullptr)
	{
		return "Unknown error";
	}

	wchar_t buffer[12];
	swprintf_s(buffer, 12, L"0x%08X", ex->HResult);
	auto errorCode = ref new Platform::String(buffer);

	if (ex->Message == nullptr || ex->Message->Length() == 0)
	{
		return errorCode;
	}

	return ex->Message + " (" + errorCode + ")";
}

bool XboxOneMabUtils::CreatePeerNetwork(bool becomeServer /*= false*/)
{
	//Create my socket here, and shut down old one
	try
	{
		LogComment(L"Initilize network manager here");
		//Send a message to UI to notify that local session is created
		if(becomeServer){
			XboxOneMabUISessionController::Get()->onHostSession();

			/*
			MultiplayerSession^ session = XboxOneSessionManager->GetGameSession();
			
			// Become host if we're the first in the game session
			if (session->Members->Size == 1 && session->Members->GetAt(0)->XboxUserId->Equals(session->CurrentUser->XboxUserId))
			{
				if(XboxOneStateManager->GetMyState() == XboxOneMabLib::GameState::Matchmaking)
					XboxOneSessionManager->TryBecomeGameHostAsync();

				//I'm the host, trigger the event here

				//send update msg including peer host info
				XboxOneMabNetOnlineClient sessionClient;

				XboxOneMabUISessionController::Get()->OnPeerJoin(sessionClient);

			}
			*/

		}
	}
	catch (...)
	{
		return false;
	}
	return true;
}

bool XboxOneMabUtils::EstablishPeerNetwork(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session)
{
	LogComment(L"XboxOneMabUtils::EstablishPeerNetwork()");
	MABASSERT(session != nullptr);

	if (session != nullptr)
	{
		if (XboxOneStateManager->GetMyState() == XboxOneMabLib::GameState::Matchmaking || XboxOneStateManager->GetMyState() == XboxOneMabLib::GameState::Lobby)
		{
			MABASSERT(XboxOneSessionManager->IsGameSession(session));
			// Become host if we're the first in the game session
			if (session->Members->GetAt(0)->XboxUserId->Equals(session->CurrentUser->XboxUserId))
			{
				if(XboxOneStateManager->GetMyState() == XboxOneMabLib::GameState::Matchmaking)
					create_task(
						XboxOneSessionManager->TryBecomeGameHostAsync()
					).wait();

				//I'm the host, trigger the event here

				//send update msg including peer host info
				XboxOneMabNetOnlineClient sessionClient;

				//TODO:convert session secure device info to client info
				XboxOneSessionManager->GetConnectedMabXboxOneClientDetails(session, &sessionClient);
				XboxOneMabUISessionController::Get()->OnPeerJoin(sessionClient);

			}else{

				//send update msg including peer client info
				XboxOneMabNetOnlineClient host;

				//TODO:convert session secure device info to client info
				XboxOneSessionManager->GetConnectedMabXboxOneClientDetails(session, &host);
				XboxOneMabUISessionController::Get()->OnJoin(host);
			}
		}
		//All these logic will move to MABUISessionContrller
		// Connect to everyone else in the session
		for (auto member : session->Members)
		{
			if (!member->IsCurrentUser)
			{
				Platform::String^ gamertag = member->Gamertag;
				LogCommentFormat(L"Connect to client with gametag: %s",gamertag);
				/*
				create_task(XboxConnectionManager->ConnectToPeerAsync(
					member->XboxUserId, 
					member->SecureDeviceAddressBase64)
					).then([gamertag](task<void> t)
				{
					try
					{
						// Observe any exceptions
						t.get();
					}
					catch(Platform::Exception^ ex)
					{
						//TODO: Failed to get connection generated
						
					}
				});
				*/
			}
		}
	}
	else
	{
		LogComment("Couldn't create session");
		return false;
	}

	return true;
}

Platform::String^ XboxOneMabUtils::FormatUsername(Windows::Xbox::System::User^ user)
{
	if (user != nullptr && user->DisplayInfo != nullptr)
	{
		return (user->DisplayInfo->GameDisplayName + " (ID: " + user->Id + ")");
	}
	else
	{
		return "(n/a)";
	}
}

Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerQualityOfServiceMeasurements^>^>^ XboxOneMabUtils::MeasureQosForSessionAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session)
{
	LogComment(L"XboxOneMabUtils::MeasureQosForSessionAsync()\n");
	return create_async([session]() -> IVectorView<MultiplayerQualityOfServiceMeasurements^>^
	{
		auto measurementMap = XboxOneMabUtils::MeasureQosForSession(session);
		auto measurements = ref new Platform::Collections::Vector<Microsoft::Xbox::Services::Multiplayer::MultiplayerQualityOfServiceMeasurements^>();

		for (const auto& mapElement : measurementMap)
		{
			auto deviceToken = mapElement.first;

			if (!deviceToken.empty())
			{
				auto peerMeasurement = mapElement.second;

				TimeSpan latency;
				latency.Duration = peerMeasurement[Windows::Xbox::Networking::QualityOfServiceMetric::LatencyAverage] * TICKS_PER_MILLISECOND;

				MultiplayerQualityOfServiceMeasurements^ qosMeasurement = ref new MultiplayerQualityOfServiceMeasurements(
					ref new Platform::String(deviceToken.c_str()),
					latency,
					peerMeasurement[Windows::Xbox::Networking::QualityOfServiceMetric::BandwidthDown],
					peerMeasurement[Windows::Xbox::Networking::QualityOfServiceMetric::BandwidthUp],
					L"{}"
					);

				measurements->Append(qosMeasurement);
			}
		}

		return measurements->GetView();
	});
}

XboxOneMabUtils::QosMap XboxOneMabUtils::MeasureQosForSession(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ session)
{
	LogComment(L"XboxOneMabUtils::MeasureQosForSession()\n");
	QosMap resultMap;
	std::map<std::wstring, std::wstring> addressDeviceTokenMap;

	auto addresses = ref new Platform::Collections::Vector<SecureDeviceAddress^>();
	auto metrics = ref new Platform::Collections::Vector<Windows::Xbox::Networking::QualityOfServiceMetric>();

	metrics->Append(Windows::Xbox::Networking::QualityOfServiceMetric::BandwidthDown);
	metrics->Append(Windows::Xbox::Networking::QualityOfServiceMetric::BandwidthUp);
	metrics->Append(Windows::Xbox::Networking::QualityOfServiceMetric::LatencyAverage);

	// Add secure addresses for each session member
	for (const auto& member : session->Members)
	{
		if (!member->IsCurrentUser)
		{
			Platform::String^ sda = member->SecureDeviceAddressBase64;

			if (!sda->IsEmpty())
			{
				addresses->Append(SecureDeviceAddress::FromBase64String(sda));
				addressDeviceTokenMap[sda->Data()] = member->DeviceToken->Data();
			}
		}
	}

	if (addresses->Size > 0)
	{
		create_task(
			Windows::Xbox::Networking::QualityOfService::MeasureQualityOfServiceAsync(
			addresses,
			metrics,
			10000,  // timeout in milliseconds
			4       // numberOfProbes
			)
			).then([&addressDeviceTokenMap, &resultMap] (MeasureQualityOfServiceResult^ result)
		{
			LogCommentFormat(L"measurement result size %d \n", result->Measurements->Size);

			if (result->Measurements->Size > 0)
			{
				for (const auto& measurement : result->Measurements)
				{
					auto address = measurement->SecureDeviceAddress->GetBase64String();
					auto deviceToken = addressDeviceTokenMap[address->Data()];
					auto status = measurement->Status;

					LogCommentFormat(L"\tmeasurement result : address: %ws\n", address->Data());
					LogCommentFormat(L"\tmeasurement result : status: %ws\n", status.ToString()->Data());

					if (status == Windows::Xbox::Networking::QualityOfServiceMeasurementStatus::PartialResults || 
						status == Windows::Xbox::Networking::QualityOfServiceMeasurementStatus::Success)
					{
						LogCommentFormat( L"\tmeasurement result : matric: %ws\n", measurement->Metric.ToString()->Data());
						LogCommentFormat( L"\tmeasurement result : value: %ws\n", measurement->MetricValue->ToString()->Data());

						resultMap[deviceToken][measurement->Metric] = measurement->MetricValue->GetUInt32();
					}
				}
			}
		}
		).wait();
	}

	return resultMap;
}

void XboxOneMabUtils::ConvertPlatformStringToCharArray(Platform::String^ string, char* outputstring)
{
	const wchar_t *stringData= string->Data();
	int size = wcslen( stringData );

	outputstring[ size ] = 0;
	for(int y=0;y<size; y++)
	{
		outputstring[y] = (char)stringData[y];
	}
}

Platform::String^ XboxOneMabUtils::ConvertMabStringToPlatformString(const MabString* string)
{
	return ConvertCharStringToPlatformString(string->data());
}

Platform::String^ XboxOneMabUtils::ConvertCharStringToPlatformString(const char* input)
{
	size_t newsize= strlen(input) + 1;
	wchar_t* wcstring = new wchar_t[newsize];
	size_t convertedChars = 0;

	mbstowcs_s(&convertedChars, wcstring, newsize, input, _TRUNCATE);
	Platform::String ^str = ref new Platform::String(wcstring);
	delete[] wcstring;

	return str;

}


void XboxOneMabUtils::ConvertPlatformStringToMabString(Platform::String^ string, MabString& outputMabString)
{
	//Todo:Found Bugs when met unicode characters - alex
	//Need to return with rectangular placeholder characters instead
	const wchar_t *stringData= string->Data();
	int size = wcslen( stringData );
	char* outputstring = new char(size+1);

	ConvertPlatformStringToCharArray(string, outputstring);
	outputMabString.assign(MabString(outputstring));
	delete outputstring;
}



Platform::String^ XboxOneMabUtils::PrintSecureDeviceAssociation(Windows::Xbox::Networking::SecureDeviceAssociation^ association)
{
	try
	{
		Platform::String^ textLocal;
			SOCKADDR_STORAGE localSocketAddress;
			Platform::ArrayReference<BYTE> localSocketAddressBytesFromAssociationInTemplate(
				(BYTE*) &localSocketAddress,
				sizeof(localSocketAddress));
			association->GetLocalSocketAddressBytes(localSocketAddressBytesFromAssociationInTemplate);

			textLocal = PrintSocketAddress( sizeof(localSocketAddress), (SOCKADDR*) &localSocketAddress );

		Platform::String^ textRemote;

			SOCKADDR_STORAGE remoteSocketAddress;
			Platform::ArrayReference<BYTE> remoteSocketAddressBytesFromAssociationInTemplate(
				(BYTE*) &remoteSocketAddress,
				sizeof(remoteSocketAddress));
			association->GetRemoteSocketAddressBytes(remoteSocketAddressBytesFromAssociationInTemplate);

			textRemote = PrintSocketAddress( sizeof(remoteSocketAddress), (SOCKADDR*) &remoteSocketAddress );
	
			return FormatString( L"Local:%ws Remote:%ws", textLocal->Data(), textRemote->Data());
	
	}
	catch (...)
	{
		return L"Failure printing address";
	}
}

Platform::String^ XboxOneMabUtils::PrintSocketAddress(_In_ UINT32 sockaddrSize,_In_ const SOCKADDR* sockaddr)
{
	int result;
	char hostname[256] = {0};
	char port[64] = {0};

	ZeroMemory(hostname, sizeof(hostname));
	result = getnameinfo(sockaddr,
		sockaddrSize,
		hostname,
		sizeof(hostname),
		port,
		sizeof( port ),
		(NI_NUMERICHOST | NI_NUMERICSERV));

	if (result == 0)
	{
		return FormatString(
			L"[%hs]:%hs",
			hostname,
			port
			);
	}
	else
	{
		result = WSAGetLastError();
		return FormatString( L"PrintSocketAddress: %d", result );
	}
}

void XboxOneMabUtils::ConvertPlatformIBufferToBytes(__in Windows::Storage::Streams::IBuffer^ buffer, __out byte** ppOut)
{
	if ( ppOut == nullptr || buffer == nullptr )
	{
		throw ref new Platform::InvalidArgumentException();
	}

	*ppOut = nullptr;

	Microsoft::WRL::ComPtr<IInspectable> srcBufferInspectable(reinterpret_cast<IInspectable*>( buffer ));
	Microsoft::WRL::ComPtr<Windows::Storage::Streams::IBufferByteAccess> srcBufferByteAccess;
	srcBufferInspectable.As(&srcBufferByteAccess);
	srcBufferByteAccess->Buffer(ppOut);
}

Windows::Storage::Streams::IBuffer^ XboxOneMabUtils::ConvertByteArrayToPlatformIBuffer(__in Platform::Array<byte>^ array)
{
	Windows::Storage::Streams::DataWriter^ writer = ref new Windows::Storage::Streams::DataWriter();
	writer->WriteBytes(array);
	return writer->DetachBuffer();
}

Platform::Object^ XboxOneMabUtils::IntToPlatformObject( 
	__in int val 
	)
{
	return (Platform::Object^)val;
	//return Windows::Foundation::PropertyValue::CreateInt32(val);
}

bool XboxOneMabUtils::AreSecureDeviceAddressesEqual(Windows::Xbox::Networking::SecureDeviceAssociation^ secureDeviceAssociation1, Windows::Xbox::Networking::SecureDeviceAssociation^ secureDeviceAssociation2)
{
	if( secureDeviceAssociation1 != nullptr && 
		secureDeviceAssociation2 != nullptr && 
		secureDeviceAssociation1->RemoteSecureDeviceAddress != nullptr &&
		secureDeviceAssociation2->RemoteSecureDeviceAddress != nullptr &&
		secureDeviceAssociation1->RemoteSecureDeviceAddress->Compare(secureDeviceAssociation2->RemoteSecureDeviceAddress) == 0 )
	{
		return true;
	}
	return false;
}




template <class T>
Windows::Foundation::Collections::IVectorView<T>^
	XboxOneMabUtils::ConvertMabVectorToVecterView(__in MabVector<T> inputVector)
{
	Platform::Collections::Vector<T>^ outputIVector = ref new Platform::Collections::Vector<T>();

	for (T record : inputVector)
	{
		
		outputIVector->Append(record);
	}

	return outputIVector->GetView();

}


void XboxOneMabUtils::GetUserInfoFromXUIDs(__in Windows::Foundation::Collections::IVectorView<Platform::String^>^ XUIDs,__out Platform::Collections::Vector<Microsoft::Xbox::Services::Social::XboxUserProfile^>^ userInfo)
{
	Microsoft::Xbox::Services::XboxLiveContext^ m_xboxservice = XboxOneMabUserInfo::Get()->GetCurrentXboxLiveContext();
	if(m_xboxservice == nullptr)
	{
		return;
	}
	Microsoft::Xbox::Services::Social::ProfileService^ profileService = m_xboxservice->ProfileService;
	if(profileService)
	{
		create_task(
			profileService->GetUserProfilesAsync(
			XUIDs
			)
			).then([&userInfo] (task<IVectorView<Microsoft::Xbox::Services::Social::XboxUserProfile^>^> t)
		{
			auto profiles = t.get();
			for (const auto& profile : profiles)
			{
				LogCommentFormat(L"profile display name %ws",profile->GameDisplayName->Data());
				LogCommentFormat(L"profile image %ws",profile->DisplayPictureUri->DisplayUri->Data());
				userInfo->Append(profile);
			}
		}
		).wait();
	}
}

bool XboxOneMabUtils::VerifyStringResult(__in Platform::String^ userString)
{
	Microsoft::Xbox::Services::XboxLiveContext^ m_xboxservice = XboxOneMabUserInfo::Get()->GetCurrentXboxLiveContext();
	if(m_xboxservice == nullptr)
	{
		return false;
	}

	if (userString->Length() == 0) return true;
	Microsoft::Xbox::Services::System::StringService^ stringService = m_xboxservice->StringService;
	if(stringService)
	{
		create_task(
			stringService->VerifyStringAsync(
			userString
			)
			).then([] (task<Microsoft::Xbox::Services::System::VerifyStringResult^> t)
		{
			try{
				auto stringresult = t.get();			
				if(stringresult->ResultCode != Microsoft::Xbox::Services::System::VerifyStringResultCode::Success)
				{
					return false;
				}				
				return true;
			}catch(Platform::COMException^ ex)
			{
				return false;
			}
		}
		).wait();
	}
}
