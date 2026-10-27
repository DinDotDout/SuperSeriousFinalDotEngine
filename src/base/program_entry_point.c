#ifndef DOT_PROGRAM_ENTRY_POINT_H
#define DOT_PROGRAM_ENTRY_POINT_H

int entry_point(int arg_count, String8 arg_values[]);

typedef i32 ProgramEntryFlag;

enum{ PROGRAM_ENTRY_MAX_FLAGS = 32, };

typedef struct ProgramEntryFlagCtx{
    String8 flag;
    String8 flag_desc;
    void (*set_int)(i32 value);
    void (*set_bool)(b32 value);
    void (*set_string)(String8 value);
    i32 int_value;
    b32 bool_value;
}ProgramEntryFlagCtx;

internal void
program_entry_show_help(String8 program_name, i32 flags_count, ProgramEntryFlagCtx *flags) {
    printf("Usage: %s [flags]\n\n", program_name.cstr);
    printf("Flags:\n");
    for (i32 i = 0; i < flags_count; i++){
        ProgramEntryFlagCtx *flag = &flags[i];
        printf("  %-20s %s\n", flag->flag.cstr, flag->flag_desc.cstr);
    }
    printf("  %-20s Show this help\n", "--help");
}

internal b32
program_entry_flags_apply(i32 flags_count, ProgramEntryFlagCtx *flags, i32 arg_count, String8 arg_values[])
{
    b32 show_help = false;
    for (i32 i = 1; i < arg_count; i++){
        if(string8_equal(arg_values[i], str_comp("-help"))){
            show_help = true;
            break;
        }

        bool flag_exists = false;
        ProgramEntryFlagCtx *flag = NULL;
        for(i32 f = 0; f < flags_count; f++){
            flag = &flags[f];
            if(string8_equal(arg_values[i], flag->flag)){
                flag_exists = true;
                break;
            }
        }
        if(flag_exists){
            if(flag->set_int){
                flag->set_int(flag->int_value);
            }
            if(flag->set_bool){
                flag->set_bool(flag->bool_value);
            }
            if(flag->set_string){
                if(i + 1 < arg_count){
                    i += 1;
                    flag->set_string(arg_values[i]);
                }else{
                    DOT_WARNING("Flag %S expects a value\n", flag->flag);
                }
            }
        }else{
            DOT_WARNING("Unknown flag: %S\n", arg_values[i]);
        }
    }
    if(show_help){
        String8 program_name = arg_values[0];
        program_entry_show_help(program_name, flags_count, flags);
    }
    return show_help;
}

int
main(int argc, char *argv[])
{
    DOT_ASSERT(argc < PROGRAM_ENTRY_MAX_FLAGS);
    String8 flags[PROGRAM_ENTRY_MAX_FLAGS];

    for EACH_INDEX(i, cast(u64)argc){
        flags[i] = string8_from_cstring(argv[i]);
    }

    os_init(); // Install handlers and to whatever we need to set up
    int ret = entry_point(argc, flags);
    return ret;
}

#endif // !DOT_PROGRAM_ENTRY_POINT_H
