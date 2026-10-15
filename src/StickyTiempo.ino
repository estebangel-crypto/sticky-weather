//Sticky Tiempo - REDISEÑO MULTILINGÜE INTEGRAL - CORREGIDO BATERIA
 // - 6 Idiomas: Español, Inglés, Portugués, Francés, Italiano, Alemán
 // - Auto-escala tipográfica inteligente para evitar colisiones en cabecera
 // - Sin reloj congelado, optimizado para Deep Sleep profundo
 // - Estructura JSON con caracteres de escape limpios en WebServer
 // - CORRECCION BATERIA: Wire1 en GPIO 1/0, registros 0x2C/0x08/0x0C, Serial0

#include <Arduino.h>
#include <SPI.h>
#include <Seeed_GFX.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <time.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "driver/rtc_io.h"

const uint64_t TIEMPO_DESPTERTA_MIN = 3;
const uint64_t INTERVALO_DATOS_MIN = 60;
const uint64_t INTERVALO_MIN = 60;
#define SIMULAR_BATERIA -1           // PRUEBAS: porcentaje falso (ej. 8). Pon -1 para usar la bateria real
#define UMBRAL_BATERIA_BAJA 10      // al llegar a este % (o menos) se muestra el aviso
#define UMBRAL_BATERIA_RECUPERA 15  // tras el aviso, hasta superar este % (o enchufar) no se vuelve a la normalidad
const uint64_t INTERVALO_BATERIA_BAJA_MIN = 60; // en bateria baja solo despierta cada 60 min a mirar si se enchufo
const uint64_t TIEMPO_PULSACION_LARGA_MS = 1500;
const char* AP_NOMBRE = "Sticky-Tiempo";
const unsigned long TIEMPO_PORTAL_MS = 15UL * 60UL * 1000UL;
#define PRELIMPIAR true
#define PIN_BTN_AI 4
#define PIN_BTN_UP 5
#define PIN_BTN_DOWN 6
#define USAR_SHT40 true
#define Serial Serial0
#define DEBUG_SERIAL Serial0

#define PIN_PWR_HOLD 45
#define PIN_PWR_LOCK 46
#define PIN_EPD_PWR 47
#define PIN_TOUCH_PWR 42
void stickyPowerLatch(){
  pinMode(PIN_PWR_HOLD, OUTPUT);
  pinMode(PIN_PWR_LOCK, OUTPUT);
  digitalWrite(PIN_PWR_HOLD, HIGH); delay(5);
  digitalWrite(PIN_PWR_LOCK, HIGH); delay(5);
  digitalWrite(PIN_PWR_LOCK, LOW); delay(5);
  digitalWrite(PIN_PWR_LOCK, HIGH); delay(5);
  pinMode(PIN_EPD_PWR, OUTPUT); digitalWrite(PIN_EPD_PWR, HIGH);
  pinMode(PIN_TOUCH_PWR, OUTPUT); digitalWrite(PIN_TOUCH_PWR, HIGH);
}

#define PIN_SDA 1
#define PIN_SCL 0

Seeed_GFX display(Seeed_Product::reTerminal_Sticky);
#define NEGRO TFT_BLACK
#define BLANCO TFT_WHITE

enum WeatherIcon : uint8_t { WICON_SOL=0, WICON_POCO_NUBLADO=1, WICON_NUBLADO=2, WICON_LLUVIA=3, WICON_NIEVE=4, WICON_TORMENTA=5, WICON_NIEBLA=6 };

//lang: 0=ES, 1=EN, 2=PT, 3=FR, 4=IT, 5=DE
struct Config { String ssid; String pass; String ciudad; float lat=NAN; float lon=NAN; int lang=0; int uni=0; int fmt=0; bool valida=false; } cfg;
Preferences prefs;

TwoWire &sensorWire = Wire1;

void cargarConfig();
void guardarWifi(const String& s,const String& p);
void guardarCiudad(const String& n,float la,float lo, int ln, int uni, int fmt);
bool leerBateria();
void leerSHT40();
int anchoTxt(const char* s,int size);
void txt(const char* s,int x,int y,int size,bool negrita);
void txtCentrado(const char* s,int cx,int y,int size,bool negrita);
void txtDerecha(const char* s,int xd,int y,int size,bool negrita);
void txtSimple(const char* s,int x,int y,int size,bool negrita);
WeatherIcon iconoDe(int c);
const char * textoDe(int c);
void lineaGruesa(int x1,int y1,int x2,int y2,int g);
void formaNube(int cx,int cy,int s,int inset,uint32_t col);
void nube(int cx,int cy,int s);
void sol(int cx,int cy,int r);
void dibujarIcono(WeatherIcon t,int cx,int cy,int s);
void engranaje(int cx,int cy,int r);
void dibujarBateria(int x,int y,int pct);
bool conectarWifi(const String& ssid,const String& pass);
static String urlEnc(const String& s);
bool obtenerTiempo();
void dibujar();
void pantallaMensaje(const char* l1,const char* l2);
void pantallaConfig();
void hRaiz();
void hRedes();
void hWifi();
void hEstado();
void hCiudades();
void hGuardar();
void hOtros();
void procesarWifi();
void modoConfiguracion();
void dormir(uint64_t minutos);
void setup();
void loop();
void cargarConfig(){ 
  prefs.begin("tiempo",true); 
  cfg.ssid=prefs.getString("ssid",""); 
  cfg.pass=prefs.getString("pass",""); 
  cfg.ciudad=prefs.getString("ciudad",""); 
  cfg.lat=prefs.getFloat("lat",NAN); 
  cfg.lon=prefs.getFloat("lon",NAN); 
  cfg.lang=prefs.getInt("lang",0); 
  cfg.uni=prefs.getInt("uni",0);
  cfg.fmt=prefs.getInt("fmt",0);
  prefs.end(); 
  cfg.valida=cfg.ssid.length()>0 && cfg.ciudad.length()>0 && !isnan(cfg.lat) && !isnan(cfg.lon); 
}

void guardarWifi(const String& s,const String& p){ 
  prefs.begin("tiempo",false); 
  prefs.putString("ssid",s); 
  prefs.putString("pass",p); 
  prefs.end(); 
  cfg.ssid=s; 
  cfg.pass=p; 
}

void guardarCiudad(const String& n,float la,float lo, int ln, int uni, int fmt){ 
  prefs.begin("tiempo",false); 
  prefs.putString("ciudad",n); 
  prefs.putFloat("lat",la); 
  prefs.putFloat("lon",lo); 
  prefs.putInt("lang",ln);
  prefs.putInt("uni",uni);
  prefs.putInt("fmt",fmt);
  prefs.end(); 
  cfg.ciudad=n; 
  cfg.lat=la; 
  cfg.lon=lo; 
  cfg.lang=ln;
  cfg.uni=uni;
  cfg.fmt=fmt;
  cfg.valida=cfg.ssid.length()>0; 
}

struct Dia{ int code; int tmax; int tmin; int lluvia; };
RTC_DATA_ATTR time_t ultimoUpdateWeb = 0;
RTC_DATA_ATTR bool datosValidosRTC = false;
RTC_DATA_ATTR bool rtc_avisoBateriaMostrado = false;
RTC_DATA_ATTR int rtc_codeAhora=0, rtc_tempAhora=0, rtc_sensacion=0, rtc_humedad=0, rtc_viento=0;
RTC_DATA_ATTR Dia rtc_dias[4];
int codeAhora; int tempAhora; int sensacion; int humedad; int viento; Dia dias[4];
bool hayInterior=false; float tempInt=0; float humInt=0;
int nivelBateria=-1; bool cargando=false;
bool avisoBateriaBaja=false;
bool iconoLunaVisible=false;
int voltajeBateria=0;
int corrienteBateria=0;

