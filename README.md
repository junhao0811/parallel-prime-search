# 🧮 Parallel Prime Search

One problem, five implementations: find every prime below *N* using **serial C, POSIX Threads, OpenMP, MPI, and hybrid MPI + OpenMP**, then measure how each one scales.

---

## 🔢 The implementations

| File | Model | Approach |
|---|---|---|
| `serial.c` | Single core | Baseline trial division using the **6k ± 1** optimisation |
| `pthreads.c` | Shared memory | Splits the range across POSIX threads, each with thread-local result storage |
| `openmp.c` | Shared memory | Parallel loop with OpenMP scheduling directives |
| `mpi.c` | Distributed memory | Broadcasts *N*, splits the work across processes, then gathers and sorts the results |
| `hybrid_mpi_openmp.c` | Hybrid | MPI across processes with OpenMP threads inside each one, using a **block-cyclic** distribution (cache-sized blocks) for load balancing |

Every version writes the primes it finds to a text file and reports the count and execution time.

---

## 📈 Results

Shared-memory scaling (from [`docs/shared_memory_report.pdf`](docs/shared_memory_report.pdf), *n* = 10,000,000):
- **Pthreads** reached a peak speed-up of **3.15×** on 8 threads, thanks to low runtime overhead
- **OpenMP** scaled more steadily with loop scheduling, reaching **2.23×** on 16 threads

All three shared-memory versions agree on the answer, for example **78,498 primes below 1,000,000**.

The distributed MPI and hybrid design and results are in [`docs/distributed_memory_report.pdf`](docs/distributed_memory_report.pdf). Raw shared-memory timings are in [`docs/shared_memory_timings.xlsx`](docs/shared_memory_timings.xlsx).

---

## 🚀 Build and run

```bash
# Serial / shared memory (gcc)
gcc -O2 serial.c   -o serial   -lm             && ./serial 1000000
gcc -O2 pthreads.c -o pthreads -pthread -lm    && ./pthreads 1000000 4
gcc -O2 -fopenmp openmp.c -o openmp -lm        && ./openmp 1000000 4

# Distributed (Open MPI)
mpicc -O2 mpi.c -o mpi -lm                                && mpirun -np 4 ./mpi 1000000
mpicc -O2 -fopenmp hybrid_mpi_openmp.c -o hybrid -lm      && mpirun -np 2 ./hybrid 1000000
```

Arguments: `N` (search below this number) and, for the threaded versions, the number of threads.

## 👥 Team
The serial, Pthreads and OpenMP versions and their analysis are my solo work. The MPI and hybrid versions were built in a team of three.

## 🛠️ Tech
C · POSIX Threads · OpenMP · MPI · hybrid parallelism · performance analysis
