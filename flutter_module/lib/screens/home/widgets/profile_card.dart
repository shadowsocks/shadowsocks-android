import 'package:flutter/material.dart';
import '../../../models/profile.dart';
import '../../../models/traffic_stats.dart';

class ProfileCard extends StatelessWidget {
  final Profile profile;
  final bool isSelected;
  final bool isConnected;
  final VoidCallback onTap;
  final VoidCallback onEdit;
  final VoidCallback onShare;
  final VoidCallback? onDismissed;

  const ProfileCard({
    super.key,
    required this.profile,
    required this.isSelected,
    this.isConnected = false,
    required this.onTap,
    required this.onEdit,
    required this.onShare,
    this.onDismissed,
  });

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final colorScheme = theme.colorScheme;

    final selectedColor =
        isConnected ? colorScheme.secondary : colorScheme.primary;

    return Card(
      margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
      color: isSelected
          ? selectedColor.withAlpha(20)
          : theme.cardTheme.color,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(16),
        side: isSelected
            ? BorderSide(color: selectedColor, width: 2)
            : BorderSide(color: colorScheme.outlineVariant.withAlpha(77)),
      ),
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(16),
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Row(
            children: [
              // Selection indicator
              Container(
                width: 40,
                height: 40,
                decoration: BoxDecoration(
                  shape: BoxShape.circle,
                  color: isSelected
                      ? (isConnected
                          ? colorScheme.secondary
                          : colorScheme.primary)
                      : colorScheme.surfaceContainerHighest,
                ),
                child: Icon(
                  isSelected
                      ? (isConnected
                          ? Icons.flash_on_rounded
                          : Icons.check_rounded)
                      : Icons.vpn_key_outlined,
                  size: 20,
                  color: isSelected
                      ? Colors.white
                      : colorScheme.onSurfaceVariant,
                ),
              ),
              const SizedBox(width: 16),
              // Profile info
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      profile.displayName,
                      style: theme.textTheme.titleMedium?.copyWith(
                        fontWeight:
                            isSelected ? FontWeight.w600 : FontWeight.w500,
                        color: isSelected
                            ? selectedColor
                            : colorScheme.onSurface,
                      ),
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                    ),
                    const SizedBox(height: 2),
                    Text(
                      '${profile.host}:${profile.remotePort}',
                      style: theme.textTheme.bodySmall?.copyWith(
                        color: colorScheme.onSurfaceVariant,
                      ),
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                    ),
                    if (profile.plugin != null &&
                        profile.plugin!.isNotEmpty) ...[
                      const SizedBox(height: 4),
                      Container(
                        padding: const EdgeInsets.symmetric(
                            horizontal: 8, vertical: 2),
                        decoration: BoxDecoration(
                          color: colorScheme.tertiaryContainer,
                          borderRadius: BorderRadius.circular(8),
                        ),
                        child: Text(
                          profile.plugin!.split('/').last,
                          style: theme.textTheme.labelSmall?.copyWith(
                            color: colorScheme.onTertiaryContainer,
                          ),
                        ),
                      ),
                    ],
                    if (profile.tx > 0 || profile.rx > 0) ...[
                      const SizedBox(height: 6),
                      Row(
                        children: [
                          Icon(Icons.arrow_upward_rounded,
                              size: 12, color: colorScheme.primary),
                          const SizedBox(width: 2),
                          Text(
                            TrafficStats.formatBytes(profile.tx),
                            style: theme.textTheme.labelSmall?.copyWith(
                              color: colorScheme.onSurfaceVariant
                                  .withAlpha(179),
                            ),
                          ),
                          const SizedBox(width: 12),
                          Icon(Icons.arrow_downward_rounded,
                              size: 12, color: colorScheme.secondary),
                          const SizedBox(width: 2),
                          Text(
                            TrafficStats.formatBytes(profile.rx),
                            style: theme.textTheme.labelSmall?.copyWith(
                              color: colorScheme.onSurfaceVariant
                                  .withAlpha(179),
                            ),
                          ),
                        ],
                      ),
                    ],
                  ],
                ),
              ),
              // Action buttons
              Column(
                children: [
                  IconButton(
                    icon: Icon(
                      Icons.edit_outlined,
                      color: colorScheme.onSurfaceVariant,
                    ),
                    iconSize: 20,
                    visualDensity: VisualDensity.compact,
                    onPressed: onEdit,
                  ),
                  IconButton(
                    icon: Icon(
                      Icons.share_outlined,
                      color: colorScheme.onSurfaceVariant,
                    ),
                    iconSize: 20,
                    visualDensity: VisualDensity.compact,
                    onPressed: onShare,
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }
}
