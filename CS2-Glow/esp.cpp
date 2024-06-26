#include <Windows.h>
#include <stdio.h>
#include <TlHelp32.h>
#include <iostream>
#include <thread>
#include <wininet.h>
#include <string>
#include <sstream>
#include <regex>
#include <vector>
#include <map>
#include <iomanip>

namespace offsets
{
    ptrdiff_t p_entity_list = 0; 
    ptrdiff_t m_h_player_pawn = 0;
    ptrdiff_t m_fl_detected_by_enemy_sensor_time = 0;
}

struct JsonData {
    std::string key;
    long long value;
};

#pragma comment(lib, "wininet.lib")

const char* COLOR_RED = "\033[31m";
const char* COLOR_GREEN = "\033[32m";
const char* COLOR_YELLOW = "\033[33m";
const char* COLOR_BLUE = "\033[34m";
const char* COLOR_RESET = "\033[0m";

std::string findValueByKey(const std::string& json, const std::string& key) {
    std::regex keyRegex("\"" + key + "\":\\s*(?:\\{[^{}]*\\}|\\d+)");
    std::smatch match;

    if (std::regex_search(json, match, keyRegex)) {
        std::string value = match[0].str();
        if (value.find('{') != std::string::npos) {
            std::regex valueRegex(":\\s*(\\d+)");
            std::smatch valueMatch;
            if (std::regex_search(value, valueMatch, valueRegex)) {
                return valueMatch[1].str();
            }
        }
        else {
            size_t pos = value.find(':');
            return value.substr(pos + 1);
        }
    }

    return "";
}

std::string fetchValueFromJSON(const std::wstring& url, const std::string& key) {
    HINTERNET hInternet = InternetOpen(L"Fetch JSON Example", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet == NULL) {
        std::cerr << "Failed to initialize WinINet." << std::endl;
        return "";
    }

    HINTERNET hConnect = InternetOpenUrl(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (hConnect == NULL) {
        std::cerr << "Failed to open URL." << std::endl;
        InternetCloseHandle(hInternet);
        return "";
    }

    char buffer[1024];
    std::stringstream jsonStream;

    DWORD bytesRead;
    while (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        jsonStream.write(buffer, bytesRead);
    }

    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    std::string jsonData = jsonStream.str();
    size_t found = jsonData.find("{");
    if (found != std::string::npos) {
        jsonData = jsonData.substr(found);

        // Replace line breaks and tabs to make it a single line
        std::regex r("[\n\t]");
        jsonData = std::regex_replace(jsonData, r, "");

        // Find the value for the specified key
        return findValueByKey(jsonData, key);
    }
    return "";
}

void fetchOffsets() {
    const std::wstring urlOffsets = L"https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/offsets.json";
    const std::wstring urlClientDLL = L"https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/client.dll.json";

    std::vector<std::string> keysToFindOffsets = {
        "dwEntityList"
    };
    std::vector<std::string> keysToFindClientDLL = {
        "m_flDetectedByEnemySensorTime",
        "m_hPlayerPawn"
    };

    for (const auto& key : keysToFindOffsets) {
        std::string value = fetchValueFromJSON(urlOffsets, key);
        if (!value.empty()) {
            ptrdiff_t hexValue = std::stoll(value, 0, 0);
            std::cout << COLOR_GREEN << key << " (Offsets): " << COLOR_YELLOW << "0x" << std::uppercase << std::hex << hexValue << std::nouppercase << std::dec << std::endl;
            if (key == "dwEntityList") {
                offsets::p_entity_list = hexValue;
            }
        }
        else {
            std::cerr << COLOR_RED << "Key '" << key << "' (Offsets) not found in the JSON data." << std::endl;
        }
    }
    for (const auto& key : keysToFindClientDLL) {
        std::string value = fetchValueFromJSON(urlClientDLL, key);
        if (!value.empty()) {
            long long hexValue = std::stoll(value, 0, 0);
            std::cout << COLOR_GREEN << key << " (ClientDLL): " << COLOR_YELLOW << "0x" << std::uppercase << std::hex << hexValue << std::nouppercase << std::dec << std::endl;
            // Handle the specific key-value assignments here
            if (key == "m_flDetectedByEnemySensorTime") {
                offsets::m_fl_detected_by_enemy_sensor_time = hexValue;
            }
            else if (key == "m_hPlayerPawn") {
                offsets::m_h_player_pawn = hexValue;
            }
        }
        else {
            std::cerr << COLOR_RED << "Key '" << key << "' (ClientDLL) not found in the JSON data." << std::endl;
        }
    }
}

uint32_t get_process_id_by_name(const char* process_name)
{
    HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, NULL);
    if (!hSnapShot)
    {
		return 0;
	}
    PROCESSENTRY32 pEntry = { 0 };
    pEntry.dwSize = sizeof(pEntry);
    BOOL hRes = Process32First(hSnapShot, &pEntry);
    while (hRes)
    {
        wchar_t wideProcessName[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, process_name, -1, wideProcessName, MAX_PATH);

        if (wcscmp(pEntry.szExeFile, wideProcessName) == 0)
        {
            CloseHandle(hSnapShot);
            return static_cast<uint32_t>(pEntry.th32ProcessID);
        }
        hRes = Process32Next(hSnapShot, &pEntry);
    }
    CloseHandle(hSnapShot);

    return 0;
}

