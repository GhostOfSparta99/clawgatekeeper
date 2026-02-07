-- Clean up high-volume noise files from database to fix latency
DELETE FROM file_permissions 
WHERE file_path LIKE '%/node_modules/%'
   OR file_path LIKE '%\node_modules\%'
   OR file_path LIKE '%/.git/%'
   OR file_path LIKE '%__pycache__%'
   OR file_path LIKE '%.vscode%'
   OR file_path LIKE '%/dist/%'
   OR file_path LIKE '%/build/%'
   OR file_path LIKE '%/vcpkg/%'
   OR file_path LIKE '%\vcpkg\%'
   OR file_path LIKE '%/vendor/%';

-- Reset any stuck locks
UPDATE file_permissions SET accessible = true WHERE accessible = false;
