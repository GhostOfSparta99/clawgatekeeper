import 'package:flutter/material.dart';

class Scope {
  final String id;
  final String name;
  final String path;
  final IconData icon;

  const Scope({
    required this.id,
    required this.name,
    required this.path,
    required this.icon,
  });
}

final List<Scope> availableScopes = [
  const Scope(
    id: 'Documents',
    name: 'Documents',
    path: 'C:/Users/Adi/Documents',
    icon: Icons.description,
  ),
  const Scope(
    id: 'Downloads',
    name: 'Downloads',
    path: 'C:/Users/Adi/Downloads',
    icon: Icons.download,
  ),
  const Scope(
    id: 'Desktop',
    name: 'Desktop',
    path: 'C:/Users/Adi/Desktop',
    icon: Icons.folder,
  ),
  const Scope(
    id: 'Pictures',
    name: 'Pictures',
    path: 'C:/Users/Adi/Pictures',
    icon: Icons.image,
  ),
  const Scope(
    id: 'Videos',
    name: 'Videos',
    path: 'C:/Users/Adi/Videos',
    icon: Icons.video_library,
  ),
];
