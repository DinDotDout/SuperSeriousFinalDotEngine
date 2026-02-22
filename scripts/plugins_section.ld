SECTIONS
{
  .plugins : {
    PROVIDE_HIDDEN(__start_plugins = .);
    KEEP(*(SORT_BY_INIT_PRIORITY(.plugins.*)))
    PROVIDE_HIDDEN(__stop_plugins = .);
  }
} INSERT AFTER .data;
