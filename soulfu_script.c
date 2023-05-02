#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "soulfu_script.h"

#define MAX_FILENAME_LEN			(16)

#ifndef FALSE
#    define FALSE 0
#endif

#ifndef TRUE
#    define TRUE 1
#endif

#define repeat(A, B) for(A=0;  A<B;  A++)

#define SRC_PERMANENT				(0)	// Define is permanent
#define SRC_TEMPORARY_FILE			(1)	// Define goes away when done with file
#define SRC_TEMPORARY_FUNCTION		(2)	// Define goes away when done with function

#define SRC_MAX_TOKEN              64   // The maximum number of pieces per line of a SRC file
#define SRC_MAX_DEFINE           2048   // The maximum number of defines
#define SRC_MAX_TOKEN_SIZE        128   // The maximum size of each piece

// For direct code->script calls...
#  define MAX_FAST_FUNCTION          16  // For predefined script functions like Spawn
#  define FAST_FUNCTION_SPAWN        0   // Offset for the Spawn() function
#  define FAST_FUNCTION_REFRESH      2   // Offset for the Refresh() function
#  define FAST_FUNCTION_EVENT        4   // Offset for the Event() function
#  define FAST_FUNCTION_AISCRIPT     6   // Offset for the AIScript() function
#  define FAST_FUNCTION_BUTTONEVENT  8   // Offset for the ButtonEvent() function
#  define FAST_FUNCTION_GETNAME      10  // Offset for the GetName() function
#  define FAST_FUNCTION_UNPRESSED    12  // Offset for the Unpressed() function
#  define FAST_FUNCTION_FRAMEEVENT   14  // Offset for the FrameEvent() function
#  define FAST_FUNCTION_MODELSETUP   16  // Offset for the ModelSetup() function...
#  define FAST_FUNCTION_DEFENSERATING 18 // Offset for the DefenseRating() function...
#  define FAST_FUNCTION_SETUP        20  // Offset for the Setup() function...
#  define FAST_FUNCTION_DIRECTUSAGE  22  // Offset for the DirectUsage() function...
#  define FAST_FUNCTION_ENCHANTUSAGE 24  // Offset for the EnchantUsage() function...

#define RUN_BUFFER_SIZE	(65550)  //16384 extra big so run_buffer_used doesn't write other stuff...              // Size of the RUN file...
#define SRC_BUFFER_SIZE	(1 * 1024 * 1024)	// for SRC file

enum SSError
{
	SSE_NONE,
	SSE_NOFILE,
	SSE_EXTERNAL,
	SSE_HEADERIZE
};

enum SSError error = SSE_NONE;

unsigned char run_buffer[RUN_BUFFER_SIZE];  // Stick the RUN file here while building it...
unsigned short run_buffer_used = 0;         // Current size

char src_buff[SRC_BUFFER_SIZE];
unsigned int src_buff_used = 0;

char define_token[SRC_MAX_DEFINE][SRC_MAX_TOKEN_SIZE];  // ex. "TRUE"
char define_value[SRC_MAX_DEFINE][SRC_MAX_TOKEN_SIZE];  // ex. "1"
char define_temporary_level[SRC_MAX_DEFINE];            // 0 is global, 1 is file, 2 is function

int src_num_define = 0;         // The number of defined values...  May be higher than actual...

char *sdf_read_file = NULL;     // A pointer to the current read position for sdf_open
int sdf_read_first_line;        // FALSE until sdf_read_line has been called...
int sdf_read_line_number = 0;   // The current line number
int sdf_read_remaining;         // The number of bytes left to read

unsigned char token_buffer[SRC_MAX_TOKEN][SRC_MAX_TOKEN_SIZE];   // A place to put the pieces
signed char next_token_may_be_negative;     // For reading -5 as negative 5 instead of minus 5

void src_undefine_level(char temporary_level);
float sdf_read_float(unsigned char* location);
unsigned int sdf_read_unsigned_int(unsigned char* location);
void sdf_write_unsigned_int(unsigned char* location, unsigned int value);
unsigned short sdf_read_unsigned_short(unsigned char* location);
void sdf_write_unsigned_short(unsigned char* location, unsigned short value);
signed char sdf_read_line(void);
signed char src_read_token(unsigned char* buffer);
int count_indentation(char *string);
void src_add_define(char* token, char* value, char temporary_level);
int src_get_define(char* token);
void make_uppercase(char *string);

