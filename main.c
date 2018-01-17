
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>


/*
  Function Declarations for builtin shell commands:
 */
int protosh_cd(char **args);
int protosh_help(char **args);
int protosh_exit(char **args);



/* Shared global variables */
static char **history;             /* array of strings for storing history */
static int history_len;            /* current number of items in history */
static char *input;		   /* input entered by the user */

/* useful for keeping track of parent's prev call for cleanup */
static struct command *parent_cmd;
static struct commands *parent_cmds;
static char *temp_line;


/*
  List of builtin commands, followed by their corresponding functions.
 */
char *builtin_str[] = {
  "cd",
  "help",
  "exit",
  "history"
};

int (*builtin_func[]) (char **) = {
  &protosh_cd,
  &protosh_help,
  &protosh_exit,
  &protosh_history
};

int protosh_num_builtins() {
  return sizeof(builtin_str) / sizeof(char *);
}


/*
  Builtin function implementations.
*/

/**
   @brief Bultin command: change directory.
   @param args List of args.  args[0] is "cd".  args[1] is the directory.
   @return Always returns 1, to continue executing.
 */
int protosh_cd(char **args)
{
  if (args[1] == NULL) {
    fprintf(stderr, "protosh: expected argument to \"cd\"\n");
  } else {
    if (chdir(args[1]) != 0) {
      perror("protosh");
    }
  }
  return 1;
}

/**
   @brief Builtin command: print help.
   @param args List of args.  Not examined.
   @return Always returns 1, to continue executing.
 */
int protosh_help(char **args)
{
  int i;
  printf("Jonathan Medhanie's protosh\n");
  printf("Type program names and arguments, and hit enter!\n");
  printf("The following are built in:\n");

  for (i = 0; i < protosh_num_builtins(); i++) {
    printf("  %s\n", builtin_str[i]);
  }

  printf("Use the man command for information on other programs.\n");
  return 1;
}

/**
   @brief Builtin command: exit.
   @param args List of args.  Not examined.
   @return Always returns 0, to terminate execution.
 */
int protosh_exit(char **args)
{
  return 0;
}

/**
  @brief Launch a program and wait for it to terminate.
  @param args Null terminated list of arguments (including program).
  @return Always returns 1, to continue execution.
 */
