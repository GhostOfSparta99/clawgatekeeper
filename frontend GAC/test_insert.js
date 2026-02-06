
import { createClient } from '@supabase/supabase-js';

const supabaseUrl = 'https://zkpgilfposjkshvtteto.supabase.co';
const supabaseKey = 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InprcGdpbGZwb3Nqa3NodnR0ZXRvIiwicm9sZSI6ImFub24iLCJpYXQiOjE3Njg1NDUwMDIsImV4cCI6MjA4NDEyMTAwMn0.Mu0WplU6rdWPRildgowelbDGCaiy50VVaBqjfFRl17w';
const client = createClient(supabaseUrl, supabaseKey);

async function insertDetails() {
    const folderPath = "C:/Users/Adi/Documents/Visual Studio 18/Templates";
    const parentPath = "C:/Users/Adi/Documents/Visual Studio 18";

    console.log(`Attempting to insert: "${folderPath}"`);

    const { data, error } = await client
        .from('file_permissions')
        .insert([
            {
                file_path: folderPath,
                parent_path: parentPath,
                name: "Templates",
                is_directory: true,
                accessible: true,
                to_be_deleted: false,
                file_size: "0",
                file_extension: ""
            }
        ])
        .select();

    if (error) {
        console.error("Insert Error:", error);
    } else {
        console.log("Insert Success:", data);
    }
}

insertDetails();
