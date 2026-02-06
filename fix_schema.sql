-- ===================================================
-- GATEKEEPER - SCHEMA REPAIR
-- Run this if your frontend is connected but shows no files.
-- This ensures the table structure matches what the C++ agent expects.
-- ===================================================

-- 1. Create/Update the table
CREATE TABLE IF NOT EXISTS file_permissions (
    id BIGSERIAL PRIMARY KEY,
    file_path TEXT UNIQUE,
    name TEXT,
    is_directory BOOLEAN,
    accessible BOOLEAN DEFAULT true,
    parent_path TEXT,
    file_size TEXT,
    file_extension TEXT,
    last_modified TIMESTAMP WITH TIME ZONE
);

-- 2. Enable Realtime (Critical for the UI to update instantly)
-- First, check if the publication exists, if not create it, else just add the table.
-- Simpler approach that works in most Supabase setups:
-- alter publication supabase_realtime add table file_permissions;
-- NOTE: If the above line fails with "already member", it's fine. Proceed.

-- 3. Ensure permissions are OPEN for the frontend (as previously fixed)
DROP POLICY IF EXISTS "Allow all known users to read" ON file_permissions;
DROP POLICY IF EXISTS "Allow all access to file permissions" ON file_permissions;

CREATE POLICY "Allow all access to file permissions" 
ON file_permissions FOR ALL 
USING (TRUE) WITH CHECK (TRUE);
