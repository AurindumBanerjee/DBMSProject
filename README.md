# DBMS Project : ToyDB Enhancements

## Team Members

> **Aurindum Banerjee** (B23CS1006)  
> **Lokesh Motwani** (B23CS1033)

---

We implemented several enhancements across three layers of the ToyDB system:
1. **Paged File Layer (PF)**
2. **Record Management Layer (RM)**
3. **Access Method Layer (AM)**

Each objective is implemented, tested, and evaluated with performance graphs and metrics.

---

# Objective 1 - Page Buffering (PF Layer)

## Description

Objective 1 focuses on implementing a **buffer pool** for the PF layer with:

- **Configurable buffer size** via `PF_Init()`
- **Two page replacement strategies**:
  - `LRU` — Least Recently Used
  - `MRU` — Most Recently Used (optimal for sequential scans)
- **Dirty flag** support via `PF_MarkDirty()`
- **Comprehensive I/O statistics**:
  - Logical I/Os  
  - Physical I/Os  
  - Disk Reads  
  - Disk Writes  
- **Python graphing scripts** to analyze performance under different read/write workloads.

---

## Objective 1 Results

### LRU vs MRU — Total Physical I/O Comparison  
![LRU vs MRU Physical IO](ToyDB/images/Objective1_Phy.jpg)

### Full Statistics: Logical, Physical, Reads, Writes for LRU & MRU  
![LRU vs MRU Detailed IO](ToyDB/images/Objective1_IO.jpg)

These graphs show:

- In **random workloads**, LRU ≈ MRU → expected behavior  
- In **sequential workloads**:
  - **LRU performs poorly**, repeatedly evicting pages needed next  
  - **MRU performs optimally**, keeping older pages in memory → *dramatically fewer reads*
 
## Build & Run Instructions

Compile Test Files
```
# Compile Random Access Test (LRU & MRU)
gcc -DStrategy=PF_LRU -o testpf_LRU testpf.c pf.c buf.c hash.c
gcc -DStrategy=PF_MRU -o testpf_MRU testpf.c pf.c buf.c hash.c

# Compile Sequential Access Test (LRU & MRU)
gcc -DSTRATEGY=PF_LRU -o testpf_seq_LRU testpf_seq.c pf.c buf.c hash.c
gcc -DSTRATEGY=PF_MRU -o testpf_seq_MRU testpf_seq.c pf.c buf.c hash.c

```

Generate Performance Plots
```
python3 graphSeqTest.py
python3 graphRandomTest.py
```

---

# Objective 2 - Variable-Length Records (RM Layer)

## Description

In this, we implemented a **slotted-page layout** to store variable-length records efficiently:

- Supports insertion, deletion, and scanning
- Measures **space utilization**
- Compares slotted pages vs static fixed-record-size methods

---

## Objective 2 Results

![Slotted Page Results](ToyDB/images/Objective2.jpg)

Findings:

- **Slotted page** achieved the *worst space efficiency* in our implementation.
- Static layouts waste space as maximum record size increased  .  

## Build & Run Instructions

```
cd amlayer/
make clean 
make 
./test_am3
```

---

# Objective 3 - B+ Tree Index Construction (AM Layer)

## Description

We implemented and compared three index build strategies:

1. **Incremental Load**
2. **Bulk Load (Unsorted)**
3. **Optimized Bulk Load (Sorted)**

Evaluation metrics:

- Total **construction time**
- Total **physical I/Os**

---

## Objective 3 Results

![B+ Tree Construction Results](ToyDB/images/Objective3.jpg)

Summary:

- Incremental and unsorted bulk load produce higher physical I/O
- **Optimized sorted bulk load** is *better*:  
  - Lowest I/O  
  - Fastest runtime  

## Build & Run Instructions

```
cd rmlayer/
make clean 
make 
./test_rm
```





