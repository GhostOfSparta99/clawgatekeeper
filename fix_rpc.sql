-- ===================================================
-- GATEKEEPER - ULTIMATE LOCK FIX (RPC)
-- ===================================================

-- 1. Create a secure function that bypasses all RLS checks
CREATE OR REPLACE FUNCTION toggle_lock(file_id bigint, new_status boolean)
RETURNS void
LANGUAGE plpgsql
SECURITY DEFINER -- <--- This is the magic. It runs as Admin.
AS $$
BEGIN
  UPDATE file_permissions
  SET accessible = new_status
  WHERE id = file_id;
END;
$$;

-- 2. Allow the public (frontend) to verify/run this function
GRANT EXECUTE ON FUNCTION toggle_lock(bigint, boolean) TO anon;
GRANT EXECUTE ON FUNCTION toggle_lock(bigint, boolean) TO authenticated;
GRANT EXECUTE ON FUNCTION toggle_lock(bigint, boolean) TO service_role;

-- 3. Just in case, re-verify RLS is off or permissions are wide open
ALTER TABLE file_permissions DISABLE ROW LEVEL SECURITY;
GRANT ALL ON TABLE file_permissions TO anon;
