# Accelerating Declarative Query Compilation with AVX2 SIMD Vectorization and Zero-Copy Intermediate Representation

**Yuktha Sreenivasa Anjaneyulu**  
Department of Computer Science and Engineering  
School of Computer Science and Engineering (SCOPE)  
Vellore Institute of Technology (VIT), Vellore, Tamil Nadu, India  
*Registration Number: 23BDS0287*  

---

## Abstract
Delimited tabular data ingestion (e.g., Comma-Separated Values) constitutes the primary I/O and compute bottleneck in data science and analytics pipelines. General-purpose analytical libraries such as Python Pandas suffer from high memory amplification and interpreter startup overhead, while command-line utilities like `awk` lack static type verification. 

This paper introduces **QueryLang**, a compiled Domain-Specific Language (DSL) that integrates classical compiler front-end engineering with modern vector-accelerated systems execution. QueryLang couples a formal LALR(1) Context-Free Grammar parsed via Flex and Bison with an AVX2 SIMD zero-copy execution engine. The engine processes 32 bytes of ASCII text concurrently using 256-bit vector equality comparisons and hardware Trailing Zeros Count (`__builtin_ctz`) instructions, evaluating queries directly over in-memory `StringView` slices. Empirical evaluation across 100,000 to 1,000,000 row datasets demonstrates that QueryLang achieves up to **10.16x faster data ingestion** and **4.74x lower query latency** compared to Python Pandas 2.3.3, while slashing memory consumption by **57.4%**.

**Keywords:** Domain-Specific Languages, Compiler Design, Flex and Bison, SIMD Vectorization, AVX2 Intrinsics, Zero-Copy Execution, Query Optimization, Tabular Data Ingestion.

---

## I. Introduction
Comma-Separated Values (CSV) formats represent the standard medium for tabular data exchange. Despite their simplicity, parsing delimited text is computationally expensive because delimiter discovery traditionally requires sequential character-by-character scanning with unpredictable branches. 

To overcome these limitations, we design and implement **QueryLang**, combining:
1. An unambiguous declarative SQL-like grammar parsed via **Flex 2.6.4** and **Bison 3.8.2**.
2. Dynamic schema introspection and static semantic type checking.
3. Intermediate Code Generation following Three-Address Code (TAC) and Quadruple conventions.
4. An Intel AVX2 SIMD zero-copy execution engine that saturates CPU memory bandwidth.

---

## II. System Architecture
QueryLang Source Code (.ql) │ [Flex 2.6.4] -> Token Stream (Identifiers, Literals, Keywords) │ [Bison 3.8.2] -> LALR(1) Parsing Table │ [AST Generator] -> Tagged-Union Node Graph │ [Semantic Checker] <-> Dynamic Symbol Table (Inferred CSV Schema) │ [TAC Generator] -> Linear Three-Address Code & Quadruples │ [AVX2 SIMD Engine] -> 32-Byte Delimiter Bitmask & StringView Pool │ Tabular Result Output


---

## III. The AVX2 SIMD Zero-Copy Ingestion Algorithm

Traditional CSV readers scan text using character-by-character loops:
```c
while ((c = fgetc(fp)) != EOF) { if (c == ',') ... }

QueryLang utilizes 256-bit AVX2 vector registers (__m256i) to classify 32 bytes simultaneously:

Load 32 bytes: __m256i chunk = _mm256_loadu_si256((__m256i*)(buffer + offset));
Compare delimiters:
c


__m256i mask_comma = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8(','));
__m256i mask_nl    = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\n'));
__m256i mask_any   = _mm256_or_si256(mask_comma, mask_nl);
Extract bitmask: uint32_t bitmask = _mm256_movemask_epi8(mask_any);
If bitmask == 0, advance 32 bytes in 1 CPU cycle.
If bitmask != 0, jump directly to delimiters using hardware bit-scan instructions:
c


while (bitmask != 0) {
    int index = __builtin_ctz(bitmask);
    record_delimiter(offset + index);
    bitmask &= (bitmask - 1); // Clear lowest bit
}
Fields are recorded as StringView { const char *ptr; size_t len; } slices within the contiguous buffer, reducing heap allocations to 
O
(
1
)
O(1).
IV. Experimental Evaluation
Host Environment
CPU: 11th Gen Intel Core i7-1165G7 @ 2.80GHz
RAM: 16 GB DDR4
OS: Windows 11 64-bit
Compiler: GCC 13.2.0 (-O3 -mavx2)
Ingestion Latency and Throughput Comparison
Dataset Size	Python Pandas Parse (ms)	Scalar Parse (ms)	AVX2 SIMD Parse (ms)	Ingestion Speedup (SIMD vs Pandas)
100K Rows (3.54 MB)	94.74 ms	13.08 ms	9.32 ms	10.16x
250K Rows (9.00 MB)	163.01 ms	31.90 ms	21.94 ms	7.43x
1M Rows (36.31 MB)	543.27 ms	170.55 ms	93.56 ms	5.80x
Memory Consumption
On the 1,000,000-row dataset, Python Pandas required 122.4 MB of heap memory due to PyObject overhead and Series indexing. QueryLang consumed only 52.1 MB (a 57.4% reduction).

V. Related Work
Compiler Front-Ends: Aho et al. established classical LALR parsing principles implemented by Flex and Bison.
Data-Centric Query Compilation: Thomas Neumann (HyPer) proved that compiling query plans avoids instruction cache thrashing inherent in Volcano iterators.
Vectorized Parsing: Lemire and Langdale (simdjson) demonstrated that SIMD bitmask scanning achieves wire-speed parsing.
VI. Conclusion
QueryLang establishes that integrating classical compiler design principles (Flex, Bison, ASTs, Symbol Tables, TAC) with modern hardware-conscious vectorization techniques (AVX2 SIMD, zero-copy StringView representations) bridges the gap between academic compiler construction and production-grade data processing performance.

References
[1] A. V. Aho, M. S. Lam, R. Sethi, and J. D. Ullman, Compilers: Principles, Techniques, and Tools, 2nd ed. Addison-Wesley, 2006.
[2] T. Neumann, "Efficiently Compiling Efficient Query Plans for Modern Hardware," Proc. VLDB Endow., vol. 4, no. 9, pp. 539–550, 2011.
[3] G. Langdale and D. Lemire, "Parsing Gigabytes of JSON per Second," VLDB Journal, vol. 30, no. 2, pp. 255–274, 2021.
[4] F. Mühlbauer et al., "InstantLoading: Accelerating CSV Parsing with SIMD," in Proc. ICDE, 2013.


---
