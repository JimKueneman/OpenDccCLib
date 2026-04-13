# NMRA DCC Draft Specifications Reference

Index of draft documents downloaded from the [NMRA Documents Under Revision](https://www.nmra.org/documents-under-revision) page.
These are working drafts released for review/comment by the NMRA DCC Working Group and are subject to change.

Downloaded: 13-Apr-2026

---

## Table of Contents
1. [Decoder Interface (S-9.1.1.6)](#1-decoder-interface---e24-s-9116)
2. [DCC Extended Packet Formats (S-9.2.1)](#2-dcc-extended-packet-formats-s-921)
3. [Advanced Extended Packet Formats (S-9.2.1.1)](#3-advanced-extended-packet-formats-s-9211)
4. [Configuration Variables (S-9.2.2)](#4-configuration-variables-s-922)
5. [Bi-Directional Communication (S-9.3.2)](#5-bi-directional-communication-s-932)
6. [SUSI Bus Standards (S-9.4.x)](#6-susi-bus-standards-s-94x)

---

## 1. Decoder Interface - E24 (S-9.1.1.6)

| Document | File | Posted |
|----------|------|--------|
| S-9.1.1.6 E24 Decoder Interface (Draft B) | [s-9.1.1.6_e24_decoder_interface_draft.pdf](s-9.1.1.6_e24_decoder_interface_draft.pdf) | 15-Feb-2026 |
| TN-9.1.1.6 E24 Decoder Interface (Draft A) | [tn-9.1.1.6_e24_decoder_interface_draft.pdf](tn-9.1.1.6_e24_decoder_interface_draft.pdf) | 15-Feb-2026 |

Defines the E24 physical decoder interface connector standard. The Technical Note provides supplementary guidance.

---

## 2. DCC Extended Packet Formats (S-9.2.1)

| Document | File | Posted |
|----------|------|--------|
| S-9.2.1 DCC Extended Packet Formats | [s-9.2.1_dcc_extended_packet_formats_draft.pdf](s-9.2.1_dcc_extended_packet_formats_draft.pdf) | 15-Nov-2025 |

Covers multi-function decoder instructions (speed, functions, CV access), accessory decoder packets, and address encoding. This is the primary packet-level spec for command stations and decoders.

**Supersedes**: `../s-9.2.1_dcc_extended_packet_formats.pdf` (released version in parent directory)

---

## 3. Advanced Extended Packet Formats (S-9.2.1.1)

| Document | File | Posted |
|----------|------|--------|
| S-9.2.1.1 Advanced Extended Packet Formats | [s-9.2.1.1_advanced_extended_packet_formats_draft.pdf](s-9.2.1.1_advanced_extended_packet_formats_draft.pdf) | 23-Jun-2024 |

Covers binary state control, analog function control, time/date, system time, speed restriction, and extended consisting.

**Supersedes**: `../s-9.2.1.1_advanced_extended_packet_formats.pdf` (released version in parent directory)

---

## 4. Configuration Variables (S-9.2.2)

| Document | File | Posted |
|----------|------|--------|
| S-9.2.2 Configuration Variables | [s-9.2.2_configuration_variables_draft.pdf](s-9.2.2_configuration_variables_draft.pdf) | 11-Feb-2026 |

Defines all standard CVs, their ranges, mandatory/optional status, and indexed CV access (CV 31-32 paging).

**Supersedes**: `../s-9.2.2_decoder_cvs_2012.07.pdf` (released version in parent directory)

---

## 5. Bi-Directional Communication (S-9.3.2)

| Document | File | Posted |
|----------|------|--------|
| S-9.3.2 Bi-Directional Communications (RailCom) | [s-9.3.2_bidirectional_communications_draft.pdf](s-9.3.2_bidirectional_communications_draft.pdf) | 10-Apr-2026 |

Defines the RailCom cutout window, Channel 1/2 data encoding (4/8 bit), datagram IDs, and CV readback protocol. 68 pages — significantly expanded from previous releases.

**Supersedes**: `../S-9.3.2_2012_12_10.pdf` (released version in parent directory)

---

## 6. SUSI Bus Standards (S-9.4.x)

| Document | File | Posted |
|----------|------|--------|
| S-9.4.1 SUSI Bus Communication Interface | [s-9.4.1_susi_bus_communication_interface_draft.pdf](s-9.4.1_susi_bus_communication_interface_draft.pdf) | 29-Jan-2026 |
| S-9.4.2 SUSI Bus Configuration Variables | [s-9.4.2_susi_bus_configuration_variables_draft.pdf](s-9.4.2_susi_bus_configuration_variables_draft.pdf) | 29-Jan-2026 |
| S-9.4.3 SUSI Bus Bidirectional Extension | [s-9.4.3_susi_bus_bidirectional_extension_draft.pdf](s-9.4.3_susi_bus_bidirectional_extension_draft.pdf) | 29-Jan-2026 |
| S-9.4.4 SUSI Train Bus Extension Shift Registers | [s-9.4.4_susi_train_bus_extension_shift_registers_draft.pdf](s-9.4.4_susi_train_bus_extension_shift_registers_draft.pdf) | 29-Jan-2026 |

The SUSI (Serial User Standard Interface) bus is a serial interface between a DCC decoder and peripheral modules (sound, lighting, etc.):
- **S-9.4.1** — Physical layer and communication protocol
- **S-9.4.2** — Configuration variables specific to SUSI slave modules
- **S-9.4.3** — Bidirectional extension allowing slave-to-master communication
- **S-9.4.4** — Shift register extension for additional I/O (SIO)

---

## Cross-Reference: Drafts vs. Released Specs

| Draft | Replaces Released Spec | Key Changes |
|-------|----------------------|-------------|
| S-9.2.1 | s-9.2.1_dcc_extended_packet_formats.pdf | Under revision |
| S-9.2.1.1 | s-9.2.1.1_advanced_extended_packet_formats.pdf | Under revision |
| S-9.2.2 | s-9.2.2_decoder_cvs_2012.07.pdf | Under revision |
| S-9.3.2 | S-9.3.2_2012_12_10.pdf | Major expansion (68 pages) |
| S-9.1.1.6 | (new standard) | E24 connector — no prior release |
| S-9.4.x | (new standards) | SUSI bus — no prior releases |

---

*Source: [NMRA Documents Under Revision](https://www.nmra.org/documents-under-revision)*
*For released (non-draft) spec notes, see [../DCC_Spec_Reference.md](../DCC_Spec_Reference.md)*
