This document outlines the coding style used in the OpenDccCLib library.

# Use of types:

This library uses typedef liberally to allow better type checking and does not encourage the use of type casting unless absolutely necessary.

# File Names and Folders:

Filenames are all lower case with underscores between words and are descriptive.  All library source files are prefixed with `dcc_`.  The file structure is as follows:

~~~
 -src
    |
    | - dcc          // All library source files live here in a flat structure.
         |           // Each module is a .h/.c pair.  Test files are .cxx.
         |
         | - dcc_types.h / dcc_defines.h      // Core types and protocol constants
         | - dcc_config.h / dcc_config.c       // Wiring module (dependency injection)
         | - dcc_<module_name>.h / .c          // One pair per module
         | - dcc_<module_name>_Test.cxx        // Google Test file per module
~~~

# C Coding Style:

**Indentation**

Indentation of lines of code shall be 4 spaces, no tabs allowed, unless otherwise stated.   If the spec and examples are in conflict the value here shall be used.

**Doxygen Comment Indentation**

Doxygen blocks are always indented 4 spaces more than the code they document.  Find the first non-space character of the line being documented, count how many spaces precede it (call that N), and place the opening `/**` at column N + 4.  For full rules and examples see `documentation/Doxygen_StyleGuide.md`.

**Line Length**

Do not break a line prematurely if the content fits within 240 characters. Use the available width before wrapping.

Any function that has a large number or long identifier names shall be split up using these rules, IF AND ONLY IF the overall length is greater than the above rule for character length:

1) If the line for a statement is less or equal to 240 the statement shall be left on one line
2) If the line is longer than 240 characters (converting tabs equivalent number of space in the count) do the following
  -Break the parameter list up with a parameter on each line (a parameter may be the return from another function recursively in which the same rule applies).  The indentation shall be as follows:

~~~

DccPacketEncoder_speed_128(
            packet,
            address,
            address_type,
            speed,
            direction);

~~~

- The indentation shall be 3 times the normal indentation value.  If the resulting line is still longer than max limit above allow it.

3) For logical operator lines the same applies of the max line length above.  The line shall be broken up into multiple lines at the operator as in the example:

~~~

return (
            ((uint64_t) (*buffer)[0 + index] << 40) |
            ((uint64_t) (*buffer)[1 + index] << 32) |
            ((uint64_t) (*buffer)[2 + index] << 24) |
            ((uint64_t) (*buffer)[3 + index] << 16) |
            ((uint64_t) (*buffer)[4 + index] << 8) |
            ((uint64_t) (*buffer)[5 + index])
            );

~~~

4) Pointers "*" shall be placed as shown in the example:

~~~

extern bool DccPacketEncoder_speed_128(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t speed, bool direction);

~~~


## Header Files:

Header file public functions use the module name in PascalCaseStyle followed by an underscore and a descriptive action name in lower case with words separated by underscores.  For instance in the `dcc_scheduler.h` file to initialize the scheduler you would call:

~~~
  extern void DccScheduler_initialize(dcc_scheduler_context_t *context, const interface_dcc_scheduler_t *interface);
~~~

Guards are named with 2 leading and trailing underscores with the module name (minus the `dcc_` file prefix) in capitals with underscores between the words.

All source files are in the **/src/dcc** folder, so the guard for `dcc_scheduler.h` would be:

~~~
#ifndef __DCC_SCHEDULER__
#define __DCC_SCHEDULER__

#include "dcc_types.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_COMMAND_STATION

