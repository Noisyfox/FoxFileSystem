#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "shell.h"
#include "util.h"
#include "SparseArray.h"

#define ERR_ALLO {fprintf(stderr, "sh: allocation error\n");exit(1);}

#define SH_LINE_BUFSIZE 1024

//===============================================================================================

#define MAX_OPENED 128
File* fd_list[MAX_OPENED];

void fd_init()
{
    memset(fd_list, 0, sizeof(fd_list));
}

int fd_put(File* f)
{
    for (int i = 0; i < MAX_OPENED; i++)
    {
        if (fd_list[i] == NULL)
        {
            fd_list[i] = f;
            return i;
        }
    }

    return EOF;
}

File* fd_get(int fd)
{
    if (fd < 0 || fd >= MAX_OPENED)
    {
        return NULL;
    }
    return fd_list[fd];
}

void fd_remove(int fd)
{
    if (fd < 0 || fd >= MAX_OPENED)
    {
        return;
    }
    fd_list[fd] = NULL;
}

__int64 parse_longlong(char* i)
{
    char* ep;
    __int64 fd = strtoll(i, &ep, 10);
    if (ep == i)
    {
        return EOF;
    }

    return fd;
}

//===============================================================================================
static Directory* directory_service;

char* getcwd(char* p, size_t size)
{
    return directory_service->GetWD(p, size);
}

int chdir(char const* path)
{
    return directory_service->ChDir(path);
}

int mkdir(char const* name)
{
    return directory_service->MkDir(name);
}

int rmdir(char const* name)
{
    return directory_service->RmDir(name);
}

int rm(char const* name)
{
    return directory_service->Remove(name);
}

int touch(char const* name)
{
    File* f = directory_service->OpenFile(name, false, true, true, false, true);
    ASSERT_NULL(f);
    bool ret = f->Close();
    delete f;

    return ret ? 0 : EOF;

faild:
    return EOF;
}

int open(char const* name)
{
    File* f = directory_service->OpenFile(name, true, true, false, false, false);
    if (f == NULL)
    {
        return EOF;
    }

    int fd = fd_put(f);
    if (fd == EOF)
    {
        f->Close();
        delete f;
    }

    return fd;
}

int close(int fd)
{
    File* f = fd_get(fd);

    if (f == NULL)
    {
        return EOF;
    }
    fd_remove(fd);

    f->Close();
    delete f;

    return 0;
}

offset_t tell(int fd)
{
    File* f = fd_get(fd);

    if (f == NULL)
    {
        return EOF;
    }
    
    return f->Tell();
}

offset_t seek(int fd, offset_t offset, char const* orig)
{
    int o;
    switch (orig[0])
    {
    case 's':
        o = SEEK_SET;
        break;
    case 'c':
        o = SEEK_CUR;
        break;
    case 'e':
        o = SEEK_END;
        break;
    default:
        return EOF;
    }

    File* f = fd_get(fd);

    if (f == NULL)
    {
        return EOF;
    }
    
    return f->Seek(offset, o);
}

__int64 truncate(int fd, file_size_t size)
{
    File* f = fd_get(fd);

    if (f == NULL)
    {
        return EOF;
    }

    return f->Truncate(size);
}

size_t read(int fd, void* buf, size_t size)
{
    File* f = fd_get(fd);

    if (f == NULL)
    {
        return 0;
    }

    return f->Read(buf, size);
}

size_t write(int fd, void* buf, size_t size)
{
    File* f = fd_get(fd);

    if (f == NULL)
    {
        return 0;
    }

    return f->Write(buf, size);
}

//===============================================================================================

char * shell_read_line() {
    int bufSize = SH_LINE_BUFSIZE;
    int position = 0;
    char *buffer = (char*)malloc(sizeof(char) * bufSize);
    int c;

    if (buffer == NULL) {
        ERR_ALLO;
    }

    while (1) {
        c = getchar();
        if (c == EOF || c == '\n') {
            buffer[position] = '\0';
            return buffer;
        }
        else {
            buffer[position] = c;
        }
        position++;

        if (position >= bufSize) {
            bufSize += SH_LINE_BUFSIZE;
            buffer = (char*)realloc(buffer, bufSize);
            if (buffer == NULL) {
                ERR_ALLO;
            }
        }
    }
}

#define SH_TOK_BUFSIZE 64
#define SH_TOK_DELIM " \t\r\n\a"

