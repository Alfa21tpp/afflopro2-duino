/*
 * AFFLOPRO.2-duino
 */

/* TODO:
- verifica fine partita (conto quante caselle as o pn ci sono sul campo)
- memorizzare ultimo pixel a segno per cercare nave attorno
- creare un menu per scelta se gioco contro computer o contro remoto
- separare il gioco dalla base di sincronia e metterlo in un file separato
- tic tac toe?
- messaggi brevi (delta) piuttosto che rimandare sempre tutto?
- display bFA a termine turno
- reset radio reader dopo attesa prolungata
- ripristino gioco umano-vs-cpu su singolo device
- mostrare x/12 in schermata mira umano
- torna a capo nella seriale stampando la linea di bF
*/

/* Connections:

 Arduino -> LT8900

 GND        GND
 3v3        VCC
 8          PKT
 9          CS
 10         RST
 11         MOSI
 12         MISO
 13         SCK

 Connect PIN_ROLE to ground for sender, leave open for receiver role.
*/

//==LIBRERIE==================================================================

//radio link
#include <SPI.h>
#include "src/LT8900.h"

//display oled
#include <Arduino.h>
#include <U8x8lib.h>
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

//EEPROM
#include <EEPROM.h>


//==DEFINIZIONI===============================================================
const uint8_t PIN_NRF_RST = 10;
const uint8_t PIN_NRF_CS = 9;
const uint8_t PIN_NRF_PKT = 8;

bool writer;
#define PIN_ROLE A3
#define PIN_RANDOM A0  //floating pin per generatore di numeri casuali

//int x;
String str;

/*
const char prmP[] PROGMEM = "\nPortaerei:\n";
const char prmI[] PROGMEM = "\nIncrociatore:\n";
const char prmM[] PROGMEM = "\nMotovedetta:\n";
*/


//==
#define DEBUG         //DECOMMENTARE PER STAMPE DEBUG
#define DEBUG_avv     //DECOMMENTARE PER STAMPE DEBUG avverso
//OGNI STAMPA DI DEBUG È NUMERATA. ULTIMO NUMERO UTILIZZATO = B6

#define NUMPIXELS 64

#define PROPRIO 0     //flag per turno
#define AVVERSO 1     //flag per turno
#define NORMALE 2     //flag per turno
#define CACCIA 3      //flag per turno
#define COLPITO 4     //flag per turno

#define ORIZZONTALE 0 //flag per posizionamento
#define VERTICALE 1   //flag per posizionamento
#define EFFETTUATO 2  //flag per posizionamento
#define ERRATO 3      //flag per posizionamento
#define INVERTE 4     //flag per posizionamento
#define PIAZZA 5      //flag per posizionamento

#define NONOK false
#define OK true

#define SU 6     //pulsante spostamento su
#define GIU 5    //pulsante spostamento giu
#define SIN 4    //pulsante spostamento a sinistra
#define DST 3    //pulsante spostamento a destra
#define GO 2     //pulsante multiuso

//definizione colori pixel piazzamento navi
#define pn 35    //pixel con nave
//definizione colori pixel colpo a vuoto
#define av 20    //pixel a vuoto
//definizione colori pixel colpo a segno
#define as 42    //pixel a segno
//definizione colori pixel sfondo
#define pm 126    //pixel mare
//definizione colori pixel cursore

//#define battleFieldMia bFM
//#define battleFieldAltro bFA

//definizione celle per EEPROM
#define totBattMsb 0
#define totBattLsb 1
#define totVinIoMsb 2
#define totVinIoLsb 3
#define totColpIoMsb 4
#define totColpIoLsb 5
#define recColpIo 6
#define totVinTuMsb 7
#define totVinTuLsb 8
#define totColpTuMsb 9
#define totColpTuLsb 10
#define recColpTu 11

//==OGGETTI===================================================================
LT8900 lt(PIN_NRF_CS, PIN_NRF_PKT, PIN_NRF_RST);

U8X8_SSD1306_128X64_NONAME_HW_I2C lcd(/* reset=*/ U8X8_PIN_NONE);
//U8G2_SH1106_128X64_NONAME_1_HW_I2C lcd(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);   // All Boards without Reset of the Display

//==VARIABILI GLOBALI=========================================================
uint8_t num02, num05, num06, num07, num08;  //usati per random()
uint8_t fine;
uint8_t currLed;
uint8_t estratto;

int memNavi[12];             //posizionamento navi avversario
int posiz[4];                //posizionamento temporaneo pixel navi
int posizionamento;
bool orientamento;  //orienatmento nave (ORIZZONTALE / VERTICALE)
int toggleGO;
int *pNave;
int nave[4];
int lNave;
int currLim;
int ind;
int adia[4];      //matrice adiacenze
int aSegno[12];   //matrice colpi a segno AVVERSO
int turno;        //controllo alternanza colpi
int regime;       //controllo colpi avverso
int lista;        //switch fra liste colpi efficaci
int minLim;       //limite minimo nave
int maxLim;       //limite massimo nave
int minHit;       //pixel da colpire poppanave
int maxHit;       //pixel da colpire pruanave

bool synced = false;
#define MSG_SIZE 10
byte input[1+MSG_SIZE], output[MSG_SIZE]; //input MUST be 1byte bigger than incoming data

byte bFM[64];   //battleFieldMia   = display campo di battaglia proprio
byte bFA[64];   //battleFieldAltro = display campo di battaglia avversario

//riserva colpi a griglie sfalsate
uint8_t cella[2][16] = {{0, 2, 4, 6, 16,
                     18,20,22,32,34,
                     36,38,48,50,52,
                     54},{ 9,11,13,15,
                     25,27,29,31,41,
                     43,45,47,57,59,
                     61,63}};

//pre modifica bFM e bFA
//Lo sketch usa 28074 byte (91%) dello spazio disponibile per i programmi. Il massimo è 30720 byte.
//Le variabili globali usano 1299 byte (63%) di memoria dinamica, lasciando altri 749 byte liberi per le variabili locali. Il massimo è 2048 byte.

//post modifica
//Lo sketch usa 27234 byte (88%) dello spazio disponibile per i programmi. Il massimo è 30720 byte.
//Le variabili globali usano 915 byte (44%) di memoria dinamica, lasciando altri 1133 byte liberi per le variabili locali. Il massimo è 2048 byte.

// dimensioni programma al 20220513:
// Sketch uses 25174 bytes (78%) of program storage space. Maximum is 32256 bytes.
// Global variables use 1408 bytes (68%) of dynamic memory, leaving 640 bytes for local variables. Maximum is 2048 bytes.

//============================================================================

//==INIZIO SETUP==============================================================
void setup()
{
  // Setup serial monitor
  Serial.begin(9600);
//  Serial.println(F("Hello! :) I'm afflopro2-duino game"));

  lcd.begin();
//  lcd.setFont(u8x8_font_amstrad_cpc_extended_f);
//  lcd.setFont(u8x8_font_5x8_f);
  lcd.setFont(u8x8_font_5x8_r);

  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE1);
  SPI.setClockDivider(SPI_CLOCK_DIV4);

  //Inizializza pin
  pinMode(GO, INPUT_PULLUP);
  pinMode(DST, INPUT_PULLUP);
  pinMode(SU, INPUT_PULLUP);
  pinMode(GIU, INPUT_PULLUP);
  pinMode(SIN, INPUT_PULLUP);
  pinMode(PIN_ROLE, INPUT_PULLUP);

//  delay(500);

  lt.begin();
  lt.setCurrentControl(4,15);
  lt.setDataRate(LT8900::LT8910_62KBPS);
  lt.setChannel(0x06);

/* verifica radio
  char sbuf[32];

  //verify chip registers.
  for (int i = 0; i <= 50; i++)
  {
    uint16_t value = lt.readRegister(i);

    sprintf_P(sbuf, PSTR("%d = %04x\r\n"), i, value);
    Serial.print(sbuf);
  }
*/

  if ((digitalRead(PIN_ROLE) == LOW))
  {
      writer = true;
    Serial.println(F("Writer mode"));
  }
  else
  {
      writer = false;
    Serial.println(F("Reader mode"));
    lt.startListening();

    lcd.setCursor(0,0);
    lcd.print(F("Reader mode"));
    delay(3000);
  }

  //Predisposizione campo di battaglia proprio
  for (int i = 0; i < 64; i++) {
    bFM[i] = pm;
  }

/*
    bFM[0] = as;
    bFM[1] = as;
    bFM[8] = as;

    bFM[6] = as;
    bFM[7] = as;
    bFM[15] = as;

    bFM[56] = as;
    bFM[57] = as;
    bFM[48] = as;

    bFM[63] = as;
    bFM[62] = as;
    bFM[55] = as;
*/

  //Predisposizione campo di battaglia avverso
  for (int i = 0; i < 64; i++) {
    bFA[i] = pm;
  }
  //Azzeramento matrice memNavi[]
  for (int i = 0; i < 12; i++) {
    memNavi[i] = 0;
  }
  //Inizializza variabili
//  nNavi = 0;
  ind = 0;
  fine = 16;
  lista = 0;

  regime = NORMALE;
  currLed = 35;   //posiziona cursore al centro del campo di battaglia
  randomSeed(analogRead(PIN_RANDOM));   //attiva generatore di numeri casuali

  lt.whatsUp(Serial);
  Serial.println(F("Boot completed."));

}   //==FINE SETUP============================================================

//==INIZIO LOOP===============================================================
void loop()
{
//==COMMENTARE PER DEBUG POSIZIONAMENTO NAVI AVVERSO
//per saltare il posizionamento navi proprie
//    posiziona_navi_umano();

/*
 * bF:
 * 1 = bFM
 * 0 = bFA
 */
    posiziona_navi_computer(1);

  lcd.clear();
  lcd.print(F("    INIZIO      "));
  lcd.setCursor(0,2);
  lcd.print(F("   BATTAGLIA    "));
//  mp3.playFileByIndexNumber(GONG);
  delay(2000);

#ifdef DEBUG
Serial.println(F("15-Mostro campo AVVERSO"));
/*
//Mostra campo di battaglia avversario con posizionamento navi
  lcd.clear();
  lcd.print(F("- DEBUG: bFA  -"));
delay(1000);

for (int i=0; i<64; i++)
{
  int c = i % 8;
  int r = i / 8;
  lcd.setCursor(c,r);

  if (bFA[i] == pn)
  {
    lcd.print(F("x"));
  }else
  {
    lcd.print(F("."));
  }

//  bFM[i] = bFA[i];
}
*/
//WS.show();
draw_bf(1);
delay(3000);
draw_bf(0);
delay(3000);
#endif

//==QUI INIZIA IL COMBATTIMENTO VERO E PROPRIO================================

  currLed = 35;    //reset posizione iniziale del cursore al centro del campo


  #ifdef DEBUG
  Serial.println(F("16-Se DEBUG turno iniziale = 0"));
  turno = 0;
  #endif

  for (;;)
  {
    if (writer)
    {
      Serial.print("inizio loop, turno locale = ");
      Serial.println(turno, DEC);

      sync_check();
//      delay(5000);

      if (synced)
      {

        if (turno == 0)
        {
          //se turno 0 ed ho copiato reciprocamente bFM e bFA, scelgo casualmente se partire da turno 1 o 2
          //Decisione su chi spara per primo
          turno = random(1, 3); //turno is 1 or 2
          lcd.clear();
          lcd.print(F("    SORTEGGIO   "));
          lcd.setCursor(0,2);
          lcd.print(F("  CHI COMINCIA  "));
        }

        for (; ((turno%2)^(writer)); )
        {
          //TODO
          /*
           * se su bFA ci sono 12 "as", hai vinto
           * se su bFM ci sono 12 "as", hai perso
           */
          if (!game_verifica_fine())
          {
            Serial.println("eseguo turno di gioco");
            //game_main();
  //          game_play_umano();
            game_play_computer(1);
    //        game_fake();
          }
        }
        Serial.println("fine turno di gioco");
      }else
      {
        send_bf(1); // 1 = bFM
//        draw_bf(1); // 1 = bFM
//        delay(3000);

        send_bf(0); // altro = bFA
//        draw_bf(0);
//        delay(3000);
      }
    }else
    {
    //  do_test_ack();
      game_reader();
    }
  }
}

