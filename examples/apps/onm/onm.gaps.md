# Gap Analysis: ijkLang Capabilities vs. onm Specification

This document provides a concise gap analysis comparing the current capabilities of the **ijkLang (`ijkL`)** programming language and its virtual machine (`ijkl-m`) against the requirements described in [onm.spec.md](onm.spec.md).

---

## Summary of Identified Gaps

To implement the **onm (Object Notation Management)** educational application in ijkLang, the language and runtime currently lack features in four primary categories:

1. **File System and Disk Persistence (File I/O)**
2. **Interactive Terminal Input (Console Line Reading)**
3. **String Manipulation and Dynamic Text Operations**
4. **Dynamic Collections and Variable-Length Buffers**

---

## 1. File System and Disk Persistence (File I/O)

The `onm` specification requires managing an `onm.data` file, verifying file existence at startup, reading the entire file on every command, performing atomic writes via temporary files, and deleting temporary files.

* **Missing File I/O Primitives**: Neither the compiler nor the VM (`ijkl-m`) currently offers instructions or standard library routines to interact with the file system (`file_open`, `file_read`, `file_write`, `file_close`, or `read_all_text`).
* **Missing File Existence Check**: No mechanism exists to query whether `onm.data` already exists in the current working directory (`file_exists`).
* **Missing Atomic Rename / Replacement**: No primitive exists to atomically rename or replace a file (`file_rename`).
* **Missing File Deletion**: No primitive exists to physically delete files (`file_delete` / `remove`) for cleaning up temporary files.

---

## 2. Interactive Terminal Input (Console Line Reading)

The application requires an interactive read-eval-print loop with custom prompts (`onm:/>`), reading full command lines and arguments containing spaces enclosed in double quotes (e.g., `add node "personal information"`).

* **Missing Line-Reading Primitive (`readLine` / `readln`)**: The current `read()` / `leia()` statement is mapped exclusively to port `0x0003` (numeric integer `scanf("%lld")`), port `0x0002` (`scanf("%lf")`), or port `0x0000` (single raw character `getchar()`). There is no built-in primitive or I/O port to read a full string or line of text from standard input (`stdin`).
* **Missing Interactive Prompt Without Newline**: Interactive prompts require emitting text without a newline (`write` / `escreva`) followed by reading a line; while `write` is supported, input capture is limited to numbers.

---

## 3. String Manipulation and Dynamic Text Operations

`onm` requires intensive string processing: parsing user commands, tokenizing arguments with quotes, path manipulation (e.g., navigating and splitting `/course/teacher`), JSON syntax validation, and formatted JSON serialization with indentation.

* **Opaque / Pointer-Only String Semantics**: In `ijkLang`, the `string` (`texto`) primitive is currently an opaque pointer to string literals stored in memory:
  * **Content Equality (`==` / `!=`)**: The equality operator on strings compares raw memory pointer addresses, not string byte contents (`strcmp`). Consequently, dynamic strings or user inputs cannot be compared against command literals like `"open"`, `"add"`, or `"exit"`.
  * **String Concatenation (`+`)**: While the semantic analyzer allows `string + string`, the code generator and VM do not allocate a new concatenated string buffer on the heap at runtime.
* **Missing String Inspection & Slicing**:
  * No string length query (`length(str)` / `str.len`).
  * No character indexing (`str[i]`).
  * No substring extraction (`substring(str, start, length)`).
  * No search/trim functions (`indexOf`, `contains`, `trim`, `split`).
* **Missing Value-to-String and String-to-Value Conversions**: No routines to convert numbers or integers to formatted strings (`int_to_string`) or parse strings into values.

---

## 4. Dynamic Collections and Variable-Length Buffers

`onm` represents an arbitrary in-memory tree of nodes and keys with dynamic additions, renames, updates, and deletions of child elements at any depth.

* **Fixed-Size Arrays Only**: ijkLang arrays (`array[N] of Type`) are strictly fixed-capacity at compile time (`size_t fixed_element_count`). There are no dynamic arrays, slices, or resizable vectors.
* **No Dynamic Collections Library**: There is no standard collection library (e.g., dynamic lists, associative maps, hash tables, or tree abstractions). While recursive pointer structures (`struct Node { string key; Node* next; Node* child; }`) are syntactically permitted via `alloc()` and `free()`, the inability to store dynamic strings or compare string values prevents building dynamic property maps.
* **No Variable-Length Byte/Char Buffer Allocation**: The `alloc(Type)` expression allocates memory for fixed-size type descriptors; allocating an arbitrary variable-length string buffer (`alloc(char, size)`) is not natively supported as a high-level language construct.

---

## 5. Architectural Recommendations to Bridge the Gaps

To enable the implementation of `onm` in `ijkLang`, the following incremental additions would be needed:

1. **Standard I/O Extensions in VM**:
   - Add a VM I/O port or system routine for reading a line of text (`readLine(string& buffer)` or `readln(str)`).
2. **File System Built-ins / Standard Module (`io.file` or `System.IO`)**:
   - Introduce standard procedures/functions for basic file operations: `fileExists(path): bool`, `readFile(path): string`, `writeFile(path, content): bool`, `renameFile(src, dst): bool`, and `deleteFile(path): bool`.
3. **Core String Library (`std.string` or native operators)**:
   - Implement value-based string comparison (`string == string`), dynamic concatenation, `stringLength(s): int`, `charAt(s, index): char`, and `substring(s, start, count): string`.
4. **Dynamic Data Structures**:
   - Provide a dynamic list or map type, or support flexible character arrays / byte buffers to allow manual construction of linked lists and JSON trees.
