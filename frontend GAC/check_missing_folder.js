
import { createClient } from '@supabase/supabase-js';

const supabaseUrl = 'https://zkpgilfposjkshvtteto.supabase.co';
const supabaseKey = 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InprcGdpbGZwb3Nqa3NodnR0ZXRvIiwicm9sZSI6ImFub24iLCJpYXQiOjE3Njg1NDUwMDIsImV4cCI6MjA4NDEyMTAwMn0.Mu0WplU6rdWPRildgowelbDGCaiy50VVaBqjfFRl17w';
const client = createClient(supabaseUrl, supabaseKey);

async function checkPath() {
    // Check for the potentially missing intermediate folder
    const pathToCheck = "C:/Users/Adi/Documents/Visual Studio 18/Templates";
    console.log(`Checking for path: "${pathToCheck}"`);

    const { data, error } = await client
        .from('file_permissions')
        .select('*')
        .eq('file_path', pathToCheck);

    if (error) {
        console.error("Error:", error);
    } else {
        console.log("Found:", data.length);
        if (data.length > 0) {
            console.log("Item:", data[0]);
        } else {
            console.log("Item NOT FOUND in DB.");
        }
    }
}

checkPath();
