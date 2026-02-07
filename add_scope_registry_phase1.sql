-- Register Phase 1 Scopes (Desktop, Pictures, Videos)
-- Note: Requires 'monitored_scopes' table to exist (from previous steps)

INSERT INTO monitored_scopes (folder_name, root_path)
VALUES 
    ('Desktop', 'C:/Users/Adi/Desktop'),
    ('Pictures', 'C:/Users/Adi/Pictures'),
    ('Videos', 'C:/Users/Adi/Videos')
ON CONFLICT (root_path) DO NOTHING;

-- Optional: Verify scopes
SELECT * FROM monitored_scopes;
