# Administrivia

- **Due date**: November 22nd, 2023 at 11:59pm.
- **Handing it in**: 
  - Copy all your files to `~cs537-1/handin/<cslogin>/P6/`. More details provided below.
- Late submissions
  - Projects may be turned in up to 3 days late but you will receive a penalty of 10 percentage points for every day it is turned in late.
  - Slip days: 
    - In case you need extra time on projects,  you each will have 2 slip days for individual projects and 2 slip days for group projects (4 total slip days for the semester). After the due date we will make a copy of the handin directory for on time grading.
    - To use a slip day you will submit your files with an additional file `slipdays.txt` in your regular project handin directory. This file should include one thing only and that is a single number, which is the number of slip days you want to use (ie. 1 or 2). Each consecutive day we will make a copy of any directories which contain one of these slipdays.txt files.
    - After using up your slip days you can get up to 90% if turned in 1 day late, 80% for 2 days late, and 70% for 3 days late. After 3 days we won't accept submissions.
    - Any exception will need to be requested from the instructors.
    - Example slipdays.txt
      ```
      1
      ```    
- Some tests will be provided at ~cs537-1/tests/P6. The tests will be partially complete and you are encouraged to create more tests.
- Questions: We will be using Piazza for all questions.
- Collaboration: The assignment can be done alone or in pairs. Copying code (from others) is considered cheating. [Read this](https://pages.cs.wisc.edu/~remzi/Classes/537/Spring2018/dontcheat.html) for more info on what is OK and what is not. Please help us all have a good semester by not doing this.
- This project is to be done on the [Linux lab machines](https://csl.cs.wisc.edu/docs/csl/2012-08-16-instructional-facilities/), so you can learn more about programming in C on a typical UNIX-based platform (Linux).  Your solution will be tested on these machines.

# Introduction

In this project, you will be creating a proxy web server using HTTP. The Hypertext Transport Protocol (HTTP) is the most commonly used application protocol on the Internet today. Like many network protocols, HTTP uses a client-server model. An HTTP client opens a network connection to an HTTP server and sends an HTTP request message. Then, the server replies with an HTTP response message, which usually contains some resource (file, text, binary data) that was requested by the client.

You will implement an HTTP proxy server that handles HTTP GET requests. You will provide functionality through the use of HTTP response headers, add support for HTTP error codes, and pass the request to an HTTP file server. The proxy server will wait for the response from the file server, and then forward the response to the client.

You will also be implementing a Priority Queue for the jobs sent to the proxy server (More information provided later). This proxy server will also implement functionality that will allow the client to query it for job status and order in the priority queue.

# Objectives

- To understand multi-threading, concurrency and synchronization primitives
- To implement a simplified version of a web server
- To understand client server interaction
- To understand the purpose of a proxy server
- To implement a thread-safe priority queue

# Background

## Structure of an HTTP Request
The format of a HTTP request message is:
- an HTTP request line (containing a method, a query string, and the HTTP protocol version) 
- zero or more HTTP header lines
- a blank line (i.e. a CRLF by itself)

The line ending used in HTTP requests is CRLF, which is represented as \r\n in C.
Below is an example HTTP request message sent by the Google Chrome browser to a HTTP web server
running on localhost (127.0.0.1) on port 8000 (the CRLF’s are written out using their escape sequences):

```
GET /hello.html HTTP/1.0\r\n
Host: 127.0.0.1:8000\r\n
Connection: keep-alive\r\n
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n
User-Agent: Chrome/45.0.2454.93\r\n
Accept-Encoding: gzip,deflate,sdch\r\n
Accept-Language: en-US,en;q=0.8\r\n
\r\n
```

Header lines provide information about the request. Here are some HTTP request header types:
- Host: contains the hostname part of the URL of the HTTP request (e.g. inst.eecs.berkeley.edu
or 127.0.0.1:8000)
- User-Agent: identifies the HTTP client program, takes the form Program-name/x.xx, where x.xx is the version of the program. In the above example, the Google Chrome browser sets User- Agent as Chrome/45.0.2454.93.

## Structure of an HTTP Response

The format of a HTTP response message is:
- an HTTP response status line (containing the HTTP protocol version, the status code, and a description of the status code)
- zero or more HTTP header lines
- a blank line (i.e. a CRLF by itself)
- the content requested by the HTTP request

The line ending used in HTTP requests is CRLF, which is represented as \r\n in C.
Here is a example HTTP response with a status code of 200 and an HTML file attached to the response
(the CRLF’s are written out using their escape sequences):

```
HTTP/1.0 200 OK\r\n
Content-Type: text/html\r\n
Content-Length: 128\r\n
\r\n
<html>\n
<body>\n
<h1>Hello World</h1>\n
<p>\n
Let’s see if this works\n
</p>\n
</body>\n
</html>\n
```

Typical status lines might be HTTP/1.0 200 OK (as in our example above), HTTP/1.0 404 Not Found, etc.
The status code is a three-digit integer, and the first digit identifies the general category of response:
- 1xx indicates an informational message only 
- 2xx indicates success
- 3xx redirects the client to another URL
- 4xx indicates an error in the client
- 5xx indicates an error in the server

Header lines provide information about the response. Here are some HTTP response header types:
- Content-Type: the MIME type of the data attached to the response, such as text/html or text/plain
- Content-Length: the number of bytes in the body of the response

# Project details

## Getting Started

We have provided skeleton code for the project, please download a copy of the starter code from gitlab.

## Create a basic proxy web server

You will be creating a proxy web server. From a network standpoint, the server should implement the following:
1. Create a listening socket and bind it to a port.
2. Wait for clients to connect to the port.
3. Accept the client and obtain a new connection socket
4. Read in and parse the HTTP request
5. Add the request to your priority queue in priority order (More details below).
6. Worker threads will pick up a request from the queue in priority order and send it to the target server.
7. Once the response is received from the target server, send it back to the client (or send an error message).

![Client-Proxy-Server Interaction](client_server_proxy.png)

## Running the Proxy Server

Here is the usage string for proxyserver. The argument parsing step has already been implemented for
you:

```
$ ./proxyserver -l <num listeners> <ports list> 
                -w <num workers> 
                -p <port of target file server> 
                -i <IP address of target file server> 
                -q <max number of requests in the priority queue> 
```

For example,

```
$ ./proxyserver -l 1 33489 -w 1 -i 127.0.0.1 -p 57455 -q 10
```

The available options are:
- -l : The number of listening threads, and a list of the ports for each of these threads. These threads will listen for clients trying to connect to the proxy server, and once they receive a request, will add the request to the priority queue. 
- -w : Indicates the number of threads in your thread pool that are able to concurrently process client requests. These threads will take requests from the queue and process them, and then return responses to the client.
- -p : Port number for the target file server.
- -i : IP address of the target file server.
- -q : maximum number of requests that can be added to the priority queue. If more requests arrive, you will return an error code to the user, and that request will be dropped. 


## Accessing a Webserver

You can send HTTP requests with the curl program. An example of how to use curl is:
```
    $ curl -v http://192.168.162.162:8000/
    $ curl -v http://192.168.162.162:8000/index.html
    $ curl -v http://192.168.162.162:8000/path/to/file
```

Once you have implemented your basic Proxy server, you can test it by starting a simple fileserver using Python3, and running the following commands:

```
    Start the file server inside the public_html folder
    $ python3 -m http.server 57455

    Start your proxyserver
    $ ./proxyserver -l 1 33489 -w 1 -i 127.0.0.1 -p 57455 -q 10

    Send a request to the proxy server
    $ curl 'http://localhost:33489/1/dummy1.html'

    Send a GetJob request to your proxyserver
    curl 'http://localhost:33489/GetJob'
```

We have also provided some scripts to help you run the servers
- `start.py`:  
    - Use to start the test client, the proxy server, and the target server, and pass it all the relevant information needed to start the proxy server.
    - You can run `./start.py -h` to see the usage of this script. 
    - Most of the arguments are the same as the ones described to start the proxy server
- `clean.sh`
    - This can be used to clean up any ports or other network resources.


## Common Error Messages

### Failed to bind on socket: Address already in use

This means you have a proxyserver running in the background. This can happen if your code leaks processes that hold on to their sockets, or if you disconnected from the lab machine and never shut down your server. You can fix this by running “pkill -9 proxyserver”. If that doesn’t work, you can specify a different port by running “proxyserver --files files/ --port 8001”.

### Failed to bind on socket: Permission denied
If you use a port number that is less than 1024, you may receive this error. Only the root user can use the “well-known” ports (numbers 1 to 1023), so you should choose a higher port number (1024 to 65535).

## Step by Step instructions

The tests are designed in a way to help you build up from a simple proxy server to one with more functionality, so we highly recommend going through the test cases.

1. Start listening threads and bind the socket to an IP address and port specified for that thread.

    - Bind the socket to an IPv4 address and port specified at the command line (i.e. server port)
    with the bind() syscall.
    - Afterwards, begin listening for incoming clients with the listen() syscall. At this stage, a value of 1024 is sufficient for the backlog argument of listen(). 
    - The -l flag specifies how many listening threads to create. You can start with just one, and then extend the functionality to start multiple listeners.
    - You will use the pthreads library to create and manage threads.
    - Use the accept() system call to wait for clients to connect.
    - Once the listening thread gets a request, it will parse the request. There are two types of requests that can come in
        1. A request for a file - this request must be inserted into the priority queue. 
        2. GetJob request - this is a special type of request that must be handled by the Proxy server directly. More information provided below.
    - NOTE: If the queue is full and the listener is unable to add it, reply to the client with an error message. Take a look at the enum `scode` in `proxyserver.h` to find the appropriate error code. There are also functions in `proxyserver.h` to send http headers, requests and responses.

2. Creating the priority queue. There are more details in the priority queue section below, but the basic idea is that when a request comes in from a client, it will be added to the queue in priority order. The worker threads will then take the request with highest priority from the queue and process it.

    - To begin, you can implement a simple queue structure to make sure that listening threads are able to add to it, and worker threads are able to process it.
    - Remember, since your queue is a shared data structure, with multiple threads accessing and modifying it, you MUST use some synchronization primitives (locks, semaphores, condition variables) to ensure that the queue does not get corrupted.
    - Once your simple queue is implemented and you are able to see that requests are being processed, you can work on making it a priority queue. 
    - The maximum size of the queue will be specified using the -q flag when the proxyserver is started.
    - Finding the priority of a request:
        - The priority of a request is determined by the directory name that the request is for. The public_html folder will contain subdirectories that are named with numbers, and these numbers will indicate the priority.
        - For example, if the file being requested is http://localhost:33489/10/dummy1.html, that should be serviced before a request for http://localhost:33489/3/dummy3.html. 
        - This is the directory structure inside the public_html folder.

            ```
            public_html
            |__ 1
                |__ dummy1.html
                |__ dummy2.html
            |__ 2
                |__ dummy1.html
                |__ dummy2.html
            .
            .
            .
            
            |__ 10
                |__ dummy1.html
                |__ dummy2.html
                |__ dummy3.html
            ```
        - NOTE: The directory names indicate priority, but the file names (dummy 1, dummy 2, etc.) do not. Files from within the same directory can be serviced in any order.

3. Implementing the GetJob request type.

    - This is a special request type that will be handled by the listening threads of the proxy server directly, and does NOT need to be forwarded to the file server.
    - This request type is used to pop the highest priority job from the queue and return it to the client. 
    - If a client sends a GetJob request and the queue is empty, send an error back to the client. Look for the appropriate error code in `proxyserver.h`.
    - If the queue is not empty, remove the highest priority element from the queue, and send that back to the client. This means that no worker thread will be able to service the job that has been removed.
    - For a successful GetJob request, you must return a 200 Success Return code, and in the Body of the response, you must have just the path of the file requested. For example, if the request URL of the job that was removed from the queue contains `http://localhost:33489/3/dummy3.html`, the body must contain `/3/dummy3.html`.
    - You can run a GetJob request using the following command:
        ```curl 'http://localhost:33489/GetJob' ```

4. Implementing the worker thread to serve requests.

    - NOTE: You can start with just one worker thread to serve requests, and later implement the threadpool of workers.
    - The worker threads will check the queue and process requests as they come in. 
    - The number of worker threads will be specified using the -w flag when the proxyserver is started. 
    - Each worker thread will do the following:
        - Attempt to get a job from the priority queue.
        - If there are no items on the queue, the thread will be blocked. You can implement this using locks and condition variables.
        - Once it is woken up, pop the highest priority item from the queue and handle it.
        - The request headers may specify a "delay". If there is a delay, once the request is removed from the queue, the worker thread must go to sleep for "delay" seconds. You can use the sleep() system call for this. 
        - NOTE: The delay header may be specified with a value of 0. The header may also not contain a delay. In both cases, continue to process it normally.
        - The worker thread will then call serve_request().

5. Implement the serve_request() function. This function is used to get the data from the target file server, and send that back to the client. 

    - Since you are implementing a proxy server, it works as a layer in between the clients and the target server. You are essentially maintaining 2-way communication between the HTTP client and the target HTTP server.
    - You can use functions in `proxyserver.h` to send and receive http requests.
    - The steps are as follows:
        - You must create a new socket to connect to the file server, and call the connect() system call.
        - Once the connection is established, forward the request to the file server.
        - Wait for the file server to respond using the recv() system call.
        - Once the worker thread gets the response, forward the response back to the client.
        - After the request is processed, make sure to free any resources created.

# Priority Queue for Jobs 

## What is a Priority Queue?

A priority queue is a type of queue that arranges elements based on their priority values. Elements with higher priority values are always retrieved before elements with lower priority values.

In a priority queue, each element has a priority value associated with it. When you add an element to the queue, it is inserted in a position based on its priority value. For example, if you add an element with a high priority value to a priority queue, it will be inserted near the front of the queue, while an element with a low priority value will be inserted near the back. (Max priority queue - descending order of pri)

There are several ways to implement a priority queue, including using an array, linked list, heap, or binary search tree.

The main properties of a priority queue are:

- Every item has a priority associated with it.
- An element with high priority is dequeued before an element with low priority.
- If two elements have the same priority, they can be serviced in any order.

## Proxy Server Priority Queue

In this project, you will implement a priority queue to track the jobs sent to the proxy server from the clients. As jobs come in, they will be added to the queue according to the priority which is based on which directory is being accessed. Each directory is assigned a priority, and you will have to parse the path provided to figure out the priority of the request.

You can use any implementation of a priority queue, but we recommend using a heap (with an array as the underlying structure for the heap) as it will give the best performance for this particular application. 

You will need to implement the following interface:

- `create_queue()`: Create a new priority queue.
- `add_work()`: When a new request comes in, you will insert it in priority order.
- `get_work()`: Get the job with highest priority.
    - You will need two versions of the remove functionality, one that will be called by worker threads when they want to get a new job to work on, and another that is called by listener threads when they receive a "GetJob" request.
    - `get_work()`: The worker threads will call a Blocking version of remove, where if there are no elements in the queue, they will block until an item is added. You can implement this using condition variables.
    - `get_work_nonblocking()`: The listener threads should call a Non-Blocking function to get the highest priority job. If there are no elements on the queue, they will simply return and send an error message to the client.

A note on testing the priority queue:

Your proxy server must handle the special request type "GetJob" which will be used by the tests to ensure that the order of the queue is consistent. These tests will do the following:
- If there are n worker threads, the clients will send n requests with certain amount of delay.
- This will cause all the worker threads to sleep for the duration of the delays specified.
- Any new requests that are sent by the client will be inserted into the priority queue, but will not be immediately handled by a worker thread.
- Then, the client will issue "GetJob" requests to remove the highest priority job from the queue in priority order.

# Threading and Synchronization

Please use the pthreads C library for all your threading and synchronization. You can use the man pages to find more informations about following functions:

```
Threading
    pthread_create

Locking
    pthread_mutex_init
    pthread_mutex_lock
    pthread_mutex_unlock

Condition Variables
    pthread_cond_init
    pthread_cond_wait
    pthread_cond_signal
```

NOTES:
- Remember that your queue is a shared data structure, so you must lock around it.
- You can use condition variables to signal to the worker threads when a job has been added to the queue.

## Handing it in

Please copy the following files to your `~cs537-1/handin/<cslogin>/P6/` folder.

If you are working with a partner, only one student must copy the files to their handin directory.


- proxyserver.c
- proxyserver.h
- ~~safepqueue.c~~ **<span style="color:blue">safequeue.c</span>**
- ~~safepqueue.h~~ **<span style="color:blue">safequeue.h</span>**
- Makefile
- README.md


### Further Reading and References

- [Priority Queues](https://www.geeksforgeeks.org/priority-queue-set-1-introduction/) 
- [Client Server Interaction](https://developer.mozilla.org/en-US/docs/Learn/Server-side/First_steps/Client-Server_overview)
- This assignment has been adapted from the [Berkeley File server project](https://www.google.com/url?sa=t&source=web&rct=j&opi=89978449&url=https://inst.eecs.berkeley.edu/~cs162/fa19/static/hw/hw4.pdf&ved=2ahUKEwiVgfHO9LSCAxWMl4kEHSV7CbkQFnoECBAQAQ&usg=AOvVaw0hQtqy7gU7bHvyYdloPUeh).