bool game_verifica_fine()
{
  bool game_over = false;
  uint8_t colpiPropriAsegno = 0;
  uint8_t colpiAvversoAsegno = 0;
  uint8_t colpiPropri = 0;
  uint8_t colpiAvverso = 0;

  //conto i colpi a segno
  for (int i = 0; i < 64; i++)
  {
    if (bFA[i] == as)
    {
      colpiPropriAsegno++;
    }
    if (bFA[i] == av || bFA[i] == as)
    {
      colpiPropri++;
    }

    if (bFM[i] == as)
    {
      colpiAvversoAsegno++;
    }
    if (bFM[i] == av || bFM[i] == as)
    {
      colpiAvverso++;
    }
  }

  //vittoria del giocatore
  if (colpiPropriAsegno >= 12)
  {
    game_over = true;
    lcd.clear();
    lcd.print(F("GIOCO FINITO"));
    delay(1000);
    lcd.clear();
    lcd.print(F("HAI VINTO TU"));
  }

  //vittoria dell'avversario
  if (colpiAvversoAsegno >= 12)
  {
    game_over = true;
    lcd.clear();
    lcd.print(F("GIOCO FINITO"));
    delay(1000);
    lcd.clear();
    lcd.print(F("HAI PERSO"));
  }

  if (game_over)
  {
    lcd.setCursor(0,2);
    lcd.print(F("spari tuoi = "));
    lcd.print(colpiPropri);
    lcd.setCursor(0,3);
    lcd.print(F("buoni = "));
    lcd.print(colpiPropriAsegno);
    lcd.print(F("/12"));
    lcd.setCursor(0,5);
    lcd.print(F("spari avv. = "));
    lcd.print(colpiAvverso);
    lcd.setCursor(0,6);
    lcd.print(F("buoni = "));
    lcd.print(colpiAvversoAsegno);
    lcd.print(F("/12"));
    delay(5000);

    draw_bf(1); // 1 = bFM
    delay(5000);
    draw_bf(2); // 2 = bFA scoperto
    delay(5000);
  }


Serial.print(F("colpiPropriAsegno = "));
Serial.println(colpiPropriAsegno);
Serial.print(F("colpiAvversoAsegno = "));
Serial.println(colpiAvversoAsegno);
Serial.print(F("game_over = "));
Serial.println(game_over);

    return game_over;
}

