
import { createClient } from '@supabase/supabase-js';

const supabaseUrl = 'https://hyqmdnzkxhkvzynzqwug.supabase.co';
const supabaseKey = 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Imh5cW1kbnpreGhrdnp5bnpxd3VnIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzAyODM0MjksImV4cCI6MjA4NTg1OTQyOX0.Oo-jJIivU0YHRbloTJlpl_cwQ9CkrMq6tV9YQUu8F5w';

const supabase = createClient(supabaseUrl, supabaseKey);

async function setOnline() {
    console.log("Forcing system status to ONLINE...");

    const { error } = await supabase.from('system_status').upsert({
        id: 1,
        service_online: true,
        last_heartbeat: new Date().toISOString()
    });

    if (error) {
        console.error("Error:", error);
    } else {
        console.log("System status set to ONLINE.");
    }
}

setOnline();
