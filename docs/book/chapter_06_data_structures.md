# Chapter 6: Slices, Strings, and Maps

## Part 1: Strings

### The Million-Dollar Question

How long is this string?

```
"Hello, Dreamcast!"
```

In C, you have to **count**:

```c
char *msg = "Hello, Dreamcast!";
int len = 0;
while (msg[len] != '\0') {  // Keep going until null byte
    len++;
}

// H  e  l  l  o  ,     D  r  e  a  m  c  a  s  t  !  \0
// 0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17
// len is now 17... but we checked 18 characters!
```

C strings end with a special "null byte" (`\0`). To find the length, you walk through every character until you hit it. For a 10,000-character string, that's 10,000 checks.

Go strings are smarter. They **remember their length**:

```
┌────────────────┐
│  str: ─────────────────▶ h │ e │ l │ l │ o │
│  len: 5        │
└────────────────┘
```

In libgodc, this is an 8-byte structure (on 32-bit Dreamcast):

```c
// From runtime/runtime.h, see GoString C struct
typedef struct {
    const uint8_t *str;  // 4 bytes: pointer to character data
    intptr_t len;        // 4 bytes: length in bytes
} GoString;
```

Unlike C strings (null-terminated), Go strings store their length explicitly. This means:
- **O(1) length lookup** just read the `len` field
- **Can contain null bytes** no special terminator
- **Bounds checked** we know exactly where the string ends

### String Allocation

Strings are **immutable**. Every concatenation allocates new memory:

```go
s := "foo" + "bar"  // Allocates 6 bytes, copies both strings
```

