#define _CRT_SECURE_NO_WARNINGS
#define _HAS_EXCEPTIONS 0
#define WIN32_LEAN_AND_MEAN
#include <cstdint>
#include <unordered_set>
#include <Windows.h>
#include <iostream>

#include "dllmain.h"

using namespace torque3d;

struct Hook {
    void* originalFunction;
    void* hookFunction;
    uint8_t originalBytes[5];
    void* trampoline;
};

Hook hook_OriginalCompileExec;
Hook hook_OriginalAssignRegisteredMethodsToNamespaces;

char** gIgnoreFiles = nullptr;
int gFileCount = 0;

void log(const char* format, ...) {
    va_list args;
    
    // Write to console
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    std::cout << std::endl;
    
    // Write to log file
    FILE* logFile = fopen("CSORedirect.log", "a");
    if (logFile) {
        va_start(args, format);
        vfprintf(logFile, format, args);
        va_end(args);
        fprintf(logFile, "\n");
        fclose(logFile);
    }
}

template <typename H, typename O = void*>
__forceinline void MH_CreateHook(uintptr_t p_Address, H p_Hook, O* p_Original = nullptr)
{
    MH_CreateHook(reinterpret_cast<void*>(p_Address), reinterpret_cast<void*>(p_Hook), reinterpret_cast<void**>(p_Original));
}

__forceinline void MH_RemoveHook(uintptr_t p_Address)
{
    MH_RemoveHook(reinterpret_cast<void*>(p_Address));
}

using OriginalCompileExec_t = void* (__thiscall*)(void* _this, char* str, char* source, char* arg);
OriginalCompileExec_t OriginalCompileExec;

typedef int (*OriginalScriptLoadCompiled_t)(char*, char*, char*, int, int);
OriginalScriptLoadCompiled_t OriginalScriptLoadCompiled;

char __cdecl ScriptLoadCompiled(char* scriptPath, char *a2, char *Str2, int a4, int a5)
{
    log("Hook::ScriptLoadCompiled - Path: %s", scriptPath);

    if (!gIgnoreFiles || gFileCount == 0) {
        log("  -> No redirect list loaded, calling original function");
        return reinterpret_cast<char(__cdecl*)(char*, char*, char*, int, int)>(OriginalScriptLoadCompiled)(scriptPath, a2, Str2, a4, a5);
    }

    for (int i = 0; i < gFileCount; i++) {
        if (strcmp(gIgnoreFiles[i], scriptPath) == 0) {

            log("Detected .cs script: %s\n", scriptPath);
            log("Redirecting %s to alternative script.\n", scriptPath);

            // Check if alternative script exists
            HANDLE hFile = CreateFileA(scriptPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE)
            {
                DWORD fileSize = GetFileSize(hFile, NULL);
                if (fileSize > 0)
                {
                    char* fileContent = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, fileSize + 1);
                    if (fileContent)
                    {
                        DWORD bytesRead;
                        if (ReadFile(hFile, fileContent, fileSize, &bytesRead, NULL) && bytesRead > 0)
                        {
                            fileContent[bytesRead] = '\0';  // Null-terminate the content

                            static torque3d::CodeBlock* p_CBInstance = nullptr;
                            if (p_CBInstance == nullptr)
                            {
                                p_CBInstance = new torque3d::CodeBlock();
                            }

                            log("Executing alternative script: %s\n", scriptPath);
                            OriginalCompileExec(p_CBInstance, NULL, fileContent, NULL);
                        }
                        HeapFree(GetProcessHeap(), 0, fileContent);
                    }
                }
                CloseHandle(hFile);
            }
            return false;
        }
    }

    log("  -> Script not in redirect list, calling original function");
    return reinterpret_cast<char(__cdecl*)(char*, char*, char*, int, int)>(OriginalScriptLoadCompiled)(scriptPath, a2, Str2, a4, a5);
}

bool __cdecl Echo(void*, int _arg_count, char** arg_text)
{
    int arg_count = _arg_count - 1;

    log("[Echo (%d)] %s: %s", arg_count - 1, arg_text[1], arg_text[2]);

    return false;
}

typedef void* (__thiscall* RegisterMethod_t)(void*, const char*, const char*, void*, int, int, int);
RegisterMethod_t oRegisterMethod;

void* __fastcall HookedRegisterMethod(void* _this, void*, const char* a2, const char* a3, void* a4, int a5, int a6, int a7)
{
    log("Hook::RegisterMethod - Namespace: %s, Method: %s", a2 ? a2 : "NULL", a3 ? a3 : "NULL");
    
    if (strcmp(a3, "Echo") == 0) {
        log("  -> Redirecting Echo to custom implementation");
        a4 = (void*)Echo;
    }

    return oRegisterMethod(_this, a2, a3, a4, a5, a6, a7);
}

