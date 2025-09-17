#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>

/* TO-DO: Make these configurable environment variables */
#define MAX_PEERS 1024
#define BUFFER_SIZE 1024
#define PORT 59888
#define N -1
#define BOOTSTRAP_IP "172.20.0.5:59888"

typedef struct {
    char *errid;
    int errnum;
    char *errmsg;
} p2perr;

typedef struct {
    char* ip;
    unsigned int port;
    unsigned int node_id;
} p2pconfig;

static p2perr errmap[] = {
    {"envNoDocker", -3791, "no docker environment"},
    {NULL, 0, NULL},
    {"conNoPeers", 28, "no peers in the network"},
    {"conNoNet", 29, "no network connection"},
    {"timeout", 30, "connection timeout"}
};

typedef struct {
    char ip[16];
    int port;
    int node_id;
    int socket_fd;
    int active;
} peer_info_t;

typedef struct {
    char ip[16];
    int port;
    int node_id;
    int listen_port;
    peer_info_t peers[MAX_PEERS];
    int peer_count;
    pthread_mutex_t peer_mutex;
    volatile bool running;
    int server_socket;
} node_t;

typedef struct {
    char type[16];      // "HELLO", "MESSAGE", "PEER_LIST", "GOODBYE"
    char sender_ip[16];
    int sender_port;
    char data[512];
    int node_id;
    char node_ip[16];
    int peer_count;
    time_t timestamp;
} message_t;

typedef enum {
    MSG_REGISTER = 1,
    MSG_PEER_LIST = 2,
    MSG_PEER_UPDATE = 3,
    MSG_HEARTBEAT = 4
} message_type_t;

typedef struct {
    peer_info_t peers[MAX_PEERS];
    int peer_count;
    time_t last_seen[MAX_PEERS];
    int gossip_round;
} network_view_t;

// global node
node_t g_node;

// array of addresses for binary version
char* boostrap_ip = "127.0.0.1:59879";
char* addrs = "127.0.0.1:59888,127.0.0.1:59889,127.0.0.1:59890,127.0.0.1:59891";

// Function prototypes
void* server_thread(void* arg);
void* peer_listener(void* arg);
void parse_peer_addrs(const char* peer_env);
void* peer_conn(void *arg);
void send_message_to_peer(peer_info_t* peer, message_t* msg);
void broadcast_message(message_t* msg);
void add_peer(const char* ip, int port);
void remove_peer(const char* ip, int port);
void send_hello_to_peer(const char* ip, int port);
void init_node(int argc, char *args[]);
void bootstrap_register(int argc);
void print_peers();
void cleanup();
void signal_handler(int sig);
p2perr* errlook(const char* name);

int main(int argc, char* argv[]) {
    printf("Starting P2P Node...\n");
    init_node(argc - 1, argv);
    
    // Set up signal handler for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Get my IP address (simplified - using localhost for Docker)
    // strcpy(g_node.ip, "127.0.0.1");
    if(argc - 1 == 0) {
        parse_peer_addrs(getenv("PEER_ADDRESSES"));
    } else if(argc - 1 == 2) {
        parse_peer_addrs(addrs);
    }
    printf("Node %d init'd, listening on port %d\n",
        g_node.node_id, g_node.port);
    
    // Start server thread
    pthread_t server_tid;
    if (pthread_create(&server_tid, NULL, server_thread, NULL) != 0) {
        perror("Failed to create server thread");
        exit(1);
    }
    
    // Main interactive loop
    char input[256];
    printf("\nP2P Node Ready! Commands:\n");
    printf("  connect <ip> <port> - Connect to a peer\n");
    printf("  send <message>      - Broadcast message to all peers\n");
    printf("  peers              - Show connected peers\n");
    printf("  quit               - Exit\n\n");
    
    while (g_node.running) {
        printf("> ");
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        // Remove newline
        input[strcspn(input, "\n")] = 0;
        
        if (strncmp(input, "connect ", 8) == 0) {
            char ip[16];
            int port;
            if (sscanf(input + 8, "%s %d", ip, &port) == 2) {
                send_hello_to_peer(ip, port);
                printf("exited sent_hello_to_peer()\n");
            } else {
                printf("Usage: connect <ip> <port>\n");
            }
        }
        else if (strncmp(input, "send ", 5) == 0) {
            message_t msg;
            strcpy(msg.type, "MESSAGE");
            strcpy(msg.sender_ip, g_node.ip);
            msg.sender_port = g_node.port;
            strcpy(msg.data, input + 5);
            msg.timestamp = time(NULL);
            broadcast_message(&msg);
        }
        else if (strcmp(input, "peers") == 0) {
            print_peers();
        }
        else if (strcmp(input, "quit") == 0) {
            g_node.running = false;
        }
        else if (strlen(input) > 0) {
            printf("Unknown command: %s\n", input);
        }
    }
    
    cleanup();
    pthread_join(server_tid, NULL);
    printf("threades joined and leaving main\n");
    return 0;
}

