# Project Context for Antigravity — Ford Territory Smart Power Window

> Use this document as historical project context, not as proof of the current code state. Read the real workspace and source files before editing. All repository paths, pin functions, thresholds, APIs, and hardware behavior must be verified against the checked-out project.
>
> Last summarized: 2026-11-07. Language: communicate with the project owner in Vietnamese; use conventional English names in source code, comments and technical documentation when appropriate.

## 1. Project identity and goal

- Project type: final-year graduation project in Automotive Engineering Technology.
- Scope: prototype smart electric **front passenger window** control for **Ford Territory** (not every vehicle window).
- Intended capabilities:
  1. Physical/manual window UP/DOWN control.
  2. Automatic window operation (AUTO).
  3. Anti-pinch protection, including safe handling of upper/lower travel limits.
  4. Vietnamese speech commands (UP, DOWN, STOP; potentially AUTO later).
  5. ESP32-S3 controller, a laptop-based voice-processing server over the same Wi-Fi/LAN, and eventual integration with the motor-control state machine.
- The goal is a safe demonstrator. Do not treat voice control as permission to bypass any physical protection.

## 2. Hardware inventory (previously discussed)

| Part | Role / relevant details |
|---|---|
| ESP32-S3 N16R8 | Main MCU; 16 MB Flash, 8 MB PSRAM |
| INMP441 | I2S digital microphone |
| MAX98357A | I2S audio amplifier for feedback (integration status unverified) |
| 4-ohm 3-W speaker | Audio feedback (integration status unverified) |
| BTS7960 | Motor driver for window motor (not yet linked to voice firmware) |
| ACS712 | Motor-current sensing for anti-pinch strategy |
| Physical UP/DOWN switch | Manual control |
| Upper/lower limit switches | Travel limit detection |
| Window motor / suitable power supply | Bench/car-window actuator; power wiring and protections must be verified |

Previously mentioned GPIO allocation (historical; verify on actual schematics/firmware):

- INMP441: SCK/BCLK -> GPIO14; WS/LRCLK -> GPIO15; SD/DIN -> GPIO16; L/R pin tied to GND; power at 3.3 V and common GND.
- BTS7960: GPIO4, GPIO5, GPIO6, GPIO7 (exact function-to-pin mapping **not confirmed here**; inspect `chongket.ino`).
- Upper/lower limit switches: GPIO8 and GPIO9 (verify polarity and which is upper vs lower).
- Physical switch: GPIO1.
- ACS712 analog output: GPIO10 (verify analog input suitability, scaling and sensor calibration).
- MAX98357A/speaker GPIO mapping: not confirmed. Do not invent.

## 3. Chosen architecture (working voice prototype)

```text
Vietnamese speech
    -> INMP441 microphone
    -> ESP32-S3 I2S sampling / PCM audio buffer
    -> Wi-Fi (same LAN as laptop)
    -> HTTP POST /recognize
    -> Python Flask server on laptop
    -> Google SpeechRecognition / STT, language vi-VN (requires internet)
    -> map recognized text to a restricted command
    -> plain command response: UP | DOWN | STOP | UNKNOWN
    -> ESP32 prints result
    -> [NOT YET INTEGRATED] validated command -> safe window motor state machine
```

Important distinction: LAN connects ESP32 to Flask; **Google STT is an online external service**, not a fully offline, laptop-local STT model. An earlier plan considered offline Vietnamese STT / Faster-Whisper, but the currently tested implementation uses Google STT.

Historical laptop IPv4: `172.20.10.2`. Flask default port: `5000`, binding `0.0.0.0:5000`. Treat the IP as **environment-specific**, not a fixed deployment constant. ESP32 and laptop were on the same LAN/hotspot.

Known HTTP routes:

- `GET http://172.20.10.2:5000/ping` -> text `PONG`.
- `POST http://172.20.10.2:5000/recognize`, header `Content-Type: application/octet-stream`, body raw mono PCM16 little-endian audio -> a command string such as `UP`, `DOWN`, `STOP`, or `UNKNOWN`.
- Inspect the actual files before assuming there are any other routes or JSON schemas.

Audio settings from the tested firmware:

- 16,000 samples/second, mono, signed 16-bit PCM little-endian.
- Recording duration: 3 seconds.
- Expected body size: `16000 * 3 * 2 = 96000 bytes`.
- Prior firmware `MIC_GAIN` was `8` and was suspected of causing saturation; do **not** assume a different value was already tested or that merely lowering it solves every issue.

