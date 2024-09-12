#include <iostream>
#include <vector>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <memory>
#include <cassert>
#include <type_traits>

// Debugging macro (can be enabled or disabled)
#define DBG_SERIALIZE(x) x

// Abstract MemoryAllocator class
class MemoryAllocator {
public:
    virtual ~MemoryAllocator() = default;
    virtual uint8_t* allocate(size_t size) = 0;
    virtual void deallocate(uint8_t* data) = 0;
};

// CPUMemoryAllocator class
class CPUMemoryAllocator : public MemoryAllocator {
public:
    uint8_t* allocate(size_t size) override {
        return new uint8_t[size];
    }
    void deallocate(uint8_t* data) override {
        delete[] data;
    }
};

// MMapMemoryAllocator class
class MMapMemoryAllocator : public MemoryAllocator {
public:
    uint8_t* allocate(size_t size) override {
        int fd = open("/tmp/mmapfile", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if (fd == -1) {
            throw std::runtime_error("Failed to open file for mmap");
        }
        if (ftruncate(fd, size) == -1) {
            close(fd);
            throw std::runtime_error("Failed to set file size for mmap");
        }
        uint8_t* data = static_cast<uint8_t*>(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
        close(fd);
        if (data == MAP_FAILED) {
            throw std::runtime_error("Failed to mmap file");
        }
        return data;
    }

    void deallocate(uint8_t* data, size_t size) override {
        if (munmap(data, size) == -1) {
            throw std::runtime_error("Failed to munmap file");
        }
    }
};

// Buffer class
class buffer {
private:
    uint8_t* data;
    size_t size;
    std::shared_ptr<MemoryAllocator> allocator;
public:
    buffer(size_t size, std::shared_ptr<MemoryAllocator> allocator)
        : size(size), allocator(allocator) {
        data = allocator->allocate(size);
    }
    ~buffer() {
        allocator->deallocate(data);
    }
    uint8_t* getData() {
        return data;
    }
    size_t getSize() const {
        return size;
    }
};

// SerializeBuffer class
class SerializeBuffer {
    // Reference to the buffer where serialized data will be stored
    std::vector<char>& buffer;

public:
    // Constructor that initializes the buffer reference
    SerializeBuffer(std::vector<char>& vec) : buffer(vec) {}

    // Method to insert raw data into the buffer
    void insert(const void* ptr, size_t sz_bytes) {
        DBG_SERIALIZE(
            std::cout << "SER " << buffer.size() << " " << sz_bytes << " : \"";
            std::cout.write(static_cast<const char*>(ptr), sz_bytes);
            std::cout << "\"\n";
        );
        const char* cptr = static_cast<const char*>(ptr);
        buffer.insert(buffer.end(), cptr, cptr + sz_bytes);
    }

    // Template method to insert a value of any type into the buffer
    template <typename T>
    void insert(const T& val) {
        static_assert(std::is_trivially_copyable<T>::value, "Type must be trivially copyable");
        insert(&val, sizeof(T));
    }

    // Variadic template method to serialize multiple values
    template <typename T, typename... Args>
    void operator()(const T& val, Args&&... args) {
        insert(val);
        (*this)(std::forward<Args>(args)...);
    }

    // Specialization for std::string to handle string serialization
    template <typename... Args>
    void operator()(const std::string& val, Args&&... args) {
        int len = val.size();
        insert(len);
        insert(val.data(), val.size());
        (*this)(std::forward<Args>(args)...);
    }

    // Base case for the variadic template recursion
    void operator()() {}
};

// DeSerializeBuffer class
class DeSerializeBuffer {
    // Pointer to the start of the buffer containing serialized data
    const char* ptr;
    // Total size of the buffer
    size_t size;
    // Current read index in the buffer
    size_t read_index = 0;

public:
    // Constructor that initializes the buffer pointer and size
    DeSerializeBuffer(const void* p, size_t len) : ptr(static_cast<const char*>(p)), size(len) {}

    // Method to extract raw data from the buffer
    const void* extract(size_t sz_bytes) {
        if (read_index + sz_bytes > size) {
            throw std::runtime_error("Buffer overflow");
        }
        size_t id = read_index;
        read_index += sz_bytes;
        DBG_SERIALIZE(
            std::cout << "DSER " << id << " " << sz_bytes << " : \"";
            std::cout.write(ptr + id, sz_bytes);
            std::cout << "\"\n";
        );
        return static_cast<const void*>(ptr + id);
    }

    // Template method to extract a value of any type from the buffer
    template <typename T>
    T extract() {
        static_assert(std::is_trivially_copyable<T>::value, "Type must be trivially copyable");
        return *static_cast<const T*>(extract(sizeof(T)));
    }

    // Method to directly extract data into a provided buffer
    void extractToBuffer(void* dest, size_t sz_bytes) {
        const void* src = extract(sz_bytes);
        std::memcpy(dest, src, sz_bytes);
    }

    // Variadic template method to deserialize multiple values
    template <typename T, typename... Args>
    void operator()(T& val, Args&&... args) {
        val = extract<T>();
        (*this)(std::forward<Args>(args)...);
    }

    // Specialization for std::string to handle string deserialization
    template <typename... Args>
    void operator()(std::string& val, Args&&... args) {
        int len = extract<int>();
        val.assign(static_cast<const char*>(extract(len)), len);
        (*this)(std::forward<Args>(args)...);
    }

    // Base case for the variadic template recursion
    void operator()() {}
};

// Array class
class array {
private:
    std::shared_ptr<buffer> buf;
public:
    array(size_t size, std::shared_ptr<MemoryAllocator> allocator) {
        buf = std::make_shared<buffer>(size, allocator);
    }
    uint8_t* getData() {
        return buf->getData();
    }
    size_t getSize() const {
        return buf->getSize();
    }
    std::shared_ptr<buffer> getBuffer() const {
        return buf;
    }

    void serialize(SerializeBuffer& serializer) const {
        size_t size = buf->getSize();
        serializer(size);
        serializer(std::vector<uint8_t>(buf->getData(), buf->getData() + size));
    }

    void deserialize(DeSerializeBuffer& deserializer) {
        size_t size;
        deserializer(size);
        buf = std::make_shared<buffer>(size, std::make_shared<CPUMemoryAllocator>());
        deserializer.extractToBuffer(buf->getData(), size);
    }
};

// MMapArray class
class mmap_array {
private:
    std::shared_ptr<buffer> buf;
public:
    mmap_array(size_t size, std::shared_ptr<MemoryAllocator> allocator) {
        buf = std::make_shared<buffer>(size, allocator);
    }
    uint8_t* getData() {
        return buf->getData();
    }
    size_t getSize() const {
        return buf->getSize();
    }
    std::shared_ptr<buffer> getBuffer() const {
        return buf;
    }

    void serialize(SerializeBuffer& serializer) const {
        size_t size = buf->getSize();
        serializer(size);
        serializer(std::vector<uint8_t>(buf->getData(), buf->getData() + size));
    }

    void deserialize(DeSerializeBuffer& deserializer) {
        size_t size;
        deserializer(size);
        buf = std::make_shared<buffer>(size, std::make_shared<MMapMemoryAllocator>());
        deserializer.extractToBuffer(buf->getData(), size);
    }
};

// DataObj class
class dataobj {
public:
    std::vector<array> arrays;

    dataobj() = default;
    dataobj(const std::vector<array>& arrays) : arrays(arrays) {}

    void serialize(SerializeBuffer& serializer) const {
        serializer(arrays);
    }

    void deserialize(DeSerializeBuffer& deserializer) {
        deserializer(arrays);
    }
};

// IPCStrategy Interface
class IPCStrategy {
public:
    virtual ~IPCStrategy() = default;
    virtual void send(const std::vector<char>& data) = 0;
    virtual std::vector<char> receive() = 0;
};

// SocketIPC class
class SocketIPC : public IPCStrategy {
private:
    std::string ip;
    int port;
    int sockfd;
    struct sockaddr_in server_addr;

    void setupConnection() {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
            throw std::runtime_error("Invalid address/ Address not supported");
        }

        if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            throw std::runtime_error("Connection Failed");
        }
    }

