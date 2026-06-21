-- group_deps.lua -- Group of modules that other modules depend on, useless on their own

-- Some of these are order-dependent because they expose functions that others cannot work without
load "commands"
load "pid_tables"
load "caps"
load "lib_l10n"
load "lib_sock"
