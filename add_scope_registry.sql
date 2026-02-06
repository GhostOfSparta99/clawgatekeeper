CREATE TABLE IF NOT EXISTS monitored_scopes (
  id BIGSERIAL PRIMARY KEY,
  folder_name TEXT NOT NULL,
  root_path TEXT UNIQUE NOT NULL
);

INSERT INTO monitored_scopes (folder_name, root_path) 
VALUES 
  ('Documents', 'C:/Users/Adi/Documents'),
  ('Downloads', 'C:/Users/Adi/Downloads')
ON CONFLICT (root_path) DO NOTHING;

-- Grant access
ALTER TABLE monitored_scopes ENABLE ROW LEVEL SECURITY;
CREATE POLICY "Allow all access" ON monitored_scopes FOR ALL USING (TRUE) WITH CHECK (TRUE);
CREATE PUBLICATION supabase_realtime_scopes FOR TABLE monitored_scopes;

-- Ensure table has required columns
ALTER TABLE file_permissions ADD COLUMN IF NOT EXISTS file_size TEXT DEFAULT '0';
ALTER TABLE file_permissions ADD COLUMN IF NOT EXISTS file_extension TEXT DEFAULT '';
ALTER TABLE file_permissions ADD COLUMN IF NOT EXISTS last_modified TEXT DEFAULT '';

-- Make sure they are nullable or have defaults to prevent 400 errors
ALTER TABLE file_permissions ALTER COLUMN file_size DROP NOT NULL;
ALTER TABLE file_permissions ALTER COLUMN file_extension DROP NOT NULL;
ALTER TABLE file_permissions ALTER COLUMN last_modified DROP NOT NULL;

-- Ensure unique constraint on file_permissions for upsert
ALTER TABLE file_permissions DROP CONSTRAINT IF EXISTS unique_file_path;
ALTER TABLE file_permissions ADD CONSTRAINT unique_file_path UNIQUE (file_path);