#ifdef __cplusplus
  extern "C" {
#endif /* __cplusplus */


    extern void DccScheduler_initialize(dcc_scheduler_context_t *context, const interface_dcc_scheduler_t *interface);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_COMMAND_STATION */

#endif /* __DCC_SCHEDULER__ */

~~~

Note: The header guard wraps the entire file.  The `DCC_COMPILE_*` feature flag guard wraps the actual declarations (see Feature Flag Guards below).

## Type Definitions:

Any constant that can not be changed is written in capitals with underscores between the words.  This includes `#defines` that define a constant.

Library constants use the `DCC_` prefix:

~~~

#define DCC_PACKET_MAX_BYTES 6

~~~

User-configurable constants use the `USER_DEFINED_DCC_` prefix:

~~~

#define USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT 16

~~~

All #defines with mathematical operators shall be enclosed with parenthesis to ensure the math is done in the preprocessing stage.

~~~

#define DCC_SOME_COMPUTED_VALUE (1 + 5 * 8)

~~~

Enumeration type definitions use a *trailing _enum* to signify they are enumeration types.  Enum values use the `DCC_` prefix in all capitals.

~~~
typedef enum {

    DCC_ADDRESS_SHORT,
    DCC_ADDRESS_LONG,
    DCC_ADDRESS_BROADCAST,
    DCC_ADDRESS_IDLE

} dcc_address_type_enum;

~~~

Struct type definitions use a *trailing _t* to signify they are types.

~~~

typedef struct {

    uint8_t data[DCC_PACKET_MAX_BYTES];
    uint8_t byte_count;
    uint8_t preamble_bits;
    uint8_t repeat_count;

} dcc_packet_t;

~~~

Context structs hold per-instance state and use a *trailing _context_t*:

~~~

typedef struct {

    const interface_dcc_scheduler_t *interface;
    dcc_scheduler_slot_t slots[USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT];
    uint8_t refresh_index;
    volatile bool packet_complete_flag;
    bool first_packet_sent;

} dcc_scheduler_context_t;

~~~

Interface structs hold function pointers for dependency injection and use the prefix `interface_dcc_` with a *trailing _t*:

~~~

typedef struct {

    void (*load_packet)(const dcc_packet_t *packet);
    bool (*is_encoder_idle)(void);
    void (*on_packet_sent)(const dcc_packet_t *packet);
    void (*on_idle)(void);

} interface_dcc_scheduler_t;

~~~

Callback function pointer types use a *trailing _callback_t*:

~~~

typedef void (*dcc_service_mode_step_callback_t)(dcc_service_mode_result_t result);

~~~

## Feature Flag Guards:

Every module's declarations and definitions must be wrapped in the appropriate `DCC_COMPILE_*` ifdef guard.  The feature flag sits inside the header guard and around the `extern "C"` block.

Primary flags (roles):
- `DCC_COMPILE_COMMAND_STATION` — command station (DCC output) functionality
- `DCC_COMPILE_DECODER` — decoder (DCC input) functionality
- `DCC_COMPILE_ACCESSORY_DECODER` — accessory decoder functionality

Service mode flags (require `DCC_COMPILE_COMMAND_STATION`):
- `DCC_COMPILE_SERVICE_MODE_DIRECT`
- `DCC_COMPILE_SERVICE_MODE_PAGED`
- `DCC_COMPILE_SERVICE_MODE_REGISTER`
- `DCC_COMPILE_SERVICE_MODE_ADDRESS`

Feature flags (cross-cutting — require a role flag; the role decides which half compiles):
- `DCC_COMPILE_RAILCOM` — RailCom feedback. With `DCC_COMPILE_COMMAND_STATION` it compiles the
  cutout generator and the receive/decode path; with `DCC_COMPILE_DECODER` or
  `DCC_COMPILE_ACCESSORY_DECODER` it compiles the transmit encoder and datagram application layer.
  Guard form is the role-stripped compound: `#if defined(DCC_COMPILE_RAILCOM) && defined(<role>)`.
  A lone `DCC_COMPILE_RAILCOM` with no role is rejected with an `#error` in `dcc_types.h`.

All `USER_DEFINED_DCC_*` constants are validated at compile time in `dcc_types.h`.

### Core Type Headers Are Never Gated

The core type/constant headers `dcc_types.h` and `dcc_defines.h` define data types and protocol constants only.  Type, enum, struct, and constant `#define` *definitions* in these files are NEVER wrapped in a `DCC_COMPILE_*` role or feature guard — every type is always visible regardless of which role the build selects.  A type emits no code, so gating it saves nothing; it only couples the type header to the role flags and forces every consumer to mirror the guard.

The ONLY `#if`/`#ifdef` permitted in `dcc_types.h` are compile-time *validation* assertions: the `USER_DEFINED_*` "must be defined / must be in range" `#error` checks (which stay role-conditional, because a role's constants only exist in that role's build) and the lone-`DCC_COMPILE_RAILCOM` `#error`.  These guard configuration correctness, not type visibility.

This exception applies only to the core type/constant headers.  Module `.h`/`.c` files still wrap their declarations and definitions in the appropriate role guard as described above.

## Interface Structs and Cross-Module Calls:

Never `#include` a module header to call its functions directly in another module's .c or .h files.  All cross-module calls must go through interface structs (function pointers).  A direct include that pulls in a module's implementation is a bug.

The only file that includes all module headers is `dcc_config.c` — the wiring module.  It builds the interface structs, allocates context structs, and passes them to each module's `_initialize()` function.

~~~

// In dcc_config.c — the ONLY place that wires modules together:

static dcc_scheduler_context_t _main_scheduler_context;

static const interface_dcc_scheduler_t _main_scheduler_interface = {

    .load_packet     = _main_load_packet,
    .is_encoder_idle = _main_is_encoder_idle,
    .on_packet_sent  = _main_on_packet_sent,
    .on_idle         = NULL

};

// During initialization:
DccScheduler_initialize(&_main_scheduler_context, &_main_scheduler_interface);

~~~

## Static Memory Allocation:

No dynamic allocation (malloc/free) is used anywhere in the library.  All memory is statically allocated at compile time.  Buffer sizes and slot counts are controlled by `USER_DEFINED_DCC_*` constants defined in `dcc_user_config.h`.

## For loop and If statements and Functions:

All for loops and If statements will use full brackets regardless of the number of statements in the following format examples, except for single line `return`, `continue`, or `break` statements as in the following examples.  There will be blank space inserted as shown.

Insert one blank line after opening braces { and one blank line before closing braces } in all code blocks.

### For Loops
~~~

if (context->interface->is_encoder_idle()) {

    return;

}

if (!packet) {

    continue;

}

if (found) {

    break;

}

for (int slot_index = 0; slot_index < USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT; slot_index++) {

    context->slots[slot_index].active = false;
    statements....

}

if (context->refresh_index == 0) {

    statements....
    return NULL;

} else {

    statements......

}
~~~

### Else/If will use the following style

~~~
if (context->refresh_index == 0) {

    statements......
    return NULL;

 } else if (context->refresh_index == 100) {

    statements......
    statements......

 } else {

    statements......
    statements......

}
~~~

### Switch statements

~~~

switch (priority) {

    case DCC_PRIORITY_ESTOP:

        statements......
        statements......

        break;

    case DCC_PRIORITY_SPEED:

        statements......
        statements......

        break;

    case DCC_PRIORITY_FUNCTION:

        statements......
        statements......

        break;

    default:

        statements......
        statements......

        break;

}

~~~

### Stack-Allocated Struct Initialization

Any struct declared on the stack must be zero-initialized using `memset()` immediately after declaration.  Never use `= {0}` for struct initialization.  C does not zero local variables automatically; uninitialized fields will contain stack garbage and can cause undefined behavior.

~~~

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));

