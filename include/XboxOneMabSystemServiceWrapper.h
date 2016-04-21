/**
 * @file XboxOneMabSystemServiceWrapper.h
 * @Xbox One system service class.
 * @author jaydens
 * @07/04/2015
 */

#pragma once
#include <string>
#include "MabString.h"
//#include "xboxone\SIFXboxOneSystemKeyboard.h"

class XboxOneMabSystemServiceWrapper {
public:
	//static int XboxOneCallVirtualKeyboard(const MabString&,const MabString& ,const MabString& ,SIFXboxOneSystemKeyboard* );
	static int XboxOneCallVirtualKeyboard(const MabString&, const MabString&, const MabString&, wchar_t*, wchar_t*, DWORD*, bool*, int);
};