void game_play_computer(byte bF)
{
#ifdef DEBUG_avv
Serial.println(F("22-Controllo colpo AVVERSO"));
#endif

        draw_bf(1); // 1 = bFM
        delay(3000);
        draw_bf(0);
        delay(3000);

/*
 * bF:
 * 1 = giocatore su bFM, attacca su bFA
 * 0 = giocatore su bFA, attacca su bFM
 */
  byte* bFX;
  if ( bF == 1 )
  {
    bFX = bFA;
  }else
  {
    bFX = bFM;
  }

        lcd.clear();
        lcd.print(F("TOCCA A ME"));
/*
        lcd.setCursor(0,1);
        //Disegno campo di battaglia proprio
        for (int i = 0; i < 64; i++) {
//          WS.setPixelColor(i, WS.Color(bFM[i][1], bFM[i][2], bFM[i][3]));
        }
//        WS.show();
        delay(1000);
*/

//==Caso AVVERSO regime NORMALE==============================================
        if (regime == NORMALE) {
#ifdef DEBUG_avv
Serial.println(F("23-Regime NORMALE"));
#endif
          lcd.setCursor(0,1);
          lcd.print(F("scelgo il colpo"));
          delay(500);
          //Estrazione coordinate colpo da celle[]
          EstraiColpo(lista);
#ifdef DEBUG_avv
Serial.print(F("24-Estratto pixel n. "));
Serial.print(estratto);
Serial.print(F(" da lista "));
Serial.print(lista);
Serial.print(F(" fine="));
Serial.println(fine);
#endif
          if (lista == 2) {
            //Fine colpi a disposizione
#ifdef DEBUG_avv
Serial.print(F("B06-FINE COLPI A DISPOSIZIONE"));
Serial.println(F("    PROGRAMMA BLOCCATO"));
#endif
//===========================================================================
//               lcd.clear();
//               lcd.print(F("PROGRAMMA BLOCCATO"));
//               delay(3000);
//               draw_bf(1); // 1 = bFM
//               delay(3000);
              draw_bf(0);
//              delay(3000);
            while (true) ;
//===========================================================================
          }
          //Colpo su pixel già a vuoto o già colpito
          //PUÒ CAPITARE PER I COLPI SULLE ADIACENZE CHE SONO FUORI lista[]
          if (bFX[estratto] == av || bFX[estratto] == as) {
#ifdef DEBUG_avv
Serial.print(F("25-Pixel n. "));
Serial.print(estratto);
Serial.println(F(" non sparabile (av o as)"));
#endif
            //predisponi altro colpo
            regime = NORMALE;
//            turno = AVVERSO;
//             break;    //USCITA da case AVVERSO regime NORMALE
            return;
          }
//Qui il pixel è sparabile, ma occorrerebbe controllare che non abbia intorno
//(DX, SX, UP, DWN) pixel già sparati (av) o colpiti (as). però se le navi
//sono affiancate potrebbe capitare che un pixel fosse rimasto isolato
//==DA VERIFICARE ED EVENTUALMENTE MODIFICARE
          if (ControllaPixelAdiacenti(estratto, bFX) == NONOK) {
#ifdef DEBUG_avv
Serial.print(F("26-Pixel n. "));
Serial.print(estratto);
Serial.println(F(" attorniato da colpi già sparati (as) o a vuoto (av)"));
#endif
            //pixel attorniato da colpi già sparati (as) o a vuoto (av)
            //predisponi altro colpo
            regime = NORMALE;
//            turno = AVVERSO;
//             break;    //USCITA da case AVVERSO regime NORMALE
            return;
          }
          //Il pixel estratto ha passato tutti i controlli ed è sparabile
//          lcd.setCursor(15,1);
//          lcd.print(F("     "));
          lcd.setCursor(0,2);
          lcd.print(F("FATTO!"));
//          WS.setPixelColor(estratto, WS.Color(cuR, cuG, cuB));
//          WS.show();
          delay(500);
          lcd.setCursor(0,3);
          lcd.print(F("-SPARO-"));
#ifdef DEBUG_avv
Serial.print(F("27-Sparato colpo"));
Serial.print(F(" in casella "));
Serial.println(estratto);
#endif

//==Case AVVERSO regime NORMALE colpo a vuoto=================================
          if (bFX[estratto] == pm) {
#ifdef DEBUG_avv
Serial.print(F("28-Colpo AVVERSO a vuoto"));
#endif
            lcd.setCursor(0,3);
            lcd.print(F("colpo a vuoto"));
//            mp3.playFileByIndexNumber(SPLASH);
            delay(500);
            //cambia la condizione dei pixel da fp ad av
            bFX[estratto] = av;
            //bFM[estratto][1] = avR; bFM[estratto][2] = avG; bFM[estratto][3] = avB;
            regime = NORMALE;
//            turno = PROPRIO;
            turno++;
//             break;    //USCITA da case AVVERSO regime NORMALE colpo a vuoto
            return;
          }

//==Case AVVERSO regime NORMALE colpo a segno=================================
          //è il primo colpo a segno
          else if (bFX[estratto] == pn) {
#ifdef DEBUG_avv
Serial.print(F("29-Colpo AVVERSO a segno"));
#endif
            //memorizza estratto in aSegno[ind] con ind=0
            aSegno[ind] = estratto;
#ifdef DEBUG_avv
Serial.print(F("30-Memorizzato colpo a segno "));
Serial.print(estratto);
Serial.print(F(" in aSegno["));
Serial.print(ind);
Serial.println(F("]"));
#endif
            ind = ind + 1;
            lcd.setCursor(0,3);
            lcd.print(F("colpo a segno!"));
            //cambia la condizione dei pixel da pn ad as
            bFX[estratto] = as;
//            bFM[estratto][1] = asR; bFM[estratto][2] = asG; bFM[estratto][3] = asB;
//            mp3.playFileByIndexNumber(BOOM);
            //Entra in caccia alla nave
#ifdef DEBUG_avv
Serial.print(F("31-Entro in CACCIA alla nave da pixel n. "));
Serial.println(estratto);
#endif
            lcd.clear();
            lcd.print(F("TOCCA A ME"));
            lcd.setCursor(0,1);
            lcd.print(F("check adiacenze"));
            delay(500);
            //Calcolo delle adiacenze (max 4) del colpo a segno
            CalcolaAdiacenze(estratto, bFX);
//SE IL CONTROLLO DEL COLPO SPARABILE COMPRENDEVA ANCHE IL CONTROLLO CHE NON
//SI TRATTASSE DI UN COLPO ISOLATO, LA FUNZIONE RITORNA ALMENO 1 ADIACENZA.
//SE INVECE IL CONTROLLO NON È STATO FATTO LA FUNZIONE PUÒ RITORNARE ANCHE
//TUTTE LE ADIACENZE A ZERO
#ifdef DEBUG_avv
Serial.print(F("32-Adiacenze calcolate "));
for (int i=0; i<4; i++) {
  if (adia[i] != 0) {
    Serial.print(F(" "));
    Serial.print(adia[i]);
  }
}
Serial.println(F(" *"));
#endif
            lcd.setCursor(0,2);
            lcd.print(F("FATTO!"));
//            mp3.playFileByIndexNumber(BEEP);
            delay(500);
            regime = CACCIA;
//            turno = AVVERSO;
//             break;    //USCITA da case AVVERSO regime NORMALE colpo a segno
            return;
          }  //FINE else if (bFM[estratto][0] == pn)
//           break;    //torna a switch (turno)
          return;
        }  //FINE if (regime == NORMALE)

//==Caso AVVERSO regime CACCIA================================================
        else if (regime == CACCIA) {
#ifdef DEBUG_avv
Serial.println(F("33-Regime CACCIA"));
#endif
          lcd.clear();
          lcd.print(F("TOCCA A ME"));
          lcd.setCursor(0,1);
          lcd.print(F("sono in CACCIA"));
//          mp3.playFileByIndexNumber(BEEP);
          delay(500);
//In regime CACCIA è presente un solo colpo a segno e da 0 a 4 adiacenze
//la cui sparabilità è già controllata da CalcolaAdiacenze()
          //Scansione adiacenze
          for (int i=0; i<4; i++) {
//IN REGIME CACCIA LE ADIACENZE DEL SOLO COLPO DOVREBBERO ESSERE GIÀ
//CONTROLLATE E ALMENO UNA SPARABILE PERCHÈ IL CONTROLLO DEL PIXEL SINGOLO
//ATTORNIATO DA A VUOTO O GIÀ SPARATI DOVREBBE ESSERE GIÀ STATO FATTO
            if (adia[i] == 0 || bFX[adia[i]] == av ||
                                bFX[adia[i]] == as) {
              //adiacenza già usata o a vuoto o già sparata
              //predisponi uscita da for (int i=0; i<4; i++)
              //se le adia[i] sono tutte a 0 ho diritto a un altro colpo
              regime = NORMALE;
//              turno = AVVERSO;
              continue;     //esamina la prossima adiacenza (adia[i])
              //se sono TUTTE a 0 esci dal ciclo con NORMALE e AVVERSO
            }
            //Qui si arriva con un adia[i] sparabile (!=0)
            lcd.setCursor(0,3);
            lcd.print(F("-SPARO-"));
            delay(1000);

//==Caso AVVERSO regime CACCIA colpo a vuoto==================================
            if (bFX[adia[i]] == pm) {
#ifdef DEBUG_avv
Serial.print(F("34-Adiacenza colpo a vuoto n. "));
Serial.println(adia[i]);
#endif
              DispAvuoto(adia[i], bFX);
              //azzera colpo
              adia[i] = 0;
//Questa adiacenza ppotrebbe essere l'ultima della serie di max 4sparabili.
//Il controllo se TUTTE le adiacenze sono a 0 verrà fatto al prossimo turno
//dell'AVVERSO
              regime = CACCIA;
//              turno = PROPRIO;
              turno++;
              break;   //USCITA da scansione adiacenze for (int i=0; i<4; i++)
            }

//==Caso AVVERSO regime CACCIA colpo a segno==================================
            if (bFX[adia[i]] == pn) {
#ifdef DEBUG_avv
Serial.print(F("35-Case AVVERSO regime CACCIA colpo a segno "));
Serial.println(adia[i]);
#endif
              //visualizza su display e campo di battaglia
              DispAsegno(adia[i], bFX);
              //memorizza colpo in aSegno[ind] con ind=1
              aSegno[ind] = adia[i];
#ifdef DEBUG_avv
Serial.print(F("36-Memorizzato colpo a segno "));
Serial.print(adia[i]);
Serial.print(F(" in aSegno["));
Serial.print(ind);
Serial.println(F("]"));
#endif
              ind = ind + 1;
//SIAMO IN PRESENZA DI DUE COLPI A SEGNO SI CALCOLANO I LIMITI NAVE
              lcd.clear();
              lcd.print(F("TOCCA A ME"));
              lcd.setCursor(0,1);
              lcd.print(F("chk limiti nave"));
              delay(500);
              //Calcolo limiti nave
#ifdef DEBUG_avv
Serial.print(F("37-Calcolo limiti nave colpita in "));
Serial.print(aSegno[0]);
Serial.print(F(" "));
Serial.println(aSegno[1]);
Serial.print(F("   e azzeramento adiacenze "));
for (int i=0; i<4; i++) {
  if (adia[i] != 0) {
    Serial.print(F(" "));
    Serial.print(adia[i]);
  }
}
Serial.println(F(" *"));
#endif
              //azzera TUTTE le adiacenze
              for (int i=0; i<4; i++) {
                adia[i] = 0;
              }
              //calcola limiti nave
              maxLim = max(aSegno[0], aSegno[1]);
              minLim = min(aSegno[0], aSegno[1]);
#ifdef DEBUG_avv
Serial.print(F("B2-Estremi nave "));
Serial.print(maxLim);
Serial.print(F(" "));
Serial.println(minLim);
#endif
              if (maxLim - minLim == 1) {
                //la nave è in posizione orizzontale
                orientamento = ORIZZONTALE;
#ifdef DEBUG_avv
Serial.println(F("B3-Calcolo estremi nave orizzontale"));
#endif
                CalcHitOrizz(bFX);
#ifdef DEBUG_avv
Serial.print(F("A0-Limite destro "));
Serial.print(minHit);
Serial.print(F(" Limite sinistro "));
Serial.println(maxHit);
#endif
              }
              if (maxLim - minLim == 8) {
                //la nave è in posizione verticale
                orientamento = VERTICALE;
#ifdef DEBUG_avv
Serial.println(F("A1-Calcolo estremi nave verticale"));
#endif
                CalcHitVert(bFX);
#ifdef DEBUG_avv
Serial.print(F("A2-Limite superiore "));
Serial.print(maxHit);
Serial.print(F(" Limite inferiore "));
Serial.println(minHit);
#endif
              }
              //se non ci sono limiti sparabili la nave è affondata
              if (minHit == 0 && maxHit == 0) {
                lcd.setCursor(0,2);
                lcd.print(F("AFFONDATA!"));
#ifdef DEBUG_avv
Serial.print(F("40-Non ci sono limiti sparabili. La nave è affondata"));
Serial.println(F("   Ho diritto a un altro colpo"));
#endif
                delay(500);
                //azzera matrice aSegno[] (basterebbe solo 0 e 1)
                for (int i=0; i<8; i++) {
                  aSegno[i] = 0;
                }
                ind = 0;          //inizializza indice colpi a segno
                regime = NORMALE;
                //turno = AVVERSO;
              }
              else {
                lcd.setCursor(0,2);
                lcd.print(F("FATTO!"));
                delay(500);
                regime = COLPITO;
                //turno = AVVERSO;
              }

              break;   //USCITA da scansione adiacenze for (int i=0; i<16; i++)
            }   //FINE colpo a segno
          }   //FINE ciclo for (int i=0; i<16; i++) scansione adiacenze
          //regime e turno già settati
//           break;    //USCITA case AVVERSO
          return;
        }   //FINE if (regime == CACCIA)

//QUI SI ARRIVA SE CI SONO ALMENO DUE COLPI A SEGNO E C'È ALMENO UN LIMITE
//VALIDO DA SPARARE. L'ORIENTAMENTO DELLA NAVE È GIÀ SETTATO
//==Case AVVERSO regime COLPITO===============================================
        else if (regime == COLPITO) {
#ifdef DEBUG_avv
Serial.println(F("41-Regime COLPITO"));
#endif
          lcd.clear();
          lcd.print(F("TOCCA A ME"));
          lcd.setCursor(0,1);
          lcd.print(F("COLPITA una nave"));
//          mp3.playFileByIndexNumber(BEEP);
          delay(1000);
          //seleziona limite nave sparabile; seleziona primo limite se esiste
          if (maxHit != 0) {
            currLim = maxHit;
            maxHit = 0;
          }
          //o il secondo se il primo è già stato sparato o non esiste
          else if (minHit != 0) {
            currLim = minHit;
            minHit = 0;
          }
#ifdef DEBUG_avv
Serial.print(F("A3-Limite nave sparabile "));
Serial.println(currLim);
#endif
          //Qui si arriva con currLim sparabile
          lcd.setCursor(0,3);
          lcd.print(F("-SPARO-"));
          delay(1000);

//==Case AVVERSO regime COLPITO colpo a vuoto=================================
          if (bFX[currLim] == pm) {
#ifdef DEBUG_avv
Serial.print(F("A4-Case AVVERSO regime COLPITO colpo a vuoto "));
Serial.println(currLim);
#endif
            DispAvuoto(currLim, bFX);
            //se non ci sono più limiti della nave da colpire
            if (minHit == 0 && maxHit == 0) {
#ifdef DEBUG_avv
Serial.println(F("42-Non ci sono più limiti da colpire"));
Serial.print(F("   Primo limite "));
Serial.print(minHit);
Serial.print(F(" secondo limite "));
Serial.println(maxHit);
#endif
              //azzera colpi a segno
              for (int i=0; i<8; i++) {
                aSegno[i] = 0;
              }
              ind = 0;
              lcd.setCursor(0,3);
              lcd.print(F("NAVE AFFONDATA!"));
//              mp3.playFileByIndexNumber(SPLASH);
              delay(1000);
              regime = NORMALE;
              //turno = PROPRIO;
              turno++;
//               break;     //USCITA da case AVVERSO regime COLPITO colpo a segno
              return;
                         //non ci sono più limiti nave, affondata
            }
            else {
#ifdef DEBUG_avv
Serial.println(F("A5-C'è ancora almeno un limite da colpire"));
Serial.print(F("   Primo limite "));
Serial.print(adia[0]);
Serial.print(F(", secondo limite "));
Serial.println(adia[1]);
#endif
              regime = COLPITO;
              //turno = PROPRIO;
              turno++;
              return;
//               break;     //USCITA da case AVVERSO regime COLPITO colpo a vuoto
                         //c'è un altro limite da sparare
            }
          }    //FINE case AVVERSO regime COLPITO colpo a vuoto

//==Case AVVERSO regime COLPITO colpo a segno=================================
          if (bFX[currLim] == pn) {
#ifdef DEBUG_avv
Serial.print(F("A6-Case AVVERSO regime COLPITO colpo a segno "));
Serial.println(currLim);
#endif
            DispAsegno(currLim, bFX);
            //memorizza colpo in asegno
//LA MEMORIZZAZIONE DEVE AVVENIRE NEL PRIMO aSegno LIBERO INDICE ind==========
            aSegno[ind] = currLim;
#ifdef DEBUG_avv
Serial.print(F("B4-Memorizzato colpo a segno "));
Serial.print(currLim);
Serial.print(F(" in aSegno["));
Serial.print(ind);
Serial.println(F("]"));
#endif
            ind = ind + 1;      //serve anche per for successivo
            //calcola nuovi limiti nave
//==QUI I COLPI A SEGNO POSSONO ESSERE 3 O 4==================================
#ifdef DEBUG_avv
Serial.println(F("A8-Calcolo nuovi limiti nave"));
#endif
            maxLim = 0;     //predisponi per estremo numericamente più alto
            minLim = 64;    //predisponi per estremo numericamente più basso
            for (int i=0; i<ind; i++) {   //va bene ind già incrementato
              if (aSegno[i] > maxLim) {
                maxLim = aSegno[i];
              }
              if (aSegno[i] < minLim) {
                minLim = aSegno[i];
              }
            }
            if (orientamento == ORIZZONTALE) {
#ifdef DEBUG_avv
Serial.print(F("A9-Limiti nave orizzontale "));
Serial.print(minLim);
Serial.print(F(" "));
Serial.println(maxLim);
#endif
              CalcHitOrizz(bFX);
#ifdef DEBUG_avv
Serial.print(F("B5-Limite superiore "));
Serial.print(maxHit);
Serial.print(F(" Limite inferiore "));
Serial.println(minHit);
#endif
            }
            if (orientamento == VERTICALE) {
#ifdef DEBUG_avv
Serial.print(F("B0-Limiti nave verticale "));
Serial.print(minLim);
Serial.print(F(" "));
Serial.println(maxLim);
#endif
              CalcHitVert(bFX);
            }
            //se non ci sono limiti sparabili la nave è affondata
            if (minHit == 0 && maxHit == 0) {
              lcd.setCursor(0,2);
              lcd.print(F("AFFONDATA!"));
#ifdef DEBUG_avv
Serial.print(F("B6-Non ci sono limiti sparabili. La nave è affondata"));
Serial.println(F("   Ho diritto a un altro colpo"));
#endif
              delay(1000);
              //azzera matrice aSegno[] (basterebbe solo 0 e 1)
              for (int i=0; i<8; i++) {
                aSegno[i] = 0;
              }
              ind = 0;          //inizializza indice colpi a segno
              regime = NORMALE;
              //turno = AVVERSO;
            }
            else {
              lcd.setCursor(0,2);
              lcd.print(F("FATTO!"));
              delay(1000);
              regime = COLPITO;
              //turno = AVVERSO;
            }
            return;//break;    //USCITA da for (int i=0; i<16; i++)
          }   //FINE case AVVERSO regime COLPITO colpo a segno
        }   //FINE case AVVERSO regime COLPITO
        //regime e turno già settati
//         break;    //USCITA da case AVVERSO
        return;
}