    void closeConnection() {
        close(sockfd);
    }

public:
    SocketIPC(const std::string& ip, int port) : ip(ip), port(port) {}

    void send(const std::vector<char>& data) override {
        setupConnection();
        ssize_t sent_bytes = ::send(sockfd, data.data(), data.size(), 0);
        if (sent_bytes < 0) {
            throw std::runtime_error("Failed to send data");
        }
        closeConnection();
    }

    std::vector<char> receive() override {
        setupConnection();
        std::vector<char> buffer(1024);
        ssize_t received_bytes = ::recv(sockfd, buffer.data(), buffer.size(), 0);
        if (received_bytes < 0) {
            throw std::runtime_error("Failed to receive data");
        }
        buffer.resize(received_bytes);
        closeConnection();
        return buffer;
    }
};

// FileIPC class
class FileIPC : public IPCStrategy {
private:
    std::string filename;

public:
    FileIPC(const std::string& filename) : filename(filename) {}

    void send(const std::vector<char>& data) override {
        std::ofstream ofs(filename, std::ios::binary);
        if (!ofs) {
            throw std::runtime_error("Failed to open file for writing");
        }
        ofs.write(data.data(), data.size());
    }

    std::vector<char> receive() override {
        std::ifstream ifs(filename, std::ios::binary | std::ios::ate);
        if (!ifs) {
            throw std::runtime_error("Failed to open file for reading");
        }
        std::streamsize size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);
        std::vector<char> buffer(size);
        if (!ifs.read(buffer.data(), size)) {
            throw std::runtime_error("Failed to read file");
        }
        return buffer;
    }
};