Repeated concatenation in a loop is O(n²), where each iteration copies all previous data. This is a common Go performance pitfall; see [Effective Go](https://go.dev/doc/effective_go#printing) for solutions.

### The tmpBuf Optimization

Here's a secret: libgodc cheats for short strings.

When you concatenate strings that total ≤32 bytes, we use a **stack buffer** instead of allocating from the heap:

```
"a" + "b" = "ab"

Stack (fast):  ┌────────────────────────────────┐
               │ a │ b │   │   │ ... │   │   │  │  32 bytes
               └────────────────────────────────┘

No GC allocation needed!
```

This happens automatically. You don't have to do anything—the compiler passes a stack buffer to the runtime, and we use it when we can.

---

## Part 2: Slices

### The Three-Part Header

A slice is not just a pointer. It's a **header** (that means struct) with three fields:

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   Slice: []int with values [10, 20, 30]                     │
│                                                             │
│   ┌────────────────┐        ┌─────┬─────┬─────┬─────┬─────┐ │
│   │  array: ───────────────▶│ 10  │ 20  │ 30  │  ?  │  ?  │ │
│   │  len:   3      │        └─────┴─────┴─────┴─────┴─────┘ │
│   │  cap:   5      │             ▲           ▲              │
│   └────────────────┘          length      capacity          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

- **array** :: Pointer to the underlying data
- **len** :: How many elements are currently in use
- **cap**:: How many elements *could* fit before reallocation

Think of it like a notebook. You have 100 pages (capacity), but you've only written on 30 (length).

### The Magic of Slicing

Here's the trick that makes Go slices amazing. When you "slice" a slice, **no data is copied**:

```go
a := []int{10, 20, 30, 40, 50}
b := a[1:4]  // b is [20, 30, 40]
```

What actually happens:

```
Underlying array:
┌─────┬─────┬─────┬─────┬─────┐
│ 10  │ 20  │ 30  │ 40  │ 50  │
└─────┴─────┴─────┴─────┴─────┘
  ▲     ▲
  │     │
  │     └── b.array points here
  │         b.len = 3
  │         b.cap = 4
  │
  └── a.array points here
      a.len = 5
      a.cap = 5
```

Both `a` and `b` point to the **same memory**. Slicing is O(1) — just create a new 12-byte header.

### The Sharing Trap

But wait. If they share memory...

```go
a := []int{10, 20, 30, 40, 50}
b := a[1:4]

b[0] = 999  // What happens to a?
```

```
After b[0] = 999:
┌─────┬─────┬─────┬─────┬─────┐
│ 10  │ 999 │ 30  │ 40  │ 50  │
└─────┴─────┴─────┴─────┴─────┘
  ▲     ▲
  │     │
  a     b

a is now [10, 999, 30, 40, 50]!
```

**Both slices see the change!** This is usually a bug waiting to happen.

If you need independent data, use `copy`:

```go
b := make([]int, 3)
copy(b, a[1:4])  // b has its own data now
```

### How libgodc Implements `copy`

When you write `copy(dst, src)`, what actually happens?

```
Step 1: Figure out how many elements to copy
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   dst has room for 3       src has 5 elements               │
│   ┌───┬───┬───┐            ┌───┬───┬───┬───┬───┐            │
│   │   │   │   │            │ A │ B │ C │ D │ E │            │
│   └───┴───┴───┘            └───┴───┴───┴───┴───┘            │
│                                                             │
│   Copy min(3, 5) = 3 elements                               │
│                                                             │
└─────────────────────────────────────────────────────────────┘

Step 2: Calculate byte size
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   3 elements × 4 bytes each (int) = 12 bytes                │
│                                                             │
└─────────────────────────────────────────────────────────────┘

Step 3: copy the bytes safely (aka memmove in C)
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   src:  ████████████░░░░░░░░  (copy first 12 bytes)         │
│              │                                              │
│              ▼                                              │
│   dst:  ████████████                                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘

Step 4: Return 3 (number of elements copied)
```

Why `memmove` instead of `memcpy`? Because slices can overlap:

```go
s := []int{1, 2, 3, 4, 5}
copy(s[1:], s[:4])  // Shift elements right — overlapping!
```

`memmove` handles this safely. `memcpy` would corrupt the data.

### Growing Slices: The append Dance

What happens when you append beyond capacity?

```go
s := make([]int, 3, 4)  // len=3, cap=4
s = append(s, 10)       // len=4, cap=4 — fits!
s = append(s, 20)       // len=5, cap=??? — doesn't fit!
```

libgodc allocates a new, bigger array:

```
Before:
┌─────┬─────┬─────┬─────┐
│  0  │  0  │  0  │ 10  │  cap=4, FULL
└─────┴─────┴─────┴─────┘

After append(s, 20):
┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
│  0  │  0  │  0  │ 10  │ 20  │     │     │     │  cap=8, NEW ARRAY
└─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘

Old array becomes garbage (GC will clean it up).
```

### libgodc's Growth Strategy

Standard Go doubles capacity for small slices and grows by 25% for large ones. But Dreamcast only has 16MB RAM, so **libgodc is more conservative** by design:

```
┌─────────────────────────────────────────────────────────────┐
│   libgodc growth algorithm (runtime_growslice)              │
│                                                             │
│   if capacity < 64:                                         │
│       new_cap = capacity × 2      ← Double (same as std Go) │
│   else:                                                     │
│       new_cap = capacity × 1.125  ← Only 12.5% growth!      │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

|        Slice size     | Standard Go | libgodc |
|-----------------------|-------------|---------|
| Small (< 64 elements) | Double      | Double  |
| Large (≥ 64 elements) | +25%        | +12.5%  |

Why the difference? On a 16MB system, aggressive doubling wastes precious memory. A 10,000-element slice growing by 25% allocates 2,500 extra slots. At 12.5%, that's only 1,250, so half the waste.

**Pro tip:** If you know how big you'll need, then pre-allocate!

```go
// Bad: many reallocations
enemies := []Enemy{}
for i := 0; i < 100; i++ {
    enemies = append(enemies, loadEnemy(i))
}

// Good: one allocation
enemies := make([]Enemy, 0, 100)
for i := 0; i < 100; i++ {
    enemies = append(enemies, loadEnemy(i))
}
```

---

## Part 3: Maps

### The Problem: Finding Things Fast

Suppose you're building an item shop for your game. You have a price list:

```go
type Item struct {
    Name  string
    Price int
}

items := []Item{
    {"Potion", 50},
    {"Sword", 300},
    {"Shield", 250},
    {"Bow", 200},
    // ... 100 more items
}
```

A customer asks: "How much is the Bow?"

You have to search through every item:

```go
for _, item := range items {
    if item.Name == "Bow" {
        return item.Price
    }
}
```

If the item list has 100 items, you might check up to 100 items. That's O(n) time.

Now imagine you have a friend named Maggie who has memorized every item and its price. You ask "How much is the Bow?" and she instantly says "200 gold!"

Maggie gives you the answer in O(1) time — constant time. It doesn't matter if there are 10 items or 10,000. She just *knows*.

**How do you get a "Maggie"?**

You use a hash table. In Go, that's a `map`.

### Building Your Own Maggie

A hash table combines two things:
1. A **hash function** that turns keys into numbers
2. An **array** to store the values

Let's build one step by step. Start with an empty array of 5 slots:

```
┌───────┬───────┬───────┬───────┬───────┐
│   0   │   1   │   2   │   3   │   4   │
├───────┼───────┼───────┼───────┼───────┤
│       │       │       │       │       │
└───────┴───────┴───────┴───────┴───────┘
```

Now we need a hash function. A hash function takes a string and returns a number. Here's the important part:

- It must be **consistent**: "Potion" always returns the same number.
- It should **spread things out**: different strings should (usually) give different numbers.

Let's add the price of a Potion. We feed "Potion" into the hash function:

```
hash("Potion") → 7392
7392 % 5 = 2  ← slot 2!
```

We store the price (50) at index 2:

```
┌───────┬───────┬───────┬───────┬───────┐
│   0   │   1   │   2   │   3   │   4   │
├───────┼───────┼───────┼───────┼───────┤
│       │       │ 50    │       │       │
│       │       │Potion │       │       │
└───────┴───────┴───────┴───────┴───────┘
```

Now add the Sword (300 gold):

```
hash("Sword") → 4281
4281 % 5 = 1  ← slot 1!
```

```
┌───────┬───────┬───────┬───────┬───────┐
│   0   │   1   │   2   │   3   │   4   │
├───────┼───────┼───────┼───────┼───────┤
│       │ 300   │ 50    │       │       │
│       │ Sword │Potion │       │       │
└───────┴───────┴───────┴───────┴───────┘
```

Add the Shield and Bow:

```
hash("Shield") % 5 = 0
hash("Bow") % 5 = 4
```

```
┌───────┬───────┬───────┬───────┬───────┐
│   0   │   1   │   2   │   3   │   4   │
├───────┼───────┼───────┼───────┼───────┤
│ 250   │ 300   │ 50    │       │ 200   │
│Shield │ Sword │Potion │       │ Bow   │
└───────┴───────┴───────┴───────┴───────┘
```

Now when someone asks "How much is the Bow?":

1. `hash("Bow") % 5 = 4`
2. Look at slot 4
3. It's 200 gold!

**No searching!** The hash function tells you exactly where to look. This is O(1) — constant time.

You just built a "Maggie"!

### Collisions: When Two Keys Want the Same Slot

Here's a problem. What if two items hash to the same slot?

```
hash("Potion") % 5 = 2
hash("Scroll") % 5 = 2  ← Same slot!
```

Oh no! Potions are already in slot 2. If we put Scrolls there, we'll overwrite Potions!

This is called a **collision**. There are different ways to handle it. Go uses a simple approach: store both items in the same slot using a small list.

```
┌───────┬───────┬────────────────────┬───────┬───────┐
│   0   │   1   │         2          │   3   │   4   │
├───────┼───────┼────────────────────┼───────┼───────┤
│ 250   │ 300   │ Potion→50          │       │ 200   │
│Shield │ Sword │ Scroll→75          │       │ Bow   │
└───────┴───────┴────────────────────┴───────┴───────┘
```

Now when you look up "Scroll":
1. `hash("Scroll") % 5 = 2`
2. Look at slot 2
3. Check if "Potion" matches — no
4. Check if "Scroll" matches — yes! Return 75.

It takes a tiny bit longer, but it works.

### The Worst Case: Everyone in One Slot

What if you're really unlucky and every item hashes to the same slot?

```
┌───────┬───────┬──────────────────────────┬───────┬───────┐
│   0   │   1   │            2             │   3   │   4   │
├───────┼───────┼──────────────────────────┼───────┼───────┤
│       │       │ Potion→50                │       │       │
│       │       │ Sword→300                │       │       │
│       │       │ Shield→250               │       │       │
│       │       │ Bow→200                  │       │       │
│       │       │ Scroll→75                │       │       │
└───────┴───────┴──────────────────────────┴───────┴───────┘
```

Now looking up "Scroll" requires checking 5 items. That's just as slow as a regular list!

This is the **worst case**: O(n) instead of O(1).

Two things prevent this:
1. **Good hash functions** spread keys evenly
2. **Resizing** — when the table gets too full, Go makes it bigger

### The Tophash Optimization

Each bucket stores a "tophash" — the top 8 bits of the hash — for quick rejection:

```
Bucket 2:
┌─────────────────────────────────────────────────┐
│ tophash: [a3] [7f] [  ] [  ] [  ] [  ] [  ] [  ]│
│ keys:    [Potion] [Scroll] [  ] [  ] [  ] [  ]  │
│ values:  [  50  ] [  75  ] [  ] [  ] [  ] [  ]  │
└─────────────────────────────────────────────────┘
```

When looking up "Sword" (tophash = 0xb2):
1. Check if 0xb2 == 0xa3? No. Skip.
2. Check if 0xb2 == 0x7f? No. Skip.
3. Not found!

We didn't even compare the full strings. The tophash check is super fast.

### Performance Comparison

```
┌─────────────────────────────────────────────────────────────┐
│   Hash Table vs Array: Searching 100 elements               │
│                                                             │
│   Array (linear search):                                    │
│   ┌───────────────────────────────────────────────────────┐ │
│   │ Average: check 50 elements                            │ │
│   │ Worst:   check 100 elements                           │ │
│   │ Time:    O(n)                                         │ │
│   └───────────────────────────────────────────────────────┘ │
│                                                             │
│   Hash Table (map):                                         │
│   ┌───────────────────────────────────────────────────────┐ │
│   │ Average: check 1 element                              │ │
│   │ Worst:   check all elements (very rare!)              │ │
│   │ Time:    O(1) average                                 │ │
│   └───────────────────────────────────────────────────────┘ │
│                                                             │
│   With 1,000,000 elements:                                  │
│   • Array: up to 1,000,000 checks                           │
│   • Map:   still just ~1 check!                             │
└─────────────────────────────────────────────────────────────┘
```

### How libgodc Implements Maps

libgodc's map implementation is tuned for the Dreamcast's SH-4 CPU and 16MB memory limit.

**The GoMap header (28 bytes):**

```
┌─────────────────────────────────────────────────────────────┐
│   GoMap Structure                                           │
│                                                             │
│   ┌──────────────┬──────────────────────────────────────┐   │
│   │ count        │ Number of entries                    │   │
│   │ flags + B    │ State flags + log2(bucket count)     │   │
│   │ hash0        │ Random seed (different per map!)     │   │
│   │ buckets ─────────▶ Current bucket array             │   │
│   │ oldbuckets ──────▶ Old buckets (during resize)      │   │
│   │ nevacuate    │ Resize progress counter              │   │
│   └──────────────┴──────────────────────────────────────┘   │
│                                                             │
│   Total: 28 bytes (compact for Dreamcast's limited RAM)     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**SH-4 optimized hashing:**

The hash function uses `wyhash`, a fast 32-bit algorithm that takes advantage of SH-4's `dmuls.l` instruction (32×32→64 multiply):

```
┌─────────────────────────────────────────────────────────────┐
│   Hash("Potion", seed=0x12345678)                           │
│                                                             │
│   Step 1: Mix 4 bytes at a time                             │
│           wymix32(h ^ "Poti", 0x9E3779B9)                   │
│                                                             │
│   Step 2: Handle remaining bytes                            │
│           wymix32(h ^ "on\0\0", 0x85EBCA6B)                 │
│                                                             │
│   Step 3: Final mix with length                             │
│           wymix32(h, 6)  →  0x7A3B2C1D                      │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**Dreamcast-specific limits:**

| Setting | libgodc | Standard Go |
|---------|---------|-------------|
| Max bucket shift | 15 (32K buckets) | ~24 (16M buckets) |
| Hash seed source | Dreamcast timer | OS random |
| Prefetch hint | SH-4 `pref @Rn` | Platform-specific |

**Lazy allocation for small maps:**

```go
items := make(map[string]int)  // No buckets yet!
items["key"] = 1               // NOW buckets are allocated
```

This saves memory when you create maps that might stay empty.

### The Nil Map Trap

This is the #1 map bug for Go beginners:

```go
var inventory map[string]int  // nil map

// Reading: works! Returns zero value.
count := inventory["sword"]  // count is 0

// Writing: PANIC!
inventory["sword"] = 1  // "assignment to entry in nil map"
```

A nil map is like a locked filing cabinet. You can look through the glass (read), but you can't put anything in (write).

**Always initialize:**

```go
inventory := make(map[string]int)
// or
inventory := map[string]int{}
```

### Map Iteration is Random

```go
scores := map[string]int{
    "Mario": 100,
    "Luigi": 85,
    "Peach": 95,
}

for name, score := range scores {
    println(name, score)
}
```

Run this twice. You might get:

```
Run 1:          Run 2:
Luigi 85        Peach 95
Peach 95        Mario 100
Mario 100       Luigi 85
```

This is **intentional**. Go randomizes iteration order to prevent you from depending on it. If you need sorted keys, sort them yourself.

---

## Choosing the Right Tool

```
┌─────────────────────────────────────────────────────────────┐
│   DECISION TREE: What Data Structure Should I Use?          │
│                                                             │
│   Need to look up by name/key?                              │
│           │                                                 │
│           ├── YES → Use a map (O(1) lookup!)                │
│           │                                                 │
│           └── NO → Is the data ordered/sequential?          │
│                       │                                     │
│                       ├── YES → Use a slice                 │
│                       │                                     │
│                       └── NO → Still probably use a slice   │
│                                (maps have memory overhead)  │
│                                                             │
│   Is it text? → Use a string (immutable)                    │
│   Need to build text? → Use []byte, convert at the end      │
└─────────────────────────────────────────────────────────────┘
```

### Summary Table

| Operation | String | Slice | Map |
|-----------|--------|-------|-----|
| Get length | O(1) | O(1) | O(1) |
| Access by index | O(1) | O(1) | — |
| Access by key | — | — | O(1) avg |
| Append | N/A | O(1)* | O(1) avg |
| Concatenate | O(n) | O(n) | — |

\* Amortized — occasional reallocations

### Memory Overhead

```
String header:  8 bytes  (pointer + length)
Slice header:  12 bytes  (pointer + length + capacity)
Map header:    28 bytes  (+ bucket overhead per entry)
```

Maps have the most overhead. For small, dense integer keys (0 to N), a slice is often better:

```go
// If enemy IDs are 0-999, use a slice!
enemies := make([]*Enemy, 1000)
enemies[42] = &orc  // O(1), less memory than map
```

---

## Real Benchmark Results

We ran these benchmarks on actual Dreamcast hardware. The numbers don't lie!

### Map vs Slice: The "Maggie" Effect

Looking up an item by ID, searching near the end of the collection:

| Elements | Slice (linear search) | Map lookup | Map is... |
|----------|----------------------|------------|-----------|
| 100 | 17 μs | 1.3 μs | **13× faster** |
| 500 | 92 μs | 0.9 μs | **97× faster** |
| 1,000 | 187 μs | 0.9 μs | **203× faster** |
| 2,000 | 443 μs | 1.2 μs | **376× faster** |

Notice how slice time grows linearly (O(n)) while map time stays constant (O(1)). With 2,000 enemies, map lookup is **376× faster**!

### String Concatenation: The Hidden Cost

Building a string character by character:

| Characters | `s += "x"` in loop | `append` to `[]byte` | Speedup |
|------------|-------------------|---------------------|---------|
| 50 | 122 μs | 23 μs | **5× faster** |
| 200 | 665 μs | 69 μs | **9× faster** |
| 500 | 2,725 μs | 161 μs | **16× faster** |
| 1,000 | 8,973 μs | 314 μs | **28× faster** |

The loop method is O(n²) — time explodes as strings get longer. For 1,000 characters, pre-allocation is **28× faster**!

### Slice Pre-allocation: One Allocation vs Many

Appending items to a slice:

| Items | Growing `[]int{}` | Pre-alloc `make(0,n)` | Time saved |
|-------|------------------|----------------------|------------|
| 50 | 35 μs | 24 μs | 32% faster |
| 100 | 76 μs | 41 μs | 46% faster |
| 200 | 178 μs | 76 μs | 57% faster |

Pre-allocation eliminates the repeated reallocations as the slice grows.

---

The right data structure is like having the right superpower. A map turns an O(n) search into O(1). That's not just faster... it's magic.
