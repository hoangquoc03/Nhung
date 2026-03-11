#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>

#define MAX_LOG 20
// ===== RC522 =====
#define SS_PIN  D8
#define RST_PIN D3
// ===== USER LOGIN =====
#define MAX_USERS 5
struct User {
  String user;
  String pass;
};
String lastScannedUID = "";
User users[MAX_USERS];
int userCount = 0;
bool loggedIn = false;

MFRC522 rfid(SS_PIN, RST_PIN);

// ===== WIFI =====
const char* ssid = "Van Phu Quan";
const char* pass = "12345678";

// ===== SERVER =====
ESP8266WebServer server(80);
struct DoorLog {
  String time;
  String source;
  String uid;
  String action;
};
DoorLog logs[MAX_LOG];
int logIndex = 0;
// ===== EEPROM =====
#define MAX_CARDS 10
#define UID_SIZE 4
#define EEPROM_SIZE (MAX_CARDS * UID_SIZE + 1)
#define EEPROM_FLAG_ADDR (EEPROM_SIZE - 1)


// ===== ADMIN UID =====
byte adminUID[4] = {0xE0, 0xEB, 0x64, 0x5F};
bool systemLocked = false;
// ===== STATE =====
bool doorOpen = false;
char cardMode = 0; // 0 = normal, T = add, X = delete

// ================== UID FUNCTIONS ==================
bool uidEqual(byte *a, byte *b) {
  for (int i = 0; i < 4; i++)
    if (a[i] != b[i]) return false;
  return true;
}

bool isAdmin(byte *uid) {
  return uidEqual(uid, adminUID);
}

bool findUID(byte *uid) {
  for (int i = 0; i < MAX_CARDS; i++) {
    bool match = true;
    for (int j = 0; j < 4; j++) {
      if (EEPROM.read(i * 4 + j) != uid[j]) {
        match = false;
        break;
      }
    }
    if (match) return true;
  }
  return false;
}
String getTime() {
  unsigned long s = millis() / 1000;
  int h = (s / 3600) % 24;
  int m = (s / 60) % 60;
  int sec = s % 60;

  char buf[20];
  sprintf(buf, "%02d:%02d:%02d", h, m, sec);
  return String(buf);
}
void addLog(String source, String uid, String action) {
  logs[logIndex] = { getTime(), source, uid, action };
  logIndex = (logIndex + 1) % MAX_LOG;
}

bool addUID(byte *uid) {
  if (findUID(uid)) return false;

  for (int i = 0; i < MAX_CARDS; i++) {
    if (EEPROM.read(i * 4) == 0xFF) {
      for (int j = 0; j < 4; j++)
        EEPROM.write(i * 4 + j, uid[j]);
      EEPROM.commit();
      return true;
    }
  }
  return false;
}

