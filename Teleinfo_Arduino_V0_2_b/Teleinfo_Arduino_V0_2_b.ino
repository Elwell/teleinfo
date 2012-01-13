/*
              Datalogger Téléinfo 2 compteurs sur Arduino

              Compteur 1: consommation
              Compteur 2: production solaire en tarif BASE
              
              Juin 2011:
              v0.2  * passage automatique à l'heure d'été
                    * correctif erreur abo BBR
                    * modification ecriture sur SD (utilisation de teleinfoFile.print à la place s'une variable STRING
                      qui plante l'Arduino avec les abonnements BBR
              v0.2a * ajout mode sans puissance apparente pour ancien compteur et calcul de celle-ci pour les logiciels d'analyse
              v0.2b * pour Arduino 1.0 (mise à jour de la RTC et utilisation de la librairie SD livrée avec la 1.0)              
*/

char version_logiciel[5] = "V0.2";

#include "print.h"
#include "string.h"
#include "stdio.h"
#include <SD.h>
#include <Wire.h>
#include "RTClib.h"

// ** sélectionnez votre abonnement **
//
//#define abo_BASE  // abonnement de Base
#define abo_HCHP  // abonnement Heures Creuses
//#define abo_EJP   // abonnement EJP
//#define abo_BBR   // abonnement tempo (Bleu Blanc Rouge)

#define MONOPHASE
//#define TRIPHASE

//#define compteur2_actif  // active la lecture du compteur 2

#define echo_USB            //envoie toutes les trames téléinfo sur l'USB
//#define messages_USB        //envoie seulement les valeurs sélectionnées de l'abonnement sur l'USB
#define message_système_USB //envoie des messages sur l'USB (init SD, heure au demarrage, et echo des erreures)

// suivant la version de votre compteur (anciennetée) certaines infos ne sont pas dispo,
// alors le logiciel les calculs
//#define sans_Puissance_Apparente

//*****************************************************************************************
  byte inByte = 0 ;        // caractère entrant téléinfo
  char buffteleinfo[21] = "";
  byte bufflen = 0;
  byte mem_sauv_minute = 1;
  byte mem_sauv_journee = 1;
  
// declarations Teleinfo
//  char adco[13]= "";      // numero ADCO compteur consommation
//  char optarif[5]= "";    // Option tarifaire choisie: BASE => Option Base, HC.. => Option Heures Creuses, EJP. => Option EJP, BBRx => Option Tempo [x selon contacts auxiliaires]
//  byte isousc = 0;        // Intensité souscrite, A
  unsigned int papp = 0;  // Puissance apparente, VA
// char MOTDETAT[10] = "";  // Mot d'état du compteur

//  char ptec[4] = "";      // Période Tarifaire en cours, 4 alphanumériques
// TH.. => Toutes les Heures
// HC.. => Heures Creuses
// HP.. => Heures Pleines
// HN.. => Heures Normales
// PM.. => Heures de Pointe Mobile
// HCJB => Heures Creuses Jours Bleus
// HCJW => Heures Creuses Jours Blancs
// HCJR => Heures Creuses Jours Rouges
// HPJB => Heures Pleines Jours Bleus
// HPJW => Heures Pleines Jours Blancs
// HPJR => Heures Pleines Jours Rouges

  // monophasé
#ifdef MONOPHASE
  unsigned int iinst = 0;      // Monophasé - Intensité Instantanée, A  (intensité efficace instantanée)
//  unsigned int imax = 0;  // Monophasé - Intensité maximale appelée, A
#endif  

  // triphasé
#ifdef TRIPHASE
//  unsigned long ADPS = 0;   // Avertissement de Dépassement Puissance Souscrite, A (n'est émis que lorsque la puissance consommée dépasse la puissance souscrite - intensité efficace instantanée)
  unsigned int IINST1 = 0; // Triphasé - Intensité Instantanée Phase 1, A  (intensité efficace instantanée)
  unsigned int IINST2 = 0; // Triphasé - Intensité Instantanée Phase 2, A  (intensité efficace instantanée)
  unsigned int IINST3 = 0; // Triphasé - Intensité Instantanée Phase 3, A  (intensité efficace instantanée)