signed char src_headerize(const char *filename)
{
	// <ZZ> This function runs through an SRC file that has been stored in memory and finds
	//      function information to store in the header.  This must be done for every file
	//      before compilation can occur.  Index is a pointer to the start of the file's
	//      index in sdf_index, and can be gotten from sdf_find_index.  If the function
	//      works okay, it should create a new RUN file in the index and return TRUE.  If
	//      it fails it should return FALSE.

	FILE *file = NULL;
	unsigned short number_of_functions = 0;
	unsigned short keepgoing;
	char returncode;
	unsigned char indent;
	unsigned short number_of_strings;

	unsigned char* data;    // Original, pre-existing RUN file
	unsigned char* newdata; // New header
	unsigned int newsize;   // New header

	int length;
	int token_count;
	char read_char;
	int def;

	printf("Generating header file for %s\n", filename);

	// Undefine any variables and defines used by the last file
	src_undefine_level(SRC_TEMPORARY_FILE);

	file = fopen(filename, "r");
	if (NULL == file)
	{
		error = SSE_NOFILE;
		goto src_headerize_error_handle;
	}
	fseek(file, 0, SEEK_END);
	src_buff_used = ftell(file);
	fseek(file, 0, SEEK_SET);
	fread(src_buff, src_buff_used, 1, file);
	fclose(file);

    // Just like sdf_open...
    sdf_read_first_line = FALSE;
    sdf_read_remaining = src_buff_used;
    sdf_read_file = (unsigned char *)src_buff;
    sdf_read_line_number = 0;

    next_token_may_be_negative = TRUE;

	// Pad the start of the header with fast function offsets...
	run_buffer_used = 0;
	for (int i = 0; i < MAX_FAST_FUNCTION; ++i)
	{
		// 2 bytes each...
		run_buffer[run_buffer_used] = 0;
		run_buffer_used++;
		run_buffer[run_buffer_used] = 0;
		run_buffer_used++;
	}
	// Something like this in the run_buffer...
	//   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00


    // Make a list of all the functions in this file...  Figure out offsets in another pass...
    run_buffer_used += 4;  // Skip a word for the string count...  Filled in after counting...
	printf("Headerizing functions...\n");
    number_of_functions = 0;
    while (sdf_read_line())
    {
        // Count the indentation (by 2's), and skip over any whitespace...
        indent = count_indentation(sdf_read_file);
        sdf_read_file += indent;
        sdf_read_remaining -= indent;
        indent = indent >> 1;

        // Skip #defines, we're only interested in functions...
        if(sdf_read_file[0] != '#' && indent == 0)
        {
            // Read all of the tokens on this line...
            token_count = 0;
            keepgoing = TRUE;
            while (keepgoing)
            {
                keepgoing = FALSE;
                if (token_count < SRC_MAX_TOKEN)
                {
                    if (src_read_token(token_buffer[token_count]))
                    {
						//printf("Line %d, %s\n", sdf_read_line_number, token_buffer[token_count]);
                        token_count++;
                        keepgoing = TRUE;
                    }
                }
            }

            // Did we read anything?
            if (token_count > 0)
            {
                // Get the return type for the function...
                int i = 0;
                returncode = 'I';
                if (strcmp(token_buffer[0], "INT") == 0)  { returncode = 'I';  i++; }
                if (strcmp(token_buffer[0], "FLOAT") == 0)  { returncode = 'F';  i++; }

                // Make sure the function has parentheses after it
                if (token_buffer[i+1][0] == '(')
                {
                    // Now stick the function address and name in our RUN file...
                    sprintf((run_buffer + run_buffer_used), "%c%c%s%c", 0, 0, token_buffer[i], 0);
                    run_buffer_used += strlen(token_buffer[i]) + 3;
                    printf("function...  %s()\n", token_buffer[i]);

                    // Now store the return code type...
                    sprintf((run_buffer + run_buffer_used), "%c", returncode);
                    run_buffer_used++;

                    // Now store any arguments types...
                    i = i + 2;
                    while (i < token_count)
                    {
                        if (strcmp(token_buffer[i], "INT") == 0)
                        {
                            sprintf((run_buffer + run_buffer_used), "I");
                            run_buffer_used++;
                        }
                        else if (strcmp(token_buffer[i], "FLOAT") == 0)
                        {
                            sprintf((run_buffer + run_buffer_used), "F");
                            run_buffer_used++;
                        }
                        i++;
                    }


                    // And zero terminate the string...
                    sprintf((run_buffer + run_buffer_used), "%c", 0);
                    run_buffer_used++;
                    number_of_functions++;
                }
                else
                {
                    printf("Error: %s(line %d), function symbol '%s' not followed by ()\n", filename, sdf_read_line_number, token_buffer[i]);
                    error = SSE_HEADERIZE;
                    goto src_headerize_error_handle;
                }
            }
        }
    }
	sdf_write_unsigned_short(run_buffer +- (MAX_FAST_FUNCTION << 1), number_of_functions);
	// Something like this in the run_buffer...
	//   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
	//   00 03 00 00
	//   0  0  D  E  A  T  H  00 I  F  F  F  I  00
	//   0  0  B  L  A  H  00 V  00
	//   0  0  A  T  T  A  C  K  00 V  I  00
	//   Address     FunctionName      ReturnAndArgumentTypes

	// Check for errors too...
	if (number_of_functions == 0)
	{
		printf("Error: %s: No functions found\n", filename);
		error = SSE_HEADERIZE;
		goto src_headerize_error_handle;
	}

	// Now search for any #defines...  Just in case someone #define'd a string...
	printf("Headerizing #define's...\n");

    sdf_read_first_line = FALSE;
    sdf_read_remaining = src_buff_used;
    sdf_read_file = (unsigned char *)src_buff;
    sdf_read_line_number = 0;

	next_token_may_be_negative = TRUE;

	// Make a list of all the functions in this file...  Figure out offsets in another pass...
	while (sdf_read_line())
	{
		// Count the indentation (by 2's), and skip over any whitespace...
		indent = count_indentation(sdf_read_file);
		sdf_read_file += indent;
		sdf_read_remaining -= indent;
		indent = indent >> 1;

		// Check for a #define...
		if (sdf_read_file[0] == '#')
		{
			if (sdf_read_file[1] == 'D')
			{
				if (sdf_read_file[2] == 'E')
				{
					if (sdf_read_file[3] == 'F')
					{
						if (sdf_read_file[4] == 'I')
						{
							if (sdf_read_file[5] == 'N')
							{
								if (sdf_read_file[6] == 'E')
								{
									sdf_read_file+=7;
									sdf_read_remaining-=7;
									if (src_read_token(token_buffer[0]))
									{
										src_add_define(token_buffer[0], sdf_read_file, SRC_TEMPORARY_FILE);
									}
								}
							}
						}
					}
				}
			}
		}
	}

	// Now search for strings to append to the header...  Just like above...
    sdf_read_first_line = FALSE;
    sdf_read_remaining = src_buff_used;
    sdf_read_file = (unsigned char *)src_buff;
    sdf_read_line_number = 0;

	next_token_may_be_negative = TRUE;

	// Make a list of all the functions in this file...  Figure out offsets in another pass...
	printf("Headerizing strings...\n");
	number_of_strings = 0;
	while (sdf_read_line())
	{
		// Count the indentation (by 2's), and skip over any whitespace...
		indent = count_indentation(sdf_read_file);
		sdf_read_file += indent;
		sdf_read_remaining -= indent;
		indent = indent >> 1;

		// Skip #defines, we're only interested in strings...
		if (sdf_read_file[0] != '#' && indent != 0)
		{
			// Read all of the tokens on this line...
			token_count = 0;
			keepgoing = TRUE;
			while (keepgoing)
			{
				keepgoing = FALSE;
				if (token_count < SRC_MAX_TOKEN)
				{
					if (src_read_token(token_buffer[token_count]))
					{
						token_count++;
						keepgoing = TRUE;
					}
				}
			}


			// Look through all of the tokens for a string token...
			for (int i = 0; i < token_count; ++i)
			{
				// Look for string tokens...
				//printf("Token %s\n", token_buffer[i]);

				if(token_buffer[i][0] == '"')
				{
					// Found a string, so put it in the header
					sdf_write_unsigned_short(run_buffer+run_buffer_used, 65535);
					run_buffer_used += 2;
					length = strlen(token_buffer[i]+1)-1;
					for (int j = 0; j < length; ++j)
					{
						*(run_buffer + run_buffer_used) = token_buffer[i][j+1];
						run_buffer_used++;
					}
					*(run_buffer + run_buffer_used) = 0;
					run_buffer_used++;
					number_of_strings++;
				}
				else
				{
					// Check for #define'd strings...
					def = src_get_define(token_buffer[i]);
					if(def >= 0)
					{
						printf("Sneaky define... %s\n", define_token[def]);

						// It was a sneaky #define...  Search for " characters...
						read_char = define_value[def][0];
						int k = 0;
						while (read_char != 0)
						{
							if (read_char == '"')
							{
								// Start of a string, so write it into the header...
								sdf_write_unsigned_short(run_buffer + run_buffer_used, 65535);
								run_buffer_used += 2;
								k++;
								read_char = define_value[def][k];
								while(read_char != 0 && read_char != '"')
								{
									*(run_buffer + run_buffer_used) = read_char;
									run_buffer_used++;
									k++;
									read_char = define_value[def][k];
								}
								*(run_buffer + run_buffer_used) = 0;
								run_buffer_used++;
								number_of_strings++;
								if(read_char == 0) k--;
							}
							k++;
							read_char = define_value[def][k];
						}
					}
				}
			}
		}
	}
	sdf_write_unsigned_short(run_buffer+(MAX_FAST_FUNCTION<<1)+2, number_of_strings);
	// Something like this in the run_buffer...
	//   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
	//   00 03 00 02
	//   0  0  D  E  A  T  H  00 I  F  F  F  I  00
	//   0  0  B  L  A  H  00 V  00
	//   0  0  A  T  T  A  C  K  00 V  I  00
	//   0  0  B  i  g     B  o  b     i  s     c  o  o  l  00
	//   FF FF T  e  s  t     s  t  r  i  n  g  00
	//   FF FF F  I  L  E  :  T  E  S  T  .  T  X  T  00
	//   String address           String


	// Now copy the header data into a (temporary) RUN file
	// Copy the header information to the new location...
	// to do - rework
	/*
	printf("Dump:\n");
	for (int j = 0; j < run_buffer_used; ++j)
	{
		printf("%02x ", run_buffer[j]);
		if (j != 0 && (j % 32) == 0)
			printf("\n");
	}*/

	return error;
src_headerize_error_handle:
	// Had an error, so delete any existing file... - to do
	return error;
}

