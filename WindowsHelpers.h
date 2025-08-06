#pragma once


#include <windows.h>

#include <stdexcept>

#include <cstdio>


#define CHECK_TYPE HMODULE

CHECK_TYPE require(CHECK_TYPE aValue, CHECK_TYPE aInvalid = NULL)
{
    if(aValue == aInvalid)
    {
        DWORD lastError = GetLastError();
        wchar_t * buffer;
        DWORD formatEr = FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            0,
            lastError,
            0,
            // The API writes to the provided address, but still expects a LPWSTR. So we pretend.
            reinterpret_cast<LPWSTR>(&buffer),
            0,
            nullptr);
        if(formatEr == 0)
        {
            wprintf(L"Format message failed with 0x%x\n", GetLastError());
        }
        else
        {
            wprintf(L"Error message: %s\n", buffer);
            LocalFree(buffer);
        }
        throw std::logic_error{"Requirement failed"};
    }
    return aValue;
}

#undef CHECK_TYPE