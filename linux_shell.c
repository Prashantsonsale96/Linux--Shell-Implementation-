#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<dirent.h>
#include<fcntl.h>
#include<errno.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>

#define MAX_INPUT_SIZE 1024
#define MAX_NO_TOKENS 64
#define MAX_TOKEN_SIZE 64

enum tokentype {
	BASECMD,
	PIPE,
	REDIR
};

struct cmdline                    //Structure for the parse tree node which represents the command.
{
	enum tokentype type;         //one of the type: PIPE, REDIR, BASECMD.
	int argsc;                   //number of tokens.
	int fd;                      //for file descriptor use in REDIR
	char** argsv;                //holds the tokens for command.
	struct cmdline *left;        //For left child node.
	struct cmdline *right;       //For right child node.
};

char** globltokens;
static struct cmdline* globlcmd;
static char commands[MAX_NO_TOKENS][MAX_INPUT_SIZE];

/*
 Funtion to clean the allocated memory for parse tree to avoid the memery leaks
*/
void free_tree(struct cmdline* ptree)
{
	if(ptree == NULL) return;
	if(ptree->left) free_tree(ptree->left);
	if(ptree->right) free_tree(ptree->right);
	free(ptree);
}

/*
 Function to clean the allocated memory to avoid the memory leaks.
*/
void clean(char** tokens)
{
	int i;
	for(i=0;tokens[i]!=NULL;i++)
	{
		free(tokens[i]);
	}

	free(tokens);
}

/*
 Function to clean the allocated memory to avoid the memory leaks.
*/
void cleanup_(char** tokens, struct cmdline* ptree)
{
    free_tree(ptree);
    clean(tokens);
}

/*
 Function to check if the input command is linux standard command or not.
*/
int IS_Standard(char* token)
{
	DIR *dir;
	int found = 0;
	struct dirent *d_info;
	if((dir = opendir("/bin/")) != NULL)
	{
		while((d_info = readdir(dir)) != NULL)
		{
			if(!strcmp(token,d_info->d_name))
			{
				found = 1;
				break;
			}
		}
		closedir(dir);
	}
	else
	{
		fprintf(stderr, "Could not opened /bin/");
	}

	DIR *dir2;
	struct dirent *d_info2;
	if((dir2 = opendir("/usr/bin")) != NULL)
	{
		while((d_info2 = readdir(dir2)) != NULL)
		{
			if(!strcmp(token,d_info2->d_name))
			{
				found = 1;
				break;
			}
		}
		closedir(dir2);
	}
	else
	{
		fprintf(stderr,"Could not opened /usr/bin/");

	}

	return found;
}

/*
 * Function to split the command line string into tokens.
 * Returns: tokens and set the tokens count.
 */
char** tokenize_cmd(char*line, int *NO_tokens)
{
	char** tokens = (char**)calloc((sizeof(char*) * MAX_NO_TOKENS),1);
	char* token = (char*)calloc((sizeof(char) * MAX_TOKEN_SIZE),1);

	int i,index=0,found=0 ;
	for(i=0;i<strlen(line);i++)
	{
		if(line[i] == ' ' || line[i] == '\t' || line[i] == '\n')
		{
			token[index] = '\0';
			if(index != 0)
			{
				tokens[found] = (char*)malloc(sizeof(char) * MAX_TOKEN_SIZE);
				strcpy(tokens[found++], token);
				index = 0;
			}
		}
		else
		{
			token[index++] = line[i];
		}
	}
	free(token);
	tokens[found] = NULL;
	*NO_tokens = found;
	return tokens;
}


/*
 Function to generate the parse tree for given 
 commandline arguments.
*/
struct cmdline* creatpt(char** tokens, int start, int end, int* validcmd)
{
	if(end < start) return NULL;
	struct cmdline* cmd = (struct cmdline*)malloc(sizeof(struct cmdline));
	int i;
	for(i=start;i<=end;i++)
	{
		if(!strcmp(tokens[i],"|"))
		{
			int left_valid=0, right_valid=0;
			struct cmdline* leftnode;
			struct cmdline* rightnode;
			leftnode = creatpt(tokens,start,i-1,&left_valid);
			rightnode = creatpt(tokens,i+1,end,&right_valid);
			
			cmd->type = PIPE;
			cmd->left = leftnode;
			cmd->right = rightnode;
			cmd->argsc = 0;
			cmd->argsv = NULL;
			
			if(left_valid && right_valid)
            {
                *validcmd = 1;
            }
			else
			{
				fprintf(stderr,"Invalid Command");
				free_tree(cmd);
				return NULL;
			}

			return cmd;
		}
	}

	for(i=start;i<=end;i++)
	{
		if(!strcmp(tokens[i],"<") || !strcmp(tokens[i],">"))
		{
			int left_valid=0,right_valid=0;
			struct cmdline* leftnode;
			struct cmdline* rightnode;
			leftnode = creatpt(tokens,start,i-1,&left_valid);
			rightnode = creatpt(tokens,i+1,end,&right_valid);
			cmd->type = REDIR;
			if(!strcmp(tokens[i],">"))
				cmd->fd = 1;
			else
				cmd->fd = 0;
			cmd->left = leftnode;
			cmd->right = rightnode;
			cmd->argsc = 1;
			cmd->argsv = &tokens[i+1];		//exactly one arg after one REDIR operator.
			if(left_valid && right_valid && end-1 == i)
			{
				*validcmd = 1;
			}
			else
			{
				fprintf(stderr,"Invalid Command");
				*validcmd = 0;
				free_tree(cmd);
				return NULL;
			}
		return cmd;
		}
	}