void src_undefine_level(char temporary_level)
{
    // <ZZ> This function gets rid of any defines that we don't need
    for (int i; i < src_num_define; ++i)
    {
        if(define_temporary_level[i] >= temporary_level)
        {
            define_token[i][0] = 0;
            define_value[i][0] = 0;
            define_temporary_level[i] = 0;
        }
    }
}

float sdf_read_float(unsigned char* location)
{
    // <ZZ> This function reads an float value at the given memory location, assuming that
    //      the memory is stored in big endian format.  The function returns the value it
    //      read.
    unsigned int value;
// !!!BAD!!!
// !!!BAD!!!  This might not work...
// !!!BAD!!!
    value  = *location;     value = value<<8;
    value |= *(location+1); value = value<<8;
    value |= *(location+2); value = value<<8;
    value |= *(location+3);
    return *((float*) &value);
}

unsigned int sdf_read_unsigned_int(unsigned char* location)
{
    // <ZZ> This function reads an unsigned integer at the given memory location, assuming that
    //      the memory is stored in big endian format.  The function returns the value it
    //      read.
    unsigned int value;
    value  = *location;     value = value<<8;
    value |= *(location+1); value = value<<8;
    value |= *(location+2); value = value<<8;
    value |= *(location+3);
    return value;
}

