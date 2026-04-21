#!/usr/bin/env python3
"""
Generate OpenDccCLib documentation PDFs:
  1. QuickStartGuide_CommandStation.pdf
  2. QuickStartGuide_Decoder.pdf
  3. DeveloperGuide_CommandStation.pdf
  4. DeveloperGuide_Decoder.pdf

Style matches the OpenLcbCLib documentation set.
"""

from reportlab.lib.pagesizes import letter
from reportlab.lib.units import inch
from reportlab.lib.colors import HexColor
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_LEFT, TA_CENTER
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, PageBreak,
    Table, TableStyle, Preformatted, KeepTogether,
)
from reportlab.lib import colors
import os

# ============================================================================
# Color palette (matches OpenLcbCLib docs)
# ============================================================================
DARK_NAVY = HexColor("#2C3E5A")
STEEL_BLUE = HexColor("#3B6FA0")
LIGHT_BLUE_BG = HexColor("#E8F0FE")
TABLE_HEADER_BG = HexColor("#3B6FA0")
TABLE_ALT_ROW = HexColor("#F5F7FA")
WHITE = colors.white
BLACK = colors.black
BODY_GRAY = HexColor("#333333")

# ============================================================================
# Styles
# ============================================================================
def make_styles():
    s = {}
    s['title'] = ParagraphStyle('Title', fontName='Helvetica-Bold', fontSize=28,
                                 textColor=DARK_NAVY, spaceAfter=4, leading=34)
    s['subtitle'] = ParagraphStyle('Subtitle', fontName='Helvetica', fontSize=14,
                                    textColor=STEEL_BLUE, spaceAfter=6, leading=18)
    s['description'] = ParagraphStyle('Desc', fontName='Helvetica', fontSize=11,
                                       textColor=BODY_GRAY, spaceAfter=4, leading=14)
    s['heading1'] = ParagraphStyle('H1', fontName='Helvetica-Bold', fontSize=20,
                                    textColor=DARK_NAVY, spaceBefore=18, spaceAfter=8, leading=24)
    s['heading2'] = ParagraphStyle('H2', fontName='Helvetica-Bold', fontSize=14,
                                    textColor=STEEL_BLUE, spaceBefore=14, spaceAfter=6, leading=18)
    s['heading3'] = ParagraphStyle('H3', fontName='Helvetica-Bold', fontSize=11,
                                    textColor=STEEL_BLUE, spaceBefore=10, spaceAfter=4, leading=14)
    s['body'] = ParagraphStyle('Body', fontName='Helvetica', fontSize=10,
                                textColor=BODY_GRAY, spaceAfter=6, leading=14)
    s['bullet'] = ParagraphStyle('Bullet', fontName='Helvetica', fontSize=10,
                                  textColor=BODY_GRAY, spaceAfter=3, leading=14,
                                  leftIndent=18, bulletIndent=6)
    s['note'] = ParagraphStyle('Note', fontName='Helvetica-Oblique', fontSize=9,
                                textColor=STEEL_BLUE, spaceAfter=6, leading=12,
                                leftIndent=18, rightIndent=18, backColor=LIGHT_BLUE_BG,
                                borderPadding=(6, 8, 6, 8))
    s['code'] = ParagraphStyle('Code', fontName='Courier', fontSize=8,
                                textColor=BODY_GRAY, spaceAfter=6, leading=11,
                                leftIndent=18, backColor=HexColor("#F5F5F5"),
                                borderPadding=(6, 8, 6, 8))
    s['toc'] = ParagraphStyle('TOC', fontName='Helvetica', fontSize=11,
                               textColor=STEEL_BLUE, spaceAfter=4, leading=16,
                               leftIndent=12)
    s['footer'] = ParagraphStyle('Footer', fontName='Helvetica', fontSize=8,
                                  textColor=HexColor("#999999"))
    return s

STYLES = make_styles()

# ============================================================================
# Helper functions
# ============================================================================
def sp(pts=6):
    return Spacer(1, pts)

def note_box(text):
    """Blue italic note in a light-blue background box."""
    return Paragraph(text, STYLES['note'])

def code_block(text):
    """Monospace code block with gray background."""
    # Replace < and > for XML safety
    text = text.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
    return Preformatted(text, STYLES['code'])

def bullet(text):
    return Paragraph(f"\u2022 {text}", STYLES['bullet'])

def make_table(headers, rows, col_widths=None):
    """Create a styled table matching OpenLcbCLib look."""
    header_row = [Paragraph(f"<b>{h}</b>", ParagraphStyle('TH', fontName='Helvetica-Bold',
                  fontSize=9, textColor=WHITE, leading=12)) for h in headers]
    data = [header_row]
    for row in rows:
        data.append([Paragraph(str(cell), ParagraphStyle('TD', fontName='Helvetica',
                     fontSize=9, textColor=BODY_GRAY, leading=12)) for cell in row])

    if col_widths is None:
        col_widths = [None] * len(headers)

    t = Table(data, colWidths=col_widths, hAlign='LEFT')
    style_cmds = [
        ('BACKGROUND', (0, 0), (-1, 0), TABLE_HEADER_BG),
        ('TEXTCOLOR', (0, 0), (-1, 0), WHITE),
        ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
        ('FONTSIZE', (0, 0), (-1, -1), 9),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 6),
        ('TOPPADDING', (0, 0), (-1, -1), 4),
        ('LEFTPADDING', (0, 0), (-1, -1), 8),
        ('GRID', (0, 0), (-1, -1), 0.5, HexColor("#CCCCCC")),
        ('VALIGN', (0, 0), (-1, -1), 'TOP'),
    ]
    # Alternate row shading
    for i in range(1, len(data)):
        if i % 2 == 0:
            style_cmds.append(('BACKGROUND', (0, i), (-1, i), TABLE_ALT_ROW))
    t.setStyle(TableStyle(style_cmds))
    return t


def build_pdf(filename, footer_text, story_func):
    """Build a PDF with header/footer."""
    filepath = os.path.join(os.path.dirname(__file__), filename)

    def on_page(canvas_obj, doc):
        canvas_obj.saveState()
        canvas_obj.setFont('Helvetica', 8)
        canvas_obj.setFillColor(HexColor("#999999"))
        canvas_obj.drawString(inch, 0.5 * inch, footer_text)
        canvas_obj.drawRightString(letter[0] - inch, 0.5 * inch, f"Page {doc.page}")
        canvas_obj.restoreState()

    doc = SimpleDocTemplate(filepath, pagesize=letter,
                            topMargin=0.8*inch, bottomMargin=0.8*inch,
                            leftMargin=inch, rightMargin=inch)
    story = story_func()
    doc.build(story, onFirstPage=on_page, onLaterPages=on_page)
    print(f"  Created: {filepath}")


# ============================================================================
# 1. Quick Start Guide — Command Station
# ============================================================================
def qsg_command_station():
    S = STYLES
    story = []

    # --- TITLE PAGE ---
    story.append(sp(72))
    story.append(Paragraph("OpenDccCLib", S['title']))
    story.append(Paragraph("Quick Start Guide \u2014 Command Station", S['subtitle']))
    story.append(Paragraph("Get a DCC command station running in minutes", S['description']))
    story.append(sp(12))
    story.append(Paragraph("TI MSPM0G3507 LaunchPad + Code Composer Studio / Theia edition", S['description']))
    story.append(sp(12))
    story.append(note_box("OpenDccCLib supports any microcontroller with a C compiler \u2014 this guide covers one specific path."))
    story.append(PageBreak())

    # --- TABLE OF CONTENTS ---
    story.append(Paragraph("Table of Contents", S['heading1']))
    for item in [
        "1. What is DCC?",
        "2. What You Need",
        "3. Project Setup for Code Composer Studio",
        "4. Understanding dcc_user_config.h",
        "5. Building and Flashing",
        "6. Using the UART Command Interface",
        "7. What\u2019s Next",
    ]:
        story.append(Paragraph(item, S['toc']))
    story.append(PageBreak())

    # --- 1. What is DCC? ---
    story.append(Paragraph("1. What is DCC?", S['heading1']))
    story.append(note_box(
        "This guide uses a TI MSPM0G3507 LaunchPad with Code Composer Studio / Theia as a concrete, "
        "working example. OpenDccCLib itself is platform-agnostic C \u2014 it can be adapted to any "
        "microcontroller with a C compiler and a timer peripheral. See the Developer Guide for other "
        "platforms and IDEs."
    ))
    story.append(sp(4))
    story.append(Paragraph(
        "DCC (Digital Command Control) is the NMRA standard for controlling model railroad "
        "locomotives and accessories. A command station generates the DCC signal \u2014 a modulated "
        "square wave on the track rails that carries addressed speed, function, and programming "
        "commands to decoders installed in locomotives and accessories.",
        S['body']
    ))
    story.append(Paragraph(
        "OpenDccCLib is a portable C library that implements the DCC protocol for microcontrollers, "
        "handling all low-level bit encoding, packet scheduling, service mode programming, and "
        "RailCom detection so you can focus on what your command station actually does.",
        S['body']
    ))
    story.append(Paragraph("Key Terms", S['heading2']))
    for term, desc in [
        ("Command Station", "The device that generates DCC packets on the track."),
        ("Decoder", "A device inside a locomotive or accessory that receives and acts on DCC commands."),
        ("Packet", "A sequence of bytes encoding one command (address + instruction + error check)."),
        ("Preamble", "At least 14 one-bits before each packet for synchronization."),
        ("Service Mode", "A special programming track mode for reading/writing decoder Configuration Variables (CVs)."),
        ("RailCom", "A bi-directional communication channel where decoders respond during a cutout in the DCC signal."),
        ("Scheduler", "The library component that manages concurrent packets, priorities, and auto-refresh."),
    ]:
        story.append(bullet(f"<b>{term}:</b> {desc}"))
    story.append(PageBreak())

    # --- 2. What You Need ---
    story.append(Paragraph("2. What You Need", S['heading1']))
    story.append(Paragraph("Hardware", S['heading2']))
    for item in [
        "TI MSPM0G3507 LaunchPad development board (LP-MSPM0G3507)",
        "H-bridge motor driver board (e.g. L298N or IBT-2) for track power",
        "DCC-compatible locomotive with decoder, OR a second LaunchPad running the decoder demo",
        "USB cable for programming and UART terminal",
        "12\u201318V DC power supply for the H-bridge",
    ]:
        story.append(bullet(item))

    story.append(Paragraph("DCC Signal Wiring", S['heading2']))
    story.append(make_table(
        ["LaunchPad Pin", "Function"],
        [
            ["PA15", "Main track DCC signal output (connect to H-bridge IN1/IN2)"],
            ["PB4", "Service track DCC signal output"],
            ["PB12", "Service track current sense input (ACK detection)"],
            ["PA18", "Debug GPIO (scope trigger on packet sent)"],
        ],
        col_widths=[1.5*inch, 4.5*inch]
    ))

    story.append(Paragraph("Software", S['heading2']))
    for item in [
        "TI Code Composer Studio (CCS) or CCS Theia IDE",
        "TI SysConfig tool (included with CCS)",
        "A serial terminal (PuTTY, Tera Term, or CCS Terminal) at 115200 baud",
        "The OpenDccCLib source code",
    ]:
        story.append(bullet(item))
    story.append(PageBreak())

    # --- 3. Project Setup ---
    story.append(Paragraph("3. Project Setup for Code Composer Studio", S['heading1']))
    story.append(Paragraph("Starting From the Example", S['heading2']))
    story.append(Paragraph("The library includes a ready-to-run command station example at:", S['body']))
    story.append(code_block("OpenDccCLib/applications/ti_theia/mspm03507_launchpad/command_station/"))
    story.append(Paragraph(
        "1. Open CCS/Theia and import the project (File &gt; Import &gt; CCS Projects).<br/>"
        "2. Point it at the command_station folder.<br/>"
        "3. The project references the DCC library source via a relative path to src/dcc/.",
        S['body']
    ))

    story.append(Paragraph("Complete Project File Structure", S['heading2']))
    story.append(code_block(
        "command_station/\n"
        "  command_station.c              <- Main entry point + ISR handlers\n"
        "  dcc_user_config.h              <- Feature flags & buffer sizes\n"
        "  uart_command_parser.c/h        <- UART command-line interface\n"
        "  application_callbacks/\n"
        "    callbacks_dcc.c/h            <- Your application logic\n"
        "  application_drivers/\n"
        "    ti_driverlib_dcc_driver.c/h  <- Hardware abstraction (timers, GPIO)\n"
        "    ti_driverlib_uart_driver.c/h <- UART I/O\n"
        "  dcc_lib/                       <- Library (do not edit)\n"
        "    dcc_config.h/c               <- Config struct & lifecycle API\n"
        "    dcc_bit_encoder.h/c          <- ISR-level bit serialization\n"
        "    dcc_scheduler.h/c            <- Packet scheduling\n"
        "    dcc_packet_encoder.h/c       <- Packet construction\n"
        "    dcc_railcom_cutout.h/c       <- RailCom cutout timing\n"
        "    dcc_railcom_decoder.h/c      <- RailCom datagram decoding\n"
        "    dcc_types.h                  <- All data types\n"
        "    dcc_defines.h                <- NMRA protocol constants\n"
        "  Debug/\n"
        "    ti_msp_dl_config.c/h         <- SysConfig-generated init"
    ))
    story.append(PageBreak())

    # --- 4. Understanding dcc_user_config.h ---
    story.append(Paragraph("4. Understanding dcc_user_config.h", S['heading1']))
    story.append(Paragraph(
        "This is the most important file in your project. It tells the library which role to compile "
        "and how much memory to allocate. Every value is mandatory for the enabled role.",
        S['body']
    ))
    story.append(code_block(
        "// --- Role Selection ---\n"
        "#define DCC_COMPILE_COMMAND_STATION    // Packet scheduler, bit encoder, service mode\n"
        "\n"
        "// --- Service Mode Methods ---\n"
        "#define DCC_COMPILE_SERVICE_MODE_DIRECT   // Direct CV read/write\n"
        "#define DCC_COMPILE_SERVICE_MODE_PAGED    // Paged mode\n"
        "// #define DCC_COMPILE_SERVICE_MODE_REGISTER  // Register mode\n"
        "// #define DCC_COMPILE_SERVICE_MODE_ADDRESS   // Address-only mode\n"
        "\n"
        "// --- Buffer & Pool Sizes ---\n"
        "#define USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT   24  // Max concurrent packets\n"
        "#define USER_DEFINED_DCC_MAX_LOCOS              10  // Auto-refresh locomotives\n"
        "#define USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH    4  // RailCom receive ring buffer\n"
        "#define USER_DEFINED_DCC_SERVICE_MODE_RETRIES    3  // Programming retry count\n"
        "#define USER_DEFINED_DCC_ACK_THRESHOLD_MA       60  // ACK detection threshold (mA)\n"
        "#define USER_DEFINED_DCC_ACK_MIN_DURATION_US  5000  // ACK min pulse (us)\n"
        "#define USER_DEFINED_DCC_ACK_MAX_DURATION_US  7000  // ACK max pulse (us)"
    ))
    story.append(note_box(
        "DCC_COMPILE_SERVICE_MODE_* flags require DCC_COMPILE_COMMAND_STATION. The compiler will "
        "error if you enable a service mode without the command station role."
    ))
    story.append(PageBreak())

    # --- 5. Building and Flashing ---
    story.append(Paragraph("5. Building and Flashing", S['heading1']))
    story.append(Paragraph("In Code Composer Studio", S['heading2']))
    story.append(Paragraph(
        "1. Open the imported command station project.<br/>"
        "2. Build the project (Project &gt; Build or Ctrl+B).<br/>"
        "3. Connect the LaunchPad via USB.<br/>"
        "4. Flash and run (Run &gt; Debug or F11).<br/>"
        "5. Open a serial terminal at 115200 baud on the LaunchPad\u2019s COM port.<br/>"
        "6. You should see the startup banner: \u201cDCC Command Station - MSPM0G3507 LaunchPad\u201d",
        S['body']
    ))
    story.append(Paragraph("What Happens at Power-On", S['heading2']))
    story.append(Paragraph(
        "The main() function initializes hardware drivers, passes the dcc_config_t struct to "
        "DccConfig_initialize(), then enters the main loop calling DccConfig_run() and the UART "
        "parser. Track power is OFF by default \u2014 type POWER ON to start generating DCC packets.",
        S['body']
    ))
    story.append(Paragraph("The Main Loop", S['heading2']))
    story.append(code_block(
        "while (1) {\n"
        "    DccConfig_run();                // Library packet scheduling & callbacks\n"
        "    TI_UartDriver_echo_process();   // Echo typed characters\n"
        "    UartCommandParser_process();    // Parse and execute commands\n"
        "}"
    ))
    story.append(PageBreak())

    # --- 6. Using the UART Command Interface ---
    story.append(Paragraph("6. Using the UART Command Interface", S['heading1']))
    story.append(Paragraph(
        "Type HELP at the terminal to see all commands. Here are the most common:",
        S['body']
    ))
    story.append(make_table(
        ["Command", "Description"],
        [
            ["POWER ON|OFF", "Enable/disable track power"],
            ["SPEED &lt;addr&gt; &lt;speed&gt; &lt;FWD|REV&gt; [128]", "Set locomotive speed (0\u2013126, 128-step mode)"],
            ["ESTOP [addr]", "Emergency stop (all or specific loco)"],
            ["FUNC &lt;addr&gt; &lt;0-68&gt; &lt;ON|OFF&gt;", "Toggle function output (lights, sound, etc.)"],
            ["ACC &lt;board&gt; &lt;pair&gt; &lt;ON|OFF&gt;", "Throw/close basic accessory (turnout)"],
            ["ACCE &lt;addr&gt; &lt;aspect&gt;", "Set extended accessory signal aspect"],
            ["SVC ENTER", "Enter service mode (programming track)"],
            ["SVC EXIT", "Exit service mode"],
            ["SVC DIRECT WRITE &lt;cv&gt; &lt;value&gt;", "Write CV in direct mode"],
            ["SVC DIRECT VERIFY &lt;cv&gt; &lt;value&gt;", "Verify CV in direct mode"],
            ["STATUS", "Show scheduler and track status"],
            ["HELP", "List all commands"],
        ],
        col_widths=[3*inch, 3*inch]
    ))
    story.append(Paragraph("Quick Test", S['heading2']))
    story.append(code_block(
        "> POWER ON\n"
        "OK: track power ON\n"
        "> SPEED 3 50 FWD\n"
        "OK: SPEED addr=3 speed=50 dir=FWD mode=128\n"
        "> FUNC 3 0 ON\n"
        "OK: FUNC addr=3 f0=ON\n"
        "> ESTOP\n"
        "OK: ESTOP broadcast"
    ))
    story.append(PageBreak())

    # --- 7. What's Next ---
    story.append(Paragraph("7. What\u2019s Next", S['heading1']))
    story.append(Paragraph("Add RailCom Detection", S['heading2']))
    story.append(Paragraph(
        "Wire a RailCom detector\u2019s UART output to a 250 kbaud UART peripheral. Populate the "
        "dcc_railcom_hw_t struct and point main_track.railcom to it. The library handles cutout "
        "timing, datagram decoding, and fires on_railcom_datagram_result.",
        S['body']
    ))
    story.append(Paragraph("Add Current-Sense Protection", S['heading2']))
    story.append(Paragraph(
        "Wire an ADC or comparator to the main track current sense. Set main_track.current_sense_read "
        "to your read function. The library uses this for overcurrent protection.",
        S['body']
    ))
    story.append(Paragraph("Port to Another MCU", S['heading2']))
    story.append(Paragraph(
        "Copy the command_station project. Rewrite the two driver files (ti_driverlib_dcc_driver.c/h) "
        "for your MCU\u2019s timer and GPIO API. The library never calls hardware directly \u2014 all "
        "hardware access goes through the function pointers in dcc_config_t.",
        S['body']
    ))
    story.append(Paragraph("Read the Developer Guide", S['heading2']))
    story.append(Paragraph(
        "The companion OpenDccCLib Command Station Developer Guide walks through every part of the "
        "system in detail: the config struct, scheduler internals, bit encoding, RailCom cutout, "
        "service mode state machines, and more.",
        S['body']
    ))

    return story


