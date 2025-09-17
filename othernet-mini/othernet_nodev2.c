#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>

#define MAX_PEERS 50
#define MAX_HELD_MESSAGES 1000
#define BUFFER_SIZE 2048
#define PORT 8080
#define MAX_RETRIES 5

// Othernet addressing structure
typedef struct {
    uint16_t realm;
    uint16_t cluster;
    uint32_t node_id;
} othernet_address_t;

// Message priorities (like FidoNet)
typedef enum {
    PRIORITY_CRASH = 0,   // Immediate delivery
    PRIORITY_DIRECT = 1,  // High priority
    PRIORITY_NORMAL = 2,  // Standard delivery
    PRIORITY_HOLD = 3     // Low priority, wait for destination
} message_priority_t;

// Message status for holding system
typedef enum {
    MSG_STATUS_QUEUED,
    MSG_STATUS_ATTEMPTING,
    MSG_STATUS_HELD,
    MSG_STATUS_DELIVERED,
    MSG_STATUS_EXPIRED,
    MSG_STATUS_FAILED
} message_status_t;

// Held message structure
typedef struct {
    uint64_t message_id;
    othernet_address_t target_address;
    othernet_address_t sender_address;
    message_priority_t priority;
    char payload[1024];
    
    time_t created_time;
    time_t last_attempt;
    time_t next_attempt;
    uint16_t attempt_count;
    time_t expires_at;
    
    message_status_t status;
    char holding_node_ip[16];
} held_message_t;

// Node capability flags
typedef enum {
    CAPABILITY_HOLDING = 0x01,
    CAPABILITY_ROUTING = 0x02,
    CAPABILITY_GATEWAY = 0x04
} node_capability_t;

// Peer information with capabilities
typedef struct {
    char ip[16];
    int port;
    othernet_address_t address;
    uint32_t capabilities;
    float load_factor;
    time_t last_seen;
    int active;
} peer_t;

// Discovery scope (like AppleTalk zones)
typedef struct {
    uint16_t realm;      // 0 = all realms
    uint16_t cluster;    // 0 = all clusters
    uint8_t max_hops;
} discovery_scope_t;

// Protocol message types
typedef enum {
    MSG_TYPE_HELLO,
    MSG_TYPE_PEER_LIST,
    MSG_TYPE_OTHERNET_MESSAGE,
    MSG_TYPE_HOLD_REQUEST,
    MSG_TYPE_HOLD_RESPONSE,
    MSG_TYPE_DELIVERY_ATTEMPT,
    MSG_TYPE_DELIVERY_CONFIRM,
    MSG_TYPE_CAPABILITY_UPDATE,
    MSG_TYPE_GOODBYE
} protocol_message_type_t;

// Protocol message structure
typedef struct {
    protocol_message_type_t type;
    othernet_address_t sender;
    char sender_ip[16];
    int sender_port;
    discovery_scope_t scope;
    uint8_t ttl;
    time_t timestamp;
    char data[1024];
} protocol_message_t;

// Global state
peer_t peers[MAX_PEERS];
held_message_t held_messages[MAX_HELD_MESSAGES];
int peer_count = 0;
int held_message_count = 0;
int server_socket = -1;
int running = 1;
othernet_address_t my_address;
char my_ip[16];
int my_port = PORT;
uint32_t my_capabilities = CAPABILITY_HOLDING | CAPABILITY_ROUTING;
pthread_mutex_t peers_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t messages_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function prototypes
void* server_thread(void* arg);
void* maintenance_thread(void* arg);
void* peer_listener(void* arg);
void handle_protocol_message(protocol_message_t* msg, const char* from_ip);
void send_protocol_message(const char* ip, int port, protocol_message_t* msg);
void broadcast_protocol_message(protocol_message_t* msg);

// Peer management
void add_peer(const char* ip, int port, othernet_address_t* addr, uint32_t capabilities);
void remove_peer(const char* ip, int port);
peer_t* find_peer_by_address(othernet_address_t* addr);
peer_t* find_best_holding_node(othernet_address_t* target);

// Message holding system
void queue_message_for_holding(othernet_address_t* target, const char* payload, message_priority_t priority);
void attempt_message_delivery(held_message_t* msg);
void cleanup_expired_messages();
void redistribute_held_messages(const char* failed_node_ip);

