#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

#include "proxyserver.h"
#include "safequeue.h"


/*
 * Constants
 */
#define RESPONSE_BUFSIZE 10000

/*
 * Global configuration variables.
 * Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
int num_listener;
int *listener_ports;
int *server_fd_list;
int num_workers;
char *fileserver_ipaddr;
int fileserver_port;
int max_queue_size;
PriorityQueue *queue;


void send_error_response(int client_fd, status_code_t err_code, char *err_msg) {
    http_start_response(client_fd, err_code);
    http_send_header(client_fd, "Content-Type", "text/html");
    http_end_headers(client_fd);
    char *buf = malloc(strlen(err_msg) + 2);
    sprintf(buf, "%s\n", err_msg);
    http_send_string(client_fd, buf);
    return;
}

/*
 * forward the client request to the fileserver and
 * forward the fileserver response to the client
 */
void serve_request(Job * toServe) {
    int client_fd = toServe -> client_fd;
    
    // create a fileserver socket
    int fileserver_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fileserver_fd == -1) {
        fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
        exit(errno);
    }

    // create the full fileserver address
    struct sockaddr_in fileserver_address;
    fileserver_address.sin_addr.s_addr = inet_addr(fileserver_ipaddr);
    fileserver_address.sin_family = AF_INET;
    fileserver_address.sin_port = htons(fileserver_port);
    
    // connect to the fileserver
    int connection_status = connect(fileserver_fd, (struct sockaddr *)&fileserver_address,
                                    sizeof(fileserver_address));
    if (connection_status < 0) {
        // failed to connect to the fileserver
        printf("Failed to connect to the file server\n");
        send_error_response(client_fd, BAD_GATEWAY, "Bad Gateway\n");
        exit(errno);
        return;
    }

    // successfully connected to the file server
    char *buffer = (char *)malloc(RESPONSE_BUFSIZE * sizeof(char));
    strcpy(buffer, toServe -> read_buffer);    

    // forward the client request to the fileserver
    int bytes_read = toServe -> bytes_read;
    if (bytes_read == -1) {
        printf("read failed\n");
        exit(EXIT_FAILURE);
    }
    
    
    int ret = http_send_data(fileserver_fd, buffer, bytes_read);
    if (ret < 0) {
        printf("Failed to send request to the file server\n");
        send_error_response(client_fd, BAD_GATEWAY, "Bad Gateway");

    } else {
        // forward the fileserver response to the client
        while (1) {
            int bytes_read = recv(fileserver_fd, buffer, RESPONSE_BUFSIZE - 1, 0);
            if (bytes_read <= 0) // fileserver_fd has been closed, break
                break;
            ret = http_send_data(client_fd, buffer, bytes_read);
            if (ret < 0) { // write failed, client_fd has been closed
                break;
            }
        }
    }

    // close the connection to the fileserver
    shutdown(fileserver_fd, SHUT_WR);
    close(fileserver_fd);

    // Free resources and exit
    free(buffer);
}


/*
 * opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void *serve_forever(void *arg) { // this is where the listener threads are
    
    int index = (int)(intptr_t)arg;

    // create a socket to listen
    server_fd_list[index] = socket(PF_INET, SOCK_STREAM, 0);
    if (server_fd_list[index] == -1) {
        perror("Failed to create a new socket");
        exit(errno);
    }

    // manipulate options for the socket
    int socket_option = 1;
    if (setsockopt(server_fd_list[index], SOL_SOCKET, SO_REUSEADDR, &socket_option,
                   sizeof(socket_option)) == -1) {
        perror("Failed to set socket options");
        exit(errno);
    }

    int proxy_port = listener_ports[index];
    // create the full address of this proxyserver
    struct sockaddr_in proxy_address;
    memset(&proxy_address, 0, sizeof(proxy_address));
    proxy_address.sin_family = AF_INET;
    proxy_address.sin_addr.s_addr = INADDR_ANY;
    proxy_address.sin_port = htons(proxy_port); // listening port

    // bind the socket to the address and port number specified in
    if (bind(server_fd_list[index], (struct sockaddr *)&proxy_address,
             sizeof(proxy_address)) == -1) {
        perror("Failed to bind on socket");
        exit(errno);
    }

    // starts waiting for the client to request a connection
    if (listen(server_fd_list[index], 1024) == -1) {
        perror("Failed to listen on socket");
        exit(errno);
    }

    printf("Listening on port %d...\n", proxy_port);

    struct sockaddr_in client_address;
    size_t client_address_length = sizeof(client_address);
    int client_fd;
    while (1) {
        printf("top of while loop in serve_forever()\n");
        client_fd = accept(server_fd_list[index], 
                           (struct sockaddr *)&client_address,
                           (socklen_t *)&client_address_length);
        if (client_fd < 0) {
            perror("Error accepting socket");
            continue;
        }

        printf("Accepted connection from %s on port %d\n",
               inet_ntoa(client_address.sin_addr),
               client_address.sin_port);

        struct parsed_request *thisReq = parse_client_request(client_fd);
        Job *getJob;
        printf("top of id(thisReq == NULL) in serve_forever()\n");
        if (thisReq == NULL) { // this is null if getJob was found
            // remove the highest priority element from the queue
            getJob = get_work_nonblocking(queue);
            if (getJob == NULL) { // if queue empty
                send_error_response(client_fd, QUEUE_EMPTY, "Queue Empty");
                //shutdown(client_fd, SHUT_RDWR);
                close(client_fd);
                continue;
            }
            
            printf("about to send response from serve_forever()\n");
            // send back to client successfully
            http_start_response(client_fd, OK);
            http_send_header(client_fd, "Content-Type", "text/plain");
            http_end_headers(client_fd);
            char *buf = malloc(strlen(getJob -> path) + 2);
            sprintf(buf, "%s\n", getJob -> path);
            http_send_string(client_fd, buf);
            //http_send_string(client_fd, getJob->path);

            // Close the client connection
            // shutdown(client_fd, SHUT_RDWR);
            
            printf("about to close client_fd from serve_forever()\n");
            close(client_fd);
            printf("closed client_fd from serve_forever()\n");
        } else { // not GetJob
            printf("NOT GET JOB\n");

            Job *newJob = malloc(sizeof(Job));
            if (newJob == NULL) {
                printf("job malloc fail\n");
                exit(EXIT_FAILURE);
            }
            

            newJob -> priority = thisReq -> priority; 
            newJob -> client_fd = client_fd;
            newJob -> delay = thisReq -> delay;
            newJob -> bytes_read = thisReq -> bytes_read;
            newJob -> read_buffer = thisReq -> read_buffer; // strcpy?
            newJob -> path = thisReq -> path; // strcpy?

            printf("about to add_work\n");
            if(add_work(queue, newJob) == NULL) {
                send_error_response(client_fd, QUEUE_FULL, "Queue Full");
                printf("add_work(queue, newJob) == NULL");
                //shutdown(client_fd, SHUT_RDWR);
                close(client_fd);
            }
            printf("added work\n");
        }
    }

    shutdown(server_fd_list[index], SHUT_RDWR);
    close(server_fd_list[index]);
}

void* worker_thread(void *arg) {

    while(1) {
        // get next job from PQ
        Job nextJob = get_work(queue);
        if (nextJob.delay > 0) {
            sleep(nextJob.delay);
        }

        serve_request(&nextJob);

        shutdown(nextJob.client_fd, SHUT_RDWR);
        close(nextJob.client_fd);
    }

    return NULL;
}

/*
 * Default settings for in the global configuration variables
 */
