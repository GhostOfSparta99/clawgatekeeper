-- SQL Script to Remove Ghost Files from Database
-- Run this in your Supabase SQL Editor to clean up ghost file entries

-- Delete the ghost files that don't actually exist
DELETE FROM file_permissions 
WHERE name IN ('My Music', 'My Pictures', 'My Videos', 'desktop.ini')
  AND file_path LIKE 'C:/Users/Adi/Documents/%';

-- Verify the deletion
SELECT COUNT(*) as remaining_ghost_files
FROM file_permissions 
WHERE name IN ('My Music', 'My Pictures', 'My Videos', 'desktop.ini');
