#include "pch.h"

#include "XboxOneMabConnect.h"
#include "XboxOneMabConnectManager.h"
#include "XboxOneMabSessionManager.h"
#include "XboxOneMabUtils.h"
#include "XboxOneMabGame.h"
#include "XboxOneMabUserInfo.h"

#include <MabNetOnlineClientDetails.h>

#include <xboxone/XboxOneMabNetTypes.h>
#include <MabNetTypes.h>
#include <MabNet.h>

#include "MabLog.h"


using namespace Concurrency;
using namespace Windows::Data;
using namespace Windows::Data::Json;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Microsoft::Xbox::Services;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace Windows::Xbox::Multiplayer;
using namespace Windows::Xbox::System;
using namespace Windows::Xbox::Networking;

using namespace XboxOneMabLib;
namespace XboxOneMabLib {


	static char g_ca[] ={"-----BEGIN CERTIFICATE-----\n"
		"MIIG2TCCBMGgAwIBAgIQWYTb0JERTq1Ag8xDe1mpMzANBgkqhkiG9w0BAQUFADBV\n"
		"MRMwEQYKCZImiZPyLGQBGRYDb3JnMRgwFgYKCZImiZPyLGQBGRYIZ2FtZWxvZnQx\n"
		"JDAiBgNVBAMTG0dhbWVsb2Z0IEVudGVycHJpc2UgUm9vdCBDQTAeFw0wNjA3MTky\n"
		"MDQzMzRaFw0yMTA3MTkyMDUyMTFaMFUxEzARBgoJkiaJk/IsZAEZFgNvcmcxGDAW\n"
		"BgoJkiaJk/IsZAEZFghnYW1lbG9mdDEkMCIGA1UEAxMbR2FtZWxvZnQgRW50ZXJw\n"
		"cmlzZSBSb290IENBMIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEA3r2k\n"
		"BEMxmilIyv80VhLCvKDXJbW+RWlOOQbArgkijJZ9Hq7+dZiNF1xiaI+TjPNn3Ay5\n"
		"MfF+0DuJ1qQHFzJ3zczquI2SAxiOhfDVxbqv/DwKgr1+ZW+WKUaApplaiNVZwmly\n"
		"BM/+kLwRD5mP0w0f1R4b1G2dki7PoDLzOMW0pN9KQ5m+KxxMAsiKYtZLGZb1qVaj\n"
		"IWcsTcMyUVglZRxpO10P1LvfTgZSjXahidaCifMGv0T6Cr/uzeXIOMVgM20UR5FM\n"
		"ZAlJiD5CmB68Psl5AauqRaTlhJJkyt0XRJhCNhfJlzpMCKmbw0iKuD4cAs3JVJCI\n"
		"aj3zlAZrvG0w3CGqZGX27uLm8ENUDZfVBDlPJZWtvsk+vlq8zOKmwC2ctmaAZpxd\n"
		"xUSFJepd4I4StwZm7ACflfTkPBSZlXj9N3kagsI9cl9rkO0v2Ux+VibjH4X7YG/M\n"
		"SN/JJDNpEMa6QWHCa6iAlp8A4iShP4R9/Dt0zr32ZuZY3R2ee8T0Mbpxf6yI0LN0\n"
		"N/uNEx48HFEwGWxMwm+jr0c5Bf5aqQuVfiSZ8odcB6bJIe0UO63Rkj03wsu5tHmV\n"
		"igZjxHmDiAOK6WJ4SpAhQhDN/DotDOLssDVKB7BWgPyfl9qX8ho5p1XbxiJ2uDk4\n"
		"XW0nnlz0n0yVphXP4sNkH9N7nU2vJttf2rtzgsECAwEAAaOCAaMwggGfMBMGCSsG\n"
		"AQQBgjcUAgQGHgQAQwBBMAsGA1UdDwQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0G\n"
		"A1UdDgQWBBQOjkvk0WdHMB7WRcqtpCfrglqvOjCCATcGA1UdHwSCAS4wggEqMIIB\n"
		"JqCCASKgggEehoHLbGRhcDovLy9DTj1HYW1lbG9mdCUyMEVudGVycHJpc2UlMjBS\n"
		"b290JTIwQ0EsQ049bWRjLWFjZTAxLENOPUNEUCxDTj1QdWJsaWMlMjBLZXklMjBT\n"
		"ZXJ2aWNlcyxDTj1TZXJ2aWNlcyxDTj1Db25maWd1cmF0aW9uLERDPWdhbWVsb2Z0\n"
		"LERDPW9yZz9jZXJ0aWZpY2F0ZVJldm9jYXRpb25MaXN0P2Jhc2U/b2JqZWN0Q2xh\n"
		"c3M9Y1JMRGlzdHJpYnV0aW9uUG9pbnSGTmh0dHA6Ly9tZGMtYWNlMDEuZ2FtZWxv\n"
		"ZnQub3JnL0NlcnRFbnJvbGwvR2FtZWxvZnQlMjBFbnRlcnByaXNlJTIwUm9vdCUy\n"
		"MENBLmNybDAQBgkrBgEEAYI3FQEEAwIBADANBgkqhkiG9w0BAQUFAAOCAgEACBOk\n"
		"nHZ6IL4sF71kA1XEuGPxDr/rAFL8w6xSqaUUJdjMjg6+elK2OXPcWd2ZsGZdL8Pj\n"
		"2qeZDymPfqlie9O0SP9E+QLMa0yG/yATbcNeVA04Ke9W7qyjkHbqwC6yJn2CKGIg\n"
		"fY+w82KMrAAb3rYHtyt98oG4AWesWubxMuB+gnKfXeCVSW6CzC66BySUqgWVn5DW\n"
		"UhAHtjR0buR59L1rDoYx9VrQs0xMdq9YA3GFeR6As8YI1boNlvYIlUuTgiDrsfHy\n"
		"K9bpX6HJK8kUNDaHq9jR6mOa6XFQHEmeQxGBD4zynHwdIvj/H57Qpj49P1RjOZhr\n"
		"LWUeIFEXin6TkVaci6H1QuNovwp7LJq/JSp27f2oA7ZDEWNvBhoYuLAWWbNYUlyD\n"
		"w94yIEQPayKIhiZtuHp+WuZlt8DrK7jHAbRKLovB/WHXq6X2Kp+HSJ9pJckkgRlO\n"
		"J+COTc7/Cqe9as51AtEMpt7Qtc0TQga4+3CCeRR7ywj1DkMRcQJL8ZmMvAlIVRBr\n"
		"hK5uXCGdNN1qWaoAazym1wrRWAHad2CIUek5p/OnXlFVDNaViyWke+K3HPpu6d7a\n"
		"ZvT1WMIlkrSAo8Jz7UYVWCp8zvyiBXuTbX5EgTRlFxlQE/Ls+560wCd+gW0SxTRb\n"
		"yHtOzWaIaMbN2z3BWJGdIojcoeFXw4b9Y0aF2rw=\n"
		"-----END CERTIFICATE-----\n"
		"-----BEGIN CERTIFICATE-----\n"
		"MIIEXDCCA0SgAwIBAgIEOGO5ZjANBgkqhkiG9w0BAQUFADCBtDEUMBIGA1UEChML\n"
		"RW50cnVzdC5uZXQxQDA+BgNVBAsUN3d3dy5lbnRydXN0Lm5ldC9DUFNfMjA0OCBp\n"
		"bmNvcnAuIGJ5IHJlZi4gKGxpbWl0cyBsaWFiLikxJTAjBgNVBAsTHChjKSAxOTk5\n"
		"IEVudHJ1c3QubmV0IExpbWl0ZWQxMzAxBgNVBAMTKkVudHJ1c3QubmV0IENlcnRp\n"
		"ZmljYXRpb24gQXV0aG9yaXR5ICgyMDQ4KTAeFw05OTEyMjQxNzUwNTFaFw0xOTEy\n"
		"MjQxODIwNTFaMIG0MRQwEgYDVQQKEwtFbnRydXN0Lm5ldDFAMD4GA1UECxQ3d3d3\n"
		"LmVudHJ1c3QubmV0L0NQU18yMDQ4IGluY29ycC4gYnkgcmVmLiAobGltaXRzIGxp\n"
		"YWIuKTElMCMGA1UECxMcKGMpIDE5OTkgRW50cnVzdC5uZXQgTGltaXRlZDEzMDEG\n"
		"A1UEAxMqRW50cnVzdC5uZXQgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkgKDIwNDgp\n"
		"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEArU1LqRKGsuqjIAcVFmQq\n"
		"K0vRvwtKTY7tgHalZ7d4QMBzQshowNtTK91euHaYNZOLGp18EzoOH1u3Hs/lJBQe\n"
		"sYGpjX24zGtLA/ECDNyrpUAkAH90lKGdCCmziAv1h3edVc3kw37XamSrhRSGlVuX\n"
		"MlBvPci6Zgzj/L24ScF2iUkZ/cCovYmjZy/Gn7xxGWC4LeksyZB2ZnuU4q941mVT\n"
		"XTzWnLLPKQP5L6RQstRIzgUyVYr9smRMDuSYB3Xbf9+5CFVghTAp+XtIpGmG4zU/\n"
		"HoZdenoVve8AjhUiVBcAkCaTvA5JaJG/+EfTnZVCwQ5N328mz8MYIWJmQ3DW1cAH\n"
		"4QIDAQABo3QwcjARBglghkgBhvhCAQEEBAMCAAcwHwYDVR0jBBgwFoAUVeSB0RGA\n"
		"vtiJuQijMfmhJAkWuXAwHQYDVR0OBBYEFFXkgdERgL7YibkIozH5oSQJFrlwMB0G\n"
		"CSqGSIb2fQdBAAQQMA4bCFY1LjA6NC4wAwIEkDANBgkqhkiG9w0BAQUFAAOCAQEA\n"
		"WUesIYSKF8mciVMeuoCFGsY8Tj6xnLZ8xpJdGGQC49MGCBFhfGPjK50xA3B20qMo\n"
		"oPS7mmNz7W3lKtvtFKkrxjYR0CvrB4ul2p5cGZ1WEvVUKcgF7bISKo30Axv/55IQ\n"
		"h7A6tcOdBTcSo8f0FbnVpDkWm1M6I5HxqIKiaohowXkCIryqptau37AUX7iH0N18\n"
		"f3v/rxzP5tsHrV7bhZ3QKw0z2wTR5klAEyt2+z7pnIkPFc4YsIV4IU9rTw76NmfN\n"
		"B/L/CNDi3tm/Kq+4h4YhPATKt5Rof8886ZjXOP/swNlQ8C5LWK5Gb9Auw2DaclVy\n"
		"vUxFnmG6v4SBkgPR0ml8xQ==\n"
		"-----END CERTIFICATE-----\n"
		"-----BEGIN CERTIFICATE-----\n"
		"MIIDxTCCAq2gAwIBAgIQAqxcJmoLQJuPC3nyrkYldzANBgkqhkiG9w0BAQUFADBs\n"
		"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
		"d3cuZGlnaWNlcnQuY29tMSswKQYDVQQDEyJEaWdpQ2VydCBIaWdoIEFzc3VyYW5j\n"
		"ZSBFViBSb290IENBMB4XDTA2MTExMDAwMDAwMFoXDTMxMTExMDAwMDAwMFowbDEL\n"
		"MAkGA1UEBhMCVVMxFTATBgNVBAoTDERpZ2lDZXJ0IEluYzEZMBcGA1UECxMQd3d3\n"
		"LmRpZ2ljZXJ0LmNvbTErMCkGA1UEAxMiRGlnaUNlcnQgSGlnaCBBc3N1cmFuY2Ug\n"
		"RVYgUm9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMbM5XPm\n"
		"+9S75S0tMqbf5YE/yc0lSbZxKsPVlDRnogocsF9ppkCxxLeyj9CYpKlBWTrT3JTW\n"
		"PNt0OKRKzE0lgvdKpVMSOO7zSW1xkX5jtqumX8OkhPhPYlG++MXs2ziS4wblCJEM\n"
		"xChBVfvLWokVfnHoNb9Ncgk9vjo4UFt3MRuNs8ckRZqnrG0AFFoEt7oT61EKmEFB\n"
		"Ik5lYYeBQVCmeVyJ3hlKV9Uu5l0cUyx+mM0aBhakaHPQNAQTXKFx01p8VdteZOE3\n"
		"hzBWBOURtCmAEvF5OYiiAhF8J2a3iLd48soKqDirCmTCv2ZdlYTBoSUeh10aUAsg\n"
		"EsxBu24LUTi4S8sCAwEAAaNjMGEwDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB/wQF\n"
		"MAMBAf8wHQYDVR0OBBYEFLE+w2kD+L9HAdSYJhoIAu9jZCvDMB8GA1UdIwQYMBaA\n"
		"FLE+w2kD+L9HAdSYJhoIAu9jZCvDMA0GCSqGSIb3DQEBBQUAA4IBAQAcGgaX3Nec\n"
		"nzyIZgYIVyHbIUf4KmeqvxgydkAQV8GK83rZEWWONfqe/EW1ntlMMUu4kehDLI6z\n"
		"eM7b41N5cdblIZQB2lWHmiRk9opmzN6cN82oNLFpmyPInngiK3BD41VHMWEZ71jF\n"
		"hS9OMPagMRYjyOfiZRYzy78aG6A9+MpeizGLYAiJLQwGXFK3xPkKmNEVX58Svnw2\n"
		"Yzi9RKR/5CYrCsSXaQ3pjOLAEFe4yHYSkVXySGnYvCoCWw9E1CAx2/S6cCZdkGCe\n"
		"vEsXCS+0yx5DaMkHJ8HSXPfqIbloEpw8nL+e/IBcm2PN7EeqJSdnoDfzAIJ9VNep\n"
		"+OkuE6N36B9K\n"
		"-----END CERTIFICATE-----\n"
		"-----BEGIN CERTIFICATE-----\n"
		"MIIE0zCCA7ugAwIBAgIQGNrRniZ96LtKIVjNzGs7SjANBgkqhkiG9w0BAQUFADCB\n"
		"yjELMAkGA1UEBhMCVVMxFzAVBgNVBAoTDlZlcmlTaWduLCBJbmMuMR8wHQYDVQQL\n"
		"ExZWZXJpU2lnbiBUcnVzdCBOZXR3b3JrMTowOAYDVQQLEzEoYykgMjAwNiBWZXJp\n"
		"U2lnbiwgSW5jLiAtIEZvciBhdXRob3JpemVkIHVzZSBvbmx5MUUwQwYDVQQDEzxW\n"
		"ZXJpU2lnbiBDbGFzcyAzIFB1YmxpYyBQcmltYXJ5IENlcnRpZmljYXRpb24gQXV0\n"
		"aG9yaXR5IC0gRzUwHhcNMDYxMTA4MDAwMDAwWhcNMzYwNzE2MjM1OTU5WjCByjEL\n"
		"MAkGA1UEBhMCVVMxFzAVBgNVBAoTDlZlcmlTaWduLCBJbmMuMR8wHQYDVQQLExZW\n"
		"ZXJpU2lnbiBUcnVzdCBOZXR3b3JrMTowOAYDVQQLEzEoYykgMjAwNiBWZXJpU2ln\n"
		"biwgSW5jLiAtIEZvciBhdXRob3JpemVkIHVzZSBvbmx5MUUwQwYDVQQDEzxWZXJp\n"
		"U2lnbiBDbGFzcyAzIFB1YmxpYyBQcmltYXJ5IENlcnRpZmljYXRpb24gQXV0aG9y\n"
		"aXR5IC0gRzUwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCvJAgIKXo1\n"
		"nmAMqudLO07cfLw8RRy7K+D+KQL5VwijZIUVJ/XxrcgxiV0i6CqqpkKzj/i5Vbex\n"
		"t0uz/o9+B1fs70PbZmIVYc9gDaTY3vjgw2IIPVQT60nKWVSFJuUrjxuf6/WhkcIz\n"
		"SdhDY2pSS9KP6HBRTdGJaXvHcPaz3BJ023tdS1bTlr8Vd6Gw9KIl8q8ckmcY5fQG\n"
		"BO+QueQA5N06tRn/Arr0PO7gi+s3i+z016zy9vA9r911kTMZHRxAy3QkGSGT2RT+\n"
		"rCpSx4/VBEnkjWNHiDxpg8v+R70rfk/Fla4OndTRQ8Bnc+MUCH7lP59zuDMKz10/\n"
		"NIeWiu5T6CUVAgMBAAGjgbIwga8wDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8E\n"
		"BAMCAQYwbQYIKwYBBQUHAQwEYTBfoV2gWzBZMFcwVRYJaW1hZ2UvZ2lmMCEwHzAH\n"
		"BgUrDgMCGgQUj+XTGoasjY5rw8+AatRIGCx7GS4wJRYjaHR0cDovL2xvZ28udmVy\n"
		"aXNpZ24uY29tL3ZzbG9nby5naWYwHQYDVR0OBBYEFH/TZafC3ey78DAJ80M5+gKv\n"
		"MzEzMA0GCSqGSIb3DQEBBQUAA4IBAQCTJEowX2LP2BqYLz3q3JktvXf2pXkiOOzE\n"
		"p6B4Eq1iDkVwZMXnl2YtmAl+X6/WzChl8gGqCBpH3vn5fJJaCGkgDdk+bW48DW7Y\n"
		"5gaRQBi5+MHt39tBquCWIMnNZBU4gcmU7qKEKQsTb47bDN0lAtukixlE0kF6BWlK\n"
		"WE9gyn6CagsCqiUXObXbf+eEZSqVir2G3l6BFoMtEMze/aiCKm0oHw0LxOXnGiYZ\n"
		"4fQRbxC1lfznQgUy286dUV4otp6F01vvpX1FQHKOtw5rDgb7MzVIcbidJ4vEZV8N\n"
		"hnacRHr2lVz2XTIIM6RUthg/aFzyQkqFOFSDX9HoLPKsEdao7WNq\n"
		"-----END CERTIFICATE-----\n"
		"-----BEGIN CERTIFICATE-----\n"
		"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\n"
		"-----END CERTIFICATE-----\n"
	};


XboxOneMabConnect::XboxOneMabConnect(Windows::Xbox::Networking::SecureDeviceAddress^ secureDeviceAddress, XboxOneMabConnectManager^ manager) :
	m_secureDeviceAddress(secureDeviceAddress),
	m_customProperty(nullptr),
	m_onlineclient(nullptr),
	m_assocationFoundInTemplate(false),
	m_isInComingAssociation(false),
	m_isConnectionDestroying(false),
	m_isConnectionInProgress(false),
	m_retryAttempts(0),
	m_timerSinceLastAttempt(0.0f),
	m_connectionStatus(ConnectionStatus::Disconnected),
	m_consoleId(0xFF),
	m_heartTimer(0.0f)
{
	m_ConnectManager = Platform::WeakReference(manager);
	if(secureDeviceAddress == nullptr || manager == nullptr)
	{
		throw ref new Platform::InvalidArgumentException();
	}
}

void XboxOneMabConnect::Initialize()
{
	try
	{
		m_peerDeviceAssociationTemplate = SecureDeviceAssociationTemplate::GetTemplateByName("PeerTraffic");
	}
	catch(Platform::Exception^ e)
	{
		XboxOneMabUtils::LogComment("Failed to get the SecureDeviceAssociationTemplate.\n");
		throw;
	}


//	BIO *bio = NULL;
//	bio =  BIO_new_mem_buf(g_ca, -1);
}

void XboxOneMabConnect::GetPeersAddressesFromGameSessionAndConnect( Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ multiplayerSession)
{
	for(MultiplayerSessionMember^ member : multiplayerSession->Members)
	{
		Windows::Data::Json::JsonObject^ jsonMemberCustomProperties = Windows::Data::Json::JsonObject::Parse(member->MemberCustomConstantsJson);

		// Parse the jsonMemberCustomProperties as desired.
		Platform::String^ xboxUserId = member->XboxUserId;
		Platform::String^ gametag = member->Gamertag;
		Platform::String^ secureDeviceAddressBase64 = member->SecureDeviceAddressBase64;

		SecureDeviceAddress^ peerSecureDeviceAddress =
		SecureDeviceAddress::FromBase64String(secureDeviceAddressBase64);
		XboxOneMabUtils::LogComment("Got peerSecureDeviceAddress for " + xboxUserId);

		// Create and use SecureDeviceAssociations using the acquired SecureDeviceAddress. 
		// This method is detailed in the next section.
		CreateAndUsePeerAssociation(peerSecureDeviceAddress, xboxUserId);
	}


}

void XboxOneMabConnect::CreateAndUsePeerAssociation(SecureDeviceAddress^ peerAddress, Platform::String^ xboxUserId)
{
	IAsyncOperation<SecureDeviceAssociation^>^ asyncOp =
		m_peerDeviceAssociationTemplate->CreateAssociationAsync(
		peerAddress,
		Windows::Xbox::Networking::CreateSecureDeviceAssociationBehavior::Default
		);
	create_task(asyncOp).then([this, xboxUserId]  (SecureDeviceAssociation^ association)
	{
		SOCKADDR_STORAGE remoteSocketAddress;
		Platform::ArrayReference<BYTE> remoteSocketAddressBytes(
			(BYTE*) &remoteSocketAddress,
			sizeof( remoteSocketAddress )
			);
		association->GetRemoteSocketAddressBytes(
			remoteSocketAddressBytes
			);

		//Here we get the remote address for the server
		SOCKADDR_IN6* remote_address = (SOCKADDR_IN6*)&remoteSocketAddress;
		m_onlineclient = new XboxOneMabNetOnlineClient();
		std::string narrow(xboxUserId->Begin(), xboxUserId->End());
		m_onlineclient->member_id = narrow.c_str();

//		XboxOneMabUtils::ConvertPlatformStringToCharArray(xboxUserId,m_onlineclient->member_id);

		m_onlineclient->local_address.saddrV6 = *remote_address;
//		m_onlineclient->secureDeviceAssociation = association;
		//stub

	}).wait();
}

uint8 XboxOneMabConnect::GetConsoleId()
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	return m_consoleId;
}

