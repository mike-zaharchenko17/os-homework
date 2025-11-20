# Assignment-04 README 

For this assignment, I ran experiments in two environments: on my computer (MacOS, 14 core) and on GitHub Codespaces (Linux, 4 core). This is because MacOS doesn't have pthread_spinlock_t.

Predictably, results in the Codespaces machine were slower and benefitted less from concurrency. Likewise, the unsafe threads lost less keys in Codespaces.

### Question 1

**Code Trace**

Spawn a bunch of threads (as specified) with the put_phase routine

In each thread:
- execute put_phase
	- put_phase takes the thread number as an argument
	- it begins with the thread number and iterates through the number of keys
	- it chunks work by incrementing by the number of threads
		- in a single threaded environment, this will just iterate through everything from 0
		- in a multi-threaded environment, we'll use the TID's as an initial offset and increment by the number of threads 
			- so, given 5 threads
				- t0 handles 0, 5, 10, ..
				- .
				- t1 handles 1, 6, 11, ...
				- t2 handles 2, 7, 12, ...
				- t3 handles 3, 8, 13, ...
				- t4 handles 4, 9, 14, ...
	- and it calls insert with the `keys[key]` as argument and the initial (offset) tid as value

- insert 
	- first picks a bucket
		- `int i = key % NUM_BUCKETS` is the hash function
			- every key will land in bucket \[0, NUM_KEYS]
	- then allocates a new node
		- allocates a linked list node that will represent a (k, v) pair
	- sets e->next to the current head of the list in that bucket
	- copies over the data we want to insert to e's key and value
	- sets the head of the table at that bucket to the current node

**What a Lost Entry Is**

A lost entry is an entry that is overwritten due to a race condition. Insert is the reason these race conditions happen. 

The key passed to put_phase is staggered by design, so there is no conflict there.

However, there are conflicts in the insert statement.

Say two threads both mod to the same i value and both attempt to access table\[i]. A race condition can occur.

- Thread A reads `table[i]` (gets pointer to old head)
- Thread B reads `table[i]` (gets same pointer)
- Thread A writes new head = entryA
- Thread B writes new head = entryB (overwriting entryA)

Thus, the key is lost.

Due to the barrier, there is no potential for a race condition between the put phase and the get phase (like if the retrieval threads were reading from the hash map as the insertion threads were modifying it). There is no direct potential for a race condition in the retrieval code either as no writes are being performed.

*** MacOS Mutex vs. No Mutex ***

*** Mutex ***

| Mutex | Num Threads | Time To Insert | Time to Retrieve | % Retrieved | % Loss |
| ----- | ----------- | -------------- | ---------------- | ----------- | ------ |
| TRUE  | 1           | 0.002307       | 1.201325         | 100%        | 0%     |
| TRUE  | 2           | 0.003116       | 0.742021         | 100%        | 0%     |
| TRUE  | 4           | 0.007622       | 0.472902         | 100%        | 0%     |
| TRUE  | 8           | 0.006069       | 0.271812         | 100%        | 0%     |
| TRUE  | 12          | 0.006252       | 0.23633          | 100%        | 0%     |

*** No Mutex ***

| Mutex | Num Threads | Time To Insert | Time to Retrieve | % Retrieved | % Loss |
| ----- | ----------- | -------------- | ---------------- | ----------- | ------ |
| FALSE | 1           | 0.002094       | 1.144693         | 100%        | 0%     |
| FALSE | 2           | 0.002295       | 0.648996         | 87%         | 13%    |
| FALSE | 4           | 0.003486       | 0.418115         | 69%         | 31%    |
| FALSE | 8           | 0.005925       | 0.28013          | 55%         | 45%    |
| FALSE | 12          | 0.003983       | 0.253012         | 51%         | 49%    |

*** Codespaces Mutex vs. No Mutex ***

*** Mutex ***

| Num Threads | Time To Insert | Time to Retrieve | % Retrieved | % Lost |
|-------------|----------------|------------------|-------------|--------|
| 1           | 0.006021       | 7.147848         | 100%        | 0%     |
| 2           | 0.00813        | 3.571987         | 100%        | 0%     |
| 4           | 0.006709       | 1.727046         | 100%        | 0%     |
| 8           | 0.006862       | 2.013532         | 100%        | 0%     |
| 12          | 0.007648       | 2.087144         | 100%        | 0%     |

