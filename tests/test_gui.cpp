#include <QApplication>
#include <QCoreApplication>
#include <QProcess>
#include <QTest>
#include <QSignalSpy>
#include <QFileInfo>
#include <QFile>
#include <QScrollBar>
#include <QTextStream>
#include <iostream>
#include <cstdlib>
#include "../src/gui/mainwindow.h"
#include "../src/gui/simulatormanager.h"
#include "../src/gui/codeeditor.h"
#include "../src/gui/listingwidget.h"
#include "../src/gui/registersview.h"
#include "../src/gui/memoryview.h"
#include "../src/gui/stackview.h"
#include "../src/gui/terminalwidget.h"
#include "../src/gui/hardwarewidget.h"
#include "../src/gui/easybinwidget.h"
#include "../src/gui/helpwidget.h"
#include "../src/gui/updater.h"
#include "../src/simulator/proto.h"
#include <QCryptographicHash>

#define TEST_CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "[GUI TEST FAILED] " << #cond << " at line " << __LINE__ << "\n"; \
            return 1; \
        } \
    } while (0)

extern int seg7loc, LEDloc, switchLoc, pbLoc;
extern int irq;
extern char *memory;
extern bool simhalt_on;

struct InstRecRef {
    int mask;
    int val;
    char name[13];
};
extern int offsets[];
extern struct InstRecRef inst_arr[];

static int testSimExecFsmTransitionsAndInvariants()
{
    std::cout << "[GUI TEST] Running testSimExecFsmTransitionsAndInvariants...\n";
    SimulatorManager sim;

    // Invariant at start: Unloaded
    TEST_CHECK(sim.execState() == SimExecState::Unloaded);
    TEST_CHECK(!sim.isLoaded());
    TEST_CHECK(!sim.isRunning());

    // 1. Unloaded -> IdleHalted via LoadSuccess
    sim.applyExecEvent(SimExecEvent::LoadSuccess);
    TEST_CHECK(sim.execState() == SimExecState::IdleHalted);
    TEST_CHECK(sim.isLoaded());
    TEST_CHECK(!sim.isRunning());

    // 2. IdleHalted -> SteppingInto via CmdTraceInto
    sim.applyExecEvent(SimExecEvent::CmdTraceInto);
    TEST_CHECK(sim.execState() == SimExecState::SteppingInto);
    TEST_CHECK(sim.isLoaded());

    // 3. SteppingInto -> Paused via StepCompleted
    sim.applyExecEvent(SimExecEvent::StepCompleted);
    TEST_CHECK(sim.execState() == SimExecState::Paused);
    TEST_CHECK(sim.isLoaded());
    TEST_CHECK(!sim.isRunning());

    // 4. Paused -> SteppingOver via CmdStepOver
    sim.applyExecEvent(SimExecEvent::CmdStepOver);
    TEST_CHECK(sim.execState() == SimExecState::SteppingOver);
    TEST_CHECK(sim.isLoaded());

    // 5. SteppingOver -> RunningContinuous via CmdRun
    sim.applyExecEvent(SimExecEvent::CmdRun);
    TEST_CHECK(sim.execState() == SimExecState::RunningContinuous);
    TEST_CHECK(sim.isLoaded());
    TEST_CHECK(sim.isRunning());

    // 6. RunningContinuous -> WaitingTerminalInput via InputRequested
    sim.applyExecEvent(SimExecEvent::InputRequested);
    TEST_CHECK(sim.execState() == SimExecState::WaitingTerminalInput);
    TEST_CHECK(sim.isLoaded());
    TEST_CHECK(!sim.isRunning());

    // 7. WaitingTerminalInput -> IdleHalted via SimHaltHit
    sim.applyExecEvent(SimExecEvent::SimHaltHit);
    TEST_CHECK(sim.execState() == SimExecState::IdleHalted);
    TEST_CHECK(sim.isLoaded());
    TEST_CHECK(!sim.isRunning());

    return 0;
}

static int testOpcodeDecodeLutExhaustive64kEquivalence()
{
    std::cout << "[GUI TEST] Running testOpcodeDecodeLutExhaustive64kEquivalence (65,536 opcodes)...\n";

    auto referenceDecode = [](uint16_t op) -> int {
        int start = offsets[(op >> 12) & 0x0F];
        int finish = offsets[((op >> 12) & 0x0F) + 1] - 1;
        for (int i = start; i <= finish; ++i) {
            if ((op & ~static_cast<uint32_t>(inst_arr[i].mask)) == static_cast<uint32_t>(inst_arr[i].val)) {
                return i;
            }
        }
        return -1;
    };

    for (uint32_t op = 0; op <= 0xFFFF; ++op) {
        uint16_t opcode = static_cast<uint16_t>(op);
        int refIdx = referenceDecode(opcode);
        int lutIdx = decodeOpcodePrimaryIndex(opcode);
        TEST_CHECK(refIdx == lutIdx);
    }

    return 0;
}

