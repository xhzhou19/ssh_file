#include <functional>
#include <libssh2.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <unistd.h>
#include <string>
#include <vector>
#include <libssh2_sftp.h>
#include <fstream>
#include <vector>
#include <string>

class SSHReader {
private:
    LIBSSH2_SESSION *session;
    LIBSSH2_CHANNEL *channel;
    LIBSSH2_SFTP *sftp_session;
    int sock;

public:
    SSHReader() : session(nullptr), channel(nullptr), sftp_session(nullptr), sock(-1) {}

    bool connect(const std::string& host, int port, const std::string& username, const std::string& password) {
        // 初始化libssh2
        if (libssh2_init(0) != 0) {
            return false;
        }

        // 创建socket连接
        sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in sin;
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);
        sin.sin_addr.s_addr = inet_addr(host.c_str());

        if (::connect(sock, (struct sockaddr*)(&sin), sizeof(sin)) != 0) {
            return false;
        }

        // 创建SSH会话
        session = libssh2_session_init();
        if (libssh2_session_handshake(session, sock) != 0) {
            return false;
        }

        // 认证
        if (libssh2_userauth_password(session, username.c_str(), password.c_str()) != 0) {
            return false;
        }

        // 初始化SFTP会话
        sftp_session = libssh2_sftp_init(session);
        if (!sftp_session) {
            return false;
        }

        return true;
    }

    std::vector<std::string> listRemoteDirectory(const std::string& remotePath) {
        std::vector<std::string> fileList;
        LIBSSH2_SFTP_HANDLE *sftp_handle = libssh2_sftp_opendir(sftp_session, remotePath.c_str());
        if (!sftp_handle) {
            return fileList;
        }

        char buffer[512];
        LIBSSH2_SFTP_ATTRIBUTES attrs;

        while (libssh2_sftp_readdir(sftp_handle, buffer, sizeof(buffer), &attrs) > 0) {
            if (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) {
                if (!LIBSSH2_SFTP_S_ISDIR(attrs.permissions)) {
                    fileList.push_back(remotePath + "/" + buffer);
                }
            }
        }

        libssh2_sftp_closedir(sftp_handle);
        return fileList;
    }

    bool readRemoteFile(const std::string& remotePath, std::function<void(const char*, size_t)> processor) {
        LIBSSH2_SFTP_HANDLE* sftp_handle = libssh2_sftp_open(sftp_session,
                                                            remotePath.c_str(),
                                                            LIBSSH2_FXF_READ,
                                                            0);
        if (!sftp_handle) {
            std::cerr << "Failed to open remote file via SFTP: " << remotePath << std::endl;
            return false;
        }

        // 设置非阻塞模式
        libssh2_session_set_blocking(session, 0);

        char buffer[8192];
        ssize_t bytesRead;
        bool success = true;

        while (true) {
            bytesRead = libssh2_sftp_read(sftp_handle, buffer, sizeof(buffer));

            if (bytesRead > 0) {
                processor(buffer, bytesRead);
            } else if (bytesRead == 0) {
                // 文件读取完成
                break;
            } else if (bytesRead == LIBSSH2_ERROR_EAGAIN) {
                // 需要等待更多数据
                usleep(100000); // 等待100ms
                continue;
            } else {
                std::cerr << "Error reading remote file: " << remotePath << std::endl;
                success = false;
                break;
            }
        }

        // 恢复阻塞模式
        libssh2_session_set_blocking(session, 1);

        // 关闭SFTP句柄
        libssh2_sftp_close(sftp_handle);

        return success;
    }

    ~SSHReader() {
        if (channel) libssh2_channel_free(channel);
        if (sftp_session) libssh2_sftp_shutdown(sftp_session);
        if (session) {
            libssh2_session_disconnect(session, "Normal Shutdown");
            libssh2_session_free(session);
        }
        if (sock != -1) close(sock);
        libssh2_exit();
    }
};
int main() {
    SSHReader reader;

    if (!reader.connect("xxx", 22, "xx", "xx")) {
        std::cerr << "Connection failed\n";
        return 1;
    }

    std::vector<std::string> fileList = reader.listRemoteDirectory("/home/frpc");

    for (const auto& filePath : fileList) {
        std::cout << "Processing file: " << filePath << std::endl;

        std::string localFilePath = "./" + filePath.substr(filePath.find_last_of("/") + 1);
        std::ofstream localFile(localFilePath, std::ios::binary);

        if (!localFile.is_open()) {
            std::cerr << "Failed to open local file: " << localFilePath << std::endl;
            continue;
        }

        size_t totalBytes = 0;
        auto processor = [&localFile, &totalBytes](const char* data, size_t length) {
            localFile.write(data, length);
            totalBytes += length;
            std::cout << "\rReceived: " << totalBytes << " bytes" << std::flush;
        };

        if (!reader.readRemoteFile(filePath, processor)) {
            std::cerr << "\nFailed to read remote file: " << filePath << std::endl;
        } else {
            std::cout << "\nSuccessfully downloaded: " << localFilePath << std::endl;
        }

        localFile.close();
    }

    return 0;
}