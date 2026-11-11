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
#include "../src/gui/updater.h"
#include <QCryptographicHash>

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

    std::cout << "[GUI TEST] ALL TESTS (CORE, HARDWARE, GRAPHICS, EASYBIN, HELP, UPDATER) PASSED!\n";
    return 0;
}
