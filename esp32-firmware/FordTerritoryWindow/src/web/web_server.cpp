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
:root{font-family:system-ui,-apple-system,sans-serif;color-scheme:dark}body{margin:0;background:#0d1728;color:#f0f4fc;line-height:1.4}
main{max-width:1380px;margin:auto;padding:12px 16px}
header{margin-bottom:12px;border-bottom:1px solid #23354d;padding-bottom:8px}
h1{font-size:1.35rem;margin:0 0 4px}
.notice{color:#ffcf8c;font-size:.85rem;margin:0}
.layout{display:grid;grid-template-columns:1fr 1fr;gap:14px;align-items:start}
@media(max-width:880px){.layout{grid-template-columns:1fr}}
.col-left{display:flex;flex-direction:column;gap:10px}
.col-right{display:flex;flex-direction:column;gap:8px}
.voice-box{padding:12px;background:#13233a;border:1px solid #0284c7;border-radius:10px;display:flex;flex-direction:column;gap:8px}
.cal-box{padding:12px;background:#20180a;border:1px solid #f59e0b;border-radius:10px;display:flex;flex-direction:column;gap:8px}
.btn-voice{background:#0284c7;color:#fff;border:none;padding:10px 18px;border-radius:8px;font-weight:750;font-size:1rem;cursor:pointer;transition:all .15s;text-align:left}
.btn-voice:hover{background:#0369a1}.btn-voice:disabled{opacity:0.5;cursor:not-allowed}
.btn-cal{background:#d97706;color:#fff;border:none;padding:10px 18px;border-radius:8px;font-weight:750;font-size:1rem;cursor:pointer;transition:all .15s;text-align:left}
.btn-cal:hover{background:#b45309}.btn-cal:disabled{opacity:0.5;cursor:not-allowed}
.btn-abort{background:#dc2626;color:#fff;border:none;padding:10px 16px;border-radius:8px;font-weight:750;font-size:.95rem;cursor:pointer;transition:all .15s}
.btn-abort:hover{background:#b91c1c}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:8px}
.card{background:#1a2941;border:1px solid #314562;border-radius:9px;padding:10px}
.label{font-size:.74rem;color:#a7bbd6}
.value{font-weight:750;font-size:1.05rem;margin-top:4px;overflow-wrap:anywhere}
.small{font-size:.82rem;color:#b7c7da;line-height:1.45}
.log-box{display:flex;flex-direction:column;gap:8px}
.log-header{display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:8px}
h2{font-size:1rem;margin:0}
pre{white-space:pre-wrap;height:calc(100vh - 120px);min-height:500px;max-height:850px;overflow-y:auto;background:#07101e;padding:12px;border:1px solid #314562;border-radius:10px;font-size:.82rem;line-height:1.45;margin:0}
</style></head><body><main>
<header>
  <h1>Ford Territory — Window Control</h1>
  <p class="notice">BẢN THỬ NGHIỆM. Ngưỡng ACS712 +20mV. Chống kẹt tự động đảo chiều hạ kính 1 giây.</p>
</header>

<div class="layout">
  <!-- CỘT TRÁI: ĐIỀU KHIỂN & THÔNG SỐ (col-6) -->
  <div class="col-left">
    <div class="voice-box">
      <button id="btnVoice" class="btn-voice" onclick="triggerVoice()">🎤 BẤM ĐỂ RA LỆNH GIỌNG NÓI (2s / Dual-Core)</button>
      <span id="voiceNotice" class="small" style="color:#7dd3fc">Đồng bộ 2 giây toàn hệ thống. Tự động nhận diện lệnh ngắt khi kính chạy!</span>
    </div>

    <div class="cal-box">
      <div style="display:flex;align-items:center;gap:10px;flex-wrap:wrap">
        <button id="btnCal" class="btn-cal" onclick="triggerCal()">⚙️ BẮT ĐẦU HIỆU CHUẨN (CAL MODE)</button>
        <button id="btnAbortCal" class="btn-abort" onclick="abortCalAction()" style="display:none">❌ HỦY CAL</button>
      </div>
      <span id="calNotice" class="small" style="color:#fcd34d">Nhấn nút để vào CAL tự động tính Baseline (không cần giữ AUTO DOWN 2s + AUTO UP 2s).</span>
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
      <div class="card"><div class="label">Ngưỡng thử (BASE + 20)</div><div class="value" id="thresh">—</div></div>
      <div class="card"><div class="label">Mẫu CAL / kết quả</div><div class="value" id="samples">—</div></div>
      <div class="card"><div class="label">Hành trình</div><div class="value" id="limits">—</div></div>
      <div class="card"><div class="label">Wi-Fi iPhone</div><div class="value" id="wifi_sta">—</div></div>
    </div>

    <div class="card" style="background:#131c2e;margin-top:2px">
      <div class="label" style="font-weight:700;color:#93c5fd;margin-bottom:4px">💡 Cách hiệu chuẩn (không cần USB)</div>
      <p class="small" style="margin:0">
        <b>Cách 1:</b> Click nút <b>⚙️ BẮT ĐẦU HIỆU CHUẨN (CAL MODE)</b> ở trên.<br>
        <b>Cách 2:</b> Giữ AUTO DOWN 2s → Thả → Giữ AUTO UP 2s.<br>
        👉 Kính tự hạ đáy → <b>Ấn và giữ UP</b> để nâng lên TOP học Baseline mới.
      </p>
    </div>
  </div>

  <!-- CỘT PHẢI: LOGS THỜI GIAN THỰC (col-6) -->
  <div class="col-right">
    <div class="log-box">
      <div class="log-header">
        <h2>📋 Realtime Logs</h2>
        <span id="conn" class="small" style="color:#38bdf8"></span>
      </div>
      <pre id="log">Đang kết nối...</pre>
    </div>
  </div>
</div>

<script>
const e=id=>document.getElementById(id);
let inFlight=false;
let calActivePrev=false;

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

async function triggerCal(){
 const btn=e('btnCal'),st=e('calNotice');
 btn.disabled=true;btn.style.opacity='0.6';
 st.textContent='⏳ Đang gửi yêu cầu vào chế độ CAL...';
 try{
  const r=await fetch('/api/cal/start',{method:'POST'});
  const d=await r.json();
  if(d.status==='ok'){
    st.textContent='✅ Đã vào chế độ CAL! Kính đang tự hạ xuống BOTTOM...';
  }else{
    st.textContent='⚠️ '+d.message;
  }
 }catch(err){
  st.textContent='Lỗi kết nối: '+err.message;
 }finally{
  setTimeout(()=>{btn.disabled=false;btn.style.opacity='1';}, 1500);
 }
}

async function abortCalAction(){
 const st=e('calNotice');
 try{
  const r=await fetch('/api/cal/abort',{method:'POST'});
  const d=await r.json();
  st.textContent='Đã hủy CAL, quay về NORMAL.';
 }catch(err){
  st.textContent='Lỗi kết nối: '+err.message;
 }
}

async function tick(){
 if(inFlight)return;
 inFlight=true;
 const ctl=new AbortController(),tid=setTimeout(()=>ctl.abort(),1500);
 try{
  const r=await fetch('/api/status',{cache:'no-store',signal:ctl.signal});
  clearTimeout(tid);
  if(!r.ok)throw Error('HTTP '+r.status);const s=await r.json();
  e('mode').textContent=s.mode;e('voice').textContent=s.voice||'CHƯA CÓ';e('phase').textContent=s.phase;e('motor').textContent=s.motor;
  e('sw').textContent=s.switch;e('swmv').textContent=s.switch_mv+' mV';e('acsmv').textContent=s.acs_mv+' mV';
  e('base').textContent=s.saved_valid?s.baseline_mv+' mV':'CHƯA CÓ';
  e('thresh').textContent=s.saved_valid?s.threshold_mv+' mV (thử nghiệm)':'CHƯA CÓ';
  e('samples').textContent=s.samples+' mẫu / '+s.result;
  e('limits').textContent=(s.top?'TOP CHẠM':'TOP OK')+' / '+(s.bottom?'BOTTOM CHẠM':'BOTTOM OK');
  e('wifi_sta').textContent=s.wifi_sta;
  e('conn').textContent='ESP32 IP: '+s.ip+' | Realtime Live';

  if(s.cal_mode){
    calActivePrev=true;
    e('btnCal').disabled=true;e('btnCal').style.opacity='0.6';
    e('btnAbortCal').style.display='inline-block';
    if(s.phase.indexOf('DOWN')!==-1){
      e('calNotice').innerHTML='⏳ <b>Đang tự hạ kính xuống đáy (BOTTOM)...</b> Vui lòng đợi!';
    }else if(s.phase.indexOf('HOLD')!==-1||s.phase.indexOf('WAIT')!==-1){
      e('calNotice').innerHTML='👉 <b>ĐÃ XUỐNG ĐÁY!</b> Hãy <b>ấn và GIỮ công tắc UP</b> trên xe để nâng lên TOP học Baseline.';
    }else if(s.phase.indexOf('RECORDING')!==-1||s.phase.indexOf('UP')!==-1){
      e('calNotice').innerHTML='📈 <b>Đang ghi nhận mẫu dòng ACS712...</b> Tiếp tục GIỮ nút UP cho đến khi chạm TOP!';
    }
  }else{
    e('btnCal').disabled=false;e('btnCal').style.opacity='1';
    e('btnAbortCal').style.display='none';
    if(calActivePrev){
      calActivePrev=false;
      e('calNotice').innerHTML='✅ <b>Đã hoàn tất / thoát CAL.</b> Nhấn nút để vào lại CAL khi cần tính Baseline mới.';
    }
  }

  if(s.lines){
    const el=e('log'),follow=el.scrollHeight-el.scrollTop-el.clientHeight<60;
    el.textContent=s.lines.join('\n')||'(trống)';
    if(follow) el.scrollTop=el.scrollHeight;
  }
 }catch(err){e('conn').textContent='Đang cập nhật... '+err.message;}
 finally{inFlight=false;setTimeout(tick,250);}
}tick();
</script></main></body></html>
)HTML";

void sendStatus() {
  String s;
  s.reserve(2500);
  s += "{\"mode\":\"" + String(modeName()) + "\",";
  s += "\"cal_mode\":" + String(calMode ? "true" : "false") + ",";
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
  size_t count = logCount > 20 ? 20 : logCount;
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

  // Endpoint kich hoat che do CAL truc tiep tu Web Dashboard
  webServer.on("/api/cal/start", []() {
    if (startCal()) {
      String json = "{\"status\":\"ok\",\"message\":\"Da kich hoat CAL. Kinh dang tu ha xuong BOTTOM...\"}";
      webServer.sendHeader("Cache-Control", "no-store");
      webServer.send(200, "application/json; charset=utf-8", json);
    } else {
      String json = "{\"status\":\"error\",\"message\":\"He thong da o trong che do CAL.\"}";
      webServer.sendHeader("Cache-Control", "no-store");
      webServer.send(200, "application/json; charset=utf-8", json);
    }
  });

  // Endpoint huy che do CAL tu Web Dashboard
  webServer.on("/api/cal/abort", []() {
    if (!calMode) {
      String json = "{\"status\":\"error\",\"message\":\"He thong khong o che do CAL.\"}";
      webServer.sendHeader("Cache-Control", "no-store");
      webServer.send(200, "application/json; charset=utf-8", json);
      return;
    }
    abortCal("Huy CAL tu Web Dashboard");
    String json = "{\"status\":\"ok\",\"message\":\"Da huy CAL, quay ve NORMAL.\"}";
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