void* server_thread(void* arg) {
    struct sockaddr_in server_addr;
    
    // Create socket
    g_node.server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_node.server_socket < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(g_node.server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(g_node.port);

    if(inet_pton(AF_INET, g_node.ip, &server_addr.sin_addr) <= 0) {
        printf("Invalid IP address: %s\n", g_node.ip);
        close(g_node.server_socket);
        return NULL;
    }
    
    // Bind socket
    if (bind(g_node.server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Server address: %d\n", server_addr.sin_addr.s_addr);
        perror("Bind failed");
        exit(1);
    }
    
    // Listen for connections
    if (listen(g_node.server_socket, 5) < 0) {
        perror("Listen failed");
        exit(1);
    }
    
    printf("Server listening on port %d\n", g_node.port);
    
    while (g_node.running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(g_node.server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            if (g_node.running) {
                perror("Accept failed");
            }
            continue;
        }
        
        // Handle client in a separate thread
        pthread_t client_thread;
        int* socket_ptr = malloc(sizeof(int));
        *socket_ptr = client_socket;
        
        if (pthread_create(&client_thread, NULL, peer_listener, socket_ptr) != 0) {
            perror("Failed to create client thread");
            close(client_socket);
            free(socket_ptr);
        } else {
            pthread_detach(client_thread);
        }
    }
    printf("Server shutting down gracefully\n");
    return NULL;
}

void* peer_listener(void* arg) {
    int client_socket = *(int*)arg;
    free(arg);

    printf("=================================Peer listener=================================\n");
    
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    
    while ((bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        
        message_t msg;
        if (sscanf(buffer, "%s %s %d %ld %[^\n]", 
                   msg.type, msg.sender_ip, &msg.sender_port, &msg.timestamp, msg.data) >= 4) {
            
            printf("\n[%s from %s:%d] %s\n> ", msg.type, msg.sender_ip, msg.sender_port, msg.data);
            fflush(stdout);
            
            if (strcmp(msg.type, "HELLO") == 0) {
                add_peer(msg.sender_ip, msg.sender_port);
                
                // Send back our peer list
                message_t response;
                strcpy(response.type, "PEER_LIST");
                strcpy(response.sender_ip, g_node.ip);
                response.sender_port = g_node.port;
                
                pthread_mutex_lock(&g_node.peer_mutex);
                snprintf(response.data, sizeof(response.data), "peers:%d", g_node.peer_count);
                for (int i = 0; i < g_node.peer_count; i++) {
                    if (g_node.peers[i].active) {
                        char peer_info[32];
                        snprintf(peer_info, sizeof(peer_info), " %s:%d", g_node.peers[i].ip, g_node.peers[i].port);
                        strcat(response.data, peer_info);
                    }
                }
                pthread_mutex_unlock(&g_node.peer_mutex);
                
                response.timestamp = time(NULL);
                
                char response_buffer[BUFFER_SIZE];
                snprintf(response_buffer, sizeof(response_buffer), "%s %s %d %ld %s\n",
                        response.type, response.sender_ip, response.sender_port, 
                        response.timestamp, response.data);
                send(client_socket, response_buffer, strlen(response_buffer), 0);
            }
            else if (strcmp(msg.type, "PEER_LIST") == 0) {
                // Parse and add new peers
                char* peer_data = strchr(msg.data, ':');
                if (peer_data) {
                    peer_data++;
                    char* token = strtok(peer_data, " ");
                    while (token != NULL) {
                        char peer_ip[16];
                        int peer_port;
                        if (sscanf(token, "%[^:]:%d", peer_ip, &peer_port) == 2) {
                            if (strcmp(peer_ip, g_node.ip) != 0 || peer_port != g_node.port) {
                                send_hello_to_peer(peer_ip, peer_port);
                            }
                        }
                        token = strtok(NULL, " ");
                    }
                }
            }
            else if (strcmp(msg.type, "MESSAGE") == 0) {
                // Just display the message (already printed above)
            }
            else if (strcmp(msg.type, "GOODBYE") == 0) {
                remove_peer(msg.sender_ip, msg.sender_port);
                break;
            }
        }
    }
    
    close(client_socket);
    return NULL;
}

void parse_peer_addrs(const char* peer_env) {
    if(!peer_env) return;

    char* env_cpy = strdup(peer_env);
    char* token = strtok(env_cpy, ",");

    g_node.peer_count = 0;
    while(token != NULL && g_node.peer_count < MAX_PEERS) {
        char* colon = strchr(token, ':');
        if(colon != NULL) {
            *colon = '\0';
            strncpy(g_node.peers[g_node.peer_count].ip, token, 15);
            g_node.peers[g_node.peer_count].port = atoi(colon + 1);
            g_node.peer_count++;
        }
        token = strtok(NULL, ",");
    }
    free(env_cpy);
}

void* peer_conn(void *arg) {
    sleep(2);
    free(arg);

    for(int i = 0; i < g_node.peer_count; i++) {
        pthread_mutex_lock(&g_node.peer_mutex);
        peer_info_t peer = g_node.peers[i];
        pthread_mutex_unlock(&g_node.peer_mutex);

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if(sock < 0) {
            perror("Failed to create client socket");
            continue;
        }

        struct sockaddr_in peer_addr;
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_port = htons(peer.port);
        
        // Convert IP string to binary format
        if (inet_pton(AF_INET, peer.ip, &peer_addr.sin_addr) <= 0) {
            printf("Invalid peer address: %s\n", peer.ip);
            close(sock);
            continue;
        }
        
        printf("Node %d attempting to connect to %s:%d\n", 
               g_node.node_id, peer.ip, peer.port);
        
        if (connect(sock, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) {
            printf("Failed to connect to peer %s:%d: %s\n", 
                   peer.ip, peer.port, strerror(errno));
            close(sock);
            continue;
        }
        
        printf("Node %d connected to peer %s:%d\n", 
               g_node.node_id, peer.ip, peer.port);

    }
    return NULL;
}

void send_hello_to_peer(const char* ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    } else {
        printf("Opened socket for ip %s, port %d successfully\n", ip, port);
    }
    
    struct sockaddr_in peer_addr;
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, g_node.ip, &peer_addr.sin_addr) <= 0) {
        perror("Invalid address\n");
        close(sock);
        return;
    } else {
        printf("Valid address\n");
    }
    
    if (connect(sock, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) {
        printf("Failed to connect to %s:%d\n", ip, port);
        close(sock);
        return;
    } else {
        printf("Connected to peer\n");
    }
    
    message_t hello_msg;
    strcpy(hello_msg.type, "HELLO");
    printf("Sent HELLO message\n");
    strcpy(hello_msg.sender_ip, g_node.ip);
    hello_msg.sender_port = g_node.port;
    strcpy(hello_msg.data, "Hello from new peer");
    hello_msg.timestamp = time(NULL);
    
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "%s %s %d %ld %s\n",
            hello_msg.type, hello_msg.sender_ip, hello_msg.sender_port,
            hello_msg.timestamp, hello_msg.data);
    
    send(sock, buffer, strlen(buffer), 0);
    
    // Keep connection open for this peer
    add_peer(ip, port);
    
    pthread_t listener_thread;
    int* socket_ptr = malloc(sizeof(int));
    *socket_ptr = sock;
    
    if (pthread_create(&listener_thread, NULL, peer_listener, socket_ptr) != 0) {
        perror("Failed to create listener thread");
        close(sock);
        free(socket_ptr);
    } else {
        pthread_detach(listener_thread);
    }
}

void add_peer(const char* ip, int port) {
    pthread_mutex_lock(&g_node.peer_mutex);
    
    // Check if peer already exists
    for (int i = 0; i < g_node.peer_count; i++) {
        if (strcmp(g_node.peers[i].ip, ip) == 0 && g_node.peers[i].port == port) {
            g_node.peers[i].active = 1;
            pthread_mutex_unlock(&g_node.peer_mutex);
            return;
        }
    }
    
    // Add new peer
    if (g_node.peer_count < MAX_PEERS) {
        strcpy(g_node.peers[g_node.peer_count].ip, ip);
        g_node.peers[g_node.peer_count].port = port;
        g_node.peers[g_node.peer_count].active = 1;
        g_node.peer_count++;
        printf("Added peer: %s:%d\n", ip, port);
    }
    
    pthread_mutex_unlock(&g_node.peer_mutex);
}

void remove_peer(const char* ip, int port) {
    pthread_mutex_lock(&g_node.peer_mutex);
    
    for (int i = 0; i < g_node.peer_count; i++) {
        if (strcmp(g_node.peers[i].ip, ip) == 0 && g_node.peers[i].port == port) {
            g_node.peers[i].active = 0;
            printf("Removed peer: %s:%d\n", ip, port);
            break;
        }
    }
    
    pthread_mutex_unlock(&g_node.peer_mutex);
}

void broadcast_message(message_t* msg) {
    pthread_mutex_lock(&g_node.peer_mutex);
    printf("Broadcasting message ");
    printf("type: %s\n", msg->type);
    printf("data: %s\n", msg->data);
    
    for (int i = 0; i < g_node.peer_count; i++) {
        printf("peer counting during broadcast message\n");
        if (g_node.peers[i].active) {
            printf("about to send message to peer\n");
            send_message_to_peer(&g_node.peers[i], msg);
        }
    }
    printf("leaving broadcast message\n");
    pthread_mutex_unlock(&g_node.peer_mutex);
}

void send_message_to_peer(peer_info_t* peer, message_t* msg) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;
    
    struct sockaddr_in peer_addr;
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(peer->port);
    inet_pton(AF_INET, peer->ip, &peer_addr.sin_addr);
    
    if (connect(sock, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) == 0) {
        char buffer[BUFFER_SIZE];
        snprintf(buffer, sizeof(buffer), "%s %s %d %ld %s\n",
                msg->type, msg->sender_ip, msg->sender_port,
                msg->timestamp, msg->data);
        send(sock, buffer, strlen(buffer), 0);
    }
    
    close(sock);
}

