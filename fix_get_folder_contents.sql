-- FIXED get_folder_contents function
-- Run this in Supabase SQL Editor to fix subfolder visibility

CREATE OR REPLACE FUNCTION get_folder_contents(folder_path TEXT)
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
    WHERE 
        -- Standardize slashes for comparison to handle slight mismatches
        REPLACE(fp.parent_path, '\', '/') = REPLACE(folder_path, '\', '/')
        -- Fallback: Try case-insensitive comparison if exact match fails
        OR LOWER(REPLACE(fp.parent_path, '\', '/')) = LOWER(REPLACE(folder_path, '\', '/'))
    AND fp.to_be_deleted = FALSE
    ORDER BY fp.is_directory DESC, fp.name ASC;
END;
$$ LANGUAGE plpgsql;