void XboxOneMabConnect::SetConsoleId(uint8 consoleId)
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	m_consoleId = consoleId;
}

Platform::String^ XboxOneMabConnect::GetConsoleName()
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);

	Platform::String^ consoleName = L"n/a";
	if( !m_consoleName->IsEmpty() )
	{
		consoleName = m_consoleName;
	}

	return consoleName;
}

void XboxOneMabConnect::SetConsoleName(Platform::String^ consoleName)
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	m_consoleName = consoleName;
}

Platform::Object^ XboxOneMabConnect::GetCustomProperty()
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	return m_customProperty;
}

void XboxOneMabConnect::SetCustomProperty(Platform::Object^ object)
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	m_customProperty = object;
}

int XboxOneMabConnect::GetNumberOfRetryAttempts()
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	return m_retryAttempts;
}

void XboxOneMabConnect::SetNumberOfRetryAttempts(int retryAttempt)
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	m_retryAttempts = retryAttempt;
}

ConnectionStatus XboxOneMabConnect::GetConnectionStatus()
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	return m_connectionStatus;
}

void XboxOneMabConnect::SetConnectionStatus(ConnectionStatus status)
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	m_connectionStatus = status;
}

bool XboxOneMabConnect::GetAssocationFoundInTemplate()
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	return m_assocationFoundInTemplate;
}

