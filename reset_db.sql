-- 1. Ensure the frontend sends full row data during updates so the C++ service can identify the path
ALTER TABLE file_permissions REPLICA IDENTITY FULL;

-- 2. Clear any stale data to ensure the new multi-scope logic starts with a clean state
DELETE FROM file_permissions;
