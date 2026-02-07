-- DROP and RECREATE to confirm schema
DROP TABLE IF EXISTS system_status;

CREATE TABLE system_status (
    id INT PRIMARY KEY,
    service_online BOOLEAN DEFAULT FALSE,
    last_seen TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

INSERT INTO system_status (id, service_online, last_seen)
VALUES (1, true, NOW())
ON CONFLICT (id) DO UPDATE 
SET service_online = true, last_seen = NOW();

-- Grant access (just in case)
ALTER TABLE system_status ENABLE ROW LEVEL SECURITY;
CREATE POLICY "Public Read Status" ON system_status FOR SELECT TO anon, authenticated USING (true);
CREATE POLICY "Service Update Status" ON system_status FOR UPDATE TO anon, authenticated USING (true);
CREATE POLICY "Service Insert Status" ON system_status FOR INSERT TO anon, authenticated WITH CHECK (true);
