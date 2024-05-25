#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#define __USE_XOPEN_EXTENDED
#include <ftw.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>

char response[1024];
size_t response_size = 0;
int found_file = 0;
char *find_file;
long size1;
long size2;
char tar_size[150] = "0";
char tar_file_path[150];
char client_request[150];
char extensions[50];
long date;
char *mirror1IPPort = "127.0.0.1 8001";
char *mirror2IPPort = "127.0.0.1 8002";
int client_count = 0;
int server_fd, client_fd;
struct sockaddr_in server_info, client_info;
char *continue_status = "continue_status";
socklen_t client_size;
pid_t pid;

//nftw function to fetch the file details
int nftw_func(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    //extract the file name from the path
    char *traversed_file = strrchr(path, '/') + 1;
    if (typeflag == FTW_F && strcmp(traversed_file, find_file) == 0) {
        //when item is file and matches the desired filename
        char cmd[256];
        //command to fetch the necessary details about the given file (using formated operands of stat command)
        sprintf(cmd, "echo $(stat -c \"File name: %%n\\nSize: %%s bytes\\nDate Created: %%y-\\nFile Permissions: %%A\" %s)", find_file);

        FILE *fp = popen(cmd, "r");
        if (fp == NULL) {
            return 0;
        }

        char output[256];

        //get the file info in response
        while (fgets(output, sizeof(output), fp) != NULL) {
            strcat(response, output);
            response_size += strlen(output) + 1;
        }

        //set the file found flag to true
        found_file = 1;
        //stop traversal (file found)
        return 1;
    }
    //continue traversal
    return 0;
}

//nftw function to get the files within specific file size (bytes)
int nftw_find_size(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_F && sb->st_size >= size1 && sb->st_size <= size2) {
        //when item matches as file and the size is within required size range
        //get the filename form whole path
        char *traversed_file = strrchr(path, '/') + 1;
        char cmd[256];
        //check the found_file flag for creating new tar
        if (!found_file) {
            //run tar command on the found file with the destination(tar_file_path) 
            //-c for creating new tar
            //-f for specific tar file name
            //2>/dev/null add error redirecting to avoid unnecessary logs on server
            sprintf(cmd, "tar -cf %s %s 2>/dev/null", tar_file_path, traversed_file);
        } else {
            //-r for using the previouly created tar (to append the next file in it)
            sprintf(cmd, "tar -rf %s %s 2>/dev/null", tar_file_path, traversed_file);
        }
        found_file = 1;
        //run the command
        system(cmd);
    }
    //no need to stop traversal as we need to look for all the files with size range
    // continue traversal
    return 0;
}

// nftw function to look for files with specific extensions (max 3 ext)
int nftw_find_ext(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_F) {
        //when item is file
        char *filename = strrchr(path, '/') + 1;
        //get the file extension from the path
        char *ext = strrchr(filename, '.');
        if (ext != NULL) {
            //when extension has some valid data
            //increment the address pointer to get the extension name (ignoring '.')
            ext++; 
            if (strstr(extensions, ext) != NULL) {
                //when extension matches with the required extensions list
                char cmd[256];
                if (!found_file) {
                    //run tar command on the found file with the destination(tar_file_path)
                    //-c for creating new tar
                    //-f for specific tar file name
                    //2>/dev/null add error redirecting to avoid unnecessary logs on server
                    sprintf(cmd, "tar -cf %s %s 2>/dev/null", tar_file_path, filename);
                } else {
                    //-r for using the previouly created tar (to append the next file in it)
                    sprintf(cmd, "tar -rf %s %s 2>/dev/null", tar_file_path, filename);
                }
                found_file = 1;
                system(cmd);
            }
        }
    }
    //continue traversal
    return 0;
}

