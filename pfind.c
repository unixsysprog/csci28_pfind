/*
 * ==========================
 *   FILE: ./pfind.c
 * ==========================
 * Purpose: Search directories and subdirectories for files matching criteria.
 *
 * Main features: as seen in the function signatures below, the main features
 *		of pfind can be broken into the main logic, memory allocation, option
 *		processing, and helper functions to output error messages.
 *
 * Outline: pfind recursively searches, depth-first, through directories and
 *		any subdirectories it encounters, starting with a provided path.
 *		Results are filtered according to user-specified "-name" and/or
 *		"-type" options.
 *
 *
 * Data structures: construct_path() will malloc() a block of memory to store
 *		the full path of the current directory entry returned by the call to
 *		readdir(). If it is a directory, this is passed through to be
 *		recursively searched, otherwise, the char * is freed.
 */

 /* INCLUDES */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <fnmatch.h>

/* CONSTANTS */
#define NO	0
#define YES	1

/* MAIN LOGIC FUNCTIONS */
void searchdir(char *, char *, int);
void process_file(char *, char *, int);
void process_dir(char *, char *, int, DIR *);
int check_entry(char *, int, char *, char *, mode_t);
int recurse_directory(char *, mode_t);

/* MEMORY ALLOCATION */
char * construct_path(char *, char *);

/* OPTION PROCESSING FUNCTIONS */
void get_option(char **, char **, int *);
void get_path(char **, char **, char **, int *);
int get_type(char);

/* ERROR FUNCTIONS */
void file_error(char *);
void syntax_error();
void type_error(char *, char *);

/* FILE-SCOPE VARIABLES*/
static char *progname;			//used for error-reporting

/*
 * main()
 *  Method: Process command-line arguments, if any, and then call searchdir()
 *			to recursively search based on the starting path provided. If no
 *			path is provided, output error message with usage.
 *  Return: 0 on success, exits 1 and prints message to stderr on other
 *			failures (see corresponding functions for more info).
 *    Note: Options are processed with the help of two functions, get_path()
 *			and get_option(). The program exits if more than six command line
 *			arguments exist, as that would be a syntax error (1 program name,
 *			1 start path, max 2 args for -name, and max 2 args for -type.)
 *
 *			On invalid or missing arguments, these functions will print an
 *			error and exit(1). Option processing is done by going through
 *			char **av. On path processing, av is incremented once (av++)
 *			after the if/else block as the path is one argument. For options,
 *			av is incremented twice, once in get_option to cycle past the
 *			"-option" flag, and the av++ after the if/else block to iterate
 *			past the value for the option.
 */
int main (int ac, char **av)
{
	//variables set to default values for user options
	char *path = NULL;
	char *name = NULL;
	int type = 0;

	progname = *av++;							//initialize to program name

	if(ac > 6)
		syntax_error();							//syntax error, see above

	while (*av)									//process command-line args
	{
		if (!path)								//no starting_path given
			get_path(av, &path, &name, &type);	//exit(1) if not valid
		else									//check args are valid options
			get_option(av++, &name, &type);		//exit(1) if not valid

		av++;
	}

	if (path)									//if path was specified
		searchdir(path, name, type);			//perform find there
	else
		syntax_error();							//otherwise, syntax error

	return 0;
}

/*
 * searchdir()
 * Purpose: Recursively search a directory, filtering output based on
 *			optional findme and type parameters.
 *   Input: dirname, path of the current directory to search
 * 			findme, the pattern to look for/match against
 * 			type, the kind of file to search for
 *  Output: searchdir() calls on two helper functions -- process_file()
 *			and process_dir() -- to match a file/entries within a directory
 *			to the, optionally, specified criteria. If they match, those
 *			functions will print to stdout.
 *  Method: searchdir() initially tests to see if it can open a directory
 *			with name of "dirname". In this case, searchdir() next tries
 *			to see if the starting path specified by "dirname" is actually
 *			a file. Otherwise, it iterates recursively though all entries in
 *			the directory with help of process_dir().
 */