void game_play_umano()
{
#ifdef DEBUG
Serial.println(F("17-Controllo colpo PROPRIO"));
#endif
        lcd.clear();
        lcd.print(F("TOCCA A TE"));
        lcd.setCursor(0,1);
        lcd.print(F("Muovi ^ v > <"));
        lcd.setCursor(0,2);
        lcd.print(F("Spara con GO"));

    delay(500);

        //Disegno campo di battaglia proprio
        draw_bf(1);
//        send_bf(1);

for (;;) {//pausa per go
  if (digitalRead(GO) == LOW) {
    delay(30);
    break;
  }
}

        //Disegno campo di battaglia avverso
        draw_bf(0);
//        send_bf(2);
    delay(300);

//        for (int i = 0; i < 64; i++) {
//          WS.setPixelColor(i, WS.Color(bFA[i][1], bFA[i][2], bFA[i][3]));
//        }
//        WS.show();
//        mp3.playFileByIndexNumber(BEEP);
//        delay(1000);
        //Accendi cursore in posizione corrente
//        WS.setPixelColor(currLed, WS.Color(cuR, cuG, cuB));
//        WS.show();

        //Muovi cursore su campo di battaglia avverso con pulsanti di
        //direzione fino alla pressione del pulsante GO
        for (;;) {
          if (MuoviCursore(AVVERSO)) continue;
          else break;
        }
        //Qui è stato premuto il pulsante di sparo
//        delay(30);
        //Accendi pixel colpo
#ifdef DEBUG
Serial.print(F("18-Sparato colpo"));
Serial.print(F(" in casella "));
Serial.println(currLed);
#endif

//==Case PROPRIO regime NORMALE colpo a vuoto=================================
        if (bFA[currLed] == pm) {
#ifdef DEBUG
Serial.print(F("19-Colpo PROPRIO a vuoto"));
#endif
          lcd.setCursor(0,3);
          lcd.print(F("colpo a vuoto"));
//          mp3.playFileByIndexNumber(SPLASH);
//          delay(1000);
          bFA[currLed] = av;// bFA[currLed][1] = avR; bFA[currLed][2] = avG; bFA[currLed][3] = avB;
//          WS.setPixelColor(currLed, WS.Color(bFA[currLed][1], bFA[currLed][2],
//                                             bFA[currLed][3]));
//          WS.show();
          delay(1000);
          //passa il controllo all'avverso
//          turno = AVVERSO;
          turno++;
//           break;    //USCITA da case PROPRIO regime NORMALE colpo a vuoto
          return;
        }

//==Case PROPRIO regime NORMALE colpo a segno=================================
        else if (bFA[currLed] == pn) {

          lcd.setCursor(0,3);
          lcd.print(F("colpo a segno!"));
//          mp3.playFileByIndexNumber(BOOM);
//          delay(1000);
          bFA[currLed] = as;// bFA[currLed][1] = asR; bFA[currLed][2] = asG; bFA[currLed][3] = asB;
//          WS.setPixelColor(currLed, WS.Color(bFA[currLed][1], bFA[currLed][2],
//                                             bFA[currLed][3]));
//          WS.show();
          delay(1000);
          //continua con il prossimo colpo
//          turno = PROPRIO;
//           break;    //USCITA da case PROPRIO regime NORMALE colpo a segno
          return;
        }

        else {
#ifdef DEBUG
Serial.println(F("21-sparo ripetuto su av o as!!!"));
#endif
//          turno = AVVERSO;
          turno++;
//           break;    //USCITA da case PROPRIO regime NORMALE colpo ripetuto
          return;
        }
//         break;    //USCITA da case PROPRIO
        return;
}

void game_reader()
{
  if (lt.available())
  {
    Serial.println(F("Data available"));

    int packetSize = lt.read((uint8_t*)input, sizeof(input));
    if (packetSize > 0)
    {
      Serial.print(F("Packet read OK size="));
      Serial.println(packetSize);
      //dump the received packet.
      for(int i = 0; i < packetSize; i++)
      {
        Serial.print(i);
        Serial.print("=");
        Serial.println(input[i]);
      }

      uint8_t L = input[9];
      Serial.print(" line = ");
      Serial.println(L, DEC);

      for(int i = 0; i < 8; i++)
      {

        if (turno < input[8]) // buono il remoto
        {
          output[i]=input[i]; //copy the byte

          if ((unsigned)(L) <= 7) //0..7
          {
            if (i < 8) bFM[(L*8)+i]=input[i]; //copy the byte
          }
          if ((unsigned)(L-8) <= 7) //8..15
          {
            if (i < 8) bFA[((L-8)*8)+i]=input[i]; //copy the byte
          }
        }else
        {
          if (turno == 0) // copia iniziale incrociata
          {
            //mando battleFieldMia
            if ((unsigned)(L) <= 7) //0..7
            {
              Serial.println("T0: mando battleFieldMia");
              if (i < 8) output[i] = bFM[(L*8)+i]; //copy the byte
            }
            //copio battleFieldAltro
            if ((unsigned)(L-8) <= 7) //8..15
            {
              Serial.println("T0: copio battleFieldAltro");
              output[i]=input[i]; //copy the byte
              if (i < 8) bFA[((L-8)*8)+i]=input[i]; //copy the byte
            }
          }else // buono il locale
          {
            if ((unsigned)(L) <= 7) //0..7
            {
              if (i < 8) output[i] = bFM[(L*8)+i]; //copy the byte
            }
            if ((unsigned)(L-8) <= 7) //8..15
            {
              if (i < 8) output[i] = bFA[((L-8)*8)+i]; //copy the byte
            }
          }
        }
      }

      delay(100);//necessario!
      //send ACK
/*
      if (L==15)
      {
        draw_bf(0);
      }

      if (L==7)
      {
        draw_bf(1);
      }
*/
      if (L==16)
      {
        //16 = turno | bFA_ck | bFM_ck | _ | _ | _ | _ | _ | 0 | 16

        byte bFA_ck = 0;
        byte bFM_ck = 0;
        for (int i = 0; i < 64; i++) {
          bFA_ck += (i+1)*bFA[i];
          bFM_ck += (i+1)*bFM[i];
        }

        Serial.print("turno (ingresso) = ");
        Serial.println(turno, DEC);
        Serial.print("bFA_ck = ");
        Serial.println(bFA_ck, DEC);
        Serial.print("bFM_ck = ");
        Serial.println(bFM_ck, DEC);

        if ((bFA_ck == input[2])&&(bFM_ck == input[1]))
        {
          if (turno < input[0])
          {
            turno = input[0];
            Serial.println("copio il turno remoto... ");
            //qui il flag di sync ok
            for (; ((turno%2)^(writer)); )
            {
              if (!game_verifica_fine())
              {
                Serial.println("eseguo turno di gioco");
                //game_main();
  //              game_play_umano();
                game_play_computer(1);
        //        game_fake();
              }
            }
            Serial.println("fine turno di gioco");
          }
        }

        bFA_ck = 0;
        bFM_ck = 0;
        for (int i = 0; i < 64; i++) {
          bFA_ck += (i+1)*bFA[i];
          bFM_ck += (i+1)*bFM[i];
        }

        Serial.print("turno (uscita) = ");
        Serial.println(turno, DEC);
        Serial.print("bFA_ck = ");
        Serial.println(bFA_ck, DEC);
        Serial.print("bFM_ck = ");
        Serial.println(bFM_ck, DEC);

        output[0] = turno;
        output[2] = bFA_ck;
        output[1] = bFM_ck;
        output[3] = 0;
        output[4] = 0;
        output[5] = 0;
        output[6] = 0;
        output[7] = 0;
//          output[8] = turno;
//        output[sizeof(output)-1] = 16;   /* line number manually added @ last byte */
      }

      //send ACK
      output[8] = turno;
      output[9] = L;

      Serial.println();
      Serial.println("invio questo ack:");
      Serial.println((char *) output);
      for(int i = 0; i < sizeof(output); i++)
      {
        Serial.print(i);
        Serial.print("=");
        Serial.println(output[i]);
      }
      lt.sendPacket((uint8_t*)output, sizeof(output));

    }
    else
    {
      Serial.println(F("Packet read fail"));
//        lt.whatsUp(Serial);
    }

    lt.startListening();
  }
}

void game_fake()
{
//        send_bf(1);
        draw_bf(1); // 1 = bFM
        delay(3000);
        draw_bf(0);
        delay(3000);
//        bFM[turno%sizeof(bFM)] = bFM[turno%sizeof(bFM)]-1;
        if ((turno%2)^(writer))
        {
          Serial.println(F("TOCCA A ME, muovo e mando avanti il turno..."));
          bFM[turno%sizeof(bFM)] = bFM[turno%sizeof(bFM)]-1;
//          bFA[turno%sizeof(bFA)] = bFA[turno%sizeof(bFA)]-1;
          turno++;
        }
//        send_bf(1);
}

void sync_check()
{
  /*
   * invio il mio turno ed il checksum di bf0 e bf1
   * confronto con quello remoto e se coincide imposto synced = true
   */
  byte bFA_ck = 0;
  byte bFM_ck = 0;
  for (int i = 0; i < 64; i++) {
    bFA_ck += (i+1)*bFA[i];
    bFM_ck += (i+1)*bFM[i];
  }
  Serial.print("turno = ");
  Serial.println(turno, DEC);
  Serial.print("bFA_ck = ");
  Serial.println(bFA_ck, DEC);
  Serial.print("bFM_ck = ");
  Serial.println(bFM_ck, DEC);

  /* valori di r (row):
   *  0 -  7 = bFM
   *  8 - 15 = bFA
   *      16 = sync_check ( turno | bFA_ck | bFM_ck | 0 | 0 | 0 | 0 | 0 | 0 | 16 )
   */

  output[0] = turno;

  if ( writer )
  {
    output[1] = bFA_ck;
    output[2] = bFM_ck;
  }else
  {
    output[2] = bFA_ck;
    output[1] = bFM_ck;
  }

  output[3] = 0;
  output[4] = 0;
  output[5] = 0;
  output[6] = 0;
  output[7] = 0;
  output[8] = turno;
  output[sizeof(output)-1] = 16;   /* line number manually added @ last byte */

  if (msg_send())
  {
    synced=true;
  }else
  {
    if ((bFA_ck == input[1])&&(bFM_ck == input[2]))
    {
      if (turno < input[0])
      {
        turno = input[0];
        Serial.println("copio il turno remoto... ");
        //qui il flag di sync ok
        synced=true;
      }
    }else
    {
      synced=false;
    }
  }

}

void draw_bf(byte bF)
{
  lcd.clear();
  uint8_t c;
  uint8_t r;
  //Disegno campo di battaglia
  for (int i=0; i<64; i++)
  {
    c = i % 8;
    r = i / 8;
//    Serial.print(F(" c="));
//    Serial.print(c);
//    Serial.print(F(" r="));
//    Serial.println(r);
    lcd.setCursor(c,r);

    if ( bF == 1 )
    {
      lcd.write(bFM[i]);
    } else
    {

      if (bFA[i] == pn && bF == 0)
      {
        lcd.write(pm); //camuffo nave avversaria ignota
      } else
      {
        lcd.write(bFA[i]);
      }

    }
  }

#ifdef DEBUG
  lcd.setCursor(9,3);
  if ( bF == 1 )
  {
    lcd.print(F("bFM"));
  } else
  {
    lcd.print(F("bFA"));
  }
  lcd.setCursor(9,5);
  lcd.print(F("T="));
  lcd.print(turno);
#endif

}

bool msg_send()
{

  bool same_msg=false;
  byte retry_counter = 0;

      //send and wait the ACK
      for (bool ack=0; ack==0; )
      {
        Serial.println(F("the output packet:"));
        //dump the output packet.
        for(int i = 0; i < sizeof(output); i++)
        {
          Serial.print(i);
          Serial.print(" = ");
          Serial.println(output[i], DEC);
        }
        Serial.println((char *) output);
        lt.sendPacket((uint8_t*)output, sizeof(output));
//        lt.whatsUp(Serial);
        lt.startListening();
//        Serial.println(F("2listen..."));

        for (byte wh=0; (not lt.available()) and (wh < 30); wh++ )
        {
          Serial.print(F("while="));
          Serial.print(wh);
          Serial.print(F(" available="));
          Serial.println((not lt.available()), DEC);
          delay(200);
        }

        if (lt.available())
        {
          Serial.println(F("3Data available"));

          int packetSize = lt.read((uint8_t*)input, sizeof(input));
          if (packetSize > 0)
          {
            Serial.println(F("4Packet read OK"));
            ack=1;

            //dump the received packet.
            for(int i = 0; i < packetSize; i++)
            {
              Serial.print(i);
              Serial.print(" = ");
              Serial.println(input[i], DEC);
            }
            Serial.println((char *) input);

            //compara output con ack e se uguale ok
            //altrimenti alza il flag per sync_check()
            int n;
            n=memcmp ( output, input, sizeof(output) );
            if (n != 0)
            {
              Serial.println(F("sent data differs from received data"));
//              synced=false;
            }else
            {
              same_msg=true;
            }

          }else
          {
            Serial.println(F("5Packet read fail"));
//            lt.whatsUp(Serial);
            delay(10);
          }
        }else
        {
          retry_counter++;
          Serial.print("retry_counter = ");
          Serial.println(retry_counter, DEC);

          if (retry_counter>5)
          {
            Serial.println("resetting the radio");
            lt.begin();
            lt.setCurrentControl(4,15);
            lt.setDataRate(LT8900::LT8910_62KBPS);
            lt.setChannel(0x06);
          }
        }
      }

  return same_msg;
}

