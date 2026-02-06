-- ===================================================
-- GATEKEEPER - FIX RLS POLICY (BLOCKING DELETE)
-- ===================================================

-- The error "new row violates row-level security policy for table permission_audit_log"
-- means the C++ service (anon user) cannot create audit logs when it deletes files.

-- 1. Enable INSERT access for 'anon' and 'authenticated' roles on the audit log table
ALTER TABLE permission_audit_log ENABLE ROW LEVEL SECURITY;

CREATE POLICY "Allow Service to Insert Logs" 
ON permission_audit_log
FOR INSERT 
TO anon, authenticated 
WITH CHECK (true);

-- 2. Also ensure file_permissions allows deletion
CREATE POLICY "Allow Service to Delete Files" 
ON file_permissions
FOR DELETE
TO anon, authenticated
USING (true);

-- 3. (Optional) If you just want to allow everything during dev:
-- ALTER TABLE permission_audit_log DISABLE ROW LEVEL SECURITY;
