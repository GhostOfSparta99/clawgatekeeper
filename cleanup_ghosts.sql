-- Remove invalid folders that may have been synced
DELETE FROM file_permissions WHERE file_path LIKE '%My Music%';
DELETE FROM file_permissions WHERE file_path LIKE '%My Pictures%';
DELETE FROM file_permissions WHERE file_path LIKE '%My Videos%';
DELETE FROM file_permissions WHERE file_path LIKE '%Visual Studio 18%';
DELETE FROM file_permissions WHERE file_path LIKE '%desktop.ini%';