bool deleteUID(byte *uid) {
  for (int i = 0; i < MAX_CARDS; i++) {
    bool match = true;
    for (int j = 0; j < 4; j++) {
      if (EEPROM.read(i * 4 + j) != uid[j]) {
        match = false;
        break;
      }
    }
    if (match) {
      for (int j = 0; j < 4; j++)
        EEPROM.write(i * 4 + j, 0xFF);
      EEPROM.commit();
      return true;
    }
  }
  return false;
}
String loginPage(){
return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SMART DOOR LOGIN</title>

<style>

*{
box-sizing:border-box;
font-family:Arial,Helvetica,sans-serif;
}

body{
margin:0;
height:100vh;
display:flex;
align-items:center;
justify-content:center;
background:linear-gradient(135deg,#2980b9,#6dd5fa);
}

.card{

width:340px;
background:white;
padding:35px;
border-radius:18px;
box-shadow:0 10px 25px rgba(0,0,0,0.2);
text-align:center;

}

h2{
margin-bottom:25px;
color:#2c3e50;
}

.inputBox{

position:relative;
margin-bottom:15px;

}

input{

width:100%;
padding:12px;
padding-left:38px;
border-radius:8px;
border:1px solid #ccc;
font-size:15px;

}

input:focus{

border-color:#2980b9;
outline:none;

}

.icon{

position:absolute;
left:10px;
top:50%;
transform:translateY(-50%);
font-size:16px;
color:#777;

}

button{

width:100%;
padding:13px;
margin-top:10px;
background:#2980b9;
color:white;
border:none;
border-radius:10px;
font-size:17px;
cursor:pointer;
transition:.2s;

}

button:hover{

background:#1f6391;

}

.register{

margin-top:15px;
font-size:14px;

}

.register a{

color:#2980b9;
text-decoration:none;
font-weight:bold;

}

.register a:hover{

text-decoration:underline;

}

.error{

color:#e74c3c;
margin-top:10px;
font-size:14px;
display:none;

}

.logo{

font-size:32px;
margin-bottom:10px;

}

</style>
</head>

<body>

<div class="card">

<div class="logo">🔐</div>
<h2>SMART DOOR</h2>

<div class="inputBox">
<span class="icon">👤</span>
<input id="user" placeholder="Username">
</div>

<div class="inputBox">
<span class="icon">🔑</span>
<input id="pass" type="password" placeholder="Password">
</div>

<button onclick="login()">LOGIN</button>

<div class="error" id="err">
Sai tài khoản hoặc mật khẩu
</div>

<div class="register">
Chưa có tài khoản?
<a href="/registerPage">Đăng ký</a>
</div>

</div>

<script>

function login(){

let u=document.getElementById("user").value
let p=document.getElementById("pass").value

document.getElementById("err").style.display="none"

fetch("/login?u="+u+"&p="+p)

.then(r=>r.text())

.then(d=>{

if(d=="OK"){

location.href="/dashboard"

}

else{

document.getElementById("err").style.display="block"

}

})

}

</script>

</body>
</html>
)rawliteral";
}

String registerPage(){
return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SMART DOOR REGISTER</title>

<style>

*{
box-sizing:border-box;
font-family:Arial,Helvetica,sans-serif;
}

body{
margin:0;
height:100vh;
display:flex;
align-items:center;
justify-content:center;
background:linear-gradient(135deg,#2980b9,#6dd5fa);
}

.card{
width:340px;
background:white;
padding:35px;
border-radius:18px;
box-shadow:0 10px 25px rgba(0,0,0,0.2);
text-align:center;
}

h2{
margin-bottom:25px;
color:#2c3e50;
}

.inputBox{
position:relative;
margin-bottom:15px;
}

input{
width:100%;
padding:12px;
padding-left:38px;
border-radius:8px;
border:1px solid #ccc;
font-size:15px;
}

input:focus{
border-color:#2980b9;
outline:none;
}

.icon{
position:absolute;
left:10px;
top:50%;
transform:translateY(-50%);
font-size:16px;
color:#777;
}

button{
width:100%;
padding:13px;
margin-top:10px;
background:#27ae60;
color:white;
border:none;
border-radius:10px;
font-size:17px;
cursor:pointer;
transition:.2s;
}

button:hover{
background:#1e874b;
}

.login{
margin-top:15px;
font-size:14px;
}

.login a{
color:#2980b9;
text-decoration:none;
font-weight:bold;
}

.login a:hover{
text-decoration:underline;
}

.msg{
margin-top:10px;
font-size:14px;
display:none;
}

.logo{
font-size:32px;
margin-bottom:10px;
}

</style>
</head>

<body>

<div class="card">

<div class="logo">📝</div>
<h2>REGISTER</h2>

<div class="inputBox">
<span class="icon">👤</span>
<input id="user" placeholder="Username">
</div>

<div class="inputBox">
<span class="icon">🔑</span>
<input id="pass" type="password" placeholder="Password">
</div>

<button onclick="reg()">REGISTER</button>

<div class="msg" id="msg"></div>

<div class="login">
Đã có tài khoản?
<a href="/">Đăng nhập</a>
</div>

</div>

<script>

function reg(){

let u=document.getElementById("user").value
let p=document.getElementById("pass").value

fetch("/register?u="+u+"&p="+p)

.then(r=>r.text())

.then(d=>{

let m=document.getElementById("msg")
m.style.display="block"

if(d=="OK"){
m.style.color="green"
m.innerHTML="Đăng ký thành công"
setTimeout(()=>location.href="/",1200)
}
else{
m.style.color="red"
m.innerHTML="Tài khoản đã tồn tại"
}

})

}

</script>

</body>
</html>
)rawliteral";
}

