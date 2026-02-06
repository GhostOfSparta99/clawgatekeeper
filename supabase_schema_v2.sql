-- ===================================================
-- GATEKEEPER - HIERARCHICAL FILE SYSTEM SCHEMA
-- ===================================================
-- Supports folder navigation, file discovery, and permissions
-- ===================================================

-- 1. CLEANUP
-- ===================================================
DROP PUBLICATION IF EXISTS supabase_realtime; 
DROP TABLE IF EXISTS file_permissions CASCADE;
DROP TABLE IF EXISTS system_status CASCADE;
DROP TABLE IF EXISTS permission_audit_log CASCADE;

-- ===================================================
-- 2. SYSTEM STATUS TABLE
-- ===================================================
CREATE TABLE system_status (
  id BIGINT PRIMARY KEY DEFAULT 1,
  is_locked BOOLEAN DEFAULT FALSE,
  service_online BOOLEAN DEFAULT FALSE,
  last_heartbeat TIMESTAMP WITH TIME ZONE DEFAULT TIMEZONE('utc'::text, NOW()),
  updated_at TIMESTAMP WITH TIME ZONE DEFAULT TIMEZONE('utc'::text, NOW()),
  CONSTRAINT single_row CHECK (id = 1)
);

INSERT INTO system_status (id, is_locked, service_online) 
VALUES (1, FALSE, FALSE)
ON CONFLICT (id) DO NOTHING;

ALTER TABLE system_status ENABLE ROW LEVEL SECURITY;
CREATE POLICY "Allow all access to system status" 
ON system_status FOR ALL 
USING (TRUE) WITH CHECK (TRUE);

-- ===================================================
-- 3. FILE PERMISSIONS TABLE (Hierarchical Structure)
-- ===================================================
CREATE TABLE file_permissions (
  id BIGSERIAL PRIMARY KEY,
  file_path TEXT UNIQUE NOT NULL,           -- Full path: "C:/Users/Name/Documents/folder/file.txt"
  name TEXT NOT NULL,                        -- Just the filename: "file.txt"
  parent_path TEXT,                          -- Parent directory path
  is_directory BOOLEAN DEFAULT FALSE,
  accessible BOOLEAN DEFAULT TRUE,
  access_code TEXT DEFAULT NULL,
  file_size TEXT DEFAULT '0',                -- Stored as string to handle large files
  file_extension TEXT DEFAULT '',
  last_modified TIMESTAMP WITH TIME ZONE,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT TIMEZONE('utc'::text, NOW()),
  updated_at TIMESTAMP WITH TIME ZONE DEFAULT TIMEZONE('utc'::text, NOW()),
  to_be_deleted BOOLEAN DEFAULT FALSE
);

-- Critical indexes for hierarchical queries
CREATE INDEX idx_file_path ON file_permissions(file_path);
CREATE INDEX idx_parent_path ON file_permissions(parent_path);
CREATE INDEX idx_is_directory ON file_permissions(is_directory);
CREATE INDEX idx_accessible ON file_permissions(accessible);
CREATE INDEX idx_name ON file_permissions(name);
CREATE INDEX idx_to_be_deleted ON file_permissions(to_be_deleted);

-- Composite index for folder contents query
CREATE INDEX idx_parent_accessible ON file_permissions(parent_path, accessible);

ALTER TABLE file_permissions ENABLE ROW LEVEL SECURITY;
CREATE POLICY "Allow all access to file permissions" 
ON file_permissions FOR ALL 
USING (TRUE) WITH CHECK (TRUE);

-- ===================================================
-- 4. AUDIT LOG TABLE
-- ===================================================
CREATE TABLE permission_audit_log (
  id BIGSERIAL PRIMARY KEY,
  file_path TEXT NOT NULL,
  action TEXT NOT NULL,
  previous_state BOOLEAN,
  new_state BOOLEAN,
  changed_by TEXT DEFAULT NULL,
  changed_at TIMESTAMP WITH TIME ZONE DEFAULT TIMEZONE('utc'::text, NOW())
);

CREATE INDEX idx_audit_file_path ON permission_audit_log(file_path);
CREATE INDEX idx_audit_changed_at ON permission_audit_log(changed_at DESC);

ALTER TABLE permission_audit_log ENABLE ROW LEVEL SECURITY;
CREATE POLICY "Allow all access to audit log" 
ON permission_audit_log FOR ALL 
USING (TRUE) WITH CHECK (TRUE);

-- ===================================================
-- 5. AUTO-UPDATE TRIGGERS
-- ===================================================

CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = TIMEZONE('utc'::text, NOW());
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER update_file_permissions_updated_at
    BEFORE UPDATE ON file_permissions
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_system_status_updated_at
    BEFORE UPDATE ON system_status
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- ===================================================
-- 6. AUDIT LOGGING TRIGGER
-- ===================================================

