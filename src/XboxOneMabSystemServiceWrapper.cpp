/**
 * @file XboxOneMabSystemServiceWrapper.cpp
 * @Xbox One system service class.
 * @author jaydens
 * @07/04/2015
 */
#include "pch.h"
#include "XboxOneMabSystemServiceWrapper.h"
#include "XboxOneMabUtils.h"

using namespace Platform;
using namespace Windows::Data::Json;
using namespace Windows::Xbox::Services;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace Windows::Xbox::Networking;
using namespace Windows::Xbox::ApplicationModel;
using namespace Windows::Xbox::Multiplayer;
using namespace Windows::Xbox::System;
using namespace Windows::Foundation::Collections;
using namespace Windows::Foundation;
using namespace Windows::Xbox::UI;

static bool pending_flag = false;
int XboxOneMabSystemServiceWrapper::XboxOneCallVirtualKeyboard(const MabString& default_text,const MabString& keyboard_title,const MabString& Keyboard_description,wchar_t* buffer_text, wchar_t* buffer_error, DWORD* err_code, bool *results_pending_ptr, int max_inpu_length)
{
	*results_pending_ptr = false;

	KeyboardOptions^ options = ref new(KeyboardOptions);
	
	//options->DefaultText = XboxOneMabUtils::ConvertCharStringToPlatformString(default_text.c_str());
	//This is a hack for fixing ticket #11790. Strings sent to the system keyboard are converted to UTF16 
	// so it displays special characters properly. -Kade WW
	size_t len = default_text.length() + 1;
	wchar_t* encoded_text = new wchar_t[len];
	MabStringHelper::ConvertUTF8ToUTF16(default_text, encoded_text, len);
	options->DefaultText = ref new Platform::String(encoded_text);
	delete[] encoded_text;


	options->DescriptionText = XboxOneMabUtils::ConvertCharStringToPlatformString(Keyboard_description.c_str());
	options->TitleText = XboxOneMabUtils::ConvertCharStringToPlatformString(keyboard_title.c_str());
	options->InputScope = Windows::Xbox::UI::VirtualKeyboardInputScope::Default;
	options->MaxLength = max_inpu_length;
	options->LengthIndicatorThreshold = int(max_inpu_length * 4 / 5 );
	options->LengthIndicatorVisibility = true;
	pending_flag = true;

	auto operation = SystemUI::ShowVirtualKeyboardWithOptionsAsync(options);
	// Start the async show keyboard operation to retrieve an email address from the user
#if 0
	auto operation = SystemUI::ShowVirtualKeyboardAsync(XboxOneMabUtils::ConvertCharStringToPlatformString(default_text.c_str()), 
														XboxOneMabUtils::ConvertCharStringToPlatformString(keyboard_title.c_str()), 
														XboxOneMabUtils::ConvertCharStringToPlatformString(Keyboard_description.c_str()), 
														Windows::Xbox::UI::VirtualKeyboardInputScope::Default);
#endif
	// Hook up our completion handler that will be called when the ShowAsync call is finished


	operation->Completed = ref new AsyncOperationCompletedHandler<Platform::String^>(
                [err_code, buffer_text, buffer_error, results_pending_ptr, options] (IAsyncOperation<Platform::String^>^ result, Windows::Foundation::AsyncStatus status)
    {

		switch (status)
		{
		case Windows::Foundation::AsyncStatus::Completed: 
		{
			Platform::String^ text =result->GetResults();
			*err_code = S_OK;
			buffer_error[0] = L'\0';
			wmemcpy(buffer_text, text->Data(), text->Length());
			bool isvalide = XboxOneMabUtils::VerifyStringResult(text);
			break;
		}
		case Windows::Foundation::AsyncStatus::Canceled:
		{
			*err_code = result->ErrorCode.Value;
			Platform::String^ text = options->DefaultText;
			wmemcpy(buffer_text, text->Data(), text->Length());
			buffer_error[0] = L'\0';
			break;
		}
		case Windows::Foundation::AsyncStatus::Error:
		{
			HRESULT error_code = result->ErrorCode.Value;
			*err_code = result->ErrorCode.Value;
			Platform::String^ error_msg_wchar = XboxOneMabUtils::ConvertHResultToErrorName(error_code);
			buffer_text[0] = L'\0';
			memcpy(buffer_error, error_msg_wchar->Data(), error_msg_wchar->Length());
			break;
		}
		default:
			_ASSERT(false);
		}

		*results_pending_ptr = true;
		pending_flag = false;
	}
	);

	while(pending_flag) {  SwitchToThread(); };
	return 0;
}