int protosh_launch(char **args)
{
  pid_t pid;
  int status;

  pid = fork();
  if (pid == 0) {
    // Child process
    if (execvp(args[0], args) == -1) {
      perror("protosh");
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) {
    // Error forking
    perror("protosh");
  } else {
    // Parent process
    do {
      waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
}

/**
   @brief Execute shell built-in or launch program.
   @param args Null terminated list of arguments.
   @return 1 if the shell should continue running, 0 if it should terminate
 */
int protosh_execute(char **args)
{
  int i;

  if (args[0] == NULL) {
    // An empty command was entered.
    return 1;
  }

  for (i = 0; i < protosh_num_builtins(); i++) {
    if (strcmp(args[0], builtin_str[i]) == 0) {
      return (*builtin_func[i])(args);
    }
  }

  return protosh_launch(args);
}

#define protosh_RL_BUFSIZE 1024
/**
   @brief Read a line of input from stdin.
   @return The line from stdin.
 */
char *protosh_read_line(void)
{
  int bufsize = protosh_RL_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * bufsize);
  int c;

  if (!buffer) {
    fprintf(stderr, "protosh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    // Read a character
    c = getchar();

    if (c == EOF) {
      exit(EXIT_SUCCESS);
    } else if (c == '\n') {
      buffer[position] = '\0';
      return buffer;
    } else {
      buffer[position] = c;
    }
    position++;

    // If we have exceeded the buffer, reallocate.
    if (position >= bufsize) {
      bufsize += protosh_RL_BUFSIZE;
      buffer = realloc(buffer, bufsize);
      if (!buffer) {
        fprintf(stderr, "protosh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
  }
}

#define protosh_TOK_BUFSIZE 64
#define protosh_TOK_DELIM " \t\r\n\a"
/**
   @brief Split a line into tokens (very naively).
   @param line The line.
   @return Null-terminated array of tokens.
 */
char **protosh_split_line(char *line)
{
  int bufsize = protosh_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token, **tokens_backup;

  if (!tokens) {
    fprintf(stderr, "protosh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, protosh_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    if (position >= bufsize) {
      bufsize += protosh_TOK_BUFSIZE;
      tokens_backup = tokens;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
		free(tokens_backup);
        fprintf(stderr, "protosh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, protosh_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}

/**
   @brief Loop getting input and executing it.
 */
void protosh_loop(void)
{
  char *line;
  char **args;
  int status;

  do {
    printf("> ");
    line = protosh_read_line();
    args = protosh_split_line(line);
    status = protosh_execute(args);

    free(line);
    free(args);
  } while (status);
}


/* returns whether a command is a history command */
int is_history_command(char *input)
{
	const char *key = "history";

	if (strlen(input) < strlen(key))
		return 0;
	int i;

	for (i = 0; i < (int) strlen(key); i++) {
		if (input[i] != key[i])
			return 0;
	}
	return 1;
}
/* Clears the history */
int clear_history(void)
{
	int i;

	for (i = 0; i < history_len; i++)
		free(history[i]);
	history_len = 0;
	return 0;
}

/* Responsible for handling the history shell builtin */
int protosh_history(struct commands *cmds, struct command *cmd)
{
	/* just `history` executed? print history */
	if (cmd->argc == 1) {
		int i;

		for (i = 0; i < history_len ; i++) {
			// write to a file descriptor - output_fd
			printf("%d %s\n", i, history[i]);
		}
		return 1;
	}
	if (cmd->argc > 1) {
		/* clear history */
		if (strcmp(cmd->argv[1], "-c") == 0) {
			clear_history();
			return 0;
		}

		/* exec command from history */
		char *end;
		long loffset;
		int offset;

		loffset = strtol(cmd->argv[1], &end, 10);
		if (end == cmd->argv[1]) {
			fprintf(stderr, "error: cannot convert to number\n");
			return 1;
		}

		offset = (int) loffset;
		if (offset > history_len) {
			fprintf(stderr, "error: offset > number of items\n");
			return 1;
		}

		/* parse execute command */
		char *line = strdup(history[offset]);

		if (line == NULL)
			return 1;

		struct commands *new_commands = parse_commands_with_pipes(line);

		/* set pointers so that these can be freed when
		 * child processes die during execution
		 */
		parent_cmd = cmd;
		temp_line = line;
		parent_cmds = cmds;

		exec_commands(new_commands);
		cleanup_commands(new_commands);
		free(line);

		/* reset */
		parent_cmd = NULL;
		temp_line = NULL;
		parent_cmds = NULL;

		return 0;
	}
	return 0;
}

/* Adds the user's input to the history. the implementation
 * is rather simplistic in that it uses memmove whenever
 * the number of items reaches max number of items.
 * For a few 100 items, this works well and easy to reason about
 */
int add_to_history(char *input)
{
	/* initialize on first call */
	if (history == NULL) {
		history = calloc(sizeof(char *) * HISTORY_MAXITEMS, 1);
		if (history == NULL) {
			fprintf(stderr, "error: memory alloc error\n");
			return 0;
		}
	}

	/* make a copy of the input */
	char *line;

	line = strdup(input);
	if (line == NULL)
		return 0;

	/* when max items have been reached, move the old
	 * contents to a previous position, and decrement len
	 */
	if (history_len == HISTORY_MAXITEMS) {
		free(history[0]);
		int space_to_move = sizeof(char *) * (HISTORY_MAXITEMS - 1);

		memmove(history, history+1, space_to_move);
		if (history == NULL) {
			fprintf(stderr, "error: memory alloc error\n");
			return 0;
		}

		history_len--;
	}

	history[history_len++] = line;
	return 1;
}

/**
   @brief Main entry point.
   @param argc Argument count.
   @param argv Argument vector.
   @return status code
 */
int main(int argc, char **argv)
{
  // Load config files, if any.

  // Run command loop.
  protosh_loop();

  // Perform any shutdown/cleanup.

  return EXIT_SUCCESS;
}