//nftw function to look for files with creation date before the given date
int nftw_find_before_date(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_F && sb->st_mtime <= date && strstr(path, "temp.tar.gz") == NULL) {
        //when item matches file and the date (sb->st_mtime) is less than equal given date
        //also exclude the (temp.tar.gz) as server and client are on same system (to avoid double files)
        char *traversed_file = strrchr(path, '/') + 1;
        char cmd[256];
        if (!found_file) {
            sprintf(cmd, "tar -cf %s %s 2>/dev/null", tar_file_path, traversed_file);
        }
        else {
            sprintf(cmd, "tar -rf %s %s 2>/dev/null", tar_file_path, traversed_file);
        }
        found_file = 1;
        system(cmd);
    }
    return 0;
}

//nftw function to look for files with creation date after the given date
int nftw_find_after_date(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_F && sb->st_mtime >= date && strstr(path, "temp.tar.gz") == NULL) {
        //when item matches file and the date (sb->st_mtime) is greater than equal given date
        //also exclude the (temp.tar.gz) as server and client are on same system (to avoid double files)
        char *traversed_file = strrchr(path, '/') + 1;
        char cmd[256];
        if (!found_file) {
            sprintf(cmd, "tar -cf %s %s 2>/dev/null", tar_file_path, traversed_file);
        }
        else {
            sprintf(cmd, "tar -rf %s %s 2>/dev/null", tar_file_path, traversed_file);
        }
        found_file = 1;
        system(cmd);
    }
    return 0;
}

//compare function for qsort (to sort files in alphabetical ascending order)
int compare_dir_names(const void *a, const void *b) {
    return strcmp(*(char**)a, *(char**)b);
}

//compare function for qsort (to sort files with oldest created as first)
int compare_dir_ctime(const void *a, const void *b) {
    struct dirent **dir_a = (struct dirent **)a;
    struct dirent **dir_b = (struct dirent **)b;

    struct stat stat_a, stat_b;
    stat(dir_a[0]->d_name, &stat_a);
    stat(dir_b[0]->d_name, &stat_b);

    return difftime(stat_a.st_mtime, stat_b.st_mtime);
}

//function to handle the client request ("dirlist -a")
void handle_dirlist(int client_fd) {
    DIR *dir;
    struct dirent *entry;
    strcpy(response, "");
    response_size = 0;
    char *home_dir = getenv("HOME");

    if ((dir = opendir(home_dir)) == NULL) {
        perror("opendir");
        return;
    }

    int num_dirs = 0;
    char *dir_names[1024];

    //readdir to get all the directories of the specific directory
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            //when directory is discovered
            if (strcspn(entry->d_name, ".") == 0) {
                //check for '.' as first char (to avoid returing hidden folders)
                continue;
            }
            //copy the traversed dir in dir_names
            dir_names[num_dirs++] = strdup(entry->d_name);
        }
    }

    //sort the dir_names (has all directories of home directory) 
    qsort(dir_names, num_dirs, sizeof(char *), compare_dir_names);

    //build the client response message (adding dir_names seperated with '\n' for better view on client side)
    for (int i = 0; i < num_dirs; i++) {
        strcat(response, dir_names[i]);
        strcat(response, "\n");
        response_size += strlen(dir_names[i]) + 1;
        free(dir_names[i]);
    }

    closedir(dir);

    //send the response to client
    if (write(client_fd, response, response_size) == -1) {
        fprintf(stderr, "Failure Sending Message\n");
        close(client_fd);
        exit(1);
    }
}

//function to handle the client request ("dirlist -t")
void handle_dirlist_time(int client_fd) {
    DIR *dir;
    struct dirent *entry;
    strcpy(response, "");
    response_size = 0;
    char *home_dir = getenv("HOME");

    if ((dir = opendir(home_dir)) == NULL) {
        perror("opendir");
        return;
    }

    int num_dirs = 0;
    struct dirent *dir_names[1024];

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            if (strcspn(entry->d_name, ".") == 0) {
                continue;
            }
            dir_names[num_dirs++] = entry;
        }
    }

    //sort the directories with creation date
    qsort(dir_names, num_dirs, sizeof(struct dirent *), compare_dir_ctime);

    for (int i = 0; i < num_dirs; i++) {
        strcat(response, dir_names[i]->d_name);
        strcat(response, "\n");
        response_size += strlen(dir_names[i]->d_name) + 1;
    }

    closedir(dir);

    //send the response to client
    if (write(client_fd, response, response_size) == -1) {
        fprintf(stderr, "Failure Sending Message\n");
        close(client_fd);
        exit(1);
    }
}

