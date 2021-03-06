#pragma once

#include "FunctionFwd.hpp"
#include <atomic>

struct IO_Handle
{
    int field_0_flags;
    int field_4;
    FILE *field_8_hFile;
    int field_C_last_api_result;
    std::atomic<bool> field_10_bDone; // Note: OG bug - appears to be no thread sync on this
    int field_14;
    int field_18;
};
ALIVE_ASSERT_SIZEOF(IO_Handle, 0x1C);

EXPORT IO_Handle* CC IO_Open_4F2320(const char* fileName, int modeFlag);
EXPORT void CC IO_WaitForComplete_4F2510(IO_Handle* hFile);
EXPORT int CC IO_Seek_4F2490(IO_Handle* hFile, int offset, int origin);
EXPORT void CC IO_fclose_4F24E0(IO_Handle* hFile);
EXPORT DWORD WINAPI FS_IOThread_4F25A0(LPVOID lpThreadParameter);
EXPORT signed int CC IO_Issue_ASync_Read_4F2430(IO_Handle *hFile, int always3, void* readBuffer, size_t bytesToRead, int /*notUsed1*/, int /*notUsed2*/, int /*notUsed3*/);
EXPORT int CC IO_Read_4F23A0(IO_Handle* hFile, void* pBuffer, size_t bytesCount);

ALIVE_VAR_EXTERN(DWORD, sIoThreadId_BBC558);