void send_bf(byte bF)
{
  uint8_t c; //bf column
  uint8_t r; //bf row
  //invia campo di battaglia una linea alla volta
  for (int i=0; i<64; i++)
  {
    c = i % 8;
    r = i / 8;

    if ( bF == 1 )
    {
      output[c] = bFM[i];
    } else
    {
      if (bFA[i] == pn)
      {
        output[c] = bFA[i];
      } else
      {
        output[c] = bFA[i];
      }
    }

    if (c==7)
    {
      //indica il nostro turno
      output[8] = turno;
      //indica nel messaggio quale dei due bF stiamo inviando
  /* valori di r (row):
   *  0 -  7 = bFM
   *  8 - 15 = bFA
   *      16 = sync_check ( turno | bFA_ck | bFM_ck | 0 | 0 | 0 | 0 | 0 | 0 | 16 )
   */
      if (( bF != 1 )^( writer ))
      {
        r = r + 8;
      }

      output[sizeof(output)-1] = r;   /* line number manually added @ last byte */

      //msg_send();
      if (!msg_send())
      {

        uint8_t L = input[9];
        Serial.print("message type: sent = ");
        Serial.print(output[9], DEC);
        Serial.print(" received = ");
        Serial.println(L, DEC);

        if ((turno == 0) and (L == output[9]))
        {
          //copio battleFieldAltro
          Serial.println("T0: copio battleFieldAltro");
          if ((unsigned)(L) <= 7) //0..7
          {
            for(int i = 0; i < 8; i++)
            {
              bFA[(L*8)+i]=input[i]; //copy the byte
            }
          }
        }

        if ((turno < input[8]) and (L == output[9])) // buono il remoto
        {
          Serial.println("prendo per buono il remoto");
          if ((unsigned)(L) <= 7) //0..7
          {
            Serial.print("copio su bFA L=");
            Serial.println(L, DEC);
            for(int i = 0; i < 8; i++)
            {
              bFA[(L*8)+i]=input[i]; //copy the byte
            }
          }
          if ((unsigned)(L-8) <= 7) //8..15
          {
            Serial.print("copio su bFM L=");
            Serial.println(L, DEC);
            for(int i = 0; i < 8; i++)
            {
              bFM[((L-8)*8)+i]=input[i]; //copy the byte
            }
          }
        }
      }

      Serial.println((char *) output);
      delay(250);
    }
  }
}


//==FUNZIONI=================================================================

void posiziona_navi_umano(void) {
//==POSIZIONAMENTO NAVI PROPRIE===============================================
//Sono da posizionare: una nave da 4 (corazzata), due navi da 3
//(cacciatorpediniere) e una nave da 2 (motovedetta).
//Il posizionamento avviene attraverso il pulsante "toggle" GO e i pulsanti di
//spostamento orizzontale e verticale
//All'inizio viene proposto un orientamento orizzontale della nave da
//piazzare.
//Inizialmente con il polsante "GO" è possibile cambiare l'orientamento da
//orizzontale a verticale a piacere, ma non appena si effettua un primo
//spostamento l'orientamento non è più modificabile.
//Una ulteriore pressione del pulsante "GO" determina il posiionamento
//definitivo della nave.
//Viene effettuato un controllo sulla sovrapposizione di parti di navi. Se
//viene riscontrata una sovrapposizione viene segnalato l'errore e riproposto
//il posizionamento della nave.
//Al piazzamento dell'ultima nave (motovedetta) si passa automaticamente alla
//fase di combattimento.

#ifdef DEBUG
Serial.println(F("03-Inizio posizionamento navi proprie"));
#endif

  //Cancella schermo
  lcd.clear();
//  lcd.print(F("PIAZZA LE TUE NAVI"));
  lcd.print(F("  LE TUE NAVI:  "));
  lcd.setCursor(0,1);
  lcd.print(F("1x Corazzata 4x@"));
  lcd.setCursor(0,2);
  lcd.print(F("2x Cacciat.  3x@"));
  lcd.setCursor(0,3);
  lcd.print(F("1x Motoved.  2x@"));
  //Richiesta attenzione sonora
//  mp3.playFileByIndexNumber(BEEP);
  delay(4000);
  lcd.clear();
  lcd.print(F("il pulsante GO: "));
  lcd.setCursor(0,1);
  lcd.print(F("prima INVERTE"));
  lcd.setCursor(0,2);
  lcd.print(F("poi PIAZZA la"));
  lcd.setCursor(0,3);
  lcd.print(F("nave"));
  //Richiesta attenzione sonora
//  mp3.playFileByIndexNumber(BEEP);
  delay(4000);

//PIAZZAMENTO 4 NAVI IN SEQUENZA 4, 3, 3, 2
  for (int i=0; i<4; i++) {
//    mp3.setLoopMode(MP3_LOOP_ONE);
//    mp3.playFileByIndexNumber(PING);
    lcd.clear();
//    lcd.print(F("INVERTI o MUOVI"));
//    lcd.setCursor(0,1);
//    lcd.print(F("e infine PIAZZA"));
    if (i == 0) {
      lNave=4;
      lcd.setCursor(0,2);
      lcd.print(F("la corazzata"));
#ifdef DEBUG
Serial.println(F("04-Piazzamento corazzata"));
#endif
    }
    if (i == 1) {
      lNave=3;
      lcd.setCursor(0,2);
      lcd.print(F("cacciatorp. #1"));
#ifdef DEBUG
Serial.println(F("05-Piazzamento primo cacciatorpediniere"));
#endif
    }
    if (i == 2) {
      lNave=3;
      lcd.setCursor(0,2);
      lcd.print(F("cacciatorp. #2"));
#ifdef DEBUG
Serial.println(F("06-Piazzamento secondo cacciatorpediniere"));
#endif
    }
    if (i == 3) {
      lNave=2;
      lcd.setCursor(0,2);
      lcd.print(F("la motovedetta"));
#ifdef DEBUG
Serial.println(F("07-Piazzamento motovedetta"));
#endif
    }
    //Se il piazzamento risulta ERRATO (false)
    if (!PiazzamentoNave(lNave)) {
      i = i-1;    //ripeti il piazzamento della stessa nave
    }
    delay(2000);
  }   //FINE for (int i=0; i<4; i++) loop piazzamento 4 navi

#ifdef DEBUG
Serial.println(F("08-Termine posizionamento navi proprie"));
#endif
}

void posiziona_navi_computer(byte bF)
{
#ifdef DEBUG
Serial.println(F("09-Inizio posizionamento navi avversarie"));
#endif

//==POSIZIONAMENTO NAVI AVVERSO===============================================
//Sono da posizionare: una nave da 4 (corazzata), due navi da 3
//(cacciatorpediniere) e una nave da 2 (motovedetta).
//Il posizionamento avviene scegliendo prima di tutto se la nave deve essere
//posizionata in verticale oppure in orizzontale.
//Successivamente si sceglie in maniera casuale un punto di posizionamento e
//si verifica che la nave non vada ad occupare caselle già impegnate dal
//posizionamento di un'altra nave. In questo caso si procede automaticamente
//ad un'altra scelta in maniera ancora casuale, fino al posizionamento
//definitivamente corretto

  //Creazione e posizionamento navi avversarie
  //Posizionamento corazzata (4 pixel)
  lcd.clear();
  lcd.print(F("Piazzo le MIE"));
  lcd.setCursor(0,1);
  lcd.print(F("navi..."));
  lcd.setCursor(0,2);
  lcd.print(F("corazzata"));
//  delay(1000);
  PosizionaCorazzata();
  lcd.setCursor(0,3);
  lcd.print(F("- PIAZZATA! -"));
//  mp3.playFileByIndexNumber(GONG);
  delay(500);
#ifdef DEBUG
Serial.print(F("10-Piazzata corazzata da 4 pixel "));
for (int i = 0; i < 4; i++) {
  Serial.print(posiz[i]);
  Serial.print(" ");
}
Serial.println("");
#endif
  for (int i = 0; i < 4; i++) { //azzeramento matrice posiz[]
    posiz[i] = 0;
  }
  //Posizionamento primo cacciatorpediniere (3 pixel)
  lcd.clear();
  lcd.print(F("Piazzo le MIE"));
  lcd.setCursor(0,1);
  lcd.print(F("navi..."));
  lcd.setCursor(0,2);
  lcd.print(F("primo caccia"));
//  delay(1000);
  PosizionaCacciatorpediniere();
  lcd.setCursor(0,3);
  lcd.print(F("- PIAZZATO! -"));
//  mp3.playFileByIndexNumber(GONG);
  delay(500);
#ifdef DEBUG
Serial.print(F("11-Piazzato primo cacciatorpediniere da 3 pixel "));
for (int i = 0; i < 3; i++) {
  Serial.print(posiz[i]);
  Serial.print(" ");
}
Serial.println("");
#endif
  for (int i = 0; i < 4; i++) { //azzeramento matrice posiz[]
    posiz[i] = 0;
  }
  //Posizionamento secondo cacciatorpediniere (3 pixel)
  lcd.clear();
  lcd.print(F("Piazzo le MIE"));
  lcd.setCursor(0,1);
  lcd.print(F("navi..."));
  lcd.setCursor(0,2);
  lcd.print(F("secondo caccia"));
//  delay(1000);
  PosizionaCacciatorpediniere();
  lcd.setCursor(0,3);
  lcd.print(F("- PIAZZATO! -"));
//  mp3.playFileByIndexNumber(GONG);
  delay(500);
#ifdef DEBUG
Serial.print(F("12-Piazzato secondo cacciatorpediniere da 3 pixel "));
for (int i = 0; i < 3; i++) {
  Serial.print(posiz[i]);
  Serial.print(" ");
}
Serial.println("");
#endif
  for (int i = 0; i < 4; i++) { //azzeramento matrice posiz[]
    posiz[i] = 0;
  }
  //Posizionamento motovedetta (2 pixel)
  lcd.clear();
  lcd.print(F("Piazzo le MIE"));
  lcd.setCursor(0,1);
  lcd.print(F("navi..."));
  lcd.setCursor(0,2);
  lcd.print(F("motovedetta"));
//  delay(1000);
  PosizionaMotovedetta();
  lcd.setCursor(0,3);
  lcd.print(F("- PIAZZATA! -"));
//  mp3.playFileByIndexNumber(GONG);
  delay(500);
#ifdef DEBUG
Serial.print(F("13-Piazzata motovedetta da 2 pixel "));
for (int i = 0; i < 2; i++) {
  Serial.print(posiz[i]);
  Serial.print(" ");
}
Serial.println("");
#endif
  for (int i = 0; i < 4; i++) { //azzeramento matrice posiz[]
    posiz[i] = 0;
  }
  //Termine posizionamento navi avversario
#ifdef DEBUG
Serial.println(F("14-Termine posizionamento navi avversarie"));
//delay(1000);
#endif
  lcd.clear();
  lcd.print(F("Piazzamento navi"));
  lcd.setCursor(0,1);
  lcd.print(F("- TERMINATO! -"));
//  mp3.playFileByIndexNumber(BEEP);
  delay(1000);
//  WS.clear();
//  WS.show();

  //Predisponi campo di battaglia avverso
  for (int i = 0; i < 64; i++) {
    if ( bF == 1 )
    {
      bFM[i] = pm;
    }else
    {
      bFA[i] = pm;
    }
  }
  //Trasferimento navi avversario in campo di battaglia avversario
  //Il trasferimento deve avvenire SOLO con il marcatore (bFA[0]=pn) perché
  //occorre evitare che durante lo spostamento del cursore venga evidenziata
  //la posizione delle navi avversarie.
  for (int i = 0; i < 12; i++) {
/*
 * bF:
 * 1 = bFM
 * 0 = bFA
 */
    if ( bF == 1 )
    {
      bFM[memNavi[i]] = pn;
    }else
    {
      bFA[memNavi[i]] = pn;
    }
  }
}