static int testMemoryCursorFsmNavigationAndColumnMapping()
{
    std::cout << "[GUI TEST] Running testMemoryCursorFsmNavigationAndColumnMapping...\n";
    MemoryCursorFSM fsm;
    uint32_t baseAddress = 0x00001000;
    const int maxRows = 32;

    fsm.row = 0;
    fsm.byteIndex = 0;
    fsm.substate = MemEditSubstate::HexHighNybble;

    for (int step = 0; step < 32; ++step) {
        int expectedByte = step / 2;
        MemEditSubstate expectedSubstate = (step % 2 == 0)
            ? MemEditSubstate::HexHighNybble
            : MemEditSubstate::HexLowNybble;

        TEST_CHECK(fsm.byteIndex == expectedByte);
        TEST_CHECK(fsm.substate == expectedSubstate);

        int col = fsm.toScreenColumn();
        int offset = col - 10;
        TEST_CHECK(offset % 3 != 2);
        TEST_CHECK(col == 10 + expectedByte * 3 + static_cast<int>(expectedSubstate));

        if (step < 31) {
            fsm.moveRight(maxRows, baseAddress);
        }
    }

    TEST_CHECK(fsm.row == 0 && fsm.byteIndex == 15 && fsm.substate == MemEditSubstate::HexLowNybble);
    fsm.moveRight(maxRows, baseAddress);
    TEST_CHECK(fsm.row == 1 && fsm.byteIndex == 0 && fsm.substate == MemEditSubstate::HexHighNybble);

    fsm.row = maxRows - 1;
    fsm.byteIndex = 15;
    fsm.substate = MemEditSubstate::HexLowNybble;
    uint32_t oldBase = baseAddress;
    fsm.moveRight(maxRows, baseAddress, 0x01000000);
    TEST_CHECK(baseAddress == oldBase + 16);
    TEST_CHECK(fsm.row == maxRows - 1 && fsm.byteIndex == 0 && fsm.substate == MemEditSubstate::HexHighNybble);

    fsm.row = 0;
    fsm.byteIndex = 0;
    fsm.substate = MemEditSubstate::HexHighNybble;
    oldBase = baseAddress;
    fsm.moveLeft(maxRows, baseAddress);
    TEST_CHECK(baseAddress == oldBase - 16);
    TEST_CHECK(fsm.row == 0 && fsm.byteIndex == 15 && fsm.substate == MemEditSubstate::HexLowNybble);

    for (int b = 0; b < 16; ++b) {
        fsm.byteIndex = b;
        fsm.substate = MemEditSubstate::HexHighNybble;
        fsm.toggleRegion();
        TEST_CHECK(fsm.substate == MemEditSubstate::AsciiByte);
        TEST_CHECK(fsm.byteIndex == b);
        TEST_CHECK(fsm.toScreenColumn() == 58 + b);

        fsm.toggleRegion();
        TEST_CHECK(fsm.substate == MemEditSubstate::HexHighNybble);
        TEST_CHECK(fsm.byteIndex == b);
    }

    for (int r = 0; r < maxRows; ++r) {
        for (int b = 0; b < 16; ++b) {
            fsm.row = r;
            fsm.byteIndex = b;

            fsm.substate = MemEditSubstate::HexHighNybble;
            int colHi = fsm.toScreenColumn();
            TEST_CHECK((colHi - 10) % 3 == 0);

            fsm.substate = MemEditSubstate::HexLowNybble;
            int colLo = fsm.toScreenColumn();
            TEST_CHECK((colLo - 10) % 3 == 1);

            fsm.substate = MemEditSubstate::AsciiByte;
            int colAsc = fsm.toScreenColumn();
            TEST_CHECK(colAsc >= 58 && colAsc <= 73);
        }
    }

    return 0;
}

static int testSRecordParserFsmValidAndCorruptedRecords()
{
    std::cout << "[GUI TEST] Running testSRecordParserFsmValidAndCorruptedRecords...\n";
    SRecordLineFSM fsm;

    // 1. Valid S0, S1, S2, S3, S7, S8, S9 records
    const char *kValidS0 = "S021000036384B50524F47202020323043524541544544204259204541535936384B6D\n";
    TEST_CHECK(fsm.parseLine(kValidS0) == true);
    TEST_CHECK(fsm.state == SRecordLineState::Complete);
    TEST_CHECK(fsm.recordType == '0');
    TEST_CHECK(fsm.checksumValid == true);

    const char *kValidS1 = "S1231000307C40003010E048314000023010E148E048314000040C68001A00026600001CED\n";
    TEST_CHECK(fsm.parseLine(kValidS1) == true);
    TEST_CHECK(fsm.state == SRecordLineState::Complete);
    TEST_CHECK(fsm.recordType == '1');
    TEST_CHECK(fsm.address == 0x1000);
    TEST_CHECK(fsm.payload.size() == 32);
    TEST_CHECK(fsm.checksumValid == true);

    const char *kValidS2 = "S20800100012344E75DE\n";
    TEST_CHECK(fsm.parseLine(kValidS2) == true);
    TEST_CHECK(fsm.state == SRecordLineState::Complete);
    TEST_CHECK(fsm.recordType == '2');
    TEST_CHECK(fsm.address == 0x001000);
    TEST_CHECK(fsm.payload.size() == 4);
    TEST_CHECK(fsm.checksumValid == true);

    const char *kValidS3 = "S3090000100012344E75DD\n";
    TEST_CHECK(fsm.parseLine(kValidS3) == true);
    TEST_CHECK(fsm.state == SRecordLineState::Complete);
    TEST_CHECK(fsm.recordType == '3');
    TEST_CHECK(fsm.address == 0x00001000);
    TEST_CHECK(fsm.payload.size() == 4);
    TEST_CHECK(fsm.checksumValid == true);

    const char *kValidS7 = "S70500001000EA\n";
    TEST_CHECK(fsm.parseLine(kValidS7) == true);
    TEST_CHECK(fsm.state == SRecordLineState::Complete);
    TEST_CHECK(fsm.recordType == '7');
    TEST_CHECK(fsm.address == 0x00001000);
    TEST_CHECK(fsm.checksumValid == true);

    const char *kValidS8 = "S804001000EB\n";
    TEST_CHECK(fsm.parseLine(kValidS8) == true);
    TEST_CHECK(fsm.state == SRecordLineState::Complete);
    TEST_CHECK(fsm.recordType == '8');
    TEST_CHECK(fsm.address == 0x001000);
    TEST_CHECK(fsm.checksumValid == true);

    const char *kValidS9 = "S9031000EC\n";
    TEST_CHECK(fsm.parseLine(kValidS9) == true);
    TEST_CHECK(fsm.state == SRecordLineState::Complete);
    TEST_CHECK(fsm.recordType == '9');
    TEST_CHECK(fsm.address == 0x1000);
    TEST_CHECK(fsm.checksumValid == true);

    // 2. Corrupted checksum record
    const char *kCorruptedChecksum = "S1231000307C40003010E048314000023010E148E048314000040C68001A00026600001C00\n";
    TEST_CHECK(fsm.parseLine(kCorruptedChecksum) == true);
    TEST_CHECK(fsm.checksumValid == false);

    // 3. Malformed hex characters
    const char *kMalformedHex = "S1231000GG7C40003010E048314000023010E148E048314000040C68001A00026600001CED\n";
    TEST_CHECK(fsm.parseLine(kMalformedHex) == false);
    TEST_CHECK(fsm.state == SRecordLineState::Invalid);

    // 4. Truncated record
    const char *kTruncated = "S10510\n";
    TEST_CHECK(fsm.parseLine(kTruncated) == false);
    TEST_CHECK(fsm.state == SRecordLineState::Invalid);

    return 0;
}