void XboxOneMabConnect::SetAssocationFoundInTemplate(bool val)
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	m_assocationFoundInTemplate = val;
}

Windows::Xbox::Networking::SecureDeviceAssociation^ XboxOneMabConnect::GetAssociation()
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	return m_association;
}

void XboxOneMabConnect::SetAssociation( Windows::Xbox::Networking::SecureDeviceAssociation^ association )
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);

	if (m_association != nullptr)
	{
		m_association->StateChanged -= m_associationStateChangeToken;
	}

	m_association = association;

	if(association != nullptr)
	{
		// This is needed to know if the association is ever dropped.
		TypedEventHandler<Windows::Xbox::Networking::SecureDeviceAssociation^, Windows::Xbox::Networking::SecureDeviceAssociationStateChangedEventArgs^>^ stateChangeEvent = 
			ref new TypedEventHandler<Windows::Xbox::Networking::SecureDeviceAssociation^, Windows::Xbox::Networking::SecureDeviceAssociationStateChangedEventArgs^>(
			[this] (Windows::Xbox::Networking::SecureDeviceAssociation^ association, Windows::Xbox::Networking::SecureDeviceAssociationStateChangedEventArgs^ args)
		{
			HandleAssociationChangedEvent(association, args);
		});

		m_associationStateChangeToken = association->StateChanged += stateChangeEvent;
	}
}

