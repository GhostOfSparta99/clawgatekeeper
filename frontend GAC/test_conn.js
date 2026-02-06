
import { createClient } from '@supabase/supabase-js';

const supabaseUrl = 'https://zkpgilfposjkshvtteto.supabase.co';
const supabaseKey = 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InprcGdpbGZwb3Nqa3NodnR0ZXRvIiwicm9sZSI6ImFub24iLCJpYXQiOjE3Njg1NDUwMDIsImV4cCI6MjA4NDEyMTAwMn0.Mu0WplU6rdWPRildgowelbDGCaiy50VVaBqjfFRl17w';

const supabase = createClient(supabaseUrl, supabaseKey);

async function testConnection() {
    console.log("Testing Supabase WRITE Connection...");
    console.log("URL:", supabaseUrl);

    // Try to insert a row
    const { data, error } = await supabase
        .from('file_permissions')
        .insert([
            {
                file_path: "C:/Test/test_conn_write.txt",
                name: "test_conn_write.txt",
                accessible: true,
                parent_path: "C:/Test"
            }
        ])
        .select();

    if (error) {
        console.error("WRITE FAILED:");
        console.error(error);
    } else {
        console.log("WRITE SUCCESS:");
        console.log(data);

        // Cleanup
        await supabase.from('file_permissions').delete().eq('file_path', "C:/Test/test_conn_write.txt");
    }
}

testConnection();
