import 'package:supabase_flutter/supabase_flutter.dart';
import '../models/file_item.dart';

class SupabaseService {
  final SupabaseClient _client = Supabase.instance.client;

  Future<List<FileItem>> fetchRootItems(String rootPath) async {
    try {
      final response = await _client
          .from('file_permissions')
          .select()
          .eq('parent_path', rootPath)
          .order('is_directory', ascending: false)
          .order('name');

      return (response as List)
          .map((item) => FileItem.fromJson(item as Map<String, dynamic>))
          .toList();
    } catch (e) {
      print('Error fetching root items: $e');
      return [];
    }
  }

  Future<List<FileItem>> fetchFolderContents(String folderPath) async {
    try {
      final response = await _client
          .from('file_permissions')
          .select()
          .eq('parent_path', folderPath)
          .order('is_directory', ascending: false)
          .order('name');

      return (response as List)
          .map((item) => FileItem.fromJson(item as Map<String, dynamic>))
          .toList();
    } catch (e) {
      print('Error fetching folder contents: $e');
      return [];
    }
  }

  Future<bool> toggleAccessibility(int fileId, bool newStatus) async {
    try {
      await _client.rpc('toggle_lock', params: {
        'file_id': fileId,
        'new_status': newStatus,
      });
      return true;
    } catch (e) {
      print('Error in RPC, trying direct update: $e');
      try {
        await _client
            .from('file_permissions')
            .update({'accessible': newStatus})
            .eq('id', fileId);
        return true;
      } catch (e2) {
        print('Error updating accessibility: $e2');
        return false;
      }
    }
  }

  Future<List<FileItem>> searchFiles(String searchTerm) async {
    try {
      final response = await _client.rpc('search_files', params: {
        'search_term': searchTerm,
      });

      return (response as List)
          .map((item) => FileItem.fromJson(item as Map<String, dynamic>))
          .toList();
    } catch (e) {
      print('Error searching files: $e');
      return [];
    }
  }

  Future<bool> checkServiceStatus() async {
    try {
      final response = await _client
          .from('system_status')
          .select('service_online')
          .eq('id', 1)
          .single();

      return response['service_online'] as bool? ?? false;
    } catch (e) {
      print('Error checking service status: $e');
      return false;
    }
  }

  Stream<List<Map<String, dynamic>>> subscribeToFileChanges() {
    return _client
        .from('file_permissions')
        .stream(primaryKey: ['id'])
        .order('name');
  }

  Stream<List<Map<String, dynamic>>> subscribeToSystemStatus() {
    return _client
        .from('system_status')
        .stream(primaryKey: ['id']);
  }
}
