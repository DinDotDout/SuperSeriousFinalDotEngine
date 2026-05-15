SECTIONS
{
  .DOT_plugins : {
    PROVIDE_HIDDEN(__start_DOT_plugins = .);
    KEEP(*(SORT_BY_INIT_PRIORITY(.DOT_plugins.*)))
    PROVIDE_HIDDEN(__stop_DOT_plugins = .);
  }
  .DOT_tests : {
    PROVIDE_HIDDEN(__start_DOT_tests = .);
    KEEP(*(SORT_BY_INIT_PRIORITY(.DOT_tests.*)))
    PROVIDE_HIDDEN(__stop_DOT_tests = .);
  }
} INSERT AFTER .data;
