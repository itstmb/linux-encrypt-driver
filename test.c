#define _GNU_SOURCE
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "encdec.h"

#define READ_BUFFER_SIZE 1000
#define CMD_BUFFER_SIZE 50
#define MAX_FD_COUNT 10

char* delimiters = " \n\r\t";
char* string_delimiter = "\"";
char command[CMD_BUFFER_SIZE];
int fds[MAX_FD_COUNT];
char read_buffer[READ_BUFFER_SIZE];
int read_cmd = 0;

char** parse_command(char* command, int* args_count_out)
{
    char **args = NULL;
    int args_count = 0;

    char* save_ptr_token;
    char* save_ptr_even_token;

    int even = 1;
    char* token = strtok_r(command, string_delimiter, &save_ptr_token);
    while (token != NULL)
    {
    	if(even)
    	{
		    char* even_token = strtok_r(token, delimiters, &save_ptr_even_token);
		    while (even_token != NULL)
		    {
				args_count++;
				args = (char**)realloc(args, args_count * sizeof(char*));
				args[args_count - 1] = malloc(sizeof(char) * strlen(even_token));
				strcpy(args[args_count - 1], even_token);
				even_token = strtok_r(NULL, delimiters, &save_ptr_even_token);
		    }    		

    	}
    	else
    	{
  	        args_count++;
	        args = (char**)realloc(args, args_count * sizeof(char*));
	        args[args_count - 1] = malloc(sizeof(char) * strlen(token));
	        strcpy(args[args_count - 1], token);		
    	}

    	token = strtok_r(NULL, string_delimiter, &save_ptr_token);
    	even = (even + 1) % 2;
    }

    *args_count_out = args_count;
    return args;
}

int execute_command(char** args, int args_count)
{
	read_cmd = 0;
	if(args_count > 0)
	{
		if(strcmp(args[0], "open") == 0)
		{
			int device = atoi(args[1]);
			int fd_index = atoi(args[2]);
			int flags;
			char* path;

			if(strcmp(args[3], "read") == 0)
			{
				flags = O_RDONLY;
			}
			else if(strcmp(args[3], "write") == 0)
			{
				flags = O_WRONLY;	
			}
			else if(strcmp(args[3], "read|write") == 0)
			{
				flags = O_RDWR;			
			}

			if(device == 0)
			{
				path = "/dev/encdec0";
			}
			else if(device == 1)
			{
				path = "/dev/encdec1";
			}

			int result = open(path, flags);
			if(result < 0)
			{
				return result;
			}

			fds[fd_index] = result;
			return 0;
		}
		else if(strcmp(args[0], "close") == 0)
		{
			int fd_index = atoi(args[1]);
			int result = close(fds[fd_index]);
			if(result < 0)
			{
				return result;
			}

			fds[fd_index] = -1;

			return 0;
		}
		else if(strcmp(args[0], "ioctl") == 0)
		{
			int fd_index = atoi(args[1]);
			int cmd_type;
			int cmd_arg;

			if(strcmp(args[2], "change_key") == 0)
			{
				cmd_type = ENCDEC_CMD_CHANGE_KEY;
				cmd_arg = atoi(args[3]);
			}
			else if(strcmp(args[2], "change_read_state") == 0)
			{
				cmd_type = ENCDEC_CMD_SET_READ_STATE;
				if(strcmp(args[3], "raw") == 0)
				{
					cmd_arg = ENCDEC_READ_STATE_RAW;
				}
				else if(strcmp(args[3], "decrypt") == 0)
				{
					cmd_arg = ENCDEC_READ_STATE_DECRYPT;
				}
			}
			else if(strcmp(args[2], "zero") == 0)
			{
				cmd_type = ENCDEC_CMD_ZERO;
				cmd_arg = 0;
			}			

			return ioctl(fds[fd_index], cmd_type, cmd_arg);
		}
		else if(strcmp(args[0], "lseek") == 0)
		{
			int fd_index = atoi(args[1]);
			int pos = atoi(args[2]);

			return lseek(fds[fd_index], pos, SEEK_SET);
		}		
		else if(strcmp(args[0], "read") == 0)
		{
			int fd_index = atoi(args[1]);
			int count = atoi(args[2]);

			read_cmd = 1;
			memset(read_buffer, 0, READ_BUFFER_SIZE);
			return read(fds[fd_index], read_buffer, count);
		}		
		else if(strcmp(args[0], "write") == 0)
		{
			int fd_index = atoi(args[1]);
			char* buffer = args[2];
			return write(fds[fd_index], buffer, strlen(buffer));
		}	
	}

	return 0;
}

void free_parsed_command(char** args, int args_count)
{
    for(int i = 0; i < args_count; i++)
    {
        if(args[i] != NULL)
        {
            free(args[i]);
        }
    }

    if(args != NULL)
    {
        free(args);
    }
}

int main(int argc, const char** argv)
{
    memset(fds, -1, MAX_FD_COUNT * sizeof(int));
    while (1)
    {
        memset(command, 0, CMD_BUFFER_SIZE);
        fgets(command, CMD_BUFFER_SIZE, stdin);
        if(strncmp(command, "exit", 4) == 0)
        {
            break;
        }

        char **args;
        int args_count;

        args = parse_command(command, &args_count);

        int result = execute_command(args, args_count);
        if(result < 0)
        {
        	printf("ERROR - %s\n", strerror(errno));
        }
        else
        {
        	if(read_cmd)
        	{
        		printf("SUCCESS - %s\n", read_buffer);
        	}
        	else
        	{
				printf("SUCCESS\n");
        	}

        	read_cmd = 0;
        }

        free_parsed_command(args, args_count);
    }

    return 0;
}