# ============================================================================
# 2. Quick Start Guide — Decoder
# ============================================================================
def qsg_decoder():
    S = STYLES
    story = []

    # --- TITLE PAGE ---
    story.append(sp(72))
    story.append(Paragraph("OpenDccCLib", S['title']))
    story.append(Paragraph("Quick Start Guide \u2014 Decoder", S['subtitle']))
    story.append(Paragraph("Get a DCC decoder running in minutes", S['description']))
    story.append(sp(12))
    story.append(Paragraph("TI MSPM0G3507 LaunchPad + Code Composer Studio / Theia edition", S['description']))
    story.append(sp(12))
    story.append(note_box("OpenDccCLib supports any microcontroller with a C compiler \u2014 this guide covers one specific path."))
    story.append(PageBreak())

    # --- TOC ---
    story.append(Paragraph("Table of Contents", S['heading1']))
    for item in [
        "1. What is a DCC Decoder?",
        "2. What You Need",
        "3. Project Setup for Code Composer Studio",
        "4. Understanding dcc_user_config.h",
        "5. Building and Flashing",
        "6. How the Decoder Works",
        "7. Customizing Callbacks",
        "8. What\u2019s Next",
    ]:
        story.append(Paragraph(item, S['toc']))
    story.append(PageBreak())

    # --- 1. What is a DCC Decoder? ---
    story.append(Paragraph("1. What is a DCC Decoder?", S['heading1']))
    story.append(note_box(
        "This guide uses a TI MSPM0G3507 LaunchPad with Code Composer Studio / Theia as a concrete, "
        "working example. OpenDccCLib itself is platform-agnostic C \u2014 it can be adapted to any "
        "microcontroller with a C compiler and GPIO input-capture. See the Developer Guide for other "
        "platforms and IDEs."
    ))
    story.append(sp(4))
    story.append(Paragraph(
        "A DCC decoder is a device installed in a locomotive or accessory that listens to the DCC "
        "signal on the track rails, recognizes commands addressed to it, and acts on them \u2014 "
        "controlling motors, lights, servos, solenoids, or sound.",
        S['body']
    ))
    story.append(Paragraph(
        "OpenDccCLib handles all the low-level work: classifying one/zero bits from edge timing, "
        "assembling packets, validating the XOR error check, matching addresses, and dispatching "
        "commands to your application callbacks. You write the code that drives your hardware.",
        S['body']
    ))
    story.append(Paragraph("Key Terms", S['heading2']))
    for term, desc in [
        ("Decoder", "The device that receives DCC commands and acts on them."),
        ("Command Station", "The device that generates DCC packets on the track."),
        ("Bit Decoder", "Classifies DCC one-bits (58\u00b5s half-period) from zero-bits (100\u00b5s+) based on edge timing."),
        ("Packet Decoder", "Assembles bits into bytes, validates XOR error check, and dispatches to callbacks."),
        ("Configuration Variable (CV)", "A numbered setting stored in the decoder (address, speed curve, functions, etc.)."),
        ("Service Mode", "Programming track mode where the command station reads/writes CVs. The decoder responds with a 6ms current pulse (ACK)."),
        ("Failsafe", "If no valid DCC packet arrives for too long, the decoder enters failsafe mode (motor stop, outputs off)."),
    ]:
        story.append(bullet(f"<b>{term}:</b> {desc}"))
    story.append(PageBreak())

    # --- 2. What You Need ---
    story.append(Paragraph("2. What You Need", S['heading1']))
    story.append(Paragraph("Hardware", S['heading2']))
    for item in [
        "TI MSPM0G3507 LaunchPad development board (LP-MSPM0G3507)",
        "A DCC command station (commercial unit, OR a second LaunchPad running the command station demo)",
        "DCC signal input circuit (optocoupler or voltage divider to convert track-level DCC to 3.3V logic)",
        "USB cable for programming and UART terminal",
        "Optional: motor driver, LEDs, servos for output testing",
    ]:
        story.append(bullet(item))

    story.append(Paragraph("DCC Signal Wiring", S['heading2']))
    story.append(make_table(
        ["LaunchPad Pin", "Function"],
        [
            ["PB1", "Main track DCC signal input (edge-detect GPIO)"],
            ["PB4", "Service track DCC signal input (edge-detect GPIO)"],
            ["PB17", "Track select mux (LOW = main, HIGH = service)"],
            ["PB3", "Test pin (toggles on each decoded edge, for logic analyzer)"],
            ["ACK pin", "ACK current load output (6ms pulse for service mode)"],
        ],
        col_widths=[1.5*inch, 4.5*inch]
    ))

    story.append(Paragraph("Software", S['heading2']))
    for item in [
        "TI Code Composer Studio (CCS) or CCS Theia IDE",
        "TI SysConfig tool (included with CCS)",
        "A serial terminal (PuTTY, Tera Term, or CCS Terminal) at 115200 baud",
        "The OpenDccCLib source code",
    ]:
        story.append(bullet(item))
    story.append(PageBreak())

    # --- 3. Project Setup ---
    story.append(Paragraph("3. Project Setup for Code Composer Studio", S['heading1']))
    story.append(Paragraph("Starting From the Example", S['heading2']))
    story.append(Paragraph("The library includes a ready-to-run decoder example at:", S['body']))
    story.append(code_block("OpenDccCLib/applications/ti_theia/mspm03507_launchpad/decoder/"))
    story.append(Paragraph(
        "1. Open CCS/Theia and import the project (File &gt; Import &gt; CCS Projects).<br/>"
        "2. Point it at the decoder folder.<br/>"
        "3. The project references the DCC library source via a relative path to src/dcc/.",
        S['body']
    ))
    story.append(Paragraph("Complete Project File Structure", S['heading2']))
    story.append(code_block(
        "decoder/\n"
        "  decoder.c                      <- Main entry point + edge ISR\n"
        "  dcc_user_config.h              <- Feature flags & limits\n"
        "  decoder_command_parser.c/h     <- UART command interface\n"
        "  application_callbacks/\n"
        "    callbacks_dcc.c/h            <- Your motor/LED/servo logic\n"
        "  application_drivers/\n"
        "    ti_driverlib_dcc_driver.c/h  <- Hardware abstraction (timers, GPIO)\n"
        "    ti_driverlib_uart_driver.c/h <- UART I/O\n"
        "    ack_pulse_driver.c/h         <- ACK current load control\n"
        "  dcc_lib/                       <- Library (do not edit)\n"
        "    dcc_config.h/c               <- Config struct & lifecycle API\n"
        "    dcc_bit_decoder.h/c          <- Edge timing classification\n"
        "    dcc_packet_decoder.h/c       <- Packet assembly & dispatch\n"
        "    dcc_cv_storage.h/c           <- CV read/write with lock support\n"
        "    dcc_types.h                  <- All data types\n"
        "    dcc_defines.h                <- NMRA protocol constants\n"
        "  Debug/\n"
        "    ti_msp_dl_config.c/h         <- SysConfig-generated init"
    ))
    story.append(PageBreak())

    # --- 4. dcc_user_config.h ---
    story.append(Paragraph("4. Understanding dcc_user_config.h", S['heading1']))
    story.append(Paragraph(
        "For a decoder, the configuration file is simpler than a command station \u2014 you only need "
        "the decoder role and a function count.",
        S['body']
    ))
    story.append(code_block(
        "// --- Role Selection ---\n"
        "#define DCC_COMPILE_DECODER            // Bit decoder, packet parser, CV storage\n"
        "// #define DCC_COMPILE_ACCESSORY_DECODER  // Accessory-specific features\n"
        "\n"
        "// --- Decoder Configuration ---\n"
        "#define USER_DEFINED_DCC_DECODER_MAX_FUNCTIONS  29  // F0-F28"
    ))
    story.append(note_box(
        "Do NOT define DCC_COMPILE_COMMAND_STATION in a decoder project. The decoder and command "
        "station roles can coexist in the same binary (for testing), but a real decoder only needs "
        "DCC_COMPILE_DECODER."
    ))
    story.append(PageBreak())

    # --- 5. Building and Flashing ---
    story.append(Paragraph("5. Building and Flashing", S['heading1']))
    story.append(Paragraph("In Code Composer Studio", S['heading2']))
    story.append(Paragraph(
        "1. Open the imported decoder project.<br/>"
        "2. Build the project (Project &gt; Build or Ctrl+B).<br/>"
        "3. Connect the LaunchPad via USB.<br/>"
        "4. Flash and run (Run &gt; Debug or F11).<br/>"
        "5. Open a serial terminal at 115200 baud on the LaunchPad\u2019s COM port.<br/>"
        "6. You should see: \u201cDCC Decoder - MSPM0G3507 LaunchPad\u201d",
        S['body']
    ))
    story.append(Paragraph("What Happens at Power-On", S['heading2']))
    story.append(Paragraph(
        "The main() function initializes hardware drivers and CV defaults (address 3 by default via "
        "CV1=3, CV29=0x06), passes the dcc_config_t struct to DccConfig_initialize(), then enters "
        "the main loop. The decoder immediately begins listening for DCC edges on PB1 (main track).",
        S['body']
    ))
    story.append(Paragraph("The Main Loop", S['heading2']))
    story.append(code_block(
        "while (1) {\n"
        "    _drain_edge_buffer();            // Feed buffered ISR timestamps to bit decoder\n"
        "    DccConfig_run();                  // Library housekeeping & callback dispatch\n"
        "    CallbacksDcc_drain();             // Push RECV log lines to UART\n"
        "    TI_UartDriver_echo_process();     // Echo typed characters\n"
        "    DecoderCommandParser_process();   // Handle typed commands\n"
        "}"
    ))
    story.append(PageBreak())

    # --- 6. How the Decoder Works ---
    story.append(Paragraph("6. How the Decoder Works", S['heading1']))
    story.append(Paragraph("Edge Capture (ISR)", S['heading2']))
    story.append(Paragraph(
        "The GPIO edge-detect ISR captures a microsecond timestamp into a 256-entry ring buffer. "
        "The ISR is minimal (\u223c1\u00b5s) to avoid overrunning the 58\u00b5s DCC half-bit period. "
        "No decoding happens in the ISR.",
        S['body']
    ))
    story.append(Paragraph("Bit Decoding (Main Loop)", S['heading2']))
    story.append(Paragraph(
        "The main loop drains the ring buffer and feeds each timestamp to DccConfig_decoder_edge_isr(). "
        "The bit decoder measures the interval between edges: &lt;80\u00b5s = one-bit, \u226580\u00b5s = zero-bit "
        "(threshold set by DCC_DECODER_HALF_BIT_THRESHOLD_US = 80).",
        S['body']
    ))
    story.append(Paragraph("Packet Decoding", S['heading2']))
    story.append(Paragraph(
        "Once the bit decoder accumulates a valid preamble (10+ one-bits) followed by a start bit "
        "(zero), it collects bytes until the end bit (one). The packet decoder validates the XOR "
        "error check, matches the address against CV1/CV17-18/CV29, and dispatches to your "
        "application callbacks.",
        S['body']
    ))
    story.append(Paragraph("Address Matching", S['heading2']))
    story.append(Paragraph(
        "The library reads CV1 (short address), CV17-18 (long address), and CV29 (configuration "
        "register) during DccConfig_initialize(). CV29 bit 5 selects short vs. long addressing. "
        "The default demo uses short address 3.",
        S['body']
    ))
    story.append(PageBreak())

    # --- 7. Customizing Callbacks ---
    story.append(Paragraph("7. Customizing Callbacks", S['heading1']))
    story.append(Paragraph(
        "All your application logic lives in callbacks_dcc.c. Each callback fires when the library "
        "decodes the corresponding DCC command. The demo implementation logs commands to UART; "
        "replace with your motor/LED/servo control code.",
        S['body']
    ))
    story.append(Paragraph("Speed Command Callback", S['heading3']))
    story.append(code_block(
        "void CallbacksDcc_on_speed_command(uint16_t address,\n"
        "                                   uint8_t speed,\n"
        "                                   bool direction,\n"
        "                                   dcc_speed_mode_enum mode)\n"
        "{\n"
        "    // speed: 0 = stop, 1 = emergency stop, 2-126 = speed steps\n"
        "    // direction: true = forward, false = reverse\n"
        "    // mode: DCC_SPEED_MODE_14, _28, or _128\n"
        "    set_motor_speed(speed, direction);\n"
        "}"
    ))
    story.append(Paragraph("Function Command Callback", S['heading3']))
    story.append(code_block(
        "void CallbacksDcc_on_function_command(uint16_t address,\n"
        "                                      uint8_t function_number,\n"
        "                                      bool state)\n"
        "{\n"
        "    // function_number: 0 = headlight (FL), 1-68 = F1-F68\n"
        "    // state: true = on, false = off\n"
        "    if (function_number == 0)\n"
        "        set_headlight(state);\n"
        "}"
    ))
    story.append(Paragraph("All Available Callbacks", S['heading3']))
    story.append(make_table(
        ["Callback", "When It Fires"],
        [
            ["on_speed_command", "Speed and direction command for this address"],
            ["on_emergency_stop_command", "Emergency stop for this address or broadcast"],
            ["on_function_command", "Function on/off (F0\u2013F68)"],
            ["on_accessory_basic_command", "Basic accessory (turnout throw/close)"],
            ["on_accessory_extended_command", "Extended accessory (signal aspect)"],
            ["on_cv_write_command", "CV was written (notification after the fact)"],
            ["on_cv_verify_command", "CV verify command received"],
            ["on_cv_bit_command", "CV bit-level manipulation"],
            ["on_consist_command", "Consist (multi-unit) setup/clear"],
            ["on_failsafe_entered", "No valid DCC \u2014 stop motor, safe outputs"],
            ["on_failsafe_exited", "Valid DCC resumed"],
        ],
        col_widths=[2.5*inch, 3.5*inch]
    ))
    story.append(PageBreak())

    # --- 8. What's Next ---
    story.append(Paragraph("8. What\u2019s Next", S['heading1']))
    story.append(Paragraph("Add RailCom Responses", S['heading2']))
    story.append(Paragraph(
        "Set railcom_tx_pin_set in dcc_config_t to a function that drives a GPIO connected to a "
        "current-source circuit. The library bit-bangs 4/8-encoded RailCom bytes at 250 kbaud "
        "during the cutout window. Also set decoder_edge_irq_enable to blank the edge interrupt "
        "during the cutout.",
        S['body']
    ))
    story.append(Paragraph("Add Persistent CV Storage", S['heading2']))
    story.append(Paragraph(
        "The demo stores CVs in a RAM array that resets on power cycle. Replace CallbacksDcc_cv_read "
        "and CallbacksDcc_cv_write with Flash or EEPROM read/write functions so CV changes survive "
        "power cycles.",
        S['body']
    ))
    story.append(Paragraph("Port to Another MCU", S['heading2']))
    story.append(Paragraph(
        "Copy the decoder project. Rewrite the driver files for your MCU\u2019s GPIO input-capture "
        "and timer API. The only ISR requirement is capturing a microsecond timestamp on each DCC "
        "signal edge.",
        S['body']
    ))
    story.append(Paragraph("Read the Developer Guide", S['heading2']))
    story.append(Paragraph(
        "The companion OpenDccCLib Decoder Developer Guide walks through every part of the system in "
        "detail: the config struct, bit decoder state machine, packet decoder, CV storage, address "
        "matching, failsafe logic, and more.",
        S['body']
    ))

    return story