String getCardListJSON(){

String json = "[";

for(int i=0;i<MAX_CARDS;i++){

byte uid[4];
bool empty=true;

for(int j=0;j<4;j++){
uid[j] = EEPROM.read(i*4+j);
if(uid[j] != 0xFF) empty=false;
}

if(!empty){

String uidStr="";

for(int j=0;j<4;j++){
if(uid[j]<0x10) uidStr+="0";
uidStr += String(uid[j],HEX);
}

json += "{\"uid\":\""+uidStr+"\"},";
}

}

if(json.endsWith(",")) json.remove(json.length()-1);

json+="]";

return json;

}
// ================== WEB UI ==================
String webpage() {
return R"rawliteral(
<!DOCTYPE html>
<html>
<head>

<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">

<title>SMART DOOR</title>

<style>

*{
box-sizing:border-box;
font-family:Arial,Helvetica,sans-serif;
}

body{
margin:0;
background:linear-gradient(135deg,#2980b9,#6dd5fa);
min-height:100vh;
}

/* HEADER */

.header{
background:#1f6391;
color:white;
padding:18px;
text-align:center;
font-size:22px;
font-weight:bold;
letter-spacing:1px;
box-shadow:0 4px 10px rgba(0,0,0,0.2);
}

/* MAIN CARD */

.card{
background:white;
margin:25px auto;
padding:25px;
border-radius:18px;
max-width:520px;
box-shadow:0 10px 25px rgba(0,0,0,0.2);
}

/* STATE */

.state{
font-size:32px;
margin:15px 0;
font-weight:bold;
text-align:center;
}

.open{
color:#27ae60;
}

.close{
color:#e74c3c;
}

/* BUTTONS */

button{
width:100%;
padding:14px;
font-size:17px;
border:none;
border-radius:10px;
margin-top:10px;
cursor:pointer;
transition:0.2s;
}

button:hover{
transform:scale(1.02);
}

.mainBtn{
background:#2980b9;
color:white;
}

.logBtn{
background:#8e44ad;
color:white;
}

.logout{
background:#e74c3c;
color:white;
}

/* SECTION */

.section{
margin-top:25px;
}

.section h3{
margin-bottom:10px;
color:#333;
}

/* INPUT */

input{
padding:10px;
border-radius:8px;
border:1px solid #ccc;
}

/* TABLE */

table{
width:100%;
border-collapse:collapse;
font-size:14px;
}

th{
background:#ecf0f1;
padding:8px;
}

td{
padding:8px;
border-bottom:1px solid #eee;
text-align:center;
}

tr:hover{
background:#f8f8f8;
}

/* DELETE BUTTON */

.deleteBtn{
background:#e74c3c;
color:white;
padding:4px 8px;
border-radius:6px;
cursor:pointer;
}

.openLog{
color:#27ae60;
font-weight:bold;
}

.denyLog{
color:#e74c3c;
font-weight:bold;
}

/* SCROLL */

.log-box{
max-height:220px;
overflow-y:auto;
border:1px solid #eee;
border-radius:8px;
}

</style>

</head>

<body>

<div class="header">
🔐 SMART DOOR CONTROL PANEL
</div>

<div class="card">

<div id="state" class="state">Đang tải...</div>

<button id="btnToggle" class="mainBtn" onclick="toggleDoor()">🚪 MỞ / ĐÓNG CỬA</button>

<button class="logBtn" onclick="loadLog()">📜 XEM LỊCH SỬ</button>

<button class="logout" onclick="logout()">🚪 ĐĂNG XUẤT</button>


<div class="section">

<h3>📇 RFID CARDS</h3>

<div style="display:flex;gap:6px;margin-bottom:8px">

<input id="newUID" placeholder="Nhập UID (vd: e0eb645f)" style="flex:1">

<button onclick="addCard()" style="width:110px;background:#27ae60;color:white">ADD</button>

</div>

<div class="log-box">

<table>

<thead>
<tr>
<th>#</th>
<th>UID</th>
<th>Xóa</th>
</tr>
</thead>

<tbody id="cards"></tbody>

</table>

</div>

</div>


<div class="section">

<h3>📜 ACCESS LOG</h3>

<div class="log-box">

<table>

<thead>
<tr>
<th>Time</th>
<th>Source</th>
<th>UID</th>
<th>Action</th>
</tr>
</thead>

<tbody id="log"></tbody>

</table>

</div>

</div>

</div>


<script>

function toggleDoor(){

let btn=document.getElementById("btnToggle")

btn.disabled=true
btn.innerText="ĐANG XỬ LÝ..."

fetch('/toggle').finally(()=>{

setTimeout(()=>{
btn.disabled=false
btn.innerText="🚪 MỞ / ĐÓNG CỬA"
},800)

})

}

function updateState(){

fetch('/state')

.then(res=>res.json())

.then(data=>{

let el=document.getElementById('state')

if(data.door==="open"){

el.innerHTML="🚪 CỬA ĐANG MỞ"
el.className="state open"

}

else{

el.innerHTML="🔒 CỬA ĐANG ĐÓNG"
el.className="state close"

}

})

}

function loadCards(){

fetch('/cards')

.then(r=>r.json())

.then(d=>{

let tb=document.getElementById("cards")

tb.innerHTML=""

let i=1

d.forEach(c=>{

tb.innerHTML+=`
<tr>
<td>${i++}</td>
<td>${c.uid}</td>
<td>
<button class="deleteBtn" onclick="deleteCard('${c.uid}')">❌</button>
</td>
</tr>
`

})

})

}

function deleteCard(uid){

if(!confirm("Xóa thẻ "+uid+" ?")) return

fetch('/deleteCard?uid='+uid)

.then(r=>r.text())

.then(d=>{

if(d=="OK"){
loadCards()
}else{
alert("Xóa thất bại")
}

})

}

function addCard(){

let uid=document.getElementById("newUID").value

fetch('/addCard?uid='+uid)

.then(r=>r.text())

.then(d=>{

if(d=="OK"){
alert("Thêm thẻ thành công")
loadCards()
}else{
alert("Không thêm được")
}

})

}

function loadLog(){

fetch('/log')

.then(r=>r.json())

.then(d=>{

let tb=document.getElementById("log")

tb.innerHTML=""

d.forEach(l=>{

let color=""

if(l.action=="OPEN") color="openLog"
if(l.action=="DENY") color="denyLog"

tb.innerHTML+=`
<tr>
<td>${l.time}</td>
<td>${l.source}</td>
<td>${l.uid}</td>
<td class="${color}">${l.action}</td>
</tr>
`

})

})

}

function logout(){

fetch('/logout')

.then(()=>{location.href="/"})

}

setInterval(updateState,1000)
setInterval(loadCards,3000)

updateState()
loadCards()

</script>

</body>
</html>
)rawliteral";
}