CREATE OR REPLACE FUNCTION log_permission_change()
RETURNS TRIGGER AS $$
BEGIN
    IF (TG_OP = 'UPDATE' AND OLD.accessible != NEW.accessible) THEN
        INSERT INTO permission_audit_log (file_path, action, previous_state, new_state)
        VALUES (
            NEW.file_path,
            CASE WHEN NEW.accessible THEN 'unlocked' ELSE 'locked' END,
            OLD.accessible,
            NEW.accessible
        );
    ELSIF (TG_OP = 'INSERT') THEN
        INSERT INTO permission_audit_log (file_path, action, new_state)
        VALUES (NEW.file_path, 'created', NEW.accessible);
    ELSIF (TG_OP = 'DELETE') THEN
        INSERT INTO permission_audit_log (file_path, action, previous_state)
        VALUES (OLD.file_path, 'deleted', OLD.accessible);
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER audit_permission_changes
    AFTER INSERT OR UPDATE OR DELETE ON file_permissions
    FOR EACH ROW
    EXECUTE FUNCTION log_permission_change();

-- ===================================================
-- 7. HELPER FUNCTIONS FOR FRONTEND
-- ===================================================

-- Get all items in a specific folder
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
    WHERE fp.parent_path = folder_path
    AND fp.to_be_deleted = FALSE
    ORDER BY fp.is_directory DESC, fp.name ASC;
END;
$$ LANGUAGE plpgsql;

-- Get root level items (Documents folder)
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
    -- These are the "roots" of our scanned trees
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

-- Count items in a folder
CREATE OR REPLACE FUNCTION count_folder_items(folder_path TEXT)
RETURNS INTEGER AS $$
DECLARE
    item_count INTEGER;
BEGIN
    SELECT COUNT(*) INTO item_count
    FROM file_permissions
    WHERE parent_path = folder_path
    AND to_be_deleted = FALSE;
    
    RETURN item_count;
END;
$$ LANGUAGE plpgsql;

-- Toggle accessibility for entire folder tree
CREATE OR REPLACE FUNCTION toggle_folder_tree(folder_path TEXT, should_lock BOOLEAN)
RETURNS INTEGER AS $$
DECLARE
    rows_affected INTEGER;
BEGIN
    UPDATE file_permissions
    SET accessible = NOT should_lock
    WHERE file_path LIKE folder_path || '%'
    AND to_be_deleted = FALSE;
    
    GET DIAGNOSTICS rows_affected = ROW_COUNT;
    RETURN rows_affected;
END;
$$ LANGUAGE plpgsql;

-- Clean up marked files
CREATE OR REPLACE FUNCTION cleanup_deleted_files()
RETURNS INTEGER AS $$
DECLARE
    rows_deleted INTEGER;
BEGIN
    DELETE FROM file_permissions
    WHERE to_be_deleted = TRUE;
    
    GET DIAGNOSTICS rows_deleted = ROW_COUNT;
    RETURN rows_deleted;
END;
$$ LANGUAGE plpgsql;

-- Search files by name
CREATE OR REPLACE FUNCTION search_files(search_term TEXT)
RETURNS TABLE (
    id BIGINT,
    file_path TEXT,
    name TEXT,
    parent_path TEXT,
    is_directory BOOLEAN,
    accessible BOOLEAN
) AS $$
BEGIN
    RETURN QUERY
    SELECT 
        fp.id,
        fp.file_path,
        fp.name,
        fp.parent_path,
        fp.is_directory,
        fp.accessible
    FROM file_permissions fp
    WHERE fp.name ILIKE '%' || search_term || '%'
    AND fp.to_be_deleted = FALSE
    ORDER BY fp.is_directory DESC, fp.name ASC
    LIMIT 100;
END;
$$ LANGUAGE plpgsql;

-- ===================================================
-- 8. VIEWS FOR FRONTEND
-- ===================================================

-- View: All folders
CREATE OR REPLACE VIEW folders_only AS
SELECT 
    id,
    file_path,
    name,
    parent_path,
    accessible,
    last_modified
FROM file_permissions
WHERE is_directory = TRUE
AND to_be_deleted = FALSE
ORDER BY file_path;

-- View: All locked items
CREATE OR REPLACE VIEW locked_items AS
SELECT 
    id,
    file_path,
    name,
    is_directory,
    parent_path,
    updated_at
FROM file_permissions
WHERE accessible = FALSE
AND to_be_deleted = FALSE
ORDER BY updated_at DESC;

-- View: Recent changes
CREATE OR REPLACE VIEW recent_changes AS
SELECT 
    file_path,
    action,
    previous_state,
    new_state,
    changed_at
FROM permission_audit_log
ORDER BY changed_at DESC
LIMIT 100;

-- View: File statistics
CREATE OR REPLACE VIEW file_statistics AS
SELECT 
    COUNT(*) as total_files,
    COUNT(CASE WHEN is_directory = TRUE THEN 1 END) as total_folders,
    COUNT(CASE WHEN accessible = FALSE THEN 1 END) as locked_items,
    COUNT(CASE WHEN accessible = TRUE THEN 1 END) as accessible_items
FROM file_permissions
WHERE to_be_deleted = FALSE;

-- ===================================================
-- 9. ENABLE REALTIME
-- ===================================================

CREATE PUBLICATION supabase_realtime FOR TABLE 
    system_status, 
    file_permissions,
    permission_audit_log;

ALTER TABLE file_permissions REPLICA IDENTITY FULL;
ALTER TABLE system_status REPLICA IDENTITY FULL;
ALTER TABLE permission_audit_log REPLICA IDENTITY FULL;
