
import { createClient } from '@supabase/supabase-js';

const supabaseUrl = 'https://hyqmdnzkxhkvzynzqwug.supabase.co';
const supabaseKey = 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Imh5cW1kbnpreGhrdnp5bnpxd3VnIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzAyODM0MjksImV4cCI6MjA4NTg1OTQyOX0.Oo-jJIivU0YHRbloTJlpl_cwQ9CkrMq6tV9YQUu8F5w';

const supabase = createClient(supabaseUrl, supabaseKey);

async function purgeRobust() {
    console.log("Starting ROBUST database purge...");

    let totalDeleted = 0;

    while (true) {
        // Fetch a batch of IDs (all rows)
        const { data: rows, error: fetchError } = await supabase
            .from('file_permissions')
            .select('id')
            .limit(1000); // 1000 at a time

        if (fetchError) {
            console.error("Error fetching rows:", fetchError);
            break;
        }

        if (!rows || rows.length === 0) {
            console.log("No more rows to delete.");
            break;
        }

        const ids = rows.map(r => r.id);
        console.log(`Found ${ids.length} rows. Deleting...`);

        const { error: deleteError } = await supabase
            .from('file_permissions')
            .delete()
            .in('id', ids);

        if (deleteError) {
            console.error("Error deleting batch:", deleteError);
            break;
        }

        totalDeleted += ids.length;
        console.log(`Deleted batch. Total so far: ${totalDeleted}`);
    }

    // Reset System Status
    console.log("Resetting system status...");
    await supabase.from('system_status').upsert({
        id: 1,
        service_online: false,
        is_locked: false,
        last_heartbeat: new Date().toISOString()
    });

    console.log(`Purge complete. ${totalDeleted} files removed.`);
}

purgeRobust();