char **shell_split_line(char *line) {
    int bufSize = SH_TOK_BUFSIZE;
    int position = 0;
    char **tokens = (char**)malloc(bufSize * sizeof(char*));
    char *token;

    if (tokens == NULL) {
        ERR_ALLO;
    }

    token = strtok(line, SH_TOK_DELIM);
    while (token != NULL) {
        tokens[position++] = token;

        if (position >= bufSize) {
            bufSize += SH_TOK_BUFSIZE;
            tokens = (char**)realloc(tokens, bufSize * sizeof(char*));
            if (tokens == NULL) {
                ERR_ALLO;
            }
        }

        token = strtok(NULL, SH_TOK_DELIM);
    }

    tokens[position] = NULL;

    return tokens;
}

int shell_builtin_cd(char** args) {
    int result;
    if (args[1] == NULL) {
        result = chdir("~");
    }
    else {
        result = chdir(args[1]);
    }

    if (result != 0) {
        fprintf(stderr, "sh: Error change dir.\n");
    }

    return 1;
}

int shell_builtin_pwd(char** args) {
    char * pwd;
    pwd = getcwd(NULL, 0);

    if (pwd == NULL) {
        fprintf(stderr, "sh: Error get pwd.\n");
        return 1;
    }

    printf("%s\n", pwd);
    free(pwd);

    return 1;
}

int shell_builtin_exit(char** args) {
    return 0;
}

int shell_builtin_ls(char** args)
{

    DirectoryFile* dir = directory_service->OpenDirectory(".");
    if (dir == NULL)
    {
        fprintf(stderr, "sh: Error list directory.\n");
        return 1;
    }
    if (dir->NextEntry(true) != EOC)
    {
        do
        {
            printf("%s\n", dir->entry_current);
        } while (dir->NextEntry(false) != EOC);
    }
    directory_service->CloseDirectory(dir);

    return 1;
}

int shell_builtin_mkdir(char** args)
{
    int result;
    if (args[1] == NULL) {
        result = EOF;
    }
    else {
        result = mkdir(args[1]);
    }

    if (result != 0) {
        fprintf(stderr, "sh: Error create dir.\n");
    }

    return 1;
}

int shell_builtin_rmdir(char** args)
{
    int result;
    if (args[1] == NULL) {
        result = EOF;
    }
    else {
        result = rmdir(args[1]);
    }

    if (result != 0) {
        fprintf(stderr, "sh: Error remove dir.\n");
    }

    return 1;
}

int shell_builtin_rm(char** args)
{
    int result;
    if (args[1] == NULL) {
        result = EOF;
    }
    else {
        result = rm(args[1]);
    }

    if (result != 0) {
        fprintf(stderr, "sh: Error remove file.\n");
    }

    return 1;
}

int shell_builtin_touch(char** args)
{
    int result;
    if (args[1] == NULL) {
        result = EOF;
    }
    else {
        result = touch(args[1]);
    }

    if (result != 0) {
        fprintf(stderr, "sh: Error create file.\n");
    }

    return 1;
}

int shell_builtin_open(char** args)
{
    int result;
    if (args[1] == NULL) {
        result = EOF;
    }
    else {
        result = open(args[1]);
    }

    if (result == EOF) {
        fprintf(stderr, "sh: Error open file.\n");
    }
    else
    {
        fprintf(stdout, "File opened for read and write as %d.\n", result);
    }

    return 1;
}

int shell_builtin_close(char** args)
{
    int result;
    if (args[1] == NULL) {
        result = EOF;
    }
    else
    {
        result = close((int)parse_longlong(args[1]));
    }

    if (result == EOF) {
        fprintf(stderr, "sh: Error close file.\n");
    }
    else
    {
        fprintf(stdout, "File closed.\n");
    }

    return 1;
}

int shell_builtin_tell(char** args)
{
    offset_t result;
    if (args[1] == NULL) {
        result = EOF;
    }
    else
    {
        result = tell((int)parse_longlong(args[1]));
    }

    if (result == EOF) {
        fprintf(stderr, "sh: Error get file offset.\n");
    }
    else
    {
        fprintf(stdout, "Current file offset: %lld.\n", result);
    }

    return 1;
}

int shell_builtin_seek(char** args)
{
    offset_t result;
    if (args[1] == NULL || args[2] == NULL || args[3] == NULL) {
        result = EOF;
    }
    else
    {
        result = seek((int)parse_longlong(args[1]), parse_longlong(args[2]), args[3]);
    }

    if (result == EOF) {
        fprintf(stderr, "sh: Error change file offset.\n");
    }
    else
    {
        fprintf(stdout, "Current file offset: %lld.\n", result);
    }

    return 1;
}

int shell_builtin_truncate(char** args)
{
    __int64 result;
    if (args[1] == NULL || args[2] == NULL) {
        result = EOF;
    }
    else
    {
        result = truncate((int)parse_longlong(args[1]), parse_longlong(args[2]));
    }

    if (result == EOF) {
        fprintf(stderr, "sh: Error change file size.\n");
    }
    else
    {
        fprintf(stdout, "Current file size: %lld.\n", result);
    }

    return 1;
}