// Discovery and capabilities
void announce_presence();
void send_capability_update();
void handle_hello_message(protocol_message_t* msg, const char* from_ip);
void handle_peer_list_message(protocol_message_t* msg);

// Utility functions
uint64_t generate_message_id();
time_t calculate_next_retry(held_message_t* msg);
void print_othernet_address(othernet_address_t* addr);
void print_peers();
void print_held_messages();
void cleanup();
void signal_handler(int sig);

int main(int argc, char* argv[]) {
    printf("Starting Othernet Node...\n");
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize my address (could be configurable)
    my_address.realm = 1;
    my_address.cluster = 1;
    my_address.node_id = (uint32_t)time(NULL) % 10000; // Simple ID generation
    
    strcpy(my_ip, "0.0.0.0");
    
    printf("My Othernet address: %d.%d.%d\n", 
           my_address.realm, my_address.cluster, my_address.node_id);
    
    // Connect to bootstrap if provided
    if (argc == 3) {
        printf("Bootstrapping from %s:%s\n", argv[1], argv[2]);
        
        protocol_message_t hello;
        hello.type = MSG_TYPE_HELLO;
        hello.sender = my_address;
        strcpy(hello.sender_ip, my_ip);
        hello.sender_port = my_port;
        hello.scope.realm = 0;  // Announce to all realms initially
        hello.scope.cluster = 0;
        hello.scope.max_hops = 8;
        hello.ttl = 8;
        hello.timestamp = time(NULL);
        snprintf(hello.data, sizeof(hello.data), "capabilities:%d", my_capabilities);
        
        send_protocol_message(argv[1], atoi(argv[2]), &hello);
    }
    
    // Start server thread
    pthread_t server_tid;
    pthread_create(&server_tid, NULL, server_thread, NULL);
    
    // Start maintenance thread
    pthread_t maintenance_tid;
    pthread_create(&maintenance_tid, NULL, maintenance_thread, NULL);
    
    // Main interactive loop
    char input[256];
    printf("\nOthernet Node Ready! Commands:\n");
    printf("  connect <ip> <port>         - Connect to a peer\n");
    printf("  send <realm.cluster.node> <msg> - Send message to othernet address\n");
    printf("  broadcast <message>         - Broadcast to all peers\n");
    printf("  peers                       - Show connected peers\n");
    printf("  held                        - Show held messages\n");
    printf("  capabilities                - Show my capabilities\n");
    printf("  quit                        - Exit\n\n");
    
    while (running) {
        printf("> ");
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        input[strcspn(input, "\n")] = 0;
        
        if (strncmp(input, "connect ", 8) == 0) {
            char ip[16];
            int port;
            if (sscanf(input + 8, "%s %d", ip, &port) == 2) {
                protocol_message_t hello;
                hello.type = MSG_TYPE_HELLO;
                hello.sender = my_address;
                strcpy(hello.sender_ip, my_ip);
                hello.sender_port = my_port;
                hello.timestamp = time(NULL);
                snprintf(hello.data, sizeof(hello.data), "capabilities:%d", my_capabilities);
                send_protocol_message(ip, port, &hello);
            }
        }
        else if (strncmp(input, "send ", 5) == 0) {
            char addr_str[32], message[512];
            if (sscanf(input + 5, "%s %[^\n]", addr_str, message) == 2) {
                othernet_address_t target;
                if (sscanf(addr_str, "%hu.%hu.%u", &target.realm, &target.cluster, &target.node_id) == 3) {
                    queue_message_for_holding(&target, message, PRIORITY_NORMAL);
                    printf("Message queued for delivery to ");
                    print_othernet_address(&target);
                    printf("\n");
                }
            }
        }
        else if (strncmp(input, "broadcast ", 10) == 0) {
            protocol_message_t msg;
            msg.type = MSG_TYPE_OTHERNET_MESSAGE;
            msg.sender = my_address;
            strcpy(msg.sender_ip, my_ip);
            msg.sender_port = my_port;
            msg.timestamp = time(NULL);
            strcpy(msg.data, input + 10);
            broadcast_protocol_message(&msg);
        }
        else if (strcmp(input, "peers") == 0) {
            print_peers();
        }
        else if (strcmp(input, "held") == 0) {
            print_held_messages();
        }
        else if (strcmp(input, "capabilities") == 0) {
            printf("My capabilities: ");
            if (my_capabilities & CAPABILITY_HOLDING) printf("HOLDING ");
            if (my_capabilities & CAPABILITY_ROUTING) printf("ROUTING ");
            if (my_capabilities & CAPABILITY_GATEWAY) printf("GATEWAY ");
            printf("\n");
        }
        else if (strcmp(input, "quit") == 0) {
            running = 0;
        }
        else if (strlen(input) > 0) {
            printf("Unknown command: %s\n", input);
        }
    }
    
    cleanup();
    pthread_join(server_tid, NULL);
    pthread_join(maintenance_tid, NULL);
    return 0;
}

