# Transport Layer Deamon Process

## Overview

The goal of this daemon process is to handle all network message exchanges.

The process is split into two logical parts, the message buffer space, and the network stack.

```
            |-----------------|---------------|
App <--->   | Message buffers | Network stack |    <---> NIC
            |-----------------|---------------|
```

The first step is to build the network stack that takes pre-determined random messages in the message buffer space and operations like send and receive messages, and interact with the NIC.
The second step is to switch message buffers to use shared memory, to avoid copying data when calling send/recv from applications, and allow application-specific operations.

The end goal is to provide a generic network programming interface to applications, presumably similar to the UNIX standard socket operations like `send()` and `recv()`, but we may not support the full sets of socket operations, due to the different logic in the underlying API (in RDMA or MPI).

## Supported Operations

Connection set up / tear down:

* `connect()`: client connects to server
* `listen()`: server `bind()`
* `accept()`: server accepts new connection
* `disconnect()`: client/server closes connection

Data transfer:

* `send()`: synchronous send.
* `isend()`: asynchronous send.
* `recv()`: synchronous receive.
* `irecv()`: asynchronous receive.
* `barrier()`: similar to `MPI_Barrier()`.

~~Shared memory:~~

* ~~`register()`: share/register a memory region.~~
* ~~`deregister()`: undo a shared/registered memory region.~~

(We don't need to do this manually, see below)

## Implementation

The networking stack will use either RDMA or MPI APIs to implement various data transfer functions.

### RDMA vs MPI

Based on our observations so far, it is possible to create a generic API that allows both RDMA and MPI implementation underneath.
We're planning to perform further evaluations before deciding which one will be preferred.

### Potential Issues

* Should we make memory region registration transparent to users like in MPI?
    * Yes. The overhead is only 30ns per register call.
* How many MRs can we keep whiling achieving reasonable performance?
    * Based on a previous paper FaRM, it will get worse with large # of entries, due to limited PTEs the NIC can hold.
    * They solved the problem with a custom kernel driver for huge table. We need to evaluate the huge table support in Linux again, as previous attempt to use huge table did not make a difference.
* Connection Multiplexing Performance
    * Preliminary results show that there's little performance drop on 1-to-N traffic, but not N-to-1 to N-to-N traffic.
    * FaRM also showed that the optimal # of parallel connections may not be a fixed number.
* Should each application has its own write buffer? Maybe use a circular ring buffer?
    * This depends on how much flexibility we want to give applications.
    * For our purpose, we want to build a byte stream protocol, and applications can supply the buffers.
    * Initially we can just support asynchoronous (i.e. non-blocking) `send()/recv()` calls.