//---------------------------------------------------------------------------
//Questa funzione effettua il piazzamento di una nave di lunghezza lNave del
//giocatore. Controlla anche il pulsante GO.
#define DEBUGspost      //DECOMMENTA PER SPOSTAMENTI
bool PiazzamentoNave(int lNave) {
  //Si inizia con il posizionamento ORIZZONTALE
  posizionamento = ORIZZONTALE;
  lcd.setCursor(0,3);
  lcd.print(F("ORIZZONTALE"));
  while (posizionamento != EFFETTUATO) {
    //Disegna nave
    for (int i=0; i<lNave; i++) {
      if (posizionamento == ORIZZONTALE) {
        nave[i] = 8 - lNave + i;
      }
      else if (posizionamento == VERTICALE) {
        nave[i] = (lNave-i)*8-1;
      }
    }

    toggleGO = INVERTE;
#ifdef DEBUGspost
Serial.println(F("89-Entro in for(;;) analisi comando spostamento"));
#endif
    //Si resta in questa loop fino al posizionamento EFFETTUATO
    for (;;) {
      //SE pulsante SU
      if (digitalRead(SU) == LOW) {
#ifdef DEBUGspost
Serial.println(F("90-Comando sposta in su"));
#endif
        delay(30);
        // SE spostamento consentito
        if (posizionamento == ORIZZONTALE) {
          if (nave[0] + 8 < 64) {
            pNave = &nave[0];
            SpostaNave(pNave, lNave, SU);
          }
          while (digitalRead(SU) == LOW) ;
        }
        else if (posizionamento == VERTICALE) {
          if (nave[0] < 56) {
            pNave = &nave[0];
            SpostaNave(pNave, lNave, SU);
          }
          while (digitalRead(SU) == LOW) ;
        }
      }   //FINE if (digitalRead(SU) == LOW)

      //SE pulsante GIU
      if (digitalRead(GIU) == LOW) {
#ifdef DEBUGspost
Serial.println(F("91-Comando sposta in giu"));
#endif
        delay(30);
        //SE spostamento consentito
        if (posizionamento == ORIZZONTALE) {
          if (nave[0] - 8 >= 0) {
            pNave = &nave[0];
            SpostaNave(pNave, lNave, GIU);
          }
          while (digitalRead(GIU) == LOW) ;
        }
        else if (posizionamento == VERTICALE) {
          if (nave[lNave-1] >= 8) {
            pNave = &nave[0];
            SpostaNave(pNave, lNave, GIU);
          }
          while (digitalRead(GIU) == LOW) ;
        }
      }   //FINE if (digitalRead(GIU) == LOW)

      //SE pulsante DST
      if (digitalRead(DST) == LOW) {
#ifdef DEBUGspost
Serial.println(F("92-Comando sposta a destra"));
#endif
        delay(30);
        //SE spostamento consentito
        if (posizionamento == ORIZZONTALE) {
          if (nave[0] % 8 != 0) {
            pNave = &nave[0];
            SpostaNave(pNave, lNave, DST);
          }
          while (digitalRead(DST) == LOW) ;
        }
        else if (posizionamento == VERTICALE) {
          if (nave[0] % 8 != 0) {
            pNave = &nave[0];
            SpostaNave(pNave, lNave, DST);
          }
          while (digitalRead(DST) == LOW) ;
        }
      }   //FINE if (digitalRead(DST) == LOW)

      //SE pulsante SIN
      if (digitalRead(SIN) == LOW) {
#ifdef DEBUGspost
Serial.println(F("93-Comando sposta a sinistra"));
#endif
        delay(30);
        //SE spostamento consentito
        if (posizionamento == ORIZZONTALE) {
          if (nave[0] % 8 < 8 - lNave) {
            pNave = &nave[0];
            SpostaNave(pNave, lNave, SIN);
          }
          while (digitalRead(SIN) == LOW) ;
        }
        else if ( posizionamento == VERTICALE) {
          if (nave[0] % 8 != 7) {
            pNave = &nave[0];
            SpostaNave(pNave, lNave, SIN);
          }
          while (digitalRead(SIN) == LOW) ;
        }
      }   //FINE if (digitalRead(SIN) == LOW)

      //SE pulsante GO
      if (digitalRead(GO) == LOW) {
        delay(30);
        if (toggleGO == INVERTE) {
#ifdef DEBUGspost
Serial.println(F("94-Comando inverti nave"));
#endif
          if (posizionamento == ORIZZONTALE) {
            posizionamento = VERTICALE;
            lcd.setCursor(0,3);
            lcd.print(F("VERTICALE  "));
          }
          else if(posizionamento == VERTICALE) {
            posizionamento = ORIZZONTALE;
            lcd.setCursor(0,3);
            lcd.print(F("ORIZZONTALE"));
          }

          while (digitalRead(GO) == LOW) ;
          break;    //esci da for (;;)
        }
        else if (toggleGO == PIAZZA) {
#ifdef DEBUGspost
Serial.println(F("96-Comando piazza nave"));
#endif
          posizionamento = EFFETTUATO;
          //controllo sovrapposizione navi (anche parziale)
          for (int i=0; i<lNave; i++) {
            if (bFM[nave[i]] == pn) {
              posizionamento = ERRATO;
#ifdef DEBUG
Serial.print(F("E00-Posizionamento errato! nave[i]="));
Serial.println(nave[i]);
#endif
              //ripristina sfondo in pixel non sovrapposti
              for (int i=0; i<lNave; i++) {
                if (bFM[nave[i]] == pn) {  //se è un pixel sovrapposto
                  continue;                   //non ripristinarlo
                }
              }   //FINE for (int i=0; i<lNave; i++)
            }   //FINE if (bFM[nave[i]][0] == pn)
          }   //FINE for (int i=0; i<lNave; i++)
          //Qui si esce con posizionamento = EFFETTUATO oppure ERRATO
          if (posizionamento == EFFETTUATO) {
            //si piazza la nave e si esce da while
            for (int i=0; i<lNave; i++) {
              bFM[nave[i]] = pn;
            }
#ifdef DEBUG
Serial.print(F("97-Nave da "));
Serial.print(lNave);
Serial.print(F(" piazzata ai pixel"));
for (int i=0; i<lNave; i++) {
  Serial.print(F(" "));
  Serial.print(nave[i]);
}
Serial.println("");
#endif
            while (digitalRead(GO) == LOW) ;
            break;    //esci da for(;;) e torna a while
          }
          else if (posizionamento == ERRATO) {
//            mp3.setLoopMode(MP3_LOOP_NONE);
//            mp3.playFileByIndexNumber(GONG);
            lcd.clear();
            lcd.print(F("++ERRORE++"));
            lcd.setCursor(0,1);
            lcd.print(F("SOVRAPPOSIZIONE"));
            lcd.setCursor(0,2);
            lcd.print(F("NAVI O POS. 0"));
            lcd.setCursor(0,3);
            lcd.print(F("Riposiziona nave"));
            delay(4000);
            posizionamento = ORIZZONTALE;
            return false;
          }
        }   //FINE else if (toggleGO == PIAZZA)
      }   //FINE if (digitalRead(GO) == LOW)
    }   //FINE for (;;)
#ifdef DEBUGspost
Serial.println(F("98-Sono uscito da loop for(;;) posizionamento nave"));
#endif
  }   //FINE while (posizionamento != EFFETTUATO)
  lcd.setCursor(0,3);
  if (lNave == 3) {
    lcd.print(F("- PIAZZATO! -"));
  }
  else {
    lcd.print(F("- PIAZZATA! -"));
  }
//  mp3.playFileByIndexNumber(GONG);
  delay(500);
  lcd.setCursor(0,3);
  lcd.print(F("             "));
#ifdef DEBUGspost
Serial.println(F("99-Piazzamento effettuato"));
#endif
  return true;
}


//---------------------------------------------------------------------------
//Questa funzione posiziona una corazzata (4 pixel) in campo AVVERSO
void PosizionaCorazzata() {
#ifdef DEBUG
Serial.println(F("45-Posizionamento corazzata"));
#endif
  for (;;) {
    //Scelta se posizionamento orizzontale (=0) o verticale (=1)
    num02 = random(0, 2);
    //Posizionamento orizzontale
    if (num02 == 0) {
#ifdef DEBUG
Serial.println(F("46-Posizionamento orizzontale"));
#endif
      num08 = random(0, 8);   //num08 = numero riga
#ifdef DEBUG
Serial.print(F("47-Scelta riga = "));
Serial.println(num08);
#endif
      //ricavo numero iniziale della riga scelta (0,8,16,24,32,40,48,56)
      num08 = num08 * 8;      //numero iniziale della riga
      //selezione numero iniziale posizionamento corazzata nella riga scelta
      num05 = random(0, 5);    //posizione primo numero nella riga
      //ricavo posizione primo elemento della corazzata
      posiz[0] = num08 + num05; //posizione primo pixel della corazzata
#ifdef DEBUG
Serial.print(F("48-Primo pixel = "));
Serial.println(posiz[0]);
#endif
      //occorre controllare che i 4 elementi consecutivi che costituiscono la
      //corazzata non compaiano nella memoria delle posizioni (12 elementi).
      bool flag = false;
      for (int i=0; i<4; i++) {
        posiz[i] = posiz[0] + i;
        if (VerificaNumero(posiz[i])) {   //verifica numero
#ifdef DEBUG
Serial.print(F("49-Posizione  "));
Serial.print(posiz[i]);
Serial.println(F(" già usata."));
#endif
          flag = true;
        }
      }
      if (flag) {
        continue;
      }
      //i 4 numeri sono validi; memorizza posizioni nelle prime posizioni vuote
      //della matrice memNavi; non è possibile che non ci siano posizioni vuote.
      for (int i = 0; i < 4; i++) { //per ciascuno delle 4 posizioni
        for (int j = 0; j < 12; j++) { //scandisci la matrice
          if (memNavi[j] == 0) {    //al primo elemento vuoto
            memNavi[j] = posiz[i];  //memorizza posizione
            break;                  //vai alla posizione successiva
          }
        }
      }
      break;      //dovrebbe uscire dalla loop for (;;)
    }  //FINE posizionamento orizzontale

    //Posizionamento verticale
    if (num02 == 1) {
#ifdef DEBUG
Serial.println(F("53-Posizionamento verticale"));
#endif
      num08 = random(0, 8);   //num08 = numero colonna (0-7)
#ifdef DEBUG
Serial.print(F("54-Scelta colonna = "));
Serial.println(num08);
#endif
      num05 = random(0, 5);    //posizione primo numero nella colonna
      //ricavo posizione primo elemento della corazzata
      posiz[0] = num05 * 8 + num08;
#ifdef DEBUG
Serial.print(F("55-Primo pixel = "));
Serial.println(posiz[0]);
#endif
      //occorre controllare che i 4 elementi modulo 8 che costituiscono la
      //corazzata non compaiano nella memoria delle posizioni (12 elementi).
      bool flag = false;
      for (int i=0; i<4; i++) {
        posiz[i] = posiz[0] + (i*8);
        if (VerificaNumero(posiz[i])) {   //verifica numero
#ifdef DEBUG
Serial.print(F("5-Posizione  "));
Serial.print(posiz[i]);
Serial.println(F(" già usata."));
#endif
          flag = true;
        }
      }
      if (flag) {
        continue;
      }
      //i 4 numeri sono validi; memorizza posizioni nelle prime posizioni vuote
      //della matrice memNavi; non è possibile che non ci siano posizioni vuote.
      for (int i = 0; i < 4; i++) { //per ciascuno delle 4 posizioni
        for (int j = 0; j < 12; j++) { //scandisci la matrice
          if (memNavi[j] == 0) {    //al primo elemento vuoto
            memNavi[j] = posiz[i];  //memorizza posizione
            break;                  //vai alla posizione successiva
          }
        }
      }
      break;    //esce dalla loop for (;;)
    }  //FINE posizionamento verticale
  }  //FINE for (;;)
}


