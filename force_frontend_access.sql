-- =========================================================
-- GATEKEEPER - FORCE FRONTEND ACCESS (FIX FAILED UPDATE)
-- =========================================================

-- 1. Drop existing policies to avoid conflicts
DROP POLICY IF EXISTS "Allow Frontend Update" ON file_permissions;
DROP POLICY IF EXISTS "Allow Service to Insert Logs" ON permission_audit_log;
DROP POLICY IF EXISTS "Enable All Access" ON file_permissions;
DROP POLICY IF EXISTS "Enable All Access" ON permission_audit_log;

-- 2. NUCLEAR OPTION: Disable RLS for file_permissions
-- This removes ALL restrictions. Since this is a personal tool, this is the safest way to ensure it works.
ALTER TABLE file_permissions DISABLE ROW LEVEL SECURITY;

-- 3. (Alternative) If you prefer RLS to stay On, use this:
-- ALTER TABLE file_permissions ENABLE ROW LEVEL SECURITY;
-- CREATE POLICY "Enable All Access" ON file_permissions FOR ALL USING (true) WITH CHECK (true);

-- 4. Enable Audit Log Access
ALTER TABLE permission_audit_log DISABLE ROW LEVEL SECURITY;
