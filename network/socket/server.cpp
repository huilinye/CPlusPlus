#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <iostream>
#include <unistd.h>

void server() {

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server_addr;

    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(8888);

    //    int bind(int sockfd, const struct sockaddr *addr,
    //          socklen_t addrlen);

    bind(sockfd, (sockaddr*)&server_addr, sizeof(server_addr));

    listen(sockfd, SOMAXCONN);

    // accept client
    sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    bzero(&client_addr, sizeof(client_addr));
    //    int accept(int sockfd, struct sockaddr *_Nullable restrict addr,
    //            socklen_t *_Nullable restrict addrlen);
    int client_fd = accept(sockfd, (sockaddr*)&client_addr, &len);
    //std::cout<<"New info from client: "<<client_fd<<" with IP: "<<inet_ntoa(client_addr.sin_addr)<<" and port: "<<ntohs(client_addr.sin_port)<<std::endl;

    // keep receiving data from client and display
    while(true) {


        //       ssize_t read(int fd, void buf[.count], size_t count);
        char buf[1024];
        bzero(&buf, sizeof(buf));
        ssize_t read_bytes = read(client_fd, buf, sizeof(buf));
        if(read_bytes > 0) std::cout<<std::string(buf)<<std::endl;
        else close(client_fd);
    }
}



int main(int argc, char** argv) {

    server();
    return 0;
}