bool leerBateriaReal(){
  bool ok=false;
  for(int i=0;i<5;i++){
    sensorWire.beginTransmission(0x55);
    sensorWire.write(0x2C);
    if(sensorWire.endTransmission(false)==0){
      if(sensorWire.requestFrom((uint8_t)0x55, (uint8_t)2)==2){
        uint8_t lo = sensorWire.read(); uint8_t hi = sensorWire.read();
        int soc = lo;
        if(soc>=0 && soc<=100){ nivelBateria=soc; ok=true; break; }
      }
    }
    delay(20);
  }
  if(!ok && nivelBateria<0) nivelBateria=50;
  int voltaje=0;
  sensorWire.beginTransmission(0x55);
  sensorWire.write(0x08);
  if(sensorWire.endTransmission(false)==0){
    if(sensorWire.requestFrom((uint8_t)0x55, (uint8_t)2)==2){
      uint8_t vLo = sensorWire.read(); uint8_t vHi = sensorWire.read();
      voltaje = (vHi<<8)|vLo;
    }
  }
  sensorWire.beginTransmission(0x55);
  sensorWire.write(0x0C);
  if(sensorWire.endTransmission(false)==0){
    if(sensorWire.requestFrom((uint8_t)0x55, (uint8_t)2)==2){
      int16_t corriente = (int16_t)(sensorWire.read() | (sensorWire.read()<<8));
      if(voltaje > 3800) cargando = true;
      else if(voltaje > 3600 && abs(corriente) > 10) cargando = true;
      else cargando = false;
      Serial.printf("BAT soc=%d volt=%d cur=%d carg=%d\n",nivelBateria,voltaje,corriente,cargando);
      return true;
    }
  }
  cargando = voltaje > 4100;
  Serial.printf("BAT fb soc=%d volt=%d carg=%d\n",nivelBateria,voltaje,cargando);
  return ok;
}

bool leerBateria(){
  bool ok = leerBateriaReal();
#if SIMULAR_BATERIA >= 0
  nivelBateria = SIMULAR_BATERIA;
  cargando = false;
  Serial.printf("BAT SIMULADA %d%%\n", nivelBateria);
  return true;
#else
  return ok;
#endif
}

void leerSHT40(){
#if USAR_SHT40
  sensorWire.beginTransmission(0x44);
  sensorWire.write(0xFD);
  if(sensorWire.endTransmission()!=0) return;
  delay(15);
  if(sensorWire.requestFrom((uint8_t)0x44, (uint8_t)6)!=6) return;
  uint8_t b[6]; for(int i=0;i<6;i++) b[i]=sensorWire.read();
  uint16_t rt=(b[0]<<8)|b[1]; uint16_t rh=(b[3]<<8)|b[4];
  tempInt=-45.0f+175.0f*rt/65535.0f; humInt=constrain(-6.0f+125.0f*rh/65535.0f,0.0f,100.0f); hayInterior=true;
#endif
}

int anchoTxt(const char* s,int size){ int n=0; for(;*s;s++) if(((uint8_t)*s & 0xC0)!=0x80) n++; return n*6*size; }

void txt(const char* s,int x,int y,int size,bool negrita){
  display.setTextSize(size); display.setTextColor(NEGRO); int cw=6*size; int g=max(1,size/3);
  while(*s){
    uint8_t c=(uint8_t)*s; char base=0; int marca=0; bool esComa=false; bool esPunto=false;
    if(c<0x80){ 
      if(c==','){ esComa=true; s++; }
      else if(c=='.'){ esPunto=true; s++; }
      else { base=c; s++; }
    }
    else if(c==0xC3 && s[1]){
      uint8_t d=(uint8_t)s[1]; s+=2;
      switch(d){
        case 0xA1: base='a'; marca=1; break; case 0xA9: base='e'; marca=1; break; case 0xAD: base='i'; marca=1; break;
        case 0xB3: base='o'; marca=1; break; case 0xBA: base='u'; marca=1; break; case 0xB1: base='n'; marca=2; break;
        case 0x81: base='A'; marca=1; break; case 0x89: base='E'; marca=1; break; case 0x8D: base='I'; marca=1; break;
        case 0x93: base='O'; marca=1; break; case 0x9A: base='U'; marca=1; break; case 0x91: base='N'; marca=2; break;
        case 0xA0: base='a'; marca=3; break; case 0xA2: base='a'; marca=4; break; case 0xA4: base='a'; marca=5; break; case 0xA7: base='c'; marca=6; break;
        case 0xA8: base='e'; marca=3; break; case 0xAA: base='e'; marca=4; break; case 0xAB: base='e'; marca=5; break;
        case 0xAE: base='i'; marca=4; break; case 0xAF: base='i'; marca=5; break; case 0xB4: base='o'; marca=4; break; case 0xB6: base='o'; marca=5; break;
        case 0xB9: base='u'; marca=3; break; case 0xBB: base='u'; marca=4; break; case 0xBC: base='u'; marca=5; break;
        case 0x84: base='A'; marca=5; break; case 0x96: base='O'; marca=5; break; case 0x9C: base='U'; marca=5; break;
      }
    } else { s++; continue; }
    if(esComa){
      display.fillRect(x+2*size, y+5*size, 2*size, 2*size, NEGRO);
      display.fillRect(x+1*size, y+6*size, 3*size, 1*size, NEGRO);
    } else if(esPunto){
      display.fillRect(x+2*size, y+5*size, 2*size, 2*size, NEGRO);
    } else if(base){
      char b[2]={base,0}; display.drawString(b,x,y); if(negrita) display.drawString(b,x+g,y);
      int dy=0; if(base>='A' && base<='Z') dy=-2*size;
      if(marca==1){ display.fillRect(x+3*size,y+dy,size,size,NEGRO); display.fillRect(x+2*size,y+dy+size,size,size,NEGRO); }
      else if(marca==2){ display.fillRect(x+1*size,y+dy+size,size,size,NEGRO); display.fillRect(x+2*size,y+dy,size,size,NEGRO); display.fillRect(x+3*size,y+dy+size,size,size,NEGRO); display.fillRect(x+4*size,y+dy,size,size,NEGRO); }
      else if(marca==3){ display.fillRect(x+1*size,y+dy,size,size,NEGRO); display.fillRect(x+2*size,y+dy+size,size,size,NEGRO); }
      else if(marca==4){ display.fillRect(x+2*size,y+dy,size,size,NEGRO); display.fillRect(x+1*size,y+dy+size,size,size,NEGRO); display.fillRect(x+3*size,y+dy+size,size,size,NEGRO); }
      else if(marca==5){ display.fillRect(x+1*size,y+dy,size,size,NEGRO); display.fillRect(x+3*size,y+dy,size,size,NEGRO); }
      else if(marca==6){ display.fillRect(x+2*size,y+6*size,size,size,NEGRO); display.fillRect(x+1*size,y+7*size,2*size,size,NEGRO); }
    }
    x+=cw;
  }
}
void txtCentrado(const char* s,int cx,int y,int size,bool negrita){ int w=anchoTxt(s,size); txt(s,cx-w/2,y,size,negrita); }
void txtDerecha(const char* s,int xd,int y,int size,bool negrita){ int w=anchoTxt(s,size); txt(s,xd-w,y,size,negrita); }
void txtSimple(const char* s,int x,int y,int size,bool negrita){ txt(s,x,y,size,negrita); }

