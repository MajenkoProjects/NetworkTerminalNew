/* Copyright (c) 2020, Majenko Technologies
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without modification,
 *  are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice, this
 *    list of conditions and the following disclaimer in the documentation and/or
 *    other materials provided with the distribution.
 *
 *  * Neither the name of Majenko Technologies nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <fabgl.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <CommandParser.h>
#include <HTTPUpdate.h>
#include <nvs.h>
#include <nvs_flash.h>
#include "network/ICMP.h"

#define INRED(X) "\e[31m" X "\e[37m"
#define INGREEN(X) "\e[32m" X "\e[37m"
#define INYELLOW(X) "\e[33m" X "\e[37m"
#define INBLUE(X) "\e[34m" X "\e[37m"
#define INMAGENTA(X) "\e[35m" X "\e[37m"
#define INCYAN(X) "\e[36m" X "\e[37m"
#define INWHITE(X) "\e[37m" X "\e[37m"

#define VERSION "1.0.0"
#define FABGLVER "0.9.0"



CommandParser CP;

enum class State { Prompt, PromptInput, UnknownCommand, Help, Info, TelnetInit, Telnet, Scan, Ping, Reset, Sessions, KeyTest, Session, OTA, Set, Password, EnterPriv, Priv, AskPass, Pass };

#define NUM_SESSIONS 10

State        state = State::Prompt;
WiFiClient   client[10];
bool         error = false;
int          session = 0;
uint32_t     nvs;
TaskHandle_t otaTask = NULL;

bool priv = false;

fabgl::VGATextController DisplayController;
fabgl::PS2Controller     PS2Controller;
fabgl::Terminal          Terminal;
fabgl::LineEditor        LineEditor(&Terminal);

struct settings_t {
    char *ssid;
    char *psk;
    char *name;
    char *domain;
    char *password;
} settings;

void initSettings() {
    size_t length;
    memset(&settings, 0, sizeof(settings));

    nvs_flash_init();
    nvs_open("lat", NVS_READWRITE, &nvs);

    if (nvs_get_str(nvs, "ssid", NULL, &length) == ESP_OK) {
        settings.ssid = (char *)malloc(length);
        nvs_get_str(nvs, "ssid", settings.ssid, &length);
    } else {
        settings.ssid = NULL;
    }

    if (nvs_get_str(nvs, "psk", NULL, &length) == ESP_OK) {
        settings.psk = (char *)malloc(length);
        nvs_get_str(nvs, "psk", settings.psk, &length);
    } else {
        settings.psk = NULL;
    }

    if (nvs_get_str(nvs, "name", NULL, &length) == ESP_OK) {
        settings.name = (char *)malloc(length);
        nvs_get_str(nvs, "name", settings.name, &length);
    } else {
        settings.name = strdup("unnamed");
    }

    if (nvs_get_str(nvs, "domain", NULL, &length) == ESP_OK) {
        settings.domain = (char *)malloc(length);
        nvs_get_str(nvs, "domain", settings.domain, &length);
    } else {
        settings.domain = strdup("local");
    }
    if (nvs_get_str(nvs, "password", NULL, &length) == ESP_OK) {
        settings.password = (char *)malloc(length);
        nvs_get_str(nvs, "password", settings.password, &length);
    } else {
        settings.password = strdup("password");
    }
    ArduinoOTA.setHostname(settings.name);
}

void runPrompt() {
    if (priv) {
        Terminal.write("Priv> ");
    } else {
        Terminal.write("Local> ");
    }
    state = State::PromptInput;
}

void runAskPass() {
    // Setting the text to black is a bit of a hack.
    // But, until there's a "no echo" setting then
    // it's the best we can do with the tools we have.
    Terminal.write("Password> \e[30m");
    state = State::Pass;
}

void runPromptInput() {
    LineEditor.setText("");
    LineEditor.edit();
    auto inputLine = LineEditor.get();
    if (inputLine == NULL) {
        state = State::Prompt;
        return;
    }
    if (strlen(inputLine) == 0) {
        state = State::Prompt;
        return;
    }

    if (CP.process((char *)inputLine) == -1) {
        Terminal.println("*** Error: Unknown command");
        state = State::Prompt;
    }
}

void runPassInput() {
    LineEditor.setText("");
    LineEditor.edit();
    auto inputLine = LineEditor.get();
    Terminal.print("\e[37m");
    if (inputLine == NULL) {
        state = State::Prompt;
        return;
    }
    if (strcmp(inputLine, settings.password) == 0) {
        priv = true;
    } else {
        Terminal.println("*** Error: Bad password");
    }
    state = State::Prompt;
}

COMMAND(scan) {
    if (!priv) return -1;
    static char const * ENC2STR[] = { "Open", "WEP", "WPA-PSK", "WPA2-PSK", "WPA/WPA2-PSK", "WPA-ENTERPRISE" };
    Terminal.write("Scanning...");
    Terminal.flush();
    fabgl::suspendInterrupts();
    int networksCount = WiFi.scanNetworks();
    fabgl::resumeInterrupts();
    Terminal.printf("%d network(s) found\r\n", networksCount);
    if (networksCount) {
        Terminal.write   ("\e[90m #\e[4GSSID\e[45GRSSI\e[55GCh\e[60GEncryption\e[37m\r\n");
        for (int i = 0; i < networksCount; ++i)
            Terminal.printf("\e[33m %d\e[4G%s\e[33m\e[45G%d dBm\e[55G%d\e[60G%s\e[37m\r\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i), ENC2STR[WiFi.encryptionType(i)]);
    }
    WiFi.scanDelete();
    state = State::Prompt;
    return 0;
}

void connectToWiFi() {
    if (settings.ssid == NULL) return;
    if (settings.psk == NULL) return;
    Terminal.write("Connecting WiFi...");
    Terminal.flush();
    AutoSuspendInterrupts autoInt;
    WiFi.disconnect(true, true);
    for (int i = 0; i < 2; ++i) {
        WiFi.begin(settings.ssid, settings.psk);
        if (WiFi.waitForConnectResult() == WL_CONNECTED)
            break;
        WiFi.disconnect(true, true);
    }
    if (WiFi.status() == WL_CONNECTED) {
        Terminal.printf("connected to %s, IP is %s\r\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        error = false;
        startOta();
    } else {
        Terminal.write("failed!\r\n");
    }
}

int clientWaitForChar() {
    // not so good...:-)
    while (!client[session].available())
        ;
    return client[session].read();
}


void runTelnet() {
    // process data from remote host (up to 1024 codes at the time)
    for (int i = 0; client[session].available() && i < 1024; ++i) {
        int c = client[session].read();
        if (c == 0xFF) {
            // IAC (Interpret As Command)
            uint8_t cmd = clientWaitForChar();
            uint8_t opt = clientWaitForChar();
            if (cmd == 0xFD && opt == 0x1F) {
                // DO WINDOWSIZE
                client[session].write("\xFF\xFB\x1F", 3); // IAC WILL WINDOWSIZE
                client[session].write("\xFF\xFA\x1F" "\x00\x50\x00\x19" "\xFF\xF0", 9);  // IAC SB WINDOWSIZE 0 80 0 25 IAC SE
            } else if (cmd == 0xFD && opt == 0x18) {
                // DO TERMINALTYPE
                client[session].write("\xFF\xFB\x18", 3); // IAC WILL TERMINALTYPE
            } else if (cmd == 0xFA && opt == 0x18) {
                // SB TERMINALTYPE
                c = clientWaitForChar();  // bypass '1'
                c = clientWaitForChar();  // bypass IAC
                c = clientWaitForChar();  // bypass SE
                client[session].write("\xFF\xFA\x18\x00" "wsvt25" "\xFF\xF0", 12); // IAC SB TERMINALTYPE 0 "...." IAC SE
            } else {
                uint8_t pck[3] = {0xFF, 0, opt};
                if (cmd == 0xFD)  // DO -> WONT
                    pck[1] = 0xFC;
                else if (cmd == 0xFB) // WILL -> DO
                    pck[1] = 0xFD;
                client[session].write(pck, 3);
            }
        } else {
            Terminal.write(c);
        }
    }
    // process data from terminal (keyboard)
    while (Terminal.available()) {
        client[session].write( Terminal.read() );
    }
    // return to prompt?
    if (!client[session].connected()) {
        client[session].stop();
        state = State::Prompt;
    }
}


COMMAND(ping) {
    if (argc != 2) {
        Terminal.println("Usage: ping <host>");
        state = State::Prompt;
        return 10;
    }
    int sent = 0, recv = 0;
    fabgl::ICMP icmp;
    while (true) {

        // CTRL-C ?
        if (Terminal.available() && Terminal.read() == 0x03)
            break;

        int t = icmp.ping(argv[1]);
        if (t >= 0) {
            Terminal.printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\r\n", icmp.receivedBytes(), icmp.hostIP().toString().c_str(), icmp.receivedSeq(), icmp.receivedTTL(), (double)t / 1000.0);
            delay(1000);
            ++recv;
        } else if (t == -2) {
            Terminal.printf("Cannot resolve %s: Unknown host\r\n", argv[1]);
            break;
        } else {
            Terminal.printf("Request timeout for icmp_seq %d\r\n", icmp.receivedSeq());
        }
        ++sent;

    }
    if (sent > 0) {
        Terminal.printf("--- %s ping statistics ---\r\n", argv[1]);
        Terminal.printf("%d packets transmitted, %d packets received, %.1f%% packet loss\r\n", sent, recv, (double)(sent - recv) / sent * 100.0);
    }
    state = State::Prompt;
    return 0;
}

void specialFunction(VirtualKey *vk, bool keyDown) {
    if (*vk == VirtualKey::VK_BREAK) {
        state = State::Prompt;
        return;
    }
}

void beginOTA() {
    state = State::OTA;
    Terminal.clear();
    Terminal.println(INGREEN("Beginning OTA update..."));
}

void endOTA() {
    state = State::Prompt;
    Terminal.println(INGREEN("OTA update completed."));
    state = State::Prompt;
}

void errorOTA(ota_error_t err) {
    state = State::Prompt;
    Terminal.printf(INRED("\r\n\r\n*** ERROR %d while performing OTA update!\r\n"), err);
    state = State::Prompt;
}

void progressOTA(unsigned int total, unsigned int size) {
    Terminal.printf(INYELLOW("\rReceiving: %3d%%..."), (total * 100) / size);
}

void otaProcessor(void *arg) {
    ArduinoOTA.setHostname(settings.name);
    ArduinoOTA.setPassword(settings.password);
    ArduinoOTA.onStart(beginOTA);
    ArduinoOTA.onEnd(endOTA);
    ArduinoOTA.onError(errorOTA);
    ArduinoOTA.onProgress(progressOTA);
    ArduinoOTA.begin();
    while (1) {
        ArduinoOTA.handle();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void startOta() {
    xTaskCreate(otaProcessor, "OTA", 3027, NULL, 1, &otaTask);
}

void stopOta() {
    vTaskDelete(otaTask);
}







COMMAND(openSession) {
    int port = 23;
    if ((argc < 2) || (argc > 3)) {
        Terminal.println("Usage: open <hostname> [port]");
        state = State::Prompt;
        return 10;
    }

    if (argc == 3) {
        port = atoi(argv[2]);
    }

    if ((port <= 0) || (port >= 65535)) {
        Terminal.println("*** Error: Invalid port number");
        state = State::Prompt;
        return 10;
    }

    int freesess = -1;
    for (int i = 0; i < NUM_SESSIONS; i++) {
        if (!client[i].connected()) {
            freesess = i;
            break;
        }
    }

    if (freesess == -1) {
        Terminal.println("*** Error: No free sessions");
        state = State::Prompt;
        return 10;
    }

    session = freesess;

    Terminal.printf("Trying %s...\r\n", argv[1]);
    if (client[session].connect(argv[1], port)) {
        Terminal.printf("Connected to %s\r\n", client[session].remoteIP().toString().c_str());
        state = State::Telnet;
        return 0;
    }

    char *temp = (char *)alloca(strlen(argv[1]) + strlen(settings.domain)+2);
    sprintf(temp, "%s.%s", argv[1], settings.domain);

    if (client[session].connect(temp, port)) {
        Terminal.printf("Connected to %s\r\n", client[session].remoteIP().toString().c_str());
        state = State::Telnet;
        return 0;
    }

    Terminal.println("*** Error: Unable to connect to remote host");
    state = State::Prompt;
    return 10;
}



COMMAND(listSessions) {
    for (int i = 0; i < NUM_SESSIONS; i++) {
        if (client[i].connected()) {
            Terminal.printf("Session %-2d - " INGREEN("Connected to %s") "\r\n", i, client[i].remoteIP().toString().c_str());
        } else {
            Terminal.printf("Session %-2d - " INRED("Disconnected") "\r\n", i);
        }
    }
    state = State::Prompt;
    return 0;
}

COMMAND(reconnectSession) {
    if (argc != 2) {
        Terminal.println("Usage: session <number>");
        state = State::Prompt;
        return 10;
    }

    int sess = atoi(argv[1]);

    if ((sess < 0) || (sess >= NUM_SESSIONS)) {
        Terminal.println("*** Error: Invalid session");
        state = State::Prompt;
        return 10;
    }

    if (!client[sess].connected()) {
        Terminal.println("*** Error: Session not connected");
        state = State::Prompt;
        return 10;
    }

    session = sess;
    state = State::Telnet;
    Terminal.printf("Connected to %s\r\n", client[session].remoteIP().toString().c_str());
    return 0;
}

COMMAND(set) {
    // In unpriv mode there's only one thing we can do: set priv
    if (!priv) {
        if (argc != 2) return -1;
        if (strcmp(argv[1], "priv") != 0) return -1;
        state = State::AskPass;
        return 0;
    }

    if (argc != 3) {
        Terminal.println("Usage: set <key> <value>");
        state = State::Prompt;
        return 10;
    }

    if (strcmp(argv[1], "ssid") == 0) {
        if (settings.ssid != NULL) {
            free(settings.ssid);
        }
        settings.ssid = strdup(argv[2]);
        nvs_set_str(nvs, "ssid", settings.ssid);
        connectToWiFi();
        state = State::Prompt;
        return 0;
    }

    if (strcmp(argv[1], "psk") == 0) {
        if (settings.psk != NULL) {
            free(settings.psk);
        }
        settings.psk = strdup(argv[2]);
        nvs_set_str(nvs, "psk", settings.psk);
        connectToWiFi();
        state = State::Prompt;
        return 0;
    }

    if (strcmp(argv[1], "password") == 0) {
        if (settings.password != NULL) {
            free(settings.password);
        }
        settings.password = strdup(argv[2]);
        nvs_set_str(nvs, "password", settings.password);
        state = State::Prompt;
        return 0;
    }

    if (strcmp(argv[1], "name") == 0) {
        if (settings.name != NULL) {
            free(settings.name);
        }
        settings.name = strdup(argv[2]);
        nvs_set_str(nvs, "name", settings.name);
        state = State::Prompt;
        return 0;
    }

    if (strcmp(argv[1], "domain") == 0) {
        if (settings.domain != NULL) {
            free(settings.domain);
        }
        settings.domain = strdup(argv[2]);
        nvs_set_str(nvs, "domain", settings.domain);
        state = State::Prompt;
        return 0;
    }

    Terminal.println("*** Error: Unknown key");
    state = State::Prompt;
    return 10;
}

COMMAND(info) {
    if (!priv) return -1;

    Terminal.println(INGREEN("WiFi Terminal Version " VERSION " (c) 2020 Majenko Technologies"));
    Terminal.println(INGREEN("FabGL Version " FABGLVER " (c) 2019-2020 Fabrizio Di Vittorio"));
    Terminal.println();
    Terminal.printf("ssid        : %s\r\n", settings.ssid);
    Terminal.printf("psk         : %s\r\n", settings.psk);
    Terminal.printf("name        : %s\r\n", settings.name);
    Terminal.printf("domain      : %s\r\n", settings.domain);
    Terminal.printf("password    : %s\r\n", settings.password);
    Terminal.printf("resolution  : %dx%d\r\n", DisplayController.getScreenWidth(), DisplayController.getScreenHeight());
    Terminal.printf("terminal    : %dx%d\r\n", Terminal.getColumns(), Terminal.getRows());
    Terminal.printf("free dma    : %d\r\n", heap_caps_get_free_size(MALLOC_CAP_DMA));
    Terminal.printf("free memory : %d\r\n", heap_caps_get_free_size(MALLOC_CAP_32BIT));
    if (WiFi.status() == WL_CONNECTED) {
        Terminal.printf("ip address  : %s\r\n", WiFi.localIP().toString().c_str());
    }
    state = State::Prompt;
    return 0;
}

COMMAND(exitPriv) {
    if (!priv) return -1;
    priv = false;
    state = State::Prompt;
    return 0;
}

void setup() {
    initSettings();

    PS2Controller.begin(PS2Preset::KeyboardPort0);
    PS2Controller.keyboard()->setLayout(&fabgl::UKLayout);

    DisplayController.begin();
    DisplayController.setResolution();

    Terminal.begin(&DisplayController, 80, 34, PS2Controller.keyboard());
    Terminal.connectLocally();
    Terminal.setBackgroundColor(Color::Black);
    Terminal.setForegroundColor(Color::White);
    Terminal.clear();
    Terminal.onVirtualKey = specialFunction;
    Terminal.enableCursor(true);

    CP.addCommand("open", openSession);
    CP.addCommand("sessions", listSessions);
    CP.addCommand("session", reconnectSession);
    CP.addCommand("set", set);
    CP.addCommand("info", info);
    CP.addCommand("exit", exitPriv);
    CP.addCommand("scan", scan);
    CP.addCommand("ping", ping);

    connectToWiFi();
}


void loop() {
    switch (state) {

        case State::Prompt:
            runPrompt();
            break;

        case State::PromptInput:
            runPromptInput();
            break;

        case State::AskPass:
            runAskPass();
            break;

        case State::Pass:
            runPassInput();
            break;

        case State::Telnet:
            runTelnet();
            break;

        case State::OTA:
            break;

        default:
            Terminal.write("\r\nState Error\r\n");
            state = State::Prompt;
            break;

    }
}
