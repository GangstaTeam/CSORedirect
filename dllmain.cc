#include <fstream>
#include <string>
#define _CRT_SECURE_NO_WARNINGS
#define _HAS_EXCEPTIONS 0
#define WIN32_LEAN_AND_MEAN
#include <cstdint>
#include <unordered_set>
#include <Windows.h>
#include <iostream>
#include <unordered_map>
#include <sys/stat.h>

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

using OriginalCompileExec_t = void* (__thiscall*)(void* _this, char* str, char* source, char* arg);
OriginalCompileExec_t OriginalCompileExec;

typedef int (*OriginalScriptLoadCompiled_t)(char*, char*, char*, int, int);
OriginalScriptLoadCompiled_t OriginalScriptLoadCompiled;

typedef void* (__thiscall* CompileExecFunc_t)(void* _this, char* fileName, const char* sourceCode, const char* argList);
CompileExecFunc_t OriginalCompileExec_Hook;

typedef void* (__cdecl* ExecScript_t)(void* _this, int argc, char** argv);
ExecScript_t OriginalExecScript;

typedef void* (__thiscall* RegisterMethod_t)(void*, const char*, const char*, void*, int, int, int);
RegisterMethod_t oRegisterMethod;

bool gDebugConsole = false;
char** gIgnoreFiles = nullptr;
int gFileCount = 0;

void LoadConfig()
{
    std::ifstream iniFile("ScriptRedirect.ini");
    if (!iniFile.is_open()) return;
    std::string line;
    while (std::getline(iniFile, line)) {
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = line.substr(0, eq);
            std::string value = line.substr(eq + 1);
            if (key == "DebugConsole" && value == "1") {
                gDebugConsole = true;
            }
        }
    }
    iniFile.close();
}

void log(const char* format, ...) {
    va_list args;
    
    // Write to console
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    std::cout << std::endl;
    
    // Write to log file
    FILE* logFile = fopen("ScriptRedirect.log", "a");
    if (logFile) {
        va_start(args, format);
        vfprintf(logFile, format, args);
        va_end(args);
        fprintf(logFile, "\n");
        fclose(logFile);
    }
}

