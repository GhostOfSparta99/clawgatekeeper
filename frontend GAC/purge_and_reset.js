
import { createClient } from '@supabase/supabase-js';

const supabaseUrl = 'https://hyqmdnzkxhkvzynzqwug.supabase.co';
const supabaseKey = 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Imh5cW1kbnpreGhrdnp5bnpxd3VnIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzAyODM0MjksImV4cCI6MjA4NTg1OTQyOX0.Oo-jJIivU0YHRbloTJlpl_cwQ9CkrMq6tV9YQUu8F5w';

const supabase = createClient(supabaseUrl, supabaseKey);

async function purgeAndReset() {
    console.log("Starting database purge...");

    // 1. Delete all file permissions
    const { error: deleteError } = await supabase
        .from('file_permissions')
        .delete()
        .neq('id', 0); // Delete all rows

    if (deleteError) {
        console.error("Error purging file_permissions:", deleteError);
    } else {
        console.log("Successfully purged file_permissions table.");
    }

    // 2. Reset System Status
    const { error: statusError } = await supabase
        .from('system_status')
        .upsert({ 
            id: 1, 
            service_online: false, // Will be set to true when C++ starts
            is_locked: false,
            last_heartbeat: new Date().toISOString()
        });

    if (statusError) {
        console.error("Error resetting system_status:", statusError);
    } else {
        console.log("Successfully reset system_status.");
    }

    console.log("Database reset complete.");
}

purgeAndReset();