//---------------------------------------------------------------------------
//Questa funzione posiziona un cacciatorpediniere (3 pixel)
void PosizionaCacciatorpediniere() {
#ifdef DEBUG
Serial.println(F("60-Posiziona cacciatorpediniere"));
#endif
  for (;;) {
    //Scelta se posizionamento orizzontale (=0) o verticale (=1)
    num02 = random(0, 2);
    //Posizionamento orizzontale
    if (num02 == 0) {
#ifdef DEBUG
Serial.println(F("61-Posizionamento orizzontale"));
#endif
      num08 = random(0, 8);   //num08 = numero riga
#ifdef DEBUG
Serial.print(F("62-Scelta riga = "));
Serial.println(num08);
#endif
      //ricavo numero iniziale della riga scelta (0,8,16,24,32,40,48,56)
      num08 = num08 * 8;      //numero iniziale della riga
      //selezione numero iniziale posizionamento cacciatorpediniere nella riga
      num06 = random(0, 6);   //posizione primo numero nella riga
      //ricavo posizione primo elemento del cacciatorpediniere
      posiz[0] = num08 + num06; //posizione primo elemento
#ifdef DEBUG
Serial.print(F("63-Primo pixel = "));
Serial.println(posiz[0]);
#endif
      //occorre controllare che i 3 elementi consecutivi che costituiscono il
      //cacciatorpediniere non compaiano nella matrice memNavi (12 elementi);
      bool flag = false;
      for (int i=0; i<3; i++) {
        posiz[i] = posiz[0] + i;
        if (VerificaNumero(posiz[i])) {   //verifica numero
#ifdef DEBUG
Serial.print(F("64-Posizione  "));
Serial.print(posiz[i]);
Serial.println(F(" già usata."));
#endif
          flag = true;
        }
      }
      if (flag) {
        continue;
      }
      //i 3 numeri sono validi; memorizza posizioni nelle prime posizioni vuote
      //della matrice memNavi; non è possibile che non ci siano posizioni vuote.
      for (int i = 0; i < 3; i++) { //per ciascuno delle 3 posizioni
        for (int j = 0; j < 12; j++) { //scandisci la matrice
          if (memNavi[j] == 0) {    //al primo elemento vuoto
            memNavi[j] = posiz[i];  //memorizza posizione
            break;                  //vai alla posizione successiva
          }
        }
      }
      break;    //esce dalla loop for (;;)
    }  //FINE posizionamento orizzontale

    //Posizionamento verticale
    if (num02 == 1) {
#ifdef DEBUG
Serial.println(F("67-Posizionamento verticale"));
#endif
      num08 = random(0, 8);   //num08 = numero colonna (0-7)
#ifdef DEBUG
Serial.print(F("68-Scelta colonna = "));
Serial.println(num08);
#endif
      num06 = random(0, 6);   //posizione primo numero nella colonna
      //ricavo posizione primo elemento del cacciatorpediniere
      posiz[0] = num06 * 8 + num08;
#ifdef DEBUG
Serial.print(F("69-Primo pixel = "));
Serial.println(posiz[0]);
#endif
      //occorre controllare che i 3 elementi consecutivi che costituiscono il
      //cacciatorpediniere non compaiano nella matrice memNavi (12 elementi);
      bool flag = false;
      for (int i=0; i<3; i++) {
        posiz[i] = posiz[0] + (i*8);
        if (VerificaNumero(posiz[i])) {   //verifica numero
#ifdef DEBUG
Serial.print(F("5-Posizione  "));
Serial.print(posiz[i]);
Serial.println(F(" già usata."));
#endif
          flag = true;
        }
      }
      if (flag) {
        continue;
      }
      //i 3 numeri sono validi; memorizza posizioni nelle prime posizioni vuote
      //della matrice memNavi; non è possibile che non ci siano posizioni vuote.
      for (int i = 0; i < 3; i++) { //per ciascuno delle 3 posizioni
        for (int j = 0; j < 12; j++) { //scandisci la matrice
          if (memNavi[j] == 0) {    //al primo elemento vuoto
            memNavi[j] = posiz[i];  //memorizza posizione
            break;                  //vai alla posizione successiva
          }
        }
      }
      break;    //esce dalla loop for (;;)
    }  //FINE posizionamento verticale
  }  //FINE for (;;)
}


//---------------------------------------------------------------------------
//Questa funzione posiziona una motovedetta (2 pixel)
void PosizionaMotovedetta () {
#ifdef DEBUG
Serial.println(F("73-Posiziona motovedetta"));
#endif
  for (;;) {
    //Scelta se posizionamento orizzontale (=0) o verticale (=1)
    num02 = random(0, 2);
    //Posizionamento orizzontale
    if (num02 == 0) {
#ifdef DEBUG
Serial.println(F("74-Posizionamento orizzontale"));
#endif
      num08 = random(0, 8);   //num08 = numero riga
#ifdef DEBUG
Serial.print(F("75-Scelta riga = "));
Serial.println(num08);
#endif
      //ricavo numero iniziale della riga scelta (0,8,16,24,32,40,48,56)
      num08 = num08 * 8;      //numero iniziale della riga
      //selezione numero iniziale posizionamento motovedetta nella riga scelta
      num07 = random(0, 7);   //posizione primo numero nella riga
      //ricavo posizione primo elemento della motovedetta
      posiz[0] = num08 + num07; //posizione primo elemento
#ifdef DEBUG
Serial.print(F("76-Primo pixel = "));
Serial.println(posiz[0]);
#endif
      //occorre controllare che i 2 elementi consecutivi che costituiscono la
      //motovedetta non compaiano nella matrice memNavi (12 elementi);
      if (VerificaNumero(posiz[0])) {   //verifica primo numero
#ifdef DEBUG
Serial.print(F("77- "));
Serial.print(posiz[0]);
Serial.println(F(" già usato. Scartato"));
#endif
        continue;
      }
      posiz[1] = posiz[0] + 1;
      if (VerificaNumero(posiz[1])) {   //verifica secondo numero
#ifdef DEBUG
Serial.print(F("78- "));
Serial.print(posiz[1]);
Serial.println(F(" già usato. Scartato"));
#endif
        continue;
      }
      //i 2 numeri sono validi; memorizza posizioni nelle prime posizioni vuote
      //della matrice memNavi; non è possibile che non ci siano posizioni vuote.
      for (int i = 0; i < 2; i++) { //per ciascuno delle 2 posizioni
        for (int j = 0; j < 12; j++) { //scandisci la matrice
          if (memNavi[j] == 0) {    //al primo elemento vuoto
            memNavi[j] = posiz[i];  //memorizza posizione
            break;                  //vai alla posizione successiva
          }
        }
      }
      break;    //esce dalla loop for (;;)
    }  //FINE posizionamento orizzontale
    //Posizionamento verticale
    if (num02 == 1) {
#ifdef DEBUG
Serial.println(F("79-Posizionamento verticale"));
#endif
      num08 = random(0, 8);   //num08 = numero colonna (0-7)
#ifdef DEBUG
Serial.print(F("80-Scelta colonna = "));
Serial.println(num08);
#endif
      num07 = random(0, 7);   //posizione primo numero nella colonna
      //ricavo posizione primo elemento della motovedetta
      posiz[0] = num02 * 8 + num08;
#ifdef DEBUG
Serial.print(F("81-Primo pixel = "));
Serial.println(posiz[0]);
#endif
      //occorre controllare che i 3 elementi consecutivi che costituiscono la
      //motovedetta non compaiano nella matrice memNavi (12 elementi);
      if (VerificaNumero(posiz[0])) {   //verifica primo numero
#ifdef DEBUG
Serial.print(F("82- "));
Serial.print(posiz[0]);
Serial.println(F(" già usato. Scartato"));
#endif
        continue;
      }
      posiz[1] = posiz[0] + 8;
      if (VerificaNumero(posiz[1])) {   //verifica secondo numero
#ifdef DEBUG
Serial.print(F("83- "));
Serial.print(posiz[1]);
Serial.println(F(" già usato. Scartato"));
#endif
        continue;
      }
      //i 2 numeri sono validi; memorizza posizioni nelle prime posizioni vuote
      //della matrice memNavi; non è possibile che non ci siano posizioni vuote.
      for (int i = 0; i < 2; i++) { //per ciascuno delle 2 posizioni
        for (int j = 0; j < 12; j++) { //scandisci la matrice
          if (memNavi[j] == 0) {    //al primo elemento vuoto
            memNavi[j] = posiz[i];  //memorizza posizione
            break;                  //vai alla posizione successiva
          }
        }
      }
      break;    //esce dalla loop for (;;)
    }  //FINE posizionamento verticale
  }  //FINE for (;;)
}


//---------------------------------------------------------------------------
//Questa funzione sposta il cursore secondo quattro direzioni: su, giu, a
//destra e a sinistra
boolean MuoviCursore(int campo) {

  bool redraw = false;

  //Verifica pressione pulsante SU
  if (digitalRead(SU) == LOW) {
    delay(30);  //antirmbalzo
//    lcd.setCursor(0,3);
//    lcd.print(F("SU"));
//    RipristinaPixel(campo);
    redraw = true;
    //sposta il cursore in su di una posizione
    currLed = currLed + 8;
    //se il cursore è oltre il limite superiore
    if (currLed > 63) {
      //riportalo al primo pixel in basso
      currLed = currLed % 64;
    }
    //accendi cursore in posizione corrente
//    WS.setPixelColor(currLed, WS.Color(cuR, cuG, cuB));
//    WS.show();
    //blocca il pulsante fino al rilascio
    while (digitalRead(SU) == LOW) ;
    delay(30);  //antirimbalzo
  }
  //Verifica pressione pulsante GIU
  if (digitalRead(GIU) == LOW) {
    delay(30);  //antirmbalzo
//    lcd.setCursor(0,3);
//    lcd.print(F("GIU"));
//    RipristinaPixel(campo);
    redraw = true;
    //sposta il cursore in giu di una posizione
    currLed = currLed - 8;
    //se il cursore è oltre il limite inferiore
    if (currLed < 0) {
      //riportalo al primo pixel in alto
      currLed = currLed + 64;
    }
    //accendi cursore in posizione corrente
//    WS.setPixelColor(currLed, WS.Color(cuR, cuG, cuB));
//    WS.show();
    //blocca il pulsante fino al rilascio
    while (digitalRead(GIU) == LOW) ;
    delay(30);  //antirimbalzo
  }
  //Verifica pressione pulsante A DESTRA
  if (digitalRead(DST) == LOW) {
//    lcd.setCursor(0,3);
//    lcd.print(F("A DESTRA  "));
    delay(30);  //antirmbalzo
//    RipristinaPixel(campo);
    redraw = true;
    //se il cursore è sull'ultimo pixel a destra
    if (currLed % 8 == 0) {
      //portalo al primo pixel a sinistra
      currLed = currLed + 7;
    }
    //altrimenti spostalo a destra di una posizione
    else {
      currLed = currLed - 1;
    }
    //Accendi cursore in posizione corrente
//    WS.setPixelColor(currLed, WS.Color(cuR, cuG, cuB));
//    WS.show();
    //blocca il pulsante fino al rilascio
    while (digitalRead(DST) == LOW) ;
    delay(30);  //antirimbalzo
  }
  //Verifica pressione pulsante A SINISTRA
  if (digitalRead(SIN) == LOW) {
//    lcd.setCursor(0,3);
//    lcd.print(F("A SINISTRA"));
    delay(30);  //antirmbalzo
//    RipristinaPixel(campo);
    redraw = true;
    //se il cursore è sull'ultimo pixel a sinistra
    if (currLed % 8 == 7) {
      //portalo al primo pixel a destra
      currLed = currLed - 7;
    }
    //altrimenti spostalo a sinistra di una posizione
    else {
      currLed = currLed + 1;
    }
    //Accendi cursore in posizione corrente
//    WS.setPixelColor(currLed, WS.Color(cuR, cuG, cuB));
//    WS.show();
    //blocca il pulsante fino al rilascio
    while (digitalRead(SIN) == LOW) ;
    delay(30);  //antirimbalzo
  }

if (redraw == true)
{
//  send_bf(1);
//  send_bf(2);

  draw_bf(0);
  int c = currLed % 8;
  int r = currLed / 8;
//  Serial.print(F(" c="));
//  Serial.print(c);
//  Serial.print(F(" r="));
//  Serial.println(r);
  lcd.setCursor(c,r);
  lcd.print(F("@"));
  redraw = false;
}

  if (digitalRead(GO) == LOW) {
    delay(30);
    lcd.setCursor(9,3);
    lcd.print(F("- GO! -"));
    return false;
  }
  return true;
}

