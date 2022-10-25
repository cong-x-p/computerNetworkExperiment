#include <cstring>
#include <cstdio>
#include "thread"
#include "sys/types.h"
#include "sys/socket.h"
#include "netinet/in.h"
#include "unistd.h"
#include "iostream"
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "pthread.h"
#include "algorithm"
#include "fstream"
#include "fcntl.h"

using namespace std;

#define SERV_HTTP_PORT 80
#define SERV_HTTPS_PORT 443
#define MAX_LINE 1024
#define THREAD_NUM 2
#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"
char bufferHttp[MAX_LINE] = {0};
char bufferHttps[MAX_LINE] = {0};

void getVideoFile(SSL *ssl, const string &fileName);

void getFile(SSL *ssl, const string &fileName);

void request206Http(int client, int beginNum, int endNum, int needLen, int video_length);

void request206(SSL *ssl, int beginNum, int endNum, int needLen, int video_length);

void request200(SSL *ssl);

void request404(SSL *ssl);

void request301(int client, string args_);

void parseHttp(int client);

void parseHttps(SSL *ssl);

int listenBind(u_short port);

void httpStartUp(u_short port);

void httpsStartUp();


void getVideoFileHttp(int client, const string &fileName, int index) {
    char buf[BUFSIZ];
    string fileNameTemp = fileName.substr(1);
    const char *fileName_ = fileNameTemp.c_str();
    int video_fd = open(fileName_, O_RDONLY);
    int video_length = lseek(video_fd, 0, SEEK_END) - lseek(video_fd, 0, SEEK_SET);

//    cout << video_length << endl;
    string bufferHttps_(bufferHttp);
    string requestRange = bufferHttps_.substr(index);
    int endIndex = 0;
    for (int i = 0; i < strlen(requestRange.c_str()); i++) {
        if (requestRange[i] == '\r') {
            endIndex = i;
            break;
        }
    }
//    cout << endIndex << endl;
//    SSL_write(ssl, requestRange.c_str(), strlen(requestRange.c_str()));
    int pos = requestRange.find('-');
    string begin = requestRange.substr(13, pos - 13);
    string end = requestRange.substr(pos + 1, endIndex - pos - 1);
//    SSL_write(ssl, begin.c_str(), strlen(begin.c_str()));
//    SSL_write(ssl, "\n", 1);
//    SSL_write(ssl, end.c_str(), strlen(end.c_str()));
    int beginNum = 0, endNum = 0;
    if (!begin.empty() && !end.empty()) {
        beginNum = stoi(begin);
        endNum = stoi(end);
    } else if (!begin.empty() && end.empty()) {
        beginNum = stoi(begin);
        endNum = video_length - 1;
    } else if (begin.empty() && !end.empty()) {
        beginNum = video_length - stoi(end);
        endNum = video_length - 1;
    }

    unsigned needLen = endNum - beginNum + 1;
    request206Http(client, beginNum, endNum, needLen, video_length);
    lseek(video_fd, beginNum, SEEK_SET);
    int readLen = 0;
    while (needLen >= sizeof(buf)) {
        readLen = read(video_fd, buf, BUFSIZ);
        send(client, buf, readLen, 0);
//        write(client, buf, readLen);
        needLen -= readLen;
    }
    read(video_fd, buf, BUFSIZ);
    send(client, buf, needLen, 0);
//    write(client, buf, needLen);
    close(video_fd);
}

void getVideoFile(SSL *ssl, const string &fileName, int index) {
    char buf[4096];
    string fileNameTemp = fileName.substr(1);
    const char *fileName_ = fileNameTemp.c_str();
    int video_fd = open(fileName_, O_RDONLY);
    int video_length = lseek(video_fd, 0, SEEK_END) - lseek(video_fd, 0, SEEK_SET);

    string bufferHttps_(bufferHttps);
    string requestRange = bufferHttps_.substr(index);
    int endIndex = 0;
    for (int i = 0; i < strlen(requestRange.c_str()); i++) {
        if (requestRange[i] == '\r') {
            endIndex = i;
            break;
        }
    }
//    cout << endIndex << endl;
//    SSL_write(ssl, requestRange.c_str(), strlen(requestRange.c_str()));
    int pos = requestRange.find('-');
    string begin = requestRange.substr(13, pos - 13);
    string end = requestRange.substr(pos + 1, endIndex - pos - 1);
//    SSL_write(ssl, begin.c_str(), strlen(begin.c_str()));
//    SSL_write(ssl, "\n", 1);
//    SSL_write(ssl, end.c_str(), strlen(end.c_str()));
    int beginNum = 0, endNum = 0;
    if (!begin.empty() && !end.empty()) {
        beginNum = stoi(begin);
        endNum = stoi(end);
    } else if (!begin.empty() && end.empty()) {
        beginNum = stoi(begin);
        endNum = video_length - 1;
    } else if (begin.empty() && !end.empty()) {
        beginNum = video_length - stoi(end);
        endNum = video_length - 1;
    }

    unsigned needLen = endNum - beginNum + 1;
    request206(ssl, beginNum, endNum, needLen, video_length);
    lseek(video_fd, beginNum, SEEK_SET);
    int readLen = 0;
    while (needLen >= sizeof(buf)) {
        readLen = read(video_fd, buf, sizeof(buf));
        SSL_write(ssl, buf, readLen);
        needLen -= readLen;
    }
    read(video_fd, buf, sizeof(buf));
    SSL_write(ssl, buf, needLen);
    close(video_fd);
}

