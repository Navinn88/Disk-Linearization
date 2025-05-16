# JBOD Storage Emulator & MDADM Linear Device
JBOD Storage Emulator & MDADM Linear Device

This project implements a storage emulator in C, providing a Just a Bunch Of Disks (JBOD) driver, an MDADM-style linear device layer, an LRU block cache, and a networked client interface. It includes a test harness that replays I/O traces to validate functional correctness and performance.

Overview

JBOD Driver (jbod.h, jbod.o): Low-level interface with commands for mounting disks, reading/writing blocks, and querying device geometry.

MDADM Linear Layer (mdadm.c, mdadm.h): Maps a virtual linear address space across multiple JBOD disks. Supports mount, unmount, read, and write operations.

LRU Cache (cache.c, cache.h): In-memory cache of block data, using a Least-Recently-Used eviction policy to reduce disk I/O and improve throughput.

Networking Client (net.c, net.h): Socket-based implementation of the JBOD protocol, allowing a remote client to issue mount/read/write commands over TCP/IP.

Utilities (util.c, util.h): Helper functions for Base64 encoding/decoding (for secure credential exchanges), logging, and configuration file parsing.

Test Harness (tester.c, tester.h): Unit tests and trace-replay logic that feed prerecorded workloads (in traces/) into the emulator and compare outputs against expected checksums.

Project Structure
- **src/**
  - `jbod.h`             – JBOD driver command definitions and constants
  - `jbod.o`             – Precompiled driver object file (linker input)
  - `mdadm.h`            – MDADM layer interface: mount/unmount/read/write prototypes
  - `mdadm.c`            – Linear device implementation over JBOD
  - `cache.h`            – LRU cache interface: init, lookup, insert, evict
  - `cache.c`            – Block‐cache implementation with timestamp tracking
  - `net.h`              – Network protocol definitions for JBOD commands
  - `net.c`              – Client-side socket setup and packet handling
  - `util.h`             – Utility declarations: Base64, logging, file I/O
  - `util.c`             – Utility implementations
  - `tester.h`           – Trace harness declarations
  - `tester.c`           – Drive trace replay and result validation
  - `Makefile`           – Build rules: compile, link, clean
- **traces/**
  - `simple-input`       – Basic mount/read/write trace workload
  - `linear-input`       – Sequential access trace
  - `random-input`       – Randomized read/write workload
  - `*-expected-output`  – Reference checksums for each trace
Build & Test
Build all components and tester
make
Run the test harness against all traces
./tester simple-input
./tester linear-input
./tester random-input

Techniques & Skills Learned

Low-Level Systems Programming

Designed and integrated a custom block-device protocol (JBOD) in C.

Managed memory buffers for I/O with careful alignment and error checks.

Modular Layered Architecture

Separated concerns across driver, linear mapping, cache, and networking layers.

Employed clear interfaces (.h files) to decouple components.

LRU Cache Implementation

Implemented doubly-linked lists and timestamp-based eviction for block caching.

Measured cache hit/miss rates and tuned cache size for optimal performance.

Socket Programming & Networking

Built a TCP client for issuing JBOD commands remotely.

Serialized commands and payloads with explicit header formats.

Automated Testing & Trace Replay

Created a reproducible test harness to validate functional correctness.

Compared computed checksums against expected outputs to detect regressions.

Build Automation & Makefiles

Wrote efficient Makefile rules with automatic dependency handling.

Supported incremental builds and clean targets.

Debugging & Logging

Integrated configurable logging for driver and network events.

Employed verbose modes and log levels to trace I/O flows.