void default_settings() {
    num_listener = 1;
    listener_ports = (int *)malloc(num_listener * sizeof(int));
    server_fd_list = (int *)malloc(num_listener * sizeof(int));
    listener_ports[0] = 8000;

    num_workers = 1;

    fileserver_ipaddr = "127.0.0.1";
    fileserver_port = 3333;

    max_queue_size = 100;
}

void print_settings() {
    printf("\t---- Setting ----\n");
    printf("\t%d listeners [", num_listener);
    for (int i = 0; i < num_listener; i++)
        printf(" %d", listener_ports[i]);
    printf(" ]\n");
    printf("\t%d workers\n", num_workers);
    printf("\tfileserver ipaddr %s port %d\n", fileserver_ipaddr, fileserver_port);
    printf("\tmax queue size  %d\n", max_queue_size);
    printf("\t  ----\t----\t\n");
}

void signal_callback_handler(int signum) {
    printf("Caught signal %d: %s\n", signum, strsignal(signum));
    for (int i = 0; i < num_listener; i++) {
        if (close(server_fd_list[i]) < 0) perror("Failed to close server_fd (ignoring)\n");
    }
    free(listener_ports);
    exit(0);
}

pthread_t *listeners;
pthread_t *workers;


void setupThreads() {

    queue = create_queue(max_queue_size);

    // create threads
    listeners = malloc(num_listener * sizeof(pthread_t)); // this keeps track of all threads so we can join them later
    for(int i=0; i<num_listener; i++) {

        pthread_t listener;
        if (pthread_create(&listener, NULL, serve_forever, (void*)(intptr_t)i) != 0) { 
            printf("error: pthread_create on %d thread\n", i);
            exit(EXIT_FAILURE);
        }

        listeners[i] = listener;
    }

    // create worker threads
    workers = malloc(num_workers * sizeof(pthread_t)); // this keeps track of all threads so we can join them later
    for(int i=0; i<num_workers; i++) {
        pthread_t worker;
        if (pthread_create(&worker, NULL, worker_thread, (void*)NULL) != 0) { 
            printf("error: pthread_create on %d thread\n", i);
            exit(EXIT_FAILURE);
        }
        workers[i] = worker;
    }
}

char *USAGE =
    "Usage: ./proxyserver [-l 1 8000] [-n 1] [-i 127.0.0.1 -p 3333] [-q 100]\n";

void exit_with_usage() {
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {

    signal(SIGINT, signal_callback_handler);

    /* Default settings */
    default_settings();

    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp("-l", argv[i]) == 0) {
            num_listener = atoi(argv[++i]);
            free(listener_ports);
            listener_ports = (int *)malloc(num_listener * sizeof(int));
            for (int j = 0; j < num_listener; j++) {
                listener_ports[j] = atoi(argv[++i]);
            }
        } else if (strcmp("-w", argv[i]) == 0) {
            num_workers = atoi(argv[++i]);
        } else if (strcmp("-q", argv[i]) == 0) {
            max_queue_size = atoi(argv[++i]);
        } else if (strcmp("-i", argv[i]) == 0) {
            fileserver_ipaddr = argv[++i];
        } else if (strcmp("-p", argv[i]) == 0) {
            fileserver_port = atoi(argv[++i]);
        } else {
            fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
            exit_with_usage();
        }
    }

    // setting up threads
    setupThreads();

    print_settings();
    
    for (int i = 0; i < num_listener; i++) {
        pthread_join(listeners[i], NULL); // wait for all threads to finish before exiting
    }

    free(listeners);

    for (int i = 0; i < num_workers; i++) {
        pthread_join(workers[i], NULL); // wait for all threads to finish before exiting
    }

    free(workers);

    return EXIT_SUCCESS;
}