void getFile(SSL *ssl, const string &fileName) {
    FILE *resource = nullptr;
    string fileNameTemp = fileName.substr(1);
    const char *fileName_ = fileNameTemp.c_str();
    char currentFile[BUFSIZ];
    resource = fopen(fileName_, "r");
    if (resource == nullptr) {
        request404(ssl);
    } else {
//        SSL_write(ssl, "hello", 5);
        request200(ssl);
        fgets(currentFile, sizeof(currentFile), resource);
        while (!feof(resource)) {
            SSL_write(ssl, currentFile, strlen(currentFile));
            fgets(currentFile, sizeof(currentFile), resource);
        }
        fclose(resource);
    }
}

void request206Http(int client, int beginNum, int endNum, int needLen, int video_length) {
    char buf[1024];
    sprintf(buf, "HTTP/1.1 206 Partial Content\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Range: bytes ");
    send(client, buf, strlen(buf), 0);
    string temp = to_string(beginNum) + "-" + to_string(endNum) + "/" + to_string(video_length) + "\r\n";
    const char *temp_ = temp.c_str();
    sprintf(buf, "%s", temp_);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Length: ");
    send(client, buf, strlen(buf), 0);
    temp = to_string(video_length) + "\r\n";
    temp_ = temp.c_str();
    sprintf(buf, "%s", temp_);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: video/mp4\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

void request206(SSL *ssl, int beginNum, int endNum, int needLen, int video_length) {
    char buf[1024];
    sprintf(buf, "HTTP/1.1 206 Partial Content\r\n");
    SSL_write(ssl, buf, strlen(buf));
    sprintf(buf, "Content-Range: bytes ");
    SSL_write(ssl, buf, strlen(buf));
    string temp = to_string(beginNum) + "-" + to_string(endNum) + "/" + to_string(video_length) + "\r\n";
    const char *temp_ = temp.c_str();
    sprintf(buf, "%s", temp_);
    SSL_write(ssl, buf, strlen(buf));
    sprintf(buf, "Content-Length: ");
    SSL_write(ssl, buf, strlen(buf));
    temp = to_string(video_length) + "\r\n";
    temp_ = temp.c_str();
    sprintf(buf, "%s", temp_);
    SSL_write(ssl, buf, strlen(buf));
    sprintf(buf, SERVER_STRING);
    SSL_write(ssl, buf, strlen(buf));
    sprintf(buf, "Content-Type: video/mp4\r\n");
    SSL_write(ssl, buf, strlen(buf));
    sprintf(buf, "\r\n");
    SSL_write(ssl, buf, strlen(buf));
}

void request200(SSL *ssl) {
    char buf[1024];
    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    SSL_write(ssl, buf, strlen(buf));
    sprintf(buf, SERVER_STRING);
    SSL_write(ssl, buf, strlen(buf));
    sprintf(buf, "Content-Type: text/html\r\n");
    SSL_write(ssl, buf, strlen(buf));
    sprintf(buf, "\r\n");
    SSL_write(ssl, buf, strlen(buf));
}

void request404(SSL *ssl) {
    char buf[1024];
    sprintf(buf, "HTTP/1.1 404 NOT FOUND\r\n");
    SSL_write(ssl, buf, strlen(buf));
    sprintf(buf, SERVER_STRING);
    SSL_write(ssl, buf, strlen(buf));
    sprintf(buf, "Content-Type: text/html\r\n");
    SSL_write(ssl, buf, strlen(buf));
    sprintf(buf, "\r\n");
    SSL_write(ssl, buf, strlen(buf));
}

void request301(int client, string args_) {
    char buf[1024];
    sprintf(buf, "HTTP/1.1 301 Moved Permanently\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Location: https://127.0.0.1");
    send(client, buf, strlen(buf), 0);
    args_ = args_ + "\r\n";
    const char *arg = args_.c_str();
    sprintf(buf, "%s", arg);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Cache-control: private; max-age=600");
    send(client, buf, strlen(buf), 0);
}

void parseHttp(int client) {
    char method[500];
    char args[500];
    char protocal[500];
    sscanf(bufferHttp, "%s %s %s", method, args, protocal);
//    cout << method << endl;
//    cout << args << endl;
//    cout << protocal << endl;
    string method_(method);
    string args_(args);
    string protocal_(protocal);
//    cout << method_ << endl;
//    cout << args_ << endl;
//    cout << protocal_ << endl;
    if (method_ == "GET") {
        string strFind = "Range";
        string bufferHttps_(bufferHttp);
        cout << bufferHttps_ << endl;
        int index = 0;
        index = bufferHttps_.find(strFind);
        cout << index << endl;
        if (index < bufferHttps_.length()) {
            getVideoFileHttp(client, args_, index);
        } else {
            request301(client, args_);
        }
//        request301(client, args_);
    }
    close(client);
}

void parseHttps(SSL *ssl) {
    char method[500];
    char args[500];
    char protocal[500];
    sscanf(bufferHttps, "%s %s %s", method, args, protocal);
//    cout << method << endl;
//    cout << args << endl;
//    cout << protocal << endl;
    string method_(method);
    string args_(args);
    string protocal_(protocal);
//    cout << method_ << endl;
//    cout << args_ << endl;
//    cout << protocal_ << endl;
    if (method_ == "GET") {
        string strFind = "Range";
        string bufferHttps_(bufferHttps);
        int index = bufferHttps_.find(strFind);
        if (index < bufferHttps_.length()) {
            getVideoFile(ssl, args_, index);
        } else {
            getFile(ssl, args_);
        }
    }
}

int listenBind(u_short port) {
    struct sockaddr_in servaddr{};
    int listenfd;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    listen(listenfd, 50);

    return listenfd;
}


void httpStartUp(u_short port) {
    struct sockaddr_in clientaddr{};
    socklen_t clientaddrLen;
    int connfd;
    int listenfd = listenBind(SERV_HTTP_PORT);
    cout << "http connection success" << endl;

    while (true) {
        clientaddrLen = sizeof(clientaddr);
        connfd = accept(listenfd, (struct sockaddr *) &clientaddr, &clientaddrLen);
        long n = recv(connfd, bufferHttp, MAX_LINE, 0);
        bufferHttp[n] = '\0';
        if (n != 0) {
//            cout << bufferHttp << endl;
            parseHttp(connfd);
        } else {
            cout << "close connection..." << endl;
            break;
        }
    }
    close(connfd);
}

void httpsStartUp() {
    SSL_library_init();
    SSL_load_error_strings();
    SSLeay_add_ssl_algorithms();
    OpenSSL_add_all_algorithms();
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_server_method());

    if (ctx == nullptr) {
        cout << "error" << endl;
    }

    int ret = SSL_CTX_use_certificate_file(ctx, "cnlab.cert", SSL_FILETYPE_PEM);
    ret = SSL_CTX_use_PrivateKey_file(ctx, "cnlab.prikey", SSL_FILETYPE_PEM);
    ret = SSL_CTX_check_private_key(ctx);
    int sockS = listenBind(SERV_HTTPS_PORT);
    cout << "https connection success" << endl;

    while (true) {
        struct sockaddr_in sinform{};
        socklen_t sinformlen = sizeof(sinform);
        int sockC = accept(sockS, (struct sockaddr *) &sinform, &sinformlen);
        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, sockC);
        ret = SSL_accept(ssl);

        char sBuf[MAX_LINE] = {0};
        int bytesin = SSL_read(ssl, bufferHttps, sizeof(sBuf) - 1);
        if (bytesin <= 0) {
            cout << "error2" << endl;
            break;
        }
        parseHttps(ssl);
//        cout << bufferHttps << endl;
//        SSL_write(ssl, "hello", 5);
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(sockC);
    }

    close(sockS);
    SSL_CTX_free(ctx);
}


int main() {
    pthread_t threads[THREAD_NUM];
    int ret1 = pthread_create(&threads[0], nullptr, reinterpret_cast<void *(*)(void *)>(httpStartUp), nullptr);
    int ret2 = pthread_create(&threads[1], nullptr, reinterpret_cast<void *(*)(void *)>(httpsStartUp), nullptr);
    pthread_exit(nullptr);
    return 0;
}
