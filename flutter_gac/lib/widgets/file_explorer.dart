import 'package:flutter/material.dart';
import 'package:intl/intl.dart';
import 'dart:async';
import '../models/file_item.dart';
import '../services/supabase_service.dart';

class FileExplorer extends StatefulWidget {
  final String rootPath;

  const FileExplorer({super.key, required this.rootPath});

  @override
  State<FileExplorer> createState() => _FileExplorerState();
}

class _FileExplorerState extends State<FileExplorer> {
  final SupabaseService _supabaseService = SupabaseService();
  final TextEditingController _searchController = TextEditingController();

  List<FileItem> _rootItems = [];
  Map<String, FolderState> _folderStates = {};
  List<FileItem> _searchResults = [];
  bool _loading = true;
  bool _serviceOnline = false;
  DateTime _lastSynced = DateTime.now();
  Timer? _pollTimer;

  @override
  void initState() {
    super.initState();
    _loadRootItems();
    _checkServiceStatus();
    _startPolling();
  }

  @override
  void dispose() {
    _pollTimer?.cancel();
    _searchController.dispose();
    super.dispose();
  }

  void _startPolling() {
    _pollTimer = Timer.periodic(const Duration(seconds: 3), (timer) {
      _loadRootItems(silent: true);
      _checkServiceStatus();
      _refreshOpenFolders();
    });
  }

  Future<void> _loadRootItems({bool silent = false}) async {
    if (!silent) setState(() => _loading = true);

    final items = await _supabaseService.fetchRootItems(widget.rootPath);

    if (mounted) {
      setState(() {
        _rootItems = items;
        _lastSynced = DateTime.now();
        if (!silent) _loading = false;
      });
    }
  }

  Future<void> _checkServiceStatus() async {
    final online = await _supabaseService.checkServiceStatus();
    if (mounted) {
      setState(() {
        _serviceOnline = online;
      });
    }
  }

  Future<void> _refreshOpenFolders() async {
    final openPaths = _folderStates.entries
        .where((entry) => entry.value.isOpen)
        .map((entry) => entry.key)
        .toList();

    for (final path in openPaths) {
      await _loadFolderContents(path, silent: true);
    }
  }

  Future<void> _loadFolderContents(String folderPath, {bool silent = false}) async {
    if (!silent) {
      setState(() {
        _folderStates[folderPath] = FolderState(
          isOpen: true,
          children: _folderStates[folderPath]?.children ?? [],
          loading: true,
        );
      });
    }

    final items = await _supabaseService.fetchFolderContents(folderPath);

    if (mounted) {
      setState(() {
        _folderStates[folderPath] = FolderState(
          isOpen: true,
          children: items,
          loading: false,
        );
      });
    }
  }

  Future<void> _toggleFolder(FileItem item) async {
    final folderState = _folderStates[item.filePath];

    if (folderState?.isOpen != true) {
      await _loadFolderContents(item.filePath);
    } else {
      setState(() {
        _folderStates[item.filePath] = FolderState(
          isOpen: false,
          children: folderState!.children,
          loading: false,
        );
      });
    }
  }

  Future<void> _toggleAccessibility(FileItem item) async {
    final newStatus = !item.accessible;
    final success = await _supabaseService.toggleAccessibility(item.id, newStatus);

    if (success && mounted) {
      await _loadRootItems(silent: true);
    }
  }

  Future<void> _handleSearch() async {
    final searchTerm = _searchController.text.trim();
    if (searchTerm.isEmpty) {
      setState(() => _searchResults = []);
      return;
    }

    final results = await _supabaseService.searchFiles(searchTerm);
    if (mounted) {
      setState(() => _searchResults = results);
    }
  }

  String _formatFileSize(String? bytes) {
    if (bytes == null || bytes == '0') return '-';
    final num = int.tryParse(bytes) ?? 0;
    if (num == 0) return '-';
    if (num < 1024) return '$num B';
    if (num < 1024 * 1024) return '${(num / 1024).toStringAsFixed(1)} KB';
    if (num < 1024 * 1024 * 1024) {
      return '${(num / (1024 * 1024)).toStringAsFixed(1)} MB';
    }
    return '${(num / (1024 * 1024 * 1024)).toStringAsFixed(1)} GB';
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);