//---------------------------------------------------------------------------
//Questa funzione estrae un colpo casuale dall'array celle[]
void EstraiColpo(int ind)
{
  uint8_t casuale;

  casuale = random(0, fine);
  estratto = cella[ind][casuale];
  cella[ind][casuale] = cella[ind][fine - 1];
  fine = fine - 1;
  if (fine == 0)
  {
    fine = 16;
    lista = lista+1;
  }
}


//---------------------------------------------------------------------------
//Questa funzione controlla che i pixel adiacenti a quello sparabile non siano
//tutti già sparati (av) o già colpiti (as)
boolean ControllaPixelAdiacenti(int pixel, byte* bFX)
{
Serial.print("in ControllaPixelAdiacenti...");
//           Serial.print(bFX[pixel], DEC);
//           Serial.print(" +1= ");
//           Serial.print(bFX[pixel+1], DEC);
//           Serial.print(" -1= ");
//           Serial.print(bFX[pixel-1], DEC);
//           Serial.print(" -8= ");
//           Serial.println(bFX[pixel-8], DEC);

  //SE pixel è 0
  if (pixel == 0) {
    if ((bFX[1] == av || bFX[1] == as) &&
        (bFX[8] == av || bFX[8] == as)) {
      Serial.println("A");
      return NONOK;
    }
  }
  //SE pixel è 7
  else if (pixel == 7) {
    if ((bFX[6] == av || bFX[6] == as) &&
        (bFX[15] == av || bFX[15] == as)) {
      Serial.println("B");
      return NONOK;
    }
  }
  //SE pixel è 56
  else if (pixel == 56) {
    if ((bFX[48] == av || bFX[48] == as) &&
        (bFX[57] == av || bFX[57] == as)) {
      Serial.println("C");
      return NONOK;
    }
  }
  //SE pixel è 63
  else if (pixel == 63) {
    if ((bFX[55] == av || bFX[55] == as) &&
        (bFX[62] == av || bFX[62] == as)) {
      Serial.println("D");
      return NONOK;
    }
  }
  //SE pixel è >56 e <63 (linea di base)
  else if (pixel > 56 && pixel < 63)
  {
    if ((bFX[pixel+1] == av || bFX[pixel+1] == as) &&
        (bFX[pixel-1] == av || bFX[pixel-1] == as) &&
        (bFX[pixel-8] == av || bFX[pixel-8] == as)) {
      Serial.println("E");
      return NONOK;
    }
  }
  //SE pixel è >0 e <7 (linea di testa)
  else if (pixel > 0 && pixel < 7) {
    if ((bFX[pixel+1] == av || bFX[pixel+1] == as) &&
        (bFX[pixel-1] == av || bFX[pixel-1] == as) &&
        (bFX[pixel+8] == av || bFX[pixel+8] == as)) {
      Serial.println("F");
      return NONOK;
    }
  }
  //SE pixel è su colonna sinistra escluso 7 e 63 già testati
  else if (pixel %8 == 7) {
    if ((bFX[pixel+8] == av || bFX[pixel+8] == as) &&
        (bFX[pixel-8] == av || bFX[pixel-8] == as) &&
        (bFX[pixel-1] == av || bFX[pixel-1] == as)) {
      Serial.println("G");
      return NONOK;
    }
  }
  //SE pixel è su colonna destra escluso 0 e 56 già testati
  else if (pixel %8 == 0) {
    if ((bFX[pixel+8] == av || bFX[pixel+8] == as) &&
        (bFX[pixel-8] == av || bFX[pixel-8] == as) &&
        (bFX[pixel-1] == av || bFX[pixel-1] == as)) {
      Serial.println("H");
      return NONOK;
    }
  }

  Serial.println("OK");
  return OK;
}


//---------------------------------------------------------------------------
//Questa funzione calcola le adiacenze di un colpo a segno e popola la matrice
//adia[] (nell'ordine DX, UP, SX, DWN) SOLO con le adiacenze sparabili
void CalcolaAdiacenze(int val, byte* bFX)
{
  //Adiacenza a destra
  if (val % 8 != 0) {    //se NON siamo sull'ultima colonna di destra
    for (int i=0; i<4; i++) {
      if (adia[i] == 0) {
        adia[i] = val - 1;
#ifdef DEBUG_avv
Serial.print(F("84-Adiacenza n. "));
Serial.print(i);
Serial.print(F(" pixel "));
Serial.println(val-1);
#endif
        break;
      }
    }
  }
  //Adiacenza di sopra
  if (val / 8 != 7) {   //se NON siamo sull'ultima riga di sopra
    for (int i=0; i<4; i++) {
      if (adia[i] == 0) {
        adia[i] = val + 8;
#ifdef DEBUG_avv
Serial.print(F("86-Adiacenza n. "));
Serial.print(i);
Serial.print(F(" pixel "));
Serial.println(val+8);
#endif
        break;
      }
    }
  }
  //Adiacenza a sinistra
  if (val % 8 != 7) {    //se NON siamo sull'ultima colonna di sinistra
    for (int i=0; i<4; i++) {
      if (adia[i] == 0) {
        adia[i] = val + 1;
#ifdef DEBUG_avv
Serial.print(F("85-Adiacenza n. "));
Serial.print(i);
Serial.print(F(" pixel "));
Serial.println(val+1);
#endif
        break;
      }
    }
  }
  //Adiacenza di sotto
  if (val / 8 != 0) {   //se NON siamo sull'ultima riga di sotto
    for (int i=0; i<4; i++) {
      if (adia[i] == 0) {
        adia[i] = val - 8;
#ifdef DEBUG_avv
Serial.print(F("87-Adiacenza n. "));
Serial.print(i);
Serial.print(F(" pixel "));
Serial.println(val-8);
#endif
        break;
      }
    }
  }
  //Spunta adiacenze già colpite (as) o già sparate (av)
  for (int i=0; i<4; i++) {
    if (bFX[adia[i]] == as || bFX[adia[i]] == av) {
#ifdef DEBUG_avv
Serial.print(F("88-Cancellata adiacenza n. "));
Serial.print(i);
Serial.print(F(" pixel "));
Serial.println(adia[i]);
#endif
      adia[i] = 0;
    }
  }
}


//---------------------------------------------------------------------------
//Questa funzione visualizza il display a vuoto Avverso
void DispAvuoto(int curs, byte* bFX)
{
  lcd.setCursor(0,3);
  lcd.print(F("colpo a vuoto"));
  delay(1000);

  //cambio condizione pixel da fp a av
  bFX[curs] = av;
}


//---------------------------------------------------------------------------
//Questa funzione visualizza il display a segno Avverso
void DispAsegno(int curs, byte* bFX)
{
  lcd.setCursor(0,3);
  lcd.print(F("colpo a segno"));
  delay(1000);

  //cambio condizione pixel da pn a as
  bFX[curs] = as;
}


//---------------------------------------------------------------------------
//Questa funzione calcola i limiti di una nave orizzontale
void CalcHitOrizz(byte* bFX)
{
  //se minLim NON è in colonna destra e il limite è sparabile
  if (minLim % 8 != 0 && bFX[minLim-1] != as &&
                         bFX[minLim-1] != av) {
    //piazza limite destro
    minHit = minLim - 1;
  }
  else minHit = 0;
  //se maxLim NON è in colonna sinistra e il limite è sparabile
  if (maxLim % 8 != 7 && bFX[maxLim+1] != as &&
                         bFX[maxLim+1] != av) {
    //piazza limite sinistro
    maxHit = maxLim + 1;
  }
  else maxHit = 0;
}


//---------------------------------------------------------------------------
//Questa funzione calcola i limiti di una nave verticale
//Qui si arriva con adia[0] e adia[1] già azzarati
void CalcHitVert(byte* bFX)
{
  //se maxLim NON è sulla riga superiore
  //e il pixel è sparabile
  if (maxLim < 56 && bFX[maxLim+8] != as &&
                     bFX[maxLim+8] != av) {
    //piazza limite superiore
    maxHit = maxLim + 8;
  }
  else maxHit = 0;
  //se minLim NON è sulla riga inferiore
  //e il pixel è sparabile
  if (minLim > 7 && bFX[minLim-8] != as &&
                    bFX[minLim-8] != av) {
    //piazza limite inferiore
    minHit = minLim - 8;
  }
  else minHit = 0;
}


//---------------------------------------------------------------------------
//Questa funzione sposta una nave di lunghezza nPix all'indirizzo *nave nella
//direzione dir SU, GIU, DST, SIN.
void SpostaNave(int *nave, int nPix, int dir)
{
  toggleGO = PIAZZA;

  //ripristina pixel (pm o pn)
  for (int i=0; i<nPix; i++)
  {
    if (bFM[*(nave+i)] == pm) {
        bFM[*(nave+i)] = pm;
    }
    if (bFM[*(nave+i)] == pn) {
        bFM[*(nave+i)] = pn;
    }
  }

  //ridisegna schermo
  draw_bf(1);

  //muovi nave in nuova posizione
  for (int i=0; i<nPix; i++)
  {
    if (dir == SU) *(nave+i) = *(nave+i) + 8;
    if (dir == GIU) *(nave+i) = *(nave+i) - 8;
    if (dir == DST) *(nave+i) = *(nave+i) - 1;
    if (dir == SIN) *(nave+i) = *(nave+i) + 1;

    int c = *(nave+i) % 8;
    int r = *(nave+i) / 8;
    Serial.print(F(" c="));
    Serial.print(c);
    Serial.print(F(" r="));
    Serial.println(r);
    lcd.setCursor(c,r);
    lcd.print(F("N"));
  }
}


//---------------------------------------------------------------------------
//Questa funzione verifica se il numero (num) è contenuto nella matrice memNavi
//Restituisce true se il numero è contenuto e false in caso contrario
//Impedisce il posizionamento di navi nel pixel 35
boolean VerificaNumero(int num) {
  if (num == 35) {
    return true;
  }
  for (int i = 0; i < 12; i++) {
    if (num == memNavi[i]) {
      return true;
    }
  }
  return false;
}