//  unsigned int IMAX1 = 0;  // Triphasé - Intensité maximale appelée Phase 1, A
//  unsigned int IMAX2 = 0;  // Triphasé - Intensité maximale appelée Phase 2, A
//  unsigned int IMAX3 = 0;  // Triphasé - Intensité maximale appelée Phase 3, A
//  byte PPOT = 0;  // Triphasé - Présence des potentiels, (code hexa, voir specification EDF)
//  unsigned int PMAX = 0; // Triphasé - Puissance maximale triphasée atteinte, W

  // Uniquement suite à un dépassement d'intensité de réglage sur l'une des trois phases
//  unsigned int ADIR1 = 0;  // Triphasé - Avertissement de Dépassement d'intensité de réglage par phase - Phase 1, A
//  unsigned int ADIR2 = 0;  // Triphasé - Avertissement de Dépassement d'intensité de réglage par phase - Phase 2, A
//  unsigned int ADIR3 = 0;  // Triphasé - Avertissement de Dépassement d'intensité de réglage par phase - Phase 2, A
#endif

  // abonnement Base
#ifdef abo_BASE
  unsigned long base = 0;      // Index option Base, Wh
#endif
    
  // abonnement Heures Creuses
#ifdef abo_HCHP
  unsigned long hchc = 0;
  unsigned long hchp = 0;
//  char HHPHC[1] = ""; // Horaire Heures Pleines Heures Creuses: A, C, D, E ou Y selon programmation du compteur
#endif

  // abonnement EJP
#ifdef abo_EJP
  unsigned long EJPHN = 0;   // Index option EJP - Heures Normales, Wh
  unsigned long EJPHPM = 0;  // Index option EJP - Heures de Pointe Mobile, Wh
//  unsigned long PEJP = 0; // Option EPJ - Préavis Début EJP (30 min), min (pendant toute la période de préavis et pendant la période de pointe mobile, valeur fixe à 30)
#endif
  
  // abonnement TEMPO Bleu Blanc Rouge
#ifdef abo_BBR
  unsigned long BBRHCJB = 0;    // Index option Tempo - Heures Creuses Jours Bleus, Wh
  unsigned long BBRHPJB = 0;    // Index option Tempo - Heures Pleines Jours Bleus, Wh
  unsigned long BBRHCJW = 0;    // Index option Tempo - Heures Creuses Jours Blancs, Wh
  unsigned long BBRHPJW = 0;    // Index option Tempo - Heures Pleines Jours Blancs, Wh
  unsigned long BBRHCJR = 0;    // Index option Tempo - Heures Creuses Jours Rouges, Wh
  unsigned long BBRHPJR = 0;    // Index option Tempo - Heures Pleines Jours Rouges, Wh
//  unsigned long DEMAIN = 0; // Option Tempo - Couleur du lendemain, ---- : non connue, BLEU : jour BLEU, BLAN : jour BLANC, ROUG : jour ROUGE
#endif

  // compteur 2 (solaire configuré en tarif BASE par ERDF)
#ifdef compteur2_actif
  unsigned long cpt2index = 0; // Index option Base compteur production solaire, Wh
  unsigned long cpt2puissance = 0;  // Puissance apparente compteur production solaire, VA
  unsigned long cpt2intensite = 0; // Monophasé - Intensité Instantanée compteur production solaire, A  (intensité efficace instantanée)
#endif

const char debtrame = 0x02;
const char deb = 0x0A;
const char fin = 0x0D;

// *************** déclaration carte micro SD ******************
const byte chipSelect = 4;

// *************** déclaration activation compteur 1 ou 2 ******
#define LEC_CPT1 5  // lecture compteur 1
#define LEC_CPT2 6  // lecture compteur 2

int compteur_actif = 1;  // numero du compteur en cours de lecture
byte donnee_ok_cpt1 = 0;  // pour vérifier que les donnees sont bien en memoire avant ecriture dans fichier
byte donnee_ok_cpt2 = 0;
byte donnee_ok_cpt1_ph = 0;

