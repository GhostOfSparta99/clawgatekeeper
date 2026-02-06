
import { createClient } from '@supabase/supabase-js';

const supabaseUrl = 'https://zkpgilfposjkshvtteto.supabase.co';
const supabaseKey = 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InprcGdpbGZwb3Nqa3NodnR0ZXRvIiwicm9sZSI6ImFub24iLCJpYXQiOjE3Njg1NDUwMDIsImV4cCI6MjA4NDEyMTAwMn0.Mu0WplU6rdWPRildgowelbDGCaiy50VVaBqjfFRl17w';

const supabase = createClient(supabaseUrl, supabaseKey);

async function debugDB() {
    console.log("Fetching first 20 items from DB...");

    const { data, error } = await supabase
        .from('file_permissions')
        .select('id, name, is_directory, file_path, parent_path')
        .limit(20);

    if (error) {
        console.error("Error:", error);
    } else {
        console.log(JSON.stringify(data, null, 2));
    }
}

debugDB();
