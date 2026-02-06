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

namespace fs = std::filesystem;

// ===== CONFIGURATION =====
const std::string SUPABASE_URL = "https://hyqmdnzkxhkvzynzqwug.supabase.co";
const std::string SUPABASE_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Imh5cW1kbnpreGhrdnp5bnpxd3VnIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzAyODM0MjksImV4cCI6MjA4NTg1OTQyOX0.Oo-jJIivU0YHRbloTJlpl_cwQ9CkrMq6tV9YQUu8F5w";
const std::string LOG_FILE = "C:\\GateKeeper\\debug.log";
const std::string SCAN_DIR = "C:\\Users\\Adi\\Documents";

void LogMessage(const std::string& message) {
    try {
        if (!fs::exists("C:\\GateKeeper")) {
            fs::create_directories("C:\\GateKeeper");
        }
        std::ofstream logFile(LOG_FILE, std::ios::app);
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::cout << "[" << std::put_time(&tm, "%H:%M:%S") << "] " << message << std::endl;
        if (logFile.is_open()) {
            logFile << "[" << std::put_time(&tm, "%H:%M:%S") << "] " << message << std::endl;
        }
    } catch (...) {
        std::cout << "LOG ERROR: " << message << std::endl;
    }
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

// FIXED: Windows ACL permission setter
bool SetFileAccessibility(const std::string& filePath, bool makeAccessible) {
    try {
        // Convert to wide string for Windows API
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, NULL, 0);
        std::wstring wPath(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, &wPath[0], size_needed);

        PACL pOldDACL = NULL, pNewDACL = NULL;
        PSECURITY_DESCRIPTOR pSD = NULL;
        EXPLICIT_ACCESS ea;

        // Get current DACL
        DWORD res = GetNamedSecurityInfoW(
            wPath.c_str(),
            SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION,
            NULL, NULL,
            &pOldDACL, NULL,
            &pSD
        );

        if (res != ERROR_SUCCESS) {
            LogMessage("GetNamedSecurityInfo failed: " + std::to_string(res));
            return false;
        }

        // Build EVERYONE SID
        SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;
        PSID pEveryoneSID = NULL;
        
        if (!AllocateAndInitializeSid(&SIDAuthWorld, 1,
            SECURITY_WORLD_RID,
            0, 0, 0, 0, 0, 0, 0,
            &pEveryoneSID)) {
            if (pSD) LocalFree(pSD);
            return false;
        }

        // Set up ACE
        ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));
        ea.grfAccessPermissions = GENERIC_ALL;
        // FIXED: SET_ACCESS replaces existing rules (removes DENY), GRANT_ACCESS only adds (leaves DENY)
        ea.grfAccessMode = makeAccessible ? SET_ACCESS : DENY_ACCESS;
        ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea.Trustee.ptstrName = (LPTSTR)pEveryoneSID;

        // Create new ACL
        res = SetEntriesInAcl(1, &ea, pOldDACL, &pNewDACL);
        
        if (res == ERROR_SUCCESS) {
            // Apply new ACL
            res = SetNamedSecurityInfoW(
                (LPWSTR)wPath.c_str(),
                SE_FILE_OBJECT,
                DACL_SECURITY_INFORMATION,
                NULL, NULL,
                pNewDACL, NULL
            );
        }

        // Cleanup
        if (pSD) LocalFree(pSD);
        if (pNewDACL) LocalFree(pNewDACL);
        if (pEveryoneSID) FreeSid(pEveryoneSID);

        if (res == ERROR_SUCCESS) {
            LogMessage("Permission applied: " + filePath + " -> " + (makeAccessible ? "accessible" : "locked"));
            return true;
        } else {
            LogMessage("SetNamedSecurityInfo failed: " + std::to_string(res));
            return false;
        }

    } catch (const std::exception& e) {
        LogMessage("Exception in SetFileAccessibility: " + std::string(e.what()));
        return false;
    } catch (...) {
        LogMessage("Unknown exception in SetFileAccessibility");
        return false;
    }
}

