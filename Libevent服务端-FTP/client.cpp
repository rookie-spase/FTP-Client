#include <fstream>
#include <iostream>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <event2/buffer.h>

int filename_len = 0;
char filename[255] = {0};
int file_len = 0;
int flag = 0;



// 为什么？为什么把前三个读取都加到一个if里面，这个函数就执行一次就完了？
// 刚才不是执行了4次吗？
// 现在读取的东西太多的话又会执行两次，第二次又不会执行flag++
// 最后的解决方式：把三个读取信息的地方加到一个if里面，在if的最后flag++
// 解析：不知道
// 总结：this must be magic . the magic happend right there!
void read_cb(struct bufferevent *bev, void *ctx) {     // 这个函数调用了2次
    
    printf("flag:%d\n",flag);  // 为什么这个值没变过,难道是程序重启了？
                            // 答：并没有
                            // 解析：不知道

    // 这个if会调用多次
    if(flag == 0){              // 

        // 读取文件名的大小
        bufferevent_read(bev, &filename_len, sizeof(filename_len));
        filename_len = ntohl(filename_len);
        printf("this is filename_len:%d\n",filename_len);

        // 读取文件名   
        bufferevent_read(bev, filename, filename_len); // 这个函数调用了2次,当数据量大的时候，read_cb会调用三次
        printf("Received filename: %s\n", filename);

        // 读取文件大小
        bufferevent_read(bev, &file_len, sizeof(file_len));
        printf("Received file size: %d\n", file_len);
        flag++;                     // 最后还得是加在这里，下面while中的又出毛病了

    }

    std::ofstream *output_file = static_cast<std::ofstream*>(ctx);
    char buffer[1024];
    int n;
    while ((n = bufferevent_read(bev, buffer, sizeof(buffer))) > 0) {
        output_file->write(buffer, n);
        //flag++;          // 怪了，放在这里就可以了。 而且一次加了4，这个while执行了4次？
                        // 而且为什么放这里就可以执行，而括号外一次都没有
                        // 如果括号外一次都没有执行为什么printf这个flag的语句会执行很多次
                        // 有时候写代码挺玄学的，现在知道为什么改了一行就动不了了
    }

    // flag++;  // 在这里加的话，这个flag一次都没有执行
}

#include <string.h>
void event_cb(struct bufferevent *bev, short events, void *ctx) {
    if (events & BEV_EVENT_EOF) {
        std::cout << "Connection closed." << std::endl;
    } else if (events & BEV_EVENT_ERROR) {
        std::cout << "Got an error on the connection: " << strerror(errno) << std::endl;
    }
    bufferevent_free(bev);
    std::ofstream *output_file = static_cast<std::ofstream*>(ctx);
    output_file->close();
    delete output_file;
}

int main() {
    struct event_base *base = event_base_new();
    if (!base) {
        perror("Could not initialize libevent!");
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        event_base_free(base);
        return 1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(1231);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(sockfd);
        event_base_free(base);
        return 1;
    }

    std::ofstream *output_file = new std::ofstream("2.txt", std::ios::binary);
    if (!output_file->is_open()) {
        perror("Failed to open file 2.txt");
        close(sockfd);
        event_base_free(base);
        delete output_file;
        return 1;
    }

    struct bufferevent *bev = bufferevent_socket_new(base, sockfd, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, read_cb, NULL, event_cb, output_file);
    bufferevent_enable(bev, EV_READ | EV_WRITE);

    event_base_dispatch(base);

    event_base_free(base);

    return 0;
}