void init_node(int argc, char* args[]) {
    printf("Starting up initialization...\n");

    if(argc == 3) {
        strncpy(g_node.ip, args[1], strlen(args[1]));
        g_node.port = atoi(args[2]);
        g_node.node_id = atoi(args[3]);
    } else if(argc == 0) {
        char* node_id = getenv("NODE_ID");
        printf("node id is %c \n", node_id);
        if(node_id == NULL) {
            p2perr* enverr = errlook("envNoDocker");
            printf("Error #%d: %s; %s\n", enverr->errnum, enverr->errid, enverr->errmsg);
            exit(1);
        }
        // g_node.node_id = atoi(getenv("NODE_ID") ?: 0);
        g_node.node_id = atoi(node_id ?: 0);
        g_node.port = atoi(getenv("LISTEN_PORT") ?: 0);
        char* ip_env = getenv("NODE_IP");
        printf("Initializing docker mode...");
        strncpy(g_node.ip, ip_env, strlen(ip_env));
    } else {
        printf("Wrong number of arguments\n");
        exit(1);
    }

    printf("Node on address %s and port %d\n", g_node.ip, g_node.port);

    g_node.running = true;

    // if not boostrap node, register with bootstrap node
    if(g_node.node_id != 0) {
        bootstrap_register(argc);
    }

    printf("Creating pthread mutex...\n");
    pthread_mutex_init(&g_node.peer_mutex, NULL);

    // printf("Getting peer addresses....\n");
    // parse_peer_addrs(getenv("PEER_ADDRESSES"));

    printf("Known peers: %d\n", g_node.peer_count);
    for(int i = 0; i < g_node.peer_count; i++) {
        printf("   Peer: %s:%d\n", g_node.peers[i].ip, g_node.peers[i].port);
    }
}

