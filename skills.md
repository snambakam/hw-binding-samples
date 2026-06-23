# skills.md  
## AI Agent Coding & Repository Specification  
### Version 1.0 — Formal, Strict, AI‑Optimized

---

## 0. Agent Contract

This document defines the **mandatory rules** for all AI agents generating or modifying code in this repository.

- **MUST**, **MUST NOT**, **SHOULD**, and **MAY** follow RFC 2119 semantics.  
- All examples are **normative**, not illustrative.  
- If rules conflict, the **more restrictive** rule wins.  
- If uncertain, the agent MUST choose the option that:
  - Preserves existing structure  
  - Avoids introducing new patterns  
  - Follows this specification exactly  

---

## 1. Repository Structure

### 1.1 Global Configuration Header

- The repository root contains a single global header:
  ```
  config.h
  ```
- It MUST be included using:
  ```c
  #include <config.h>
  ```
- It MAY have include guards.  
- It MUST NOT be treated as a module-local header.

### 1.2 Module Definition

A “module” consists of:

- A public header in `include/`
- Internal implementation in `src/`
- A single public API surface

---

## 2. Module Layout

Each module MUST follow this structure:

```
mymodule/
    src/
        includes.h
        defines.h
        externs.h
        globals.c
        *.c
    include/
        mymodule.h
config.h
```

### 2.1 Public Header

- Only one public header per module: `mymodule.h`
- MUST have include guards
- External code MUST include it using:
  ```c
  #include <mymodule.h>
  ```

### 2.2 Internal Headers

- `includes.h`, `defines.h`, `externs.h` MUST NOT have include guards.
- Internal `.c` files MUST include exactly one header:
  ```c
  #include "includes.h"
  ```

---

## 3. Internal Header Rules

### 3.1 includes.h

This is the internal umbrella header.  
It MUST follow this exact include order:

1. `<config.h>`
2. System headers
3. Public module header `<mymodule.h>`
4. Internal headers
5. `"defines.h"`
6. `"externs.h"`

**Normative example:**

```c
// includes.h — no include guards

#include <config.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <mymodule.h>

// #include "internal_foo.h"

#include "defines.h"
#include "externs.h"
```

### 3.2 defines.h

- Internal-only macros  
- MUST NOT have include guards  
- Public macros MUST be in `mymodule.h`

### 3.3 externs.h

- MUST contain `extern` declarations for globals  
- MUST NOT define variables  
- MUST NOT have include guards  

---

## 4. Global Variables

### 4.1 Definitions

- All global variables MUST be defined in:
  ```
  globals.c
  ```

### 4.2 Declarations

- All globals MUST be declared in:
  ```
  externs.h
  ```

### 4.3 File-Local State

- `static` variables MAY be used  
- MUST NOT appear in headers  
- MUST follow placement rules in section 7  

---

## 5. Binary vs Library Modules

### 5.1 Binary Modules

- MUST contain `main.c`
- MUST call:
  - `<ModuleName>Initialize`
  - `<ModuleName>Finalize`

### 5.2 Library Modules

- MUST contain `libmain.c`
- MUST implement:
  - `<ModuleName>Initialize`
  - `<ModuleName>Finalize`

### 5.3 Naming

- Initialization/finalization functions MUST use PascalCase.

---

## 6. Naming Conventions

### 6.1 Functions

- MUST use PascalCase  
  Examples:
  - `MymoduleInitialize`
  - `NetworkSendPacket`

### 6.2 Variables

- MUST use camelCase  
  Examples:
  - `bufferSize`
  - `currentState`

### 6.3 Typedefs

- MUST be lowercase  
- MUST end with `_t`  
  Examples:
  - `config_t`
  - `state_t`

### 6.4 Defines

- MUST be UPPER_CASE  
  Examples:
  - `MAX_BUFFER_SIZE`

---

## 7. Source File Structure

Every `.c` file MUST follow this structure:

```
#include "includes.h"

// 1. Static variables
static int exampleStatic = 0;

// 2. Static function prototypes
static void
StaticHelper(
    int value
);

// 3. Public function definitions
void
MymoduleInitialize(
    void
) {
    ...
}

void
MymoduleDoWork(
    int param
) {
    ...
}

void
MymoduleFinalize(
    void
) {
    ...
}

// 4. Static function definitions
static void
StaticHelper(
    int value
) {
    ...
}
```

### 7.1 Include Placement

- The first non-comment line MUST be:
  ```c
  #include "includes.h"
  ```

### 7.2 Static Variables

- MUST appear immediately after includes  
- MUST precede all functions  

### 7.3 Static Prototypes

- MUST appear after static variables  
- MUST follow formatting rules in section 8  

### 7.4 Public Functions

- MUST appear before static function definitions  
- MUST be ordered in logical call order  

### 7.5 Static Functions

- MUST appear at the bottom of the file  

---

## 8. Function Formatting Rules

### 8.1 Single-Line Declarations

Allowed if they fit on one line:

```c
void MymoduleInitialize(void);
```

### 8.2 Multi-Line Declarations & Definitions

If not single-line, MUST follow this exact format:

```c
int
LongFunctionName(
    int firstArg,
    const char *secondArg,
    size_t thirdArg
) {
    ...
}
```

Rules:

- Return type on its own line  
- Function name + `(` on next line  
- Each argument on its own line  
- One tab indent for arguments  
- `) {` MUST be on the same line  
- No trailing spaces  

### 8.3 Static Functions

- Prototypes near top  
- Definitions at bottom  
- Same formatting rules  

---

## 9. Rust Integration (Optional)

### 9.1 Layout

```
mymodule/
    src/
        lib.rs
        mod.rs
    include/
        mymodule.h
```

### 9.2 FFI Rules

- All FFI functions MUST be declared in `mymodule.h`  
- MUST use PascalCase  
- MUST NOT expose internal Rust modules  

---

## 10. Forbidden Patterns

Agents MUST NOT introduce:

- Relative includes (`../include/...`)  
- External code including internal headers  
- Multiple public headers per module  
- Globals outside `globals.c`  
- Include guards in internal headers  
- Alternative formatting styles  
- Reordered `.c` file sections  

---

## 11. Agent Behavior Requirements

Agents MUST:

- Obey all rules  
- Preserve compliant patterns  
- Refactor non-compliant code  

Agents MUST NOT:

- Introduce new patterns  
- Simplify formatting  
- Deviate from examples  

Agents SHOULD:

- Use examples as templates  
- Maintain structural consistency  

---

# End of `skills.md`