~~~

During style review, verify that no stack-allocated struct is declared without a corresponding `memset()` call.

### Functions and Variable Formats and Naming

All variable and function names SHALL not be named short historical C name such as i, x, src.  The names shall contain a context for what the variable function does and be limited to 40 characters.

~~~

static void _generate_idle_packet(dcc_packet_t *packet) {

    statements......
    statements......

}

~~~

Module function naming

Within a module the following function and variable naming convention is used.

Any function/variable that is accessed outside the module uses the module name in PascalCaseStyle followed by an underscore and a descriptive action in lower case with underscores between words.  The following is in a module named:

> dcc_scheduler.h

~~~

extern void DccScheduler_initialize(dcc_scheduler_context_t *context, const interface_dcc_scheduler_t *interface);
extern void DccScheduler_run(dcc_scheduler_context_t *context);
extern bool DccScheduler_insert(dcc_scheduler_context_t *context, const dcc_packet_t *packet, dcc_address_t address, dcc_tag_enum tag, dcc_priority_enum priority, bool auto_refresh);

~~~

Standard lifecycle functions: `Dcc<Module>_initialize()`, `Dcc<Module>_run()`

ISR entry points use an `_isr` suffix: `DccConfig_main_track_isr()`

The public config API uses: `DccConfig_<action>()`

### Maximum Nesting Depth

Nested `{ }` blocks shall not be deeper than 3 levels, counting the function body as level 1.  If a function would exceed 3 levels of nesting, refactor the inner logic into a sub-function to keep each function within the limit.

~~~

// Level 1: function body
static void _process_slots(dcc_scheduler_context_t *context) {

    // Level 2: for loop
    for (int slot_index = 0; slot_index < USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT; slot_index++) {

        // Level 3: maximum allowed depth
        if (context->slots[slot_index].active) {

            _handle_active_slot(context, slot_index);  // further logic goes in a sub-function

        }

    }

}

~~~

Variables used as function parameters or local variables are lower case and separated by underscores.  If the function or variable is local to the module then it also begins with an underscore.  All internally used variables and functions will be defined as *static*.

~~~

static dcc_scheduler_context_t _main_scheduler_context;

static void _select_next_packet(dcc_scheduler_context_t *context) {


}

~~~
