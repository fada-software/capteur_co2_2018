//=============================================================================
// Projet : capteur CO2
// Date: 08/01/2026
// Author: fada-software
//=============================================================================
//librairies à installer avec arduino (menu sketch / include library / manage library)
#include <Adafruit_Sensor.h>      // V1.1.15 "adafruit unified sensor"
#include <Adafruit_BME680.h>      // V2.0.5, capteur environnement Bosch BME680
#include <MCUFRIEND_kbv.h>        // V3.0.0, librairie pour écran LCD
//librairies déjà installées avec arduino
#include <Wire.h>                 //I2C
#include <SoftwareSerial.h>       // pour port serie "logiciel" vers capteur CO2 MHZ19B
#include <Fonts/FreeSans9pt7b.h>  // inclu avec MCUFRIEND_kbv.h
#include <Fonts/FreeSans12pt7b.h> // inclu avec MCUFRIEND_kbv.h
#include <FreeDefaultFonts.h>     // inclu avec MCUFRIEND_kbv.h


//########## version 1 Fab ###############
#define ORIENTATION_TFT 3 //orientation 0,1,2,3 (3 = format paysage avec USB à droite / 1 à gauche)
#define OFFSET_TEMPERATURE -3 //-3°C pour afficher la température réelle, le capteur C02 chauffe juste à côté
//########## version 2 Mam, Aur ##########
//#define ORIENTATION_TFT 1
//#define OFFSET_TEMPERATURE -1

//#define CO2_DEBUG
//#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME680 bme; // capteur BME680, interface I2C
SoftwareSerial co2Serial(A15, 47); // define RX TX to MH-Z19B
MCUFRIEND_kbv tft; //ecran LCD TFT 480x320
#define MAX_TFT_WIDTH 480
#define MAX_TFT_HEIGHT 320
#define GRAPH_TOP_TFT_HEIGHT 65
#define TRACE_THICKNESS 5

//couleurs pour ecran LCD
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define GREY    0x8410
#define ORANGE  0xFA60
#define LIME    0x07FF
#define AQUA    0x04FF
#define C_PINK  0xF8FF //C_PINK parce que PINK est deja defini pour autre chose
#define PURPLE  0x8010

#define PRESSURE_COLOR GREEN
#define CO2_COLOR ORANGE
#define GRID_COLOR GREY

//=============================================================================
//Declaration des fonctions
//=============================================================================
void draw_display(void);
void draw_grid(void);
void update_display(void);
int convert_pressure_to_pixel(int pressure);
int convert_co2_to_pixel(int co2);
void update_table(int new_pres_value, int new_co2_value);
void display_tables_for_debug(void);
int readCO2UART(void);
byte getCheckSum(char *packet);

//=============================================================================
//Variales globales
//=============================================================================
const int Position_Y_pression=80, Position_Y_iaq=190, Position_Y_co2=300;
int ppm_uart = 0;
int i = 0;
int tab_co2[MAX_TFT_WIDTH];
int tab_pres[MAX_TFT_WIDTH];
int new_pixel_pressure = 0;
int new_pixel_co2 = 0;

//=============================================================================
// initialisation
//=============================================================================
void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println(F("BME680 async test"));

  co2Serial.begin(9600); //port serie vers capteur CO2 MHZ19B

  if (!bme.begin()) {
    Serial.println(F("Could not find a valid BME680 sensor, check wiring!"));
    while (1);
  }

  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  //bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  //bme.setGasHeater(320, 150); // 320*C for 150 ms

  uint16_t ID = tft.readID();
  if (ID == 0xD3) ID = 0x9481; //correction identifiant controlleur graphique, le module renvoie 0xD3 ? alors que c'est un ILI9486, fonctionne avec ID=0x9481
  tft.begin(ID);
  tft.setRotation(ORIENTATION_TFT);
  draw_display();
  draw_grid();

  ppm_uart = readCO2UART(); //1iere lecture capteur CO2 par UART, toujours erronée, renvoie -2, fait ici pour éviter le premier affichage avec le valeur -2 pendant 3 minutes

  //init tableaux
  for (i = 0 ; i < MAX_TFT_WIDTH ; i++)
  {
    tab_pres[i] = -TRACE_THICKNESS;//-1 permet de ne pas afficher de courbe
    tab_co2[i]  = -TRACE_THICKNESS;//-1 permet de ne pas afficher de courbe
  }
}