// handles "w24fn <filename>"
int handle_w24fn(int client_fd, char *filename) {
    char *buffer = NULL;
    size_t buffer_size = 0;
    struct stat st;
    find_file = filename;
    //get the home directory (to start traversal from there)
    char *home_dir = getenv("HOME");
    strcpy(response, "");
    response_size = 0;

    //call the nftw callback function to check file and fetch it's info
    if (nftw(home_dir, nftw_func, 20, FTW_CHDIR) == -1) {
        perror("nftw");
        return 1;
    }

    //when no file was found
    if (!found_file) {
        //set the appropriate message for client
        strcpy(response, "No file found");
        response_size = strlen("No file found");
    }
    //write response to client
    if (write(client_fd, response, response_size) == -1) {
        fprintf(stderr, "Failure Sending Message\n");
        return 1;
    }

    return 0;
}

//handles "w24fz", "w24ft", "w24fdb", "w24fda"
int handle_tar_request(int client_fd, char *option) {
    strcpy(response, "");
    response_size = 0;
    found_file = 0;
    char *home_dir = getenv("HOME");
    //create a tar file path to store the files in there before sending tar to client
    sprintf(tar_file_path, "%s/server_temp.tar.gz", getenv("HOME"));

    if (strstr(client_request, "w24fz") != NULL) {
        //when request is for "w24fz"
        char* token = strtok(option, " ");
        //set size range for nftw to look for files within that range
        size1 = atoi(token);
        token = strtok(NULL, " ");
        size2 = atoi(token);
        //call the nftw callback function with FTW_CHDIR flag
        //to make sure directory is changed to the traversed path
        //(to avoid absolute path files in tar file)
        if (nftw(home_dir, nftw_find_size, 20, FTW_CHDIR) == -1) {
            perror("nftw");
            return 1;
        }
    } else if (strstr(client_request, "w24ft") != NULL) {
        //when request is for "w24ft"
        //set extensions to match with
        strcpy(extensions, option);
        if (nftw(home_dir, nftw_find_ext, 20, FTW_CHDIR) == -1) {
            perror("nftw");
            return 1;
        }
    } else if (strstr(client_request, "w24fdb") != NULL) {
        //when request is for "w24fdb"
        //set the date to filter files with creation date before the given date
        date = atol(option);
        if (nftw(home_dir, nftw_find_before_date, 20, FTW_CHDIR) == -1) {
            perror("nftw");
            return 1;
        }
    } else if (strstr(client_request, "w24fda") != NULL) {
        //when request is for "w24fda"
        //set the date to filter files with creation date after the given date
        date = atol(option);
        if (nftw(home_dir, nftw_find_after_date, 20, FTW_CHDIR) == -1) {
            perror("nftw");
            return 1;
        }
    }


    if (!found_file) {
        //no files were found
        //set the appropriate message and redirect the response to client
        strcpy(response, "No file found");
        response_size = strlen("No file found");
        if (write(client_fd, response, 200) == -1) {
            fprintf(stderr, "Failure Sending Message\n");
            return 1;
        }
    } else {
        //when files found and tar was created
        char cmd[256];
        //create a command to get the size of the tar file created with the found files in it
        sprintf(cmd, "stat -c %%s %s", tar_file_path);
        FILE *fp = popen(cmd, "r");
        if (fp == NULL) {
            return 0;
        }

        char output[256];
        while (fgets(output, sizeof(output), fp) != NULL) {
            strcpy(tar_size, output);
        }
        //build a response for client with tar file size
        //to notify client of incoming file size
        strcpy(response, "file#");
        strcat(response, tar_size);
        if (write(client_fd, response, 200) == -1) {
            fprintf(stderr, "Failure Sending Message\n");
            return 1;
        }
        int num;
        char buff[1024];
        //keep track of bytes sent
        long sent_bytes = 0;
        long total_bytes = atol(tar_size);
        //open tar file in read mode
        int fd = open(tar_file_path, O_RDONLY, 0777);
        //until the bytes sent is less then total bytes of tar file
        while (sent_bytes < total_bytes) {
            //read the file in buffer
            if ((num = read(fd, buff, 1024)) > 0) {
                sleep(0.1);
                //increase the sent bytes by adding the read bytes
                sent_bytes += num;
                //send the read bytes to client then continue reading
                write(client_fd, buff, num);
            } else if (num == 0) {
                //when bytes read zero (no need to send anything to client)
                break;
            } else {
                fprintf(stderr, "Error Reading File\n");
                return 1;
            }
        }
    }
    return 0;
}