# ============================================================================
# 3. Developer Guide — Command Station
# ============================================================================
def devguide_command_station():
    S = STYLES
    story = []

    # --- TITLE PAGE ---
    story.append(sp(72))
    story.append(Paragraph("OpenDccCLib", S['title']))
    story.append(Paragraph("Developer Guide \u2014 Command Station", S['subtitle']))
    story.append(Paragraph("From first project to a fully featured DCC command station", S['description']))
    story.append(sp(8))
    story.append(Paragraph("Covers all supported platforms and IDEs", S['description']))
    story.append(PageBreak())

    # --- TOC ---
    story.append(Paragraph("Table of Contents", S['heading1']))
    toc_items = [
        "1. Introduction",
        "   1.1 What the Library Does (and Does Not Do)",
        "   1.2 Platform Support",
        "2. DCC Protocol Concepts",
        "   2.1 Signal Encoding",
        "   2.2 Packet Format",
        "   2.3 Address Types",
        "   2.4 Instruction Types",
        "3. Project File Structure",
        "4. dcc_user_config.h in Depth",
        "   4.1 Role and Service Mode Flags",
        "   4.2 Scheduler Slot Count",
        "   4.3 Auto-Refresh Locos",
        "   4.4 RailCom Buffer",
        "   4.5 Service Mode Parameters",
        "5. Initialization \u2014 command_station.c",
        "   5.1 The dcc_config_t Struct",
        "   5.2 Per-Channel Hardware (dcc_output_hw_t)",
        "   5.3 RailCom Hardware (dcc_railcom_hw_t)",
        "   5.4 Setup and Main Loop",
        "6. ISR Architecture",
        "   6.1 The 58\u00b5s Shared Timer",
        "   6.2 RailCom One-Shot Timer",
        "   6.3 100ms Periodic Tick",
        "7. Implementing the Drivers",
        "   7.1 Platform Driver Functions",
        "   7.2 Pin Toggle",
        "   7.3 Track Power",
        "   7.4 Current Sense",
        "   7.5 Timestamp",
        "8. The Bit Encoder",
        "9. The Scheduler",
        "   9.1 Slot Allocation",
        "   9.2 Priority System",
        "   9.3 Duplicate Combining",
        "   9.4 Auto-Refresh",
        "10. Service Mode Programming",
        "   10.1 Direct Mode",
        "   10.2 Paged Mode",
        "   10.3 Register Mode",
        "   10.4 Address Mode",
        "   10.5 ACK Detection",
        "11. RailCom (Brief Overview)",
        "12. Callbacks \u2014 Where Your Application Lives",
        "13. Application API Reference",
        "14. Porting to a New MCU",
        "15. Unit Testing",
        "16. Troubleshooting",
    ]
    for item in toc_items:
        indent = 18 if item.startswith("   ") else 0
        sty = ParagraphStyle('TOCItem', parent=S['toc'], leftIndent=indent + 12)
        story.append(Paragraph(item.strip(), sty))
    story.append(PageBreak())

    # --- 1. Introduction ---
    story.append(Paragraph("1. Introduction", S['heading1']))
    story.append(Paragraph(
        "OpenDccCLib is a C library that implements the NMRA DCC (Digital Command Control) protocol "
        "for microcontrollers. It is designed to be portable across any hardware platform and IDE, "
        "with no dynamic memory allocation \u2014 all buffers are statically sized at compile time, "
        "making it suitable for processors with very limited RAM.",
        S['body']
    ))
    story.append(Paragraph(
        "This guide walks you through building a complete DCC command station, then explains every "
        "module so you understand exactly what the code does and how to extend it.",
        S['body']
    ))

    story.append(Paragraph("1.1 What the Library Does (and Does Not Do)", S['heading2']))
    story.append(Paragraph(
        "The library handles all DCC protocol compliance: bit encoding with NMRA-compliant timing "
        "(58\u00b5s one-bits, 100\u00b5s zero-bits), packet construction with XOR error detection, "
        "multi-slot scheduling with priority and duplicate combining, all four NMRA service mode "
        "methods (direct, paged, register, address), RailCom cutout timing and datagram decoding, "
        "and auto-refresh of locomotive speed/function commands.",
        S['body']
    ))
    story.append(Paragraph(
        "It does NOT include any hardware-specific code. You supply short driver functions that "
        "toggle GPIO pins, start/stop timers, and read current sense, and the library calls them "
        "as needed through function pointers in the dcc_config_t struct.",
        S['body']
    ))

    story.append(Paragraph("1.2 Platform Support", S['heading2']))
    story.append(Paragraph(
        "Because the library contains no hardware-specific code, it can be ported to any processor "
        "and toolchain. You provide a small set of driver functions for your timer and GPIO hardware, "
        "and the library does the rest.",
        S['body']
    ))
    story.append(make_table(
        ["Processor / Board", "Example IDE / Toolchain"],
        [
            ["TI MSPM0G3507", "Code Composer Studio / Theia"],
            ["ESP32", "Arduino IDE, PlatformIO"],
            ["STM32 (F4 and others)", "STM32 Cube IDE"],
            ["Raspberry Pi Pico (RP2040)", "Arduino IDE, PlatformIO"],
            ["Microchip dsPIC33", "MPLAB X"],
            ["Any ARM Cortex-M", "GCC + Makefile"],
        ],
        col_widths=[2.5*inch, 3.5*inch]
    ))
    story.append(note_box(
        "This guide focuses on TI MSPM0G3507 as the worked example. All concepts apply equally to "
        "any other target."
    ))
    story.append(PageBreak())

    # --- 2. DCC Protocol Concepts ---
    story.append(Paragraph("2. DCC Protocol Concepts", S['heading1']))

    story.append(Paragraph("2.1 Signal Encoding", S['heading2']))
    story.append(Paragraph(
        "The DCC signal is a square wave where the bit value is encoded in the half-period duration. "
        "A one-bit has a nominal half-period of 58\u00b5s (NMRA allows 55\u201361\u00b5s). "
        "A zero-bit has a minimum half-period of 100\u00b5s. The command station alternates the "
        "polarity of the track voltage at these intervals.",
        S['body']
    ))
    story.append(make_table(
        ["Bit Value", "Half-Period", "NMRA Range"],
        [
            ["One (1)", "58 \u00b5s nominal", "55\u201361 \u00b5s"],
            ["Zero (0)", "100 \u00b5s minimum", "\u2265100 \u00b5s (command station)"],
        ],
        col_widths=[1.5*inch, 2*inch, 2.5*inch]
    ))

    story.append(Paragraph("2.2 Packet Format", S['heading2']))
    story.append(Paragraph(
        "Every DCC packet follows this structure: Preamble (14+ one-bits) | Start bit (0) | "
        "Address byte | Start bit (0) | Instruction byte(s) | Start bit (0) | XOR error check | "
        "End bit (1). The XOR byte is the exclusive-or of all preceding data bytes.",
        S['body']
    ))

    story.append(Paragraph("2.3 Address Types", S['heading2']))
    story.append(make_table(
        ["Address Type", "Range", "Bytes"],
        [
            ["Broadcast", "0", "1"],
            ["Short (7-bit)", "1\u2013127", "1"],
            ["Long (14-bit)", "128\u201310239", "2 (0xC0 prefix)"],
            ["Idle", "255", "1"],
            ["Accessory", "1\u2013511 (board)", "2"],
            ["Extended Accessory", "1\u20132048", "2"],
        ],
        col_widths=[2*inch, 2*inch, 2*inch]
    ))

    story.append(Paragraph("2.4 Instruction Types", S['heading2']))
    story.append(make_table(
        ["Instruction", "Mask", "Description"],
        [
            ["Speed (14/28-step)", "0x40/0x60", "Forward/reverse speed command"],
            ["Speed (128-step)", "0x3F (advanced)", "Extended speed with 126 steps"],
            ["Function Group 1", "0x80", "FL, F1\u2013F4"],
            ["Function Group 2a", "0xB0", "F5\u2013F8"],
            ["Function Group 2b", "0xA0", "F9\u2013F12"],
            ["Feature Expansion", "0xC0", "F13+, binary state, analog"],
            ["CV Access (long)", "0xE0", "Operations mode CV read/write"],
        ],
        col_widths=[2*inch, 1.5*inch, 2.5*inch]
    ))
    story.append(PageBreak())

    # --- 3. Project File Structure ---
    story.append(Paragraph("3. Project File Structure", S['heading1']))
    story.append(Paragraph(
        "Understanding the file layout tells you where to look when you need to change something. "
        "The files in dcc_lib/ are library code \u2014 do not edit them. The files at the top level "
        "and in application_callbacks/ and application_drivers/ are yours.",
        S['body']
    ))
    story.append(code_block(
        "command_station/                  <- Your project folder\n"
        "  command_station.c              <- Main entry (setup + loop + ISRs)\n"
        "  dcc_user_config.h              <- REQUIRED: feature flags & buffer sizes\n"
        "  uart_command_parser.c/h        <- Demo UART CLI\n"
        "  application_callbacks/\n"
        "    callbacks_dcc.c/h            <- Application event hooks\n"
        "  application_drivers/\n"
        "    ti_driverlib_dcc_driver.c/h  <- Hardware interface (timers, GPIO)\n"
        "    ti_driverlib_uart_driver.c/h <- UART I/O\n"
        "  dcc_lib/                       <- Library core (do not edit)\n"
        "    dcc_config.h/c              - Stack init API\n"
        "    dcc_types.h                 - All data types\n"
        "    dcc_defines.h               - NMRA constants\n"
        "    dcc_bit_encoder.h/c         - ISR-level bit serialization\n"
        "    dcc_packet_encoder.h/c      - Packet byte construction\n"
        "    dcc_scheduler.h/c           - Multi-slot packet scheduling\n"
        "    dcc_railcom_cutout.h/c      - RailCom cutout state machine\n"
        "    dcc_railcom_decoder.h/c     - RailCom datagram decoder\n"
        "    dcc_service_mode_*.h/c      - Service mode state machines\n"
        "    dcc_application_*.h/c       - Public application API"
    ))
    story.append(PageBreak())

    # --- 4. dcc_user_config.h in Depth ---
    story.append(Paragraph("4. dcc_user_config.h in Depth", S['heading1']))
    story.append(Paragraph(
        "This file is the single most important file in your project. The library uses these "
        "constants at compile time to determine how much memory to allocate. If a value is wrong, "
        "the library will either waste RAM or crash at runtime.",
        S['body']
    ))

    story.append(Paragraph("4.1 Role and Service Mode Flags", S['heading2']))
    story.append(code_block(
        "// Uncomment the role(s) your project uses.\n"
        "#define DCC_COMPILE_COMMAND_STATION\n"
        "\n"
        "// Service mode methods (each requires DCC_COMPILE_COMMAND_STATION).\n"
        "#define DCC_COMPILE_SERVICE_MODE_DIRECT    // Direct byte R/W\n"
        "#define DCC_COMPILE_SERVICE_MODE_PAGED     // Paged addressing\n"
        "#define DCC_COMPILE_SERVICE_MODE_REGISTER  // Register-only (old decoders)\n"
        "#define DCC_COMPILE_SERVICE_MODE_ADDRESS   // Address-only (CV1 shortcut)"
    ))
    story.append(note_box(
        "Each DCC_COMPILE_SERVICE_MODE_* flag requires DCC_COMPILE_COMMAND_STATION. "
        "The compiler will emit a #error if you enable a service mode without the command station role."
    ))

    story.append(Paragraph("4.2 Scheduler Slot Count", S['heading2']))
    story.append(Paragraph(
        "USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT controls how many packets can be queued simultaneously. "
        "Each slot holds one dcc_scheduler_slot_t (packet bytes + metadata). The scheduler uses a "
        "static array \u2014 no dynamic allocation. A typical command station needs 16\u201324 slots.",
        S['body']
    ))

    story.append(Paragraph("4.3 Auto-Refresh Locos", S['heading2']))
    story.append(Paragraph(
        "USER_DEFINED_DCC_MAX_LOCOS sets how many locomotives the scheduler can auto-refresh. "
        "The NMRA standard requires command stations to periodically re-send speed and function "
        "commands so decoders do not time out. Each active loco consumes one refresh entry.",
        S['body']
    ))

    story.append(Paragraph("4.4 RailCom Buffer", S['heading2']))
    story.append(Paragraph(
        "USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH sets the ring buffer depth for received RailCom "
        "datagrams. The ISR writes datagrams into this buffer; DccConfig_run() drains them to "
        "your callback. A depth of 4 is sufficient for most layouts.",
        S['body']
    ))

    story.append(Paragraph("4.5 Service Mode Parameters", S['heading2']))
    story.append(make_table(
        ["Define", "Default", "Description"],
        [
            ["USER_DEFINED_DCC_SERVICE_MODE_RETRIES", "3", "Number of retry attempts before reporting failure"],
            ["USER_DEFINED_DCC_ACK_THRESHOLD_MA", "60", "Minimum current increase (mA) to detect ACK pulse"],
            ["USER_DEFINED_DCC_ACK_MIN_DURATION_US", "5000", "Minimum ACK pulse duration (\u00b5s)"],
            ["USER_DEFINED_DCC_ACK_MAX_DURATION_US", "7000", "Maximum ACK pulse duration (\u00b5s)"],
        ],
        col_widths=[3*inch, 0.7*inch, 2.3*inch]
    ))
    story.append(PageBreak())

    # --- 5. Initialization ---
    story.append(Paragraph("5. Initialization \u2014 command_station.c", S['heading1']))
    story.append(Paragraph(
        "The main file wires together the hardware drivers, the library, and your application "
        "callbacks. The pattern is always the same four steps: define the config struct, initialize "
        "hardware, initialize the library, enter the main loop.",
        S['body']
    ))

    story.append(Paragraph("5.1 The dcc_config_t Struct", S['heading2']))
    story.append(Paragraph(
        "This struct tells the DCC library how to talk to your hardware. All fields are function "
        "pointers. The library calls these when it needs to toggle a pin, start a timer, or notify "
        "you of an event.",
        S['body']
    ))
    story.append(code_block(
        "static const dcc_config_t dcc_config = {\n"
        "    // REQUIRED: common platform drivers\n"
        "    .lock_shared_resources   = &TI_DccDriver_lock_shared_resources,\n"
        "    .unlock_shared_resources = &TI_DccDriver_unlock_shared_resources,\n"
        "    .get_timestamp_usec      = &TI_DccDriver_get_timestamp_usec,\n"
        "\n"
        "    // Shared 58us timer\n"
        "    .shared_timer_start      = &TI_DccDriver_shared_timer_start,\n"
        "    .shared_timer_stop       = &TI_DccDriver_shared_timer_stop,\n"
        "\n"
        "    // RailCom cutout one-shot timer\n"
        "    .railcom_timer_start     = &TI_DccDriver_railcom_timer_start,\n"
        "    .railcom_timer_stop      = &TI_DccDriver_railcom_timer_stop,\n"
        "\n"
        "    // Per-channel hardware\n"
        "    .main_track = { ... },\n"
        "    .service_track = { ... },\n"
        "\n"
        "    // Application callbacks (NULL = no notification)\n"
        "    .on_packet_sent         = &CallbacksDcc_on_packet_sent,\n"
        "    .on_service_mode_result = &CallbacksDcc_on_service_mode_result,\n"
        "};"
    ))

    story.append(Paragraph("5.2 Per-Channel Hardware (dcc_output_hw_t)", S['heading2']))
    story.append(Paragraph(
        "Each DCC output channel (main track, service track) has its own set of hardware function "
        "pointers. The shared timer architecture means timer_start and timer_stop can be NULL \u2014 "
        "pin_toggle is used instead.",
        S['body']
    ))
    story.append(make_table(
        ["Field", "Required?", "Description"],
        [
            ["pin_toggle", "REQUIRED", "Toggle DCC output GPIO pin (ISR context)"],
            ["track_power_set", "REQUIRED", "Enable/disable H-bridge"],
            ["timer_start", "NULL w/ shared timer", "Start per-channel timer"],
            ["timer_stop", "NULL w/ shared timer", "Stop per-channel timer"],
            ["current_sense_read", "Service track only", "Read milliamps for ACK detection"],
            ["railcom", "NULL if no RailCom", "Pointer to dcc_railcom_hw_t"],
        ],
        col_widths=[1.8*inch, 1.5*inch, 2.7*inch]
    ))

    story.append(Paragraph("5.3 RailCom Hardware (dcc_railcom_hw_t)", S['heading2']))
    story.append(make_table(
        ["Field", "Description"],
        [
            ["begin_railcom_cutout", "Tristate H-bridge output at T_CS (88\u00b5s after end bit)"],
            ["end_railcom_cutout", "Restore H-bridge to normal drive at T_CE"],
            ["uart_rx_enable", "Enable 250 kbaud UART for RailCom data"],
            ["uart_rx_disable", "Disable UART after RailCom window"],
            ["uart_read", "Read one byte from RailCom UART (returns true if available)"],
            ["on_railcom_datagram_result", "Decoded datagram callback (from DccConfig_run, not ISR)"],
        ],
        col_widths=[2.5*inch, 3.5*inch]
    ))

    story.append(Paragraph("5.4 Setup and Main Loop", S['heading2']))
    story.append(code_block(
        "int main(void) {\n"
        "    SYSCFG_DL_init();                  // MCU clocks, GPIO, timers, UART\n"
        "    TI_DccDriver_initialize();         // Start free-running timestamp\n"
        "    TI_UartDriver_initialize();        // Enable UART\n"
        "    DccConfig_initialize(&dcc_config); // Hand config to library\n"
        "    UartCommandParser_initialize();    // Set up CLI\n"
        "\n"
        "    while (1) {\n"
        "        DccConfig_run();               // Library scheduling & callbacks\n"
        "        TI_UartDriver_echo_process();  // Echo characters\n"
        "        UartCommandParser_process();   // Execute commands\n"
        "    }\n"
        "}"
    ))
    story.append(note_box(
        "DccConfig_run() must be called repeatedly from your main loop. Do not use delay() or "
        "blocking calls in the loop \u2014 it stalls the scheduler and callback dispatch."
    ))
    story.append(PageBreak())

    # --- 6. ISR Architecture ---
    story.append(Paragraph("6. ISR Architecture", S['heading1']))
    story.append(Paragraph("6.1 The 58\u00b5s Shared Timer", S['heading2']))
    story.append(Paragraph(
        "A single hardware timer fires every 58\u00b5s (DCC_ONE_BIT_HALF_PERIOD_US). Its ISR calls "
        "DccConfig_58us_timer_isr(), which ticks both the main track and service track bit encoders. "
        "Each encoder counts ticks to determine when to toggle its output pin and when to advance "
        "to the next bit. Zero-bits take more ticks than one-bits.",
        S['body']
    ))
    story.append(code_block(
        "void DCC_BIT_TIMER_INST_IRQHandler(void) {\n"
        "    switch (DL_TimerA_getPendingInterrupt(DCC_BIT_TIMER_INST)) {\n"
        "        case DL_TIMER_IIDX_ZERO:\n"
        "            TI_DccDriver_timestamp_tick();\n"
        "            DccConfig_58us_timer_isr();\n"
        "            break;\n"
        "        default:\n"
        "            break;\n"
        "    }\n"
        "}"
    ))

    story.append(Paragraph("6.2 RailCom One-Shot Timer", S['heading2']))
    story.append(Paragraph(
        "A second timer operates in one-shot mode. The library starts it with different periods "
        "for each phase of the RailCom cutout: 88\u00b5s delay, Ch1 window (464\u00b5s), gap, "
        "Ch2 window, and total cutout (1544\u00b5s). The ISR calls "
        "DccConfig_railcom_oneshot_timer_isr().",
        S['body']
    ))

    story.append(Paragraph("6.3 100ms Periodic Tick", S['heading2']))
    story.append(Paragraph(
        "A 100ms timer (typically SysTick) calls DccConfig_100ms_timer_tick() for timeout checking "
        "and periodic housekeeping. This is also a good place for heartbeat LED toggling.",
        S['body']
    ))
    story.append(PageBreak())

    # --- 7. Implementing the Drivers ---
    story.append(Paragraph("7. Implementing the Drivers", S['heading1']))
    story.append(Paragraph(
        "The driver files are the only hardware-specific code in a project. If you are using the "
        "MSPM0G3507 example, these are already implemented. This section explains what each function "
        "must do when porting to a new platform.",
        S['body']
    ))

    story.append(Paragraph("7.1 Platform Driver Functions", S['heading2']))
    story.append(make_table(
        ["Function", "Context", "Contract"],
        [
            ["lock_shared_resources", "Any", "Disable interrupts or acquire mutex. Must be nestable or paired."],
            ["unlock_shared_resources", "Any", "Re-enable interrupts or release mutex."],
            ["get_timestamp_usec", "Any", "Return monotonic microsecond counter. Wrapping at 2^32 is fine."],
            ["shared_timer_start(period)", "Main", "Start periodic timer with given period. ISR calls DccConfig_58us_timer_isr()."],
            ["shared_timer_stop()", "Main", "Stop the periodic timer."],
            ["railcom_timer_start(period)", "ISR", "Start one-shot timer. ISR calls DccConfig_railcom_oneshot_timer_isr()."],
            ["railcom_timer_stop()", "ISR", "Stop one-shot timer."],
        ],
        col_widths=[2.2*inch, 0.6*inch, 3.2*inch]
    ))

    story.append(Paragraph("7.2 Pin Toggle", S['heading3']))
    story.append(Paragraph(
        "Called from ISR context by the bit encoder. Must be a single register write \u2014 no "
        "disable/re-enable of interrupts. The main track and service track each have their own "
        "pin_toggle function.",
        S['body']
    ))

    story.append(Paragraph("7.3 Track Power", S['heading3']))
    story.append(Paragraph(
        "track_power_set(true) enables the H-bridge; track_power_set(false) disables it. Called "
        "from main-loop context.",
        S['body']
    ))

    story.append(Paragraph("7.4 Current Sense", S['heading3']))
    story.append(Paragraph(
        "current_sense_read() returns milliamps (if using ADC) or a threshold value (if using "
        "comparator). The service track uses this for ACK detection during service mode programming. "
        "The demo returns 100 (HIGH) or 0 (LOW) from a digital pin.",
        S['body']
    ))

    story.append(Paragraph("7.5 Timestamp", S['heading3']))
    story.append(Paragraph(
        "get_timestamp_usec() must return a free-running microsecond counter. It does not need to "
        "be absolute \u2014 only monotonic. The library handles 32-bit wrap-around (~71 minutes). "
        "The demo increments a software counter from the 58\u00b5s timer ISR.",
        S['body']
    ))
    story.append(PageBreak())

    # --- 8. The Bit Encoder ---
    story.append(Paragraph("8. The Bit Encoder", S['heading1']))
    story.append(Paragraph(
        "The bit encoder serializes DCC packets one bit at a time from ISR context. It uses a "
        "double-buffered design: while the ISR is transmitting the current packet bit-by-bit, "
        "the main loop can load the next packet into the back buffer.",
        S['body']
    ))
    story.append(Paragraph(
        "With the shared 58\u00b5s timer architecture, the encoder counts timer ticks per half-bit. "
        "A one-bit = 1 tick (58\u00b5s), a zero-bit = 2 ticks (116\u00b5s, within NMRA spec). "
        "On each tick, the encoder decides whether to toggle the output pin based on the current "
        "bit value and tick count.",
        S['body']
    ))
    story.append(Paragraph(
        "The encoder walks through the packet structure in order: preamble bits, then for each "
        "data byte: start bit (0) + 8 data bits, and finally the end bit (1). When the last bit "
        "of a packet is sent, it signals the scheduler to load the next packet.",
        S['body']
    ))
    story.append(PageBreak())

    # --- 9. The Scheduler ---
    story.append(Paragraph("9. The Scheduler", S['heading1']))
    story.append(Paragraph("9.1 Slot Allocation", S['heading2']))
    story.append(Paragraph(
        "The scheduler manages a static array of USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT slots. "
        "Each slot holds one DCC packet (up to 6 bytes), its priority, address, tag, repeat count, "
        "and auto-refresh flag. When you call an API like "
        "DccApplicationCommandStationMainTrack_load_speed_128(), the scheduler finds an available "
        "slot (or reuses one with the same address+tag) and fills it.",
        S['body']
    ))

    story.append(Paragraph("9.2 Priority System", S['heading2']))
    story.append(make_table(
        ["Priority", "Enum Value", "Use Case"],
        [
            ["ESTOP", "Highest", "Emergency stop commands"],
            ["SPEED", "High", "Speed and direction changes"],
            ["FUNCTION", "Medium", "Function on/off commands"],
            ["ACCESSORY", "Medium", "Turnout and signal commands"],
            ["CV", "Low", "Operations mode CV programming"],
            ["IDLE", "Lowest", "Idle packets (keep-alive)"],
        ],
        col_widths=[1.5*inch, 1.5*inch, 3*inch]
    ))

    story.append(Paragraph("9.3 Duplicate Combining", S['heading2']))
    story.append(Paragraph(
        "When a new command arrives for the same (address, tag) pair as an existing slot, the "
        "scheduler overwrites the old packet with the new one instead of allocating a second slot. "
        "Tags distinguish command types: SPEED, FUNC_GROUP_1, FUNC_GROUP_2A, etc. This prevents "
        "the scheduler from filling up when rapid-fire commands arrive for the same locomotive.",
        S['body']
    ))

    story.append(Paragraph("9.4 Auto-Refresh", S['heading2']))
    story.append(Paragraph(
        "The NMRA standard requires command stations to periodically re-send speed and function "
        "commands. When a slot is marked for auto-refresh, the scheduler re-queues it after "
        "transmission instead of releasing the slot. The scheduler round-robins through active "
        "refresh entries between one-shot commands.",
        S['body']
    ))
    story.append(PageBreak())

    # --- 10. Service Mode ---
    story.append(Paragraph("10. Service Mode Programming", S['heading1']))
    story.append(Paragraph(
        "Service mode operates on a dedicated programming track. The command station sends "
        "specific packet sequences, and the decoder responds with a 6ms current pulse (ACK) "
        "when a command succeeds.",
        S['body']
    ))

    story.append(Paragraph("10.1 Direct Mode", S['heading2']))
    story.append(Paragraph(
        "Reads or writes a single CV by number. The command station sends 3 reset packets, "
        "then 5 identical direct-mode instruction packets, then monitors current for ACK. "
        "This is the most common and reliable service mode method.",
        S['body']
    ))

    story.append(Paragraph("10.2 Paged Mode", S['heading2']))
    story.append(Paragraph(
        "Divides the CV space into 256-CV pages. A page preset command selects the page, then "
        "a register write/verify accesses CVs 1\u20134 within that page. Useful for older decoders "
        "that do not support direct mode.",
        S['body']
    ))

    story.append(Paragraph("10.3 Register Mode", S['heading2']))
    story.append(Paragraph(
        "Accesses only 8 physical registers (mapped to CV1\u2013CV4, CV7, CV8, CV29). The oldest "
        "service mode method. Some very old decoders only support this.",
        S['body']
    ))

    story.append(Paragraph("10.4 Address Mode", S['heading2']))
    story.append(Paragraph(
        "A shortcut for writing CV1 (primary address). Sends address-only packets that even the "
        "simplest decoders understand. Limited to short addresses (1\u2013127).",
        S['body']
    ))

    story.append(Paragraph("10.5 ACK Detection", S['heading2']))
    story.append(Paragraph(
        "The library samples current_sense_read() on the service track during the ACK window "
        "(5\u20137ms after the last command packet). If the current exceeds "
        "USER_DEFINED_DCC_ACK_THRESHOLD_MA for at least USER_DEFINED_DCC_ACK_MIN_DURATION_US, "
        "the ACK is detected and the operation reports SUCCESS. No ACK = NO_ACK result.",
        S['body']
    ))
    story.append(PageBreak())

    # --- 11. RailCom ---
    story.append(Paragraph("11. RailCom (Brief Overview)", S['heading1']))
    story.append(Paragraph(
        "RailCom is a bi-directional communication extension to DCC. After each packet, the "
        "command station opens a cutout window (1544\u00b5s total) by tristating the H-bridge. "
        "During this window, the decoder sends data back on the track as a current-source signal.",
        S['body']
    ))
    story.append(Paragraph(
        "The cutout has two channels: Ch1 (short, 2 bytes) for broadcast address data, and Ch2 "
        "(long, up to 6 bytes) for addressed data like CV values and locomotive ID. The library "
        "handles all cutout timing through the one-shot timer, UART reception, 4/8 decoding, "
        "and datagram assembly. Your callback receives fully decoded datagrams.",
        S['body']
    ))
    story.append(make_table(
        ["Cutout Phase", "Timing", "Description"],
        [
            ["Delay (T_CS)", "88 \u00b5s after end bit", "Wait for decoder to start transmitting"],
            ["Ch1 Window", "464 \u00b5s", "Decoder broadcasts address (ID)"],
            ["Gap", "Between Ch1 and Ch2", "Short idle period"],
            ["Ch2 Window", "Up to 1080 \u00b5s", "Addressed data (CV value, status)"],
            ["Total Cutout", "1544 \u00b5s", "H-bridge re-enabled after this"],
        ],
        col_widths=[1.5*inch, 1.5*inch, 3*inch]
    ))
    story.append(PageBreak())

    # --- 12. Callbacks ---
    story.append(Paragraph("12. Callbacks \u2014 Where Your Application Lives", S['heading1']))
    story.append(Paragraph(
        "Callbacks are function pointers in the dcc_config_t struct that the library calls when "
        "specific events occur. All callbacks fire from DccConfig_run() context (main loop), NOT "
        "from ISR context, so they are safe to use UART or other slow I/O.",
        S['body']
    ))

    story.append(Paragraph("on_packet_sent", S['heading3']))
    story.append(Paragraph(
        "Called after every DCC packet is fully transmitted on the track. Useful for debugging "
        "(scope trigger, UART log) or statistics.",
        S['body']
    ))
    story.append(code_block(
        "void CallbacksDcc_on_packet_sent(const dcc_packet_t *packet) {\n"
        "    (void)packet;\n"
        "    DL_GPIO_togglePins(GPIO_DEBUG_PORT, GPIO_DEBUG_DEBUG_PIN_PIN);\n"
        "}"
    ))

    story.append(Paragraph("on_service_mode_result", S['heading3']))
    story.append(Paragraph(
        "Called when a service mode operation completes. The result enum tells you whether it "
        "succeeded, failed with no ACK, or encountered an error.",
        S['body']
    ))
    story.append(make_table(
        ["Result", "Meaning"],
        [
            ["DCC_SERVICE_MODE_SUCCESS", "CV operation completed with ACK from decoder"],
            ["DCC_SERVICE_MODE_NO_ACK", "No current pulse detected \u2014 no decoder on track?"],
            ["DCC_SERVICE_MODE_VERIFY_FAIL", "Verify command: value does not match"],
            ["DCC_SERVICE_MODE_BUSY", "Another operation is already in progress"],
            ["DCC_SERVICE_MODE_ERROR", "Internal error"],
            ["DCC_SERVICE_MODE_NOT_IN_SERVICE_MODE", "Must call SVC ENTER first"],
        ],
        col_widths=[3*inch, 3*inch]
    ))
    story.append(PageBreak())

    # --- 13. Application API Reference ---
    story.append(Paragraph("13. Application API Reference", S['heading1']))
    story.append(Paragraph(
        "These are the functions you call from your application to command locomotives, "
        "accessories, and service mode operations. All are defined in the "
        "dcc_application_command_station_*.h headers.",
        S['body']
    ))

    story.append(Paragraph("Main Track Operations", S['heading2']))
    story.append(make_table(
        ["Function", "Description"],
        [
            ["_load_speed_14(addr, speed, dir, fl)", "14-step speed command"],
            ["_load_speed_28(addr, speed, dir)", "28-step speed command"],
            ["_load_speed_128(addr, speed, dir)", "128-step speed command"],
            ["_load_emergency_stop(addr)", "E-stop specific loco"],
            ["_load_emergency_stop_all()", "Broadcast e-stop"],
            ["_load_function_group_1(addr, fl, f1-f4)", "FL and F1\u2013F4"],
            ["_load_function_group_2a(addr, f5-f8)", "F5\u2013F8"],
            ["_load_function_group_2b(addr, f9-f12)", "F9\u2013F12"],
            ["_load_function_f13_f20(addr, bits)", "F13\u2013F20"],
            ["_load_function_f21_f28(addr, bits)", "F21\u2013F28"],
            ["_load_accessory_basic(board, pair, activate)", "Turnout throw/close"],
            ["_load_accessory_extended(addr, aspect)", "Signal aspect"],
            ["_load_cv_write(addr, cv, value)", "Ops mode CV write (POM)"],
            ["_load_cv_verify(addr, cv, value)", "Ops mode CV verify"],
            ["_power_on() / _power_off()", "Track power control"],
        ],
        col_widths=[3*inch, 3*inch]
    ))

    story.append(Paragraph("Service Track Operations", S['heading2']))
    story.append(make_table(
        ["Function", "Description"],
        [
            ["_enter_service_mode()", "Switch to programming mode"],
            ["_exit_service_mode()", "Return to normal operations"],
            ["_direct_write(cv, value)", "Write CV in direct mode"],
            ["_direct_verify(cv, value)", "Verify CV in direct mode"],
            ["_direct_bit_write(cv, bit, value)", "Write single CV bit"],
            ["_direct_bit_verify(cv, bit, value)", "Verify single CV bit"],
            ["_paged_write(cv, value)", "Write CV in paged mode"],
            ["_paged_verify(cv, value)", "Verify CV in paged mode"],
            ["_register_write(reg, value)", "Write register"],
            ["_register_verify(reg, value)", "Verify register"],
            ["_address_write(addr)", "Write short address (CV1)"],
            ["_address_verify(addr)", "Verify short address"],
        ],
        col_widths=[3*inch, 3*inch]
    ))
    story.append(PageBreak())

    # --- 14. Porting ---
    story.append(Paragraph("14. Porting to a New MCU", S['heading1']))
    story.append(Paragraph(
        "Porting to a new processor means writing the driver functions that match the function "
        "pointer signatures in dcc_config_t. The library never touches hardware directly.",
        S['body']
    ))
    story.append(Paragraph(
        "1. Copy the command_station example project to a new folder.<br/>"
        "2. Replace ti_driverlib_dcc_driver.c/h with your MCU\u2019s timer and GPIO API.<br/>"
        "3. Replace ti_driverlib_uart_driver.c/h with your MCU\u2019s UART API (optional, for CLI).<br/>"
        "4. Update dcc_user_config.h buffer sizes if your MCU has limited RAM.<br/>"
        "5. Wire your ISRs to call DccConfig_58us_timer_isr(), DccConfig_railcom_oneshot_timer_isr(), "
        "and DccConfig_100ms_timer_tick().<br/>"
        "6. Build and test.",
        S['body']
    ))
    story.append(Paragraph(
        "The minimum hardware requirements are: one periodic timer (~58\u00b5s capable), one "
        "GPIO output per track channel (toggled from ISR), and a microsecond timestamp source. "
        "Optional: one-shot timer for RailCom, ADC/comparator for service mode ACK, UART for "
        "RailCom detector.",
        S['body']
    ))
    story.append(PageBreak())

    # --- 15. Unit Testing ---
    story.append(Paragraph("15. Unit Testing", S['heading1']))
    story.append(Paragraph(
        "The library includes comprehensive unit tests using Google Test (C++). Tests are located "
        "alongside the source files as *_Test.cxx files in src/dcc/.",
        S['body']
    ))
    story.append(Paragraph("Running the Tests", S['heading2']))
    story.append(code_block(
        "cd test\n"
        "mkdir build && cd build\n"
        "cmake ..\n"
        "make\n"
        "ctest --output-on-failure"
    ))
    story.append(Paragraph("Test Suites", S['heading2']))
    story.append(make_table(
        ["Test File", "What It Tests"],
        [
            ["dcc_bit_encoder_Test.cxx", "Bit timing, preamble generation, packet framing"],
            ["dcc_scheduler_Test.cxx", "Slot allocation, priority, duplicate combining, auto-refresh"],
            ["dcc_packet_encoder_Test.cxx", "Packet byte construction for all instruction types"],
            ["dcc_config_Test.cxx", "Initialization, lifecycle, ISR dispatch"],
            ["dcc_service_mode_direct_Test.cxx", "Direct mode CV read/write state machine"],
            ["dcc_service_mode_paged_Test.cxx", "Paged mode state machine"],
            ["dcc_railcom_cutout_Test.cxx", "Cutout timing state machine"],
            ["dcc_railcom_decoder_Test.cxx", "4/8 datagram decoding"],
            ["dcc_bit_decoder_Test.cxx", "Edge timing classification (one/zero)"],
            ["dcc_packet_decoder_Test.cxx", "Packet assembly, XOR validation, address matching"],
            ["dcc_cv_storage_Test.cxx", "CV read/write with decoder lock"],
        ],
        col_widths=[2.8*inch, 3.2*inch]
    ))
    story.append(Paragraph("Coverage", S['heading2']))
    story.append(code_block(
        "# Generate HTML coverage report\n"
        "cmake -DCMAKE_BUILD_TYPE=Coverage ..\n"
        "make\n"
        "ctest\n"
        "gcovr --html-details coverage.html -r ../../src/dcc/"
    ))
    story.append(PageBreak())

    # --- 16. Troubleshooting ---
    story.append(Paragraph("16. Troubleshooting", S['heading1']))
    story.append(make_table(
        ["Symptom", "Likely Cause and Fix"],
        [
            ["No DCC signal on scope", "Track power is OFF. Type POWER ON. Check pin_toggle wiring."],
            ["Signal timing wrong", "Check shared timer period is 58\u00b5s. Verify MCU clock configuration."],
            ["Decoder does not respond", "Check H-bridge wiring. Verify decoder address matches SPEED command."],
            ["SVC RESULT: NO ACK", "No decoder on programming track. Check current_sense_read wiring. "
             "Verify ACK threshold (60 mA default)."],
            ["SVC RESULT: BUSY", "A service mode operation is already running. Wait for it to complete."],
            ["Scheduler full", "Increase USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT in dcc_user_config.h."],
            ["Undefined reference to USER_DEFINED_*", "dcc_user_config.h is not on the include path. "
             "Add -I flag pointing to its directory."],
            ["Compiler #error: requires DCC_COMPILE_COMMAND_STATION",
             "You enabled a service mode flag without the command station role. "
             "Add #define DCC_COMPILE_COMMAND_STATION."],
            ["RailCom datagrams not received", "Check 250 kbaud UART wiring and dcc_railcom_hw_t struct. "
             "Verify railcom_timer_start/stop are wired."],
        ],
        col_widths=[2.2*inch, 3.8*inch]
    ))
    story.append(sp(12))
    story.append(Paragraph("Online Resources", S['heading2']))
    story.append(Paragraph(
        "Library source: github.com/jimkueneman/OpenDccCLib<br/>"
        "NMRA DCC standards: www.nmra.org/dcc",
        S['body']
    ))

    return story