void* server_thread(void* arg) {
    struct sockaddr_in server_addr;
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(my_port);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }
    
    if (listen(server_socket, 10) < 0) {
        perror("Listen failed");
        exit(1);
    }
    
    printf("Othernet server listening on port %d\n", my_port);
    
    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            if (running) perror("Accept failed");
            continue;
        }
        
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
    
    return NULL;
}

void* maintenance_thread(void* arg) {
    while (running) {
        sleep(30); // Run maintenance every 30 seconds
        
        // Clean up expired messages
        cleanup_expired_messages();
        
        // Attempt delivery of held messages
        pthread_mutex_lock(&messages_mutex);
        time_t now = time(NULL);
        
        for (int i = 0; i < held_message_count; i++) {
            held_message_t* msg = &held_messages[i];
            if (msg->status == MSG_STATUS_HELD && now >= msg->next_attempt) {
                attempt_message_delivery(msg);
            }
        }
        pthread_mutex_unlock(&messages_mutex);
        
        // Send periodic capability updates
        if (peer_count > 0) {
            send_capability_update();
        }
    }
    
    return NULL;
}

void* peer_listener(void* arg) {
    int client_socket = *(int*)arg;
    free(arg);
    
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    getpeername(client_socket, (struct sockaddr*)&client_addr, &addr_len);
    char client_ip[16];
    strcpy(client_ip, inet_ntoa(client_addr.sin_addr));
    
    ssize_t bytes_received;
    while ((bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        
        // Parse protocol message
        protocol_message_t msg;
        char type_str[16], scope_str[32];
        
        int parsed = sscanf(buffer, "%s %hu.%hu.%u %s %d %s %ld %[^\n]",
                           type_str, &msg.sender.realm, &msg.sender.cluster, &msg.sender.node_id,
                           msg.sender_ip, &msg.sender_port, scope_str, &msg.timestamp, msg.data);
        
        if (parsed >= 7) {
            // Convert type string to enum
            if (strcmp(type_str, "HELLO") == 0) msg.type = MSG_TYPE_HELLO;
            else if (strcmp(type_str, "PEER_LIST") == 0) msg.type = MSG_TYPE_PEER_LIST;
            else if (strcmp(type_str, "OTHERNET_MESSAGE") == 0) msg.type = MSG_TYPE_OTHERNET_MESSAGE;
            else if (strcmp(type_str, "GOODBYE") == 0) msg.type = MSG_TYPE_GOODBYE;
            else continue; // Unknown message type
            
            handle_protocol_message(&msg, client_ip);
        }
    }
    
    close(client_socket);
    return NULL;
}

void send_protocol_message(const char* ip, int port, protocol_message_t* msg) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;
    
    struct sockaddr_in peer_addr;
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &peer_addr.sin_addr);
    
    if (connect(sock, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) == 0) {
        char buffer[BUFFER_SIZE];
        const char* type_str = "UNKNOWN";
        
        switch (msg->type) {
            case MSG_TYPE_HELLO: type_str = "HELLO"; break;
            case MSG_TYPE_PEER_LIST: type_str = "PEER_LIST"; break;
            case MSG_TYPE_OTHERNET_MESSAGE: type_str = "OTHERNET_MESSAGE"; break;
            case MSG_TYPE_GOODBYE: type_str = "GOODBYE"; break;
            default: break;
        }
        
        snprintf(buffer, sizeof(buffer), "%s %hu.%hu.%u %s %d scope:%hu.%hu.%hhu %ld %s\n",
                type_str, msg->sender.realm, msg->sender.cluster, msg->sender.node_id,
                msg->sender_ip, msg->sender_port,
                msg->scope.realm, msg->scope.cluster, msg->scope.max_hops,
                msg->timestamp, msg->data);
        
        send(sock, buffer, strlen(buffer), 0);
    }
    
    close(sock);
}

