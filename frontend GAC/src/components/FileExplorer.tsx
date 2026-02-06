// FileExplorer.tsx - Hierarchical file browser for GateKeeper
import React, { useState, useEffect } from 'react';
// import { createClient } from '@supabase/supabase-js';
import { useTheme } from '../contexts/ThemeContext';
import {
  Folder,
  File,
  Lock,
  Unlock,
  ChevronRight,
  ChevronDown,
  Home,
  Search
} from 'lucide-react';

import { supabase } from '../lib/supabase';

interface FileItem {
  id: number;
  file_path: string;
  name: string;
  parent_path: string;
  is_directory: boolean;
  accessible: boolean;
  file_size?: string;
  file_extension?: string;
  last_modified?: string;
}

interface FolderState {
  [key: string]: {
    isOpen: boolean;
    children: FileItem[];
    loading: boolean;
  };
}

interface FileExplorerProps {
  rootPath: string; // "C:/Users/Adi/Documents"
}

export default function FileExplorer({ rootPath }: FileExplorerProps) {
  const [lastSynced, setLastSynced] = useState<Date>(new Date());
  const { theme } = useTheme();
  const [rootItems, setRootItems] = useState<FileItem[]>([]);
  const [folderStates, setFolderStates] = useState<FolderState>({});
  // Initialize currentPath based on the rootPath name
  const rootName = rootPath.split('/').pop() || 'Root';
  const [currentPath, setCurrentPath] = useState<string[]>([rootName]);

  const [searchTerm, setSearchTerm] = useState('');
  const [searchResults, setSearchResults] = useState<FileItem[]>([]);
  const [loading, setLoading] = useState(true);
  const [serviceOnline, setServiceOnline] = useState(false);

  // Styling based on theme
  const containerClass = theme === 'stealth'
    ? 'bg-gray-900 border-gray-700 text-gray-200'
    : 'bg-white border-gray-200 text-gray-900';

  const hoverClass = theme === 'stealth' ? 'hover:bg-gray-800' : 'hover:bg-gray-100';
  const inputClass = theme === 'stealth'
    ? 'bg-gray-800 border-gray-700 text-white placeholder-gray-400'
    : 'bg-white border-gray-300 text-gray-900 placeholder-gray-500';

  // Load root items on mount
  useEffect(() => {
    loadRootItems();
    checkServiceStatus();
    const cleanup = subscribeToChanges();
    return cleanup;
  }, [rootPath]); // Re-run when scope changes

  // Check service status
  const checkServiceStatus = async () => {
    try {
      const { data } = await supabase
        .from('system_status')
        .select('service_online')
        .eq('id', 1)
        .single();

      setServiceOnline(data?.service_online || false);
    } catch (error) {
      console.error('Error checking service status:', error);
    }
  };

  // Subscribe to realtime changes
  const subscribeToChanges = () => {
    const channel = supabase
      .channel('file_changes')
      .on(
        'postgres_changes',
        { event: '*', schema: 'public', table: 'file_permissions' },
        (payload) => {
          // Optimization: Check if the changed file belongs to our scope
          const newFile = payload.new as FileItem;
          // If available, check if path starts with rootPath
          // But payload.new might be incomplete or normalized differently
          // Safest is to just refresh for now
          loadRootItems(true); // Silent refresh
        }
      )
      .on(
        'postgres_changes',
        { event: '*', schema: 'public', table: 'system_status' },
        () => {
          checkServiceStatus();
        }
      )
      .subscribe();

    return () => {
      supabase.removeChannel(channel);
    };
  };

  // Load root level items using the SQL function
  const loadRootItems = async (silent = false) => {
    if (!silent) setLoading(true);
    try {
      // Instead of generic get_root_items via RPC which might be hardcoded to Documents
      // We do a direct select for flexibility, OR update the RPC. 
      // Direct select is safer for this dynamic scope feature without modifying SQL functions constantly.

      const { data, error } = await supabase
        .from('file_permissions')
        .select('*')
        .eq('parent_path', rootPath) // Key change: Filter by rootPath
        .order('is_directory', { ascending: false })
        .order('name');

      if (error) {
        console.error('Error loading root items:', error);
      } else {
        setRootItems(data || []);
      }
    } catch (error) {
      console.error('Exception loading root items:', error);
    }
    if (!silent) setLoading(false);
  };

  // Load contents of a specific folder
  const loadFolderContents = async (folderPath: string, silent = false) => {
    if (!silent) {
      setFolderStates(prev => ({
        ...prev,
        [folderPath]: {
          isOpen: true,
          children: prev[folderPath]?.children || [],
          loading: true
        }
      }));
    }

    try {
      // Direct Select
      const { data, error } = await supabase
        .from('file_permissions')
        .select('*')
        .eq('parent_path', folderPath)
        .order('is_directory', { ascending: false })
        .order('name');

      if (error) {
        console.error('Error loading folder:', error);
      } else {
        setFolderStates(prev => ({
          ...prev,
          [folderPath]: {
            isOpen: true,
            children: data || [],
            loading: false
          }
        }));
      }
    } catch (error) {
      console.error('Exception loading folder:', error);
    }
  };

  // Ref to track folder states without triggering re-renders in useEffect
  const folderStatesRef = React.useRef(folderStates);

  // Update ref when state changes
  useEffect(() => {
    folderStatesRef.current = folderStates;
  }, [folderStates]);

  // Stable Polling Mechanism
  useEffect(() => {
    const poll = async () => {
      // 1. Refresh Root
      await loadRootItems(true);

      // 2. Refresh Open Folders
      const currentStates = folderStatesRef.current;
      const openPaths = Object.keys(currentStates).filter(path => currentStates[path].isOpen);

      for (const path of openPaths) {
        await loadFolderContents(path, true);
      }

      checkServiceStatus();
      setLastSynced(new Date());
    };

    const intervalId = setInterval(poll, 3000); // Relaxed polling
    return () => clearInterval(intervalId);
  }, [rootPath]); // Reset poll on scope change

  // Toggle folder open/closed
  const toggleFolder = async (item: FileItem) => {
    const folderState = folderStates[item.file_path];

    if (!folderState?.isOpen) {
      // Opening folder - load its contents
      await loadFolderContents(item.file_path);
    } else {
      // Closing folder
      setFolderStates(prev => ({
        ...prev,
        [item.file_path]: { ...prev[item.file_path], isOpen: false }
      }));
    }
  };

  // Toggle file/folder accessibility
  const toggleAccessibility = async (item: FileItem, e: React.MouseEvent) => {
    e.stopPropagation();

    const newStatus = !item.accessible;

    try {
      // Use RPC to bypass RLS and ensure update
      const { error } = await supabase.rpc('toggle_lock', {
        file_id: item.id,
        new_status: newStatus
      });

      if (error) {
        console.error('Error updating accessibility:', error);
        // Fallback to direct update if RPC fails
        const { error: directError } = await supabase
          .from('file_permissions')
          .update({ accessible: newStatus })
          .eq('id', item.id);

        if (directError) alert('Failed to update status.');
      } else {
        await loadRootItems(true);
      }
    } catch (error) {
      console.error('Exception updating accessibility:', error);
    }
  };

  // Search files - scoped to current root implicitly via UI context usually, 
  // but global search is arguably better. We'll stick to global for now or refine later.
  const handleSearch = async () => {
    if (!searchTerm.trim()) {
      setSearchResults([]);
      return;
    }

    try {
      const { data, error } = await supabase.rpc('search_files', {
        search_term: searchTerm
      });

      if (error) {
        console.error('Error searching:', error);
      } else {
        setSearchResults(data || []);
      }
    } catch (error) {
      console.error('Exception during search:', error);
    }
  };

  // Format file size
  const formatFileSize = (bytes: string) => {
    const num = parseInt(bytes);
    if (num === 0 || isNaN(num)) return '-';
    if (num < 1024) return num + ' B';
    if (num < 1024 * 1024) return (num / 1024).toFixed(1) + ' KB';
    if (num < 1024 * 1024 * 1024) return (num / (1024 * 1024)).toFixed(1) + ' MB';
    return (num / (1024 * 1024 * 1024)).toFixed(1) + ' GB';
  };

  // Render individual file/folder item
  const renderItem = (item: FileItem, depth: number = 0) => {
    const folderState = folderStates[item.file_path];
    const isOpen = folderState?.isOpen || false;
    const hasChildren = item.is_directory;

    return (
      <div key={item.id}>
        <div
          className={`flex items-center gap-2 p-2 cursor-pointer rounded transition-colors ${hoverClass} ${!item.accessible ? 'opacity-70' : ''
            }`}
          style={{ paddingLeft: `${depth * 20 + 8}px` }}
          onClick={() => hasChildren && toggleFolder(item)}
        >
          {/* Expand/Collapse Icon */}
          {hasChildren && (
            <div className="w-4">
              {isOpen ? (
                <ChevronDown className="w-4 h-4 text-gray-500" />
              ) : (
                <ChevronRight className="w-4 h-4 text-gray-500" />
              )}
            </div>
          )}
          {!hasChildren && <div className="w-4" />}

          {/* File/Folder Icon */}
          {item.is_directory ? (
            <Folder className="w-5 h-5 text-blue-500" />
          ) : (
            <File className={`w-5 h-5 ${theme === 'stealth' ? 'text-gray-400' : 'text-gray-500'}`} />
          )}

          {/* Name */}
          <span className={`flex-1 text-sm ${theme === 'stealth' ? 'text-gray-200' : 'text-gray-900'}`}>
            {item.name}
          </span>

          {/* File Size */}
          {!item.is_directory && (
            <span className={`text-xs ${theme === 'stealth' ? 'text-gray-500' : 'text-gray-400'}`}>
              {formatFileSize(item.file_size || '0')}
            </span>
          )}

          {/* Lock/Unlock Button */}
          <button
            onClick={(e) => toggleAccessibility(item, e)}
            className={`p-1 rounded transition-colors ${theme === 'stealth' ? 'hover:bg-gray-700' : 'hover:bg-gray-200'
              }`}
            title={item.accessible ? 'Click to lock' : 'Click to unlock'}
          >
            {item.accessible ? (
              <Unlock className="w-4 h-4 text-green-500" />
            ) : (
              <Lock className="w-4 h-4 text-red-500" />
            )}
          </button>
        </div>

        {/* Render children if folder is open */}
        {hasChildren && isOpen && (
          <div>
            {folderState?.loading ? (
              <div
                className="text-sm text-gray-500 p-2"
                style={{ paddingLeft: `${(depth + 1) * 20 + 8}px` }}
              >
                Loading...
              </div>
            ) : folderState?.children && folderState.children.length > 0 ? (
              folderState.children.map((child) => renderItem(child, depth + 1))
            ) : (
              <div
                className="text-sm text-gray-500 p-2 italic"
                style={{ paddingLeft: `${(depth + 1) * 20 + 8}px` }}
              >
                Empty folder
              </div>
            )}
          </div>
        )}
      </div>
    );
  };

  return (
    <div className="max-w-4xl mx-auto py-6">
      {/* Header */}
      <div className="mb-6">
        <div className="flex items-center justify-between mb-4">
          <h1 className="text-2xl font-bold">GateKeeper Explorer</h1>
          <div className="flex items-center gap-4">
            <div className="text-xs text-gray-500">
              Last synced: {lastSynced.toLocaleTimeString()}
            </div>
            <button
              onClick={() => { setLastSynced(new Date()); loadRootItems(); }}
              className="p-1 px-3 bg-blue-600 text-white rounded text-sm hover:bg-blue-700"
            >
              Refresh
            </button>
            <div className={`w-3 h-3 rounded-full ${serviceOnline ? 'bg-green-500' : 'bg-red-500'}`} />
            <span className={`text-sm ${theme === 'stealth' ? 'text-gray-400' : 'text-gray-600'}`}>
              {serviceOnline ? 'System Online' : 'System Offline'}
            </span>
          </div>
        </div>

        {/* Search Bar */}
        <div className="flex gap-2">
          <input
            type="text"
            placeholder="Search all scopes..."
            value={searchTerm}
            onChange={(e) => setSearchTerm(e.target.value)}
            onKeyPress={(e) => e.key === 'Enter' && handleSearch()}
            className={`flex-1 px-4 py-2 border rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 ${inputClass}`}
          />
          <button
            onClick={handleSearch}
            className="px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors"
          >
            <Search className="w-5 h-5" />
          </button>
        </div>
      </div>

      {/* Search Results */}
      {searchResults.length > 0 && (
        <div
          className={`mb-6 p-4 rounded-lg border ${theme === 'stealth'
            ? 'bg-yellow-900/20 border-yellow-700/50'
            : 'bg-yellow-50 border-yellow-200'
            }`}
        >
          <h3 className="font-semibold mb-2">Search Results ({searchResults.length})</h3>
          <div className="space-y-1">
            {searchResults.map((item) => renderItem(item, 0))}
          </div>
        </div>
      )}

      {/* File Tree */}
      <div className={`border rounded-lg p-4 ${containerClass}`}>
        {/* Breadcrumb - Static for Root for now */}
        <div
          className={`flex items-center gap-2 mb-4 pb-3 border-b ${theme === 'stealth' ? 'border-gray-700' : 'border-gray-100'
            }`}
        >
          <Home className="w-4 h-4 text-gray-500" />
          {currentPath.map((path, idx) => (
            <React.Fragment key={idx}>
              <ChevronRight className="w-4 h-4 text-gray-500" />
              <span className="text-sm font-medium">{path}</span>
            </React.Fragment>
          ))}
        </div>

        {/* File List */}
        {loading ? (
          <div className="text-center py-8 text-gray-500">Loading files...</div>
        ) : rootItems.length === 0 ? (
          <div className="text-center py-8 text-gray-500">
            No files found in {rootName}.
            <br /><span className="text-xs opacity-70">Make sure the C++ service has scanned this scope.</span>
          </div>
        ) : (
          <div className="space-y-1">
            {rootItems.map((item) => renderItem(item, 0))}
          </div>
        )}
      </div>

      {/* Scope Info */}
      <div className="mt-4 text-xs opacity-40 font-mono text-center">
        ScopeID: {rootPath}
      </div>

    </div>
  );
}
