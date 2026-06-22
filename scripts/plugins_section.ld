SECTIONS
{
  .PluginsSection : {
    PROVIDE_HIDDEN(__start_PluginsSection = .);
    KEEP(*(SORT_BY_INIT_PRIORITY(.PluginsSection.*)))
    PROVIDE_HIDDEN(__stop_PluginsSection = .);
  }
  .SettingsSection : {
    PROVIDE_HIDDEN(__start_SettingsSection = .);
    KEEP(*(SORT_BY_INIT_PRIORITY(.SettingsSection.*)))
    PROVIDE_HIDDEN(__stop_SettingsSection = .);
  }
} INSERT AFTER .data;
