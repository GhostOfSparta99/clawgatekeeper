-- Add the flag column for remote deletion
alter table file_locks 
add column if not exists to_be_deleted boolean default false;
