#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "soulfu_script.h"

#define PATH_LEN			(256)

enum ErrCode
{
	EC_NONE,
	EC_NOACTION,
	EC_NOFILE,
	EC_BADARGS,
	EC_BADMAGIC
};

enum Action
{
	A_NONE,
	A_HELP,
	A_COMPILE,
};

char output_path[PATH_LEN] = "";
char input_path[PATH_LEN] = "";
enum Action action = A_NONE;

void help(void);
int compile(void);

int main(int argc, char *argv[])
{
	printf("SoulFu script compiler\n\n");

	for (int i = 1; i < argc; ++i)
	{
		if (!strcmp(argv[i], "-h"))
		{
			action = A_HELP;
		}
		else if (!strcmp(argv[i], "-c"))
		{
			action = A_COMPILE;
		}
		else if (!strcmp(argv[i], "-i"))
		{
			if ((i + 1) >= argc)
			{
				printf("Error: bad arguments.\n");
				return EC_BADARGS;
			}
			strcpy(input_path, argv[++i]);
		}
		else if (!strcmp(argv[i], "-o"))
		{
			if ((i + 1) >= argc)
			{
				printf("Error: bad arguments.\n");
				return EC_BADARGS;
			}
			strcpy(output_path, argv[++i]);
		}
	}

	switch (action)
	{
		case A_HELP:
			help();
			return EC_NONE;
		case A_COMPILE:
			return compile();
		case A_NONE:
		default:
			printf("Error: no action provided.\n");
			printf("Type %s -h to get a quick help.\n", argv[0]);
			return EC_NOACTION;
	}

	// never executed
	return EC_NONE;
}

void help(void)
{
	static const char contents[] = "Quick help:\n"
		"  -h           help\n"
		"  -c           compile\n"
		"  -i <path>    specifies input path\n"
		"  -o <path>    specifies output path\n";
	printf(contents);
}

int compile(void)
{
	if ('\0' == input_path[0] || '\0' == output_path[0])
	{
		printf("Error: no input or output file provided.\n");
		return EC_BADARGS;
	}

	src_headerize(input_path);
}