char __cdecl ScriptLoadCompiled(char* scriptPath, char *a2, char *Str2, int a4, int a5)
{
    if (!gIgnoreFiles || gFileCount == 0) {
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

    return reinterpret_cast<char(__cdecl*)(char*, char*, char*, int, int)>(OriginalScriptLoadCompiled)(scriptPath, a2, Str2, a4, a5);
}

bool __cdecl Echo(void*, int _arg_count, char** arg_text)
{
    int arg_count = _arg_count - 1;

    log("[Echo (%d)] %s: %s", arg_count - 1, arg_text[1], arg_text[2]);

    return false;
}

void* __fastcall HookedRegisterMethod(void* _this, void*, const char* a2, const char* a3, void* a4, int a5, int a6, int a7)
{    
    if (strcmp(a3, "Echo") == 0) {
        log("  -> Redirecting Echo to custom implementation");
        a4 = (void*)Echo;
    }

    return oRegisterMethod(_this, a2, a3, a4, a5, a6, a7);
}

bool LoadList()
{
    FILE* pFileList = fopen("plugins\\ScriptRedirect.list", "r");
    if (!pFileList) {
        log("  plugins\\ScriptRedirect.list file not found.");
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

void* __cdecl HookedExecScript(void* _this, int argc, char** argv) {
    const char* scriptPath = (argc > 1 && argv) ? argv[1] : nullptr;
    
    if (scriptPath) {
        printf("[ExecScript] Loading: %s\n", scriptPath);
        
        // Check if in redirect list
        if (gIgnoreFiles && gFileCount > 0) {
            for (int i = 0; i < gFileCount; i++) {
                if (strcmp(gIgnoreFiles[i], scriptPath) == 0) {
                    printf("[ExecScript] MATCHED: %s - loading custom .ds\n", scriptPath);
                    
                    // Transform script/xyz.ds -> scriptc/xyz.ds
                    char customPath[512];
                    if (strncmp(scriptPath, "script/", 7) == 0) {
                        snprintf(customPath, sizeof(customPath), "scriptc/%s", scriptPath + 7);
                    } else {
                        snprintf(customPath, sizeof(customPath), "%s", scriptPath);
                    }
                    
                    // Load .ds file from filesystem
                    HANDLE hFile = CreateFileA(customPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (hFile != INVALID_HANDLE_VALUE) {
                        DWORD fileSize = GetFileSize(hFile, NULL);
                        if (fileSize > 0) {
                            char* fileContent = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, fileSize + 1);
                            if (fileContent) {
                                DWORD bytesRead;
                                if (ReadFile(hFile, fileContent, fileSize, &bytesRead, NULL)) {
                                    fileContent[bytesRead] = '\0';
                                    
                                    printf("[ExecScript] Compiling and executing custom .ds: %s\n", customPath);
                                    
                                    static torque3d::CodeBlock* p_CBInstance = nullptr;
                                    if (!p_CBInstance) {
                                        p_CBInstance = new torque3d::CodeBlock();
                                    }
                                    
                                    // Compile and execute
                                    OriginalCompileExec(p_CBInstance, NULL, fileContent, NULL);
                                    
                                    HeapFree(GetProcessHeap(), 0, fileContent);
                                    CloseHandle(hFile);
                                    
                                    // Return dummy pointer - DON'T call original
                                    return (void*)1;
                                }
                                HeapFree(GetProcessHeap(), 0, fileContent);
                            }
                        }
                        CloseHandle(hFile);
                    } else {
                        printf("[ExecScript] Custom .ds not found: %s\n", customPath);
                    }
                }
            }
        }
    }
    
    // Call original if not intercepted
    return OriginalExecScript(_this, argc, argv);
}

int __stdcall DllMain(HMODULE p_hModule, DWORD p_dwReason, void* p_pReserved)
{
    if (p_dwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(p_hModule);
        LoadConfig();
        if (gDebugConsole) {
            AllocConsole();
            AttachConsole(GetCurrentProcessId());
            freopen("CON", "w", stdout);
            freopen("CON", "w", stderr);
        }

        if (*reinterpret_cast<uint32_t*>(0x400128) != 0x455CAD51) 
        {
            MessageBoxA(0, "You're using wrong game version. v1.00.2 is required!", "ScriptRedirect", MB_OK | MB_ICONERROR);
            return 0;
        }

        if (!LoadList()) 
        {
            return 0;
        }

        // Initialize everything...
        MH_Initialize();

        OriginalCompileExec = (OriginalCompileExec_t)0x00490390;

        // Create hooks
        MH_CreateHook(reinterpret_cast<LPVOID>(0x00491760), &HookedRegisterMethod, reinterpret_cast<void**>(&oRegisterMethod));
        MH_CreateHook(reinterpret_cast<LPVOID>(0x0047FFE0), (LPVOID)ScriptLoadCompiled, reinterpret_cast<void**>(&OriginalScriptLoadCompiled));
        MH_CreateHook(reinterpret_cast<LPVOID>(0x00489EE0), &HookedExecScript, reinterpret_cast<void**>(&OriginalExecScript));
    }

    if(p_dwReason == DLL_PROCESS_DETACH)
    {
        // Safely free memory for gIgnoreFiles
        FreeList(); 
        if (gDebugConsole) {
            fclose(stdout);
            fclose(stderr);
            FreeConsole();
        }
        MH_RemoveHook(reinterpret_cast<LPVOID>(0x00491760));  // RegisterMethod
        MH_RemoveHook(reinterpret_cast<LPVOID>(0x0047FFE0));  // ScriptLoadCompiled
        MH_RemoveHook(reinterpret_cast<LPVOID>(0x00489EE0));  // CScript::Method::ExecScript
    }

    return 1;
}

