-- ===================================================
-- GATEKEEPER - FORCE ACCESS (DISABLE RLS) - FIXED
-- Run this in Supabase SQL Editor
-- ===================================================

-- 1. Disable RLS on all tables
-- This allows the C++ Service (using Anon Key) to Write/Update data.
ALTER TABLE file_permissions DISABLE ROW LEVEL SECURITY;
ALTER TABLE system_status DISABLE ROW LEVEL SECURITY;
ALTER TABLE permission_audit_log DISABLE ROW LEVEL SECURITY;

-- 2. Explicitly Grant Privileges to the 'anon' role
GRANT ALL ON TABLE file_permissions TO anon;
GRANT ALL ON TABLE system_status TO anon;
GRANT ALL ON TABLE permission_audit_log TO anon;

GRANT ALL ON TABLE file_permissions TO service_role;
GRANT ALL ON TABLE system_status TO service_role;
GRANT ALL ON TABLE permission_audit_log TO service_role;

-- 3. Grant usage on sequences (for creating new rows)
GRANT USAGE, SELECT ON SEQUENCE file_permissions_id_seq TO anon;
-- (Optional: if the audit log sequence exists)
-- GRANT USAGE, SELECT ON SEQUENCE permission_audit_log_id_seq TO anon;