// *************** variables RTC ************************************
int minute, heure, seconde, jour, mois, annee, jour_semaine;
char date_heure[18];
char mois_jour[7];
byte mem_chg_heure = 0; //pour pas passer perpetuellement de 3h à 2h du matin le dernier dimanche d'octobre
RTC_DS1307 RTC;

// ************** initialisation *******************************
void setup() 
{
 // initialisation du port 0-1 lecture Téléinfo
    Serial.begin(1200);
      // parité paire E
      // 7 bits data
    UCSR0C = B00100100;
#ifdef message_système_USB
    Serial.print("-- Teleinfo USB Arduino ");
    Serial.print(version_logiciel);
    Serial.println(" --");
#endif
 // initialisation des sorties selection compteur
    pinMode(LEC_CPT1, OUTPUT);
    pinMode(LEC_CPT2, OUTPUT);
    digitalWrite(LEC_CPT1, HIGH);
    digitalWrite(LEC_CPT2, LOW);
             
 // verification de la présence de la microSD et si elle est initialisée:
#ifdef message_système_USB
    if (!SD.begin(chipSelect)) {
      Serial.println("> Erreur carte, ou carte absente !");
      return;
     }
    Serial.println("> microSD initialisee !");
#endif

 // initialisation RTC
    Wire.begin();
    RTC.begin();

#ifdef message_système_USB
    if (! RTC.isrunning()) {
      Serial.println("RTC non configure !");
      // décommenter la ligne suivante si vous voulez programmer l'horloge de la carte à l'heure du PC
      // c'est l'heure à laquelle le code est envoyée à la carte
      RTC.adjust(DateTime(__DATE__, __TIME__));
    }
#endif

    DateTime now = RTC.now();     // lecture de l'horloge
    annee = now.year();
    mois = now.month();
    jour = now.day();
    heure = now.hour();
    minute = now.minute();
    jour_semaine = now.dayOfWeek();

    format_date_heure();
#ifdef message_système_USB
    Serial.println(date_heure);
#endif
}

// ************** boucle principale *******************************

