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

// NEW: Scalability limits
const size_t MAX_FILES_PER_SCOPE = 50000;  // Prevent DB overload
const uintmax_t MAX_FILE_SIZE = 5ULL * 1024 * 1024 * 1024; // 5GB - skip very large files

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

#include <sddl.h>

// ... (includes remain) ...

// NEW: Force Unlock by overwriting DACL if standard access fails
bool ForceUnlock(const std::wstring& wPath) {
    // SDDL: D:(A;;FA;;;WD) -> DACL: Allow FileAll to World (Everyone)
    PSECURITY_DESCRIPTOR pSD = NULL;
    if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:(A;;FA;;;WD)", 
            SDDL_REVISION_1, 
            &pSD, 
            NULL)) {
        
        BOOL present = FALSE;
        BOOL defaulted = FALSE;
        PACL pDacl = NULL;
        GetSecurityDescriptorDacl(pSD, &present, &pDacl, &defaulted);
        
        DWORD res = SetNamedSecurityInfoW(
            (LPWSTR)wPath.c_str(), 
            SE_FILE_OBJECT, 
            DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, 
            NULL, NULL, pDacl, NULL);
            
        LocalFree(pSD);
        return (res == ERROR_SUCCESS);
    }
    return false;
}

bool SetFileAccessibility(const std::string& filePath, bool makeAccessible) {
    try {
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, NULL, 0);
        std::wstring wPath(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, &wPath[0], size_needed);

        // OPTIMIZATION: If Unlocking, try ForceUnlock directly if we suspect issues
        // But let's try standard way first.

        PACL pOldDACL = NULL, pNewDACL = NULL;
        PSECURITY_DESCRIPTOR pSD = NULL;
        EXPLICIT_ACCESS ea;

        DWORD res = GetNamedSecurityInfoW(wPath.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &pOldDACL, NULL, &pSD);
        if (res != ERROR_SUCCESS) {
            // NEW: If we can't read the DACL (Access Denied) and we want to UNLOCK, force it.
            if (makeAccessible) {
                LogMessage("WARNING: Standard Unlock failed (Access Denied). Attempting Force Unlock for: " + filePath);
                return ForceUnlock(wPath);
            }
            return false;
        }

        SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;
        PSID pEveryoneSID = NULL;
        if (!AllocateAndInitializeSid(&SIDAuthWorld, 1, SECURITY_WORLD_RID, 0,0,0,0,0,0,0, &pEveryoneSID)) {
            if (pSD) LocalFree(pSD);
            return false;
        }

        ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));
        ea.grfAccessPermissions = GENERIC_ALL;
        ea.grfAccessMode = makeAccessible ? SET_ACCESS : DENY_ACCESS;
        ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea.Trustee.ptstrName = (LPTSTR)pEveryoneSID;

        res = SetEntriesInAcl(1, &ea, pOldDACL, &pNewDACL);
        if (res == ERROR_SUCCESS) {
            res = SetNamedSecurityInfoW((LPWSTR)wPath.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, pNewDACL, NULL);
        }

        if (pSD) LocalFree(pSD);
        if (pNewDACL) LocalFree(pNewDACL);
        if (pEveryoneSID) FreeSid(pEveryoneSID);

        // NEW: Fallback
        if (res != ERROR_SUCCESS && makeAccessible) {
             LogMessage("WARNING: SetEntriesInAcl failed. Attempting Force Unlock for: " + filePath);
             return ForceUnlock(wPath);
        }

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
        "desktop.ini", "thumbs.db", "ntuser.dat", "$recycle.bin", 
        "system volume information", "windows", "program files"
    };
    return BLOCKED.count(lower) > 0;
}

