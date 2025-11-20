# Assignment-04 README 

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

| Mutex | Num Threads | Time To Insert | Time to Retrieve | % Retrieved | % Loss |
| ----- | ----------- | -------------- | ---------------- | ----------- | ------ |
| TRUE  | 1           | 0.002307       | 1.201325         | 100%        | 0%     |
| TRUE  | 2           | 0.003116       | 0.742021         | 100%        | 0%     |
| TRUE  | 4           | 0.007622       | 0.472902         | 100%        | 0%     |
| TRUE  | 8           | 0.006069       | 0.271812         | 100%        | 0%     |
| TRUE  | 12          | 0.006252       | 0.23633          | 100%        | 0%     |

| Mutex | Num Threads | Time To Insert | Time to Retrieve | % Retrieved | % Loss |
| ----- | ----------- | -------------- | ---------------- | ----------- | ------ |
| FALSE | 1           | 0.002094       | 1.144693         | 100%        | 0%     |
| FALSE | 2           | 0.002295       | 0.648996         | 87%         | 13%    |
| FALSE | 4           | 0.003486       | 0.418115         | 69%         | 31%    |
| FALSE | 8           | 0.005925       | 0.28013          | 55%         | 45%    |
| FALSE | 12          | 0.003983       | 0.253012         | 51%         | 49%    |

For insertion, you can expect a 1.5-2x slowdown when guaranteeing correctness with the mutex.

n_threads=8 is the outlier, as it performs roughly the same on both the mutex and no-mutex implementation.

Since there is no mutex placed on the retrieval process, the time to retrieve is largely the same across all thread counts.
