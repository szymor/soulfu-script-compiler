#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "soulfu_script.h"

#define SRC_BUFFER_SIZE		(1 * 1024 * 1024)
#define RUN_BUFFER_SIZE		(64 * 1024)
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
int alloc_buffer(struct Buffer *buff, unsigned int size);
void free_buffer(struct Buffer *buff);

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
	if ('\0' == input_path[0])
	{
		printf("Error: no input file provided.\n");
		return EC_BADARGS;
	}
	if ('\0' == output_path[0])
	{
		// generate output filename from input filename
		char *base = strrchr(input_path, '/');
		base = base ? base + 1 : input_path;
		strcpy(output_path, base);

		base = strrchr(output_path, '.');
		if (base) *base = '\0';
		strcat(output_path, ".RUN");
	}

	printf("Input file: %s\n", input_path);
	printf("Output file: %s\n", output_path);

	struct Buffer src_buffer;
	alloc_buffer(&src_buffer, SRC_BUFFER_SIZE);
	struct Buffer run_buffer;
	alloc_buffer(&run_buffer, RUN_BUFFER_SIZE);

	FILE *input = fopen(input_path, "rb");
	if (NULL == input)
	{
		printf("Error: cannot open input file.\n");
		return EC_NOFILE;
	}
	fseek(input, 0, SEEK_END);
	src_buffer.used = ftell(input);
	fseek(input, 0, SEEK_SET);
	fread(src_buffer.mem, src_buffer.used, 1, input);
	fclose(input);

	src_headerize(&src_buffer, &run_buffer);

	FILE *output = fopen(output_path, "wb");
	fwrite(run_buffer.mem, run_buffer.used, 1, output);
	fclose(output);

	free_buffer(&src_buffer);
	free_buffer(&run_buffer);
}

int alloc_buffer(struct Buffer *buff, unsigned int size)
{
	buff->mem = malloc(size);
	if (!buff->mem)
		return -1;
	buff->max = size;
	buff->used = 0;
	return 0;
}

void free_buffer(struct Buffer *buff)
{
	if (buff->mem)
	{
		free(buff->mem);
		buff->mem = NULL;
		buff->used = 0;
		buff->max = 0;
	}
}
