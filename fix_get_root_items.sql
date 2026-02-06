-- FIXED get_root_items function
-- Run this in your Supabase SQL Editor to fix the empty file explorer

CREATE OR REPLACE FUNCTION get_root_items()
RETURNS TABLE (
    id BIGINT,
    file_path TEXT,
    name TEXT,
    is_directory BOOLEAN,
    accessible BOOLEAN,
    file_size TEXT,
    file_extension TEXT,
    last_modified TIMESTAMP WITH TIME ZONE
) AS $$
BEGIN
    -- Return items whose parent_path is NOT present in the file_path column
    -- These are the "roots" of our scanned trees (e.g. items directly in Documents)
    -- This works regardless of the actual folder name (Documents, MyDocuments, etc)
    RETURN QUERY
    SELECT 
        fp.id,
        fp.file_path,
        fp.name,
        fp.is_directory,
        fp.accessible,
        fp.file_size,
        fp.file_extension,
        fp.last_modified
    FROM file_permissions fp
    WHERE NOT EXISTS (
        SELECT 1 FROM file_permissions parent 
        WHERE parent.file_path = fp.parent_path
    )
    AND fp.to_be_deleted = FALSE
    ORDER BY fp.is_directory DESC, fp.name ASC;
END;
$$ LANGUAGE plpgsql;