uintptr_t get_module_base(uint32_t process_id, const char* module_name)
{
    int bufferSize = MultiByteToWideChar(CP_ACP, 0, module_name, -1, NULL, 0);
    wchar_t wideModuleName[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, module_name, -1, wideModuleName, bufferSize);

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, process_id);
    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    MODULEENTRY32W ModuleEntry32 = { 0 };
    ModuleEntry32.dwSize = sizeof(MODULEENTRY32W);

    if (Module32FirstW(hSnapshot, &ModuleEntry32))
    {
        do
        {
            if (wcscmp(ModuleEntry32.szModule, wideModuleName) == 0)
            {
                CloseHandle(hSnapshot);
                return reinterpret_cast<uintptr_t>(ModuleEntry32.modBaseAddr);
            }
        } while (Module32NextW(hSnapshot, &ModuleEntry32));
    }

    CloseHandle(hSnapshot);
    return 0;
}

template<typename T>
T memory_read(HANDLE cs2_handle, uintptr_t address)
{
    T val = T();
    ReadProcessMemory(cs2_handle, (LPCVOID)address, &val, sizeof(T), NULL);
    return val;
}

template<typename T>
void memory_write(HANDLE cs2_handle, uintptr_t address, T value)
{
    WriteProcessMemory(cs2_handle, (LPVOID)address, &value, sizeof(T), NULL);
}

int main(int argc, char* argv[])
{
    SetConsoleTitleA("CS2-Glow Made by: github/9xN");

    const char* cs2_process_name = "cs2.exe";
    const char* client_dll_name = "client.dll";
    uint32_t cs2_process_id = get_process_id_by_name(cs2_process_name);

    if (cs2_process_id == 0)
    {
        printf("%s%s not found!%s\n", COLOR_RED, cs2_process_name, COLOR_RESET);
        return 1;
    }
    printf("%s%s has pid: %s%lu%s\n", COLOR_GREEN, cs2_process_name, COLOR_YELLOW, (unsigned long)cs2_process_id, COLOR_RESET);

    HANDLE cs2_process_handle = OpenProcess(PROCESS_ALL_ACCESS, 0, cs2_process_id);

    if (cs2_process_handle == NULL)
    {
        printf("%sFailed to open %s process!%s\n", COLOR_RED, cs2_process_name, COLOR_RESET);
        return 1;
    }
    printf("%s%s process handle: %s0x%p%s\n", COLOR_GREEN, cs2_process_name, COLOR_YELLOW, (void*)cs2_process_handle, COLOR_RESET);

    uintptr_t cs2_module_client = get_module_base(cs2_process_id, client_dll_name);

    if (cs2_module_client == 0)
    {
        printf("%s%s not found in %s!%s\n", COLOR_RED, client_dll_name, cs2_process_name, COLOR_RESET);
        CloseHandle(cs2_process_handle);
        return 1;
    }
    printf("%s%s base address: %s0x%lx%s\n", COLOR_GREEN, client_dll_name, COLOR_YELLOW, (unsigned long)cs2_module_client, COLOR_RESET);
    printf("%sFetching offsets...%s\n", COLOR_GREEN, COLOR_RESET);
    fetchOffsets();
    printf("%sCS2-Glow Made by: github/9xN - Press F1 to enable/disable%s\n", COLOR_BLUE, COLOR_RESET);
    while (true)
    {
        static bool glow_enabled = false;
        if (GetAsyncKeyState(VK_F1))
        {
            glow_enabled = !glow_enabled;
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            printf("status enabled: %s%s%s\n", glow_enabled ? COLOR_GREEN : COLOR_RED, glow_enabled ? "true" : "false", COLOR_RESET);
        }
        for (int i = 1; i < 64; i++)
        {
            uintptr_t entity_list = memory_read<uintptr_t>(cs2_process_handle, cs2_module_client + offsets::p_entity_list);
            if (entity_list == 0)
                continue;
            uintptr_t list_entry = memory_read<uintptr_t>(cs2_process_handle, entity_list + (8 * (i & 0x7FFF) >> 9) + 16);
            if (list_entry == 0)
                continue;
            uintptr_t player = memory_read<uintptr_t>(cs2_process_handle, list_entry + 120 * (i & 0x1FF));
            if (player == 0)
                continue;
            uint32_t player_pawn = memory_read<uint32_t>(cs2_process_handle, player + offsets::m_h_player_pawn);
            uintptr_t list_entry2 = memory_read<uintptr_t>(cs2_process_handle, entity_list + 0x8 * ((player_pawn & 0x7FFF) >> 9) + 16);
            if (list_entry2 == 0)
                continue;
            uintptr_t p_cs_player_pawn = memory_read<uintptr_t>(cs2_process_handle, list_entry2 + 120 * (player_pawn & 0x1FF));
            if (p_cs_player_pawn == 0)
                continue;
            if (!glow_enabled)
                memory_write<float>(cs2_process_handle, p_cs_player_pawn + offsets::m_fl_detected_by_enemy_sensor_time, 0.f); // off
            else
                memory_write<float>(cs2_process_handle, p_cs_player_pawn + offsets::m_fl_detected_by_enemy_sensor_time, 100000.f); // on
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    CloseHandle(cs2_process_handle);
    std::cin.get();
    return 0;
}