## 4. Known source files / local developer environment

- `ESP32_VOICE_LAN.ino`: Wi-Fi + INMP441 + test interactions with Flask. Serial input `P` calls `/ping`, `R` triggers 3-second recording and POST `/recognize`. A comment in the previous version explicitly stated that **this firmware does not control the motor**.
- `server_voice.py`: Python Flask voice endpoint, raw audio handling, level diagnostics (peak/RMS), Google STT with `vi-VN`, command mapping, potentially diagnostic recording `last_voice.wav`.
- `01_test_micro_INMP441.ino`: separate microphone test firmware.
- `chongket.ino`: earlier standalone/manual/AUTO/anti-pinch motor-control sketch was discussed, but its actual contents and current correctness were **not available for full verification**. Inspect the real file before drawing conclusions.
- `main.py`: earlier laptop-microphone STT experiment; exact status in current workspace unknown.
- Windows workstation; PyCharm/VS Code + Python virtual environment (`.venv`) were used; Arduino-style ESP32 `.ino` source.

Proposed GitHub organization (a suggestion from earlier chats, **not a confirmed existing remote or directory tree**):

```text
ford-territory-smart-window-control/
  esp32-firmware/
    ESP32_VOICE_LAN.ino
    [possibly chongket.ino and microphone experiments]
  voice-processing-server/
    server_voice.py
    requirements.txt
  docs/
  README.md
  .gitignore
```

GitHub username: `dienakdz` (`https://github.com/dienakdz`). Proposed repository name: `ford-territory-smart-window-control`. Do not assume the repo was already created or the files placed into the proposed directories; discover the actual workspace and `git remote -v`.

Do not commit `.venv/`, `__pycache__/`, `.env`, secrets, raw debugging audio (`last_voice.wav`), or device-specific credentials. Do not reveal Wi-Fi passwords or private data in summaries.

## 5. Tests already performed (historical evidence)

1. Laptop Python STT, using its own microphone, recognized the phrase **`nâng kính`** and mapped it to `COMMAND: UP`.
2. Other Vietnamese pronunciations/misrecognitions, e.g. `hạ cánh ghế phụ`, `hạ kính giới phụ`, `hạt trứng ghế phụ`, often mapped to `UNKNOWN`. Recognition accuracy was accepted only as an initial prototype.
3. ESP32-S3 connected to laptop Flask over Wi-Fi. Sending `P` resulted in `PONG`.
4. Sending `R` recorded and POSTed a 3-second / 96,000-byte audio sample; HTTP response code `200` was observed.
5. The system produced `COMMAND: UP` at least once and often `COMMAND: UNKNOWN`.
6. Example recordings showed `peak=32768` (likely signed 16-bit magnitude saturation) and RMS around `20479.5` or `17139.6`; firmware logged `[WARN] Audio clipped; lower MIC_GAIN if frequent`. The exact acoustic quality / gain path must be measured before deciding the fix.
7. The implemented voice test did **not** actuate the motor. A successful HTTP request proves transport, not accuracy, anti-pinch correctness, or integrated motor control.

Potentially useful diagnostics to inspect: actual recognized Google text before mapping (`[GOOGLE] Text` if present), audio peak/RMS/distribution and WAV listening, I2S sample conversion / bit shift / clipping, environment noise, network timeout, STT service errors, and exact command normalization.

## 6. Functional requirements / desired future behavior

- Speech commands in Vietnamese for raising/lowering/stopping the **passenger** window.
- Manual controls, AUTO logic, upper/lower limit switches and anti-pinch behavior must remain independently functional, even if Wi-Fi, Flask or Google STT fail.
- `UNKNOWN`, invalid response, timeout, communication failure or ambiguous transcript **must never cause motor movement**.
- A STOP request should stop motion as appropriate within a reviewed and tested safety architecture; physical limit and anti-pinch protections must always take precedence over remote/voice commands.
- User wants **wake-word/voice activation** similar to Siri: an always-available lightweight listening mechanism that awakens command capture, **not continuous 3-second audio uploads or an always-on full STT loop**. No wake-word solution has been implemented or verified. Compare feasible ESP32-local detection, server-side VAD/wake word, and realistic Vietnamese-language support without claiming unsupported offline capabilities.
- ESP32 can be powered separately instead of over the USB flashing cable and still talk to the laptop over LAN if powered correctly and the Wi-Fi/network/server stay available. Power from a vehicle battery requires a suitable, protected regulator, stable voltage, correct grounds, reverse-polarity/transient protection and safe bench testing first. USB is not the communications path to Flask.
- MAX98357A/speaker feedback and an optional display were discussed as possibilities but **were not confirmed implemented**.

