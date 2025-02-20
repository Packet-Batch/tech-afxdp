A small C API for interacting with [AF_XDP sockets](https://docs.kernel.org/networking/af_xdp.html) on Linux which includes setting up sockets, sending packets on sockets, and cleaning up sockets. While this may be used separately, this was primarily made for an upcoming revamp for [Packet Batch](https://github.com/Packet-Batch).

**NOTE** - This is experimental and a **work-in-progress**!

## How It Works
When building, a shared library (`libpbafxdp.so`) is created which is then installed to the `/usr/lib` directory.

There is also an [`api.h`](./include/api.h) header file that may be installed to `/usr/include/pb/afxdp/api.h` which should be included by programs that want to utilize this small API.

## Building & Installing
Before building this project, make sure to build and install [`xdp-tools`](https://github.com/xdp-project/xdp-tools) onto your server's system. When creating programs using this API, you will need to link `xdp-tools` via the `-lxdp -lbpf` linker flags.

Afterwards, use `make` to build the shared library and `make install` (as root or via `sudo`) to install the shared library and API header file onto your system.

```bash
# Clone repository (include sub-modules).
git clone --recursive https://github.com/Packet-Batch/tech-afxdp.git

# Change to repository.
cd tech-afxdp/

# Build shared library.
make

# Install shared library and API header.
sudo make install
```

## API Usage
The following functions are exposed by the API.

```C
/**
 * Sets up AF_XDP sockets.
 * 
 * @param dev The interface to bind the AF_XDP sockets to.
 * @param queueId The TX queue ID to use.
 * @param needWakeup If 1, sets the need wakeup flag on the sockets.
 * @param sharedUmem If 1, uses a shared umem allocated between all sockets.
 * @param forceSkb If 1, forces SKB mode (slower, but useful for debugging).
 * @param zeroCopy If 1, will set the zero-copy flag.
 * @param threads The amount of sockets and threads to create.
 * 
 * @return 0 on success or other value on error.
 */
extern int Setup(const char *dev, int queueId, int needWakeup, int sharedUmem, int forceSkb, int zeroCopy, int threads);

/**
 * Cleans up AF_XDP sockets.
 * 
 * @param threads The amount of sockets and threads that were created in Setup().
 * 
 * @return 0 on success or other value on error.
 */
extern int Cleanup(int threads);

/**
 * Sends a packet on an AF_XDP socket (at index).
 * 
 * @param pkt The packet buffer.
 * @param length The packet's full length (includes ethernet header, layer 3/4 headers, and payload).
 * @param threadIdx The socket index to send on.
 * @param batchSize The batch size.
 * 
 * @return 0 on success or other value on error.
 */
extern int SendPacket(void *pkt, int length, int threadIdx, int batchSize);
```

Take a look at tests from the [`tests/`](./tests/) directory for more information.

## Tests
* [`tcp_syn_test.c`](./tests/tcp_syn_test.c) - Sends a single TCP SYN packet on an AF_XDP socket (no checksum calculated).

## Notes
### Do I Need To Set The `LD_LIBRARY_PATH` Environmental Variable?
You should **not** need to set your `LD_LIBRARY_PATH` environmental variable if you installed the shared library to `/usr/lib` since that directory is usually included when searching for linked libraries by default.

If you have issues, you could try setting it specifically via `export LD_LIBRARY_PATH=/usr/lib`.

## Credits
* [Christian Deacon](https://github.com/gamemann)