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
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <regex.h>

#define BUFFER_SIZE 1024
int sock_fd, num;
//declare buufer to send request to server
char buffer[BUFFER_SIZE];
struct sockaddr_in server_info;
struct hostent *he;
char buff[BUFFER_SIZE];

//validator for checking size range
int validateSizeRange(char *sizerange) {
    regex_t regex;
    int return_regex;

    //it checks pattern like: (90 100)
    return_regex = regcomp(&regex, "^[0-9]+ [0-9]+$", REG_EXTENDED);
    if (return_regex) {
        fprintf(stderr, "Could not compile regex\n");
        return 1;
    }

    return_regex = regexec(&regex, sizerange, 0, NULL, 0);
    if (!return_regex) {
        regfree(&regex);
        //also check range is in proper order or not
        char* token = strtok(sizerange, " ");
        int size1 = atoi(token);
        token = strtok(NULL, " ");
        int size2 = atoi(token);
        if (size1 > size2) {
            return -1;
        }
        return 0;
    } else if (return_regex == REG_NOMATCH) {
        regfree(&regex);
        return -1;
    } else {
        regfree(&regex);
        fprintf(stderr, "Regex match failed\n");
        return -1;
    }
}

//check the dirlist options are within (-a or -t)
int validateDirlistOption(char *option) {
    if (strcmp(option, "-a") == 0 || strcmp(option, "-t") == 0) {
        return 0;
    } else {
        return -1;
    }
}

//check filename has proper extension and name like (hello.txt)
int checkFileName(char *filename) {
    regex_t regex;
    int return_regex;

    return_regex = regcomp(&regex, "^[A-Za-z0-9_-]+\\.[A-Za-z0-9]{1,5}$", REG_EXTENDED);
    if (return_regex) {
        fprintf(stderr, "Could not compile regex\n");
        return 1;
    }

    return_regex = regexec(&regex, filename, 0, NULL, 0);
    if (!return_regex) {
        regfree(&regex);
        return 0;
    } else if (return_regex == REG_NOMATCH) {
        regfree(&regex);
        return -1;
    } else {
        regfree(&regex);
        fprintf(stderr, "Regex match failed\n");
        return -1;
    }
}

//checks if the file extension are at max 3 and with proper characters
int validateFileExtensions(char *extensions) {
    regex_t regex;
    int return_regex;

    return_regex = regcomp(&regex, "^[a-zA-Z0-9]+(\\s[a-zA-Z0-9]+){0,2}$", REG_EXTENDED);
    if (return_regex) {
        fprintf(stderr, "Could not compile regex\n");
        return 1;
    }

    return_regex = regexec(&regex, extensions, 0, NULL, 0);
    if (!return_regex) {
        regfree(&regex);
        return 0;
    } else if (return_regex == REG_NOMATCH) {
        regfree(&regex);
        return -1;
    } else {
        regfree(&regex);
        fprintf(stderr, "Regex match failed\n");
        return -1;
    }
}

//validate date with format (2024-4-12)
int validateDate(char *date) {
    regex_t regex;
    int return_regex;

    return_regex = regcomp(&regex, "^[0-9]{4}-[0-9]{1,2}-[0-9]{1,2}$", REG_EXTENDED);
    if (return_regex) {
        fprintf(stderr, "Could not compile regex\n");
        return 1;
    }

    return_regex = regexec(&regex, date, 0, NULL, 0);
    if (!return_regex) {
        regfree(&regex);
        return 0;
    } else if (return_regex == REG_NOMATCH) {
        regfree(&regex);
        return -1;
    } else {
        regfree(&regex);
        fprintf(stderr, "Regex match failed\n");
        return -1;
    }
}

void setupConnection(char *server_ip, int server_port) {

    printf("Connecting to server...\n");

    while (1) {
        if ((he = gethostbyname(server_ip)) == NULL) {
            fprintf(stderr, "Cannot get host name\n");
            exit(1);
        }

        if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            fprintf(stderr, "Socket Failure!!\n");
            exit(1);
        }

        memset(&server_info, 0, sizeof(server_info));
        server_info.sin_family = AF_INET;
        server_info.sin_port = htons(server_port);
        server_info.sin_addr = *((struct in_addr *)he->h_addr);

        if (connect(sock_fd, (struct sockaddr *)&server_info, sizeof(struct sockaddr)) < 0) {
            perror("connect");
            exit(1);
        }

        if ((num = read(sock_fd, buff, BUFFER_SIZE)) == -1) {
            perror("recv");
            exit(1);
        }
        if (strstr(buff, "continue_status") != NULL) {
            break;
        } else {
            printf("Switching to mirror...\n");
            server_ip = strtok(buff, " ");
            server_port = atoi(strtok(NULL, " "));
            close(sock_fd);
        }
    }
}

