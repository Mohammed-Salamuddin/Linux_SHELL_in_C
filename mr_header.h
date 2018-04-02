void mr_shell_start();
int redirection_pipe_both(char *line);
int mr_is_both(char* line);
int mr_is_pipe(char *line);
int mr_is_redirection(char* line);
char ** mr_split_pipe(char *line);
char** mr_split_redirection(char* line);
char** mr_split_both(char *line);
int mr_execute_both(char**cmds);

//to be removed
int mr_execute_pipe(char **cmds);
int mr_check_infile(char *last_cmd);
char* mr_pure_last_cmd(char *last_cmd);

char* mr_read_line(void);
char **mr_split_line(char *line);
int mr_launch(char **args);
int mr_execute(char **args);

//builtin
int mr_exit(char **args);
int mr_help(char **args);
int mr_cd(char **args);
int mr_history(char** args);

//new
char** mr_update_cmd_with_alias(char **cmds);
char * mr_do_alias_maping(char *line);
int mr_is_source_cmd(char *line);
void mr_sourcing();
int mr_execute_redirection(char **cmds);