void sdf_write_unsigned_int(unsigned char* location, unsigned int value)
{
    // <ZZ> This function writes an unsigned integer to the given memory location, assuming that
    //      the memory is stored in big endian format.
    *(location+3) = (unsigned char) (value & 255);  value = value>>8;
    *(location+2) = (unsigned char) (value & 255);  value = value>>8;
    *(location+1) = (unsigned char) (value & 255);  value = value>>8;
    *(location)   = (unsigned char) (value & 255);
}

unsigned short sdf_read_unsigned_short(unsigned char* location)
{
    // <ZZ> This function reads an unsigned short at the given memory location, assuming that
    //      the memory is stored in big endian format.  The function returns the value it
    //      read.
    unsigned short value;
    value  = *location;    value = value<<8;
    value |= *(location+1);
    return value;
}

void sdf_write_unsigned_short(unsigned char* location, unsigned short value)
{
    // <ZZ> This function writes an unsigned short to the given memory location, assuming that
    //      the memory is stored in big endian format.
    *(location+1) = (unsigned char) (value & 255);  value = value>>8;
    *(location)   = (unsigned char) (value & 255);
}

int count_indentation(char *string)
{
    // <ZZ> This function returns the number of spaces at the start of string
    int count;

    count = 0;
    while(string[count] == ' ')
    {
        count++;
    }
    return count;
}