void handle_protocol_message(protocol_message_t* msg, const char* from_ip) {
    switch (msg->type) {
        case MSG_TYPE_HELLO:
            handle_hello_message(msg, from_ip);
            break;
            
        case MSG_TYPE_OTHERNET_MESSAGE:
            printf("\n[MESSAGE from ");
            print_othernet_address(&msg->sender);
            printf("] %s\n> ", msg->data);
            fflush(stdout);
            break;
            
        case MSG_TYPE_GOODBYE:
            remove_peer(msg->sender_ip, msg->sender_port);
            break;
            
        default:
            break;
    }
}

void handle_hello_message(protocol_message_t* msg, const char* from_ip) {
    uint32_t capabilities = 0;
    sscanf(msg->data, "capabilities:%u", &capabilities);
    
    add_peer(from_ip, msg->sender_port, &msg->sender, capabilities);
    
    // Send back our capabilities
    protocol_message_t response;
    response.type = MSG_TYPE_HELLO;
    response.sender = my_address;
    strcpy(response.sender_ip, my_ip);
    response.sender_port = my_port;
    response.timestamp = time(NULL);
    snprintf(response.data, sizeof(response.data), "capabilities:%u", my_capabilities);
    
    send_protocol_message(from_ip, msg->sender_port, &response);
}

void queue_message_for_holding(othernet_address_t* target, const char* payload, message_priority_t priority) {
    pthread_mutex_lock(&messages_mutex);
    
    if (held_message_count < MAX_HELD_MESSAGES) {
        held_message_t* msg = &held_messages[held_message_count++];
        
        msg->message_id = generate_message_id();
        msg->target_address = *target;
        msg->sender_address = my_address;
        msg->priority = priority;
        strcpy(msg->payload, payload);
        
        time_t now = time(NULL);
        msg->created_time = now;
        msg->last_attempt = 0;
        msg->next_attempt = now;
        msg->attempt_count = 0;
        msg->expires_at = now + 86400; // 24 hours
        msg->status = MSG_STATUS_QUEUED;
        
        // Try immediate delivery
        attempt_message_delivery(msg);
    }
    
    pthread_mutex_unlock(&messages_mutex);
}

void attempt_message_delivery(held_message_t* msg) {
    peer_t* target_peer = find_peer_by_address(&msg->target_address);
    
    if (target_peer && target_peer->active) {
        // Direct delivery attempt
        protocol_message_t delivery;
        delivery.type = MSG_TYPE_OTHERNET_MESSAGE;
        delivery.sender = msg->sender_address;
        delivery.timestamp = time(NULL);
        strcpy(delivery.data, msg->payload);
        
        send_protocol_message(target_peer->ip, target_peer->port, &delivery);
        
        msg->status = MSG_STATUS_DELIVERED;
        printf("Message %lu delivered to ", msg->message_id);
        print_othernet_address(&msg->target_address);
        printf("\n");
    } else {
        // Target not online, update retry schedule
        msg->status = MSG_STATUS_HELD;
        msg->attempt_count++;
        msg->last_attempt = time(NULL);
        msg->next_attempt = calculate_next_retry(msg);
        
        if (msg->attempt_count >= MAX_RETRIES) {
            msg->status = MSG_STATUS_FAILED;
            printf("Message %lu failed after %d attempts\n", msg->message_id, msg->attempt_count);
        }
    }
}

