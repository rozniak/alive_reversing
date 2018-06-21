#pragma once

#include "detours.h"
#include <assert.h>

class ExportHooker
{
public:
    explicit ExportHooker(HINSTANCE instance) : mhInstance(instance)
    {
        mExports.reserve(5000);
    }

    void Apply()
    {
        if (!DetourEnumerateExports(mhInstance, this, EnumExports))
        {
            ALIVE_FATAL("Export enumeration failed");
        }
        if (IsAlive())
        {
            ProcessExports();
        }
    }

private:
    void ProcessExports()
    {
        TRACE_ENTRYEXIT;
        LONG err = DetourTransactionBegin();

        if (err != NO_ERROR)
        {
            ALIVE_FATAL("DetourTransactionBegin failed");
        }

        err = DetourUpdateThread(GetCurrentThread());

        if (err != NO_ERROR)
        {
            ALIVE_FATAL("DetourUpdateThread failed");
        }

        for (const auto& e : mExports)
        {
            LOG_INFO("Hook: "
                << e.mName.c_str()
                << " From "
                << "0x" << std::hex << (e.mIsImplemented ? e.mGameFunctionAddr : (DWORD)e.mCode)
                << " To "
                << "0x" << std::hex << (e.mIsImplemented ? (DWORD)e.mCode : e.mGameFunctionAddr)
                << " Implemented: " << e.mIsImplemented);

            if (e.mIsImplemented)
            {
                err = DetourAttach(&(PVOID&)e.mGameFunctionAddr, e.mCode);
            }
            else
            {
                err = DetourAttach(&(PVOID&)e.mCode, (PVOID)e.mGameFunctionAddr);
            }

            if (err != NO_ERROR)
            {
                ALIVE_FATAL("DetourAttach failed");
            }
        }

        err = DetourTransactionCommit();
        if (err != NO_ERROR)
        {
            ALIVE_FATAL("DetourTransactionCommit failed");
        }
    }

    static bool IsHexDigit(char letter)
    {
        if (letter >= '0' && letter <= '9')
        {
            return true;
        }
        const char lower = ::tolower(letter);
        return (lower >= 'a' && lower <= 'f');
    }

    static bool IsImplemented(PVOID pCode)
    {
        // 4 nops, int 3, 4 nops
        const static BYTE kPatternToFind[] = { 0x90, 0x90, 0x90, 0x90, 0xCC, 0x90, 0x90, 0x90, 0x90 };
        BYTE codeBuffer[256] = {};
        memcpy(codeBuffer, pCode, sizeof(codeBuffer));
        for (int i = 0; i < sizeof(codeBuffer) - sizeof(kPatternToFind); i++)
        {
            if (codeBuffer[i] == kPatternToFind[0])
            {
                if (memcmp(&codeBuffer[i], kPatternToFind, sizeof(kPatternToFind)) == 0)
                {
                    if (!IsAlive())
                    {
                        BYTE* ptr = &reinterpret_cast<BYTE*>(pCode)[i + 4];
                        DWORD old = 0;
                        VirtualProtect(ptr, 1, PAGE_EXECUTE_READWRITE, &old);
                        *ptr = 0x90;
                    }
                    return false;
                }
            }
        }
        return true;
    }

    void OnExport(PCHAR pszName, PVOID pCode)
    {
        std::string name(pszName);
        auto underScorePos = name.find_first_of('_');
        while (underScorePos != std::string::npos)
        {
            int hexNumLen = 0;
            for (size_t i = underScorePos + 1; i < name.length(); i++)
            {
                if (IsHexDigit(name[i]))
                {
                    hexNumLen++;
                }
                else
                {
                    break;
                }
            }

            if (hexNumLen >= 6 && hexNumLen <= 8)
            {
                unsigned long addr = std::stoul(name.substr(underScorePos + 1, hexNumLen), nullptr, 16);

                mExports.push_back({ name, pCode, addr, IsImplemented(pCode) });
                return;
            }

            underScorePos = name.find('_', underScorePos + 1);
        }
        LOG_WARNING(pszName << " was not hooked");
    }

    static BOOL CALLBACK EnumExports(PVOID pContext,
        ULONG /*nOrdinal*/,
        PCHAR pszName,
        PVOID pCode)
    {
        if (pszName && pCode)
        {
            // Resolve 1 level long jumps, not using DetourCodeFromPointer
            // as it appears to have a bug where it checks for 0xeb before 0xe9 and so
            // won't skip jmps that start with long jmps.
            BYTE* pbCode = (BYTE*)pCode;
            if (pbCode[0] == 0xe9)
            {
                // jmp +imm32
                PBYTE pbNew = pbCode + 5 + *(DWORD *)&pbCode[1];
                pCode = pbNew;
            }
            reinterpret_cast<ExportHooker*>(pContext)->OnExport(pszName, pCode);
        }
        return TRUE;
    }

    HINSTANCE mhInstance = nullptr;
    struct Export
    {
        std::string mName;
        LPVOID mCode;
        DWORD mGameFunctionAddr;
        bool mIsImplemented;
    };
    std::vector<Export> mExports;
};