//all client requests are handled from here
void crequest(int client_fd) {
    char buffer[1024];
    int num;

    while (1) {
        if ((num = read(client_fd, buffer, 1023)) == -1) {
            perror("recv");
            exit(1);
        }
        if (num == 0) {
            //checks for client connection status
            printf("Client disconnected.\n");
            close(client_fd);
            exit(0);
        }

        buffer[num] = '\0';

        strcpy(client_request, "");
        strcpy(client_request, buffer);

        if (strcmp(buffer, "quitc") == 0) {
            //when client explicitly specifies quitc to disconnect from server
            printf("Client disconnected.\n");
            close(client_fd);
            close(server_fd);
            exit(0);
        } else if (strcmp(buffer, "dirlist -a") == 0) {
            handle_dirlist(client_fd);
            continue;
        } else if (strcmp(buffer, "dirlist -t") == 0) {
            handle_dirlist_time(client_fd);
            continue;
        }
        else if (strstr(buffer, "w24fn") != NULL) {
            handle_w24fn(client_fd, buffer + strlen("w24fn") + 1);
            continue;
        }
        else if (strstr(buffer, "w24fz") != NULL) {
            handle_tar_request(client_fd, buffer + strlen("w24fz") + 1);
            continue;
        }
        else if (strstr(buffer, "w24ft") != NULL) {
            handle_tar_request(client_fd, buffer + strlen("w24ft") + 1);
            continue;
        }
        else if (strstr(buffer, "w24fdb") != NULL) {
            handle_tar_request(client_fd, buffer + strlen("w24fdb") + 1);
            continue;
        }
        else if (strstr(buffer, "w24fda") != NULL) {
            handle_tar_request(client_fd, buffer + strlen("w24fda") + 1);
            continue;
        }
    }
}

//create socket connection on server side
void makeConnection(int server_port, int backlog) {
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Socket Failure!!\n");
        exit(1);
    }

    memset(&server_info, 0, sizeof(server_info));
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(server_port);
    server_info.sin_addr.s_addr = INADDR_ANY;

    //bind the socket with the server address
    if (bind(server_fd, (struct sockaddr *)&server_info, sizeof(struct sockaddr)) == -1) {
        fprintf(stderr, "Binding Failure\n");
        exit(1);
    }
    //start listening for any client connections
    if (listen(server_fd, backlog) == -1) {
        fprintf(stderr, "Listening Failure\n");
        exit(1);
    }
    printf("Server is running and listening on port %d...\n", server_port);
}