// ================== SETUP ==================
void setup() {
  Serial.begin(9600);
  EEPROM.begin(EEPROM_SIZE);
  if (EEPROM.read(EEPROM_FLAG_ADDR) != 0xAA) {
    for (int i = 0; i < EEPROM_SIZE - 1; i++) {
      EEPROM.write(i, 0xFF);
    }
    EEPROM.write(EEPROM_FLAG_ADDR, 0xAA);
    EEPROM.commit();
  }
  SPI.begin();
  rfid.PCD_Init();

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) delay(300);

  server.on("/", [](){
    if(!loggedIn)
      server.send(200,"text/html",loginPage());
    else
      server.send(200,"text/html",webpage());
  });

  server.on("/state", []() {
    String json = "{";
    json += "\"door\":";
    json += doorOpen ? "\"open\"" : "\"close\"";
    json += "}";
    server.send(200, "application/json", json);
  });
  server.on("/log", []() {
    String json = "[";
    for (int i = 0; i < MAX_LOG; i++) {
      int idx = (logIndex - 1 - i + MAX_LOG) % MAX_LOG;
      if (logs[idx].time == "") continue;

      json += "{";
      json += "\"time\":\"" + logs[idx].time + "\",";
      json += "\"source\":\"" + logs[idx].source + "\",";
      json += "\"uid\":\"" + logs[idx].uid + "\",";
      json += "\"action\":\"" + logs[idx].action + "\"";
      json += "},";
    }
    if (json.endsWith(",")) json.remove(json.length() - 1);
    json += "]";
    server.send(200, "application/json", json);
  });

  server.on("/toggle", []() {
    if (systemLocked) {
      addLog("WEB", "-", "BLOCKED");
      server.send(403, "text/plain", "LOCKED");
      return;
    }
    Serial.write('T');
    addLog("WEB", "-", "TOGGLE");
    server.send(200, "text/plain", "OK");
  });

  server.on("/login", [](){
  String u = server.arg("u");
  String p = server.arg("p");

  for(int i=0;i<userCount;i++){

  if(users[i].user==u && users[i].pass==p){
  loggedIn=true;
  server.send(200,"text/plain","OK");
  return;
  }

  }

  server.send(200,"text/plain","FAIL");

  });

  server.on("/register", [](){
  if(userCount>=MAX_USERS){
  server.send(200,"text/plain","USER FULL");
  return;
  }

  User newUser;

  newUser.user = server.arg("u");
  newUser.pass = server.arg("p");

  users[userCount] = newUser;

  userCount++;

  server.send(200,"text/plain","REGISTER OK");

  });
  server.on("/dashboard", [](){

  if(!loggedIn){
  server.send(200,"text/html",loginPage());
  return;
  }

  server.send(200,"text/html",webpage());

  });
  server.on("/registerPage", [](){
    server.send(200,"text/html",registerPage());
  });
  server.on("/logout", [](){
    loggedIn = false; 
    server.send(200, "text/plain", "OK");
  });

  server.on("/cards", [](){
  server.send(200,"application/json",getCardListJSON());

  });
  server.on("/addCard", [](){

  String uidStr = server.arg("uid");

  byte uid[4];

  for(int i=0;i<4;i++){
  uid[i] = strtoul(uidStr.substring(i*2,i*2+2).c_str(),NULL,16);
  }


  bool ok = addUID(uid);

  server.send(200,"text/plain", ok ? "OK" : "FAIL");

  });
  server.on("/scanUID", [](){
  server.send(200,"text/plain", lastScannedUID);
  });
  
  server.on("/deleteCard", [](){

  String uidStr = server.arg("uid");

  byte uid[4];

  for(int i=0;i<4;i++){
  uid[i] = strtoul(uidStr.substring(i*2,i*2+2).c_str(),NULL,16);
  }

  bool ok = deleteUID(uid);

  server.send(200,"text/plain", ok ? "OK" : "FAIL");

  });


    server.begin();
  }