	int found = 0;
	found = IS_Standard(tokens[0]);
	if(!strcmp(tokens[0],commands[0]) || !strcmp(tokens[0],commands[1]))
		found = 1;
	if(found)
	{
		*validcmd = 1;
		cmd->type = BASECMD;
		cmd->argsc = end-start+1;
		cmd->argsv = &tokens[start];
		cmd->left = NULL;
		cmd->right = NULL;
		return cmd;
	}
	else
	{
		free(cmd);
		fprintf(stderr,"%s Invalid Command",tokens[start]);
		//free(res);
		return NULL;
	}
}


void CD_Implementation(char** tokens, int NO_tokens)
{
	int status;
	if(NO_tokens != 2)
	{
		fprintf(stderr,"Usage %s DIR_NAME", tokens[0]);
		return;
	}
	
	status = chdir(tokens[1]);
	if(status != 0)
	{
		fprintf(stderr," %s %s", tokens[0], strerror(errno));
		return;
	}
	char pwd[MAX_INPUT_SIZE];
	getcwd(pwd,MAX_INPUT_SIZE);
	printf("\nDirectory is changed to %s",pwd);
}

/*
 Main Function to execute the commands over the parse tree accordingly.
*/
void run_command(struct cmdline* cmd)
{
	if(!cmd)
	{
		cleanup_(globltokens,globlcmd);
		exit(1);
	}
	int tokenscount = cmd->argsc;
	char **tokens = cmd->argsv;
	if(cmd->type == PIPE)					//Classify the root cmd type either its PIPE, RDDIR or BASECMD.
	{
		int p[2];
		if(pipe(p) == -1)
		{
			fprintf(stderr,"PIPE:%s",strerror(errno));
			cleanup_(globltokens,globlcmd);
			exit(1);
		}

		if(fork() == 0)						//"wc | ls" in this right "ls" is parent and "wc" is running in child process. 
		{
			close(1);
			dup(p[1]);     					//STDOUT now goes to PIPE
			close(p[0]);
			close(p[1]);
			run_command(cmd->left);
			cleanup_(globltokens, globlcmd);
			exit(0);
		}
		
		close(0);
		dup(p[0]);
		close(p[1]);
		close(p[0]);
		run_command(cmd->right);
		cleanup_(globltokens, globlcmd);		
		
	}

	else if(cmd->type == REDIR) 			//Run if command contains the redirectional operator.
	{
		close(cmd->fd);
		int fd;
		if(fd = open(cmd->argsv[0], O_WRONLY|O_CREAT|O_TRUNC, 0444) < 0)
		{
			fprintf(stderr,"%s File Opend Failed. %s",cmd->argsv[0],strerror(errno));
			cleanup_(globltokens, globlcmd);
			exit(1);
		}
		run_command(cmd->left);
		close(fd);
		cleanup_(globltokens, globlcmd);
		exit(0);
	}

	else if(cmd->type == BASECMD)
	{
		char *args[MAX_TOKEN_SIZE];
		bzero(args,(MAX_TOKEN_SIZE * sizeof(char*)));
		int i;
		for(i=0;i<tokenscount;++i)
		{
			args[i] = tokens[i];
		}
		args[tokenscount] = NULL;			//array passed to exec is to be null terminated.

		if(execvp(args[0],args));
		{
			fprintf(stderr,"%s\n",strerror(errno));
		}
		cleanup_(globltokens,globlcmd);		//Program control reaches here only if the exec returns failure.
		exit(1);
	}
	else
	{
		fprintf(stderr,"%s Command Not Found",tokens[0]);
		exit(0);
	}	
}

void main()
{
	char line[MAX_INPUT_SIZE];
	sprintf(commands[0],"cd");
	sprintf(commands[1],"exit");
	while(1)
	{
		int NO_tokens = 0;
		printf("UNIX>>");
		bzero(line,MAX_INPUT_SIZE);
		fgets(line,MAX_INPUT_SIZE,stdin);
		globltokens = tokenize_cmd(line,&NO_tokens);
		if(globltokens[0] ==  NULL)
		{
			clean(globltokens);
			continue;
		}
		if(!strcmp(globltokens[0],"clear"))
		{
			clean(globltokens);
			printf("%s","\033[2J\033[1;1H");
			continue;
		}
		if(!strcmp(globltokens[0],"exit"))
		{
			clean(globltokens);
			exit(0);
		}
		int validcmd = 0;
		globlcmd = creatpt(globltokens,0,NO_tokens-1,&validcmd);

		if(validcmd == 0)
		{
			cleanup_(globltokens,globlcmd);
			continue;
		}

		if(globlcmd->type == BASECMD)
		{
			if(!strcmp(globltokens[0],commands[0]))
			{
				CD_Implementation(globltokens,NO_tokens);
				cleanup_(globltokens,globlcmd);
				continue;
			}
		}

		int status;
		int mainproxy = fork();
		if(mainproxy == 0)
		{
			run_command(globlcmd);
			exit(0);
		}
		else
		{	
			waitpid(mainproxy, &status, 0);
		}
		cleanup_(globltokens,globlcmd);
	}
	
}