void loop()                     // Programme en boucle
{
  DateTime now = RTC.now();     // lecture de l'horloge
  minute = now.minute();
  heure = now.hour();
  seconde = now.second();

  if ((heure == 0) and (minute == 0) and (seconde ==0))
  {
    annee = now.year();
    mois = now.month();
    jour = now.day();
    jour_semaine = now.dayOfWeek();
  }

  // passage à l'heure d'été +1 heure
  // la lib RTC a une fonction: dayOfWeek qui donne le jour de la semaine (la DS1307 se charge de tout !)
  // réponse: 0 -> dimanche, 1 -> lundi etc...
  //
  if ((heure == 2) and (minute == 0) and (seconde == 0) and (jour_semaine == 0) and (mois == 3) and (jour > 24))
  {
    heure = 3;
    RTCsetTime();
  }

  // passage à l'heure d'hiver -1 heure
  if ((heure == 3) and (minute == 0) and (seconde == 0) and (jour_semaine == 0) and (mois == 10) and (jour > 24) and (mem_chg_heure == 0))
  {
    heure = 2;
    RTCsetTime();
    mem_chg_heure = 1;
  }

  if ((heure == 23) and (minute == 59) and (seconde ==10)) // pour être sur de pas tomber pendant l'enregistrement toutes les minutes
  {
    if (mem_sauv_journee == 0) // un seul enregistrement par jour !
    {
      fichier_annee();
    }
    ++mem_sauv_journee;
    mem_chg_heure = 0;  
  }
  else mem_sauv_journee = 0;

  if (seconde == 1) 
  {
    if (mem_sauv_minute == 0) // un seul enregistrement par minute !
    {
      enregistre();
    }
    ++mem_sauv_minute;
  }
  else mem_sauv_minute = 0;

#ifdef compteur2_actif
  #ifdef abo_BASE
    if (donnee_ok_cpt1 == B00000001) {
  #endif
  #ifdef abo_HCHP or abo_EJP
    if (donnee_ok_cpt1 == B00000011) {
  #endif
  #ifdef abo_EJP
    if (donnee_ok_cpt1 == B00000011) {
  #endif
  #ifdef abo_BBR
    if (donnee_ok_cpt1 == B00111111) {
  #endif
  #ifdef MONOPHASE
      if (donnee_ok_cpt1_ph == B10000001) bascule_compteur();
  #endif
  #ifdef TRIPHASE
      if (donnee_ok_cpt1_ph == B10000111) bascule_compteur();
  #endif
    }
  if (donnee_ok_cpt2 == B00000111) bascule_compteur();
#endif

  read_teleinfo();

}

///////////////////////////////////////////////////////////////////
// Calcul Checksum teleinfo
///////////////////////////////////////////////////////////////////
char chksum(char *buff, int len)
{
  int i;
  char sum = 0;
    for (i=1; i<(len-2); i++) sum = sum + buff[i];
    sum = (sum & 0x3F) + 0x20;
    return(sum);
}

///////////////////////////////////////////////////////////////////
// Analyse de la ligne de Teleinfo
///////////////////////////////////////////////////////////////////
void traitbuf_cpt(char *buff, int len)
{

if (compteur_actif == 1){

#ifdef abo_BASE
  if (strncmp("BASE ", &buff[1] , 5)==0){
      base = atol(&buff[6]);
      donnee_ok_cpt1 = donnee_ok_cpt1 | B00000001;
  #ifdef messages_USB
      Serial.print("- Index BASE: "); Serial.println(base,DEC);
  #endif        
  }
#endif

#ifdef abo_HCHP
  if (strncmp("HCHP ", &buff[1] , 5)==0){
      hchp = atol(&buff[6]);
      donnee_ok_cpt1 = donnee_ok_cpt1 | B00000001;
  #ifdef messages_USB
      Serial.print("- Index Heures Pleines: "); Serial.println(hchp,DEC);
  #endif        
  }

  else if (strncmp("HCHC ", &buff[1] , 5)==0){
      hchc = atol(&buff[6]);
      donnee_ok_cpt1 = donnee_ok_cpt1 | B00000010;
  #ifdef messages_USB
      Serial.print("- Index Heures Creuses: ");Serial.println(hchc,DEC);
  #endif        
  }
#endif

#ifdef abo_EJP
  if (strncmp("EJPHN ", &buff[1] , 6)==0){
      EJPHN = atol(&buff[7]);
      donnee_ok_cpt1 = donnee_ok_cpt1 | B00000001;
  #ifdef messages_USB
      Serial.print("- Index Heures Normales: "); Serial.println(EJPHN,DEC);
  #endif        
  }

  else if (strncmp("EJPHPM ", &buff[1] , 7)==0){
      EJPHPM = atol(&buff[8]);
      donnee_ok_cpt1 = donnee_ok_cpt1 | B00000010;
  #ifdef messages_USB
      Serial.print("- Index Heures Pointes Mobiles: ");Serial.println(EJPHPM,DEC);
  #endif        
  }
#endif

#ifdef abo_BBR
  if (strncmp("BBRHCJB ", &buff[1] , 8)==0){
      BBRHCJB = atol(&buff[9]);
      donnee_ok_cpt1 = donnee_ok_cpt1 | B00000001;
  #ifdef messages_USB
      Serial.print("- Heures Creuses Bleu: "); Serial.println(BBRHCJB,DEC);
  #endif        
  }
  else if (strncmp("BBRHPJB ", &buff[1] , 8)==0){
      BBRHPJB = atol(&buff[9]);
      donnee_ok_cpt1 = donnee_ok_cpt1 | B00000010;
  #ifdef messages_USB
      Serial.print("- Heures Pleines Bleu: ");Serial.println(BBRHPJB,DEC);
  #endif        
  }
  else if (strncmp("BBRHCJW ", &buff[1] , 8)==0){
      BBRHCJW = atol(&buff[9]);
      donnee_ok_cpt1 = donnee_ok_cpt1 | B00000100;
  #ifdef messages_USB
      Serial.print("- Heures Creuses Blanc: "); Serial.println(BBRHCJW,DEC);
  #endif        
  }

  else if (strncmp("BBRHPJW ", &buff[1] , 8)==0){
      BBRHPJW = atol(&buff[9]);
      donnee_ok_cpt1 = donnee_ok_cpt1 | B00001000;
  #ifdef messages_USB
      Serial.print("- Heures Pleines Blanc: ");Serial.println(BBRHPJW,DEC);
  #endif        
  }
  else if (strncmp("BBRHCJR ", &buff[1] , 8)==0){
      BBRHCJR = atol(&buff[9]);
      donnee_ok_cpt1 = donnee_ok_cpt1 | B00010000;
  #ifdef messages_USB
      Serial.print("- Heures Creuses Rouge: "); Serial.println(BBRHCJR,DEC);
  #endif        
  }
  else if (strncmp("BBRHPJR ", &buff[1] , 8)==0){
      BBRHPJR = atol(&buff[9]);
      donnee_ok_cpt1 = donnee_ok_cpt1 | B00100000;
  #ifdef messages_USB
      Serial.print("- Heures Pleines Rouge: ");Serial.println(BBRHPJR,DEC);
  #endif        
  }
#endif

#ifdef MONOPHASE
  else if (strncmp("IINST ", &buff[1] , 6)==0){ 
      iinst = atol(&buff[7]);
      donnee_ok_cpt1_ph = donnee_ok_cpt1_ph | B00000001;
  #ifdef messages_USB
      Serial.print("- I iNStantannee : "); Serial.println(iinst,DEC);
  #endif 
  #ifdef sans_Puissance_Apparente
    papp = iinst * 240;
    donnee_ok_cpt1_ph = donnee_ok_cpt1_ph | B10000000;    
  #endif  
  }
#endif

#ifdef TRIPHASE
  else if (strncmp("IINST1 ", &buff[1] , 7)==0){ 
      IINST1 = atol(&buff[8]);
      donnee_ok_cpt1_ph = donnee_ok_cpt1_ph | B00000001;
  #ifdef messages_USB
      Serial.print("- I iNStantannee ph 1: "); Serial.println(IINST1,DEC);
  #endif        
  }
  else if (strncmp("IINST2 ", &buff[1] , 7)==0){ 
      IINST2 = atol(&buff[8]);
      donnee_ok_cpt1_ph = donnee_ok_cpt1_ph | B00000010;
  #ifdef messages_USB
      Serial.print("- I iNStantannee ph 2: "); Serial.println(IINST2,DEC);
  #endif        
  }
  else if (strncmp("IINST3 ", &buff[1] , 7)==0){ 
      IINST3 = atol(&buff[8]);
      donnee_ok_cpt1_ph = donnee_ok_cpt1_ph | B00000100;
  #ifdef messages_USB
      Serial.print("- I iNStantannee ph 3: "); Serial.println(IINST3,DEC);
  #endif        
  }
#endif

#ifndef sans_Puissance_Apparente
  else if (strncmp("PAPP ", &buff[1] , 5)==0){
      papp = atol(&buff[6]);
      donnee_ok_cpt1_ph = donnee_ok_cpt1_ph | B10000000;
  #ifdef messages_USB
      Serial.print("- Puissance apparente : ");Serial.println(papp,DEC);
  #endif        
  }
#endif

#ifdef sans_Puissance_Apparente
  if ((donnee_ok_cpt1_ph & B00000001) > 0){
      papp = iinst * 240;
      donnee_ok_cpt1_ph = donnee_ok_cpt1_ph | B10000000;
  #ifdef messages_USB
      Serial.print("- Puissance apparente : ");Serial.println(papp,DEC);
  #endif        
  }
#endif

/*
    else if (strncmp("ADCO ", &buff[1] , 5)==0){
        strncpy(adco, &buff[6], 12);
        adco[12]='\0';
#ifdef messages_USB
        Serial.print("- ADCO : "); Serial.println(adco);  // messages sur l'USB (choisir entre les messages ou l'echo des trames)
#endif        

    else if (strncmp("IMAX ", &buff[1] , 5)==0){
        imax = atol(&buff[6]);
#ifdef messages_USB
        Serial.print("- Intensite MAX : "); Serial.println(imax,DEC);
#endif        
      }

    else if (strncmp("OPTARIF ", &buff[1] , 8)==0){
        strncpy(optarif, &buff[9], 4);
        optarif[4]='\0';
#ifdef messages_USB
        Serial.print("- Abonnement : ");Serial.println(optarif);
#endif        
      }

    else if (strncmp("ISOUSC ", &buff[1] , 7)==0){
        isousc = atol(&buff[8]);
#ifdef messages_USB
        Serial.print("- I SOUScrite : "); Serial.println(isousc,DEC);
#endif        
      }

    else if (strncmp("PTEC ", &buff[1] , 5)==0){
        strncpy(ptec, &buff[6], 4);
        ptec[4]='\0';
#ifdef messages_USB
        Serial.print("- Periode Tarifaire En Cours : "); Serial.println(ptec);
#endif        
      }

    else if (strncmp("BASE ", &buff[1] , 5)==0){
        base = atol(&buff[6]);
#ifdef messages_USB
        Serial.print("- Index BASE : ");Serial.println(base,DEC);
#endif        
      }

    else if (strncmp("HHPHC ", &buff[1] , 6)==0){
        hhphc = buff[7];
#ifdef messages_USB
        Serial.print(" HHPHC ");Serial.println(hhphc);
#endif        
      }

    else if (strncmp("MOTDETAT ", &buff[1] , 9)==0){
        strncpy(motdetat, &buff[10], 6);
        motdetat[6]='\0';
#ifdef messages_USB
        Serial.print(" MOTDETAT "); Serial.println(motdetat);
#endif        
      }   
*/      
  }
    
#ifdef compteur2_actif
  if (compteur_actif == 2){
    if (strncmp("BASE ", &buff[1] , 5)==0){
      cpt2index = atol(&buff[6]);
      donnee_ok_cpt2 = donnee_ok_cpt2 | B00000001;
  #ifdef messages_USB
      Serial.print("- Index Solaire : "); Serial.println(cpt2index,DEC);
  #endif        
    }
    else if (strncmp("IINST ", &buff[1] , 6)==0){ 
      cpt2intensite = atol(&buff[7]);
      donnee_ok_cpt2 = donnee_ok_cpt2 | B00000010;
  #ifdef messages_USB
      Serial.print("- I INST Solaire : "); Serial.println(cpt2intensite,DEC);
  #endif        
    }
    else if (strncmp("PAPP ", &buff[1] , 5)==0){
      cpt2puissance = atol(&buff[6]);
      donnee_ok_cpt2 = donnee_ok_cpt2 | B00000100;
  #ifdef messages_USB
      Serial.print("- Puissance Solaire : "); Serial.println(cpt2puissance,DEC);
  #endif        
    }
  }
#endif

}

///////////////////////////////////////////////////////////////////
// Changement de lecture de compteur
///////////////////////////////////////////////////////////////////
void bascule_compteur()
{
  if (compteur_actif == 1)
 {
   digitalWrite(LEC_CPT1, LOW);
   digitalWrite(LEC_CPT2, HIGH);
   compteur_actif = 2;
 }
 else
 {
   digitalWrite(LEC_CPT1, HIGH);
   digitalWrite(LEC_CPT2, LOW);
   compteur_actif = 1;
 }

 donnee_ok_cpt1 = B00000000;
 donnee_ok_cpt2 = B00000000;
 donnee_ok_cpt1_ph = B00000000;
 bufflen=0;

// memset(buffteleinfo,0,21);  // vide le buffer lorsque l'on change de compteur
 Serial.flush(); 
 buffteleinfo[0]='\0';
 delay(500);
}

///////////////////////////////////////////////////////////////////
// Lecture trame teleinfo (ligne par ligne)
/////////////////////////////////////////////////////////////////// 
void read_teleinfo()
{
  // si une donnée est dispo sur le port série
  if (Serial.available() > 0) 
  {
  // recupère le caractère dispo
    inByte = Serial.read();

#ifdef echo_USB    
    Serial.print(char(inByte));  // echo des trames sur l'USB (choisir entre les messages ou l'echo des trames)
#endif

    if (inByte == debtrame) bufflen = 0; // test le début de trame
    if (inByte == deb) // test si c'est le caractère de début de ligne
    {
      bufflen = 0;
    }  
    buffteleinfo[bufflen] = inByte;
    bufflen++;
    if (bufflen > 21)bufflen=0;       // longueur max du buffer (21 pour lire trame ADCO)
    if (inByte == fin && bufflen > 5) // si Fin de ligne trouvée 
    {
       
      if (chksum(buffteleinfo,bufflen-1)== buffteleinfo[bufflen-2]) // Test du Checksum
      {
        traitbuf_cpt(buffteleinfo,bufflen-1); // ChekSum OK => Analyse de la Trame
      }
    }
  } 
}

///////////////////////////////////////////////////////////////////
// Enregistrement trame Teleinfo toutes les minutes
///////////////////////////////////////////////////////////////////
void enregistre()
{
  char fichier_journee[13];
  boolean nouveau_fichier = false;
 
  sprintf( fichier_journee, "TI-%02d-%02d.csv", mois,jour ); // nom du fichier court (8 caractères maxi . 3 caractères maxi)
  if (!SD.exists(fichier_journee)) nouveau_fichier = true;
  File teleinfoFile = SD.open(fichier_journee, FILE_WRITE);

  // si le fichier est bien ouvert-> écriture
  if (teleinfoFile) {
    if (nouveau_fichier)
    {
      #ifdef abo_BASE     
        teleinfoFile.print("date,hh:mm,BASE,"); 
      #endif
      #ifdef abo_HCHP
        teleinfoFile.print("date,hh:mm,HCHP,HCHC,"); 
      #endif
      #ifdef abo_EJP
        teleinfoFile.print("date,hh:mm,EJPHN,EJPHPM,"); 
      #endif
      #ifdef abo_BBR
        teleinfoFile.print("date,hh:mm,BBRHCJB,BBRHPJB,BBRHCJW,BBRHPJW,BBRHCJR,BBRHPJR,"); 
      #endif
      #ifdef MONOPHASE
        teleinfoFile.print("I A,P VA");
      #endif
      #ifdef TRIPHASE
        teleinfoFile.print("I ph1,ph2,ph3,P VA");
      #endif
      #ifdef compteur2_actif
        teleinfoFile.println(",Base cpt 2,I A cpt2,P VA cpt2");
      #else
        teleinfoFile.println("");
      #endif
    }
    format_date_heure();

    teleinfoFile.print(date_heure);
    teleinfoFile.print(",");
  
#ifdef abo_BASE
    teleinfoFile.print(base);
    teleinfoFile.print(",");
#endif
#ifdef abo_HCHP
    teleinfoFile.print(hchp);
    teleinfoFile.print(",");
    teleinfoFile.print(hchc);
    teleinfoFile.print(",");
#endif
#ifdef abo_EJP
    teleinfoFile.print(EJPHN);
    teleinfoFile.print(",");
    teleinfoFile.print(EJPHPM);
    teleinfoFile.print(",");
#endif
#ifdef abo_BBR
    teleinfoFile.print(BBRHCJB);
    teleinfoFile.print(",");
    teleinfoFile.print(BBRHPJB);
    teleinfoFile.print(",");
    teleinfoFile.print(BBRHCJW);
    teleinfoFile.print(",");
    teleinfoFile.print(BBRHPJW);
    teleinfoFile.print(",");
    teleinfoFile.print(BBRHCJR);
    teleinfoFile.print(",");
    teleinfoFile.print(BBRHPJR);
    teleinfoFile.print(",");
#endif

#ifdef MONOPHASE
    teleinfoFile.print(iinst);
    teleinfoFile.print(",");
#endif

#ifdef TRIPHASE
    teleinfoFile.print(IINST1);
    teleinfoFile.print(",");
    teleinfoFile.print(IINST2);
    teleinfoFile.print(",");
    teleinfoFile.print(IINST3);
    teleinfoFile.print(",");
#endif
    teleinfoFile.print(papp);

#ifdef compteur2_actif
    teleinfoFile.print(",");
    teleinfoFile.print(cpt2index);
    teleinfoFile.print(",");
    teleinfoFile.print(cpt2intensite);
    teleinfoFile.print(",");
    teleinfoFile.print(cpt2puissance);
#endif

    teleinfoFile.println("");
    teleinfoFile.flush(); 
    teleinfoFile.close();
  }
  
#ifdef message_système_USB
  // si le fichier ne peut pas être ouvert alors erreur !
  else {
    Serial.println("erreur ouverture fichier enregistrement journee");
  } 
#endif
}

///////////////////////////////////////////////////////////////////
// Enregistrement index journalier teleinfo dans fichier annuel
///////////////////////////////////////////////////////////////////
void fichier_annee()
{
  char fichier_annee[13];
  boolean nouveau_fichier = false;
  
  String dataCPT = "";
  sprintf( fichier_annee, "%dTELE.csv", annee ); // nom du fichier court (8 caractères maxi . 3 caractères maxi)
  if (!SD.exists(fichier_annee)) nouveau_fichier = true;
  File annetelefile = SD.open(fichier_annee, FILE_WRITE);

  // si le fichier est bien ouvert-> écriture
  if (annetelefile) {
    if (nouveau_fichier)
    {
      #ifdef abo_BASE     
        annetelefile.print("BAS,"); 
      #endif
      #ifdef abo_HCHP
        annetelefile.print("HC.,"); 
      #endif
      #ifdef abo_EJP
        annetelefile.print("EJP,"); 
      #endif
      #ifdef abo_BBR
        annetelefile.print("BBR,"); 
      #endif
      #ifdef MONOPHASE
        annetelefile.print("1,15");
      #endif

      #ifdef compteur2_actif
        annetelefile.println(",CPT2 actif,BAS");
      #else
        annetelefile.println("");
      #endif
    }
    format_mois_jour();

    annetelefile.print(mois_jour);
  
#ifdef abo_BASE
    annetelefile.print(base);
#endif
#ifdef abo_HCHP
    annetelefile.print(hchp);
    annetelefile.print(",");
    annetelefile.print(hchc);
#endif
#ifdef abo_EJP
    annetelefile.print(EJPHN);
    annetelefile.print(",");
    annetelefile.print(EJPHPM);
#endif
#ifdef abo_BBR
    annetelefile.print(BBRHCJB);
    annetelefile.print(",");
    annetelefile.print(BBRHPJB);
    annetelefile.print(",");
    annetelefile.print(BBRHCJW);
    annetelefile.print(",");
    annetelefile.print(BBRHPJW);
    annetelefile.print(",");
    annetelefile.print(BBRHCJR);
    annetelefile.print(",");
    annetelefile.print(BBRHPJR);
#endif

#ifdef compteur2_actif
    annetelefile.print(",");
    annetelefile.print(cpt2index);
#endif

    annetelefile.println("");
    annetelefile.close();
  }  

}
  
///////////////////////////////////////////////////////////////////
// mise en forme Date & heure pour affichage ou enregistrement
///////////////////////////////////////////////////////////////////
void format_date_heure()
{
      sprintf(date_heure,"%02d/%02d/%d,%02d:%02d",jour, mois, annee, heure, minute);
}

///////////////////////////////////////////////////////////////////
// mise en forme Date & heure pour affichage ou enregistrement
///////////////////////////////////////////////////////////////////
void format_mois_jour()
{
      sprintf(mois_jour,"%02d,%02d,",mois, jour);
}

// Convert normal decimal numbers to binary coded decimal
static uint8_t bin2bcd (uint8_t val) { return val + 6 * (val / 10); }

///////////////////////////////////////////////////////////////////
// mise à l'heure de la RTC (DS1307)
///////////////////////////////////////////////////////////////////
void RTCsetTime(void)
{
  Wire.beginTransmission(104); // 104 is DS1307 device address (0x68)
  Wire.write(bin2bcd(0)); // start at register 0

  Wire.write(bin2bcd(seconde)); //Send seconds as BCD
  Wire.write(bin2bcd(minute)); //Send minutes as BCD
  Wire.write(bin2bcd(heure)); //Send hours as BCD
  Wire.write(bin2bcd(jour_semaine)); // dow
  Wire.write(bin2bcd(jour)); //Send day as BCD
  Wire.write(bin2bcd(mois)); //Send month as BCD
  Wire.write(bin2bcd(annee % 1000)); //Send year as BCD

  Wire.endTransmission();  
}