// ================== LOOP ==================
void loop() {
  server.handleClient();

  if (Serial.available()) {

    char c = Serial.read();

    if (c == '1') doorOpen = true;
    else if (c == '0') doorOpen = false;
    else if (c == 'L') doorOpen = false;
    if (c == 'K') systemLocked = true;
    if (c == 'U') systemLocked = false;

    // ===== CHẾ ĐỘ RFID =====
    else if (c == 'T' || c == 'X') cardMode = c;

  }

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  byte *uid = rfid.uid.uidByte;
  static unsigned long lastRead = 0;
  if (millis() - lastRead < 1500) {
    rfid.PICC_HaltA();
    return;
  }
  lastRead = millis();
  if (cardMode == 'T') {
    Serial.write(addUID(uid) ? 'S' : 'F');
    cardMode = 0;
  }
  else if (cardMode == 'X') {
    Serial.write(deleteUID(uid) ? 'S' : 'F');
    cardMode = 0;
  }
  else {
    if (isAdmin(uid)) {
      Serial.write('A');

      String uidStr = "";
      for (byte i = 0; i < 4; i++) {
        if (uid[i] < 0x10) uidStr += "0";
        uidStr += String(uid[i], HEX);
      }
      lastScannedUID = uidStr;
      addLog("ADMIN", uidStr, "ADMIN");
      
    }
    else if (findUID(uid)) {
      Serial.write('O');

      String uidStr = "";
      for (byte i = 0; i < 4; i++) {
        if (uid[i] < 0x10) uidStr += "0";
        uidStr += String(uid[i], HEX);
      }
      addLog("RFID", uidStr, "OPEN");
    }
    else {
      Serial.write('F');

      String uidStr = "";
      for (byte i = 0; i < 4; i++) {
        if (uid[i] < 0x10) uidStr += "0";
        uidStr += String(uid[i], HEX);
      }
      
      addLog("RFID", uidStr, "DENY");
    }
  }
  rfid.PICC_HaltA();
}

