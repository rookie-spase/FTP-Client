#include <event2/event.h>
#include <event2/listener.h>
#include <event2/util.h> // Include for evutil_make_socket_nonblocking
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include "Frame/EveForge.h"
#include <sys/stat.h>


#include <dirent.h>
#include <cstring> // 为了使用 strlen
#include <sys/socket.h> // 为了使用 send 函数


// 这个用于获取一个文件下的文件和目录，因为只需要获取到一层，就没必要像TRE指令那样复杂了。
void dir_instruction(const char* path, evutil_socket_t fd)
{
    DIR *dir;
    struct dirent *entry;
    char buffer[1024];

    if ((dir = opendir(path)) != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            snprintf(buffer, sizeof(buffer), "%s\n", entry->d_name);
            send(fd, buffer, strlen(buffer), 0);
            printf("send:%s",buffer);
        }
        closedir(dir);
    } else {
        // 无法打开目录
        perror("Failed to open directory");
        return;
    }

}


/* 为了递归获取文件目录，我制定了如下的文件列表协议
 * 文件树
├── 1.txt
├── 2.txt
└── abcd
    ├── 123123
    └── abcdabcd
        └── bbbbbbbbbbb
 * 那么发送则是这种形式， 如此往复，每多加一层则多一个'-'(深度)。
 * Qt端使用出了QStack来处理，Linux端使用出了递归来发送
send:1.txt|FILE
send:2.txt|FILE
send:abcd|DIR
send:123123|-FILE
send:abcdabcd|-DIR
send:bbbbbbbbbbb|--FILE
 */
void TRE_instruction_Core_Function(const char* path, int fd, int depth = 0)
{
    DIR *dir;
    struct dirent *entry;
    struct stat entry_stat;
    char buffer[1024];
    char prefix[1024] = "";

    // 根据深度决定前缀，每一层深度增加一个"-"符号
    for (int i = 0; i < depth; i++) {
        strcat(prefix, "-");
    }

    if ((dir = opendir(path)) != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
            stat(full_path, &entry_stat);
            if (S_ISDIR(entry_stat.st_mode)) {
                if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                    sprintf(buffer, "%s|%sDIR\n", entry->d_name, prefix);
                    send(fd, buffer, strlen(buffer), 0);
                    printf("send:%s", buffer);
                    TRE_instruction_Core_Function(full_path, fd, depth + 1); // 递归调用处理子目录，增加深度
                }
            } else {
                sprintf(buffer, "%s|%sFILE\n", entry->d_name,prefix); // 修改了这里的格式，将文件名和前缀的位置调整正确
                send(fd, buffer, strlen(buffer), 0);
                printf("send:%s", buffer);
            }
        }
        closedir(dir);
    } else {
        perror("Failed to open directory");
        return;
    }
}
void DNL_instruction(const char* file,evutil_socket_t fd)
{
    // const char* filename = "1.txt";
    const char* filename = file;
    int filename_len = strlen(filename);
    int file_len = 0; // 文件的大小
    struct stat file_stat; // Declare a stat structure
    // Get file statistics using stat
    if (stat(filename, &file_stat) == 0) {
        file_len = file_stat.st_size; // Assign file size from stat structure
        printf("\nFile size: %d bytes\n", file_len);
    } else {
        printf("Error retrieving file size: %s\n", strerror(errno));
    }
    // 发送文件名的大小
    filename_len = htonl(filename_len);

    send(fd, &filename_len, sizeof(filename_len), 0);  // 发送文件名大小
    send(fd, filename, strlen(filename), 0);  // 发送文件名
    send(fd, &file_len, sizeof(file_len), 0);  // 发送文件大小
    EveForge_Send_File_to_fd(filename,fd);  // 发送文件内容
    // close(fd);  // 这是最开始做调试的，放到现在执行一次就不能执行简直荒谬

}

void TRE_instruction(evutil_socket_t fd)
{
    // 发送固定路径下的所有文件和目录
    printf("\nTRE instruction\n");
    TRE_instruction_Core_Function("/code_side/libevent/FTP_Dictionary", fd);
}

// 对应的是删除文件或文件夹的指令，传入一个文件路径，然后删除它
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
void DEL_instruction(const char* file_path,evutil_socket_t fd) {
    struct stat path_stat;
    stat(file_path, &path_stat);
    if (S_ISDIR(path_stat.st_mode)) {
        // 如果是文件夹
        if (rmdir(file_path) == 0) {
            printf("Directory deleted successfully.\n");
        } else {
            perror("Error deleting directory");
        }
    } else {
        // 如果是文件
        if (unlink(file_path) == 0) {
            printf("File deleted successfully.\n");
        } else {
            perror("Error deleting file");
        }
    }
}