// NEW: Path safety validator to prevent scanning dangerous directories
// Check if path is safely within the user's profile and NOT a system/hidden/heavy folder
bool IsSafePath(const std::string& path) {
    // 1. Explicitly Blocked System/Critical Paths
    static const std::vector<std::string> FORBIDDEN_SUBSTRINGS = {
        "C:/Windows", "C:\\Windows", 
        "C:/Program Files", "C:\\Program Files",
        "C:/Program Files (x86)", "C:\\Program Files (x86)",
        "AppData/Local", "AppData\\Local", 
        "AppData/Roaming", "AppData\\Roaming", 
        "$Recycle.Bin", "System Volume Information"
    };

    // 2. High-volume Development/Build Folders (Noisy & Useless to Sync)
    static const std::vector<std::string> IGNORED_FOLDERS = {
        "node_modules", ".git", ".vscode", ".idea", 
        "dist", "build", "vendor", "target", "bin", "obj",
        "__pycache__", ".DS_Store", "vcpkg", "ports"
    };
    
    // Check against forbidden system paths
    for (const auto& blocked : FORBIDDEN_SUBSTRINGS) {
        if (path.find(blocked) != std::string::npos) {
            // LogMessage("SAFETY: Blocked unsafe path - " + path); // Too noisy
            return false;
        }
    }

    // Check against high-volume ignore list (prevent latency spikes)
    for (const auto& ignored : IGNORED_FOLDERS) {
        if (path.find("/" + ignored + "/") != std::string::npos || 
            path.find("\\" + ignored + "\\") != std::string::npos ||
            path.find("/" + ignored) != std::string::npos ||  // End of path check
            path.find("\\" + ignored) != std::string::npos) {
            return false;
        }
    }
    
    char* userProfile = std::getenv("USERPROFILE");
    if (userProfile) {
        std::string userProfilePath(userProfile);
        std::replace(userProfilePath.begin(), userProfilePath.end(), '\\', '/');
        
        std::string testPath = path;
        std::replace(testPath.begin(), testPath.end(), '\\', '/');
        
        // Ensure we are inside user profile (unless explicitly allowed elsewhere)
        if (testPath.find(userProfilePath) == std::string::npos) {
            // LogMessage("SAFETY: Path outside user profile - " + path);
            return false;
        }
    }
    
    return true;
}

// NEW: Resolve OneDrive redirects automatically
std::string ResolveActualPath(const std::string& requestedPath) {
    // Check if local path exists and is not empty
    if (fs::exists(requestedPath) && !fs::is_empty(requestedPath)) {
        return requestedPath;
    }
    
    // Try OneDrive fallback
    char* userProfile = std::getenv("USERPROFILE");
    if (!userProfile) return requestedPath;
    
    std::string base(userProfile);
    std::replace(base.begin(), base.end(), '\\', '/');
    
    // Extract folder name (e.g., "Documents" from "C:/Users/Name/Documents")
    size_t lastSlash = requestedPath.find_last_of("/\\");
    std::string folderName = (lastSlash != std::string::npos) 
        ? requestedPath.substr(lastSlash + 1) 
        : requestedPath;
    
    std::string oneDrivePath = base + "/OneDrive/" + folderName;
    
    if (fs::exists(oneDrivePath)) {
        LogMessage("INFO: Redirecting " + folderName + " to OneDrive: " + oneDrivePath);
        return oneDrivePath;
    }
    
    return requestedPath;
}

// Helper to create a JSON record for a single file
Json::Value CreateFileRecord(const fs::directory_entry& entry) {
    Json::Value item;
    try {
        std::string fullPath = entry.path().string();
        std::string fileName = entry.path().filename().string();
        
        std::string normPath = fullPath;
        std::replace(normPath.begin(), normPath.end(), '\\', '/');

        std::string parentPath = entry.path().parent_path().string();
        std::replace(parentPath.begin(), parentPath.end(), '\\', '/');

        std::string modTime;
        try {
            auto ftime = fs::last_write_time(entry.path());
            modTime = GetISOTimestamp(ftime);
        } catch(...) { modTime = "UNKNOWN"; }

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
             try { 
                 uintmax_t size = fs::file_size(entry.path());
                 item["file_size"] = std::to_string(size); 
             } 
             catch(...) { item["file_size"] = "0"; }
             item["file_extension"] = entry.path().extension().string();
        }
    } catch (...) {}
    return item;
}