int shell_builtin_read(char** args)
{
    __int64 result;
    if (args[1] == NULL || args[2] == NULL || args[3] == NULL) {
        result = EOF;
    }
    else
    {
        result = EOF;
        do {
            int fd = (int)parse_longlong(args[1]);


            if (fd == EOF) {
                break;
            }
            FILE* of = fopen(args[2], "wb+");
            if (of == NULL)
            {
                break;
            }

            size_t read_size = (size_t)parse_longlong(args[3]);
            result = 0;
            byte_t buf[1024];
            while (read_size > 0)
            {
                size_t r_size = read(fd, buf, read_size > 1024 ? 1024 : read_size);
                if (r_size == 0)
                {
                    break;
                }

                fwrite(buf, sizeof(byte_t), r_size, of);

                result += r_size;
                read_size -= r_size;
            }

            fclose(of);
        } while (0);
    }

    if (result == EOF) {
        fprintf(stderr, "sh: Error read file.\n");
    }
    else
    {
        fprintf(stdout, "File data read: %lld byte(s).\n", result);
    }

    return 1;
}

int shell_builtin_write(char** args)
{
    __int64 result;
    if (args[1] == NULL || args[2] == NULL || args[3] == NULL) {
        result = EOF;
    }
    else
    {
        result = EOF;
        do {
            int fd = (int)parse_longlong(args[1]);


            if (fd == EOF) {
                break;
            }
            FILE* of = fopen(args[2], "rb+");
            if (of == NULL)
            {
                break;
            }

            size_t read_size = (size_t)parse_longlong(args[3]);
            result = 0;
            byte_t buf[1024];
            while (read_size > 0)
            {
                size_t r_size = fread(buf, sizeof(byte_t), read_size > 1024 ? 1024 : read_size, of);
                if (r_size == 0)
                {
                    break;
                }

                size_t w_size = write(fd, buf, r_size);

                result += w_size;
                read_size -= w_size;
            }

            fclose(of);
        } while (0);
    }

    if (result == EOF) {
        fprintf(stderr, "sh: Error write file.\n");
    }
    else
    {
        fprintf(stdout, "File data write: %lld byte(s).\n", result);
    }

    return 1;
}

char *builtin_cmds_str[] = {
    "cd",
    "pwd",
    "exit",
    "ls",
    "mkdir",
    "rmdir",
    "rm",
    "touch",
    "open",
    "close",
    "tell",
    "seek",
    "truncate",
    "read",
    "write"
};

int(*builtin_cmds_func[])(char**) = {
    &shell_builtin_cd,
    &shell_builtin_pwd,
    &shell_builtin_exit,
    &shell_builtin_ls,
    &shell_builtin_mkdir,
    &shell_builtin_rmdir,
    &shell_builtin_rm,
    &shell_builtin_touch,
    &shell_builtin_open,
    &shell_builtin_close,
    &shell_builtin_tell,
    &shell_builtin_seek,
    &shell_builtin_truncate,
    &shell_builtin_read,
    &shell_builtin_write
};

#define SH_BUILTIN_COUNT (sizeof(builtin_cmds_str) / sizeof(char*))

int shell_exec(char** tok) {
    int i;

    if (tok[0] == NULL)return 1;

    for (i = 0; i < SH_BUILTIN_COUNT; i++) {
        if (strcmp(tok[0], builtin_cmds_str[i]) == 0) {
            return (*builtin_cmds_func[i])(tok);
        }
    }

    fprintf(stderr, "sh: unknown command: %s\n", tok[0]);
    return 1;
}

int shell_main(Directory* dir_serv) {
    directory_service = dir_serv;

    fd_init();

    char * line, *cwd;
    char ** token;

    fprintf(stderr, "            FOX Shell Ver 0.000001\n");
    fprintf(stderr, "    built in commands:\n");
    fprintf(stderr, "        ");
    for (int i = 0; i < SH_BUILTIN_COUNT - 1; i++)
    {
        fprintf(stderr, "%s,", builtin_cmds_str[i]);
    }
    fprintf(stderr, "%s\n", builtin_cmds_str[SH_BUILTIN_COUNT - 1]);
    fprintf(stderr, "\n");

    int re = 1;

    while (re) {
        cwd = getcwd(NULL, 0);
        if (cwd == NULL) {
            ERR_ALLO;
        }
        printf("%s $ ", cwd);
        free(cwd);

        line = shell_read_line();
        token = shell_split_line(line);
        re = shell_exec(token);

        free(token);
        free(line);
    }
    return 0;
}
