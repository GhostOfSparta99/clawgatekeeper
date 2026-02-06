
import { createClient } from '@supabase/supabase-js';

const supabaseUrl = 'https://zkpgilfposjkshvtteto.supabase.co';
const supabaseKey = 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InprcGdpbGZwb3Nqa3NodnR0ZXRvIiwicm9sZSI6ImFub24iLCJpYXQiOjE3Njg1NDUwMDIsImV4cCI6MjA4NDEyMTAwMn0.Mu0WplU6rdWPRildgowelbDGCaiy50VVaBqjfFRl17w';

const supabase = createClient(supabaseUrl, supabaseKey);

async function testRootRPC() {
    console.log("Calling get_root_items...");

    const { data, error } = await supabase.rpc('get_root_items');

    if (error) {
        console.error("RPC Error:", error);
    } else {
        console.log("RPC Result (Count):", data ? data.length : 0);
        // Filter to show just directories to see if they are returned
        const dirs = data.filter(i => i.is_directory);
        console.log("Directories found:", dirs.length);
        console.log("First 5 Dirs:", JSON.stringify(dirs.slice(0, 5), null, 2));
    }
}

testRootRPC();