int sendRequest() {

    char buffer_copy[150];
    strcpy(buffer_copy, buffer);
    //requests that will have the text as response from server
    if (strstr(buffer, "dirlist") != NULL || strstr(buffer, "w24fn") != NULL) {

        if (strstr(buffer, "dirlist") != NULL && (validateDirlistOption(buffer_copy + strlen("dirlist") + 1) < 0)) {
            printf("Invalid dirlist option\n");
            return 1;
        }

        if (strstr(buffer, "w24fn") != NULL && checkFileName(buffer_copy + strlen("w24fn") + 1) < 0) {
            printf("Invalid file name\n");
            return 1;
        }

        if (write(sock_fd, buffer, strlen(buffer)) == -1) {
            fprintf(stderr, "Failure Sending Message\n");
            close(sock_fd);
            exit(1);
        }

        if ((num = read(sock_fd, buff, BUFFER_SIZE)) == -1) {
            perror("recv");
            exit(1);
        }
        if (num == 0) {
            //checks for client connection status
            printf("Client disconnected.\n");
            close(sock_fd);
            exit(0);
        }

        buff[num] = '\0';
        printf("%s\n", buff);
    }
    //requests that will have the tar file as response from server
    else if (strstr(buffer, "w24fz") != NULL || strstr(buffer, "w24ft") != NULL ||
             strstr(buffer, "w24fdb") != NULL || strstr(buffer, "w24fda") != NULL) {

        if (strstr(buffer, "w24fz") != NULL && validateSizeRange(buffer_copy + strlen("w24fz") + 1) < 0) {
            printf("Invalid File Size Range\n");
            return 1;
        }

        if (strstr(buffer, "w24ft") != NULL && validateFileExtensions(buffer_copy + strlen("w24ft") + 1) < 0) {
            printf("Invalid File extensions(Max 3 ext)\n");
            return 1;
        }

        if ((strstr(buffer, "w24fdb") != NULL || strstr(buffer, "w24fda") != NULL) && validateDate(buffer_copy + strlen("w24fdb") + 1) < 0) {
            printf("Invalid date provided\n");
            return 1;
        }

        if (strstr(buffer, "w24fdb") != NULL || strstr(buffer, "w24fda") != NULL) {
            //extract the epoch time from the given date to compare it with the created time of files
            char *year = strtok(buffer_copy + strlen("w24fdb") + 1, "-");
            char *month = strtok(NULL, "-");
            char *day = strtok(NULL, "-");
            struct tm tm;
            memset(&tm, 0, sizeof(tm));
            tm.tm_year = atoi(year) - 1900;
            tm.tm_mon = atoi(month)-1;
            tm.tm_mday = atoi(day);
            time_t date = mktime(&tm);

            sprintf(buffer, "%s %ld", strtok(buffer_copy, " "), date);
        }

        //send the request to server
        if (write(sock_fd, buffer, strlen(buffer)) == -1) {
            fprintf(stderr, "Failure Sending Message\n");
            close(sock_fd);
            exit(1);
        }
        printf("request sent to server...\n");
        if ((num = read(sock_fd, buff, BUFFER_SIZE)) == -1) {
            perror("recv");
            exit(1);
        }
        if (num == 0) {
            //checks for client connection status
            printf("Client disconnected.\n");
            close(sock_fd);
            exit(0);
        }
        //received the size of file
        if (strstr(buff, "file#") != NULL) {
            printf("Downloading...(please wait)\n");
            strtok(buff, "#");
            char *size = strtok(NULL, "#");
            char dir_path[50];
            //create a directory to store the tar file
            sprintf(dir_path, "%s/w24project", getenv("HOME"));
            if (access(dir_path, F_OK) == -1) {
                if (mkdir(dir_path, 0777) == -1) {
                    fprintf(stderr, "Error creating directory: %s\n", dir_path);
                    exit(1);
                }
            }
            char tar_path[50];
            //specify the tar file to store the incoming buffer
            sprintf(tar_path, "%s/temp.tar.gz", dir_path);
            long total_bytes = atoi(size);
            long received_bytes = 0;
            //open file as write only with append and trunc to override if already exists
            int fd = open(tar_path, O_CREAT | O_WRONLY | O_APPEND | O_TRUNC, 0777);
            while (received_bytes < total_bytes) {
                //start receiving file buffer from server
                if ((num = read(sock_fd, buff, BUFFER_SIZE)) > 0) {
                    sleep(0.1);
                    received_bytes += num;
                    write(fd, buff, num);
                } else if (num == 0) {
                    return 0;
                } else {
                    fprintf(stderr, "Error receiving file\n");
                    return 0;
                }
            }
            printf("Download Complete(Total bytes received %d)\n", received_bytes);
        } else {
            printf("%s\n", buff);
        }
    } else if (strcmp(buffer, "quitc") == 0) {
        //send the disconnection message to sever
        printf("Exiting...\n");
        if (write(sock_fd, buffer, strlen(buffer)) == -1) {
            fprintf(stderr, "Failure Sending Message\n");
            close(sock_fd);
        }
        return 0;
    } else {
        printf("Invalid Command.\n");
        return 1;
    }
}

int main(int argc, char *argv[]) {

    char *SERVER_IP = "127.0.0.1";
    int SERVER_PORT = 8000;

    setupConnection(SERVER_IP, SERVER_PORT);

    while (1) {
        printf("clientw24$ ");
        fgets(buffer, BUFFER_SIZE - 1, stdin);
        if (strcspn(buffer, "\n") == 0) {
            continue;
        }
        //remove the \n to avoid any errors
        buffer[strcspn(buffer, "\n")] = '\0';

        if(sendRequest()){
            continue;
        }
    }

    close(sock_fd);
    return 0;
}