static int testUpdaterFsmLifecycleTransitions()
{
    std::cout << "[GUI TEST] Running testUpdaterFsmLifecycleTransitions...\n";

    TEST_CHECK(Updater::isValidUpdaterTransition(UpdaterState::Idle, UpdaterState::CheckingRelease));
    TEST_CHECK(Updater::isValidUpdaterTransition(UpdaterState::CheckingRelease, UpdaterState::UpdateAvailable));
    TEST_CHECK(Updater::isValidUpdaterTransition(UpdaterState::UpdateAvailable, UpdaterState::FetchingChecksums));
    TEST_CHECK(Updater::isValidUpdaterTransition(UpdaterState::FetchingChecksums, UpdaterState::DownloadingBinary));
    TEST_CHECK(Updater::isValidUpdaterTransition(UpdaterState::DownloadingBinary, UpdaterState::VerifyingIntegrity));
    TEST_CHECK(Updater::isValidUpdaterTransition(UpdaterState::VerifyingIntegrity, UpdaterState::ApplyingUpdate));
    TEST_CHECK(Updater::isValidUpdaterTransition(UpdaterState::ApplyingUpdate, UpdaterState::Idle));

    TEST_CHECK(Updater::isValidUpdaterTransition(UpdaterState::CheckingRelease, UpdaterState::Idle));
    TEST_CHECK(Updater::isValidUpdaterTransition(UpdaterState::CheckingRelease, UpdaterState::Failed));
    TEST_CHECK(Updater::isValidUpdaterTransition(UpdaterState::Failed, UpdaterState::Idle));

    TEST_CHECK(!Updater::isValidUpdaterTransition(UpdaterState::CheckingRelease, UpdaterState::ApplyingUpdate));
    TEST_CHECK(!Updater::isValidUpdaterTransition(UpdaterState::DownloadingBinary, UpdaterState::CheckingRelease));
    TEST_CHECK(!Updater::isValidUpdaterTransition(UpdaterState::Idle, UpdaterState::ApplyingUpdate));
    TEST_CHECK(!Updater::isValidUpdaterTransition(UpdaterState::Idle, UpdaterState::VerifyingIntegrity));
    TEST_CHECK(!Updater::isValidUpdaterTransition(UpdaterState::CheckingRelease, UpdaterState::VerifyingIntegrity));

    Updater updater;
    TEST_CHECK(updater.state() == UpdaterState::Idle);

    TEST_CHECK(updater.transitionTo(UpdaterState::CheckingRelease));
    TEST_CHECK(updater.state() == UpdaterState::CheckingRelease);

    TEST_CHECK(!updater.transitionTo(UpdaterState::ApplyingUpdate));
    TEST_CHECK(updater.state() == UpdaterState::CheckingRelease);

    TEST_CHECK(updater.transitionTo(UpdaterState::UpdateAvailable));
    TEST_CHECK(updater.state() == UpdaterState::UpdateAvailable);

    TEST_CHECK(updater.transitionTo(UpdaterState::DownloadingBinary));
    TEST_CHECK(updater.transitionTo(UpdaterState::VerifyingIntegrity));
    TEST_CHECK(updater.transitionTo(UpdaterState::ApplyingUpdate));
    TEST_CHECK(updater.transitionTo(UpdaterState::Idle));
    TEST_CHECK(updater.state() == UpdaterState::Idle);

    return 0;
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication app(argc, argv);
    std::cout << "[GUI TEST] Initializing MainWindow...\n";

    MainWindow window;
    window.show();

    // Verify non-empty window icon & vector app icon
    TEST_CHECK(!MainWindow::createAppWindowIcon().isNull());
    TEST_CHECK(!window.windowIcon().isNull());

    QString appDir = QCoreApplication::applicationDirPath();
    QString asmBin = appDir + "/easy68k-asm";
    QString testS68 = appDir + "/gui_hello_test.s68";
    QString testL68 = appDir + "/gui_hello_test.l68";
    int asmRc = QProcess::execute(asmBin, {"examples/hello.x68", "-o", testS68, "--listing", testL68});
    TEST_CHECK(asmRc == 0 && QFileInfo::exists(testS68) && QFileInfo::exists(testL68));

    std::cout << "[GUI TEST] Testing SimulatorManager directly...\n";
    SimulatorManager sim;

    bool loaded = sim.loadS68(testS68);
    if (!loaded) {
        std::cerr << "[GUI TEST ERROR] Failed to load " << testS68.toStdString() << "\n";
        return 1;
    }
    std::cout << "[GUI TEST] Initial PC: 0x" << std::hex << sim.getPC() << std::dec << "\n";
    TEST_CHECK(sim.getPC() == 0x1000);

    // Test stepping & traceInto
    std::cout << "[GUI TEST] Stepping instruction...\n";
    sim.traceInto();
    std::cout << "[GUI TEST] After step 1: PC = 0x" << std::hex << sim.getPC() << std::dec
              << ", Cycles = " << sim.getCycles() << "\n";
    TEST_CHECK(sim.getPC() == 0x1006);
    TEST_CHECK(sim.getCycles() > 0);

    // Test breakpoints
    std::cout << "[GUI TEST] Testing breakpoints...\n";
    sim.toggleBreakpoint(0x100C);
    TEST_CHECK(sim.hasBreakpoint(0x100C));

    // Run until breakpoint
    std::cout << "[GUI TEST] Running until breakpoint...\n";
    sim.run();

    // Process Qt events while running
    int maxWait = 200; // 2 seconds max
    while (sim.isRunning() && maxWait-- > 0) {
        QTest::qWait(10);
    }

    std::cout << "[GUI TEST] Halted/Paused at PC: 0x" << std::hex << sim.getPC() << std::dec << "\n";
    TEST_CHECK(sim.getPC() == 0x100C || sim.getPC() == 0x1012);

    sim.clearBreakpoints();
    TEST_CHECK(!sim.hasBreakpoint(0x100C));

    // Test Execution Modes: Auto-trace, Rewind, Register Setters, Clear Cycles
    std::cout << "[GUI TEST] Testing SimulatorManager Execution Modes...\n";
    sim.toggleAutoTrace(40);
    TEST_CHECK(sim.isAutoTracing() == true);
    sim.toggleAutoTrace();
    TEST_CHECK(sim.isAutoTracing() == false);

    sim.setD(4, 0xCAFEBABE);
    TEST_CHECK(static_cast<uint32_t>(sim.getD()[4]) == 0xCAFEBABE);
    sim.setA(3, 0x00008800);
    TEST_CHECK(static_cast<uint32_t>(sim.getA()[3]) == 0x00008800);
    sim.setPC(0x00001000);
    TEST_CHECK(sim.getPC() == 0x1000);

    TEST_CHECK(sim.getCycles() > 0);
    sim.clearCycles();
    TEST_CHECK(sim.getCycles() == 0);

    // Test TerminalWidget TRAP #15 text output
    std::cout << "[GUI TEST] Testing TerminalWidget I/O...\n";
    TerminalWidget term;
    term.textOut("Hello from GUI test!\n");
    QTest::qWait(50);
    TEST_CHECK(term.toPlainText().contains("Hello from GUI test!"));

    // -------------------------------------------------------------------------
    // Grounded Test 1: ListingWidget No-Snapping Viewport Highlighting
    // -------------------------------------------------------------------------
    std::cout << "[GUI TEST] Testing ListingWidget and no-snapping highlight...\n";
    ListingWidget listWidget;
    listWidget.setSimulatorManager(&sim);

    QFile l68File(testL68);
    TEST_CHECK(l68File.open(QFile::ReadOnly | QFile::Text));
    QStringList l68Lines;
    QTextStream l68In(&l68File);
    while (!l68In.atEnd()) {
        l68Lines.append(l68In.readLine());
    }
    listWidget.loadListing(l68Lines);
    TEST_CHECK(listWidget.scrollArea()->getLineCount() > 0);

    listWidget.resize(700, 500);
    listWidget.show();
    QTest::qWait(20);

    // Initial highlight at PC=0x1000
    listWidget.highlightPC(0x1000, false);
    int initialScroll = listWidget.scrollArea()->verticalScrollBar()->value();
    int hl0 = listWidget.scrollArea()->getHighlightedLine();
    TEST_CHECK(hl0 >= 0);

    // Step to next instruction (0x1006)
    sim.traceInto();
    TEST_CHECK(sim.getPC() == 0x1006);
    listWidget.highlightPC(static_cast<uint32_t>(sim.getPC()), false);
    int hl1 = listWidget.scrollArea()->getHighlightedLine();
    TEST_CHECK(hl1 > hl0);

    // Grounded assertion: Viewport scroll did NOT snap or change!
    int scrollAfterStep1 = listWidget.scrollArea()->verticalScrollBar()->value();
    TEST_CHECK(scrollAfterStep1 == initialScroll);

    // Step again (0x100A)
    sim.traceInto();
    TEST_CHECK(sim.getPC() == 0x100A);
    listWidget.highlightPC(static_cast<uint32_t>(sim.getPC()), false);
    int scrollAfterStep2 = listWidget.scrollArea()->verticalScrollBar()->value();
    TEST_CHECK(scrollAfterStep2 == initialScroll); // Zero snapping!

    // -------------------------------------------------------------------------
    // Grounded Test 2: MemoryView Interactive Editing, Fill & Copy
    // -------------------------------------------------------------------------
    std::cout << "[GUI TEST] Testing MemoryView (Fill, Copy, and Interactive In-Place Edit)...\n";
    MemoryView memView;
    memView.setMemoryPointer(sim.getMemory(), sim.getMemorySize());
    memView.setBaseAddress(0x1000);
    memView.resize(800, 400);
    memView.show();
    QTest::qWait(20);

    // Test Fill
    bool fillOk = memView.fillMemory(0x2000, 0x2007, 0xAABB, 2);
    TEST_CHECK(fillOk);
    TEST_CHECK(static_cast<uint8_t>(sim.getMemory()[0x2000]) == 0xAA);
    TEST_CHECK(static_cast<uint8_t>(sim.getMemory()[0x2001]) == 0xBB);
    TEST_CHECK(static_cast<uint8_t>(sim.getMemory()[0x2002]) == 0xAA);
    TEST_CHECK(static_cast<uint8_t>(sim.getMemory()[0x2003]) == 0xBB);

    // Test Copy
    bool copyOk = memView.copyMemory(0x2000, 0x2500, 8);
    TEST_CHECK(copyOk);
    TEST_CHECK(static_cast<uint8_t>(sim.getMemory()[0x2500]) == 0xAA);
    TEST_CHECK(static_cast<uint8_t>(sim.getMemory()[0x2501]) == 0xBB);

    // Test Save Memory Range
    QString tmpMemSave = appDir + "/gui_test_mem_range.bin";
    TEST_CHECK(memView.saveMemoryRange(0x2500, 0x2503, tmpMemSave) == true);
    QFile savedMemFile(tmpMemSave);
    TEST_CHECK(savedMemFile.open(QIODevice::ReadOnly));
    QByteArray savedBytes = savedMemFile.readAll();
    savedMemFile.close();
    QFile::remove(tmpMemSave);
    TEST_CHECK(savedBytes.size() == 4);
    TEST_CHECK(static_cast<uint8_t>(savedBytes[0]) == 0xAA);
    TEST_CHECK(static_cast<uint8_t>(savedBytes[1]) == 0xBB);

    // Test In-Place Interactive Nybble Edit
    QSignalSpy memSpy(&memView, &MemoryView::memoryModified);
    memView.gridArea()->setCursorPosition(0, 10); // Row 0, High nybble of 0x1000
    bool keyHandled1 = memView.gridArea()->handleKeyEdit(Qt::Key_5, '5');
    TEST_CHECK(keyHandled1);
    TEST_CHECK((static_cast<uint8_t>(sim.getMemory()[0x1000]) & 0xF0) == 0x50);
    TEST_CHECK(memSpy.count() >= 1);

    // Low nybble: cursor automatically advanced to col 11
    TEST_CHECK(memView.gridArea()->getCursorCol() == 11);
    bool keyHandled2 = memView.gridArea()->handleKeyEdit(Qt::Key_C, 'C');
    TEST_CHECK(keyHandled2);
    TEST_CHECK(static_cast<uint8_t>(sim.getMemory()[0x1000]) == 0x5C);

    // Test In-Place Interactive ASCII Edit
    memView.gridArea()->setCursorPosition(0, 58); // Row 0, first ASCII character
    bool keyHandledAscii = memView.gridArea()->handleKeyEdit(Qt::Key_M, 'M');
    TEST_CHECK(keyHandledAscii);
    TEST_CHECK(sim.getMemory()[0x1000] == 'M');

    // Test Left-Arrow Navigation across nybble and byte boundaries
    memView.gridArea()->setCursorPosition(1, 13); // Row 1, byte 1 high nybble (col 13)
    TEST_CHECK(memView.gridArea()->getCursorRow() == 1 && memView.gridArea()->getCursorCol() == 13);
    // Left arrow moves past space (col 12) directly to byte 0 low nybble (col 11)
    bool leftHandled1 = memView.gridArea()->handleKeyEdit(Qt::Key_Left, QChar());
    TEST_CHECK(leftHandled1);
    TEST_CHECK(memView.gridArea()->getCursorRow() == 1 && memView.gridArea()->getCursorCol() == 11);
    // Left arrow moves to byte 0 high nybble (col 10)
    bool leftHandled2 = memView.gridArea()->handleKeyEdit(Qt::Key_Left, QChar());
    TEST_CHECK(leftHandled2);
    TEST_CHECK(memView.gridArea()->getCursorRow() == 1 && memView.gridArea()->getCursorCol() == 10);
    // Left arrow at col 10 wraps to previous row rightmost hex column (col 56)
    bool leftHandled3 = memView.gridArea()->handleKeyEdit(Qt::Key_Left, QChar());
    TEST_CHECK(leftHandled3);
    TEST_CHECK(memView.gridArea()->getCursorRow() == 0 && memView.gridArea()->getCursorCol() == 56);

    // Test Tab key toggle between Hex and ASCII
    bool tabHandled = memView.gridArea()->handleKeyEdit(Qt::Key_Tab, QChar());
    TEST_CHECK(tabHandled);
    TEST_CHECK(memView.gridArea()->getCursorCol() == 73); // ASC_LEFT + 15
    bool tabHandledBack = memView.gridArea()->handleKeyEdit(Qt::Key_Tab, QChar());
    TEST_CHECK(tabHandledBack);
    TEST_CHECK(memView.gridArea()->getCursorCol() == 55); // HEX_LEFT + 15 * 3

    // -------------------------------------------------------------------------
    // Grounded Test 3: RegistersView Editing and Mutators
    // -------------------------------------------------------------------------
    std::cout << "[GUI TEST] Testing RegistersView...\n";
    RegistersView regView;
    regView.setSimulatorManager(&sim);
    sim.setD(1, 0x11223344);
    sim.setA(4, 0x00FF8800);
    sim.setPC(0x00001006);
    sim.setSR(0x2700);
    regView.updateRegisters(sim.getD(), sim.getA(), sim.getPC(), sim.getSR(), sim.getCycles());
    TEST_CHECK(regView.getD(1) == 0x11223344);
    TEST_CHECK(regView.getA(4) == 0x00FF8800);
    TEST_CHECK(regView.getPC() == 0x1006);
    TEST_CHECK(regView.getSR() == 0x2700);

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    // Grounded Test 4: Behavioral Verification - SIMHALT, Self-Modifying Code,
    // and Memory/Listing Interactions
    // -------------------------------------------------------------------------
    std::cout << "[GUI TEST] Testing SIMHALT emission and address-indirect shift unpacking...\n";

    auto readTextFile = [](const QString &path) {
        QStringList lines;
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&f);
            while (!in.atEnd()) lines.append(in.readLine());
        }
        return lines;
    };
    auto memByte = [&sim](uint32_t addr) -> uint8_t {
        return static_cast<uint8_t>(sim.getMemory()[addr & 0xFFFFFF]);
    };
    auto memWord = [&](uint32_t addr) -> uint16_t {
        return static_cast<uint16_t>((static_cast<uint16_t>(memByte(addr)) << 8) |
                                     static_cast<uint16_t>(memByte(addr + 1)));
    };

    // 1. SIMHALT $FFFF $FFFF emission across consecutive words and execution halt
    const QString unpackSrc = appDir + "/gui_test_shift_unpack.x68";
    const QString unpackS68 = appDir + "/gui_test_shift_unpack.s68";
    const QString unpackL68 = appDir + "/gui_test_shift_unpack.l68";

    QFile unpackFile(unpackSrc);
    TEST_CHECK(unpackFile.open(QIODevice::WriteOnly | QIODevice::Text));
    const char *unpackAsm =
        "        ORG     $4000\n"
        "VAR1    DC.W    $1A23\n"
        "        ORG     $1000\n"
        "        MOVE.W  #$4000,A0\n"
        "        MOVE.W  (A0),D0\n"
        "        LSR.W   #8,D0\n"
        "        MOVE.W  D0,2(A0)\n"
        "        MOVE.W  (A0),D0\n"
        "        LSL.W   #8,D0\n"
        "        LSR.W   #8,D0\n"
        "        MOVE.W  D0,4(A0)\n"
        "        SIMHALT\n"
        "        END     $1000\n";
    TEST_CHECK(unpackFile.write(unpackAsm) > 0);
    unpackFile.close();

    int unpackRc = QProcess::execute(asmBin, {unpackSrc, "-o", unpackS68, "--listing", unpackL68});
    TEST_CHECK(unpackRc == 0 && QFileInfo::exists(unpackS68) && QFileInfo::exists(unpackL68));

    // Verify Listing classification: headers vs real instructions
    TEST_CHECK(!ListingScrollArea::isInstructionLine("00001000 Starting Address"));
    TEST_CHECK(!ListingScrollArea::isInstructionLine("00003000 Starting Address"));
    TEST_CHECK(ListingScrollArea::isInstructionLine("00003000  3038 3100       10   MOVE.W OP1, D0") == true);
    TEST_CHECK(!ListingScrollArea::isInstructionLine("00004000= 1A23"));

    TEST_CHECK(sim.loadS68(unpackS68));
    TEST_CHECK(sim.getPC() == 0x1000);
    // Two consecutive $FFFF words emitted for SIMHALT (addresses $1016 and $1018)
    TEST_CHECK(memWord(0x1016) == 0xFFFF);
    TEST_CHECK(memWord(0x1018) == 0xFFFF);

    // Execute unpacking sequence: $1A23 unpacked into $001A @ $4002 and $0023 @ $4004
    for (int i = 0; i < 8; ++i) {
        sim.traceInto();
    }
    TEST_CHECK(memWord(0x4002) == 0x001A);
    TEST_CHECK(memWord(0x4004) == 0x0023);
    sim.traceInto(); // SIMHALT
    TEST_CHECK(sim.getPC() == 0x101A);
    TEST_CHECK(!sim.isRunning());

    // 2. Live Self-Modifying Code, Auto-Jump to startPC, and Rewind vs Reload
    std::cout << "[GUI TEST] Testing live self-modifying code and Rewind/Reload semantics...\n";
    const QString selfModSrc = appDir + "/gui_test_self_mod.x68";
    const QString selfModS68 = appDir + "/gui_test_self_mod.s68";
    const QString selfModL68 = appDir + "/gui_test_self_mod.l68";

    QFile selfModFile(selfModSrc);
    TEST_CHECK(selfModFile.open(QIODevice::WriteOnly | QIODevice::Text));
    const char *selfModAsm =
        "DATA_SEC EQU $3100\n"
        "CODE_SEC EQU $3000\n"
        "        ORG DATA_SEC\n"
        "OP1     DS.W 1\n"
        "OP2     DS.W 1\n"
        "        ORG CODE_SEC\n"
        "START:\n"
        "        MOVE.W OP1, D0\n"
        "        LSL.W  #1, D0\n"
        "        MOVE.W D0, $300E\n"
        "        MOVE.W OP2, D0\n"
        "        LSL.W  #1, D0\n"
        "        MOVE.W D0, $3004\n"
        "        MOVE.B #9, D0\n"
        "        TRAP   #15\n"
        "        END    START\n";
    TEST_CHECK(selfModFile.write(selfModAsm) > 0);
    selfModFile.close();

    int selfModRc = QProcess::execute(asmBin, {selfModSrc, "-o", selfModS68, "--listing", selfModL68});
    TEST_CHECK(selfModRc == 0 && QFileInfo::exists(selfModS68) && QFileInfo::exists(selfModL68));

    QStringList selfModListing = readTextFile(selfModL68);
    TEST_CHECK(selfModListing.size() > 5);
    TEST_CHECK(selfModListing.filter("0000300E  E348").size() == 1);

    // Auto-jump to startPC ($3000) verification
    window.getSimulatorManager()->loadS68(selfModS68);
    TEST_CHECK(window.getMemoryView()->getBaseAddress() == 0x3000);
    TEST_CHECK(window.getMemoryView()->getAddressInputText() == "00003000");

    TEST_CHECK(sim.loadS68(selfModS68));
    TEST_CHECK(sim.getPC() == 0x3000);
    TEST_CHECK(memWord(0x300E) == 0xE348); // Assembled instruction: LSL.W #1, D0

    // Set memory data for OP1 and OP2 at $3100
    sim.getMemory()[0x3100] = static_cast<char>(0x74);
    sim.getMemory()[0x3101] = static_cast<char>(0xA4);
    sim.getMemory()[0x3102] = static_cast<char>(0x12);
    sim.getMemory()[0x3103] = static_cast<char>(0x34);

    sim.traceInto(); // MOVE.W OP1, D0 -> D0 = $74A4
    TEST_CHECK((static_cast<uint32_t>(sim.getD()[0]) & 0xFFFF) == 0x74A4);
    sim.traceInto(); // LSL.W #1, D0 -> D0 = $E948
    TEST_CHECK((static_cast<uint32_t>(sim.getD()[0]) & 0xFFFF) == 0xE948);
    sim.traceInto(); // MOVE.W D0, $300E -> dynamic self-modification of opcode at $300E!
    TEST_CHECK(memWord(0x300E) == 0xE948);
    sim.traceInto(); // MOVE.W OP2, D0 -> D0 = $1234
    TEST_CHECK((static_cast<uint32_t>(sim.getD()[0]) & 0xFFFF) == 0x1234);

    // CPU decodes dynamically modified opcode ($E948: LSL.W #4, D0) from RAM:
    sim.traceInto();
    TEST_CHECK(sim.getPC() == 0x3010);
    TEST_CHECK((static_cast<uint32_t>(sim.getD()[0]) & 0xFFFF) == 0x2340); // $1234 << 4 == $2340

    sim.traceInto(); // MOVE.W D0, $3004
    TEST_CHECK(memWord(0x3004) == 0x2340);
    sim.traceInto(); // MOVE.B #9, D0
    sim.traceInto(); // TRAP #15 (Task 9)
    TEST_CHECK(!sim.isRunning());

    // Rewind resets PC/registers to startPC but keeps modified RAM (Sim68K semantics)
    sim.rewindProgram();
    TEST_CHECK(sim.getPC() == 0x3000);
    TEST_CHECK(memWord(0x300E) == 0xE948);

    // Reload restores original binary from S-Record disk image
    sim.reloadProgram();
    TEST_CHECK(sim.getPC() == 0x3000);
    TEST_CHECK(memWord(0x300E) == 0xE348);

    // Directive toggle test: *[sim68k]SIMHALT_OFF
    sim.loadListingDirectives({"*[sim68k]SIMHALT_OFF"});
    TEST_CHECK(!simhalt_on);
    sim.rewindProgram();
    TEST_CHECK(!simhalt_on);
    TEST_CHECK(sim.loadS68(selfModS68));
    TEST_CHECK(simhalt_on); // loadS68 re-enables SIMHALT by default

    // Clean up temporary test files
    QFile::remove(unpackSrc);
    QFile::remove(unpackS68);
    QFile::remove(unpackL68);
    QFile::remove(selfModSrc);
    QFile::remove(selfModS68);
    QFile::remove(selfModL68);
    std::cout << "[GUI TEST] SIMHALT and Self-Modifying tests PASSED.\n";

    // -------------------------------------------------------------------------
    // Grounded Test 5: StackView
    // -------------------------------------------------------------------------
    std::cout << "[GUI TEST] Testing StackView...\n";
    StackView stackView;
    stackView.setSimulatorManager(&sim);
    stackView.updateView();
    TEST_CHECK(stackView.displayArea() != nullptr);
    uint32_t midBefore = stackView.displayArea()->getMidAddress();
    stackView.displayArea()->scrollByBytes(4);
    TEST_CHECK(stackView.displayArea()->getMidAddress() == ((midBefore + 4) & 0x00FFFFFF));

    // Test HardwareWidget: 7-segment, LEDs, Switches, Pushbuttons, IRQ, Memory Map
    std::cout << "[GUI TEST] Testing HardwareWidget peripherals...\n";
    HardwareWidget hw;
    TEST_CHECK(memory != nullptr);

    // 1. Test 7-Segment display ($E00000..$E0000E)
    memory[seg7loc & 0xFFFFFF] = 0x7D;      // Digit 0 = '6'
    memory[(seg7loc + 2) & 0xFFFFFF] = 0x7F; // Digit 1 = '8'
    hw.updateHardware();
    TEST_CHECK(hw.getSevenSegmentValue(0) == 0x7D);
    TEST_CHECK(hw.getSevenSegmentValue(1) == 0x7F);

    // 2. Test LED bank ($E00010)
    memory[LEDloc & 0xFFFFFF] = static_cast<char>(0xA5);
    hw.updateHardware();
    TEST_CHECK(hw.getLEDValue() == 0xA5);

    // 3. Test Pushbuttons default (normally high: 0xFF)
    TEST_CHECK(hw.getPushbuttonValue() == 0xFF);

    // 4. Test Interrupt Trigger
    hw.triggerInterrupt(3);
    TEST_CHECK((irq & (1 << 2)) != 0);

    // 5. Test 2D Graphics Canvas via TerminalWidget forwarding
    std::cout << "[GUI TEST] Testing 2D Graphics Canvas API...\n";
    term.setHardwareWidget(&hw);

    // Line color Cyan ($00FFFF00), Fill color Red ($000000FF)
    term.setLineColor(0x00FFFF00);
    term.setFillColor(0x000000FF);
    term.setPenWidth(2);

    // Draw single pixel
    term.drawPixel(10, 10);
    TEST_CHECK(hw.getCanvasPixel(10, 10) == QColor(0, 255, 255));
    TEST_CHECK(term.getPixel(10, 10) == 0x00FFFF00);

    // Draw filled rectangle (50, 50) to (100, 100)
    term.rectangle(50, 50, 100, 100);
    TEST_CHECK(hw.getCanvasPixel(75, 75) == QColor(255, 0, 0));

    // Draw line
    term.line(0, 0, 20, 20);
    TEST_CHECK(hw.getCanvasPixel(0, 0) == QColor(0, 255, 255));

    // Test EasyBinWidget
    std::cout << "[GUI TEST] Testing EasyBinWidget...\n";
    EasyBinWidget easyBin;
    TEST_CHECK(easyBin.inspectSRecordFile(testS68) == true);

    // Round-trip S68 -> BIN -> S68
    QString tmpBin = appDir + "/gui_test_tmp.bin";
    QString tmpS68 = appDir + "/gui_test_tmp.s68";
    TEST_CHECK(easyBin.convertS68ToBin(testS68, tmpBin, 0) == true);
    TEST_CHECK(QFileInfo::exists(tmpBin));
    TEST_CHECK(easyBin.convertBinToS68(tmpBin, tmpS68, 0x1000, 0x1000, 0) == true);
    TEST_CHECK(QFileInfo::exists(tmpS68));
    QFile::remove(tmpBin);
    QFile::remove(tmpS68);

    // Test HelpWidget
    std::cout << "[GUI TEST] Testing HelpWidget...\n";
    HelpWidget help;
    QString helpRoot = help.findHelpRootDir();
    std::cout << "[GUI TEST] Help Root Dir: " << helpRoot.toStdString() << "\n";
    TEST_CHECK(QFileInfo::exists(helpRoot + "/main.htm"));
    help.loadPage("main.htm");
    help.loadPage("SIMHELP/Hardware.htm");

    // Clean up temporary s68 and l68 files
    QFile::remove(testS68);
    QFile::remove(testL68);

    // Test native Auto-Updater
    std::cout << "[GUI TEST] Testing native Auto-Updater...\n";
    TEST_CHECK(window.getUpdater() != nullptr);

    // 1. Version comparison logic
    TEST_CHECK(Updater::isNewerVersion("5.16.1", "v5.16.2") == true);
    TEST_CHECK(Updater::isNewerVersion("5.16.1", "v5.16.1") == false);
    TEST_CHECK(Updater::isNewerVersion("5.16.1", "v5.15.9") == false);
    TEST_CHECK(Updater::isNewerVersion("5.16.1", "6.0.0") == true);
    TEST_CHECK(Updater::isNewerVersion("v5.16.1", "5.16.2") == true);
    TEST_CHECK(!Updater::currentVersion().isEmpty());
    TEST_CHECK(Updater::repositorySlug() == "Sebo1411/EASy68KLinux");
    TEST_CHECK(Updater::aptRepoSetupCommand().contains("setup-apt.sh"));
    TEST_CHECK(Updater::rpmRepoSetupCommand().contains("setup-rpm.sh"));

    // 2. Parse GitHub Releases API JSON
    QByteArray releaseJson = R"({
        "tag_name": "v5.16.2",
        "html_url": "https://github.com/Sebo1411/EASy68KLinux/releases/tag/v5.16.2",
        "published_at": "2026-11-12T10:00:00Z",
        "body": "## EASy68K v5.16.2\n- Native auto-updater\n- Verification fixes",
        "assets": [
            {
                "name": "easy68k-5.16.2-x86_64.AppImage",
                "browser_download_url": "https://github.com/Sebo1411/EASy68KLinux/releases/download/v5.16.2/easy68k-5.16.2-x86_64.AppImage",
                "size": 25165824
            },
            {
                "name": "easy68k-5.16.2-amd64.deb",
                "browser_download_url": "https://github.com/Sebo1411/EASy68KLinux/releases/download/v5.16.2/easy68k-5.16.2-amd64.deb",
                "size": 18874368
            },
            {
                "name": "easy68k-5.16.2-1.x86_64.rpm",
                "browser_download_url": "https://github.com/Sebo1411/EASy68KLinux/releases/download/v5.16.2/easy68k-5.16.2-1.x86_64.rpm",
                "size": 19922944
            },
            {
                "name": "easy68k-5.16.2-linux-x86_64.tar.gz",
                "browser_download_url": "https://github.com/Sebo1411/EASy68KLinux/releases/download/v5.16.2/easy68k-5.16.2-linux-x86_64.tar.gz",
                "size": 15728640
            },
            {
                "name": "SHA256SUMS.txt",
                "browser_download_url": "https://github.com/Sebo1411/EASy68KLinux/releases/download/v5.16.2/SHA256SUMS.txt",
                "size": 512
            }
        ]
    })";

    ReleaseInfo info = Updater::parseReleaseJson(releaseJson);
    TEST_CHECK(info.isValid());
    TEST_CHECK(info.tagName == "v5.16.2");
    TEST_CHECK(info.version == "5.16.2");
    TEST_CHECK(info.htmlUrl == "https://github.com/Sebo1411/EASy68KLinux/releases/tag/v5.16.2");
    TEST_CHECK(info.publishedAt == "2026-11-12T10:00:00Z");
    TEST_CHECK(info.body.contains("Native auto-updater"));
    TEST_CHECK(info.assets.size() == 5);
    TEST_CHECK(info.sha256SumsUrl == "https://github.com/Sebo1411/EASy68KLinux/releases/download/v5.16.2/SHA256SUMS.txt");

    // 3. Asset selection logic
    qputenv("APPIMAGE", "/tmp/fake-EASy68K.AppImage");
    ReleaseAsset bestAppImage = Updater::selectBestAssetForSystem(info);
    TEST_CHECK(bestAppImage.name == "easy68k-5.16.2-x86_64.AppImage");

    qunsetenv("APPIMAGE");
    ReleaseAsset bestSys = Updater::selectBestAssetForSystem(info);
    if (QFile::exists("/usr/bin/dpkg")) {
        TEST_CHECK(bestSys.name == "easy68k-5.16.2-amd64.deb");
    } else if (QFile::exists("/usr/bin/rpm")) {
        TEST_CHECK(bestSys.name == "easy68k-5.16.2-1.x86_64.rpm");
    } else {
        TEST_CHECK(bestSys.name.endsWith(".AppImage") || bestSys.name.endsWith(".tar.gz"));
    }

    // 4. Real SHA-256 calculation and file integrity verification
    QString testUpdateFile = appDir + "/test_update_file.bin";
    QFile binOut(testUpdateFile);
    TEST_CHECK(binOut.open(QIODevice::WriteOnly));
    QByteArray samplePayload = "EASy68K Update Package Payload\xDE\xAD\xBE\xEF\x00\x42";
    binOut.write(samplePayload);
    binOut.close();

    QCryptographicHash hashCalc(QCryptographicHash::Sha256);
    hashCalc.addData(samplePayload);
    QString realHexHash = QString::fromLatin1(hashCalc.result().toHex()).toLower();
    TEST_CHECK(realHexHash.length() == 64);

    QString sha256SumsFileContent = QString(
        "# Automated Release Checksums\n"
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855  other.deb\n"
        "%1  test_update_file.bin\n"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef *dummy.rpm\n"
    ).arg(realHexHash);

    QString parsedHash = Updater::parseSha256ForAsset(sha256SumsFileContent, "test_update_file.bin");
    TEST_CHECK(parsedHash == realHexHash);

    QString missingHash = Updater::parseSha256ForAsset(sha256SumsFileContent, "non_existent.pkg");
    TEST_CHECK(missingHash.isEmpty());

    // Verify valid SHA-256
    TEST_CHECK(Updater::verifyFileSha256(testUpdateFile, realHexHash) == true);
    // Verify case-insensitivity
    TEST_CHECK(Updater::verifyFileSha256(testUpdateFile, realHexHash.toUpper()) == true);

    // Verify tampered hash fails
    TEST_CHECK(Updater::verifyFileSha256(testUpdateFile, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == false);

    // Tamper file content and verify it fails
    TEST_CHECK(binOut.open(QIODevice::Append));
    binOut.write("tampered");
    binOut.close();
    TEST_CHECK(Updater::verifyFileSha256(testUpdateFile, realHexHash) == false);

    QFile::remove(testUpdateFile);

    // 5. Settings persistence
    bool origSetting = Updater::autoCheckOnStartup();
    Updater::setAutoCheckOnStartup(!origSetting);
    TEST_CHECK(Updater::autoCheckOnStartup() == !origSetting);
    Updater::setAutoCheckOnStartup(origSetting);
    TEST_CHECK(Updater::autoCheckOnStartup() == origSetting);

    // -------------------------------------------------------------------------
    // 6. Explicit FSM & O(1) Table Verification Tests
    // -------------------------------------------------------------------------
    TEST_CHECK(testSimExecFsmTransitionsAndInvariants() == 0);
    TEST_CHECK(testOpcodeDecodeLutExhaustive64kEquivalence() == 0);
    TEST_CHECK(testMemoryCursorFsmNavigationAndColumnMapping() == 0);
    TEST_CHECK(testSRecordParserFsmValidAndCorruptedRecords() == 0);
    TEST_CHECK(testUpdaterFsmLifecycleTransitions() == 0);

    std::cout << "[GUI TEST] ALL TESTS (CORE, HARDWARE, GRAPHICS, EASYBIN, HELP, UPDATER, LISTING, MEMORY, STACK) PASSED!\n";
    return 0;
}