signed char src_read_token(unsigned char* buffer)
{
    // <ZZ> This function reads the next token (like a word) in a file that has been
    //      sdf_open()'d and puts that token in buffer.  The token can't be any longer than
    //      SRC_MAX_TOKEN_SIZE.  It returns TRUE if there was a token to read, or FALSE if
    //      not.  next_token_may_be_negative is also important for reading negative numbers
    //      correctly.
    int count;
    int value;


    // Skip any whitespace...
    while((*sdf_read_file) == ' ' && sdf_read_remaining > 0)
    {
        sdf_read_remaining--;
        sdf_read_file++;
    }


    // Check for a string token...
    count = 0;
    if((*sdf_read_file) == '\'' && (*(sdf_read_file+2)) == '\'')
    {
        sprintf(buffer, "%d", *(sdf_read_file+1));
        sdf_read_remaining-=3;
        sdf_read_file+=3;
        count=3;
    }
    else if((*sdf_read_file) == '"')
    {
        // Read the string token
        buffer[count] = '"';
        sdf_read_remaining--;
        sdf_read_file++;
        count++;
        while(count < SRC_MAX_TOKEN_SIZE-2 && sdf_read_remaining > 0 && (*sdf_read_file) != '"' && (*sdf_read_file) != 0)
        {
            buffer[count] = (*sdf_read_file);
            sdf_read_remaining--;
            sdf_read_file++;
            if(buffer[count] == '\\' && (*sdf_read_file) >= '0' && (*sdf_read_file) <= '9')
            {
                // It's an escape sequence, so read in the value...
                value = 0;
                while((*sdf_read_file) >= '0' && (*sdf_read_file) <= '9')
                {
                    value = (value*10) + (*sdf_read_file) - '0';
                    sdf_read_remaining--;
                    sdf_read_file++;
                }
                buffer[count] = value;
            }
            count++;
        }
        buffer[count] = '"';
        sdf_read_remaining--;
        sdf_read_file++;
        count++;
        buffer[count] = 0;
        next_token_may_be_negative = FALSE;
    }
    else
    {
        // Is the token alphanumeric or a special operator token???
        if((((*sdf_read_file) >= 'A' && (*sdf_read_file) <= 'Z') || ((*sdf_read_file) >= 'a' && (*sdf_read_file) <= 'z') || (*sdf_read_file) == '_' || (*sdf_read_file) == '.' || ((*sdf_read_file) >= '0' && (*sdf_read_file) <= '9') ))
        {
            // Read the alphanumeric token
            while(count < SRC_MAX_TOKEN_SIZE-1 && sdf_read_remaining > 0 && ((((*sdf_read_file) >= 'A' && (*sdf_read_file) <= 'Z') || ((*sdf_read_file) >= 'a' && (*sdf_read_file) <= 'z') || (*sdf_read_file) == '_' || (*sdf_read_file) == '.' || ((*sdf_read_file) >= '0' && (*sdf_read_file) <= '9') )))
            {
                buffer[count] = (*sdf_read_file);
                sdf_read_remaining--;
                sdf_read_file++;
                count++;
            }
            buffer[count] = 0;
            make_uppercase(buffer);
            next_token_may_be_negative = FALSE;
        }
        else
        {
            // Read one character...
            if((*sdf_read_file) != 0)
            {
                if(sdf_read_remaining > 0)
                {
                    buffer[count] = (*sdf_read_file);
                    sdf_read_remaining--;
                    sdf_read_file++;
                    count++;
                }


                // Do we need another?
                if((buffer[0] == '>' && (*sdf_read_file) == '>') ||
                   (buffer[0] == '<' && (*sdf_read_file) == '<') ||
                   (buffer[0] == '&' && (*sdf_read_file) == '&') ||
                   (buffer[0] == '|' && (*sdf_read_file) == '|') ||
                   (buffer[0] == '>' && (*sdf_read_file) == '=') ||
                   (buffer[0] == '<' && (*sdf_read_file) == '=') ||
                   (buffer[0] == '=' && (*sdf_read_file) == '=') ||
                   (buffer[0] == '+' && (*sdf_read_file) == '+') ||
                   (buffer[0] == '-' && (*sdf_read_file) == '-') ||
                   (buffer[0] == '/' && (*sdf_read_file) == '/') ||
                   (buffer[0] == '!' && (*sdf_read_file) == '='))
                {
                    buffer[count] = (*sdf_read_file);
                    sdf_read_remaining--;
                    sdf_read_file++;
                    count++;
                }


                // Ignore comments...
                if(buffer[0] == '/' && buffer[1] == '/') return FALSE;



                // May need to read a whole number in...  Negative numbers...
                if(next_token_may_be_negative)
                {
                    if(buffer[0] == '-' && (*sdf_read_file) >= '0' && (*sdf_read_file) <= '9')
                    {
                        // Read the numeric token...
                        while(count < SRC_MAX_TOKEN_SIZE-1 && sdf_read_remaining > 0 && (((*sdf_read_file) == '.' || ((*sdf_read_file) >= '0' && (*sdf_read_file) <= '9'))))
                        {
                            buffer[count] = (*sdf_read_file);
                            sdf_read_remaining--;
                            sdf_read_file++;
                            count++;
                        }
                        next_token_may_be_negative = FALSE;
                    }
                }
                else
                {
                    next_token_may_be_negative = TRUE;
                }
                buffer[count] = 0;



                // Figure out if the next one can be negative...
                if(strcmp(buffer, ")") == 0) next_token_may_be_negative = FALSE;
                if(strcmp(buffer, "--") == 0) next_token_may_be_negative = FALSE;
                if(strcmp(buffer, "++") == 0) next_token_may_be_negative = FALSE;
                if(strcmp(buffer, "%") == 0) next_token_may_be_negative = FALSE;
            }
        }
    }

    if(count > 0) return TRUE;
    return FALSE;
}

