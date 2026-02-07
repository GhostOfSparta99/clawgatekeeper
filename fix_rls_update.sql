-- Allow Frontend (anon) to UPDATE file_permissions
-- This is needed for the Lock/Unlock toggle
CREATE POLICY "Allow Frontend Update"
ON file_permissions
FOR UPDATE
TO anon
USING (true)
WITH CHECK (true);

-- Ensure the table has RLS enabled
ALTER TABLE file_permissions ENABLE ROW LEVEL SECURITY;