## 7. High-priority engineering problems

### A. Improve speech capture and recognition, before motor integration

- Read `ESP32_VOICE_LAN.ino` and `server_voice.py` line by line. Verify sample format, correct 24-bit I2S microphone capture converted to PCM16, channel selection, sign handling, sample rate, buffers, and timing.
- Investigate and fix clipping: do not merely suppress warning. Inspect `MIC_GAIN`, shift/scaling, saturation and microphone positioning. Add reproducible diagnostics and sample capture where useful.
- Make Flask speech recognition robust to normal Vietnamese variations using safe normalization (case, punctuation, spacing and reasonable accent/phrase alternatives). Log the real STT transcript for debugging while respecting privacy. Prefer false rejection (`UNKNOWN`) over incorrect window motion. Do not map arbitrary fuzzy/ambiguous speech directly into movement.
- Give clear diagnostics for poor audio, STT network failures and timeout; keep response contract backwards compatible unless changed deliberately and on both sides.

### B. Review / integrate motor firmware carefully

- Locate and inspect the actual `chongket.ino`, if present, and document its real state machine, pin mappings, physical switch logic, AUTO behavior, current thresholds/calibration, debounce, upper/lower limit handling and emergency-stop policy.
- Do not assume this file is verified, physically safe, or even included in the new repository.
- Propose a clean command interface/state machine integrating manual actions and validated voice actions without breaking existing protection layers. No independent motor-control code path may bypass upper/lower limits or anti-pinch.
- Provide dry-run mocks and unit tests before driving BTS7960. Never energize a physical motor or modify safety-critical pin output behavior as an unattended side effect; obtain owner approval for a controlled bench test after code review.

### C. Wake word (separate phase)

- Evaluate continuous low-power/local wake detection options and their feasibility on ESP32-S3 N16R8 for Vietnamese; distinguish wake-word detection from complete Vietnamese STT.
- On wake detection, capture a short command interval and use the existing Flask request path; define timeouts, retrigger and cancel behavior.
- Do not add continuous voice uploads without explicit approval.

## 8. Instructions to Antigravity agent for the **first run**

You are an engineering coding agent operating on the user's **local checked-out workspace**, not on the historical snapshot above. Your job is to understand the project, validate its status, and propose or implement safe, testable improvements.

1. First inspect the active workspace: list files and folders, identify `.ino`, `.py`, requirements, docs, Git status/remotes, environment config and build/test commands. Confirm what exists versus what was merely proposed in this document.
2. Summarize the real system and its status in Vietnamese, separating **implemented/tested**, **implemented but not verified**, **proposed**, and **missing**. Identify conflicts between this historical context and current code.
3. Trace end-to-end `ESP32 -> raw audio -> Flask -> STT -> command` and evaluate audio format, command parser, response and error behavior.
4. Prioritize fixing the `UNKNOWN`/clipping issue with small, auditable changes and tests or simulated inputs; do not modify the motor path until a separate safety review.
5. If code changes are requested, show the files to be changed and a concise plan, then implement in small logical commits/patches. Run available checks and report **actual** results (never claim hardware tests were run if there was no hardware access).
6. If you need a new dependency, a credential, Git push, risky file deletion or physical-device action, ask for approval. Never print or commit secrets.
7. For any motor integration, preserve manual operation, hard limits, anti-pinch, failure handling, and fail-safe defaults. Test first with mock GPIO/motor or actuator disconnected.
8. Maintain a short `docs/PROJECT_STATUS.md` (if appropriate) with what is confirmed by actual code and tests, unresolved problems and next concrete steps.
9. Communicate with the owner in Vietnamese, in concrete incremental steps. Avoid rewriting the whole architecture unnecessarily. Ask only when genuinely blocked by missing hardware facts or a safety-critical uncertainty.

## 9. What I want you to do now

**Initial assignment: repository audit + action plan, not uncontrolled hardware execution.**

- Inspect the actual workspace and map the real source tree.
- Read the ESP32 firmware and Python Flask server first; locate `chongket.ino` if it exists.
- Produce a concise technical report: verified architecture, working functions, unimplemented functions, reproducible bugs, suspected root causes, and prioritized next fixes.
- Start with an implementable plan for **audio clipping and frequent `UNKNOWN`**, with a no-motor test strategy.
- Ask before deploying firmware to connected hardware or actuating the motor.