// Recursively scan a single scope root and sync to Supabase
void SyncScope(const std::string& rootPath, std::set<std::string>& currentPaths) {
    if (!fs::exists(rootPath)) {
        LogMessage("SKIP: Path does not exist - " + rootPath);
        return;
    }

    // NEW: Safety check
    if (!IsSafePath(rootPath)) {
        LogMessage("ERROR: Unsafe scope rejected - " + rootPath);
        return;
    }

    LogMessage("Syncing Scope: " + rootPath);
    
    Json::Value batch(Json::arrayValue);
    size_t fileCount = 0; // NEW: Track file count

    try {
        for (const auto& entry : fs::recursive_directory_iterator(rootPath, fs::directory_options::skip_permission_denied)) {
            try {
                // NEW: File count limit to prevent DB overload
                if (++fileCount > MAX_FILES_PER_SCOPE) {
                    LogMessage("WARNING: Scope " + rootPath + " exceeded " + std::to_string(MAX_FILES_PER_SCOPE) + " files. Truncating.");
                    break;
                }

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

                // Optimization: reuse CreateFileRecord logic? 
                // For now, let's keep the cache check here to avoid redundant logic changes in this Diff.
                // Actually, let's use the new helper!

                std::string modTime;
                try {
                    auto ftime = fs::last_write_time(entry.path());
                    modTime = GetISOTimestamp(ftime);
                } catch(...) { modTime = "UNKNOWN"; }

                {
                    std::lock_guard<std::mutex> lock(g_cacheMutex);
                    if (g_fileCache.count(normPath)) { 
                        if (g_fileCache[normPath] == modTime) {
                            continue; // Unchanged
                        }
                    }
                }

                // Check size limit before creating record
                if (!entry.is_directory()) {
                     try {
                         if (fs::file_size(entry.path()) > MAX_FILE_SIZE) continue;
                     } catch(...) {}
                }

                Json::Value item = CreateFileRecord(entry);
                if (item.empty()) continue; // Failed to create

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

        LogMessage("Scope scan completed: " + rootPath + " (" + std::to_string(fileCount) + " files)");

    } catch (...) {
        LogMessage("ERROR: Exception during scope scan - " + rootPath);
    }
}

// Assuming UploadBatch function exists and has the following structure for the logging to be added:
// void UploadBatch(const Json::Value& batch) {
//     CURL* curl = curl_easy_init();
//     // ... setup curl ...
//     std::string readBuffer; // Assuming this is used to capture response body
//     CURLcode res = curl_easy_perform(curl);
//     // ... cleanup curl ...
//     if (res != CURLE_OK) {
//         LogMessage("ERROR: UploadBatch Failed - " + std::string(curl_easy_strerror(res)));
//     } else {
//         long response_code;
//         curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
//         if (response_code != 200 && response_code != 201) {
//              LogMessage("ERROR: UploadBatch HTTP Error " + std::to_string(response_code) + " - " + readBuffer);
//         }
//         // LogMessage("DEBUG: Batch Uploaded (" + std::to_string(batch.size()) + " items)");
//     }
// }

struct Scope {
    std::string folder_name;
    std::string root_path;
};

// ENHANCED: FetchScopes now includes Phase 1 paths with automatic OneDrive fallback
std::vector<Scope> FetchScopes() {
    std::vector<Scope> scopes;
    
    // Get the dynamic user profile path
    char* userProfile = std::getenv("USERPROFILE");
    std::string base = userProfile ? std::string(userProfile) : "C:/Users/Adi";
    std::replace(base.begin(), base.end(), '\\', '/');

    // EXISTING PATHS (unchanged)
    scopes.push_back({"Documents", base + "/Documents"});
    scopes.push_back({"Downloads", base + "/Downloads"});
    
    // ===== PHASE 1 NEW PATHS =====
    scopes.push_back({"Desktop", base + "/Desktop"});
    scopes.push_back({"Pictures", base + "/Pictures"});
    scopes.push_back({"Videos", base + "/Videos"});
    // ==============================
    
    // Validate and resolve all paths
    std::vector<Scope> validated;
    for (auto& scope : scopes) {
        // Try OneDrive fallback if local path is missing/empty
        scope.root_path = ResolveActualPath(scope.root_path);
        
        if (IsSafePath(scope.root_path) && fs::exists(scope.root_path)) {
            validated.push_back(scope);
            LogMessage("Scope ACTIVE: " + scope.folder_name + " at " + scope.root_path);
        } else {
            LogMessage("Scope SKIPPED: " + scope.folder_name + " (not found or unsafe)");
        }
    }
    
    return validated;
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
    static std::chrono::steady_clock::time_point lastHeartbeat = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    
    // THROTTLE: Only update every 10 seconds
    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastHeartbeat).count() < 10) {
        return; 
    }
    lastHeartbeat = now;

    try {
        CURL* curl = curl_easy_init();
        if (!curl) return;
        std::string url = SUPABASE_URL + "/rest/v1/system_status?id=eq.1";
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("apikey: " + SUPABASE_KEY).c_str());
        headers = curl_slist_append(headers, ("Authorization: Bearer " + SUPABASE_KEY).c_str());
        
        std::string json = "{\"service_online\": true, \"last_seen\": \"now()\"}";
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
        
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
             LogMessage("ERROR: Heartbeat Failed - " + std::string(curl_easy_strerror(res)));
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    } catch (...) {}
}

// === REAL-TIME WATCHER LOGIC ===