//=============================================================================
// boucle principale
//=============================================================================
void loop() {
  // Tell BME680 to begin measurement.
  unsigned long endTime = bme.beginReading();
  if (endTime == 0) {
    Serial.println(F("Failed to begin reading :("));
    return;
  }
  delay(100); // wait during measure
  if (!bme.endReading()) {
    Serial.println(F("Failed to complete reading :("));
    return;
  }

  Serial.print(F("Temperature = "));
  Serial.print(bme.temperature + OFFSET_TEMPERATURE);
  Serial.println(F(" *C"));

  Serial.print(F("Pressure = "));
  Serial.print(bme.pressure / 100.0);
  Serial.println(F(" hPa"));

  Serial.print(F("Humidity = "));
  Serial.print(bme.humidity);
  Serial.println(F(" %"));

  // Serial.print(F("Gas = "));
  // Serial.print(bme.gas_resistance / 1000.0);
  // Serial.println(F(" KOhms"));

  // Serial.print(F("Approx. Altitude = "));
  // Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
  // Serial.println(F(" m"));
  
  ppm_uart = readCO2UART(); //lecture capteur CO2 par UART
  
  update_display(); //mise à jour affichage LCD TFT

  Serial.println();
  //delay(3000); //3 secondes, pour debug
  delay(180000); //3 minutes pour chaque nouveau point de mesure, 24h = 480 pixels
}