// FIXED: Upload with UPSERT (no duplicate errors)
bool UploadBatch(const Json::Value& batch, int batchNum) {
    try {
        CURL* curl = curl_easy_init();
        if (!curl) {
            LogMessage("CURL init failed for batch " + std::to_string(batchNum));
            return false;
        }

        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        std::string jsonStr = Json::writeString(writer, batch);

        LogMessage("Uploading batch " + std::to_string(batchNum) + " (" + std::to_string(batch.size()) + " items)");

        std::string url = SUPABASE_URL + "/rest/v1/file_permissions";
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("apikey: " + SUPABASE_KEY).c_str());
        headers = curl_slist_append(headers, ("Authorization: Bearer " + SUPABASE_KEY).c_str());
        headers = curl_slist_append(headers, "Prefer: resolution=merge-duplicates"); // UPSERT!

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

        if (res != CURLE_OK) {
            LogMessage("CURL error: " + std::string(curl_easy_strerror(res)));
            return false;
        }

        if (http_code >= 200 && http_code < 300) {
            LogMessage("Batch " + std::to_string(batchNum) + " SUCCESS");
            return true;
        } else {
            LogMessage("Batch " + std::to_string(batchNum) + " HTTP " + std::to_string(http_code));
            return false;
        }
    } catch (...) {
        LogMessage("Exception in UploadBatch");
        return false;
    }
}