void HandleFileChange(const std::string& fullPath, DWORD action) {
    try {
        std::string normPath = fullPath;
        std::replace(normPath.begin(), normPath.end(), '\\', '/');

        if (!IsSafePath(normPath)) return;
        
        std::string fileName = fs::path(normPath).filename().string();
        if (IsBlocked(fileName)) return;

        // Debounce: If we just processed this file in the last 500ms, skip
        // (Simplified for now: Just proceed, Supabase handles merge)

        if (action == FILE_ACTION_ADDED || action == FILE_ACTION_MODIFIED || action == FILE_ACTION_RENAMED_NEW_NAME) {
            if (fs::exists(fullPath)) {
                // Wait briefly for file lock to release if it's being written
                std::this_thread::sleep_for(std::chrono::milliseconds(50)); 
                
                try {
                    fs::directory_entry entry(fullPath);
                    Json::Value item = CreateFileRecord(entry);
                    if (!item.empty()) {
                        Json::Value batch(Json::arrayValue);
                        batch.append(item);
                        UploadBatch(batch);
                        LogMessage("WATCHER: Synced " + fileName);
                    }
                } catch(...) {}
            }
        } else if (action == FILE_ACTION_REMOVED || action == FILE_ACTION_RENAMED_OLD_NAME) {
            DeleteFileFromDB(normPath);
            LogMessage("WATCHER: Removed " + fileName);
        }
    } catch (...) {}
}

