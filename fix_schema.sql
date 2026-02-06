-- Run this script to fix the columns and constraints
-- It avoids creating policies that already exist

-- 1. Ensure columns exist
ALTER TABLE file_permissions ADD COLUMN IF NOT EXISTS file_size TEXT DEFAULT '0';
ALTER TABLE file_permissions ADD COLUMN IF NOT EXISTS file_extension TEXT DEFAULT '';
ALTER TABLE file_permissions ADD COLUMN IF NOT EXISTS last_modified TEXT DEFAULT '';

-- 2. Relax constraints (nullable) to prevent 400 errors
ALTER TABLE file_permissions ALTER COLUMN file_size DROP NOT NULL;
ALTER TABLE file_permissions ALTER COLUMN file_extension DROP NOT NULL;
ALTER TABLE file_permissions ALTER COLUMN last_modified DROP NOT NULL;

-- 3. Fix Unique Constraint for Upsert
ALTER TABLE file_permissions DROP CONSTRAINT IF EXISTS unique_file_path;
ALTER TABLE file_permissions ADD CONSTRAINT unique_file_path UNIQUE (file_path);

-- 4. Verify scopes table exists (idempotent)
CREATE TABLE IF NOT EXISTS monitored_scopes (
  id BIGSERIAL PRIMARY KEY,
  folder_name TEXT NOT NULL,
  root_path TEXT UNIQUE NOT NULL
);

-- 5. Insert default scopes if missing
INSERT INTO monitored_scopes (folder_name, root_path) 
VALUES 
  ('Documents', 'C:/Users/Adi/Documents'),
  ('Downloads', 'C:/Users/Adi/Downloads')
ON CONFLICT (root_path) DO NOTHING;

-- 6. Important for Realtime updates on non-PK columns
ALTER TABLE file_permissions REPLICA IDENTITY FULL;
