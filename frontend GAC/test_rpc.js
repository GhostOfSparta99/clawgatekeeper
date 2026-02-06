
import { createClient } from '@supabase/supabase-js';

const supabaseUrl = 'https://zkpgilfposjkshvtteto.supabase.co';
const supabaseKey = 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InprcGdpbGZwb3Nqa3NodnR0ZXRvIiwicm9sZSI6ImFub24iLCJpYXQiOjE3Njg1NDUwMDIsImV4cCI6MjA4NDEyMTAwMn0.Mu0WplU6rdWPRildgowelbDGCaiy50VVaBqjfFRl17w';

const supabase = createClient(supabaseUrl, supabaseKey);

async function testRPC() {
    const folderPath = "C:/Users/Adi/Documents/Folder";
    console.log(`Calling get_folder_contents with: "${folderPath}"`);

    const { data, error } = await supabase.rpc('get_folder_contents', {
        folder_path: folderPath
    });

    if (error) {
        console.error("RPC Error:", error);
    } else {
        console.log("RPC Result (Count):", data ? data.length : 0);
        console.log("Items:", JSON.stringify(data, null, 2));
    }
}

testRPC();
