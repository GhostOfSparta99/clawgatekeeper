#define NOMINMAX
#include <windows.h>
#include <aclapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <curl/curl.h>
#include <json/json.h>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <set>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

namespace fs = std::filesystem;

// ===== CONFIGURATION =====
const std::string SUPABASE_URL = "https://hyqmdnzkxhkvzynzqwug.supabase.co";
const std::string SUPABASE_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Imh5cW1kbnpreGhrdnp5bnpxd3VnIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzAyODM0MjksImV4cCI6MjA4NTg1OTQyOX0.Oo-jJIivU0YHRbloTJlpl_cwQ9CkrMq6tV9YQUu8F5w";
const std::string LOG_FILE = "C:\\GateKeeper\\debug.log";

// Global cache to track existing files across ALL scopes
std::map<std::string, std::string> g_fileCache; // Path -> LastModified
std::mutex g_cacheMutex; // Protects g_fileCache

// Global cache for permissions to avoid redundant system calls
std::map<std::string, bool> g_lockCache; // Path -> accessible
std::mutex g_lockMutex; // Protects g_lockCache

std::atomic<bool> g_running{ true };

void LogMessage(const std::string& message) {
    try {
        if (!fs::exists("C:\\GateKeeper")) {
            fs::create_directories("C:\\GateKeeper");
        }
        std::ofstream logFile(LOG_FILE, std::ios::app);
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        // std::cout << "[" << std::put_time(&tm, "%H:%M:%S") << "] " << message << std::endl;
        if (logFile.is_open()) {
            logFile << "[" << std::put_time(&tm, "%H:%M:%S") << "] " << message << std::endl;
        }
    } catch (...) {}
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    try {
        s->append((char*)contents, size * nmemb);
        return size * nmemb;
    } catch (...) {
        return 0;
    }
}

std::string GetISOTimestamp(const fs::file_time_type& ftime) {
    try {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        auto time = std::chrono::system_clock::to_time_t(sctp);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    } catch(...) {
        return "2026-01-01T00:00:00Z";
    }
}

bool SetFileAccessibility(const std::string& filePath, bool makeAccessible) {
    try {
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, NULL, 0);
        std::wstring wPath(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, &wPath[0], size_needed);

        PACL pOldDACL = NULL, pNewDACL = NULL;
        PSECURITY_DESCRIPTOR pSD = NULL;
        EXPLICIT_ACCESS ea;

        DWORD res = GetNamedSecurityInfoW(wPath.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &pOldDACL, NULL, &pSD);
        if (res != ERROR_SUCCESS) return false;

        SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;
        PSID pEveryoneSID = NULL;
        if (!AllocateAndInitializeSid(&SIDAuthWorld, 1, SECURITY_WORLD_RID, 0,0,0,0,0,0,0, &pEveryoneSID)) {
            if (pSD) LocalFree(pSD);
            return false;
        }

        ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));
        ea.grfAccessPermissions = GENERIC_ALL;
        ea.grfAccessMode = makeAccessible ? SET_ACCESS : DENY_ACCESS; // SET_ACCESS removes existing DENY_ACCESS automatically if overwriting
        ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea.Trustee.ptstrName = (LPTSTR)pEveryoneSID;

        res = SetEntriesInAcl(1, &ea, pOldDACL, &pNewDACL);
        if (res == ERROR_SUCCESS) {
            SetNamedSecurityInfoW((LPWSTR)wPath.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, pNewDACL, NULL);
        }

        if (pSD) LocalFree(pSD);
        if (pNewDACL) LocalFree(pNewDACL);
        if (pEveryoneSID) FreeSid(pEveryoneSID);

        return (res == ERROR_SUCCESS);
    } catch (...) {
        return false;
    }
}

bool UploadBatch(const Json::Value& batch) {
    if (batch.empty()) return true;
    try {
        CURL* curl = curl_easy_init();
        if (!curl) return false;

        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        std::string jsonStr = Json::writeString(writer, batch);

        std::string url = SUPABASE_URL + "/rest/v1/file_permissions";
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("apikey: " + SUPABASE_KEY).c_str());
        headers = curl_slist_append(headers, ("Authorization: Bearer " + SUPABASE_KEY).c_str());
        headers = curl_slist_append(headers, "Prefer: resolution=merge-duplicates");

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

        CURLcode res = curl_easy_perform(curl);
        
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK && (http_code >= 200 && http_code < 300)) {
            return true;
        } else {
            return false;
        }
    } catch (...) {
        return false;
    }
}