signed char sdf_read_line(void)
{
    // <ZZ> This function places sdf_read_file at the start of the next good line in a file
    //      opened by sdf_open().  If something goes wrong, it returns FALSE.  If something
    //      goes right, it returns TRUE.


    // Make sure we have an open file
    if(sdf_read_file)
    {
        // Don't advance the read head on the first call...
        if(sdf_read_first_line == FALSE)
        {
            sdf_read_first_line = TRUE;
        }
        else
        {
            // Skip over the line until we hit a 0
            while((*sdf_read_file) != 0 && sdf_read_remaining > 0)
            {
                sdf_read_remaining--;
                sdf_read_file++;
            }
        }


        // Skip any 0's, so sdf_read_file starts on some good characters...
        while((*sdf_read_file) == 0 && sdf_read_remaining > 0)
        {
            sdf_read_remaining--;
            sdf_read_file++;
            sdf_read_line_number++;
        }


        // Make sure we didn't run out of data...
        if(sdf_read_remaining) return TRUE;
    }
    return FALSE;
}

void src_add_define(char* token, char* value, char temporary_level)
{
    // <ZZ> This function registers a new #define'd value.  Temporary_level determines if the
    //      define is removed after a file or function is done being parsed.
    char temptoken[SRC_MAX_TOKEN_SIZE];
    unsigned char* tempptr;
    int tempint;
    int tempnum;
    int i;
    int j;
    int found;
    int define_to_use;
    char define_is_new;


    // Look for an unused define to use...
    next_token_may_be_negative = TRUE;
    define_is_new = TRUE;
    define_to_use = src_num_define;
    repeat(i, src_num_define)
    {
        // Has this one been cleared?
        if(define_token[i][0] == 0)
        {
            define_to_use = i;
            define_is_new = FALSE;
            i = src_num_define;
        }
    }



    // Fill in the data
    if(define_to_use < SRC_MAX_DEFINE)
    {
        // Skip whitespace...
        while((*token) == ' ')  token++;
        while((*value) == ' ')  value++;



        // Copy the token...
        i = 0;
        while(i < SRC_MAX_TOKEN_SIZE-1 && token[i] != 0)
        {
            define_token[define_to_use][i] = token[i];
            i++;
        }
        define_token[define_to_use][i] = 0;
        define_temporary_level[define_to_use] = temporary_level;



        // Now search through the value, looking for previously #defined tokens...
        tempptr = sdf_read_file;
        tempint = sdf_read_remaining;
        tempnum = sdf_read_line_number;
        sdf_read_file = value;
        sdf_read_remaining = SRC_MAX_TOKEN_SIZE;


        i = 0;
        while(src_read_token(temptoken))
        {
            found = src_get_define(temptoken);
            if(found > -1)
            {
                // Copy the value of the old token into the new one...
                j = 0;
                while(define_value[found][j] != 0 && i < SRC_MAX_TOKEN_SIZE-1)
                {
                    define_value[define_to_use][i] = define_value[found][j];
                    i++;
                    j++;
                }
                if(i < SRC_MAX_TOKEN_SIZE-1)
                {
                    define_value[define_to_use][i] = ' ';
                    i++;
                }
            }
            else
            {
                // Copy the token into the new define...
                j = 0;
                while(temptoken[j] != 0 && i < SRC_MAX_TOKEN_SIZE-1)
                {
                    define_value[define_to_use][i] = temptoken[j];
                    i++;
                    j++;
                }
                if(i < SRC_MAX_TOKEN_SIZE-1)
                {
                    define_value[define_to_use][i] = ' ';
                    i++;
                }
            }
        }
        if(i < SRC_MAX_TOKEN_SIZE)
        {
            define_value[define_to_use][i] = 0;
            i--;
        }
        if(i >= 0)
        {
            if(define_value[define_to_use][i] == ' ')
            {
                define_value[define_to_use][i] = 0;
            }
        }
        sdf_read_line_number = tempnum;
        sdf_read_remaining = tempint;
        sdf_read_file = tempptr;


        printf("INFO:         Defined...  %s == %s\n", define_token[define_to_use], define_value[define_to_use]);


        // Increment the number of defines if we added a new one
        if(define_is_new) src_num_define++;
    }
    else
    {
        printf("Error:  Couldn't define %s...  Out of tokens to use...\n", token);
        error = SSE_EXTERNAL;
    }
}

int src_get_define(char* token)
{
    // <ZZ> This function returns the index of the first #define that matches token.  If there
    //      aren't any matches, it returns -1.

    // Check each define...
    for (int i = 0; i < src_num_define; ++i)
    {
        if(strcmp(token, define_token[i]) == 0) return i;
    }
    return -1;
}

void make_uppercase(char *string)
{
    // <ZZ> This function changes all lowercase letters in string to be uppercase.
    int i;

    i = 0;
    while(string[i] != 0)
    {
        if(string[i] >= 'a' && string[i] <= 'z')  string[i] += 'A'-'a';
        i++;
    }
}