void searchdir(char *dirname, char *findme, int type)
{
	DIR* current_dir = opendir(dirname);		//attempt to open dir

	if ( current_dir == NULL )					//couldn't open dir
		process_file(dirname, findme, type);	//try using 'dirname' as file
	else
		process_dir(dirname, findme, type, current_dir);

	if(current_dir)
		closedir(current_dir);					//prevent memory leaks

	return;
}

/*
 *	process_file()
 *	Purpose: Check to see if "dirname" references a file instead of a dir.
 *	  Input: dirname, used as the name of a file
 * 			 findme, the pattern to look for/match against
 * 			 type, the kind of file to search for
 *	 Return: If "dirname" is a file that matches the criteria, the name
 *			 will be printed to stdout. In all other cases, the function
 *			 returns.
 *   Errors: If lstat() fails, file_error() is called to print to stderr.
 *			 See man page for lstat for kinds of possible errors. An error
 *			 also occurs if the "dirname" given is actually a directory.
 *			 See man page for opendir for kinds of possible errors. Most
 *			 common cases are EACCES and ENOENT errors.
 */
void process_file(char *dirname, char *findme, int type)
{
	struct stat info;

	//get stat on starting path "file"
	if (lstat(dirname, &info) == -1)
	{
		file_error(dirname);
		return;
	}

	//check to see if it dirname is actually a directory
	if(S_ISDIR(info.st_mode))
	{
		file_error(dirname);	//it was a dir, output errno from opendir()
		return;
	}

	//filter start path/file according to criteria
	if (check_entry(findme, type, dirname, dirname, info.st_mode))
		printf("%s\n", dirname);

	return;
}

/*
 *	process_dir()
 *	Purpose: Check all entries in an open directory and match against criteria
 *	  Input: dirname, path of the current directory to search
 * 			 findme, the pattern to look for/match against
 * 			 type, the kind of file to search for
 *			 search, pointer to the directory from call to opendir()
 *	 Return: For each directory entry read, if it matches the 'find' criteria
 *			 the full path to that entry will be printed to stdout.
 *   Errors: If lstat() has a problem reading the file at 'full_path', the
 *			 errno that lstat() generates will be output by calling the helper
 *			 function file_error().
 */
void process_dir(char *dirname, char *findme, int type, DIR *search)
{
	struct dirent *dp = NULL;			//pointer to directory entry
	struct stat info;					//file info
	char *full_path = NULL;				//store full path

	//read through entries
	while( (dp = readdir(search)) != NULL )
	{
		//turn parent/child into a single pathname
		full_path = construct_path(dirname, dp->d_name);

		if (lstat(full_path, &info) == -1)		//problem reading file
		{
			file_error(full_path);				//output errno
			continue;
		}

		//filter start path/file according to criteria
		if (check_entry(findme, type, dirname, dp->d_name, info.st_mode))
			printf("%s\n", full_path);

		//check if 'd_name' is dir and should recurse -- NO for '.' & '..'
		if ( recurse_directory(dp->d_name, info.st_mode) == YES )
			searchdir(full_path, findme, type);

		if(full_path != NULL)
			free(full_path);		//prevent memory leaks
	}

	return;
}

/*
 *	check_entry()
 *	Purpose: Compare the current file/directory entry again matching criteria
 *	  Input: findme, the pattern to look for/match against
 * 			 type, the kind of file to search for
 *			 dirname, the name of the current directory we are in
 *			 fname, the name of the current entry being checked
 *			 mode, the file type for "fname"
 *	 Return: NO, if matching criteria are specified and "fname" does not match
 *			 NO, if fname is the "." or ".." entry and the directory is not
 *				 a "." or ".."
 *			 YES, for all other cases
 */
int
check_entry(char *findme, int type, char *dirname, char *fname, mode_t mode)
{
	//check if name is specified and filter if no match
	if(findme && fnmatch(findme, fname, FNM_PERIOD) != 0)
		return NO;

	//check if type is specified and filter if no match
	if( (type != 0) && ((S_IFMT & mode) != (unsigned) type) )
		return NO;

	if (strcmp(fname, "..") == 0 && strcmp(fname, dirname) != 0)
		return NO;

	if (strcmp(fname, ".") == 0 && strcmp(fname, dirname) != 0)
		return NO;

	return YES;
}