//=============================================================================
//affichage initial de l'ecran LCD, fait une seule fois dans setup(),
//ensuite on met a jour uniqument les valeurs et les fleches avec update_display()
//=============================================================================
void draw_display(void) {
  tft.fillScreen(BLACK);
  tft.setCursor(0, 0);
  tft.setTextSize(1);
  tft.setTextColor(WHITE);
  tft.setFont(&FreeSans9pt7b);

  //affichage des textes avec des valeurs initiales à 8, par la suite seules les valeurs 8 sont re-ecrites
  tft.println("");
  tft.println("    Temperature : 88.8 *C       Humidite relative : 88 %");
  tft.setTextColor(PRESSURE_COLOR);
  tft.print("    Pression : 8888 hPa          ");
  tft.setTextColor(CO2_COLOR);
  tft.println("CO2 (0~1500) : 8888 ppm");

  tft.fillRect(0, GRAPH_TOP_TFT_HEIGHT, 1, MAX_TFT_HEIGHT-GRAPH_TOP_TFT_HEIGHT, GRID_COLOR); // trait blanc gauche
  tft.fillRect(MAX_TFT_WIDTH-1, GRAPH_TOP_TFT_HEIGHT, 1, MAX_TFT_HEIGHT-GRAPH_TOP_TFT_HEIGHT, GRID_COLOR); // trait blanc droit
}
//=============================================================================
// affichage de la grille + traits haut et bas qu ipeuvent etre effacés par la courbe
//=============================================================================
void draw_grid(void) {
  tft.fillRect(0, GRAPH_TOP_TFT_HEIGHT, MAX_TFT_WIDTH-1, 1, GRID_COLOR); // trait blanc haut
  tft.fillRect(0, MAX_TFT_HEIGHT-1, MAX_TFT_WIDTH-1, 1, GRID_COLOR); // trait blanc bas
  tft.fillRect(0, convert_pressure_to_pixel(980), MAX_TFT_WIDTH, 1, GRID_COLOR);
  tft.fillRect(0, convert_pressure_to_pixel(1000), MAX_TFT_WIDTH, 1, GRID_COLOR);
  tft.fillRect(0, convert_pressure_to_pixel(1020), MAX_TFT_WIDTH, 1, GRID_COLOR);
  tft.fillRect(0, convert_pressure_to_pixel(1040), MAX_TFT_WIDTH, 1, GRID_COLOR);
  tft.fillRect(MAX_TFT_WIDTH/4, GRAPH_TOP_TFT_HEIGHT, 1, MAX_TFT_HEIGHT-GRAPH_TOP_TFT_HEIGHT, GRID_COLOR);
  tft.fillRect(MAX_TFT_WIDTH/2, GRAPH_TOP_TFT_HEIGHT, 1, MAX_TFT_HEIGHT-GRAPH_TOP_TFT_HEIGHT, GRID_COLOR);
  tft.fillRect(MAX_TFT_WIDTH*3/4, GRAPH_TOP_TFT_HEIGHT, 1, MAX_TFT_HEIGHT-GRAPH_TOP_TFT_HEIGHT, GRID_COLOR);
}
//=============================================================================
// mise à jour des valeurs et des courbes
//=============================================================================
void update_display(void) {
  //effacement aciennes valeurs en dessinant un rectangle noir, puis affichage nouvelles valeurs au meme endroit
  tft.fillRect(138, 15, 36, 15, BLACK); // coordonnées X,Y, rectangle de 36x15px
  tft.setTextColor(WHITE);
  tft.setCursor(138, 28);
  tft.print(bme.temperature-3, 1); //compensation -3°C pour afficher la température réelle, le capteur C02 chauffe juste à côté
  
  tft.fillRect(380, 15, 22, 15, BLACK); // coordonnées X,Y, rectangle de 22x15px
  tft.setCursor(380, 28);
  tft.print(bme.humidity, 0);
  
  tft.fillRect(105, 37, 42, 15, BLACK); // coordonnées X,Y, rectangle de 42x15px
  tft.setTextColor(PRESSURE_COLOR);
  tft.setCursor(105, 50);
  tft.print(bme.pressure / 100.0, 0);

  tft.fillRect(360, 37, 42, 15, BLACK); // coordonnées X,Y, rectangle de 42x15px
  tft.setTextColor(CO2_COLOR);
  if (ppm_uart < 1000) tft.setCursor(369, 50); //si 3 chiffres, écriture plus proche de "ppm"
  else tft.setCursor(360, 50); //sinon 4 chiffres
  tft.print(ppm_uart);

  //carré de couleur vert/orange/rouge selon taux de CO2
  if (ppm_uart < 700 ) tft.fillRect(450, 35 , 20, 20, GREEN);
  else if (ppm_uart < 1000) tft.fillRect(450, 35 , 20, 20, ORANGE);
  else tft.fillRect(450, 35 , 20, 20, RED);

  //ajout nouveau points de mesures aux tableaux
  new_pixel_pressure = convert_pressure_to_pixel(int(bme.pressure / 100.0));
  new_pixel_co2 = convert_co2_to_pixel(ppm_uart);
  update_table(new_pixel_pressure, new_pixel_co2); //rotation tableaux
  
  //décalage de la courbe de 1 pixel vers la gauche
  for (i = 1 ; i < MAX_TFT_WIDTH-1 ; i++) 
  // { // points sans traits verticaux pour relier les points (vide si saut important)
  //   tft.fillRect(i, tab_pres[i-1]  , 1, TRACE_THICKNESS, BLACK);
  //   tft.fillRect(i, tab_pres[i], 1, TRACE_THICKNESS, PRESSURE_COLOR);
  //   tft.fillRect(i, tab_co2[i-1]   , 1, TRACE_THICKNESS, BLACK);
  //   tft.fillRect(i, tab_co2[i] , 1, TRACE_THICKNESS, CO2_COLOR);
  // }
  // { // points avec traits verticaux pour relier les points
    // if (i > 1 && tab_pres[i-2] > 0) tft.fillRect(i, tab_pres[i-1], 1, TRACE_THICKNESS + tab_pres[i-2] - tab_pres[i-1], BLACK); //trait vertical en cas de saut important, seulement si donnée valide > 0 (init valeur négative)
    // if (i > 1 && tab_pres[i-2] > 0) tft.fillRect(i, tab_pres[i], 1, TRACE_THICKNESS + tab_pres[i-1] - tab_pres[i], PRESSURE_COLOR); //trait vertical en cas de saut important, seulement si donnée valide > 0 (init valeur négative)
    // if (i > 1 && tab_co2[i-2] > 0) tft.fillRect(i, tab_co2[i-1], 1, TRACE_THICKNESS + tab_co2[i-2] - tab_co2[i-1], BLACK); //trait vertical en cas de saut important, seulement si donnée valide > 0 (init valeur négative)
    // if (i > 1 && tab_co2[i-2] > 0) tft.fillRect(i, tab_co2[i], 1, TRACE_THICKNESS + tab_co2[i-1] - tab_co2[i], CO2_COLOR); //trait vertical en cas de saut important, seulement si donnée valide > 0 (init valeur négative)
  // }
  { // points avec traits verticaux pour relier les points, avec différenciation courbe qui monte ou qui descend, pour bien relier les points
    if (i > 1 && tab_pres[i-2] > 0) tft.fillRect(i, tab_pres[i-1], 1, TRACE_THICKNESS + tab_pres[i-2] - tab_pres[i-1], BLACK); //trait vertical en cas de saut important, seulement si donnée valide > 0 (init valeur négative)
    if (i > 1 && tab_pres[i-2] > 0) tft.fillRect(i, tab_pres[i], 1, TRACE_THICKNESS + tab_pres[i-1] - tab_pres[i], PRESSURE_COLOR); //trait vertical en cas de saut important, seulement si donnée valide > 0 (init valeur négative)
    if (i > 1 && tab_co2[i-2] > 0) 
		if (tab_co2[i-2] - tab_co2[i-1] >= 0) //courbe qui monte
			tft.fillRect(i, tab_co2[i-1], 1, TRACE_THICKNESS + tab_co2[i-2] - tab_co2[i-1], BLACK); //trait vertical en cas de saut important, seulement si donnée valide > 0 (init valeur négative)
		else //courbe qui descend
			tft.fillRect(i, tab_co2[i-1] + TRACE_THICKNESS, 1, tab_co2[i-2] - tab_co2[i-1] - TRACE_THICKNESS, BLACK); //trait vertical en cas de saut important, seulement si donnée valide > 0 (init valeur négative)
    if (i > 1 && tab_co2[i-2] > 0)
		if (tab_co2[i-1] - tab_co2[i] >= 0) //courbe qui monte
			tft.fillRect(i, tab_co2[i], 1, TRACE_THICKNESS + tab_co2[i-1] - tab_co2[i], CO2_COLOR); //trait vertical en cas de saut important, seulement si donnée valide > 0 (init valeur négative)
		else //courbe qui descend
			tft.fillRect(i, tab_co2[i] + TRACE_THICKNESS, 1, tab_co2[i-1] - tab_co2[i] - TRACE_THICKNESS, CO2_COLOR); //trait vertical en cas de saut important, seulement si donnée valide > 0 (init valeur négative)
  }
  draw_grid(); //affichage de la grille par dessus les courbes à chaque nouveau point, pour ne pas effacer la grille avec le décalage de 1 pixel vers la gauche
}
//=============================================================================
//conversion pression 960 à 1060 en pixel 319-TRACE_THICKNESS à 66 (voir fichier tableur.ods pour calcul y=a.x+b)
//=============================================================================
int convert_pressure_to_pixel(int pressure){
  return int(-2.48 * pressure + 2694.8);
}
//=============================================================================
//conversion co2 400 à 1500 en pixel 319-TRACE_THICKNESS à 66 (voir fichier tableur.ods pour calcul y=a.x+b)
//=============================================================================
int convert_co2_to_pixel(int co2){
  if (co2 < 400 ) co2 = 400;
  else if (co2 > 1500) co2 = 1500;
  return int(-0.2255 * co2 + 404.2);
}
//=============================================================================
// rotation des tableaux de valeurs de CO2 et pression
//=============================================================================
void update_table(int new_pres_value, int new_co2_value){
  for (i = 1 ; i < MAX_TFT_WIDTH-2 ; i++)
  {
    tab_pres[i]=tab_pres[i+1];
    tab_co2[i]=tab_co2[i+1];
  }
  tab_pres[MAX_TFT_WIDTH-2]=new_pres_value;
  tab_co2[MAX_TFT_WIDTH-2]=new_co2_value;
}
//=============================================================================
// affichage valeurs pour debug
//=============================================================================
void display_tables_for_debug(void){
  for (i = 1 ; i < MAX_TFT_WIDTH-2 ; i++)
  {
    Serial.print("pres ");
    Serial.print(i);
    Serial.print(" : ");
    Serial.println(tab_pres[i]);
    Serial.print("co2 ");
    Serial.print(i);
    Serial.print(" : ");
    Serial.println(tab_co2[i]);
  }
}
//=============================================================================
// lecture valeur CO2 par UART, capteur MH-Z19B
//=============================================================================
int readCO2UART(void){
  byte cmd[9] = {0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79};
  byte response[9]; // for answer
  memset(response, 0, 9); // clear the buffer
  
  //Serial.println("Sending CO2 request...");
  co2Serial.write(cmd, 9); //request PPM CO2

  // Configuration d'un time out max d'attente de réponse de 5 secondes (à 10 * 0.5secondes)
  byte TimeOutAttente = 10;
  // TANT QUE aucun caractère n'est reçu depuis la liaison série avec le capteur
  // et timeout non échu
  while( ( co2Serial.available() == 0 ) && (TimeOutAttente > 0) )
  {
    // Attente 500 ms
    delay(500);
    TimeOutAttente--;
  }
  // SI time out de réception
  if (TimeOutAttente == 0) 
  {
    Serial.println("Capteur CO2, Pas de réponse");
    return -1;
  }
  else
  { // SINON, caractère(s) reçu(s)
    // Configure un timeout de 5s max en cas de réception partielle de la réponse
    Serial.setTimeout(5000);
    // Réception de 9 caractères ou timeout
    co2Serial.readBytes(response, 9);
    
    if (response[1] != 0x86)
    {
      Serial.println("Capteur CO2, reponse invalide");
	    #ifdef CO2_DEBUG
        Serial.println(response[0]);
        Serial.println(response[1]);
        Serial.println(response[2]);
        Serial.println(response[3]);
        Serial.println(response[4]);
        Serial.println(response[5]);
        Serial.println(response[6]);
        Serial.println(response[7]);
        Serial.println(response[8]);
      #endif
      return -2;
    }

    // test checksum
    byte check = getCheckSum(response);
    if (response[8] != check) {
      Serial.println("Capteur CO2, Checksum KO");
	    #ifdef CO2_DEBUG
        Serial.println(response[8]);
        Serial.println(check);
      #endif
      return -3;
    }
    else //cheksum OK
    {
      // ppm
      int ppm = 256 * (int)response[2] + response[3];
      // temp
      byte temp = response[4] - 40;
      
      #ifdef CO2_DEBUG
        Serial.print("PPM UART: ");
        Serial.println(ppm);
        Serial.print("Temperature: ");
        Serial.println(temp);
      #endif
  
      return ppm;
    }
  }
}
//=============================================================================
// calcul checksum trame UART, capteur MH-Z19B
//=============================================================================
byte getCheckSum(byte *packet)
{
  byte i;
  unsigned char checksum = 0;
  for (i = 1; i < 8; i++) {
    checksum += packet[i];
  }
  checksum = 0xff - checksum;
  checksum += 1;
  return checksum;
}