void add_peer(const char* ip, int port, othernet_address_t* addr, uint32_t capabilities) {
    pthread_mutex_lock(&peers_mutex);
    
    // Check if peer already exists
    for (int i = 0; i < peer_count; i++) {
        if (strcmp(peers[i].ip, ip) == 0 && peers[i].port == port) {
            peers[i].active = 1;
            peers[i].last_seen = time(NULL);
            peers[i].capabilities = capabilities;
            pthread_mutex_unlock(&peers_mutex);
            return;
        }
    }
    
    // Add new peer
    if (peer_count < MAX_PEERS) {
        peer_t* peer = &peers[peer_count++];
        strcpy(peer->ip, ip);
        peer->port = port;
        peer->address = *addr;
        peer->capabilities = capabilities;
        peer->load_factor = 0.0;
        peer->last_seen = time(NULL);
        peer->active = 1;
        
        printf("Added peer: ");
        print_othernet_address(addr);
        printf(" at %s:%d\n", ip, port);
    }
    
    pthread_mutex_unlock(&peers_mutex);
}

peer_t* find_peer_by_address(othernet_address_t* addr) {
    for (int i = 0; i < peer_count; i++) {
        if (peers[i].active &&
            peers[i].address.realm == addr->realm &&
            peers[i].address.cluster == addr->cluster &&
            peers[i].address.node_id == addr->node_id) {
            return &peers[i];
        }
    }
    return NULL;
}

uint64_t generate_message_id() {
    static uint64_t counter = 0;
    return ((uint64_t)time(NULL) << 32) | (++counter);
}

time_t calculate_next_retry(held_message_t* msg) {
    // Exponential backoff: 1min, 2min, 4min, 8min, 16min
    int delay = 60 * (1 << (msg->attempt_count - 1));
    if (delay > 960) delay = 960; // Cap at 16 minutes
    return msg->last_attempt + delay;
}

void print_othernet_address(othernet_address_t* addr) {
    printf("%u.%u.%u", addr->realm, addr->cluster, addr->node_id);
}

void print_peers() {
    pthread_mutex_lock(&peers_mutex);
    
    printf("Connected peers (%d):\n", peer_count);
    for (int i = 0; i < peer_count; i++) {
        if (peers[i].active) {
            printf("  ");
            print_othernet_address(&peers[i].address);
            printf(" at %s:%d (capabilities: ", peers[i].ip, peers[i].port);
            
            if (peers[i].capabilities & CAPABILITY_HOLDING) printf("H");
            if (peers[i].capabilities & CAPABILITY_ROUTING) printf("R");
            if (peers[i].capabilities & CAPABILITY_GATEWAY) printf("G");
            printf(")\n");
        }
    }
    
    pthread_mutex_unlock(&peers_mutex);
}

void print_held_messages() {
    pthread_mutex_lock(&messages_mutex);
    
    printf("Held messages (%d):\n", held_message_count);
    for (int i = 0; i < held_message_count; i++) {
        held_message_t* msg = &held_messages[i];
        if (msg->status != MSG_STATUS_DELIVERED) {
            printf("  ID:%lu Target:", msg->message_id);
            print_othernet_address(&msg->target_address);
            printf(" Status:");
            
            switch (msg->status) {
                case MSG_STATUS_QUEUED: printf("QUEUED"); break;
                case MSG_STATUS_ATTEMPTING: printf("ATTEMPTING"); break;
                case MSG_STATUS_HELD: printf("HELD"); break;
                case MSG_STATUS_EXPIRED: printf("EXPIRED"); break;
                case MSG_STATUS_FAILED: printf("FAILED"); break;
                default: printf("UNKNOWN"); break;
            }
            
            printf(" Attempts:%d Priority:%d\n", 
                   msg->attempt_count, msg->priority);
            printf("    Payload: %.50s%s\n", 
                   msg->payload, strlen(msg->payload) > 50 ? "..." : "");
        }
    }
    
    pthread_mutex_unlock(&messages_mutex);
}

void remove_peer(const char* ip, int port) {
    pthread_mutex_lock(&peers_mutex);
    
    for (int i = 0; i < peer_count; i++) {
        if (strcmp(peers[i].ip, ip) == 0 && peers[i].port == port) {
            peers[i].active = 0;
            printf("Peer disconnected: ");
            print_othernet_address(&peers[i].address);
            printf(" at %s:%d\n", ip, port);
            
            // Redistribute any messages held by this node
            redistribute_held_messages(ip);
            break;
        }
    }
    
    pthread_mutex_unlock(&peers_mutex);
}