// Helper to check for Reparse Points (Junctions/Symlinks) reliably on Windows
bool IsReparsePoint(const std::string& path) {
    DWORD attrs = GetFileAttributesA(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;
    return (attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

Json::Value ScanDirectory() {
    Json::Value allItems(Json::arrayValue);
    
    try {
        if (!fs::exists(SCAN_DIR)) {
            LogMessage("ERROR: Directory not found: " + SCAN_DIR);
            return allItems;
        }

        LogMessage("Scanning " + SCAN_DIR + "...");
        int count = 0;

        for (const auto& entry : fs::recursive_directory_iterator(
            SCAN_DIR, 
            fs::directory_options::skip_permission_denied
        )) {
            try {
                std::string fullPath = entry.path().string();
                std::string fileName = entry.path().filename().string();
                
                // ToLower helper
                std::string lowerName = fileName;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

                // Blocklist for system folders/files (Junctions, System Artifacts)
                static const std::set<std::string> BLOCKED_NAMES = {
                    "my music", "my pictures", "my videos", "start menu", 
                    "nethood", "printhood", "recent", "sendto", 
                    "application data", "local settings", "cookies", 
                    "desktop.ini", "thumbs.db", "ntuser.dat", "printreader",
                    "documents" // Prevent self-reference if scanning root
                };

                if (lowerName == "templates") {
                     LogMessage("DEBUG: Found templates folder: " + fullPath);
                     if (BLOCKED_NAMES.count(lowerName)) {
                         LogMessage("DEBUG: BLOCKING templates folder.");
                     } else {
                         LogMessage("DEBUG: ALLOWING templates folder.");
                     }
                }

                if (BLOCKED_NAMES.count(lowerName)) {
                    continue;
                }
                
                // Check for Windows system/hidden files using file attributes
                DWORD attrs = GetFileAttributesA(fullPath.c_str());
                if (attrs != INVALID_FILE_ATTRIBUTES) {
                    // Skip hidden or system files
                    if ((attrs & FILE_ATTRIBUTE_HIDDEN) || (attrs & FILE_ATTRIBUTE_SYSTEM)) {
                        continue;
                    }
                }
                
                // Robust check for Junctions/Symlinks
                if (entry.is_symlink() || IsReparsePoint(fullPath)) {
                    continue;
                }

                Json::Value item;
                std::string parentPath = entry.path().parent_path().string();
                
                // Normalize paths
                std::replace(fullPath.begin(), fullPath.end(), '\\', '/');
                std::replace(parentPath.begin(), parentPath.end(), '\\', '/');
                
                item["file_path"] = fullPath;
                item["name"] = fileName;
                item["parent_path"] = parentPath;
                item["is_directory"] = entry.is_directory();
                item["accessible"] = true;
                item["to_be_deleted"] = false;
                
                if (entry.is_directory()) {
                    item["file_size"] = "0";
                    item["file_extension"] = "";
                } else {
                    try {
                        item["file_size"] = std::to_string(fs::file_size(entry.path()));
                    } catch(...) {
                        item["file_size"] = "0";
                    }
                    item["file_extension"] = entry.path().extension().string();
                }
                
                try {
                    auto ftime = fs::last_write_time(entry.path());
                    item["last_modified"] = GetISOTimestamp(ftime);
                } catch(...) {
                    item["last_modified"] = "2026-01-01T00:00:00Z";
                }
                
                allItems.append(item);
                count++;
                
                // if (count % 100 == 0) std::cout << "."; // Reduced spam
                
            } catch(const std::exception& e) {
                 // std::cout << "Available skip: " << e.what() << std::endl;
                continue;
            }
        }
        
        // LogMessage("Scan complete: " + std::to_string(count) + " items");
        
    } catch (const std::exception& e) {
        LogMessage("Scan error: " + std::string(e.what()));
    } catch (...) {
        LogMessage("Scan error: Unknown");
    }
    
    return allItems;
}

std::vector<std::pair<std::string, bool>> FetchPermissions() {
    std::vector<std::pair<std::string, bool>> permissions;
    
    try {
        CURL* curl = curl_easy_init();
        if (!curl) return permissions;

        std::string response;
        std::string url = SUPABASE_URL + "/rest/v1/file_permissions?select=file_path,accessible";
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("apikey: " + SUPABASE_KEY).c_str());
        headers = curl_slist_append(headers, ("Authorization: Bearer " + SUPABASE_KEY).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        if (curl_easy_perform(curl) == CURLE_OK) {
            Json::Value root;
            Json::CharReaderBuilder builder;
            std::istringstream stream(response);
            std::string errs;
            
            if (Json::parseFromStream(builder, stream, &root, &errs)) {
                for (const auto& item : root) {
                    std::string path = item["file_path"].asString();
                    std::replace(path.begin(), path.end(), '/', '\\');
                    
                    if (!item["accessible"].isNull()) {
                        permissions.push_back({path, item["accessible"].asBool()});
                    }
                }
            }
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    } catch (...) {
        LogMessage("Exception fetching permissions");
    }
    
    return permissions;
}

// Helper to delete a file from Supabase
void DeleteFileFromDB(const std::string& path) {
    try {
        CURL* curl = curl_easy_init();
        if (!curl) return;

        // URL encode the path for the query string
        // FIXED: Normalize to forward slashes because DB stores them as C:/...
        std::string dbPath = path;
        std::replace(dbPath.begin(), dbPath.end(), '\\', '/');
        
        char* encodedPath = curl_easy_escape(curl, dbPath.c_str(), dbPath.length());
        std::string url = SUPABASE_URL + "/rest/v1/file_permissions?file_path=eq." + std::string(encodedPath);
        
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("apikey: " + SUPABASE_KEY).c_str());
        headers = curl_slist_append(headers, ("Authorization: Bearer " + SUPABASE_KEY).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        CURLcode res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
             long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code == 200 || http_code == 204) {
                 LogMessage("Deleted from DB: " + path);
            } else {
                 LogMessage("Failed to delete " + path + ": HTTP " + std::to_string(http_code));
            }
        }
        
        curl_free(encodedPath);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    } catch (...) {
        LogMessage("Exception deleting file: " + path);
    }
}

void UpdateHeartbeat(bool online) {
    try {
        CURL* curl = curl_easy_init();
        if (!curl) return;

        Json::Value data;
        data["service_online"] = online;

        Json::StreamWriterBuilder writer;
        std::string jsonData = Json::writeString(writer, data);

        std::string url = SUPABASE_URL + "/rest/v1/system_status?id=eq.1";
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("apikey: " + SUPABASE_KEY).c_str());
        headers = curl_slist_append(headers, ("Authorization: Bearer " + SUPABASE_KEY).c_str());

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        curl_easy_perform(curl);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    } catch (...) {
        // Silent fail
    }
}

// Cache to track existing files and prevent redundant uploads
std::map<std::string, std::string> g_fileCache; // Path -> LastModified

int main() {
    try {
        LogMessage("========================================");
        LogMessage("GateKeeper Service v2.2 (Delta Sync)");
        LogMessage("========================================");
        LogMessage("Scanning: " + SCAN_DIR);
        LogMessage("========================================");
        
        curl_global_init(CURL_GLOBAL_DEFAULT);
        UpdateHeartbeat(true);
        
        LogMessage("Building initial file cache...");
        
        // Initial Scan - Populate cache AND Upload
        // We upload everything once to ensure DB is in sync at startup
        Json::Value allFiles = ScanDirectory();
        
        if (!allFiles.empty()) {
            LogMessage("Found " + std::to_string(allFiles.size()) + " files. Syncing...");
            
            const int BATCH_SIZE = 20;
            Json::Value batch(Json::arrayValue);
            
            for (const auto& item : allFiles) {
                // Add to cache
                std::string path = item["file_path"].asString();
                std::string mod = item["last_modified"].asString();
                g_fileCache[path] = mod;
                
                // Add to batch
                batch.append(item);
                
                if (batch.size() >= BATCH_SIZE) {
                    UploadBatch(batch, 0); // Batch number 0 for startup
                    batch.clear();
                    Sleep(50); // Small delay
                }
            }
            // Upload remaining
            if (!batch.empty()) {
                UploadBatch(batch, 0);
            }
            
            LogMessage("Initial sync complete.");
        }

        // ==========================================
        // FIXED: Cleanup Orphans / Stale DB Entries
        // ==========================================
        LogMessage("Checking for stale DB entries...");
        auto dbFiles = FetchPermissions(); // Fetch all paths currently in DB
        
        // Build set of valid disk paths from our scan
        std::set<std::string> validDiskPaths;
        for (const auto& item : allFiles) {
            validDiskPaths.insert(item["file_path"].asString());
        }

        int staleCount = 0;
        for (const auto& dbFile : dbFiles) {
            // dbFile.first is the path (which FetchPermissions converted to backslashes)
            // But validDiskPaths uses forward slashes. We must normalize to compare!
            std::string dbPathToCheck = dbFile.first;
            std::replace(dbPathToCheck.begin(), dbPathToCheck.end(), '\\', '/');

            if (validDiskPaths.find(dbPathToCheck) == validDiskPaths.end()) {
                // It's in DB but NOT in our valid disk scan -> DELETE IT
                LogMessage("Removing orphaned/stale entry: " + dbPathToCheck);
                DeleteFileFromDB(dbPathToCheck);
                staleCount++;
            }
        }
        if (staleCount > 0) {
            LogMessage("Cleanup complete. Removed " + std::to_string(staleCount) + " stale items.");
        } else {
            LogMessage("Database is clean. No stale items.");
        }
        // ==========================================
        
        LogMessage("========================================");
        LogMessage("Service is Active - Delta Sync Enabled");
        LogMessage("========================================");
        
        int cycle = 0;
        
        while (true) {
            cycle++;
            
            try {
                // Rescan phase
                if (cycle % 2 == 0) { // Every 2 seconds
                    Json::Value currentFiles = ScanDirectory();
                    Json::Value filesToUpload(Json::arrayValue);
                    
                    std::set<std::string> currentPaths; 
                    
                    // 1. Detect New/Modified Files
                    for (const auto& item : currentFiles) {
                        std::string path = item["file_path"].asString();
                        std::string mod = item["last_modified"].asString();
                        
                        currentPaths.insert(path);
                        
                        // Check if new or modified
                        if (g_fileCache.find(path) == g_fileCache.end() || g_fileCache[path] != mod) {
                            LogMessage("Detected change: " + item["name"].asString());
                            filesToUpload.append(item);
                            g_fileCache[path] = mod; // Update cache
                        }
                    }
                    
                    // 2. Detect Deletions (In Cache BUT NOT in Current Scan)
                    std::vector<std::string> filesToDelete;
                    for (auto it = g_fileCache.begin(); it != g_fileCache.end(); ) {
                        if (currentPaths.find(it->first) == currentPaths.end()) {
                            // File is in cache but not in current scan -> DELETED
                            LogMessage("Detected deletion: " + it->first);
                            filesToDelete.push_back(it->first);
                            it = g_fileCache.erase(it); // Remove from cache
                        } else {
                            ++it;
                        }
                    }

                    // 3. Exec Sync Actions
                    
                    // A. Upload Changes
                    if (!filesToUpload.empty()) {
                        LogMessage("Syncing " + std::to_string(filesToUpload.size()) + " changes...");
                        const int BATCH_SIZE = 20;
                        for (int i = 0; i < filesToUpload.size(); i += BATCH_SIZE) {
                            Json::Value batch(Json::arrayValue);
                            int end = std::min(i + BATCH_SIZE, (int)filesToUpload.size());
                            for (int j = i; j < end; j++) {
                                batch.append(filesToUpload[j]);
                            }
                            UploadBatch(batch, (i / BATCH_SIZE) + 1);
                        }
                    }
                    
                    // B. Process Deletions
                    if (!filesToDelete.empty()) {
                        for (const auto& path : filesToDelete) {
                            DeleteFileFromDB(path); // You'll need to implement this helper above main
                        }
                    }
                }
                
                // Apply permissions (Always check this, as DB is source of truth for locks)
                auto perms = FetchPermissions();
                if (!perms.empty()) {
                    for (const auto& p : perms) {
                        if (fs::exists(p.first)) {
                            SetFileAccessibility(p.first, p.second);
                        }
                    }
                }
                
                UpdateHeartbeat(true);
                Sleep(1000); 

            } catch (const std::exception& e) {
                LogMessage("Loop Error: " + std::string(e.what()));
                Sleep(2000); 
            } catch (...) {
                LogMessage("Loop Error: Unknown");
                Sleep(2000);
            }
        }
        
        curl_global_cleanup();
        
    } catch (const std::exception& e) {
        LogMessage("FATAL ERROR (Main): " + std::string(e.what()));
        std::cin.get();
        return 1;
    } catch (...) {
        LogMessage("FATAL ERROR (Main): Unknown");
        std::cin.get();
        return 1;
    }
    
    return 0;
}