bool LoadList()
{
    FILE* pFileList = fopen("plugins\\CSORedirect.list", "r");
    if (!pFileList) {
        log("  plugins\\CSORedirect.list file not found.");
        return false;
    }

    gIgnoreFiles = nullptr; // Start with an empty array
    gFileCount = 0;

    char filePath[260];  // Max single path length

    while (fgets(filePath, sizeof(filePath), pFileList) != NULL)
    {
        filePath[strcspn(filePath, "\r\n")] = '\0'; // Remove newline

        // Resize the array to hold one more element
        char** temp = (char**)realloc(gIgnoreFiles, (gFileCount + 1) * sizeof(char*));
        if (!temp) {
            fclose(pFileList);
            return false;
        }
        gIgnoreFiles = temp;

        // Allocate memory for the new path and copy it
        gIgnoreFiles[gFileCount] = (char*)malloc(strlen(filePath) + 1);
        if (!gIgnoreFiles[gFileCount]) {
            fclose(pFileList);
            return false;
        }
        strcpy(gIgnoreFiles[gFileCount], filePath);
        log("  Loaded redirect file: %s", gIgnoreFiles[gFileCount]);

        gFileCount++;
    }

    fclose(pFileList);
    return true;
}

void FreeList()
{
    for (int i = 0; i < gFileCount; i++) {
        free(gIgnoreFiles[i]); // Free each string
    }
    free(gIgnoreFiles); // Free the array itself
    gIgnoreFiles = nullptr;
    gFileCount = 0;
}


DWORD WINAPI TestThread(LPVOID param) {
    AllocConsole();
    AttachConsole(GetCurrentProcessId());
    freopen("CON", "w", stdout);
    freopen("CON", "w", stderr);

    while (true) {
        if (GetAsyncKeyState(VK_F1) & 0x8000) {

            // Check if alternative script exists
            HANDLE hFile = CreateFileA("./test.cs", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE)
            {
                DWORD fileSize = GetFileSize(hFile, NULL);
                if (fileSize > 0)
                {
                    char* fileContent = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, fileSize + 1);
                    if (fileContent)
                    {
                        DWORD bytesRead;
                        if (ReadFile(hFile, fileContent, fileSize, &bytesRead, NULL) && bytesRead > 0)
                        {
                            fileContent[bytesRead] = '\0';  // Null-terminate the content

                            static torque3d::CodeBlock* p_CBInstance = nullptr;
                            if (p_CBInstance == nullptr)
                            {
                                p_CBInstance = new torque3d::CodeBlock();
                            }

                            OriginalCompileExec(p_CBInstance, NULL, fileContent, NULL);
                        }
                        HeapFree(GetProcessHeap(), 0, fileContent);
                    }
                }
                CloseHandle(hFile);
            }
        }
        Sleep(50); // Prevents high CPU usage
    }
    return 0;
}

int __stdcall DllMain(HMODULE p_hModule, DWORD p_dwReason, void* p_pReserved)
{
    if (p_dwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(p_hModule);
        AllocConsole();
        AttachConsole(GetCurrentProcessId());
        freopen("CON", "w", stdout);
        freopen("CON", "w", stderr);

        if (*reinterpret_cast<uint32_t*>(0x400128) != 0x455CAD51) 
        {
            MessageBoxA(0, "You're using wrong game version. v1.00.2 is required!", "CSORedirect", MB_OK | MB_ICONERROR);
            return 0;
        }

        if (!LoadList()) 
        {
            return 0;
        }

        // Initialize everything...
        MH_Initialize();

        OriginalCompileExec = (OriginalCompileExec_t)0x00490390;

        MH_CreateHook(0x00491760, &HookedRegisterMethod, &oRegisterMethod);
        MH_CreateHook(0x0047FFE0, ScriptLoadCompiled, &OriginalScriptLoadCompiled);

        HANDLE hThread = CreateThread(NULL, 0, TestThread, NULL, 0, NULL);
        if (hThread) {
            CloseHandle(hThread);
        }    
    }

    if(p_dwReason == DLL_PROCESS_DETACH)
    {
        fclose(stdout);
        fclose(stderr);
        FreeConsole();
        MH_RemoveHook(0x004926D0);
        MH_RemoveHook(0x0047FFE0);
    }

    return 1;
}