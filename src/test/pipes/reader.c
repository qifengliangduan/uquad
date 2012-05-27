/**
 * reader: test program for pipe IPC, reader side
 * Copyright (C) 2012  Rodrigo Rosa <rodrigorosa.lg gmail.com>, Matias Tailanian <matias tailanian.com>, Santiago Paternain <spaternain gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @file   reader.c
 * @author Rodrigo Rosa <rodrigorosa.lg gmail.com>, Matias Tailanian <matias tailanian.com>, Santiago Paternain <spaternain gmail.com>
 * @date   Sun May 27 10:02:32 2012
 *
 * @brief  test program for pipe IPC, reader side
 *
 */
#include <stdio.h>
#include <sys/stat.h> // for mkfifo()
#include <signal.h> // for SIGINT, SIGQUIT
#include <uquad_aux_io.h>
#include <uquad_aux_time.h>
#include <uquad_error_codes.h>
#include <limits.h>

#define USAGE "Wrong arguments!\n"\
    "Usage:./reader pipe_name.p"

#define PIPE_NAME "wr_pipe"
#define DEF_PERM 0666

#define NEW_LINE_LEN PATH_MAX

#define READER_TIMEOUT 20 // sec

#define LOG_STD_ERR 1
#define LOG_FILE 1

static FILE *pipe_f = NULL;
static FILE *log_file = NULL;

static uquad_bool_t die = false;
static char *new_line = NULL;

void quit(void)
{
    if(new_line != NULL)
	free(new_line);
    if(pipe_f != NULL)
	fclose(pipe_f);
    if(log_file != NULL)
	fclose(log_file);
    exit(0);
}

void uquad_sig_handler(int signal_num)
{
    err_log_num("Caught signal! Will finsh and die",signal_num);
    die = true;
}

int main(int argc, char *argv[])
{
    struct timeval tv_new,tv_old,tv_diff;
    int retval;
    char *pipe_name;
    char log_name[PATH_MAX];
    int itmp;
    size_t len;

    new_line = (char *) malloc(sizeof(char)*PATH_MAX);

    // Catch signals
    signal(SIGINT, uquad_sig_handler);
    signal(SIGQUIT, uquad_sig_handler);

    if(argc < 2)
    {
	err_log(USAGE);
	quit();
    }
    else
    {
	pipe_name = argv[1];
    }

    pipe_f = fopen(pipe_name, "r");
    if(pipe_f == NULL)
    {
	retval = mkfifo(pipe_name,DEF_PERM);
	if(retval != ERROR_OK)
	{
	    err_log_stderr("Failed to create pipe!");
	    quit();
	}
	pipe_f = fopen(pipe_name, "r");
	if(pipe_f == NULL)
	{
	    err_log_stderr("Failed to open pipe!");
	    quit();
	}
    }

    strcpy(log_name, pipe_name);
    itmp = strlen(log_name);
    if(itmp < 2 ||
       log_name[itmp-1] != 'p' ||
       log_name[itmp-2] != '.')
    {
	err_check(ERROR_INVALID_PIPE_NAME,"Wrong pipe extension!");
    }
    log_name[itmp-1] = 'l';
    log_name[itmp] = 'o';
    log_name[itmp+1] = 'g';
    log_name[itmp+2] = '\0';

    log_file = fopen(log_name,"w");
    if(log_file == NULL)
    {
	err_log_stderr("Failed to open log file!");
	quit();
    }

    gettimeofday(&tv_old,NULL);
    for(;;)
    {
	//	retval = fscanf(pipe_f,"%lf",&lf);
	retval = getline(&new_line, &len, pipe_f);
	if(retval > 0)
	{
	    gettimeofday(&tv_new,NULL);
	    retval = uquad_timeval_substract(&tv_diff,tv_new,tv_old);
	    if(retval < 0)
	    {
		err_log("TIMING: absurd!");
	    }
	    fprintf(log_file,"%s",new_line);
	    gettimeofday(&tv_old,NULL);	
	}
	else
	{
	    gettimeofday(&tv_new,NULL);
	    retval = uquad_timeval_substract(&tv_diff,tv_new,tv_old);
	    if(die || tv_diff.tv_sec > READER_TIMEOUT)
	    {
		if(tv_diff.tv_sec > READER_TIMEOUT)
		{
		    err_log("Timed out!");
		}
		quit();
	    }
	}
    }
}
