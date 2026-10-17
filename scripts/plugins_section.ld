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
  .ReflectSection : {
    PROVIDE_HIDDEN(__start_ReflectSection = .);
    KEEP(*(SORT_BY_INIT_PRIORITY(.ReflectSection.*)))
    PROVIDE_HIDDEN(__stop_ReflectSection = .);
  }
} INSERT AFTER .data;