# ============================================================================
# 4. Developer Guide — Decoder
# ============================================================================
def devguide_decoder():
    S = STYLES
    story = []

    # --- TITLE PAGE ---
    story.append(sp(72))
    story.append(Paragraph("OpenDccCLib", S['title']))
    story.append(Paragraph("Developer Guide \u2014 Decoder", S['subtitle']))
    story.append(Paragraph("From first project to a fully featured DCC decoder", S['description']))
    story.append(sp(8))
    story.append(Paragraph("Covers all supported platforms and IDEs", S['description']))
    story.append(PageBreak())

    # --- TOC ---
    story.append(Paragraph("Table of Contents", S['heading1']))
    toc_items = [
        "1. Introduction",
        "   1.1 What the Library Does (and Does Not Do)",
        "   1.2 Platform Support",
        "2. DCC Decoder Concepts",
        "   2.1 Signal Reception",
        "   2.2 Bit Classification",
        "   2.3 Packet Structure",
        "   2.4 Address Matching",
        "   2.5 Configuration Variables (CVs)",
        "3. Project File Structure",
        "4. dcc_user_config.h in Depth",
        "5. Initialization \u2014 decoder.c",
        "   5.1 The dcc_config_t Struct (Decoder Fields)",
        "   5.2 Edge Timestamp Ring Buffer",
        "   5.3 Setup and Main Loop",
        "6. ISR Architecture",
        "   6.1 GPIO Edge Interrupt",
        "   6.2 100ms Periodic Tick",
        "7. Implementing the Drivers",
        "   7.1 Platform Drivers",
        "   7.2 CV Storage Drivers",
        "   7.3 ACK Pulse Driver",
        "8. The Bit Decoder",
        "9. The Packet Decoder",
        "10. CV Storage and Address Matching",
        "11. Service Mode (Decoder Side)",
        "12. RailCom Responses (Brief Overview)",
        "13. Failsafe Mode",
        "14. Callbacks \u2014 Where Your Application Lives",
        "15. Porting to a New MCU",
        "16. Unit Testing",
        "17. Troubleshooting",
    ]
    for item in toc_items:
        indent = 18 if item.startswith("   ") else 0
        sty = ParagraphStyle('TOCItem', parent=S['toc'], leftIndent=indent + 12)
        story.append(Paragraph(item.strip(), sty))
    story.append(PageBreak())

    # --- 1. Introduction ---
    story.append(Paragraph("1. Introduction", S['heading1']))
    story.append(Paragraph(
        "OpenDccCLib is a C library that implements the NMRA DCC protocol for microcontrollers. "
        "This guide focuses on the decoder role \u2014 receiving DCC packets from the track and "
        "acting on speed, function, accessory, and CV programming commands.",
        S['body']
    ))
    story.append(Paragraph(
        "The library is designed for zero dynamic memory allocation \u2014 all buffers are sized "
        "at compile time. It runs on a bare-metal main loop with no OS requirement.",
        S['body']
    ))

    story.append(Paragraph("1.1 What the Library Does (and Does Not Do)", S['heading2']))
    story.append(Paragraph(
        "The library handles: edge timing classification (one vs. zero bits), packet assembly "
        "with XOR validation, address matching against CV1/CV17\u201318/CV29, command dispatch "
        "to your callbacks, CV read/write with decoder lock support, service mode ACK pulse "
        "generation, failsafe timeout detection, and optional RailCom response encoding.",
        S['body']
    ))
    story.append(Paragraph(
        "It does NOT include: motor control, LED/servo/solenoid drivers, sound generation, "
        "or any hardware-specific code. You supply driver functions via function pointers.",
        S['body']
    ))

    story.append(Paragraph("1.2 Platform Support", S['heading2']))
    story.append(make_table(
        ["Processor / Board", "Example IDE / Toolchain"],
        [
            ["TI MSPM0G3507", "Code Composer Studio / Theia"],
            ["ESP32", "Arduino IDE, PlatformIO"],
            ["STM32 (F4 and others)", "STM32 Cube IDE"],
            ["ATmega328P (Arduino Uno/Nano)", "Arduino IDE"],
            ["Any ARM Cortex-M", "GCC + Makefile"],
        ],
        col_widths=[2.5*inch, 3.5*inch]
    ))
    story.append(note_box(
        "The only hardware requirement for a decoder is a GPIO input-capture (edge-detect) "
        "interrupt and a microsecond timestamp source."
    ))
    story.append(PageBreak())

    # --- 2. DCC Decoder Concepts ---
    story.append(Paragraph("2. DCC Decoder Concepts", S['heading1']))

    story.append(Paragraph("2.1 Signal Reception", S['heading2']))
    story.append(Paragraph(
        "The DCC signal on the track rails is a bipolar square wave. The decoder picks up the "
        "signal through a bridge rectifier (for power) and an optocoupler or voltage divider "
        "(for the logic signal). The logic signal is a series of edges \u2014 each transition "
        "represents a half-bit boundary.",
        S['body']
    ))

    story.append(Paragraph("2.2 Bit Classification", S['heading2']))
    story.append(Paragraph(
        "The bit decoder measures the time between consecutive edges. If the interval is less "
        "than DCC_DECODER_HALF_BIT_THRESHOLD_US (80\u00b5s), it is a one-bit half-period. If "
        "equal to or greater than 80\u00b5s, it is a zero-bit half-period. Two matching half-periods "
        "make one complete bit.",
        S['body']
    ))
    story.append(make_table(
        ["Measurement", "Classification"],
        [
            ["Interval &lt; 80 \u00b5s", "One-bit half-period (58\u00b5s nominal)"],
            ["Interval \u2265 80 \u00b5s", "Zero-bit half-period (100\u00b5s+ nominal)"],
        ],
        col_widths=[3*inch, 3*inch]
    ))

    story.append(Paragraph("2.3 Packet Structure", S['heading2']))
    story.append(Paragraph(
        "The decoder requires at least 10 consecutive one-bits for a valid preamble (command "
        "stations send 14+). After the preamble, a zero start-bit signals the beginning of data. "
        "Data bytes are separated by zero start-bits. The packet ends with a one end-bit. The "
        "last byte is XOR of all preceding data bytes \u2014 if it doesn\u2019t match, the "
        "packet is discarded.",
        S['body']
    ))

    story.append(Paragraph("2.4 Address Matching", S['heading2']))
    story.append(Paragraph(
        "The library reads the decoder\u2019s address configuration from CVs during "
        "DccConfig_initialize():",
        S['body']
    ))
    story.append(make_table(
        ["CV", "Purpose"],
        [
            ["CV1", "Primary (short) address, 1\u2013127"],
            ["CV17\u201318", "Extended (long) address, 128\u201310239"],
            ["CV29 bit 5", "0 = use short address (CV1), 1 = use long address (CV17\u201318)"],
            ["CV29 bit 2", "RailCom enable"],
            ["CV29 bit 1", "0 = 14/28-step speed, 1 = 128-step speed"],
            ["CV29 bit 0", "0 = normal direction, 1 = reversed"],
        ],
        col_widths=[1.5*inch, 4.5*inch]
    ))

    story.append(Paragraph("2.5 Configuration Variables (CVs)", S['heading2']))
    story.append(Paragraph(
        "CVs are numbered settings stored in the decoder. The NMRA defines standard meanings "
        "for many CV numbers. CVs can be read and written in service mode (programming track) "
        "or operations mode (POM, on the main track).",
        S['body']
    ))
    story.append(make_table(
        ["CV", "Name", "Default"],
        [
            ["1", "Primary address", "3"],
            ["2", "Start voltage (Vstart)", "0"],
            ["3", "Acceleration rate", "0"],
            ["4", "Deceleration rate", "0"],
            ["5", "Maximum speed (Vhigh)", "0"],
            ["7", "Version number", "Manufacturer defined"],
            ["8", "Manufacturer ID (write 8 = factory reset)", "Manufacturer defined"],
            ["17\u201318", "Extended address", "0"],
            ["28", "RailCom configuration", "0"],
            ["29", "Configuration register", "0x06"],
        ],
        col_widths=[0.5*inch, 3*inch, 2.5*inch]
    ))
    story.append(PageBreak())

    # --- 3. Project File Structure ---
    story.append(Paragraph("3. Project File Structure", S['heading1']))
    story.append(code_block(
        "decoder/                          <- Your project folder\n"
        "  decoder.c                      <- Main entry (setup + loop + edge ISR)\n"
        "  dcc_user_config.h              <- REQUIRED: feature flags & limits\n"
        "  decoder_command_parser.c/h     <- Demo UART CLI (ADDR, HELP, etc.)\n"
        "  application_callbacks/\n"
        "    callbacks_dcc.c/h            <- Your motor/LED/servo logic\n"
        "  application_drivers/\n"
        "    ti_driverlib_dcc_driver.c/h  <- Hardware interface (timestamp, GPIO)\n"
        "    ti_driverlib_uart_driver.c/h <- UART I/O\n"
        "    ack_pulse_driver.c/h         <- ACK current load control\n"
        "  dcc_lib/                       <- Library core (do not edit)\n"
        "    dcc_config.h/c              - Stack init API\n"
        "    dcc_types.h                 - All data types\n"
        "    dcc_defines.h               - NMRA constants\n"
        "    dcc_bit_decoder.h/c         - Edge timing classification\n"
        "    dcc_packet_decoder.h/c      - Packet assembly & dispatch\n"
        "    dcc_cv_storage.h/c          - CV read/write with lock\n"
        "    dcc_railcom_encoder.h/c     - RailCom 4/8 response encoding\n"
        "    dcc_application_*.h/c       - Internal application logic"
    ))
    story.append(PageBreak())

    # --- 4. dcc_user_config.h ---
    story.append(Paragraph("4. dcc_user_config.h in Depth", S['heading1']))
    story.append(Paragraph(
        "The decoder configuration is simpler than the command station \u2014 only the role flag "
        "and function count are needed.",
        S['body']
    ))
    story.append(code_block(
        "// --- Role Selection ---\n"
        "#define DCC_COMPILE_DECODER            // Bit decoder, packet parser, CV storage\n"
        "// #define DCC_COMPILE_ACCESSORY_DECODER  // Accessory-specific features\n"
        "\n"
        "// --- Decoder Configuration ---\n"
        "#define USER_DEFINED_DCC_DECODER_MAX_FUNCTIONS  29  // F0-F28"
    ))
    story.append(make_table(
        ["Define", "Value", "Description"],
        [
            ["DCC_COMPILE_DECODER", "defined", "Enables bit decoder, packet decoder, CV storage"],
            ["DCC_COMPILE_ACCESSORY_DECODER", "optional", "Adds accessory-specific address handling"],
            ["USER_DEFINED_DCC_DECODER_MAX_FUNCTIONS", "29", "F0\u2013F28 (29 functions). Increase for F29\u2013F68."],
        ],
        col_widths=[3*inch, 0.7*inch, 2.3*inch]
    ))
    story.append(PageBreak())

    # --- 5. Initialization ---
    story.append(Paragraph("5. Initialization \u2014 decoder.c", S['heading1']))

    story.append(Paragraph("5.1 The dcc_config_t Struct (Decoder Fields)", S['heading2']))
    story.append(code_block(
        "const dcc_config_t dcc_config = {\n"
        "    // REQUIRED: common platform drivers\n"
        "    .lock_shared_resources   = &TI_DccDriver_lock_shared_resources,\n"
        "    .unlock_shared_resources = &TI_DccDriver_unlock_shared_resources,\n"
        "    .get_timestamp_usec      = &TI_DccDriver_get_timestamp_usec,\n"
        "\n"
        "    // REQUIRED: CV storage\n"
        "    .cv_read                 = &CallbacksDcc_cv_read,\n"
        "    .cv_write                = &CallbacksDcc_cv_write,\n"
        "\n"
        "    // Service mode ACK pulse\n"
        "    .start_ack_pulse         = &AckPulseDriver_start,\n"
        "    .stop_ack_pulse          = &AckPulseDriver_stop,\n"
        "\n"
        "    // Application callbacks (NULL = no notification)\n"
        "    .on_speed_command        = &CallbacksDcc_on_speed_command,\n"
        "    .on_function_command     = &CallbacksDcc_on_function_command,\n"
        "    .on_failsafe_entered    = &CallbacksDcc_on_failsafe_entered,\n"
        "    // ... more callbacks ...\n"
        "};"
    ))

    story.append(Paragraph("5.2 Edge Timestamp Ring Buffer", S['heading2']))
    story.append(Paragraph(
        "The GPIO ISR captures only a timestamp and stores it in a power-of-2 ring buffer. "
        "The main loop drains the buffer and feeds timestamps to DccConfig_decoder_edge_isr(). "
        "This keeps the ISR under ~1\u00b5s so it never overruns the 58\u00b5s half-bit period.",
        S['body']
    ))
    story.append(code_block(
        "#define EDGE_BUF_SIZE 256           /* must be power of 2 */\n"
        "#define EDGE_BUF_MASK (EDGE_BUF_SIZE - 1)\n"
        "\n"
        "static volatile uint32_t _edge_buf[EDGE_BUF_SIZE];\n"
        "static volatile uint16_t _edge_head;   /* written by ISR only  */\n"
        "static volatile uint16_t _edge_tail;   /* read by main loop only */\n"
        "\n"
        "void GROUP1_IRQHandler(void) {\n"
        "    uint32_t ts = TI_DccDriver_get_timestamp_usec();\n"
        "    uint16_t next = (_edge_head + 1) & EDGE_BUF_MASK;\n"
        "    if (next != _edge_tail) {\n"
        "        _edge_buf[_edge_head] = ts;\n"
        "        _edge_head = next;\n"
        "    }\n"
        "    // clear interrupt flags...\n"
        "}"
    ))

    story.append(Paragraph("5.3 Setup and Main Loop", S['heading2']))
    story.append(code_block(
        "int main(void) {\n"
        "    SYSCFG_DL_init();                    // MCU init\n"
        "    TI_DccDriver_initialize();           // Timestamp timer\n"
        "    TI_UartDriver_initialize();          // UART\n"
        "    AckPulseDriver_initialize();         // ACK current load\n"
        "    CallbacksDcc_initialize();           // CV defaults\n"
        "    DccConfig_initialize(&dcc_config);   // Library init (reads CVs)\n"
        "\n"
        "    while (1) {\n"
        "        _drain_edge_buffer();            // Feed edges to bit decoder\n"
        "        DccConfig_run();                 // Library housekeeping\n"
        "        CallbacksDcc_drain();            // Push RECV lines to UART\n"
        "        TI_UartDriver_echo_process();\n"
        "        DecoderCommandParser_process();\n"
        "    }\n"
        "}"
    ))
    story.append(note_box(
        "_drain_edge_buffer() must be called frequently. If the ring buffer overflows, edges are "
        "silently dropped and packets may be missed."
    ))
    story.append(PageBreak())

    # --- 6. ISR Architecture ---
    story.append(Paragraph("6. ISR Architecture", S['heading1']))

    story.append(Paragraph("6.1 GPIO Edge Interrupt", S['heading2']))
    story.append(Paragraph(
        "The primary ISR fires on every rising and falling edge of the DCC input signal. Its only "
        "job is to capture a microsecond timestamp into the ring buffer. No decoding, no packet "
        "processing \u2014 just timestamp and return. This keeps ISR execution under 1\u00b5s.",
        S['body']
    ))
    story.append(Paragraph(
        "The demo supports two physical inputs (PB1 for main track, PB4 for service track) with "
        "a track-select mux pin (PB17). Both are on the same GPIO port so they share one ISR.",
        S['body']
    ))

    story.append(Paragraph("6.2 100ms Periodic Tick", S['heading2']))
    story.append(Paragraph(
        "A 100ms timer (SysTick in the demo) provides a heartbeat. The decoder uses this for "
        "LED toggling. The library does not currently require a periodic tick for the decoder role, "
        "but it is good practice to have one for watchdog and failsafe timing.",
        S['body']
    ))
    story.append(PageBreak())

    # --- 7. Implementing the Drivers ---
    story.append(Paragraph("7. Implementing the Drivers", S['heading1']))

    story.append(Paragraph("7.1 Platform Drivers", S['heading2']))
    story.append(make_table(
        ["Function", "Context", "Contract"],
        [
            ["lock_shared_resources", "Any", "Disable interrupts or acquire mutex"],
            ["unlock_shared_resources", "Any", "Re-enable interrupts or release mutex"],
            ["get_timestamp_usec", "Any", "Return monotonic microsecond counter"],
        ],
        col_widths=[2.2*inch, 0.6*inch, 3.2*inch]
    ))

    story.append(Paragraph("7.2 CV Storage Drivers", S['heading2']))
    story.append(Paragraph(
        "The library reads and writes CVs through cv_read() and cv_write() function pointers. "
        "These are REQUIRED \u2014 the library reads CV1/CV17\u201318/CV29 during initialization "
        "to determine the decoder\u2019s address.",
        S['body']
    ))
    story.append(code_block(
        "bool CallbacksDcc_cv_read(uint16_t cv_number, uint8_t *value) {\n"
        "    if (cv_number >= CV_ARRAY_SIZE) return false;\n"
        "    *value = cv_storage[cv_number];\n"
        "    return true;\n"
        "}\n"
        "\n"
        "bool CallbacksDcc_cv_write(uint16_t cv_number, uint8_t value) {\n"
        "    if (cv_number >= CV_ARRAY_SIZE) return false;\n"
        "    cv_storage[cv_number] = value;\n"
        "    return true;\n"
        "}"
    ))
    story.append(note_box(
        "The demo stores CVs in a RAM array that resets on power cycle. For a real product, "
        "replace with Flash or EEPROM read/write. The cv_write function should write to "
        "non-volatile storage."
    ))

    story.append(Paragraph("7.3 ACK Pulse Driver", S['heading2']))
    story.append(Paragraph(
        "During service mode programming, the decoder must respond with a 6ms current pulse "
        "(ACK) when a command succeeds. The library calls start_ack_pulse() to turn on the "
        "current load and stop_ack_pulse() to turn it off (after 6ms, timed by the library). "
        "Set both to NULL if your decoder does not support service mode.",
        S['body']
    ))
    story.append(PageBreak())

    # --- 8. The Bit Decoder ---
    story.append(Paragraph("8. The Bit Decoder", S['heading1']))
    story.append(Paragraph(
        "The bit decoder receives raw edge timestamps from DccConfig_decoder_edge_isr() and "
        "classifies each pair of half-periods into one-bits or zero-bits.",
        S['body']
    ))
    story.append(Paragraph(
        "For each edge, it calculates the interval since the previous edge. If the interval is "
        "less than DCC_DECODER_HALF_BIT_THRESHOLD_US (80\u00b5s), it is a short half-period (one-bit). "
        "If \u226580\u00b5s, it is a long half-period (zero-bit). Two consecutive matching "
        "half-periods form one complete bit. If the two halves disagree (one short + one long), "
        "the bit is invalid and the decoder resets to preamble search.",
        S['body']
    ))
    story.append(Paragraph(
        "The NMRA allows stretching the first half of a zero-bit up to 12000\u00b5s for "
        "decoder processing time. The bit decoder accommodates this by accepting any long "
        "half-period as a potential zero-bit first half.",
        S['body']
    ))
    story.append(PageBreak())

    # --- 9. The Packet Decoder ---
    story.append(Paragraph("9. The Packet Decoder", S['heading1']))
    story.append(Paragraph(
        "The packet decoder operates as a state machine with these states:",
        S['body']
    ))
    story.append(make_table(
        ["State", "Description"],
        [
            ["PREAMBLE", "Counting consecutive one-bits (need \u226510)"],
            ["START_BIT", "Waiting for the first zero-bit after preamble"],
            ["DATA", "Collecting 8 data bits into a byte"],
            ["END_OR_START", "After each byte: one-bit = end of packet, zero-bit = start of next byte"],
        ],
        col_widths=[2*inch, 4*inch]
    ))
    story.append(Paragraph(
        "When a complete packet is received, the decoder validates the XOR error check byte. "
        "If valid, it checks the address against the decoder\u2019s configured address (from CVs). "
        "Broadcast packets (address 0) are always accepted. If the address matches, the decoder "
        "parses the instruction byte(s) and dispatches to the appropriate callback.",
        S['body']
    ))
    story.append(PageBreak())

    # --- 10. CV Storage ---
    story.append(Paragraph("10. CV Storage and Address Matching", S['heading1']))
    story.append(Paragraph(
        "The library maintains an internal shadow copy of the critical CVs (CV1, CV17\u201318, CV29) "
        "for fast address matching. When a CV write command changes one of these CVs, the library "
        "updates its shadow copy automatically.",
        S['body']
    ))
    story.append(Paragraph(
        "CV8 has special behavior: writing the value 8 to CV8 triggers a factory reset. The library "
        "calls your cv_write function to reset all CVs to their default values.",
        S['body']
    ))
    story.append(Paragraph("Decoder Lock", S['heading2']))
    story.append(Paragraph(
        "The library supports NMRA decoder lock (CV15/CV16). When locked, CV write commands are "
        "ignored unless the lock value matches. This prevents accidental programming of the wrong "
        "decoder when multiple decoders share the same address on the programming track.",
        S['body']
    ))
    story.append(PageBreak())

    # --- 11. Service Mode (Decoder Side) ---
    story.append(Paragraph("11. Service Mode (Decoder Side)", S['heading1']))
    story.append(Paragraph(
        "When the decoder is on the programming track and receives service mode packets, the "
        "library handles the protocol automatically:",
        S['body']
    ))
    story.append(Paragraph(
        "1. The library recognizes the service mode packet sequence (reset + command packets).<br/>"
        "2. For write commands: the library calls cv_write() to store the value, then calls "
        "start_ack_pulse() to trigger the 6ms ACK current pulse.<br/>"
        "3. For verify commands: the library calls cv_read() and compares the value. If it matches, "
        "it triggers the ACK pulse.<br/>"
        "4. For bit operations: same as verify/write but at the individual bit level.<br/>"
        "5. The library calls stop_ack_pulse() after 6ms (DCC_ACK_PULSE_DURATION_US).",
        S['body']
    ))
    story.append(Paragraph(
        "Your on_cv_write_command and on_cv_verify_command callbacks fire after the operation "
        "completes, giving you a chance to log or react to CV changes.",
        S['body']
    ))
    story.append(PageBreak())

    # --- 12. RailCom ---
    story.append(Paragraph("12. RailCom Responses (Brief Overview)", S['heading1']))
    story.append(Paragraph(
        "If railcom_tx_pin_set is non-NULL, the library can send RailCom responses back to the "
        "command station during the cutout window. The library bit-bangs 4/8-encoded bytes at "
        "250 kbaud (4\u00b5s per bit) through a GPIO connected to a current-source circuit.",
        S['body']
    ))
    story.append(Paragraph(
        "The decoder must also blank the edge-detect interrupt during the cutout to avoid false "
        "edges. Set decoder_edge_irq_enable to a function that enables/disables the GPIO interrupt.",
        S['body']
    ))
    story.append(make_table(
        ["Config Field", "Purpose"],
        [
            ["railcom_tx_pin_set(high)", "Drive GPIO high/low for RailCom current-source"],
            ["decoder_edge_irq_enable(enabled)", "Blank edge-detect during cutout window"],
            ["railcom_uart_write", "Alternative: send via hardware UART (NULL in most cases)"],
        ],
        col_widths=[2.5*inch, 3.5*inch]
    ))
    story.append(PageBreak())

    # --- 13. Failsafe ---
    story.append(Paragraph("13. Failsafe Mode", S['heading1']))
    story.append(Paragraph(
        "The NMRA requires decoders to enter a safe state if no valid DCC packet is received for "
        "a defined period. The library monitors the time since the last valid packet. If it exceeds "
        "the failsafe timeout, it calls on_failsafe_entered(). When valid packets resume, it calls "
        "on_failsafe_exited().",
        S['body']
    ))
    story.append(Paragraph(
        "In your on_failsafe_entered callback, you should stop the motor and set all outputs to "
        "a safe state. Possible causes: track power turned off, wiring fault, decoder fell off the "
        "track, or command station crashed.",
        S['body']
    ))
    story.append(code_block(
        "void CallbacksDcc_on_failsafe_entered(void) {\n"
        "    set_motor_speed(0, true);  // Stop motor\n"
        "    set_all_lights(false);     // Lights off\n"
        "}\n"
        "\n"
        "void CallbacksDcc_on_failsafe_exited(void) {\n"
        "    // Normal operation resumes.\n"
        "    // Speed/function commands will arrive shortly.\n"
        "}"
    ))
    story.append(PageBreak())

    # --- 14. Callbacks ---
    story.append(Paragraph("14. Callbacks \u2014 Where Your Application Lives", S['heading1']))
    story.append(Paragraph(
        "All your application logic lives in callbacks_dcc.c. Each callback fires when the library "
        "decodes the corresponding DCC command. The demo logs to UART; replace with your "
        "motor/LED/servo control.",
        S['body']
    ))
    story.append(make_table(
        ["Callback", "Signature", "When It Fires"],
        [
            ["on_speed_command", "(addr, speed, dir, mode)", "Speed + direction"],
            ["on_emergency_stop_command", "(addr)", "E-stop for this address"],
            ["on_function_command", "(addr, func_num, state)", "Function on/off (F0\u2013F68)"],
            ["on_accessory_basic_command", "(board, pair, activate)", "Turnout throw/close"],
            ["on_accessory_extended_command", "(addr, aspect)", "Signal aspect"],
            ["on_cv_write_command", "(cv, value, svc_mode)", "CV was written"],
            ["on_cv_verify_command", "(cv, value, svc_mode)", "CV verify received"],
            ["on_cv_bit_command", "(cv, bit, value, svc_mode)", "CV bit manipulation"],
            ["on_consist_command", "(addr, ca, dir_normal)", "Consist setup/clear"],
            ["on_binary_state_short", "(addr, state, active)", "Binary state (1\u2013127)"],
            ["on_binary_state_long", "(addr, state, active)", "Binary state (1\u201332767)"],
            ["on_analog_function", "(addr, output, value)", "Analog output (0\u2013255)"],
            ["on_speed_restriction", "(addr, enabled, limit)", "Layout speed limit"],
            ["on_failsafe_entered", "()", "No valid DCC packets"],
            ["on_failsafe_exited", "()", "Valid DCC resumed"],
        ],
        col_widths=[2*inch, 2*inch, 2*inch]
    ))
    story.append(PageBreak())

    # --- 15. Porting ---
    story.append(Paragraph("15. Porting to a New MCU", S['heading1']))
    story.append(Paragraph(
        "Porting the decoder to a new MCU requires these steps:",
        S['body']
    ))
    story.append(Paragraph(
        "1. Copy the decoder example project.<br/>"
        "2. Replace ti_driverlib_dcc_driver.c/h with your MCU\u2019s GPIO and timer API.<br/>"
        "3. Replace ack_pulse_driver.c/h with your MCU\u2019s current load control.<br/>"
        "4. Replace ti_driverlib_uart_driver.c/h for debug output (optional).<br/>"
        "5. Wire the GPIO edge-detect ISR to capture timestamps into the ring buffer.<br/>"
        "6. Provide a microsecond timestamp source (hardware timer or DWT cycle counter).<br/>"
        "7. Build and test with a known-good command station.",
        S['body']
    ))
    story.append(Paragraph(
        "Minimum hardware: one GPIO with edge-detect interrupt, one microsecond timer or counter. "
        "Optional: GPIO for ACK current load, GPIO for RailCom current-source.",
        S['body']
    ))
    story.append(PageBreak())

    # --- 16. Unit Testing ---
    story.append(Paragraph("16. Unit Testing", S['heading1']))
    story.append(Paragraph(
        "The decoder modules have comprehensive unit tests:",
        S['body']
    ))
    story.append(make_table(
        ["Test File", "What It Tests"],
        [
            ["dcc_bit_decoder_Test.cxx", "Edge timing classification, threshold handling, noise rejection"],
            ["dcc_packet_decoder_Test.cxx", "Preamble detection, byte assembly, XOR validation, address matching"],
            ["dcc_cv_storage_Test.cxx", "CV read/write, decoder lock, factory reset"],
            ["dcc_railcom_encoder_Test.cxx", "4/8 encoding for RailCom responses"],
        ],
        col_widths=[2.8*inch, 3.2*inch]
    ))
    story.append(code_block(
        "cd test\n"
        "mkdir build && cd build\n"
        "cmake ..\n"
        "make\n"
        "ctest --output-on-failure"
    ))
    story.append(PageBreak())

    # --- 17. Troubleshooting ---
    story.append(Paragraph("17. Troubleshooting", S['heading1']))
    story.append(make_table(
        ["Symptom", "Likely Cause and Fix"],
        [
            ["No packets decoded", "Check DCC input wiring. Verify GPIO edge-detect interrupt is enabled. "
             "Check ring buffer is being drained."],
            ["Packets decoded but callbacks not firing", "Address mismatch. Check CV1/CV29 defaults in "
             "CallbacksDcc_initialize(). Try broadcast address 0."],
            ["Corrupted packets (XOR errors)", "Noise on DCC input. Add filtering/debounce. Check signal "
             "levels (must be clean 3.3V logic)."],
            ["ACK not detected by command station", "Check ACK current load circuit. Verify 6ms pulse is "
             "being generated. Check current threshold on command station side."],
            ["Failsafe fires immediately", "No DCC signal reaching the decoder. Check wiring and "
             "track power."],
            ["Functions work but speed doesn\u2019t", "CV29 speed mode mismatch. Check CV29 bit 1 "
             "(0 = 14/28 step, 1 = 128 step) matches command station."],
            ["Undefined reference to USER_DEFINED_*", "dcc_user_config.h not on include path. "
             "Add -I flag."],
            ["Wrong address after power cycle", "CV storage is RAM-only (demo default). Replace with "
             "Flash/EEPROM for persistence."],
        ],
        col_widths=[2.2*inch, 3.8*inch]
    ))
    story.append(sp(12))
    story.append(Paragraph("Online Resources", S['heading2']))
    story.append(Paragraph(
        "Library source: github.com/jimkueneman/OpenDccCLib<br/>"
        "NMRA DCC standards: www.nmra.org/dcc",
        S['body']
    ))

    return story