void XboxOneMabConnect::HandleAssociationChangedEvent(
	Windows::Xbox::Networking::SecureDeviceAssociation^ association, 
	Windows::Xbox::Networking::SecureDeviceAssociationStateChangedEventArgs^ args)
{
	XboxOneMabConnectManager^ meshManager = m_ConnectManager.Resolve<XboxOneMabConnectManager>();
	meshManager->OnAssociationChange(args, association);
}

Windows::Xbox::Networking::SecureDeviceAddress^ XboxOneMabConnect::GetSecureDeviceAddress()
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	return m_secureDeviceAddress;
}

bool XboxOneMabConnect::IsInComingAssociation()
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	return m_isInComingAssociation;
}

void XboxOneMabConnect::SetInComingAssociation(bool val)
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	m_isInComingAssociation = val;
}

bool XboxOneMabConnect::IsConnectionDestroying()
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	return m_isConnectionDestroying;
}

void XboxOneMabConnect::SetConnectionDestroying( bool val )
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	m_isConnectionDestroying = val;
}

bool XboxOneMabConnect::IsConnectionInProgress()
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	return m_isConnectionInProgress;
}

void XboxOneMabConnect::SetConnectionInProgress( bool val )
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	m_isConnectionInProgress = val;
}

void XboxOneMabConnect::SetHeartTimer( float timer )
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	m_heartTimer = timer;
}

float XboxOneMabConnect::GetHeartTimer()
{
	Concurrency::critical_section::scoped_lock lock(m_stateLock);
	return m_heartTimer;
}

}