//function to balance server load
int loadBalance() {
    //keep track of the client request number 
    client_count++;
    if (client_count <= 9) {
        if (client_count<=3) {
            // printf(" count- %d server\n", client_count);
        }else if (client_count <= 6) {
            //send client connection to mirror 1
            // printf(" count- %d mirror1\n", client_count);
            int child_pid = fork();
            if (!child_pid) {
                close(server_fd);
                //send the address and port of mirror 1 to client
                write(client_fd, mirror1IPPort, strlen(mirror1IPPort));
                close(client_fd);
                exit(0);
            } else {
                close(client_fd);
                waitpid(child_pid, NULL, WNOHANG);
            }
            return 1;
        } else if (client_count <= 9) {
            //send client connection to mirror 2
            // printf(" count- %d mirror2\n", client_count);
            int child_pid = fork();
            if (!child_pid) {
                close(server_fd);
                //send the address and port of mirror 2 to client
                write(client_fd, mirror2IPPort, strlen(mirror2IPPort));
                close(client_fd);
                exit(0);
            } else {
                close(client_fd);
                waitpid(child_pid, NULL, WNOHANG);
            }
            return 1;
        }
    } else {
        if ((client_count - 10) % 3 == 0) {
            // printf(" count- %d server\n", client_count);
        }else if ((client_count - 11) % 3 == 0) {
            // printf(" count- %d mirror1\n", client_count);
            int child_pid = fork();
            if (!child_pid) {
                close(server_fd);
                write(client_fd, mirror1IPPort, strlen(mirror1IPPort));
                close(client_fd);
                exit(0);
            } else {
                close(client_fd);
                waitpid(child_pid, NULL, WNOHANG);
            }
            return 1;
        } else if ((client_count - 12) % 3 == 0) {
            // printf(" count- %d mirror2\n", client_count);
            int child_pid = fork();
            if (!child_pid) {
                close(server_fd);
                write(client_fd, mirror2IPPort, strlen(mirror2IPPort));
                close(client_fd);
                exit(0);
            } else {
                close(client_fd);
                waitpid(child_pid, NULL, WNOHANG);
            }
            return 1;
        }
    }
    return 0;
}

//accept all client connections
void acceptConnections() {
    while (1) {
        //while loop to keep accepting the client connections
        client_size = sizeof(struct sockaddr_in);
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_info, &client_size)) == -1) {
            perror("accept");
            exit(1);
        }

        //perform load balancing
        if(loadBalance()){
            //when connection was redirected to either mirror1 or mirror2
            continue;
        }

        //connected to client
        printf("Got a client connection from %s\n", inet_ntoa(client_info.sin_addr));

        //fork process to handles client requests
        pid = fork();
        if (pid == 0) {
            close(server_fd);
            //send a continue status response to client
            //for letting client know that connection with server was established successfully
            write(client_fd, continue_status, strlen(continue_status));
            //now handle any incoming request from client
            crequest(client_fd);
        } else if (pid > 0) {
            close(client_fd);
            waitpid(pid, NULL, WNOHANG);
        } else {
            perror("fork");
            exit(1);
        }
    }

    // close(server_fd);
}

// //handler for SIGCHLD
// void sigint_handler(int signum) {
//     // int status;
//     // pid_t child_pid;
//     close(server_fd);
//     kill(0,SIGINT);
//     exit(0);
//     // child_pid = waitpid(-1, &status, WNOHANG);
// }

//handler for SIGCHLD
void sigchild_handler(int signum) {
    // wait for child process to terminate
    // close(fd);
    waitpid(-1, NULL, WNOHANG);
}

int main() {

    //resister sigchild handler to let the child process exit gracefully (without creating any zombie process)
    signal(SIGCHLD, sigchild_handler);
    // signal(SIGINT, sigint_handler);

    int SERVER_PORT = 8000;
    //max client listening threshold
    int BACKLOG = 10;

    //create a connection and start listening for client connections
    makeConnection(SERVER_PORT, BACKLOG);

    //handle all client connection after accepting
    acceptConnections();

    // close(server_fd);
    return 0;
}