# ============================================================================
# 5. Brochure
# ============================================================================
def brochure():
    S = STYLES
    story = []

    # --- PAGE 1 ---
    story.append(Paragraph("v1.0.0", ParagraphStyle('Ver', fontName='Helvetica',
                  fontSize=9, textColor=BODY_GRAY, spaceAfter=2)))
    story.append(Paragraph("OpenDccCLib", S['title']))
    story.append(Paragraph("Full NMRA DCC Protocol Stack in C", S['subtitle']))
    story.append(Paragraph("Runs on any processor. No OS. No heap. Just clean C.", S['description']))
    story.append(sp(16))

    story.append(Paragraph("What is DCC?", S['heading2']))
    story.append(Paragraph(
        "DCC (Digital Command Control) is the NMRA standard for controlling model railroad "
        "locomotives and accessories. A command station sends addressed speed, function, and "
        "programming commands over the track rails as a modulated square wave. Decoders in "
        "locomotives and accessories receive and act on those commands.",
        S['body']
    ))
    story.append(Paragraph(
        "DCC supports short and long addresses, 128-step speed control, up to 68 function outputs, "
        "CV-based configuration, service mode programming, and bi-directional communication via "
        "RailCom.",
        S['body']
    ))
    story.append(sp(8))

    story.append(Paragraph("What is OpenDccCLib?", S['heading2']))
    story.append(Paragraph(
        "The full NMRA DCC protocol stack, written in plain C.",
        S['body']
    ))
    story.append(note_box(
        "\u201cNo dynamic memory. No OS. No external dependencies. If your chip has a C compiler "
        "and a timer peripheral, it can run OpenDccCLib.\u201d"
    ))
    story.append(sp(8))

    story.append(Paragraph("Key Features", S['heading2']))
    features = [
        ("<b>Zero heap allocation</b> \u2014 Every buffer is a static array sized at compile time. "
         "No malloc, no fragmentation, no surprises."),
        ("<b>Command station + decoder in one library</b> \u2014 Build a command station, a decoder, "
         "or both from the same source tree. Compile-time feature flags select what you need."),
        ("<b>Full NMRA service mode</b> \u2014 Direct, paged, register, and address mode programming. "
         "ACK detection with configurable thresholds."),
        ("<b>RailCom support</b> \u2014 Command station cutout timing with one-shot timer state machine. "
         "Decoder-side 4/8-encoded responses via GPIO bit-bang. Full datagram decoding."),
        ("<b>OS-free by design</b> \u2014 No RTOS required. A single run() call per main-loop tick "
         "drives the entire protocol engine."),
        ("<b>Dependency injection</b> \u2014 Hardware callbacks are function pointers wired at init time. "
         "Swap platforms by changing the driver layer \u2014 the protocol code is untouched."),
        ("<b>Compliance-tested</b> \u2014 23 gTest suites covering bit encoding, packet scheduling, "
         "service mode state machines, RailCom, CV storage, and decoder packet parsing."),
        ("<b>BSD 2-Clause license</b> \u2014 Use in open-source or commercial products with no "
         "royalties and no strings."),
    ]
    for f in features:
        story.append(Paragraph(f, S['body']))
    story.append(PageBreak())

    # --- PAGE 2 ---
    story.append(Paragraph("Protocol Coverage", S['heading2']))
    story.append(make_table(
        ["Module", "Details"],
        [
            ["Bit Encoder", "NMRA-compliant 58\u00b5s one-bits, 100\u00b5s zero-bits, preamble generation, "
             "double-buffered packet serialization from ISR"],
            ["Packet Encoder", "Speed (14/28/128), functions (F0\u2013F68), basic & extended accessories, "
             "consist, binary state, analog, CV access (POM)"],
            ["Scheduler", "Static slot array with priority (ESTOP > SPEED > FUNCTION > ACCESSORY > CV > IDLE), "
             "duplicate combining via (address, tag), NMRA auto-refresh"],
            ["Service Mode", "Direct, paged, register, and address mode. Configurable ACK threshold, "
             "pulse duration, and retry count"],
            ["RailCom Cutout", "One-shot timer state machine: 88\u00b5s delay, Ch1 window, gap, Ch2 window, "
             "1544\u00b5s total. H-bridge tristate control"],
            ["RailCom Decoder", "4/8 datagram decoding from 250 kbaud UART. Channel 1 and Channel 2 assembly"],
            ["Bit Decoder", "Edge timing classification with 80\u00b5s threshold. NMRA-compliant "
             "stretched zero-bit support"],
            ["Packet Decoder", "Preamble detection (\u226510 bits), byte assembly, XOR validation, "
             "address matching, instruction dispatch"],
            ["CV Storage", "Read/write through function pointers. Decoder lock (CV15/CV16). "
             "Factory reset via CV8"],
            ["RailCom Encoder", "4/8 bit-bang encoding for decoder-side RailCom responses"],
            ["Failsafe", "Timeout detection, enter/exit callbacks for safe motor stop"],
        ],
        col_widths=[1.5*inch, 4.5*inch]
    ))
    story.append(sp(12))

    story.append(Paragraph("Demo Platforms, Tools & Getting Started", S['heading2']))
    story.append(Paragraph(
        "Two ready-to-run example projects ship with the library. Porting to a new platform means "
        "writing just two driver files \u2014 a timer/GPIO driver and optionally a UART driver \u2014 "
        "then everything else runs unchanged.",
        S['body']
    ))
    story.append(make_table(
        ["Example", "Platform", "Toolchain / IDE"],
        [
            ["Command Station", "TI MSPM0G3507 LaunchPad", "Code Composer Studio / Theia"],
            ["Decoder", "TI MSPM0G3507 LaunchPad", "Code Composer Studio / Theia"],
        ],
        col_widths=[1.5*inch, 2.5*inch, 2*inch]
    ))
    story.append(sp(12))

    story.append(Paragraph("Architecture Highlights", S['heading2']))
    for item in [
        "Static buffer pools sized at compile time \u2014 no malloc, no fragmentation",
        "Hardware access via nullable function-pointer interfaces (DI pattern)",
        "Single-tick main loop \u2014 no threads, no mutexes on single-core MCUs",
        "Shared 58\u00b5s timer drives both main and service track encoders simultaneously",
        "Edge-buffered decoder ISR under 1\u00b5s \u2014 never overruns the 58\u00b5s half-bit period",
        "Compile-time role selection: command station, decoder, or both",
    ]:
        story.append(bullet(item))
    story.append(PageBreak())

    # --- PAGE 3 ---
    story.append(Paragraph("Unit Test Coverage", S['heading2']))
    story.append(make_table(
        ["Metric", "Detail"],
        [
            ["Test suites", "23 gTest suites"],
            ["Source files", "43 .c/.h files in src/dcc/"],
            ["Source lines", "~13,500 lines"],
        ],
        col_widths=[2*inch, 4*inch]
    ))
    story.append(sp(4))
    story.append(Paragraph(
        "23 gTest suites cover the full source tree (bit encoder, scheduler, packet encoder, "
        "service mode state machines, RailCom cutout, RailCom decoder, bit decoder, packet decoder, "
        "CV storage, RailCom encoder, config lifecycle, application API).",
        S['body']
    ))
    story.append(sp(16))

    story.append(Paragraph("Getting Started", S['heading2']))
    story.append(Paragraph(
        "1. <b>Quick Start (Command Station)</b> \u2014 Import "
        "applications/ti_theia/mspm03507_launchpad/command_station/ into CCS, build, flash, "
        "and type POWER ON. DCC packets on the track in minutes.",
        S['body']
    ))
    story.append(Paragraph(
        "2. <b>Quick Start (Decoder)</b> \u2014 Import "
        "applications/ti_theia/mspm03507_launchpad/decoder/ into CCS, build, flash, and connect "
        "to a DCC source. Decoded commands print over UART.",
        S['body']
    ))
    story.append(Paragraph(
        "3. <b>New platform</b> \u2014 Copy an example project. Rewrite the driver files for your "
        "MCU\u2019s timer and GPIO. The entire protocol engine runs unchanged.",
        S['body']
    ))
    story.append(sp(16))

    story.append(Paragraph("Documentation & Repository", S['heading2']))
    story.append(Paragraph(
        "<b>Quick Start Guide (Command Station)</b> \u2014 Step-by-step MSPM0G3507 walkthrough<br/>"
        "<b>Quick Start Guide (Decoder)</b> \u2014 Step-by-step decoder walkthrough<br/>"
        "<b>Developer Guide (Command Station)</b> \u2014 Config struct, scheduler, bit encoder, "
        "service mode, RailCom, porting<br/>"
        "<b>Developer Guide (Decoder)</b> \u2014 Bit decoder, packet decoder, CV storage, callbacks, "
        "porting<br/>"
        "<b>API Reference</b> \u2014 Doxygen-generated HTML",
        S['body']
    ))
    story.append(sp(4))
    story.append(Paragraph(
        "github.com/jimkueneman/OpenDccCLib \u2014 BSD 2-Clause License",
        S['body']
    ))
    story.append(sp(24))
    story.append(note_box("Built for model railroaders, by a model railroader."))

    return story


