#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <unistd.h>
#include <iostream>
void client() {

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);



       // struct sockaddr_in {
       //     sa_family_t     sin_family;     /* AF_INET */
       //     in_port_t       sin_port;       /* Port number */
       //     struct in_addr  sin_addr;       /* IPv4 address */
       // };

    sockaddr_in client_addr;

    // initialize
    bzero(&client_addr, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    client_addr.sin_port = htons(8888);


    // connecting server
    //    int connect(int sockfd, const struct sockaddr *addr,
    //             socklen_t addrlen);
    connect(sockfd, (sockaddr*)&client_addr, sizeof(client_addr));

    while(true) {
        char buf[1024];
        bzero(&buf, sizeof(buf));
        std::cin>>buf;
        //        ssize_t write(int fd, const void buf[.count], size_t count);
        ssize_t write_bytes = write(sockfd, buf, sizeof(buf));

        
    }

}





int main(int argc, char** argv) {

    client();
    return 0;
}
