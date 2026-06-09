#include "services/config_server.h"

#include <WebServer.h>
#include <WiFi.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef WM_MDNS
#include <ESPmDNS.h>
#endif

#include "config.h"
#include "hardware/display.h"
#include "services/audio.h"
#include "services/radar_location.h"
#include "services/wifi_setup.h"
#include "services/wifi_store.h"
#include "ui/radar_display.h"
#include "ui/radar_range.h"

namespace services::config_server {

namespace {

WebServer s_server(80);
bool s_started = false;

// Self-contained config page. Leaflet + map tiles load from the public CDN,
// which works because in station mode the phone/PC has internet (the captive
// portal does not — hence this lives in normal operation, not the AP portal).
const char kPage[] PROGMEM = R"HTML(<!doctype html><html lang="de"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Plane Radar</title>
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css">
<style>
:root{color-scheme:dark}
body{margin:0;font:15px/1.4 system-ui,sans-serif;background:#0a0f1c;color:#dde}
header{padding:14px 16px;font-weight:700;font-size:18px;color:#8f8}
#map{height:46vh;width:100%}
main{padding:16px;max-width:560px;margin:0 auto}
.row{margin:14px 0}
label{display:block;margin-bottom:6px;color:#9ab}
input[type=range]{width:100%}
select,button{font:inherit;padding:8px;border-radius:8px;border:1px solid #345;background:#142033;color:#dde}
.inline{display:flex;align-items:center;gap:10px}
.inline label{margin:0}
button{background:#1c6;color:#021;font-weight:700;border:0;width:100%;padding:13px;margin-top:8px}
#coords{color:#cde;font-variant-numeric:tabular-nums}
#msg{min-height:20px;margin-top:8px;color:#8f8;text-align:center}
input[type=color]{width:48px;height:32px;border:0;background:none;vertical-align:middle}
</style></head><body>
<header>Plane Radar — Konfiguration</header>
<div id="map"></div>
<main>
<div class="row"><label>Zentralstandort (auf Karte tippen oder Marker ziehen)</label><span id="coords">—</span></div>
<div class="row"><label>Sweep-Geschwindigkeit: <span id="spdv"></span></label><input type="range" id="spd" min="800" max="12000" step="100"></div>
<div class="row"><label>Display-Helligkeit: <span id="brtv"></span></label><input type="range" id="brt" min="10" max="255" step="5"></div>
<div class="row inline"><label>Sweep-Farbe</label><input type="color" id="swc"><label style="margin-left:18px">Flugzeug-Farbe</label><input type="color" id="acc"></div>
<div class="row"><label>Reichweite</label><select id="rng"><option value="0">5 km</option><option value="1">10 km</option><option value="2">15 km</option><option value="3">25 km</option><option value="4">50 km</option><option value="5">100 km</option><option value="6">200 km</option></select></div>
<div class="row inline"><input type="checkbox" id="mi"><label>Entfernungen in Meilen</label></div>
<div class="row inline"><input type="checkbox" id="rw"><label>Flughafen-Runways anzeigen</label></div>
<div class="row inline"><input type="checkbox" id="ct"><label>Karten-Konturen (Küste/Gewässer)</label></div>
<div class="row inline"><input type="checkbox" id="lb"><label>Beschreibungen (Labels) anzeigen</label></div>
<div class="row inline"><input type="checkbox" id="ar"><label>Auto-Drehung (Lagesensor)</label></div>
<div class="row inline"><input type="checkbox" id="bp"><label>Beep bei Flugzeug-Kontakt</label></div>
<div class="row"><label>Beep-Lautstärke: <span id="bvv"></span></label><input type="range" id="bv" min="0" max="100" step="5"></div>
<button id="save">Speichern</button>
<div id="msg"></div>
<h3 style="margin-top:26px;color:#9cf">WLAN-Netzwerke</h3>
<p style="color:#789;font-size:13px;margin:0 0 8px">Mehrere möglich — das Gerät verbindet sich mit dem, das in Reichweite ist.</p>
<ul id="wlist" style="list-style:none;padding:0;margin:0"></ul>
<div class="row inline" style="flex-wrap:wrap">
<input id="wssid" placeholder="SSID" style="flex:1;min-width:110px;padding:8px;border-radius:8px;border:1px solid #345;background:#142033;color:#dde">
<input id="wpass" type="password" placeholder="Passwort" style="flex:1;min-width:110px;padding:8px;border-radius:8px;border:1px solid #345;background:#142033;color:#dde">
<button id="wadd" style="flex:1;min-width:120px">Hinzufügen</button>
</div>
<div id="wmsg" style="color:#9ab;min-height:18px"></div>
<p style="color:#789;font-size:13px;margin-top:20px">Flugplätze werden automatisch aus OpenStreetMap rund um den Standort geladen.</p>
<button id="reset" style="background:#933;color:#fee;margin-top:8px">WLAN zurücksetzen (Setup-Portal)</button>
</main>
<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
<script>
let lat=0,lon=0,map,marker;
const $=id=>document.getElementById(id);
function setLL(a,o){lat=a;lon=o;$('coords').textContent=a.toFixed(5)+', '+o.toFixed(5);}
function upd(){$('spdv').textContent=($('spd').value/1000).toFixed(1)+' s / Umlauf';$('brtv').textContent=Math.round($('brt').value/255*100)+'%';$('bvv').textContent=$('bv').value+'%';}
function init(s){
 setLL(s.lat,s.lon);
 map=L.map('map').setView([lat,lon],11);
 L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:19,attribution:'© OpenStreetMap'}).addTo(map);
 marker=L.marker([lat,lon],{draggable:true}).addTo(map);
 marker.on('dragend',e=>{const p=e.target.getLatLng();setLL(p.lat,p.lng);});
 map.on('click',e=>{marker.setLatLng(e.latlng);setLL(e.latlng.lat,e.latlng.lng);});
 $('spd').value=s.sweepMs;$('brt').value=s.brightness;$('swc').value=s.sweepColor;
 $('acc').value=s.acColor;$('rng').value=s.range;$('mi').checked=s.miles;$('rw').checked=s.runways;
 $('ct').checked=s.contours;$('lb').checked=s.labels;$('bp').checked=s.beep;
 $('bv').value=s.beepVol;$('bv').oninput=upd;$('ar').checked=s.autoRotate;
 upd();loadWifi();
}
function loadWifi(){
 fetch('/wifi').then(r=>r.json()).then(a=>{
  $('wlist').innerHTML=a.length?'':'<li style="color:#789">— keine —</li>';
  a.forEach((ssid,i)=>{
   const li=document.createElement('li');li.style.cssText='display:flex;justify-content:space-between;align-items:center;padding:6px 0;border-bottom:1px solid #234';
   const sp=document.createElement('span');sp.textContent=ssid;li.appendChild(sp);
   const b=document.createElement('button');b.textContent='✕';b.style.cssText='width:auto;background:#933;color:#fee;padding:4px 12px';
   b.onclick=()=>{fetch('/wifi/del',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'i='+i}).then(loadWifi);};
   li.appendChild(b);$('wlist').appendChild(li);
  });
 });
}
$('spd').oninput=upd;$('brt').oninput=upd;
$('save').onclick=()=>{
 const b=new URLSearchParams({lat:lat.toFixed(6),lon:lon.toFixed(6),sweepMs:$('spd').value,
  brightness:$('brt').value,sweepColor:$('swc').value,acColor:$('acc').value,
  range:$('rng').value,miles:$('mi').checked?1:0,runways:$('rw').checked?1:0,
  contours:$('ct').checked?1:0,labels:$('lb').checked?1:0,beep:$('bp').checked?1:0,
  beepVol:$('bv').value,autoRotate:$('ar').checked?1:0});
 $('msg').textContent='…';
 fetch('/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b})
  .then(r=>{$('msg').textContent=r.ok?'Gespeichert ✓':'Fehler';})
  .catch(()=>{$('msg').textContent='Fehler';});
};
$('wadd').onclick=()=>{
 const ssid=$('wssid').value.trim();
 if(!ssid){$('wmsg').textContent='SSID nötig.';return;}
 const b=new URLSearchParams({ssid:ssid,pass:$('wpass').value});
 fetch('/wifi/add',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b})
  .then(r=>{if(r.ok){$('wssid').value='';$('wpass').value='';$('wmsg').textContent='Gespeichert ✓';loadWifi();}else $('wmsg').textContent='Fehler (Liste voll?).';});
};
$('reset').onclick=()=>{
 if(!confirm('Alle WLAN-Netze, Standort und Einheiten löschen und neu starten? Das Gerät öffnet danach das Setup-AP "PlaneRadar-Setup" (Standard-WLAN bleibt hinterlegt).'))return;
 $('msg').textContent='Setze zurück, Gerät startet neu…';
 fetch('/reset',{method:'POST'}).catch(()=>{});
};
fetch('/state').then(r=>r.json()).then(init).catch(()=>{$('msg').textContent='Status nicht ladbar';});
</script></body></html>)HTML";

uint32_t parseHexColor(const String& s) {
  const char* p = s.c_str();
  if (*p == '#') ++p;
  return static_cast<uint32_t>(strtoul(p, nullptr, 16)) & 0xFFFFFF;
}

void handleRoot() { s_server.send_P(200, "text/html", kPage); }

void handleState() {
  char buf[320];
  snprintf(buf, sizeof(buf),
           "{\"lat\":%.6f,\"lon\":%.6f,\"sweepMs\":%u,\"brightness\":%u,"
           "\"sweepColor\":\"#%06X\",\"acColor\":\"#%06X\",\"range\":%u,"
           "\"miles\":%s,\"runways\":%s,\"contours\":%s,\"labels\":%s,"
           "\"beep\":%s,\"beepVol\":%u,\"autoRotate\":%s}",
           services::location::lat(), services::location::lon(),
           static_cast<unsigned>(ui::radar::sweepPeriodMs()),
           static_cast<unsigned>(ui::radar::brightness()),
           static_cast<unsigned>(ui::radar::sweepColorRgb()),
           static_cast<unsigned>(ui::radar::aircraftColorRgb()),
           static_cast<unsigned>(ui::radar::rangeIndex()),
           ui::radar::useMiles() ? "true" : "false",
           ui::radar::showRunways() ? "true" : "false",
           ui::radar::showContours() ? "true" : "false",
           ui::radar::showLabels() ? "true" : "false",
           ui::radar::beepEnabled() ? "true" : "false",
           static_cast<unsigned>(ui::radar::beepVolume()),
           ui::radar::autoRotate() ? "true" : "false");
  s_server.send(200, "application/json", buf);
}

void handleSave() {
  if (s_server.hasArg("lat") && s_server.hasArg("lon")) {
    services::location::saveFromStrings(s_server.arg("lat").c_str(),
                                        s_server.arg("lon").c_str());
  }
  if (s_server.hasArg("sweepMs")) {
    ui::radar::setSweepPeriodMs(
        static_cast<uint32_t>(s_server.arg("sweepMs").toInt()));
  }
  if (s_server.hasArg("sweepColor")) {
    ui::radar::setSweepColorRgb(parseHexColor(s_server.arg("sweepColor")));
  }
  if (s_server.hasArg("acColor")) {
    ui::radar::setAircraftColorRgb(parseHexColor(s_server.arg("acColor")));
  }
  if (s_server.hasArg("range")) {
    ui::radar::setRangeIndex(
        static_cast<uint8_t>(s_server.arg("range").toInt()));
  }
  if (s_server.hasArg("miles")) {
    ui::radar::setMiles(s_server.arg("miles") == "1");
  }
  if (s_server.hasArg("runways")) {
    ui::radar::setRunways(s_server.arg("runways") == "1");
  }
  if (s_server.hasArg("contours")) {
    ui::radar::setContours(s_server.arg("contours") == "1");
  }
  if (s_server.hasArg("labels")) {
    ui::radar::setLabels(s_server.arg("labels") == "1");
  }
  if (s_server.hasArg("beep")) {
    ui::radar::setBeep(s_server.arg("beep") == "1");
  }
  if (s_server.hasArg("beepVol")) {
    const uint8_t v = static_cast<uint8_t>(s_server.arg("beepVol").toInt());
    ui::radar::setBeepVolume(v);
    services::audio::setVolume(v);
  }
  if (s_server.hasArg("autoRotate")) {
    ui::radar::setAutoRotate(s_server.arg("autoRotate") == "1");
  }
  if (s_server.hasArg("brightness")) {
    const uint8_t b = static_cast<uint8_t>(s_server.arg("brightness").toInt());
    ui::radar::setBrightness(b);
    tft.setBrightness(b);
  }

  // Re-render so range/colour/units changes show immediately (the sweep speed
  // and colour are read live each frame, but palette + grid need a rebuild).
  ui::radarDisplayDraw();

  s_server.send(200, "text/plain", "ok");
}

void handleReset() {
  s_server.send(200, "text/plain", "resetting");
  delay(200);  // let the response flush before we wipe + reboot
  wifiResetCredentialsAndReboot();
}

void handleWifiList() {
  String j = "[";
  const services::wifi_store::Net* nets = services::wifi_store::list();
  const size_t n = services::wifi_store::count();
  for (size_t i = 0; i < n; ++i) {
    if (i) j += ",";
    j += "\"";
    // Escape quotes/backslashes in the SSID for valid JSON.
    for (const char* p = nets[i].ssid; *p; ++p) {
      if (*p == '"' || *p == '\\') j += '\\';
      j += *p;
    }
    j += "\"";
  }
  j += "]";
  s_server.send(200, "application/json", j);
}

void handleWifiAdd() {
  const bool ok = services::wifi_store::add(s_server.arg("ssid").c_str(),
                                            s_server.arg("pass").c_str());
  s_server.send(ok ? 200 : 400, "text/plain", ok ? "ok" : "err");
}

void handleWifiDel() {
  const bool ok =
      services::wifi_store::remove(static_cast<size_t>(s_server.arg("i").toInt()));
  s_server.send(ok ? 200 : 400, "text/plain", ok ? "ok" : "err");
}

}  // namespace

void begin() {
  if (s_started) {
    return;
  }
  s_started = true;

  s_server.on("/", handleRoot);
  s_server.on("/state", handleState);
  s_server.on("/save", HTTP_POST, handleSave);
  s_server.on("/reset", HTTP_POST, handleReset);
  s_server.on("/wifi", HTTP_GET, handleWifiList);
  s_server.on("/wifi/add", HTTP_POST, handleWifiAdd);
  s_server.on("/wifi/del", HTTP_POST, handleWifiDel);
  s_server.begin();

#ifdef WM_MDNS
  if (MDNS.begin(config::kPortalHostname)) {
    MDNS.addService("http", "tcp", 80);
  }
#endif
  Serial.printf("Config page: http://%s.local (or http://%s)\n",
                config::kPortalHostname, WiFi.localIP().toString().c_str());
}

void loop() {
  if (s_started) {
    s_server.handleClient();
  }
}

}  // namespace services::config_server
