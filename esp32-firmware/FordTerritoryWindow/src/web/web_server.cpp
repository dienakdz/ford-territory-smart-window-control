#include "web_server.h"
#include "../logger/logger.h"
#include "../motor/motor_control.h"
#include "../switch/switch_control.h"
#include "../anti_pinch/anti_pinch.h"
#include "../storage/nvs_storage.h"
#include "../calibration/calibration.h"
#include "../voice/voice_control.h"

WebServer webServer(80);
bool apReady = false;
bool wifiStaConnected = false;

String escapeJson(const String &str) {
  String s;
  s.reserve(str.length() + 8);
  for (size_t i = 0; i < str.length(); i++) {
    char c = str[i];
    if (c == '"') s += "\\\"";
    else if (c == '\\') s += "\\\\";
    else if (c == '\n') s += "\\n";
    else if (c == '\r') s += "\\r";
    else if ((unsigned char)c < 0x20) s += ' ';
    else s += c;
  }
  return s;
}

static const char WEB_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="vi"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Ford Window | ESP32-S3</title>
<style>
:root{font-family:system-ui,-apple-system,sans-serif;color-scheme:dark}body{margin:0;background:#0d1728;color:#f0f4fc}
main{max-width:960px;margin:auto;padding:18px 14px}h1{font-size:1.45rem;margin:0 0 9px}
.notice{color:#ffcf8c;line-height:1.45}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(155px,1fr));gap:10px}
.card{background:#1a2941;border:1px solid #314562;border-radius:11px;padding:12px}
.label{font-size:.78rem;color:#a7bbd6}.value{font-weight:750;font-size:1.17rem;margin-top:7px;overflow-wrap:anywhere}
pre{white-space:pre-wrap;min-height:220px;max-height:450px;overflow:auto;background:#07101e;padding:12px;border:1px solid #314562;border-radius:10px;font-size:.82rem;line-height:1.45}
.small{font-size:.85rem;color:#b7c7da;line-height:1.55}h2{font-size:1.05rem;margin-top:18px}
.voice-box{margin:16px 0;padding:14px;background:#13233a;border:1px solid #0284c7;border-radius:12px;display:flex;align-items:center;gap:14px;flex-wrap:wrap}
.btn-voice{background:#0284c7;color:#fff;border:none;padding:12px 22px;border-radius:9px;font-weight:750;font-size:1.05rem;cursor:pointer;transition:all .15s}
.btn-voice:hover{background:#0369a1}.btn-voice:disabled{opacity:0.5;cursor:not-allowed}
</style></head><body><main>
<h1>Ford Territory — Window Control</h1>
<p class="notice">BẢN THỬ NGHIỆM. Ngưỡng ACS712 +40mV. Chống kẹt tự động đảo chiều hạ kính 1 giây.</p>

<div class="voice-box">
  <button id="btnVoice" class="btn-voice" onclick="triggerVoice()">🎤 BẤM ĐỂ RA LỆNH GIỌNG NÓI (2s / Dual-Core)</button>
  <span id="voiceNotice" class="small" style="color:#7dd3fc">Đồng bộ 2 giây toàn hệ thống. Tự động nhận diện lệnh ngắt khi kính chạy!</span>
</div>

<div class="grid">
<div class="card"><div class="label">Mode</div><div class="value" id="mode">—</div></div>
<div class="card"><div class="label">Giọng nói AI</div><div class="value" id="voice" style="color:#38bdf8">—</div></div>
<div class="card"><div class="label">Calibration</div><div class="value" id="phase">—</div></div>
<div class="card"><div class="label">Motor</div><div class="value" id="motor">—</div></div>
<div class="card"><div class="label">Công tắc</div><div class="value" id="sw">—</div></div>
<div class="card"><div class="label">Công tắc ADC GPIO1</div><div class="value" id="swmv">—</div></div>
<div class="card"><div class="label">ACS712 ADC GPIO10</div><div class="value" id="acsmv">—</div></div>
<div class="card"><div class="label">Baseline đã lưu</div><div class="value" id="base">—</div></div>
<div class="card"><div class="label">Ngưỡng thử (BASE + 40)</div><div class="value" id="thresh">—</div></div>
<div class="card"><div class="label">Mẫu CAL / kết quả</div><div class="value" id="samples">—</div></div>
<div class="card"><div class="label">Hành trình</div><div class="value" id="limits">—</div></div>
<div class="card"><div class="label">Wi-Fi iPhone (Static)</div><div class="value" id="wifi_sta">—</div></div>
</div>
<h2>Cách hiệu chuẩn (không cần USB)</h2>
<p class="small">Lúc khởi động hoặc bình thường: <b>AUTO DOWN giữ 2 giây → THẢ → AUTO UP giữ 2 giây → THẢ.</b><br>
ESP32 tự hạ tới BOTTOM. Sau đó <b>ấn và giữ UP</b> để kính chạy lên cho tới công tắc hành trình TOP.<br>
ESP32 tự tính BASELINE, tự lưu Flash và tự về NORMAL.</p>
<h2>Log</h2><pre id="log">Đang kết nối...</pre><p id="conn" class="small"></p>
<script>
const e=id=>document.getElementById(id);
let inFlight=false;

async function triggerVoice(){
 const btn=e('btnVoice'),st=e('voiceNotice');
 btn.disabled=true;btn.style.opacity='0.6';
 st.textContent='🎙️ Đang thu âm 2s... Hãy nói: "nâng kính" / "hạ kính" / "dừng lại"!';
 try{
  const r=await fetch('/api/voice/record',{method:'POST'});
  const d=await r.json();
  st.textContent='Hệ thống đang mở cửa sổ 10s lắng nghe! Bạn có thể hô "dừng" bất cứ lúc nào khi kính chạy.';
 }catch(err){
  st.textContent='Lỗi kết nối: '+err.message;
 }finally{
  setTimeout(()=>{btn.disabled=false;btn.style.opacity='1';}, 2000);
 }
}

async function tick(){
 if(inFlight)return;
 inFlight=true;
 try{
  const r=await fetch('/api/status',{cache:'no-store'});if(!r.ok)throw Error('HTTP '+r.status);const s=await r.json();
  e('mode').textContent=s.mode;e('voice').textContent=s.voice||'CHƯA CÓ';e('phase').textContent=s.phase;e('motor').textContent=s.motor;
  e('sw').textContent=s.switch;e('swmv').textContent=s.switch_mv+' mV';e('acsmv').textContent=s.acs_mv+' mV';
  e('base').textContent=s.saved_valid?s.baseline_mv+' mV':'CHƯA CÓ';
  e('thresh').textContent=s.saved_valid?s.threshold_mv+' mV (thử nghiệm)':'CHƯA CÓ';
  e('samples').textContent=s.samples+' mẫu / '+s.result;
  e('limits').textContent=(s.top?'TOP CHẠM':'TOP OK')+' / '+(s.bottom?'BOTTOM CHẠM':'BOTTOM OK');
  e('wifi_sta').textContent=s.wifi_sta;
  e('conn').textContent='ESP32 IP: '+s.ip+' | Realtime Live';
  if(s.lines){
    const el=e('log'),follow=el.scrollHeight-el.scrollTop-el.clientHeight<60;
    el.textContent=s.lines.join('\n')||'(trống)';
    if(follow) el.scrollTop=el.scrollHeight;
  }
 }catch(err){e('conn').textContent='Đang cập nhật... '+err.message;}
 finally{inFlight=false;setTimeout(tick,100);}
}tick();
</script></main></body></html>
)HTML";

void sendStatus() {
  String s;
  s.reserve(4500);
  s += "{\"mode\":\"" + String(modeName()) + "\",";
  s += "\"voice\":\"" + escapeJson(lastVoiceResult) + "\",";
  s += "\"phase\":\"" + String(phaseName()) + "\",";
  s += "\"motor\":\"" + String(motorName()) + "\",";
  s += "\"switch\":\"" + String(switchName(stableSwitchMode)) + "\",";
  s += "\"switch_mv\":" + String(switchVoltageMv) + ",";
  s += "\"acs_mv\":" + String(latestAcsMv) + ",";
  s += "\"baseline_mv\":" + String(baselineValid ? savedCal.baselineMv : 0) + ",";
  s += "\"margin_mv\":" + String(ANTI_PINCH_MARGIN_MV) + ",";
  s += "\"threshold_mv\":" + String(baselineValid ? savedCal.baselineMv + ANTI_PINCH_MARGIN_MV : 0) + ",";
  s += "\"saved_valid\":" + String(baselineValid ? "true" : "false") + ",";
  s += "\"samples\":" + String(calSampleCount) + ",";
  s += "\"result\":\"" + escapeJson(lastCalResult) + "\",";
  s += "\"top\":" + String(topReached() ? "true" : "false") + ",";
  s += "\"bottom\":" + String(bottomReached() ? "true" : "false") + ",";

  String ipDesc;
  if (WiFi.status() == WL_CONNECTED) {
    ipDesc = WiFi.localIP().toString() + " (iPhone) | " + WiFi.softAPIP().toString() + " (AP)";
  } else {
    ipDesc = WiFi.softAPIP().toString() + " (AP)";
  }
  s += "\"ip\":\"" + ipDesc + "\",";
  s += "\"wifi_sta\":\"" + String(WiFi.status() == WL_CONNECTED ? "Đã kết nối (172.20.10.3)" : "Chờ iPhone...") + "\",";
  s += "\"clients\":" + String(WiFi.softAPgetStationNum()) + ",";
  s += "\"lines\":[";
  size_t count = logCount > 35 ? 35 : logCount;
  size_t first = (logNext + LOG_CAPACITY - count) % LOG_CAPACITY;
  for (size_t i = 0; i < count; i++) {
    if (i) s += ',';
    s += '"';
    s += escapeJson(logRing[(first + i) % LOG_CAPACITY]);
    s += '"';
  }
  s += "]}";
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send(200, "application/json; charset=utf-8", s);
}

void sendLogs() {
  String s = "{\"lines\":[";
  s.reserve(13000);
  size_t first = (logNext + LOG_CAPACITY - logCount) % LOG_CAPACITY;
  for (size_t i = 0; i < logCount; i++) {
    if (i) s += ',';
    s += '"';
    s += escapeJson(logRing[(first + i) % LOG_CAPACITY]);
    s += '"';
  }
  s += "]}";
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send(200, "application/json; charset=utf-8", s);
}

void startWiFi() {
  // Khoi chay che do kep WIFI_AP_STA
  WiFi.mode(WIFI_AP_STA);

  // 1. SoftAP du phong (http://192.168.4.1)
  IPAddress ip(192, 168, 4, 1), mask(255, 255, 255, 0);
  WiFi.softAPConfig(ip, ip, mask);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  // 2. Giai phap 1: Thiet lap IP tinh va ket noi toi iPhone Hotspot
  WiFi.config(STA_STATIC_IP, STA_GATEWAY, STA_SUBNET, STA_DNS);
  WiFi.begin(STA_SSID, STA_PASSWORD);

  apReady = true;

  webServer.on("/", HTTP_GET, []() {
    webServer.sendHeader("Cache-Control", "no-store");
    webServer.send_P(200, "text/html; charset=utf-8", WEB_PAGE);
  });
  webServer.on("/api/status", HTTP_GET, sendStatus);
  webServer.on("/api/logs", HTTP_GET, sendLogs);

  // Endpoint kich hoat thu am giong noi tu Web Dashboard (Non-blocking Dual-Core trigger)
  webServer.on("/api/voice/record", HTTP_POST, []() {
    triggerVoiceListen();
    String json = "{\"status\":\"ok\",\"message\":\"Listening on Core 0\"}";
    webServer.sendHeader("Cache-Control", "no-store");
    webServer.send(200, "application/json; charset=utf-8", json);
  });

  webServer.onNotFound([]() { webServer.send(404, "text/plain", "Not found"); });
  webServer.begin();

  logEvent(String("[WIFI] AP du phong: ") + AP_SSID + " | http://192.168.4.1");
  logEvent(String("[WIFI] Dang ket noi iPhone: ") + STA_SSID + " (Static IP: 172.20.10.3)...");
}

static unsigned long lastWiFiCheckMs = 0;
static unsigned long lastReconnectAttemptMs = 0;

void checkWiFiStatus() {
  unsigned long now = millis();
  if (now - lastWiFiCheckMs < 2000) {
    return;
  }
  lastWiFiCheckMs = now;

  bool connected = (WiFi.status() == WL_CONNECTED);
  if (connected && !wifiStaConnected) {
    wifiStaConnected = true;
    logEvent("[WIFI] DA KET NOI IPHONE HOTSPOT!");
    logEvent(String("[WIFI] Web Dashboard tren Laptop: http://") + WiFi.localIP().toString());
    testVoiceServerPing();
  } else if (!connected && wifiStaConnected) {
    wifiStaConnected = false;
    logEvent("[WIFI] Mat ket noi iPhone Hotspot. Vui long kiem tra 4G.");
  }

  // Tu dong thu ket noi lai moi 5 giay neu chua ket noi duoc
  // (Dac biet giup tu dong bat song ngay khi bat Hotspot iPhone sau)
  if (!connected && (now - lastReconnectAttemptMs >= 5000)) {
    lastReconnectAttemptMs = now;
    WiFi.disconnect();
    WiFi.config(STA_STATIC_IP, STA_GATEWAY, STA_SUBNET, STA_DNS);
    WiFi.begin(STA_SSID, STA_PASSWORD);
  }
}

void handleWebClient() {
  if (apReady) {
    webServer.handleClient();
  }
}
