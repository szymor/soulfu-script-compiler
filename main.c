#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "soulfu_script.h"

#define DEFAULT_SDF_DIR		"sfd"
#define SRC_BUFFER_SIZE		(1 * 1024 * 1024)
#define RUN_BUFFER_SIZE		(64 * 1024)
#define PATH_LEN			(256)

enum ErrCode
{
	EC_NONE,
	EC_NOACTION,
	EC_NODIR,
	EC_BADARGS
};

enum Action
{
	A_NONE,
	A_HELP,
	A_COMPILE,
};

char input_path[PATH_LEN] = DEFAULT_SDF_DIR;
char output_path[PATH_LEN] = "";
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

char *get_output_filename(char *input_filename)
{
	static char outbuff[PATH_LEN];
	// generate output filename from input filename
	char *base = strrchr(input_filename, '/');
	base = base ? base + 1 : input_filename;
	strcpy(outbuff, base);

	base = strrchr(outbuff, '.');
	if (base) *base = '\0';
	strcat(outbuff, ".RUN");
	return outbuff;
}

int read_file_to_buffer(char *filename, struct Buffer *buff)
{
	FILE *input = fopen(filename, "rb");
	if (NULL == input)
	{
		return -1;
	}
	fseek(input, 0, SEEK_END);
	buff->used = ftell(input);
	fseek(input, 0, SEEK_SET);
	fread(buff->mem, buff->used, 1, input);
	fclose(input);
	return 0;
}

int write_buffer_to_file(char *filename, const struct Buffer *buff)
{
	FILE *output = fopen(filename, "wb");
	fwrite(buff->mem, buff->used, 1, output);
	fclose(output);
}

int compile(void)
{
	if ('\0' == output_path[0])
	{
		strcpy(output_path, input_path);
	}
	printf("Input path: %s\n", input_path);
	printf("Output path: %s\n", output_path);

	struct Buffer src_buffer;
	alloc_buffer(&src_buffer, SRC_BUFFER_SIZE);
	struct Buffer run_buffer;
	alloc_buffer(&run_buffer, RUN_BUFFER_SIZE);

	DIR *dirp = opendir(input_path);
	if (NULL == dirp)
	{
		printf("Error: data directory not found.\n");
		return EC_NODIR;
	}

#ifdef __MINGW32__
	mkdir(output_path);
#else
	mkdir(output_path, 0755);
#endif

	printf("Headerizing...\n");
	struct dirent *entry = NULL;
	while (entry = readdir(dirp))
	{
		if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
		{
			if (strstr(entry->d_name, ".SRC"))
			{
				char inpath[PATH_LEN];
				sprintf(inpath, "%s/%s", input_path, entry->d_name);
				read_file_to_buffer(inpath, &src_buffer);
				if (SSE_NONE != src_headerize(&src_buffer, &run_buffer))
				{
					printf("Headerizing failed for %s.\n", inpath);
				}

				char outpath[PATH_LEN];
				sprintf(outpath, "%s/%s", output_path, get_output_filename(entry->d_name));
				write_buffer_to_file(outpath, &run_buffer);
			}
		}
	}
	closedir(dirp);
	printf("Headerizing done.\n");

	// compilerizing needs header data from all SRC files
	// look at src_mega_find_function for details
	//src_compilerize(&src_buffer, &run_buffer);

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
