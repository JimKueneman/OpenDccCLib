# OpenDccCLib

A portable C library for generating and decoding NMRA DCC (Digital Command Control) packets on any processor -- microcontrollers, desktop PCs, or anything in between.

DCC is the NMRA standard for digital communication between command stations and decoders on model railroads. OpenDccCLib implements DCC packet construction, validation, and decoding in plain C -- no dynamic memory, no OS, no external dependencies -- so it can run on anything with a C compiler.

## Features

- DCC packet encoding and decoding in C
- No dynamic memory allocation -- all buffers are statically defined at compile time
- No OS or RTOS required
- Dependency injection pattern -- wire in your own hardware drivers

## Repository layout

```
src/                              main library source
  dcc/                            DCC protocol engine -- packet construction and decoding
  drivers/
    canbus/                       CAN bus driver layer
  utilities/                      endian and string helpers
  utilities_pc/                   PC-specific utilities (threading)
  test/                           unit test entry point

test/                             unit test build infrastructure
  user_config/                    test configurations

templates/                        user config templates for new projects
  typical/                        standard node dcc_user_config.h

documentation/                    Doxygen API reference, style guides
```

## Building Tests

See [test/README.md](test/README.md) for instructions.

## License

BSD 2-Clause. See individual source file headers for the full license text.

## Author

Jim Kueneman
