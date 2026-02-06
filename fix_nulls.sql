-- ===================================================
-- GATEKEEPER - FIX MISSING FILES (NULL PARENT)
-- ===================================================

-- 1. Immediate Fix: Update any orphaned files to belong to the main Documents folder
-- This targets the specific files seen in your screenshot (hiiiiiiiii.txt, etc)
UPDATE file_permissions
SET parent_path = 'C:/Users/Adi/Documents'
WHERE parent_path IS NULL 
  AND file_path LIKE 'C:/Users/Adi/Documents/%';

-- 2. Clean up any weird entries (optional, just to be safe)
-- DELETE FROM file_permissions WHERE file_path IS NULL;

-- 3. Optimization: Ensure indexes exist for faster searching
CREATE INDEX IF NOT EXISTS idx_parent_path ON file_permissions(parent_path);
CREATE INDEX IF NOT EXISTS idx_file_path ON file_permissions(file_path);