    return Padding(
      padding: const EdgeInsets.all(24.0),
      child: Column(
        children: [
          Row(
            children: [
              Expanded(
                child: Text(
                  'GateKeeper Explorer',
                  style: theme.textTheme.titleLarge?.copyWith(fontSize: 24),
                ),
              ),
              Text(
                'Last synced: ${DateFormat('HH:mm:ss').format(_lastSynced)}',
                style: TextStyle(
                  fontSize: 11,
                  color: theme.textTheme.bodyMedium?.color?.withOpacity(0.5),
                ),
              ),
              const SizedBox(width: 16),
              IconButton(
                icon: const Icon(Icons.refresh, size: 20),
                onPressed: () {
                  setState(() => _lastSynced = DateTime.now());
                  _loadRootItems();
                },
                style: IconButton.styleFrom(
                  backgroundColor: theme.primaryColor,
                  foregroundColor: Colors.white,
                ),
              ),
              const SizedBox(width: 16),
              Container(
                width: 12,
                height: 12,
                decoration: BoxDecoration(
                  color: _serviceOnline ? Colors.green : Colors.red,
                  shape: BoxShape.circle,
                ),
              ),
              const SizedBox(width: 8),
              Text(
                _serviceOnline ? 'System Online' : 'System Offline',
                style: TextStyle(
                  fontSize: 13,
                  color: theme.textTheme.bodyMedium?.color?.withOpacity(0.6),
                ),
              ),
            ],
          ),
          const SizedBox(height: 24),
          Row(
            children: [
              Expanded(
                child: TextField(
                  controller: _searchController,
                  onSubmitted: (_) => _handleSearch(),
                  decoration: InputDecoration(
                    hintText: 'Search all scopes...',
                    border: OutlineInputBorder(
                      borderRadius: BorderRadius.circular(8),
                    ),
                    contentPadding: const EdgeInsets.symmetric(
                      horizontal: 16,
                      vertical: 12,
                    ),
                  ),
                ),
              ),
              const SizedBox(width: 8),
              IconButton(
                icon: const Icon(Icons.search),
                onPressed: _handleSearch,
                style: IconButton.styleFrom(
                  backgroundColor: theme.primaryColor,
                  foregroundColor: Colors.white,
                  padding: const EdgeInsets.all(12),
                ),
              ),
            ],
          ),
          if (_searchResults.isNotEmpty) ...[
            const SizedBox(height: 24),
            Container(
              padding: const EdgeInsets.all(16),
              decoration: BoxDecoration(
                color: Colors.yellow.shade900.withOpacity(0.2),
                border: Border.all(
                  color: Colors.yellow.shade700.withOpacity(0.5),
                ),
                borderRadius: BorderRadius.circular(8),
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    'Search Results (${_searchResults.length})',
                    style: theme.textTheme.titleMedium?.copyWith(
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                  const SizedBox(height: 8),
                  ..._searchResults.map((item) => _buildFileItem(item, 0)),
                ],
              ),
            ),
          ],
          const SizedBox(height: 24),
          Expanded(
            child: Container(
              decoration: BoxDecoration(
                color: theme.cardColor,
                border: Border.all(color: theme.dividerColor),
                borderRadius: BorderRadius.circular(8),
              ),
              child: _loading
                  ? const Center(child: CircularProgressIndicator())
                  : _rootItems.isEmpty
                      ? Center(
                          child: Column(
                            mainAxisAlignment: MainAxisAlignment.center,
                            children: [
                              Icon(
                                Icons.folder_off,
                                size: 48,
                                color: theme.iconTheme.color?.withOpacity(0.3),
                              ),
                              const SizedBox(height: 16),
                              Text(
                                'No files found',
                                style: TextStyle(
                                  color: theme.textTheme.bodyMedium?.color?.withOpacity(0.5),
                                ),
                              ),
                              const SizedBox(height: 8),
                              Text(
                                'Make sure the C++ service has scanned this scope',
                                style: TextStyle(
                                  fontSize: 11,
                                  color: theme.textTheme.bodyMedium?.color?.withOpacity(0.3),
                                ),
                              ),
                            ],
                          ),
                        )
                      : ListView(
                          padding: const EdgeInsets.all(16),
                          children: _rootItems.map((item) => _buildFileItem(item, 0)).toList(),
                        ),
            ),
          ),
          const SizedBox(height: 16),
          Text(
            'ScopeID: ${widget.rootPath}',
            style: TextStyle(
              fontSize: 10,
              fontFamily: 'monospace',
              color: theme.textTheme.bodyMedium?.color?.withOpacity(0.4),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildFileItem(FileItem item, int depth) {
    final theme = Theme.of(context);
    final folderState = _folderStates[item.filePath];
    final isOpen = folderState?.isOpen ?? false;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        InkWell(
          onTap: item.isDirectory ? () => _toggleFolder(item) : null,
          borderRadius: BorderRadius.circular(4),
          child: Padding(
            padding: EdgeInsets.only(
              left: depth * 20.0 + 8,
              top: 8,
              bottom: 8,
              right: 8,
            ),
            child: Row(
              children: [
                if (item.isDirectory)
                  Icon(
                    isOpen ? Icons.keyboard_arrow_down : Icons.keyboard_arrow_right,
                    size: 16,
                    color: theme.iconTheme.color?.withOpacity(0.5),
                  )
                else
                  const SizedBox(width: 16),
                const SizedBox(width: 8),
                Icon(
                  item.isDirectory ? Icons.folder : Icons.insert_drive_file,
                  size: 20,
                  color: item.isDirectory
                      ? theme.primaryColor
                      : theme.iconTheme.color?.withOpacity(0.6),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: Text(
                    item.name,
                    style: TextStyle(
                      fontSize: 14,
                      color: theme.textTheme.bodyMedium?.color,
                      decoration: item.accessible ? null : TextDecoration.lineThrough,
                      decorationColor: theme.textTheme.bodyMedium?.color?.withOpacity(0.5),
                    ),
                  ),
                ),
                if (!item.isDirectory) ...[
                  Text(
                    _formatFileSize(item.fileSize),
                    style: TextStyle(
                      fontSize: 11,
                      color: theme.textTheme.bodyMedium?.color?.withOpacity(0.4),
                    ),
                  ),
                  const SizedBox(width: 16),
                ],
                IconButton(
                  icon: Icon(
                    item.accessible ? Icons.lock_open : Icons.lock,
                    size: 16,
                    color: item.accessible ? Colors.green : Colors.red,
                  ),
                  onPressed: () => _toggleAccessibility(item),
                  tooltip: item.accessible ? 'Click to lock' : 'Click to unlock',
                ),
              ],
            ),
          ),
        ),
        if (item.isDirectory && isOpen)
          if (folderState!.loading)
            Padding(
              padding: EdgeInsets.only(left: (depth + 1) * 20.0 + 8),
              child: const Padding(
                padding: EdgeInsets.all(8.0),
                child: Text('Loading...', style: TextStyle(fontSize: 13)),
              ),
            )
          else if (folderState.children.isEmpty)
            Padding(
              padding: EdgeInsets.only(left: (depth + 1) * 20.0 + 8),
              child: Padding(
                padding: const EdgeInsets.all(8.0),
                child: Text(
                  'Empty folder',
                  style: TextStyle(
                    fontSize: 13,
                    fontStyle: FontStyle.italic,
                    color: theme.textTheme.bodyMedium?.color?.withOpacity(0.5),
                  ),
                ),
              ),
            )
          else
            ...folderState.children.map((child) => _buildFileItem(child, depth + 1)),
      ],
    );
  }
}

class FolderState {
  final bool isOpen;
  final List<FileItem> children;
  final bool loading;

  FolderState({
    required this.isOpen,
    required this.children,
    required this.loading,
  });
}
