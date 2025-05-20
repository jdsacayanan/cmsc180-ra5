#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int printMatrices = 0; // 1 to print the matrix, 0 to not print


// a function to generate a random square matrix of size nXn
double** generateSquareMatrix(int size) {
    // define min and max random integer values
    int minRandomInt = 1;
    int maxRandomInt = 100;

    // create an nXn matrix
    double **matrix = (double**)(malloc(size*sizeof(double*)));
    for (int i=0; i<size; i++) {
        double *temp = (double*)malloc(size*sizeof(double));
        for (int j=0; j<size; j++) {
            // assign a random integer value to each cell
            temp[j] = minRandomInt+ rand()%(maxRandomInt-minRandomInt+1);
        }
        matrix[i] = temp;
    }
    return matrix;
}


// function to print a matrix
void printMatrix(double **matrix, int m, int n) {
    for (int i=0; i<m; i++) {
        for (int j=0; j<n; j++) {
            printf("%f\t", matrix[i][j]);
        }
        printf("\n");
    }
}


// function to print time elapsed
void printTimeElapsed(double time_elapsed) {
    printf("\n====================================\n");
    printf("Time elapsed: %f seconds\n", time_elapsed);
    printf("====================================\n\n");
}


// a structure to hold the thread arguments
struct threadArguments {
    int num_rows;
    int row_start;
    int num_cols;
    double **matrix;
    int thread_id;
    char *ipAddress;
    int port;
};


// function to get the number of CPU cores
int get_cpu_count() {
    return sysconf(_SC_NPROCESSORS_ONLN)-1;
}


// a function to set thread affinity to a specific core
void set_thread_affinity(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t thread = pthread_self();
    if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
        perror("pthread_setaffinity_np");
    }
}


// thread to compute for the z-score normalization
void* computeZScore(void *arg) {
    // get thread arguments
    struct threadArguments *d = (struct threadArguments*)arg;

    // pin this thread to a specific CPU core
    set_thread_affinity(d->thread_id % get_cpu_count());
    printf("Thread %d pinned to core %d\n", d->thread_id, d->thread_id % get_cpu_count());

    // start z-score normalization
    for (int i=0; i<d->num_rows; i++) {
        double a_j = 0;
        for (int j=0; j<d->num_cols; j++) {
            a_j += d->matrix[d->row_start + i][j];
        }
        a_j /= d->num_cols;

        double d_j = 0;
        for (int j=0; j<d->num_cols; j++) {
            d_j += pow(d->matrix[d->row_start + i][j] - a_j, 2);
        }
        d_j = sqrt(d_j / d->num_cols);

        // solve for Tij
        for (int j=0; j<d->num_cols; j++) {
            d->matrix[d->row_start + i][j] = (d->matrix[d->row_start + i][j] - a_j) / d_j;
        }
    }
    printf("Thread %d finished z-score normalization\n", d->thread_id);
    return NULL;
}