// SharedMemoryIPC class
class SharedMemoryIPC : public IPCStrategy {
private:
    std::string shm_name;
    size_t size;
    int shm_fd;
    void* shm_ptr;

public:
    SharedMemoryIPC(const std::string& shm_name, size_t size) : shm_name(shm_name), size(size) {
        shm_fd = shm_open(shm_name.c_str(), O_CREAT | O_RDWR, 0666);
        if (shm_fd == -1) {
            throw std::runtime_error("Failed to create shared memory");
        }
        if (ftruncate(shm_fd, size) == -1) {
            throw std::runtime_error("Failed to set size of shared memory");
        }
        shm_ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (shm_ptr == MAP_FAILED) {
            throw std::runtime_error("Failed to map shared memory");
        }
    }

    ~SharedMemoryIPC() {
        munmap(shm_ptr, size);
        close(shm_fd);
        shm_unlink(shm_name.c_str());
    }

    void send(const std::vector<char>& data) override {
        if (data.size() > size) {
            throw std::runtime_error("Data size exceeds shared memory size");
        }
        std::memcpy(shm_ptr, data.data(), data.size());
    }

    std::vector<char> receive() override {
        std::vector<char> buffer(size);
        std::memcpy(buffer.data(), shm_ptr, size);
        return buffer;
    }
};

// PipeIPC class
class PipeIPC : public IPCStrategy {
private:
    std::string pipe_name;

public:
    PipeIPC(const std::string& pipe_name) : pipe_name(pipe_name) {
        mkfifo(pipe_name.c_str(), 0666);
    }

    void send(const std::vector<char>& data) override {
        int fd = open(pipe_name.c_str(), O_WRONLY);
        if (fd == -1) {
            throw std::runtime_error("Failed to open pipe for writing");
        }
        write(fd, data.data(), data.size());
        close(fd);
    }

    std::vector<char> receive() override {
        int fd = open(pipe_name.c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::runtime_error("Failed to open pipe for reading");
        }
        std::vector<char> buffer(1024);
        ssize_t received_bytes = read(fd, buffer.data(), buffer.size());
        if (received_bytes == -1) {
            throw std::runtime_error("Failed to read from pipe");
        }
        buffer.resize(received_bytes);
        close(fd);
        return buffer;
    }
};

