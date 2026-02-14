class FileItem {
  final int id;
  final String filePath;
  final String name;
  final String parentPath;
  final bool isDirectory;
  final bool accessible;
  final String? fileSize;
  final String? fileExtension;
  final DateTime? lastModified;

  FileItem({
    required this.id,
    required this.filePath,
    required this.name,
    required this.parentPath,
    required this.isDirectory,
    required this.accessible,
    this.fileSize,
    this.fileExtension,
    this.lastModified,
  });

  factory FileItem.fromJson(Map<String, dynamic> json) {
    return FileItem(
      id: json['id'] as int,
      filePath: json['file_path'] as String,
      name: json['name'] as String,
      parentPath: json['parent_path'] as String? ?? '',
      isDirectory: json['is_directory'] as bool? ?? false,
      accessible: json['accessible'] as bool? ?? true,
      fileSize: json['file_size'] as String?,
      fileExtension: json['file_extension'] as String?,
      lastModified: json['last_modified'] != null
          ? DateTime.parse(json['last_modified'] as String)
          : null,
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'id': id,
      'file_path': filePath,
      'name': name,
      'parent_path': parentPath,
      'is_directory': isDirectory,
      'accessible': accessible,
      'file_size': fileSize,
      'file_extension': fileExtension,
      'last_modified': lastModified?.toIso8601String(),
    };
  }

  FileItem copyWith({
    int? id,
    String? filePath,
    String? name,
    String? parentPath,
    bool? isDirectory,
    bool? accessible,
    String? fileSize,
    String? fileExtension,
    DateTime? lastModified,
  }) {
    return FileItem(
      id: id ?? this.id,
      filePath: filePath ?? this.filePath,
      name: name ?? this.name,
      parentPath: parentPath ?? this.parentPath,
      isDirectory: isDirectory ?? this.isDirectory,
      accessible: accessible ?? this.accessible,
      fileSize: fileSize ?? this.fileSize,
      fileExtension: fileExtension ?? this.fileExtension,
      lastModified: lastModified ?? this.lastModified,
    );
  }
}