WeatherIcon iconoDe(int c){
  if(c==0) return WICON_SOL; if(c<=2) return WICON_POCO_NUBLADO; if(c==3) return WICON_NUBLADO;
  if(c==45||c==48) return WICON_NIEBLA; if((c>=51&&c<=67)||(c>=80&&c<=82)) return WICON_LLUVIA;
  if((c>=71&&c<=77)||c==85||c==86) return WICON_NIEVE; if(c>=95) return WICON_TORMENTA; return WICON_NUBLADO;
}

const char* textoDe(int c){
  if(cfg.lang == 0) {
    switch(c){
      case 0: return "Despejado"; case 1: return "Mayormente despejado"; case 2: return "Parcialmente nublado"; case 3: return "Nublado";
      case 45: case 48: return "Niebla"; case 51: case 53: case 55: return "Llovizna"; case 56: case 57: return "Llovizna helada";
      case 61: return "Lluvia ligera"; case 63: return "Lluvia"; case 65: return "Lluvia fuerte"; case 66: case 67: return "Lluvia helada";
      case 71: return "Nieve ligera"; case 73: return "Nieve"; case 75: return "Nieve fuerte"; case 77: return "Granizo fino";
      case 80: case 81: case 82: return "Chubascos"; case 85: case 86: return "Chubascos de nieve"; case 95: return "Tormenta"; case 96: case 99: return "Tormenta con granizo";
    }
  } else if(cfg.lang == 1) {
    switch(c){
      case 0: return "Clear sky"; case 1: return "Mainly clear"; case 2: return "Partly cloudy"; case 3: return "Overcast";
      case 45: case 48: return "Fog"; case 51: case 53: case 55: return "Drizzle"; case 56: case 57: return "Freezing drizzle";
      case 61: return "Slight rain"; case 63: return "Moderate rain"; case 65: return "Heavy rain"; case 66: case 67: return "Freezing rain";
      case 71: return "Slight snow"; case 73: return "Moderate snow"; case 75: return "Heavy snow"; case 77: return "Snow grains";
      case 80: case 81: case 82: return "Rain showers"; case 85: case 86: return "Snow showers"; case 95: return "Thunderstorm"; case 96: case 99: return "Thunderstorm with hail";
    }
  } else if(cfg.lang == 2) {
    switch(c){
      case 0: return "Ceu limpo"; case 1: return "Predominantemente limpo"; case 2: return "Parcialmente nublado"; case 3: return "Encoberto";
      case 45: case 48: return "Nevoeiro"; case 51: case 53: case 55: return "Chuvisco"; case 56: case 57: return "Chuvisco gelado";
      case 61: return "Chuva leve"; case 63: return "Chuva moderada"; case 65: return "Chuva forte"; case 66: case 67: return "Chuva congelante";
      case 71: return "Neve leve"; case 73: return "Neve moderada"; case 75: return "Neve forte"; case 77: return "Granizo fino";
      case 80: case 81: case 82: return "Aguaceiros"; case 85: case 86: return "Aguaceiros de neve"; case 95: return "Trovoada"; case 96: case 99: return "Trovoada com granizo";
    }
  } else if(cfg.lang == 3) {
    switch(c){
      case 0: return "Ciel degage"; case 1: return "Principalement degage"; case 2: return "Partiellement nuageux"; case 3: return "Couvert";
      case 45: case 48: return "Brouillard"; case 51: case 53: case 55: return "Bruine"; case 56: case 57: return "Bruine verglacante";
      case 61: return "Pluie legere"; case 63: return "Pluie moderee"; case 65: return "Pluie forte"; case 66: case 67: return "Pluie verglacante";
      case 71: return "Neige legere"; case 73: return "Neige moderee"; case 75: return "Neige forte"; case 77: return "Neige en grains";
      case 80: case 81: case 82: return "Averses de pluie"; case 85: case 86: return "Averses de neige"; case 95: return "Orage"; case 96: case 99: return "Orage avec grele";
    }
  } else if(cfg.lang == 4) {
    switch(c){
      case 0: return "Cielo sereno"; case 1: return "Prevalentemente sereno"; case 2: return "Parzialmente nuvoloso"; case 3: return "Nuvoloso";
      case 45: case 48: return "Nebbia"; case 51: case 53: case 55: return "Pioviggine"; case 56: case 57: return "Pioviggine gelata";
      case 61: return "Pioggia leggera"; case 63: return "Pioggia moderata"; case 65: return "Pioggia forte"; case 66: case 67: return "Pioggia gelata";
      case 71: return "Neve leggera"; case 73: return "Neve moderata"; case 75: return "Neve forte"; case 77: return "Granello di neve";
      case 80: case 81: case 82: return "Rovesci di pioggia"; case 85: case 86: return "Rovesci di neve"; case 95: return "Temporale"; case 96: case 99: return "Temporale con grandine";
    }
  } else {
    switch(c){
      case 0: return "Wolkenlos"; case 1: return "Leicht bewolkt"; case 2: return "Teilweise bewolkt"; case 3: return "Bedeckt";
      case 45: case 48: return "Nebel"; case 51: case 53: case 55: return "Nieselregen"; case 56: case 57: return "Gefrierender Niesel";
      case 61: return "Leichter Regen"; case 63: return "Massiger Regen"; case 65: return "Starker Regen"; case 66: case 67: return "Gefrierender Regen";
      case 71: return "Leichter Schneefall"; case 73: return "Schneefall"; case 75: return "Starker Schneefall"; case 77: return "Schneegriesel";
      case 80: case 81: case 82: return "Regenschauer"; case 85: case 86: return "Schneschauer"; case 95: return "Gewitter"; case 96: case 99: return "Gewitter mit Hagel";
    }
  }
  return (cfg.lang == 0) ? "Sin datos" : "No data";
}

void lineaGruesa(int x1,int y1,int x2,int y2,int g){ for(int o=-g;o<=g;o++){ display.drawLine(x1+o,y1,x2+o,y2,NEGRO); display.drawLine(x1,y1+o,x2,y2+o,NEGRO);} }
void formaNube(int cx,int cy,int s,int inset,uint32_t col){
  display.fillCircle(cx-s*0.55,cy+s*0.10,max(1,(int)(s*0.35)-inset),col);
  display.fillCircle(cx-s*0.10,cy-s*0.20,max(1,(int)(s*0.50)-inset),col);
  display.fillCircle(cx+s*0.45,cy+s*0.05,max(1,(int)(s*0.40)-inset),col);
  display.fillRect(cx-s*0.55,cy+s*0.10,s,max(1,(int)(s*0.35)-inset),col);
}
void nube(int cx,int cy,int s){ int inset=max(3,s/12); formaNube(cx,cy,s,0,NEGRO); formaNube(cx,cy,s,inset,BLANCO); }
void sol(int cx,int cy,int r){ int g=max(1,r/14); display.fillCircle(cx,cy,r*0.55,NEGRO); for(int i=0;i<8;i++){ float a=i*PI/4; lineaGruesa(cx+cos(a)*r*0.75,cy+sin(a)*r*0.75,cx+cos(a)*r*1.05,cy+sin(a)*r*1.05,g);} }
void dibujarIcono(WeatherIcon t,int cx,int cy,int s){
  int g=max(1,s/16);
  switch(t){
    case WICON_SOL: sol(cx,cy,s); break;
    case WICON_POCO_NUBLADO: sol(cx-s*0.30,cy-s*0.30,s*0.65); nube(cx+s*0.10,cy+s*0.15,s*0.85); break;
    case WICON_NUBLADO: nube(cx,cy,s); break;
    case WICON_LLUVIA: nube(cx,cy-s*0.25,s*0.9); for(int i=-1;i<=1;i++) lineaGruesa(cx+i*s*0.40,cy+s*0.45,cx+i*s*0.40-s*0.15,cy+s*0.90,g); break;
    case WICON_NIEVE: nube(cx,cy-s*0.25,s*0.9); for(int i=-1;i<=1;i++){ display.fillCircle(cx+i*s*0.40,cy+s*0.55,max(2,s/12),NEGRO); display.fillCircle(cx+i*s*0.40-s*0.12,cy+s*0.85,max(2,s/12),NEGRO);} break;
    case WICON_TORMENTA: nube(cx,cy-s*0.25,s*0.9); display.fillTriangle(cx+s*0.05,cy+s*0.30,cx-s*0.25,cy+s*0.70,cx+s*0.05,cy+s*0.65,NEGRO); display.fillTriangle(cx+s*0.05,cy+s*0.65,cx+s*0.25,cy+s*0.65,cx-s*0.10,cy+s*1.05,NEGRO); break;
    case WICON_NIEBLA: nube(cx,cy-s*0.30,s*0.8); for(int i=0;i<3;i++) lineaGruesa(cx-s*0.7,cy+s*0.30+i*s*0.28,cx+s*0.7,cy+s*0.30+i*s*0.28,g); break;
  }
}
void engranaje(int cx,int cy,int r){ display.fillCircle(cx,cy,r*0.70,NEGRO); for(int i=0;i<8;i++){ float a=i*PI/4; display.fillCircle(cx+cos(a)*r*0.85,cy+sin(a)*r*0.85,max(2,(int)(r*0.24)),NEGRO);} display.fillCircle(cx,cy,r*0.30,BLANCO); }