void bootstrap_register(int argc) {
    char* bootstrap_env;
    if(argc == 1){
        bootstrap_env = getenv("BOOTSTRAP_ADDRESS");
    } else if(argc == 3) {
        bootstrap_env = boostrap_ip;
    }

    parse_peer_addrs(bootstrap_env);
}

void print_peers() {
    pthread_mutex_lock(&g_node.peer_mutex);
    
    printf("Connected peers (%d):\n", g_node.peer_count);
    for (int i = 0; i < g_node.peer_count; i++) {
        if (g_node.peers[i].active) {
            printf("  %s:%d\n", g_node.peers[i].ip, g_node.peers[i].port);
        }
    }
    
    pthread_mutex_unlock(&g_node.peer_mutex);
}

void cleanup() {
    printf("\nShutting down...\n");
    g_node.running = false;
    
    // Send goodbye to all peers
    message_t goodbye_msg;
    strcpy(goodbye_msg.type, "GOODBYE");
    strcpy(goodbye_msg.sender_ip, g_node.ip);
    goodbye_msg.sender_port = g_node.port;
    strcpy(goodbye_msg.data, "Node shutting down");
    goodbye_msg.timestamp = time(NULL);
    
    broadcast_message(&goodbye_msg);
    
    if (g_node.server_socket >= 0) {
        printf("closing socket during cleanup\n");
        close(g_node.server_socket);
    }
    g_node.running = false;
    printf("closed socket during cleanup %d\n", g_node.running);
}

void signal_handler(int sig) {
    g_node.running = false;
}

p2perr* errlook(const char* name) {
    for(int i = 0; errmap[i].errid != NULL; i++) {
        if(strcmp(errmap[i].errid, name) == 0) {
            return &errmap[i];
        }
    }
    return NULL;
}

static int load_config(void *user, const char* section, const int name, const int value) {

    p2pconfig* pconfig = (p2pconfig*)user 

    #define match(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0 
    
    if(match())
}