void cleanup_expired_messages() {
    pthread_mutex_lock(&messages_mutex);
    
    time_t now = time(NULL);
    int cleaned = 0;
    
    for (int i = 0; i < held_message_count; i++) {
        held_message_t* msg = &held_messages[i];
        if (now > msg->expires_at && msg->status != MSG_STATUS_DELIVERED) {
            msg->status = MSG_STATUS_EXPIRED;
            cleaned++;
        }
    }
    
    if (cleaned > 0) {
        printf("Cleaned up %d expired messages\n", cleaned);
    }
    
    pthread_mutex_unlock(&messages_mutex);
}

void redistribute_held_messages(const char* failed_node_ip) {
    // In a real implementation, this would move messages from
    // the failed holding node to other available nodes
    printf("Redistributing messages from failed node: %s\n", failed_node_ip);
}

void broadcast_protocol_message(protocol_message_t* msg) {
    pthread_mutex_lock(&peers_mutex);
    
    for (int i = 0; i < peer_count; i++) {
        if (peers[i].active) {
            send_protocol_message(peers[i].ip, peers[i].port, msg);
        }
    }
    
    pthread_mutex_unlock(&peers_mutex);
}

void announce_presence() {
    protocol_message_t announcement;
    announcement.type = MSG_TYPE_HELLO;
    announcement.sender = my_address;
    strcpy(announcement.sender_ip, my_ip);
    announcement.sender_port = my_port;
    announcement.scope.realm = 0;  // Announce to all realms
    announcement.scope.cluster = 0;
    announcement.scope.max_hops = 8;
    announcement.ttl = 8;
    announcement.timestamp = time(NULL);
    snprintf(announcement.data, sizeof(announcement.data), 
             "capabilities:%u load:%.2f", my_capabilities, 
             (float)held_message_count / MAX_HELD_MESSAGES);
    
    broadcast_protocol_message(&announcement);
}

void send_capability_update() {
    protocol_message_t update;
    update.type = MSG_TYPE_CAPABILITY_UPDATE;
    update.sender = my_address;
    strcpy(update.sender_ip, my_ip);
    update.sender_port = my_port;
    update.timestamp = time(NULL);
    
    float load = (float)held_message_count / MAX_HELD_MESSAGES;
    snprintf(update.data, sizeof(update.data), 
             "capabilities:%u load:%.2f uptime:%ld", 
             my_capabilities, load, time(NULL));
    
    broadcast_protocol_message(&update);
}

peer_t* find_best_holding_node(othernet_address_t* target) {
    peer_t* best = NULL;
    float best_score = 1000.0;  // Lower is better
    
    pthread_mutex_lock(&peers_mutex);
    
    for (int i = 0; i < peer_count; i++) {
        if (peers[i].active && (peers[i].capabilities & CAPABILITY_HOLDING)) {
            // Calculate score based on load factor and network distance
            float score = peers[i].load_factor;
            
            // Prefer nodes in same realm/cluster (simplified distance calculation)
            if (peers[i].address.realm != target->realm) score += 0.5;
            if (peers[i].address.cluster != target->cluster) score += 0.2;
            
            if (score < best_score) {
                best_score = score;
                best = &peers[i];
            }
        }
    }
    
    pthread_mutex_unlock(&peers_mutex);
    return best;
}

void cleanup() {
    printf("\nShutting down Othernet node...\n");
    running = 0;
    
    // Send goodbye to all peers
    protocol_message_t goodbye;
    goodbye.type = MSG_TYPE_GOODBYE;
    goodbye.sender = my_address;
    strcpy(goodbye.sender_ip, my_ip);
    goodbye.sender_port = my_port;
    goodbye.timestamp = time(NULL);
    strcpy(goodbye.data, "Node shutting down gracefully");
    
    broadcast_protocol_message(&goodbye);
    
    if (server_socket >= 0) {
        close(server_socket);
    }
    
    // Clean up any remaining held messages
    pthread_mutex_lock(&messages_mutex);
    printf("Had %d held messages at shutdown\n", held_message_count);
    pthread_mutex_unlock(&messages_mutex);
}

void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    running = 0;