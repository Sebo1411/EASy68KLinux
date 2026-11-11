#include <QApplication>
#include <QCoreApplication>
#include <QProcess>
#include <QTest>
#include <QSignalSpy>
#include <QFileInfo>
#include <QFile>
#include <iostream>
#include <cstdlib>
#include "../src/gui/mainwindow.h"
#include "../src/gui/simulatormanager.h"
#include "../src/gui/codeeditor.h"
#include "../src/gui/registersview.h"
#include "../src/gui/memoryview.h"
#include "../src/gui/terminalwidget.h"
#include "../src/gui/hardwarewidget.h"
#include "../src/gui/easybinwidget.h"
#include "../src/gui/helpwidget.h"

#define TEST_CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "[GUI TEST FAILED] " << #cond << " at line " << __LINE__ << "\n"; \
            return 1; \
        } \
    } while (0)

extern int seg7loc, LEDloc, switchLoc, pbLoc;
extern char *memory;

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication app(argc, argv);
    std::cout << "[GUI TEST] Initializing MainWindow...\n";

    MainWindow window;
    window.show();

    QString appDir = QCoreApplication::applicationDirPath();
    QString asmBin = appDir + "/easy68k-asm";
    QString testS68 = appDir + "/gui_hello_test.s68";
    int asmRc = QProcess::execute(asmBin, {"examples/hello.x68", "-o", testS68});
    TEST_CHECK(asmRc == 0 && QFileInfo::exists(testS68));

    std::cout << "[GUI TEST] Testing SimulatorManager directly...\n";
    SimulatorManager sim;

    bool loaded = sim.loadS68(testS68);
    if (!loaded) {
        std::cerr << "[GUI TEST ERROR] Failed to load " << testS68.toStdString() << "\n";
        return 1;
    }
    std::cout << "[GUI TEST] Initial PC: 0x" << std::hex << sim.getPC() << std::dec << "\n";
    TEST_CHECK(sim.getPC() == 0x1000);

    // Test stepping
    std::cout << "[GUI TEST] Stepping instruction...\n";
    sim.step();
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

    // Test TerminalWidget TRAP #15 text output
    std::cout << "[GUI TEST] Testing TerminalWidget I/O...\n";
    TerminalWidget term;
    term.textOut("Hello from GUI test!\n");
    QTest::qWait(50);
    TEST_CHECK(term.toPlainText().contains("Hello from GUI test!"));

    // Test MemoryView rendering
    std::cout << "[GUI TEST] Testing MemoryView...\n";
    MemoryView memView;
    memView.setMemoryPointer(sim.getMemory(), sim.getMemorySize());
    memView.setBaseAddress(0x1000);
    QTest::qWait(50);

    // Test RegistersView
    std::cout << "[GUI TEST] Testing RegistersView...\n";
    RegistersView regView;
    regView.updateRegisters(sim.getD(), sim.getA(), sim.getPC(), sim.getSR(), sim.getCycles());
    QTest::qWait(50);

    // Test HardwareWidget: 7-segment, LEDs, Switches, Pushbuttons
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

    // 4. Test 2D Graphics Canvas via TerminalWidget forwarding
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

    // Clean up temporary s68 file
    QFile::remove(testS68);

    std::cout << "[GUI TEST] ALL TESTS (CORE, HARDWARE, GRAPHICS, EASYBIN, HELP) PASSED!\n";
    return 0;
}
