# ToyDB PFLayer Buffer Manager (Assignment 1)

This project implements a paged file (PF) buffer manager for the ToyDB system. The implementation focuses on Objective 1 of the assignment: creating a configurable buffer pool with selectable page replacement strategies (LRU and MRU) and collecting I/O statistics.

## 🎯 Objective 1 Overview

The goal of this objective was to implement a page buffer for the PF layer with the following features:

* **Configurable Buffer Size:** The size of the buffer pool is set during initialization (`PF_Init`).
* **Selectable Strategies:** The replacement strategy (LRU or MRU) can be selected when a file is opened (`PF_OpenFile`).
* **Dirty Flag:** Pages track modifications via a dirty flag, with an explicit `PF_MarkDirty` function.
* **I/O Statistics:** The system collects and reports key performance metrics:
    * Logical I/Os
    * Physical I/Os (Reads + Writes)
    * Disk Reads
    * Disk Writes
* **Graphing:** The collected statistics are plotted against various mixtures of read/write queries to analyze the performance of the implemented strategies.

## 📂 Core Implementation Files

The implementation for Objective 1 required modifications to the following core files:

* `pf.h`: The public header was updated with the `PF_Strategy` enum, new function prototypes for `PF_Init`, `PF_OpenFile`, `PF_MarkDirty`, and the six statistics-gathering functions.
* `pftypes.h`: The internal header was updated to include `pf.h`, add the `strategy` field to the `PFftab_ele` struct, and declare new internal buffer functions.
* `pf.c`: The main PF layer was updated to pass the `bufsize` and `strategy` parameters to the buffer manager. It also acts as a wrapper for the public-facing statistics functions.
* `buf.c`: This file contains the core logic for the buffer manager.
    * `PFbufInit`: Initializes the buffer pool with a dynamic size.
    * `PFbufInternalAlloc`: Contains the **victim selection logic**. It checks the file's strategy and evicts from the tail (`PFlastbpage`) for **LRU** or from the head (`PFfirstbpage`) for **MRU**.
    * `PFbufUnfix` & `PFbufGet`: These functions were modified to **re-link pages based on the strategy**. For **LRU**, accessed pages are moved to the head (MRU). For **MRU**, they are moved to the tail (LRU) to protect them from eviction during a scan.
    * All statistics counters are incremented within this file.
* `hash.c`: Updated to modern ANSI C standards to resolve compiler warnings.

## 🧪 How to Compile and Run

This project includes two separate test harnesses to demonstrate the buffer manager's performance under different workloads, as required by the objective.

### 1. Compilation

You must compile two separate executables. The core `pf.c`, `buf.c`, and `hash.c` files are the same for both.

```bash
# Compile the RANDOM access workload test
gcc -o testpf pf.c buf.c hash.c testpf.c

# Compile the SEQUENTIAL access workload test
gcc -o testpf_seq pf.c buf.c hash.c testpf_seq.c
```

### 2. Running the Tests

Tests are run using the provided Python scripts, which automate the process of setting environment variables and plotting results.

---

### **Test 1: Random Access Workload**

This test (`testpf` and `graphRandomTest.py`) simulates random page accesses, which is a common database workload.

**Select Strategy:**  
Open `testpf.c` and set the strategy:

```c
#define Strategy PF_LRU 
/* or */
#define Strategy PF_MRU
```

**Recompile:**

```bash
gcc -o testpf pf.c buf.c hash.c testpf.c
```

**Run Test:**

```bash
python3 graphRandomTest.py
```

**Result:** Generates `RandomIO_statistics.png`.

---

### **Test 2: Sequential Access Workload**

This test (`testpf_seq` and `graphSeqTest.py`) simulates a full sequential scan of the file (pages 0, 1, 2...49, 0, 1...). This is the workload mentioned in the assignment to be *particularly useful* for MRU.

**Select Strategy:**  
Open `testpf_seq.c` and set the strategy:

```c
#define STRATEGY PF_LRU
/* or */
#define STRATEGY PF_MRU
```

**Recompile:**

```bash
gcc -o testpf_seq pf.c buf.c hash.c testpf_seq.c
```

**Run Test:**

```bash
python3 graphSeqTest.py
```

**Result:** Generates `SequentialIO_statistics.png`.

---

## 📊 Analysis of Results

By comparing the graphs from the two tests, you can validate the implementation:

### **Random Workload**
* LRU and MRU graphs will be nearly identical.
* This is expected — neither strategy has an advantage in purely random access when the file is larger than the buffer.

### **Sequential Workload (LRU)**
* Disk Reads ≈ **5,000**
* LRU performs poorly in sequential scans, evicting the page that is about to be needed next.

### **Sequential Workload (MRU)**
* Disk Reads ≈ **50–100**
* MRU evicts the *most recently used* page—the one just read—preserving older pages in the scan.
* This matches expected textbook behavior and validates the implementation.