bool conectarWifi(const String& ssid,const String& pass){ WiFi.mode(WIFI_STA); WiFi.begin(ssid.c_str(),pass.c_str()); for(int i=0;i<40 && WiFi.status()!=WL_CONNECTED;i++) delay(500); return WiFi.status()==WL_CONNECTED; }
String urlEnc(const String& s){ static const char* hex="0123456789ABCDEF"; String o; for(size_t i=0;i<s.length();i++){ uint8_t c=s[i]; if(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~') o+=(char)c; else { o+='%'; o+=hex[c>>4]; o+=hex[c&15]; } } return o; }

bool obtenerTiempo(){
  float lat=cfg.valida?cfg.lat:42.4627; float lon=cfg.valida?cfg.lon:-2.4449;
  String url="https://api.open-meteo.com/v1/forecast?latitude="+String(lat,4)+"&longitude="+String(lon,4)+"&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max&timezone=Europe%2FMadrid&forecast_days=4";
  WiFiClientSecure client; client.setInsecure(); HTTPClient http; http.begin(client,url);
  int cod=http.GET(); if(cod!=200){ http.end(); return false; }
  DynamicJsonDocument doc(8192); DeserializationError err=deserializeJson(doc,http.getString()); http.end(); if(err) return false;
  JsonObject cur=doc["current"]; codeAhora=cur["weather_code"]; tempAhora=round((float)cur["temperature_2m"]); sensacion=round((float)cur["apparent_temperature"]); humedad=cur["relative_humidity_2m"]; viento=round((float)cur["wind_speed_10m"]);
  JsonObject d=doc["daily"]; for(int i=0;i<4;i++){ dias[i].code=d["weather_code"][i]; dias[i].tmax=round((float)d["temperature_2m_max"][i]); dias[i].tmin=round((float)d["temperature_2m_min"][i]); dias[i].lluvia=d["precipitation_probability_max"][i]|0; }
  return true;
}

void dibujarLunaMenguante(int cx, int cy, int r){
  display.fillCircle(cx, cy, r, NEGRO);
  display.fillCircle(cx+2, cy, r-2, BLANCO);
}

void dibujarBateria(int x, int y, int pct, bool chg){
  int w=32; int h=16; int borde=2;
  display.drawRect(x,y,w,h,NEGRO);
  display.fillRect(x+w,y+4,3,h-8,NEGRO);
  if(pct>=0){
    int fillW = map(constrain(pct,0,100),0,100,0,w-borde*2);
    if(fillW>0) display.fillRect(x+borde,y+borde,fillW,h-borde*2,NEGRO);
  }
  if(chg){
    int rx = x + w/2 - 2;
    int ry = y + 1;
    display.fillTriangle(rx+4, ry, rx-2, ry+7, rx+2, ry+7, NEGRO);
    display.fillTriangle(rx+2, ry+7, rx+6, ry+7, rx+0, ry+14, NEGRO);
  }
  char buf[8]; snprintf(buf,sizeof(buf),"%d%%",pct>=0?pct:0);
  txtSimple(buf,x+w+8,y,2,true);
}

void dibujarBateria(int x,int y,int pct){
  dibujarBateria(x,y,pct,cargando);
}

void dibujar() {
  static const char* dsem[6][7] = {
    {"domingo","lunes","martes","miercoles","jueves","viernes","sabado"},
    {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"},
    {"Dom","Seg","Ter","Qua","Qui","Sex","Sab"},
    {"dimanche","lundi","mardi","mercredi","jeudi","vendredi","samedi"},
    {"domenica","lunedi","martedi","mercoledi","giovedi","venerdi","sabato"},
    {"Sonntag","Montag","Dienstag","Mittwoch","Donnerstag","Freitag","Samstag"}
  };
  static const char* meses[6][12] = {
    {"enero","febrero","marzo","abril","mayo","junio","julio","agosto","septiembre","octubre","noviembre","diciembre"},
    {"January","February","March","April","May","June","July","August","September","October","November","December"},
    {"janeiro","fevereiro","marco","abril","maio","junho","julho","agosto","setembro","outubro","novembro","dezembro"},
    {"janvier","fevrier","mars","avril","mai","juin","juillet","aout","septembre","octobre","novembre","decembre"},
    {"gennaio","febbraio","marzo","aprile","maggio","giugno","luglio","agosto","setembre","ottobre","novembre","dicembre"},
    {"Januar","Februar","Marz","April","Mai","Juni","Juli","August","September","Oktober","November","Dezember"}
  };
  static const char* dcor[6][7] = {
    {"Dom","Lun","Mar","Mie","Jue","Vie","Sab"},
    {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"},
    {"Dom","Seg","Ter","Qua","Qui","Sex","Sab"},
    {"Dim","Lun","Mar","Mer","Jeu","Ven","Sam"},
    {"Dom","Lun","Mar","Mer","Gio","Ven","Sab"},
    {"Son","Mon","Die","Mit","Don","Fre","Sam"}
  };

  char buf[128]; time_t ahora=time(nullptr); struct tm ti; localtime_r(&ahora,&ti); bool horaOk=ti.tm_year>120;
  display.fillScreen(BLANCO);

  String fStr = "";
  if(horaOk){
    int anio = ti.tm_year + 1900;
    // fmt: 0=ES largo, 1=US americano, 2=ISO
    if(cfg.fmt == 1){
      // US: October . Wednesday 14 . 2026
      fStr = String(meses[cfg.lang][ti.tm_mon]) + " . " + String(dsem[cfg.lang][ti.tm_wday]) + " " + String(ti.tm_mday) + " . " + String(anio);
    } else if(cfg.fmt == 2){
      char iso[16]; snprintf(iso,sizeof(iso),"%04d-%02d-%02d",anio,ti.tm_mon+1,ti.tm_mday);
      fStr = String(iso) + " " + String(dsem[cfg.lang][ti.tm_wday]);
    } else {
      if(cfg.lang == 0)      fStr = String(dsem[0][ti.tm_wday]) + ", " + String(ti.tm_mday) + " de " + String(meses[0][ti.tm_mon]) + " de " + String(anio);
      else if(cfg.lang == 1) fStr = String(dsem[1][ti.tm_wday]) + ", " + String(meses[1][ti.tm_mon]) + " " + String(ti.tm_mday) + ", " + String(anio);
      else if(cfg.lang == 2) fStr = String(dsem[2][ti.tm_wday]) + ". " + String(ti.tm_mday) + " de " + String(meses[2][ti.tm_mon]) + " de " + String(anio);
      else if(cfg.lang == 3) fStr = String(dsem[3][ti.tm_wday]) + " " + String(ti.tm_mday) + " " + String(meses[3][ti.tm_mon]) + " " + String(anio);
      else if(cfg.lang == 4) fStr = String(dsem[4][ti.tm_wday]) + " " + String(ti.tm_mday) + " " + String(meses[4][ti.tm_mon]) + " " + String(anio);
      else                   fStr = String(dsem[5][ti.tm_wday]) + ", " + String(ti.tm_mday) + ". " + String(meses[5][ti.tm_mon]) + " " + String(anio);
    }
  }

  const char* ciudad = cfg.valida ? cfg.ciudad.c_str() : "Logrono";
  const int HEADER_H = 85;
  const int LUNA_X = 32;
  const int LUNA_Y = 18;
  const int LUNA_R = 8;
  int fechaX = 400;
  if(iconoLunaVisible){
    dibujarLunaMenguante(LUNA_X, LUNA_Y, LUNA_R);
  }
  if(horaOk){
    txtCentrado(fStr.c_str(), fechaX, 14, 3, false);
  }
  dibujarBateria(700, 10, nivelBateria, cargando);
  int sizeCiudad = 4;
  while(sizeCiudad > 2 && anchoTxt(ciudad, sizeCiudad) > 760){
    sizeCiudad--;
  }
  txtSimple(ciudad, 20, 42, sizeCiudad, true);
  display.fillRect(20, HEADER_H, 760, 3, NEGRO);
  int mitadX = 400;
  int yTopCentral = HEADER_H + 12;
  int hCentral = 190;
  display.drawFastVLine(mitadX, yTopCentral, hCentral, NEGRO);
  const char* lblExt = "EXTERIOR"; const char* lblInt = "INTERIOR";
  const char* fmtSens = "Sens %d  Hum %d%%"; const char* fmtVnt = "Viento %d km/h";
  const char* fmtHumI = "Humedad %.0f%%"; const char* lblSinS = "Sin sensor";
  const char* fmtLlu = "Lluvia %d%%";
  if(cfg.lang == 1) { lblExt = "OUTDOOR"; lblInt = "INDOOR"; fmtSens = "Feels %d  Hum %d%%"; fmtVnt = "Wind %d mph"; fmtHumI = "Humidity %.0f%%"; lblSinS = "No sensor"; fmtLlu = "Rain %d%%"; }
  else if(cfg.lang == 2) { lblExt = "EXTERIOR"; lblInt = "INTERIOR"; fmtSens = "Sens %d  Hum %d%%"; fmtVnt = "Vento %d km/h"; fmtHumI = "Humidade %.0f%%"; lblSinS = "Sem sensor"; fmtLlu = "Chuva %d%%"; }
  else if(cfg.lang == 3) { lblExt = "EXTERIEUR"; lblInt = "INTERIEUR"; fmtSens = "Ressenti %d Humi %d%%"; fmtVnt = "Vent %d km/h"; fmtHumI = "Humidite %.0f%%"; lblSinS = "Pas de capteur"; fmtLlu = "Pluie %d%%"; }
  else if(cfg.lang == 4) { lblExt = "ESTERNO"; lblInt = "INTERNO"; fmtSens = "Percep %d  Umid %d%%"; fmtVnt = "Vento %d km/h"; fmtHumI = "Umidita %.0f%%"; lblSinS = "Senza sensore"; fmtLlu = "Pioggia %d%%"; }
  else if(cfg.lang == 5) { lblExt = "AUSSEN"; lblInt = "INNEN"; fmtSens = "Gefuhlt %d  Feuchte %d%%"; fmtVnt = "Wind %d km/h"; fmtHumI = "Feuchtigkeit %.0f%%"; lblSinS = "Kein Sensor"; fmtLlu = "Regen %d%%"; }
  int centroY = yTopCentral + hCentral/2;
  int tempMostrar = tempAhora;
  int sensMostrar = sensacion;
  int vientoMostrar = viento;
  float tempIntMostrar = tempInt;
  char unidadTemp = 'C';
  if(cfg.uni == 1){
    tempMostrar = round(tempAhora * 9.0/5.0 + 32);
    sensMostrar = round(sensacion * 9.0/5.0 + 32);
    vientoMostrar = round(viento * 0.621371);
    tempIntMostrar = tempInt * 9.0/5.0 + 32;
    unidadTemp = 'F';
  }
  txtCentrado(lblExt, 200, yTopCentral+8, 3, true);
  int yTemp = centroY - 24;
  dibujarIcono(iconoDe(codeAhora), 60, yTemp, 36);
  snprintf(buf,sizeof(buf),"%d",tempMostrar);
  txtSimple(buf, 150, yTemp, 6, true); 
  char uStr[2] = {unidadTemp, 0};
  txtSimple(uStr, 150+anchoTxt(buf,6)+6, yTemp, 4, true);
  txtCentrado(textoDe(codeAhora), 200, centroY+36, 2, true);
  snprintf(buf,sizeof(buf),fmtSens,sensMostrar,humedad); txtCentrado(buf, 200, centroY+58, 2, false);
  snprintf(buf,sizeof(buf),fmtVnt,vientoMostrar); txtCentrado(buf, 200, centroY+78, 2, false);
  txtCentrado(lblInt, 600, yTopCentral+8, 3, true);
  if(hayInterior){
    int casaX=410; 
    int casaY=yTemp;
    display.drawTriangle(casaX+25, casaY, casaX, casaY+25, casaX+50, casaY+25, NEGRO);
    display.drawTriangle(casaX+25, casaY+2, casaX+3, casaY+25, casaX+47, casaY+25, NEGRO);
    display.fillRect(casaX+4, casaY+25, 42, 30, NEGRO);
    display.fillRect(casaX+12, casaY+35, 18, 20, BLANCO);
    char bufInt[16]; snprintf(bufInt,sizeof(bufInt),"%.1f",tempIntMostrar);
    if(cfg.uni != 1) for(char *p=bufInt; *p; p++) if(*p=='.') *p=',';
    txtSimple(bufInt, 510, yTemp, 6, true);
    char uStr2[2] = {unidadTemp, 0};
    txtSimple(uStr2, 510+anchoTxt(bufInt,6)+6, yTemp, 4, true);
    snprintf(buf,sizeof(buf),fmtHumI,humInt); txtCentrado(buf, 600, centroY+58, 2, false);
  } else {
    txtCentrado(lblSinS, 600, centroY-10, 3, true);
  }
  int yPrevTop = yTopCentral + hCentral + 18;
  display.fillRect(20, yPrevTop, 760, 2, NEGRO);
  for(int i=1;i<=3;i++){
    int cx=20+(i-1)*253+126; 
    if(i>1) display.drawFastVLine(20+(i-1)*253, yPrevTop+18, 150, NEGRO);
    const char* nombre="";  
    if(horaOk){ time_t t=ahora+86400L*i; struct tm tf; localtime_r(&t,&tf); nombre = dcor[cfg.lang][tf.tm_wday]; }
    txtCentrado(nombre,cx,yPrevTop+20,3,true); 
    dibujarIcono(iconoDe(dias[i].code),cx,yPrevTop+80,36);
    snprintf(buf,sizeof(buf),"%d / %d",dias[i].tmax,dias[i].tmin); txtCentrado(buf,cx,yPrevTop+122,2,false);
    snprintf(buf,sizeof(buf),fmtLlu,dias[i].lluvia); txtCentrado(buf,cx,yPrevTop+144,2,false);
  }
  display.update();
}

void pantallaMensaje(const char* l1,const char* l2){
  display.fillScreen(BLANCO); engranaje(752,26,17); display.fillTriangle(744,46,760,46,752,54,NEGRO);
  txtCentrado(l1,400,180,4,true); txtCentrado(l2,400,250,2,false); 
  txtCentrado("Configuracion activa / Configuration mode",400,330,2,false); 
  display.update();
}

void pantallaConfig(){
  display.fillScreen(BLANCO); engranaje(70,70,40); 
  txtSimple("Configuracion / Setup",140,40,5,true); display.fillRect(20,118,760,3,NEGRO);
  txtSimple("1. Conecta a la red / Connect to WiFi:",40,145,3,false); txtSimple(AP_NOMBRE,80,185,4,true); 
  txtSimple("2. Abre el navegador / Open browser:",40,245,3,false); txtSimple("192.168.4.1",80,285,4,true); 
  txtSimple("3. Elige WiFi, Idioma y Ciudad",40,345,3,false); 
  display.update();
}

WebServer servidor(80); DNSServer dns; int wifiEstado=0; unsigned long wifiInicio=0; String tmpSsid; String tmpPass; bool terminado=false;

const char PAGINA[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="es"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Sticky Tiempo</title>
<style>body{font-family:system-ui,sans-serif;max-width:480px;margin:0 auto;padding:16px;background:#f4f4f4;color:#111}h2{margin:0 0 8px}h3{margin:22px 0 6px}select,input,button{width:100%;box-sizing:border-box;font-size:16px;padding:11px;margin:4px 0;border:1px solid #999;border-radius:8px;background:#fff}button{background:#111;color:#fff;border:0}button.sec{background:#fff;color:#111;border:1px solid #111;text-align:left}.msg{min-height:1.4em;font-size:14px;margin:4px 0}</style></head><body>
<h2>Sticky Tiempo</h2>
<h3>1. WiFi</h3><select id="redes"><option>Buscando redes...</option></select><button class="sec" onclick="buscarRedes()">Volver a buscar redes / Rescan</button><input id="clave" type="password" placeholder="Contraseña / Password"><button onclick="conectar()">Conectar / Connect</button><div class="msg" id="mWifi"></div>
<h3>2. Idioma / Language</h3><select id="lang"><option value="0">Español</option><option value="1">English</option><option value="2">Português</option><option value="3">Français</option><option value="4">Italiano</option><option value="5">Deutsch</option></select>
<h3>3. Unidades / Units</h3><select id="unidad"><option value="0">Celsius °C - km/h</option><option value="1">Fahrenheit °F - mph</option></select>
<h3>4. Formato Fecha / Date</h3><select id="fmtFecha"><option value="0">ES: lunes, 14 de octubre</option><option value="1">US: October - Wednesday 14</option><option value="2">ISO: 2026-10-14</option></select>
<h3>5. Ciudad / City</h3><input id="q" placeholder="Escribe tu ciudad / Type your city"><button onclick="buscarCiudad()">Buscar ciudad / Search</button><div id="lista"></div><div class="msg" id="mCiudad"></div>
<script>
let ciudades=[], t=null;const $=id=>document.getElementById(id);
async function buscarRedes(){$('redes').innerHTML='<option>Buscando redes...</option>';try{const r=await fetch('/redes');const l=await r.json();const sel=$('redes');sel.innerHTML='';if(!l.length){sel.innerHTML='<option>No se encontraron redes</option>';return;}l.forEach(x=>{const o=document.createElement('option');o.value=x.s;o.textContent=x.s+(x.l?' (con clave)':' (abierta)')+'  '+x.r+' dBm';sel.appendChild(o);});}catch(e){$('redes').innerHTML='<option>Error al buscar, pulsa de nuevo</option>';}}
async function conectar(){const s=$('redes').value;if(!s||s.startsWith('Buscando')||s.startsWith('No se')||s.startsWith('Error'))return;$('mWifi').textContent='Conectando...';try{await fetch('/wifi',{method:'POST',body:new URLSearchParams({ssid:s,pass:$('clave').value})});}catch(e){}sondeo();}
function sondeo(){clearInterval(t);t=setInterval(async()=>{try{const j=await (await fetch('/estado')).json();if(j.wifi==1){$('mWifi').textContent='Conectando...';}if(j.wifi==2){$('mWifi').textContent='✓ Conectada a '+j.ssid;clearInterval(t);}if(j.wifi==3){$('mWifi').textContent='✗ No se pudo conectar. Revisa la contraseña.';clearInterval(t);}}catch(e){}},1500);}
async function buscarCiudad(){const q=$('q').value.trim();if(!q)return;$('mCiudad').textContent='Buscando...';$('lista').innerHTML='';try{const ln=$('lang').value;const langs=['es','en','pt','fr','it','de'];const j=await (await fetch('/ciudades?q='+encodeURIComponent(q)+'&l='+langs[ln])).json();if(j.error){$('mCiudad').textContent=j.error;return;}ciudades=j;if(!j.length){$('mCiudad').textContent='Sin resultados';return;}$('mCiudad').textContent='Elige tu ciudad:';j.forEach((c,i)=>{const b=document.createElement('button');b.className='sec';b.textContent=[c.n,c.r,c.p].filter(Boolean).join(', ');b.onclick=()=>elegir(i);$('lista').appendChild(b);});}catch(e){$('mCiudad').textContent='Error de conexión';}}
async function elegir(i){const c=ciudades[i];const ln=$('lang').value;const uni=$('unidad').value;const fmt=$('fmtFecha').value;try{await fetch('/guardar',{method:'POST',body:new URLSearchParams({n:c.n,la:c.la,lo:c.lo,lang:ln,unidad:uni,fmtFecha:fmt})});$('mCiudad').textContent='✓ Guardado. Reiniciando... / Saved. Restarting...';$('lista').innerHTML='';}catch(e){$('mCiudad').textContent='Error al guardar';}}
buscarRedes();sondeo();
</script></body></html>
)rawliteral";

void hRaiz(){ servidor.send(200,"text/html; charset=utf-8", PAGINA); }
void hRedes(){ Serial0.println("Scan redes..."); int n=WiFi.scanNetworks(false, false, false, 300); DynamicJsonDocument doc(8192); JsonArray arr=doc.to<JsonArray>(); for(int i=0;i<n && (int)arr.size()<15;i++){ String s=WiFi.SSID(i); if(!s.length()) continue; bool dup=false; for(JsonVariant v:arr){ JsonObject o=v.as<JsonObject>(); if(o["s"]==s){dup=true;break;}} if(dup) continue; JsonObject o=arr.add<JsonObject>(); o["s"]=s; o["r"]=WiFi.RSSI(i); o["l"]=WiFi.encryptionType(i)!=WIFI_AUTH_OPEN; } WiFi.scanDelete(); String out; serializeJson(doc,out); servidor.send(200,"application/json; charset=utf-8", out); }
void hWifi(){ tmpSsid=servidor.arg("ssid"); tmpPass=servidor.arg("pass"); WiFi.begin(tmpSsid.c_str(), tmpPass.c_str()); wifiEstado=1; wifiInicio=millis(); servidor.send(200,"application/json","{\"ok\":true}"); }
void hEstado(){ String s="{\"wifi\":"+String(wifiEstado)+",\"ssid\":\""; s+=(wifiEstado==2?tmpSsid:cfg.ssid); s+="\"}"; servidor.send(200,"application/json; charset=utf-8",s); }
void hCiudades(){ 
  if(WiFi.status()!=WL_CONNECTED){ servidor.send(200,"application/json; charset=utf-8","{\"error\":\"Conecta primero la WiFi (paso 1)\"}"); return; } 
  String apiLang = servidor.arg("l"); if(apiLang.length() == 0) apiLang = "es";
  String url="https://geocoding-api.open-meteo.com/v1/search?name="+urlEnc(servidor.arg("q"))+"&count=20&language="+apiLang+"&format=json"; 
  WiFiClientSecure cl; cl.setInsecure(); HTTPClient http; http.begin(cl,url); 
  int cod=http.GET(); if(cod!=200){http.end(); servidor.send(200,"application/json; charset=utf-8","{\"error\":\"No se pudo buscar.\"}"); return;} 
  DynamicJsonDocument in(4096); deserializeJson(in,http.getString()); http.end(); 
  DynamicJsonDocument out(4096); JsonArray arr=out.to<JsonArray>(); 
  if(in.containsKey("results")) {
    JsonArray _results = in["results"].as<JsonArray>(); for(JsonObject r: _results){ JsonObject o=arr.add<JsonObject>(); o["n"]=r["name"]|""; o["r"]=r["admin1"]|""; o["p"]=r["country"]|""; o["la"]=r["latitude"]; o["lo"]=r["longitude"]; } 
  }
  String s; serializeJson(out,s); servidor.send(200,"application/json; charset=utf-8",s); 
}
void hGuardar(){ 
  String n=servidor.arg("n"); float la=servidor.arg("la").toFloat(); float lo=servidor.arg("lo").toFloat(); int ln=servidor.arg("lang").toInt(); int uni=servidor.arg("unidad").toInt(); if(servidor.hasArg("uni")) uni=servidor.arg("uni").toInt(); int fmt=servidor.arg("fmtFecha").toInt(); if(servidor.hasArg("fmt")) fmt=servidor.arg("fmt").toInt();
  if(n.length()==0){servidor.send(400,"text/plain","falta ciudad"); return;} guardarCiudad(n,la,lo,ln,uni,fmt); servidor.send(200,"application/json","{\"ok\":true}"); terminado=true; 
}
void hOtros(){ servidor.sendHeader("Location","http://192.168.4.1/"); servidor.send(302,"text/plain",""); }
void procesarWifi(){ if(wifiEstado!=1) return; if(WiFi.status()==WL_CONNECTED){wifiEstado=2; guardarWifi(tmpSsid,tmpPass);} else if(millis()-wifiInicio>20000){wifiEstado=3; WiFi.disconnect(false);} }

void modoConfiguracion(){
  Serial0.println("Entrando modo configuracion");
  pantallaConfig();
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_NOMBRE);
  delay(500);
  dns.start(53,"*",WiFi.softAPIP());
  servidor.on("/",hRaiz);
  servidor.on("/redes",hRedes);
  servidor.on("/wifi",HTTP_POST,hWifi);
  servidor.on("/estado",hEstado);
  servidor.on("/ciudades",hCiudades);
  servidor.on("/guardar",HTTP_POST,hGuardar);
  servidor.onNotFound(hOtros);
  servidor.begin();
  Serial0.printf("AP %s IP %s\n", AP_NOMBRE, WiFi.softAPIP().toString().c_str());
  if(cfg.ssid.length()){
    tmpSsid=cfg.ssid; tmpPass=cfg.pass;
    WiFi.begin(tmpSsid.c_str(), tmpPass.c_str());
    wifiEstado=1; wifiInicio=millis();
    Serial0.printf("Intentando WiFi guardada %s\n", tmpSsid.c_str());
  }
  unsigned long t0=millis();
  unsigned long tLastLog=0;
  while(!terminado && millis()-t0<TIEMPO_PORTAL_MS){
    dns.processNextRequest();
    servidor.handleClient();
    procesarWifi();
    if(digitalRead(PIN_BTN_DOWN)==LOW){
      unsigned long tp=millis();
      while(digitalRead(PIN_BTN_DOWN)==LOW && millis()-tp < 5000){ delay(50); dns.processNextRequest(); servidor.handleClient(); }
      if(millis()-tp >= 5000){
        Serial0.println("Salida forzada portal por boton DOWN largo");
        break;
      }
    }
    if(millis()-tLastLog > 5000){
      tLastLog=millis();
      Serial0.printf("Portal vivo %lu s / %lu s wifiEstado=%d clientes=%d\n", (millis()-t0)/1000, TIEMPO_PORTAL_MS/1000, wifiEstado, WiFi.softAPgetStationNum());
    }
    delay(20);
    yield();
  }
  Serial0.println("Saliendo portal");
  servidor.stop();
  dns.stop();
  WiFi.softAPdisconnect(true);
  delay(500);
  if(terminado){
    Serial0.println("Config guardada, reiniciando...");
    delay(2000);
    ESP.restart();
  }
}

bool bateriaCritica(){
  leerBateria();
  int limite = rtc_avisoBateriaMostrado ? UMBRAL_BATERIA_RECUPERA : UMBRAL_BATERIA_BAJA;
  if(cargando || nivelBateria<0 || nivelBateria>limite) return false;
  delay(1000);
  leerBateria();
  return (!cargando && nivelBateria>=0 && nivelBateria<=limite);
}

void pantallaBateriaBaja(int pct){
  static const char* titulo[6]    = {"BATERIA BAJA","LOW BATTERY","BATERIA FRACA","BATTERIE FAIBLE","BATTERIA SCARICA","AKKU SCHWACH"};
  static const char* subtitulo[6] = {"Conecta el cargador","Connect the charger","Ligue o carregador","Branchez le chargeur","Collega il caricatore","Ladegerat anschliessen"};
  int l = (cfg.lang>=0 && cfg.lang<=5) ? cfg.lang : 0;
  display.fillScreen(BLANCO);
  int bw=220, bh=120, bx=282, by=50;
  for(int i=0;i<4;i++) display.drawRect(bx+i,by+i,bw-2*i,bh-2*i,NEGRO);
  display.fillRect(bx+bw,by+bh/2-25,16,50,NEGRO);
  display.fillRect(bx+12,by+12,22,bh-24,NEGRO);
  char buf[8]; snprintf(buf,sizeof(buf),"%d%%",pct<0?0:pct);
  txtCentrado(buf,400,200,4,true);
  txtCentrado(titulo[l],400,265,5,true);
  txtCentrado(subtitulo[l],400,335,3,false);
  display.update();
}

void dormir(uint64_t minutos){
  const int pines[]={PIN_BTN_AI,PIN_BTN_UP,PIN_BTN_DOWN}; uint64_t mascara=0;
  for(int p:pines){ pinMode(p, INPUT_PULLUP); rtc_gpio_pullup_en((gpio_num_t)p); rtc_gpio_pulldown_dis((gpio_num_t)p); mascara|=(1ULL<<p); }
  rtc_gpio_hold_en((gpio_num_t)PIN_PWR_HOLD); rtc_gpio_hold_en((gpio_num_t)PIN_PWR_LOCK);
  rtc_gpio_hold_en((gpio_num_t)PIN_EPD_PWR);
  Serial.printf("Dormir %llu min\n",minutos); delay(200);
  esp_sleep_enable_ext1_wakeup(mascara, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_sleep_enable_timer_wakeup(minutos*60ULL*1000000ULL);
  esp_deep_sleep_start();
}

void setup(){
  Serial0.begin(115200);
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println(F("=== Sticky V54 BUENO ==="));
  Serial.printf("Reset %d\n", esp_reset_reason());
  stickyPowerLatch();
  pinMode(PIN_BTN_AI, INPUT_PULLUP);
  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  sensorWire.begin(PIN_SDA, PIN_SCL, 400000);
  delay(100);
  cargarConfig();
  if(bateriaCritica()){
    if(!rtc_avisoBateriaMostrado){
      if(display.begin()) pantallaBateriaBaja(nivelBateria);
      rtc_avisoBateriaMostrado = true;
    }
    dormir(INTERVALO_BATERIA_BAJA_MIN);
  }
  rtc_avisoBateriaMostrado = false;
  if(!display.begin()){ dormir(5); }
  bool abrirConfig=!cfg.valida;
  if(esp_sleep_get_wakeup_cause()==ESP_SLEEP_WAKEUP_EXT1){ uint64_t m=esp_sleep_get_ext1_wakeup_status(); if(m & (1ULL<<PIN_BTN_DOWN)) abrirConfig=true; }
  if(digitalRead(PIN_BTN_DOWN)==LOW) abrirConfig=true;
  if(abrirConfig){ modoConfiguracion(); if(!cfg.valida){ pantallaMensaje("Sin configurar","No se completo la configuracion"); dormir(60);} }
#if PRELIMPIAR
  display.fillScreen(BLANCO); display.refresh(); delay(500);
#endif
  esp_sleep_wakeup_cause_t causa = esp_sleep_get_wakeup_cause();
  bool wakePorBotonUp = false;
  bool wakePorBotonUpLargo = false;
  if(causa == ESP_SLEEP_WAKEUP_EXT1){
    uint64_t mask = esp_sleep_get_ext1_wakeup_status();
    if(mask & (1ULL<<PIN_BTN_UP)){
      unsigned long t0 = millis();
      while(digitalRead(PIN_BTN_UP)==LOW && millis()-t0 < 3000) delay(50);
      unsigned long dur = millis()-t0;
      if(dur >= TIEMPO_PULSACION_LARGA_MS){
        wakePorBotonUpLargo = true;
        wakePorBotonUp = true;
      } else {
        wakePorBotonUp = true;
      }
    }
  }
  bool necesitaDatos = true;
  if(datosValidosRTC && ultimoUpdateWeb != 0 && !wakePorBotonUpLargo){
    time_t tempAhora = time(nullptr);
    if(tempAhora > 1700000000){
      if((tempAhora - ultimoUpdateWeb) < (int64_t)(INTERVALO_DATOS_MIN*60)){
        necesitaDatos = false;
      }
    }
  }
  if(necesitaDatos){
    String ssidToUse=cfg.valida?cfg.ssid:String(""); String passToUse=cfg.valida?cfg.pass:String("");
    if(!conectarWifi(ssidToUse,passToUse)){ 
      pantallaMensaje("Sin WiFi","Reintento 3min"); 
      if(datosValidosRTC){
        codeAhora = rtc_codeAhora; tempAhora = rtc_tempAhora; sensacion = rtc_sensacion; humedad = rtc_humedad; viento = rtc_viento;
        for(int i=0;i<4;i++) dias[i]=rtc_dias[i];
      }
    } else {
      configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.google.com"); struct tm ti; for(int i=0;i<20 && !getLocalTime(&ti,500); i++){}
      if(!obtenerTiempo()){
        pantallaMensaje("Sin datos","Usando cache");
        if(datosValidosRTC){
          codeAhora = rtc_codeAhora; tempAhora = rtc_tempAhora; sensacion = rtc_sensacion; humedad = rtc_humedad; viento = rtc_viento;
          for(int i=0;i<4;i++) dias[i]=rtc_dias[i];
        }
      } else {
        rtc_codeAhora = codeAhora; rtc_tempAhora = tempAhora; rtc_sensacion = sensacion; rtc_humedad = humedad; rtc_viento = viento;
        for(int i=0;i<4;i++) rtc_dias[i]=dias[i];
        ultimoUpdateWeb = time(nullptr);
        datosValidosRTC = true;
      }
    }
  } else {
    codeAhora = rtc_codeAhora; tempAhora = rtc_tempAhora; sensacion = rtc_sensacion; humedad = rtc_humedad; viento = rtc_viento;
    for(int i=0;i<4;i++) dias[i]=rtc_dias[i];
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.google.com"); struct tm ti; getLocalTime(&ti, 800);
  }
  leerBateria(); 
  leerSHT40(); 
  iconoLunaVisible = false;
  dibujar();
  unsigned long tDespierta = millis();
  const unsigned long tiempoDespiertaMs = (unsigned long)TIEMPO_DESPTERTA_MIN * 60UL * 1000UL;
  Serial.printf("Despierto %lu ms - botones: UP=refresco DOWN=1.5s config\n", tiempoDespiertaMs);
  while(millis() - tDespierta < tiempoDespiertaMs){
    if(digitalRead(PIN_BTN_UP)==LOW){
      unsigned long tPress = millis();
      while(digitalRead(PIN_BTN_UP)==LOW && millis()-tPress < 3000){ delay(20); yield(); }
      unsigned long dur = millis()-tPress;
      Serial.printf("BTN_UP dur=%lu\n", dur);
      if(dur >= TIEMPO_PULSACION_LARGA_MS){
        String ssidToUse=cfg.valida?cfg.ssid:String(""); String passToUse=cfg.valida?cfg.pass:String("");
        pantallaMensaje("Actualizando...","Conectando WiFi");
        if(conectarWifi(ssidToUse,passToUse)){
          configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.google.com"); struct tm ti; getLocalTime(&ti, 800);
          if(obtenerTiempo()){
            rtc_codeAhora = codeAhora; rtc_tempAhora = tempAhora; rtc_sensacion = sensacion; rtc_humedad = humedad; rtc_viento = viento;
            for(int i=0;i<4;i++) rtc_dias[i]=dias[i];
            ultimoUpdateWeb = time(nullptr);
            datosValidosRTC = true;
          }
        }
        leerBateria(); leerSHT40(); dibujar();
        tDespierta = millis();
      } else if(dur > 50){
        leerBateria(); leerSHT40(); dibujar();
        tDespierta = millis();
      }
      delay(300);
    }
    if(digitalRead(PIN_BTN_DOWN)==LOW){
      unsigned long tPress = millis();
      while(digitalRead(PIN_BTN_DOWN)==LOW && millis()-tPress < 3000){ delay(20); yield(); }
      unsigned long dur = millis()-tPress;
      Serial.printf("BTN_DOWN dur=%lu\n", dur);
      if(dur >= TIEMPO_PULSACION_LARGA_MS){
        modoConfiguracion();
        cargarConfig();
        String ssidToUse=cfg.valida?cfg.ssid:String(""); String passToUse=cfg.valida?cfg.pass:String("");
        if(conectarWifi(ssidToUse,passToUse)){
          configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.google.com"); struct tm ti; getLocalTime(&ti, 800);
          obtenerTiempo();
        }
        leerBateria(); leerSHT40(); dibujar();
        tDespierta = millis();
      }
      delay(300);
    }
    if(digitalRead(PIN_BTN_AI)==LOW){
      delay(100);
      if(digitalRead(PIN_BTN_AI)==LOW){
        Serial.println("BTN_AI pulsado");
        leerBateria(); leerSHT40(); dibujar();
        tDespierta = millis();
        while(digitalRead(PIN_BTN_AI)==LOW) delay(20);
        delay(300);
      }
    }
    delay(100); yield();
  }
  Serial.println(F("Me duermo - luna"));
  iconoLunaVisible = true;
  dibujar();
  delay(600);
  dormir(INTERVALO_MIN);
}

void loop(){}