// // 上传的逻辑
// #include <cstring>
// #include <fstream>
// #include <iostream>
// int filename_len = 0;
// // 这里完全接收不到，也不知Qt是发送缓冲区没写完吗还是。这里啥都没有，也就谈不上什么东西了
// void UPL_instruction(const char* dir_path, struct bufferevent *bev) // 使用libevent处理数据接收
// {                                                                    // 但是，什么也没读到 ，猜想，难道因为客户端第二次发送过来的时候发到的函数是读回调？
//                                                                 // 那么，一次性读完所有东西，是否可行
//                                                                 // 其实是在while卡住了
//     std::cout << "\nUpload to " << dir_path << std::endl;

    
//         int n = bufferevent_read(bev, &filename_len, sizeof(filename_len));
//         filename_len = ntohl(filename_len);
//         printf("n:%d",n);
//         printf("this is filename_len:%d\n",filename_len);

//     // 创建缓冲区存储接收的数据
//     char buffer[1024];
//     size_t read = 0;

//     // // 循环读取数据，直到没有数据可读
//     // while ((read = bufferevent_read(bev, buffer, sizeof(buffer))) > 0) {
//     //     std::cout << buffer;
//     // }
// }
//     static std::string buffer;


void read_cb(struct bufferevent *bev, void *ctx)
{
    char data[1024];
    int n;
    
    while ((n = bufferevent_read(bev, data, sizeof(data)-1)) > 0) {
        data[n] = '\0';
        printf("original data:%s",data);
        if (strncmp(data, "DIR", 3) == 0) {  // 从buffer读取
            printf("\n:%s\n",data);
            // 根据客户端给出的数据，下载
            char path[1024];
            strcpy(path,data+4);
                dir_instruction(path, bufferevent_getfd(bev));
        } else if (strncmp(data, "TRE", 3) == 0) {  // 发送固定目录之下的路径
            TRE_instruction(bufferevent_getfd(bev));
        }
        else if (strncmp(data, "DNL", 3) == 0) {
            // 这里需要接收Qt发送的数据，数据格式：[文件名路径长度（4字节），文件路径]
            char file_path[1024];
            strcpy(file_path,data+4);
            printf("[DownLoad]:File_Path: %s",file_path);
            DNL_instruction(file_path, bufferevent_getfd(bev));
        } else if (strncmp(data, "DEL", 3) == 0) {
            char file_path[1024];
            strcpy(file_path,data+4);
            printf("[Delete]:File_Path: %s",file_path);
            DEL_instruction(file_path,bufferevent_getfd(bev));
       } else if (strncmp(data, "UPL", 3) == 0) {  // 这里是客户端上传数据，所以接收的是文件夹的名字，然后接收文件本身
                                            // 那么就有必要接收文件夹字符串的长度，以免把其他东西接收为文件夹名了
                                            // 格式：[3字符命令][4字节文件夹路径长度][文件夹路径名]
            // char file_path_size[4];
            // memcpy(&file_path_size, data + 4, 2);
            //             file_path_size[2] = '\n';
            // // file_name_size = ntohl(file_name_size);
            // printf("\nfile_path_size:%s\n",file_path_size);

            // printf("\ndata:%s\n",data);
            // char Dir_path[1024];
            // // strcpy(Dir_path,data+4+3);

            // int path_size = atoi(file_path_size); 
            // memcpy(&Dir_path, data + 4 + 3, path_size);
            // printf("\n[UPLOAD]:File_Path: %s\n",Dir_path);

            // UPL_instruction(Dir_path,bev);
        }
    }
};

void accept_conn_cb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen, void *ctx) 
{    
    struct event_base *base = evconnlistener_get_base(listener);
    struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, read_cb, NULL, NULL, NULL);
    bufferevent_enable(bev, EV_READ | EV_WRITE);
}

void accept_error_cb(struct evconnlistener *listener, void *ctx) {
    struct event_base *base = evconnlistener_get_base(listener);
    int err = EVUTIL_SOCKET_ERROR();
    fprintf(stderr, "Got an error %d (%s) on the listener. Shutting down.\n", err, evutil_socket_error_to_string(err));
    event_base_loopexit(base, NULL);
}

int main() {
    struct event_base *base = event_base_new();
    if (!base) {
        perror("Could not initialize libevent!");
        return 1;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(1231);

    int server_listen = socket(AF_INET, SOCK_STREAM, 0);
    if (server_listen == -1) {
        perror("Could not create socket");
        return 1;
    }
    evutil_make_socket_nonblocking(server_listen);

    struct evconnlistener *listener = 
        evconnlistener_new_bind(base, accept_conn_cb, NULL, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1, (struct sockaddr*)&sin, sizeof(sin));
    if (!listener) {
        perror("Could not create a listener!");
        close(server_listen);
        return 1;
    }

    evconnlistener_set_error_cb(listener, accept_error_cb);

    event_base_dispatch(base);

    evconnlistener_free(listener);
    event_base_free(base);

    return 0;
}