// slave thread function
void* slave_thread(void *arg) {
    // get thread arguments
    struct threadArguments *d = (struct threadArguments*)arg;

    // pin this thread to a specific CPU core
    set_thread_affinity(d->thread_id % get_cpu_count());
    printf("Thread %d pinned to core %d\n", d->thread_id, d->thread_id % get_cpu_count());

    int socket_desc, client_sock, client_size;
    struct sockaddr_in server_addr, client_addr;

    // create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc < 0) {
        printf("Error while creating socket %d\n", d->thread_id);
        pthread_exit(NULL);
    }
    printf("Socket %d created successfully\n", d->thread_id);

    // initialize server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(d->port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // bind socket to server address
    if (bind(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Couldn't bind to the port %d\n", d->port);
        pthread_exit(NULL);
    }
    printf("Successfully bound to port %d\n", d->port);

    // listen for incoming connections
    if (listen(socket_desc, 1) < 0) {
        printf("Error while listening\n");
        pthread_exit(NULL);
    }
    printf("Listening for incoming connections...\n");

    // accept incoming connection
    client_size = sizeof(client_addr);
    client_sock = accept(socket_desc, (struct sockaddr*)&client_addr, (socklen_t *)&client_size);
    if (client_sock < 0){
        printf("Can't accept connection\n");
        pthread_exit(NULL);
    }
    printf("Master connected from %s:%i\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    // receive thread id from master
    if (recv(client_sock, &d->thread_id, sizeof(d->thread_id), 0) < 0) {
        printf("Error while receiving thread id\n");
        pthread_exit(NULL);
    }
    printf("Received thread id: %d\n", d->thread_id);

    // receive num_rows from master
    if (recv(client_sock, &d->num_rows, sizeof(d->num_rows), 0) < 0) {
        printf("Error while receiving num_rows\n");
        pthread_exit(NULL);
    }
    printf("Received num_rows: %d\n", d->num_rows);

    // receive num_cols from master
    if (recv(client_sock, &d->num_cols, sizeof(d->num_cols), 0) < 0) {
        printf("Error while receiving num_cols\n");
        pthread_exit(NULL);
    }
    printf("Received num_cols: %d\n", d->num_cols);

    // create a matrix of size num_rowsXnum_cols
    double **matrix = (double**)(malloc(d->num_rows*sizeof(double*)));
    for (int i=0; i<d->num_rows; i++) {
        double *temp = (double*)malloc(d->num_cols*sizeof(double));
        matrix[i] = temp;
    }

    // receive matrix from master (number by number)
    printf("Receiving matrix from master...\n");
    for (int i=0; i<d->num_rows; i++) {
        for (int j=0; j<d->num_cols; j++) {
            if (recv(client_sock, &matrix[i][j], sizeof(double), 0) < 0) {
                printf("Error while receiving matrix\n");
                pthread_exit(NULL);
            }
        }
    }

    // print the matrix
    if (printMatrices) {
        printf("Matrix received from master:\n");
        printMatrix(matrix, d->num_rows, d->num_cols);
    }

    // perform computation on the matrix
    // use the number of cpu cores to determine the number of threads
    int cpu_count = get_cpu_count();

    // prepare threads
    pthread_t threads[cpu_count];
    struct threadArguments thread_args[cpu_count];
    int rows_per_thread = d->num_rows / cpu_count;
    int extra_rows = d->num_rows % cpu_count;
    int current_row = 0;
    for (int i=0; i<cpu_count; i++) {
        // create thread arguments
        thread_args[i].num_rows = rows_per_thread; // number of rows to process
        thread_args[i].num_cols = d->num_cols; // number of columns to process
        thread_args[i].row_start = current_row; // update starting row index
        // if there are extra rows, assign one to this submatrix
        if (extra_rows > 0) {
            thread_args[i].num_rows++;
            extra_rows--;
        }
        current_row += thread_args[i].num_rows;
        thread_args[i].matrix = matrix;
        thread_args[i].thread_id = d->thread_id + i;
    }

    // start timer
    struct timeval start, end;
    printf("Starting timer...\n");
    gettimeofday(&start, NULL);

    // create threads
    for (int i=0; i<cpu_count; i++) {
        // create thread
        if (pthread_create(&threads[i], NULL, computeZScore, (void*)&thread_args[i]) < 0) {
            printf("Error while creating thread %d\n", i+1);
            pthread_exit(NULL); // terminate thread
        }
        printf("Thread %d created successfully\n", i+1);
    }

    // wait for threads to finish
    for (int i=0; i<cpu_count; i++) {
        pthread_join(threads[i], NULL);
        printf("Thread %d finished successfully\n", i+1);
    }
    printf("All threads finished successfully\n");

    // stop timer
    gettimeofday(&end, NULL);
    double time_elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
    printTimeElapsed(time_elapsed);

    // print computed matrix
    if (printMatrices) {
        printf("Matrix after computation:\n");
        printMatrix(matrix, d->num_rows, d->num_cols);
    }

    // send computed matrix back to master
    printf("Sending computed matrix back to master...\n");
    for (int i=0; i<d->num_rows; i++) {
        for (int j=0; j<d->num_cols; j++) {
            if (send(client_sock, &matrix[i][j], sizeof(double), 0) < 0) {
                printf("Error while sending matrix\n");
                pthread_exit(NULL);
            }
        }
    }
    printf("Sent matrix back to master\n");

    // send confirmation that computation is done
    char ack[] = "ack";
    if (send(client_sock, ack, sizeof(ack), 0) < 0) {
        printf("Error while sending ack\n");
        pthread_exit(NULL);
    }
    printf("Sent ACK to master\n");

    // close socket
    close(client_sock);
    printf("Socket %d closed successfully\n", d->thread_id);

    // free the matrix
    for (int i=0; i<d->num_rows; i++) {
        free(matrix[i]);
    }
    free(matrix);
    printf("Matrix freed successfully\n");
    printf("Slave thread %d finished successfully\n", d->thread_id);
    return NULL;
}


// function to run as slave aka server
int slave_server(int port) {
    // create a thread to receive the matrix from the master
    struct threadArguments d;
    d.port = port;

    // for the sake of this example, we will use a random matrix
    d.matrix = generateSquareMatrix(4); // 4X4 matrix
    d.num_rows = 4; // number of rows
    d.num_cols = 4; // number of columns
    d.row_start = 0; // starting row index
    d.thread_id = 1; // thread id

    // create a thread to handle receiving the matrix
    // this is also to pin the thread to a specific CPU core
    pthread_t thread;
    if (pthread_create(&thread, NULL, slave_thread, (void*)&d) < 0) {
        printf("Error while creating thread\n");
        return -1; // terminate program
    }
    printf("Thread created successfully\n");

    // wait for the thread to finish
    pthread_join(thread, NULL);
    printf("Thread finished successfully\n");

    printf("Slave closed\n");
    return 0;
}


void* master_thread(void *arg) {
    // get thread arguments
    struct threadArguments *d = (struct threadArguments*)arg;

    // pin this thread to a specific CPU core
    set_thread_affinity(d->thread_id % get_cpu_count());
    printf("Thread %d pinned to core %d\n", d->thread_id, d->thread_id % get_cpu_count());

    int socket_desc;
    struct sockaddr_in server_addr;

    // create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc < 0) {
        printf("Error while creating socket %d\n", d->thread_id);
        pthread_exit(NULL); // terminate thread
    }
    printf("Socket %d created successfully\n", d->thread_id);

    // set port and ip for the slave we are connecting to
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(d->port);
    server_addr.sin_addr.s_addr = inet_addr(d->ipAddress);

    // connect to slave
    printf("Connecting to slave %d at %s:%i ...\n", d->thread_id, d->ipAddress, d->port);
    if (connect(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error while connecting to slave %d\n", d->thread_id);
        pthread_exit(NULL); // terminate thread
    }
    printf("Successfully connected to slave %d\n", d->thread_id);

    // send thread id to slave
    if (send(socket_desc, &d->thread_id, sizeof(d->thread_id), 0) < 0) {
        printf("Error while sending thread id to slave %d\n", d->thread_id);
        pthread_exit(NULL); // terminate thread
    }
    printf("Sent thread id to slave %d\n", d->thread_id);

    // send num_rows to slave
    if (send(socket_desc, &d->num_rows, sizeof(d->num_rows), 0) < 0) {
        printf("Error while sending num_rows to slave %d\n", d->thread_id);
        pthread_exit(NULL); // terminate thread
    }
    printf("Sent num_rows to slave %d\n", d->thread_id);

    // send num_cols to slave
    if (send(socket_desc, &d->num_cols, sizeof(d->num_cols), 0) < 0) {
        printf("Error while sending num_cols to slave %d\n", d->thread_id);
        pthread_exit(NULL); // terminate thread
    }
    printf("Sent num_cols to slave %d\n", d->thread_id);

    // send matrix to slave (number by number)
    printf("Sending matrix to slave %d...\n", d->thread_id);
    for (int i=0; i<d->num_rows; i++) {
        for (int j=0; j<d->num_cols; j++) {
            if (send(socket_desc, &d->matrix[d->row_start + i][j], sizeof(double), 0) < 0) {
                printf("Error while sending matrix to slave %d\n", d->thread_id);
                pthread_exit(NULL); // terminate thread
            }
        }
    }
    printf("Sent matrix to slave %d\n", d->thread_id);

    // receive computed matrix from slave
    for (int i=0; i<d->num_rows; i++) {
        for (int j=0; j<d->num_cols; j++) {
            if (recv(socket_desc, &d->matrix[d->row_start + i][j], sizeof(double), 0) < 0) {
                printf("Error while receiving matrix from slave %d\n", d->thread_id);
                pthread_exit(NULL); // terminate thread
            }
        }
    }
    printf("Received matrix from slave %d\n", d->thread_id);

    // receive confirmation that computation is done
    char ack[3];
    if (recv(socket_desc, ack, sizeof(ack), 0) < 0) {
        printf("Error while receiving ack from slave %d\n", d->thread_id);
        pthread_exit(NULL); // terminate thread
    }
    printf("Received ACK from slave %d\n", d->thread_id);

    // close socket
    close(socket_desc);
    printf("Socket %d closed successfully\n", d->thread_id);
    return NULL;
}


// function to run as master aka client
int master_client(int n) {
    // create a random matrix of size nXn
    double **matrix = generateSquareMatrix(n);

    // print the matrix
    if (printMatrices) {
        printf("Matrix before computation:\n");
        printMatrix(matrix, n, n);
    }

    // read the config file
    FILE *fp = fopen("config.txt", "r");
    if (fp == NULL) {
        printf("Error opening config file.\n");
        return -1;
    }
    char line[256];
    // read line by line to determine the number of slaves
    // used to allot memory for the thread arguments
    // and to read the IP address and port number of the slaves
    int t = 0; // slaves
    while (fgets(line, sizeof(line), fp) != NULL) {
        t++;
    }
    fclose(fp);
    printf("Number of slaves: %d\n", t);

    // prepare the submatrices to send (only arguments)
    int rows_per_thread = n / t; // number of rows per thread
    int extra_rows = n % t; // number of extra rows to distribute
    int current_row = 0; // current row index
    struct threadArguments d[t];
    for (int i=0; i<t; i++) {
        // create thread arguments
        d[i].num_rows = rows_per_thread; // number of rows to process
        d[i].num_cols = n; // number of columns to process
        d[i].row_start = current_row; // update starting row index
        // if there are extra rows, assign one to this submatrix
        if (extra_rows > 0) {
            d[i].num_rows++;
            extra_rows--;
        }
        current_row += d[i].num_rows;
        d[i].matrix = matrix;
        d[i].thread_id = i+1;
    }

    // rewind the file pointer to the beginning of slaves
    fp = fopen("config.txt", "r");
    if (fp == NULL) {
        printf("Error opening config file.\n");
        return -1;
    }

    // read the IP address and port number of the slaves
    for (int i=0; i<t; i++) {
        // read config file line by line
        char line[256];
        fscanf(fp, "%s", line);
        // line contains the IP address and port number
        char *slaveIP = strtok(line, ":");
        char *slavePort = strtok(NULL, ":");
        int port = atoi(slavePort); // convert port to integer
        d[i].ipAddress = strdup(slaveIP);
        d[i].port = port;
    }
    fclose(fp);
    printf("Slaves IP addresses and port numbers read successfully\n");

    //start timer
    struct timeval start, end;
    printf("Starting timer...\n");
    gettimeofday(&start, NULL);

    // create threads to handle sending the matrix to slaves
    pthread_t threads[t];
    for (int i=0; i<t; i++) {
        // create thread
        if (pthread_create(&threads[i], NULL, master_thread, (void*)&d[i]) < 0) {
            printf("Error while creating thread %d\n", i+1);
            return -1; // terminate program
        }
        printf("Thread %d created successfully\n", i+1);
    }

    // wait for threads to finish
    for (int i=0; i<t; i++) {
        pthread_join(threads[i], NULL);
        printf("Thread %d finished successfully\n", i+1);
    }
    printf("All threads finished successfully\n");

    // stop timer
    gettimeofday(&end, NULL);
    double time_elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
    printTimeElapsed(time_elapsed);

    // print the matrix after computation
    if (printMatrices) {
        printf("Matrix after computation:\n");
        printMatrix(matrix, n, n);
    }
    // free the matrix
    for (int i=0; i<n; i++) {
        free(matrix[i]);
    }
    free(matrix);
    printf("Matrix freed successfully\n");
    printf("Master closed\n");
}

int main() {
    int n, p, s;
    scanf("%d %d %d", &n, &p, &s);

    // check the variables read from the input
    if (s==0) {
        printf("Running as master...\n");
        printf("Matrix size: %d\n", n);
        master_client(n);
    } else {
        printf("Running as slave...\n");
        printf("Port number: %d\n", p);
        slave_server(p);
    }
}