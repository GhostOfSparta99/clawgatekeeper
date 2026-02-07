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
        ea.grfAccessMode = makeAccessible ? SET_ACCESS : DENY_ACCESS;
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
        "desktop.ini", "thumbs.db", "ntuser.dat", "$recycle.bin", 
        "system volume information", "windows", "program files"
    };
    return BLOCKED.count(lower) > 0;
}

// NEW: Path safety validator to prevent scanning dangerous directories
bool IsSafePath(const std::string& path) {
    // Blocklist: System-critical directories
    static const std::vector<std::string> FORBIDDEN = {
        "C:/Windows",
        "C:\\Windows",
        "C:/Program Files",
        "C:\\Program Files",
        "C:/Program Files (x86)",
        "C:\\Program Files (x86)",
        "AppData/Local",
        "AppData\\Local",
        "AppData/Roaming",
        "AppData\\Roaming",
        "$Recycle.Bin",
        "System Volume Information"
    };
    
    for (const auto& blocked : FORBIDDEN) {
        if (path.find(blocked) != std::string::npos) {
            LogMessage("SAFETY: Blocked unsafe path - " + path);
            return false;
        }
    }
    
    // Must be under user profile for safety
    char* userProfile = std::getenv("USERPROFILE");
    if (userProfile) {
        std::string userProfilePath(userProfile);
        std::replace(userProfilePath.begin(), userProfilePath.end(), '\\', '/');
        
        std::string testPath = path;
        std::replace(testPath.begin(), testPath.end(), '\\', '/');
        
        if (testPath.find(userProfilePath) == std::string::npos) {
            LogMessage("SAFETY: Path outside user profile - " + path);
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
                     try { 
                         uintmax_t size = fs::file_size(entry.path());
                         // NEW: Skip very large files
                         if (size > MAX_FILE_SIZE) {
                             LogMessage("SKIP: File too large (" + std::to_string(size) + " bytes) - " + fullPath);
                             continue;
                         }
                         item["file_size"] = std::to_string(size); 
                     } 
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

        LogMessage("Scope scan completed: " + rootPath + " (" + std::to_string(fileCount) + " files)");

    } catch (...) {
        LogMessage("ERROR: Exception during scope scan - " + rootPath);
    }
}

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
            LogMessage("INFO: Scanning " + std::to_string(scopes.size()) + " active scopes");
            
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

            // Sleep 5 seconds
            std::this_thread::sleep_for(std::chrono::seconds(5));

        } catch (...) {
            LogMessage("ERROR: Exception in ScannerLoop");
        }
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
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);

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

        } catch (...) {
            LogMessage("ERROR: Exception in EnforcerLoop");
        }
    }
}

int main() {
    LogMessage("=== Starting GateKeeper Service v3.3 (Phase 1 Expansion) ===");
    LogMessage("NEW PATHS ADDED: Desktop, Pictures, Videos");
    LogMessage("SAFETY FEATURES: Path validation, OneDrive fallback, file limits");
    
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
