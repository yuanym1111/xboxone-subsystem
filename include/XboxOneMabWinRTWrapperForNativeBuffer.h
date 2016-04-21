#ifndef XBOXONE_MAB_WINRT_WRAPPER_FOR_NATIVE_BUFFER_H
#define XBOXONE_MAB_WINRT_WRAPPER_FOR_NATIVE_BUFFER_H

#pragma once
#include <wrl.h>
#include <robuffer.h>
#include <Windows.Storage.Streams.h>

namespace RUGBY_CHALLENGE_XBOXONE_MAB
{

    class WinRTWrapperForNativeBuffer : public Microsoft::WRL::RuntimeClass< Microsoft::WRL::RuntimeClassFlags< Microsoft::WRL::WinRtClassicComMix >, 
                                                                             ABI::Windows::Storage::Streams::IBuffer, 
                                                                             Windows::Storage::Streams::IBufferByteAccess, 
                                                                             Microsoft::WRL::FtmBase >
    {
    public:
        WinRTWrapperForNativeBuffer( BYTE* data, UINT32 size )
            : m_data( data )
            , m_size( size )
        {
            //OutputDebugString( L" WinRTWrapperForNativeBuffer cstr\n" );
        }

        virtual ~WinRTWrapperForNativeBuffer()
        {
            //OutputDebugString( L" Destroyed WinRTWrapperForNativeBuffer dstr\n" );
        }

        // IBuffer property getter methods
        IFACEMETHODIMP get_Capacity( UINT32* capacity ) override
        {
            *capacity = m_size;
            return S_OK;
        }

        IFACEMETHODIMP get_Length( UINT32* length ) override
        {
            *length = m_size;
            return S_OK;
        }

        IFACEMETHODIMP put_Length( UINT32 length ) override
        {
            // Since this impl is just a wrapper for a native buffer already allocated, we 
            // do not allow resizing of the WinRT IBuffer
            if ( length != m_size )
            {
                return E_INVALIDARG;
            }

            return S_OK;
        }

        // IBufferByteAccess
        IFACEMETHODIMP Buffer( BYTE** buffer ) override
        {
            if( buffer )
            {
                *buffer = (BYTE *)m_data;
                return S_OK;
            }
            else
            {
                return E_FAIL;
            }
        }

        // Helper method for creating a wrapper buffer from a native buffer 
        // This helper will hide the WRL syntax needed for instantiating this class 
        // from caller code
        static Windows::Storage::Streams::IBuffer^ Create( BYTE* data, UINT32 size )
        {
            return reinterpret_cast< Windows::Storage::Streams::IBuffer^ >( Microsoft::WRL::Make<RUGBY_CHALLENGE_XBOXONE_MAB::WinRTWrapperForNativeBuffer>( data, size ).Get() );
        }

    private:

        BYTE*   m_data;
        UINT32  m_size;
    };

}

#endif