// HTTPIPC class
class HTTPIPC : public IPCStrategy {
private:
    std::string url;

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

public:
    HTTPIPC(const std::string& url) : url(url) {}

    void send(const std::vector<char>& data) override {
        CURL* curl;
        CURLcode res;

        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.data());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.size());

            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                throw std::runtime_error("Failed to send data via HTTP");
            }

            curl_easy_cleanup(curl);
        }
        curl_global_cleanup();
    }

    std::vector<char> receive() override {
        CURL* curl;
        CURLcode res;
        std::string readBuffer;

        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                throw std::runtime_error("Failed to receive data via HTTP");
            }

            curl_easy_cleanup(curl);
        }
        curl_global_cleanup();

        return std::vector<char>(readBuffer.begin(), readBuffer.end());
    }
};

// Process class
class Process {
private:
    std::shared_ptr<IPCStrategy> ipc;

public:
    // Constructor that takes an IPCStrategy object
    Process(std::shared_ptr<IPCStrategy> ipc) : ipc(ipc) {}

    // Method to send dataobj
    void send(const dataobj& data) {
        std::vector<char> buffer;
        SerializeBuffer serializer(buffer);
        data.serialize(serializer);
        ipc->send(buffer);
    }

    // Method to receive dataobj
    dataobj receive() {
        std::vector<char> buffer = ipc->receive();
        DeSerializeBuffer deserializer(buffer.data(), buffer.size());
        dataobj data;
        data.deserialize(deserializer);
        return data;
    }
};

// Server code
void handleClient(int client_sock) {
    try {
        // Create a SocketIPC object for the client connection
        std::shared_ptr<IPCStrategy> client_ipc = std::make_shared<SocketIPC>("127.0.0.1", 8080);
        Process process(client_ipc);

        // Receive data from client
        dataobj received_data = process.receive();

        // Process data (for demonstration, we'll just print the size of the first array)
        if (!received_data.arrays.empty()) {
            std::cout << "Received array of size: " << received_data.arrays[0].getSize() << std::endl;
        }

        // Send response to client (echo back the received data)
        process.send(received_data);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    close(client_sock);
}

int main() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Failed to bind socket" << std::endl;
        return 1;
    }

    if (listen(server_sock, 5) < 0) {
        std::cerr << "Failed to listen on socket" << std::endl;
        return 1;
    }

    std::cout << "Server is listening on port 8080" << std::endl;

    while (true) {
        int client_sock = accept(server_sock, nullptr, nullptr);
        if (client_sock < 0) {
            std::cerr << "Failed to accept connection" << std::endl;
            continue;
        }

        std::thread(handleClient, client_sock).detach();
    }

    close(server_sock);
    return 0;
}

#include <iostream>
#include <vector>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <memory>

// Include the necessary classes (MemoryAllocator, CPUMemoryAllocator, buffer, array, dataobj, SerializeBuffer, DeSerializeBuffer, IPCStrategy, SocketIPC, Process)

int main() {
    // Use CPU memory allocator
    std::shared_ptr<MemoryAllocator> cpuAllocator = std::make_shared<CPUMemoryAllocator>();
    array cpuArray1(1024, cpuAllocator);
    array cpuArray2(2048, cpuAllocator);

    // Create dataobj
    dataobj data({cpuArray1, cpuArray2});

    // Create a SocketIPC object for the client connection
    std::shared_ptr<IPCStrategy> client_ipc = std::make_shared<SocketIPC>("127.0.0.1", 8080);
    Process process(client_ipc);

    // Send data to server
    process.send(data);

    // Receive response from server
    dataobj received_data = process.receive();

    // Process response data (for demonstration, we'll just print the size of the first array)
    if (!received_data.arrays.empty()) {
        std::cout << "Received array of size: " << received_data.arrays[0].getSize() << std::endl;
    }

    return 0;
}