void WatchDirectory(std::string path) {
    LogMessage("WATCHER START: " + path);
    
    HANDLE hDir = CreateFileA(path.c_str(), FILE_LIST_DIRECTORY, 
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 
        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    
    if (hDir == INVALID_HANDLE_VALUE) {
        LogMessage("WATCHER ERROR: Invalid Handle for " + path);
        return;
    }

    char buffer[1024 * 64]; // 64KB Buffer
    DWORD bytesReturned;
    
    while (g_running) {
        if (ReadDirectoryChangesW(hDir, buffer, sizeof(buffer), TRUE, 
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
            &bytesReturned, NULL, NULL)) {
            
            FILE_NOTIFY_INFORMATION* pNotify = (FILE_NOTIFY_INFORMATION*)buffer;
            do {
                std::wstring wFileName(pNotify->FileName, pNotify->FileNameLength / sizeof(WCHAR));
                
                // Convert WCHAR to std::string (UTF-8)
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, wFileName.c_str(), (int)wFileName.length(), NULL, 0, NULL, NULL);
                std::string fileName(size_needed, 0);
                WideCharToMultiByte(CP_UTF8, 0, wFileName.c_str(), (int)wFileName.length(), &fileName[0], size_needed, NULL, NULL);

                std::string fullPath = path + "/" + fileName;
                
                HandleFileChange(fullPath, pNotify->Action);

                if (pNotify->NextEntryOffset == 0) break;
                pNotify = (FILE_NOTIFY_INFORMATION*)((char*)pNotify + pNotify->NextEntryOffset);
            } while (true);
        } else {
            // Wait a bit before retrying if error (or just exit loop if handle died)
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    CloseHandle(hDir);
    LogMessage("WATCHER STOP: " + path);
}

// === THREAD 1: SCANNER LOOP (Low Frequency Consistency Check) ===
void ScannerLoop() {
    LogMessage("Scanner Thread Started (Frequency: 5 minutes - Fallback Mode)");
    while (g_running) {
        try {
            std::vector<Scope> scopes = FetchScopes();
            // LogMessage("INFO: Consistency Scan Started...");
            
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

            if (!toDelete.empty()) {
                LogMessage("Garbage Collection: Removing " + std::to_string(toDelete.size()) + " deleted files from DB");
                for(const auto& path : toDelete) {
                     DeleteFileFromDB(path);
                }
            }

            // Sleep 5 minutes (Real-time checks handled by Watchers)
            for(int i=0; i<300 && g_running; i++) std::this_thread::sleep_for(std::chrono::seconds(1));

        } catch (...) {
            LogMessage("ERROR: Exception in ScannerLoop");
        }
    }
}

// === THREAD 2: ENFORCER LOOP (High Frequency) ===
void EnforcerLoop() {
    LogMessage("Enforcer Thread Started (Frequency: 100ms)");
    while (g_running) {
        try {
            // LogMessage("DEBUG: Enforcer Heartbeat - Active"); 
            long offset = 0;
            bool moreData = true;
            int totalProcessed = 0;

            while (moreData && g_running) {
                CURL* curl = curl_easy_init();
                if (!curl) break;

                std::string response;
                // PAGINATION FIX: Limit 1000 (Supabase default max) + Stable Sort
                std::string url = SUPABASE_URL + "/rest/v1/file_permissions?select=file_path,accessible&limit=1000&order=file_path.asc&offset=" + std::to_string(offset);
                
                struct curl_slist* headers = NULL;
                headers = curl_slist_append(headers, ("apikey: " + SUPABASE_KEY).c_str());
                headers = curl_slist_append(headers, ("Authorization: Bearer " + SUPABASE_KEY).c_str());

                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // Standard timeout for chunk

                CURLcode res = curl_easy_perform(curl);
                long http_code = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);

                if (res == CURLE_OK && http_code == 200) {
                    Json::Value root;
                    Json::CharReaderBuilder builder;
                    std::istringstream stream(response);
                    std::string errs;
                    if (Json::parseFromStream(builder, stream, &root, &errs)) {
                        if (root.empty()) {
                            moreData = false;
                        } else {
                            for (int i = 0; i < root.size(); i++) {
                                const auto& item = root[i];
                                std::string path = item["file_path"].asString();
                                bool accessible = item["accessible"].asBool();
                                
                                if (i == 0) {
                                    // LogMessage("DEBUG: Sample file check: " + path + " | Access: " + (accessible ? "TRUE" : "FALSE"));
                                }

                                // Fix path for Windows API
                                std::string winPath = path;
                                std::replace(winPath.begin(), winPath.end(), '/', '\\');
                                
                                // OPTIMIZATION: Skip unsafe/junk paths immediately
                                if (!IsSafePath(winPath)) {
                                    continue; 
                                }
                                
                                // CACHING: Check if state actually changed
                                bool stateChanged = false;
                                {
                                    std::lock_guard<std::mutex> lock(g_lockMutex);
                                    bool isNew = g_lockCache.find(winPath) == g_lockCache.end();
                                    
                                    if (isNew) {
                                        // STARTUP OPTIMIZATION:
                                        // If DB says "Accessible (True)", assume file is already unlocked.
                                        // Only enforce if DB says "Locked (False)" to avoid 50k syscalls on startup.
                                        if (!accessible) {
                                            stateChanged = true;
                                        }
                                        g_lockCache[winPath] = accessible;
                                    } else {
                                        if (g_lockCache[winPath] != accessible) {
                                            stateChanged = true;
                                            g_lockCache[winPath] = accessible;
                                        }
                                    }
                                }

                                if (stateChanged) {
                                    if (fs::exists(winPath)) {
                                         LogMessage("DEBUG: Enforcing Lock State " + std::string(accessible ? "UNLOCK" : "LOCK") + " on: " + winPath);
                                         bool success = SetFileAccessibility(winPath, accessible);
                                         if (!success) LogMessage("ERROR: Lock Failed for " + winPath);
                                    }
                                }
                            }
                            
                            totalProcessed += root.size();
                            // LogMessage("DEBUG: Processed batch of " + std::to_string(root.size()));
                            offset += root.size();
                            if (root.size() < 1000) moreData = false; // End of list (Limit is 1000)
                        }
                    } else {
                        LogMessage("ERROR: Failed to parse JSON response");
                        moreData = false;
                    }
                } else {
                    LogMessage("ERROR: Valid response fetch failed. Code: " + std::to_string(http_code));
                    moreData = false;
                }
            }
            
            // LogMessage("DEBUG: Enforcer Cycle Complete. Processed: " + std::to_string(totalProcessed));
            UpdateHeartbeat();
            
            // OPTIMIZATION: Reduce sleep to 100ms for faster response
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        } catch (...) {
            LogMessage("ERROR: Exception in EnforcerLoop");
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

int main() {
    LogMessage("=== Starting GateKeeper Service v4.0 (Real-Time Watchers) ===");
    LogMessage("MODE: Directory Watchers Active + 5min Consistency Check");
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    std::vector<Scope> scopes = FetchScopes();
    std::vector<std::thread> threads;

    // 1. Start Watcher Threads (One per Scope)
    for (const auto& scope : scopes) {
        threads.emplace_back(std::thread(WatchDirectory, scope.root_path));
    }

    // 2. Start Scanner (Consistency) and Enforcer (Locks)
    threads.emplace_back(std::thread(ScannerLoop));
    threads.emplace_back(std::thread(EnforcerLoop));

    // Join all (keep alive)
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    curl_global_cleanup();
    return 0;
}