bool IsReparsePoint(const std::string& path) {
    DWORD attrs = GetFileAttributesA(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;
    return (attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

bool IsBlocked(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    static const std::set<std::string> BLOCKED = {
        "my music", "my pictures", "my videos", "start menu", 
        "application data", "local settings", "cookies", 
        "desktop.ini", "thumbs.db", "ntuser.dat", "$recycle.bin", "system volume information"
    };
    return BLOCKED.count(lower) > 0;
}

// Recursively scan a single scope root and sync to Supabase
void SyncScope(const std::string& rootPath, std::set<std::string>& currentPaths) {
    if (!fs::exists(rootPath)) return;

    Json::Value batch(Json::arrayValue);

    try {
        for (const auto& entry : fs::recursive_directory_iterator(rootPath, fs::directory_options::skip_permission_denied)) {
            try {
                std::string fullPath = entry.path().string();
                std::string fileName = entry.path().filename().string();
                
                if (IsBlocked(fileName)) continue;
                if (entry.is_symlink() || IsReparsePoint(fullPath)) continue;

                DWORD attrs = GetFileAttributesA(fullPath.c_str());
                if (attrs != INVALID_FILE_ATTRIBUTES && ((attrs & FILE_ATTRIBUTE_HIDDEN) || (attrs & FILE_ATTRIBUTE_SYSTEM))) {
                    continue;
                }

                std::string normPath = fullPath;
                std::replace(normPath.begin(), normPath.end(), '\\', '/');
                currentPaths.insert(normPath);

                std::string modTime;
                try {
                    auto ftime = fs::last_write_time(entry.path());
                    modTime = GetISOTimestamp(ftime);
                } catch(...) { modTime = "UNKNOWN"; }

                {
                    std::lock_guard<std::mutex> lock(g_cacheMutex);
                    if (g_fileCache.count(normPath) && g_fileCache[normPath] == modTime) {
                        continue; // Unchanged
                    }
                }

                Json::Value item;
                std::string parentPath = entry.path().parent_path().string();
                std::replace(parentPath.begin(), parentPath.end(), '\\', '/');

                item["file_path"] = normPath;
                item["name"] = fileName;
                item["parent_path"] = parentPath;
                item["is_directory"] = entry.is_directory();
                item["accessible"] = true; // Default
                item["last_modified"] = modTime;

                if (entry.is_directory()) {
                     item["file_size"] = "0";
                     item["file_extension"] = ""; 
                } else {
                     try { item["file_size"] = std::to_string(fs::file_size(entry.path())); } 
                     catch(...) { item["file_size"] = "0"; }
                     item["file_extension"] = entry.path().extension().string();
                }

                batch.append(item);
                
                {
                    std::lock_guard<std::mutex> lock(g_cacheMutex);
                    g_fileCache[normPath] = modTime;
                }

                if (batch.size() >= 50) {
                    UploadBatch(batch);
                    batch.clear();
                }

            } catch (...) {}
        }
        
        if (!batch.empty()) {
            UploadBatch(batch);
        }

    } catch (...) {}
}

struct Scope {
    std::string folder_name;
    std::string root_path;
};

std::vector<Scope> FetchScopes() {
    std::vector<Scope> scopes;
    scopes.push_back({"Documents", "C:/Users/Adi/Documents"});
    scopes.push_back({"Downloads", "C:/Users/Adi/Downloads"});
    return scopes;
}

void DeleteFileFromDB(const std::string& path) {
    try {
        CURL* curl = curl_easy_init();
        if (!curl) return;
        
        std::string encodedPath = path;
        char* output = curl_easy_escape(curl, path.c_str(), path.length());
        if(output) {
            encodedPath = std::string(output);
            curl_free(output);
        }

        std::string url = SUPABASE_URL + "/rest/v1/file_permissions?file_path=eq." + encodedPath;
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("apikey: " + SUPABASE_KEY).c_str());
        headers = curl_slist_append(headers, ("Authorization: Bearer " + SUPABASE_KEY).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); 
        curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    } catch (...) {}
}

void UpdateHeartbeat() {
    try {
        CURL* curl = curl_easy_init();
        if (!curl) return;
        std::string url = SUPABASE_URL + "/rest/v1/system_status?id=eq.1";
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("apikey: " + SUPABASE_KEY).c_str());
        headers = curl_slist_append(headers, ("Authorization: Bearer " + SUPABASE_KEY).c_str());
        
        std::string json = "{\"service_online\": true}";
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
        curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    } catch (...) {}
}

// === THREAD 1: SCANNER LOOP (Low Frequency) ===
void ScannerLoop() {
    LogMessage("Scanner Thread Started (Frequency: 5s)");
    while (g_running) {
        try {
            std::vector<Scope> scopes = FetchScopes();
            std::set<std::string> allCurrentPaths;

            for (const auto& scope : scopes) {
                SyncScope(scope.root_path, allCurrentPaths);
            }

            // Garbage Collection
            std::vector<std::string> toDelete;
            {
                std::lock_guard<std::mutex> lock(g_cacheMutex);
                for (auto it = g_fileCache.begin(); it != g_fileCache.end(); ) {
                    if (allCurrentPaths.find(it->first) == allCurrentPaths.end()) {
                        toDelete.push_back(it->first);
                        it = g_fileCache.erase(it);
                    } else {
                        ++it;
                    }
                }
            }

            for(const auto& path : toDelete) {
                 DeleteFileFromDB(path);
            }

            // Sleep 5 seconds
            std::this_thread::sleep_for(std::chrono::seconds(5));

        } catch (...) {}
    }
}

// === THREAD 2: ENFORCER LOOP (High Frequency) ===
void EnforcerLoop() {
    LogMessage("Enforcer Thread Started (Frequency: 500ms)");
    while (g_running) {
        try {
            CURL* curl = curl_easy_init();
            if (curl) {
                std::string response;
                std::string url = SUPABASE_URL + "/rest/v1/file_permissions?select=file_path,accessible";
                struct curl_slist* headers = NULL;
                headers = curl_slist_append(headers, ("apikey: " + SUPABASE_KEY).c_str());
                headers = curl_slist_append(headers, ("Authorization: Bearer " + SUPABASE_KEY).c_str());

                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L); // Fast timeout

                if (curl_easy_perform(curl) == CURLE_OK) {
                    Json::Value root;
                    Json::CharReaderBuilder builder;
                    std::istringstream stream(response);
                    std::string errs;
                    if (Json::parseFromStream(builder, stream, &root, &errs)) {
                        for (const auto& item : root) {
                            std::string path = item["file_path"].asString();
                            if (!item["accessible"].isNull()) {
                                bool accessible = item["accessible"].asBool();
                                std::replace(path.begin(), path.end(), '/', '\\');
                                
                                // CACHING: Only apply if state changed or new
                                bool stateChanged = false;
                                {
                                    std::lock_guard<std::mutex> lock(g_lockMutex);
                                    if (g_lockCache.find(path) == g_lockCache.end() || g_lockCache[path] != accessible) {
                                        stateChanged = true;
                                        g_lockCache[path] = accessible;
                                    }
                                }

                                if (stateChanged && fs::exists(path)) {
                                    // LogMessage("Enforcing Lock State change for: " + path);
                                    SetFileAccessibility(path, accessible);
                                }
                            }
                        }
                    }
                }
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
            }

            UpdateHeartbeat();
            
            // Sleep 500 ms (Fast Response)
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

        } catch (...) {}
    }
}

int main() {
    LogMessage("Starting GateKeeper Service v3.2 (Multi-Threaded)");
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Launch threads
    std::thread scanner(ScannerLoop);
    std::thread enforcer(EnforcerLoop);

    // Keep main thread alive
    scanner.join();
    enforcer.join();

    curl_global_cleanup();
    return 0;
}