/*
 * recurse_directory()
 * Purpose: check if the given directory entry is one we need to recurse
 *   Input: name, the current entry in the directory
 * 			mode, the current mode/type of file
 *  Return: NO, the file mode is not a directory. Or, the file is a directory
 * 				but it is either the current, ".", or parent, "..", entry.
 *			YES, the entry is a subdirectory that we should recursively search
 */
int recurse_directory(char *name, mode_t mode)
{
	//if the file isn't a directory, the current ".", or parent ".." dir
	if (! S_ISDIR(mode) || strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return NO;

	//all other cases, the file is a subdirectory to recursively search
	return YES;
}

/*
 *	get_option()
 *	Purpose: process command line options
 *	  Input: args, the array pointer to command-line arguments
 *			 name, pointer to store specified user
 *			 type, pointer to store specified file type
 *	 Return: None. This function passes through pointers from main
 *			 to store the variables.
 *	 Errors: If there is an invalid option (not -name or -type), missing value,
 *			 or an option has already been declared, type_error() is called
 *			 to output a message to stderr and exit with a non-zero status.
 *			 See also, errors above for invalid input.
 *	   Note: Each option that appears, must have a corresponding value.
 *			 The order the options appear in does not matter, but they can
 *			 only appear once.
 */
void get_option(char **args, char **name, int *type)
{
	char *option = *args++;				//store option, then point to next arg
	char *value = *args;				//store value for option (if any)

	//the name option, not previously declared
	if (strcmp(option, "-name") == 0 && (*name == NULL))
	{
		if( value )											//option exists
			*name = value;
		else
			type_error(option, value);						//missing arg
	}
	//the type option, not previously declared
	else if (strcmp(option, "-type") == 0 && *type == 0)
	{
		if (value)											//option exists
			*type = get_type(value[0]);
		else
			type_error(option, value);						//missing arg
	}
	//either an unknown predicate, or is repeat of -name or -type
	else
	{
		type_error(option, value);
	}

	return;
}

/*
 *	get_path()
 *	Purpose: Test the command line argument to see if it is a valid path.
 *	  Input: args, the array of command line arguments
 *			 path, variable to store the specified path in
 *			 name, variable to use as placeholder for out-of-order option
 *			 type, variable to use as placeholder for out-of-order option
 *	 Return: Prints message to stderr and exit(1) on out of order options
 *			 or invalid options.
 *	 Method: If the argument does not begin with an option specifier "-",
 *			 get_path() assumes it is a valid start path and assigns it to
 *			 the path variable. Otherwise, it attempts to recreate the
 *			 behavior in 'find' by processing args first and then outputting
 *			 a "paths must precede expression" error, or general syntax
 *			 error. Ex. 'find -name foobar .'
 */
void get_path(char **args, char **path, char **name, int *type)
{
	if(*args[0] != '-')				//arg DOESN'T begin with option specifier
		*path = *args;				//set path to the value
	else							//arg DOES begin with option specifier '-'
	{
		//process options and args first, a la 'find'
		while(*args && *args[0] == '-')
		{
			get_option(args, name, type);
			args += 2;
		}

		if(*args)						//assume remaining arg is start path
		{
			fprintf(stderr, "%s: paths must precede expression: ", progname);
			fprintf(stderr, "%s\n", *args);
			syntax_error();
		}
		else							//otherwise, general syntax error
		{
			syntax_error();
		}
	}

	return;
}

/*
 * get_type()
 * Purpose: check to see if -type is supported, and if so, provide bitmask
 *   Input: c, the char to check
 *  Return: the bitmask associated with the -type specified. If -type is
 *			not a valid option, print message to stderr and exit.
 * Options: the following options are allowed, as per <sys/stat.h>, the
 *			'find' command, and the homework spec: {b|c|d|f|l|p|s}. See
 *			inline comments below for the types they represent.
 */
int get_type(char c)
{
	switch (c) {
		case 'b':
			return S_IFBLK;		//block special
		case 'c':
			return S_IFCHR;		//character special
		case 'd':
			return S_IFDIR;		//directory
		case 'f':
			return S_IFREG;		//regular file
		case 'l':
			return S_IFLNK;		//symbolic link
		case 'p':
			return S_IFIFO;		//FIFO
		case 's':
			return S_IFSOCK;	//socket
		default:
			fprintf(stderr, "%s: ", progname);
            fprintf(stderr, "Unknown argument to -type: %c\n", c);
			exit(1);
	}
}

/*
 *	construct_path()
 *	Purpose: concatenate a parent and child into a full path name
 *	  Input: parent, current path to the open directory
 *			 child, name of the last entry read by readdir()
 *	 Return: pointer to full path string allocated by malloc()
 *   Errors: if malloc() or sprintf() fail, return NULL. This will
 *			 cause lstat() to output an error back in process_file or
 *			 process_dir().
 *   Method: Start by malloc()ing enough memory to store the combined
 *			 path. If sucessful, call sprintf() to copy into "newstr":
 *			 1) just the parent, if parent and child are the same;
 *			 2) if parent or child has trailing or leading '/',
 *			 	respectively, do not copy an extra '/'; or
 *			 3) concatenate "parent/child"
 */
char * construct_path(char *parent, char *child)
{
	int rv;
	int path_size = 1 + strlen(parent) + 1 + strlen(child);
	char *newstr = malloc(path_size);

	//Check malloc() returned memory. If no, lstat() will output error
	if (newstr == NULL)
		return NULL;

	//Concatenate "parent/child", see Method above for how
	if (strcmp(parent, child) == 0)
		rv = sprintf(newstr, "%s", parent);
	else if (parent[strlen(parent) - 1] == '/' || child[0] == '/')
		rv = sprintf(newstr, "%s%s", parent, child);
	else
		rv = sprintf(newstr, "%s/%s", parent, child);

	//check for sprintf error --or-- overflow error
	if ( rv < 0 || rv > (path_size - 1) )
	{
		free(newstr);		//failed to construct path
		return NULL;		//return will cause lstat() error
	}

	return newstr;
}

/*
 *	file_error()
 *	Purpose: Helper function to display error message.
 *	  Input: path, the full path of the file there was an error with
 *	 Return: Prints an error message with program name, the full path,
 *			 and the error message set by errno. After printing, the
 *			 function returns to continue to search through any remaining
 *			 files.
 */
void file_error(char *path)
{
	//example -- "./pfind: `/tmp/pft.IO8Et0': Permission denied"
	fprintf(stderr, "%s: `%s': %s\n", progname, path, strerror(errno));
	return;
}

/*
 *	syntax_error()
 *	Purpose: Helper function to display error message and exit.
 *	 Return: This function is called when a syntax error occurs. It
 *			 displays the message and exits with value of 1.
 */
void syntax_error()
{
	fprintf(stderr, "usage: pfind starting_path ");
	fprintf(stderr, "[-name filename-or-pattern] ");
	fprintf(stderr, "[-type {f|d|b|c|p|l|s}]\n");
	exit(1);
}

/*
 *	type_error()
 *	Purpose: Helper function to display error message for command-line options
 *	  Input: opt, the "-option" flag entered on the command line
 *			 value, the value proceeding the "-option" flag
 *	 Return: Appropriate error message printed to stderr, then exit(1).
 *  Example: "./pfind: missing argument to `-name'"
 */
void type_error(char *opt, char *value)
{
	//output program name
	fprintf(stderr, "%s: ", progname);

	//one of two accepted options, but previously declared/missing arg
	if(strcmp(opt, "-name") == 0 || strcmp(opt, "-type") == 0)
	{
		if(value)
			fprintf(stderr, "option already declared: `%s'\n", opt);
		else
			fprintf(stderr, "missing argument to `%s'\n", opt);
	}
	else
		fprintf(stderr, "unknown predicate `%s'\n", opt);

	exit(1);
}