def build_brochure():
    """Build the brochure with centered footer instead of page numbers."""
    filepath = os.path.join(os.path.dirname(__file__), "OpenDccCLib_Brochure.pdf")

    def on_page(canvas_obj, doc):
        canvas_obj.saveState()
        canvas_obj.setFont('Helvetica', 8)
        canvas_obj.setFillColor(HexColor("#999999"))
        w = letter[0]
        canvas_obj.drawCentredString(w / 2, 0.4 * inch,
            "OpenDccCLib  |  Full DCC Protocol Stack for NMRA Digital Command Control")
        canvas_obj.drawCentredString(w / 2, 0.28 * inch,
            "jimkueneman.github.io/OpenDccCLib  \u2022  BSD 2-Clause  \u2022  Author: Jim Kueneman")
        canvas_obj.restoreState()

    doc = SimpleDocTemplate(filepath, pagesize=letter,
                            topMargin=0.6*inch, bottomMargin=0.7*inch,
                            leftMargin=inch, rightMargin=inch)
    story = brochure()
    doc.build(story, onFirstPage=on_page, onLaterPages=on_page)
    print(f"  Created: {filepath}")


# ============================================================================
# Main — generate all five PDFs
# ============================================================================
if __name__ == "__main__":
    print("Generating OpenDccCLib documentation PDFs...")
    print()

    build_pdf(
        "QuickStartGuide_CommandStation.pdf",
        "OpenDccCLib | Quick Start Guide \u2014 Command Station",
        qsg_command_station
    )

    build_pdf(
        "QuickStartGuide_Decoder.pdf",
        "OpenDccCLib | Quick Start Guide \u2014 Decoder",
        qsg_decoder
    )

    build_pdf(
        "DeveloperGuide_CommandStation.pdf",
        "OpenDccCLib | Developer Guide \u2014 Command Station",
        devguide_command_station
    )

    build_pdf(
        "DeveloperGuide_Decoder.pdf",
        "OpenDccCLib | Developer Guide \u2014 Decoder",
        devguide_decoder
    )

    build_brochure()

    print()
    print("Done! All five PDFs generated in the documentation/ folder.")