*** No Mutex ***

| Num Threads | Time To Insert | Time to Retrieve | % Retrieved | % Lost |
|-------------|----------------|------------------|-------------|--------|
| 1           | 0.005542       | 7.238285         | 100%        | 0%     |
| 2           | 0.00317        | 4.043671         | 97%         | 3%     |
| 4           | 0.002577       | 1.828751         | 98%         | 2%     |
| 8           | 0.003365       | 1.963509         | 98%         | 2%     |
| 12          | 0.003846       | 1.843432         | 98%         | 2%     |


In codespaces, you can expect a slowdown of about 2-3x using the mutex implementation as opposed to the unsafe implementation during the put phase.

Since there is no mutex placed on the retrieval process, the time to retrieve is largely the same across all thread counts.

### Question 2

*** Intuition/Hypothesis ***

Since this is a high contention scenario (only 5 buckets), I would expect the mutex to win. We would waste cycles busy waiting, which should degrade performance despite the lightweight critical section.

*** Spinlock Results vs. Mutex (Codespaces) ***

*** Spinlock ***

| Num Threads | Time To Insert | Time to Retrieve | % Retrieved | % Lost |
|-------------|----------------|------------------|-------------|--------|
| 1           | 0.005664       | 7.373502         | 100%        | 0%     |
| 2           | 0.004688       | 3.660478         | 100%        | 0%     |
| 4           | 0.00336        | 1.649929         | 100%        | 0%     |
| 8           | 0.004088       | 1.806454         | 100%        | 0%     |
| 12          | 0.004203       | 1.800111         | 100%        | 0%     |

*** Mutex ***

| Num Threads | Time To Insert | Time to Retrieve | % Retrieved | % Lost |
|-------------|----------------|------------------|-------------|--------|
| 1           | 0.006021       | 7.147848         | 100%        | 0%     |
| 2           | 0.00813        | 3.571987         | 100%        | 0%     |
| 4           | 0.006709       | 1.727046         | 100%        | 0%     |
| 8           | 0.006862       | 2.013532         | 100%        | 0%     |
| 12          | 0.007648       | 2.087144         | 100%        | 0%     |

*** Conclusion ***

My hypothesis was incorrect. The spinlock is faster than the mutex across all thread counts. This is likely because a mutex is more expensive than a spinlock to lock/unlock, and the lock/unlock cost dominates.

I imagine that if our critical section was heavier, the mutex would be more efficient because the impacts of busy-wait would become more apparent.

*** Estimate ***

With the spinlock, performance is roughly comparable to the unlocked implementation (again, codespaces). You can expect slowdowns of about 1.2x (revisit this with actual math)

### Question 3

We don't need a lock. All insert operations complete before any retrieval operations begin, and the main thread waits for all inserter threads to finish using pthread_join. This creates a barrier between the put phase and the get phase. After that barrier, the hash table is read-only- no thread modifies it during retrieval. Since there are no concurrent writers, multiple threads can safely call retrieve in parallel without locking.

I implemented this in parallel_mutex.c (instead of, say, wrapping both insert and retrieve with a mutex) so I'll just copy and paste that code.

### Question 4

*** When can insertions be parallelized? ***

Multiple insertions can happen safely if there is no conflict for the same bucket. 

We've identified our race condition as something that occurs when multiple threads attempt to access the same table[i] value at the same time and then overwrite the changes that another thread has made, so if we prevent that from happening, we can have parallel insertions.

We can achieve this using per bucket mutexes

Note: we define a 'bucket' as table[i]- the head of one linked list of entries and a 'conflict for the same bucket' as key1 % BUCKET_NUM == key2 % BUCKET_NUM

*** Changes to parallel_mutex_opt.c ***

- global
	- removed the global mutex
	- declared an array of mutexes in the global scope
- main
	- allocated space for the global mutex array s/t to accommodate NUM_BUCKETS mutexes s/t mutex i corresponds to bucket i
	- initialized mutexes in a loop (similarly to how threads are created in the code)
	- destroyed mutexes after completion
- insert
	- removed the lock/unlock with the global mutex from the critical section
	- replaced it with a lock/unlock that uses the mutex that corresponds to index key % NUM_BUCKETS










