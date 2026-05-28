#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include <time.h>

// Definisanje makroa za maksimalne velicine
#define MAX_PROMENLJIVIH 32
#define MAX_DATOTEKA 32
#define MAX_INSTRUKCIJA 64
#define MAX_LABELA 64
#define MAX_LINIJA_FAJLA 128
#define MAX_PUTANJA_FAJLA 256

//Definisanje makroa za 8bit arhitekturu
#define MASKA_8bit 0xFF
#define ZNAKOVNI_8bit 0x80


// Definisanje enumeracija za registre, flagove, instrukcije i segmente
typedef enum {AL,BL,CL,DL, REGISTAR_NEISPRAVAN} ImeRegistra;
typedef enum {CF,ZF,OF,SF, FLAG_NEISPRAVAN} ImeFlaga;
typedef enum {MOV,ADD,SUB,CMP,INC,DEC,JMP,JE, INSTRUKCIJA_NEISPRAVNA} ImeInstrukcije;
typedef enum {SEGMENT_NULL,SEGMENT_DATA,SEGMENT_CODE} Segment;
typedef enum {REGISTAR,PROMENLJIVA,VREDNOST,LABELA, OPERAND_NEISPRAVAN} TipOperanda;


// Definisanje struktura za promenljive, labele, operande, instrukcije, CPU i datoteke
typedef struct Promenljiva {
    char ime_promenljive[32];
    int vrednost_promenljive;
} Promenljiva;

typedef struct Labela {
    char ime_labele[32];
    struct Instrukcija *instrukcija_labele;
} Labela;

typedef struct Operand {
    TipOperanda tip_operanda;
    int vrednost_operanda; //Koristi se samo ako je operand tipa VREDNOST
    char ime_labele[32]; //Koristi se samo ako je operand tipa LABELA
    ImeRegistra registar;
    Promenljiva *promenljiva;
} Operand;

typedef struct Instrukcija {
    ImeInstrukcije ime_instrukcije;
    char operand1[16];
    char operand2[16];
    struct Instrukcija *sledeca_instrukcija;
} Instrukcija;

typedef struct CPU {
    int registri[4];
    int flagovi[4];

    Promenljiva promenljive[MAX_PROMENLJIVIH];
    Labela labele[MAX_LABELA];
}CPU;

typedef struct datoteka {
    char naziv[MAX_PUTANJA_FAJLA];
}datoteka;

// Globalne promenljive
char putanja_odabranog_fajla[MAX_PUTANJA_FAJLA];
char naziv_log_fajla[MAX_PUTANJA_FAJLA];

Instrukcija *glava_liste_instrukcija = NULL;
Instrukcija *rep_liste_instrukcija = NULL;

datoteka dostupne_datoteke[MAX_DATOTEKA];
int broj_datoteka = 0;

// Funkcija za prijavu greske, koja ispisuje poruku na konzolu i upisuje je u log fajl ako je kreiran
void prijaviGresku(const char *poruka) {
    printf("\n\nGreska: %s\n", poruka);
    
    if (naziv_log_fajla[0] != '\0') {
        FILE *log_fajl = fopen(naziv_log_fajla, "a");
        if (log_fajl != NULL) {
            fprintf(log_fajl, "\n\nGreska: %s\n", poruka);
            fclose(log_fajl);
        }
    }
}

//Prototip funkcija za pronalazak labele
Labela* pronadjiLabelu(CPU *cpu, char *ime_labele);

// Funkcija za trimovanje razmaka sa pocetka i kraja stringa
void trim(char *str) {
    int pocetak = 0;
    int kraj = strlen(str) - 1;

    while (isspace((unsigned char)str[pocetak])) {
        pocetak++;
    }

    while (kraj >= pocetak && isspace((unsigned char)str[kraj])) {
        kraj--;
    }

    int i = 0;
    while (pocetak <= kraj) {
        str[i++] = str[pocetak++];
    }
    str[i] = '\0';
}

// Funkcija za detekciju labele
int detektujLabelu(char *linija, char *poslednja_labela) {
    trim(linija);

    int len = strlen(linija);

    if (len <= 1) {
        return 0;
    }

    if (linija[len - 1] != ':') {
        return 0;
    }

    strncpy(poslednja_labela, linija, len - 1);
    poslednja_labela[len - 1] = '\0';

    trim(poslednja_labela);

    return 1;
}

// Funkcija za proveru vrednosti stringa kao broj
int jeBroj(char *str) {

    if (*str == '-' || *str == '+') {
        str++;
    }

    if (*str == '\0')
        return 0;

    while (*str) {
        if (!isdigit((unsigned char)*str)) {
            return 0;
        }
        str++;
    }

    return 1;
}

// Funkcije za parsiranje

ImeRegistra parsirajRegistar(char *registar) {
    if (strcmp(registar,"AL") == 0) {return AL;}
    if (strcmp(registar,"BL") == 0) {return BL;}
    if (strcmp(registar,"CL") == 0) {return CL;}
    if (strcmp(registar,"DL") == 0) {return DL;}

    return REGISTAR_NEISPRAVAN; // Vraca REGISTAR_NEISPRAVAN ako nije validan registar
}

ImeFlaga parsirajFlag(char *flag) {
    if (strcmp(flag,"CF") == 0) {return CF;}
    if (strcmp(flag,"ZF") == 0) {return ZF;}
    if (strcmp(flag,"OF") == 0) {return OF;}
    if (strcmp(flag,"SF") == 0) {return SF;}

    return FLAG_NEISPRAVAN; // Vraca FLAG_NEISPRAVAN ako nije validan flag
}

ImeInstrukcije parsirajInstrukciju(char *instrukcija) {
    if (strcmp(instrukcija,"MOV") == 0) {return MOV;}
    if (strcmp(instrukcija,"ADD") == 0) {return ADD;}
    if (strcmp(instrukcija,"SUB") == 0) {return SUB;}
    if (strcmp(instrukcija,"CMP") == 0) {return CMP;}
    if (strcmp(instrukcija,"INC") == 0) {return INC;}
    if (strcmp(instrukcija,"DEC") == 0) {return DEC;}
    if (strcmp(instrukcija,"JMP") == 0) {return JMP;}
    if (strcmp(instrukcija,"JE") == 0) {return JE;}

    return INSTRUKCIJA_NEISPRAVNA; // Vraca INSTRUKCIJA_NEISPRAVNA ako nije validna instrukcija
}

Promenljiva* parsirajPromenljivu(CPU *cpu, char *operand) {
    for (int i = 0; i < MAX_PROMENLJIVIH; i++) {
        if (strcmp(cpu->promenljive[i].ime_promenljive, operand) == 0) {
            return &cpu->promenljive[i];
        }
    }

    return NULL; // Vraca NULL ako promenljiva nije pronadjena
}

Operand parsirajOperand(CPU *cpu, char *operand) {
    // Inicijalizacija rezultata kao nevalidnog operanda
    Operand rezultat;
    rezultat.tip_operanda = OPERAND_NEISPRAVAN;
    rezultat.vrednost_operanda = 0;
    rezultat.registar = REGISTAR_NEISPRAVAN;
    rezultat.promenljiva = NULL;
    rezultat.ime_labele[0] = '\0';

    // Provera da li je operand registar
    ImeRegistra registar = parsirajRegistar(operand);
    if (registar != REGISTAR_NEISPRAVAN) {
        rezultat.tip_operanda = REGISTAR;
        rezultat.registar = registar;
        return rezultat;
    }

    // Provera da li je operand promenljiva
    Promenljiva *promenljiva = parsirajPromenljivu(cpu, operand);
    if (promenljiva != NULL) {
        rezultat.tip_operanda = PROMENLJIVA;
        rezultat.promenljiva = promenljiva;
        return rezultat;
    }

    // Provera da li je operand vrednost
    if (jeBroj(operand)) {
        rezultat.tip_operanda = VREDNOST;
        rezultat.vrednost_operanda = (int)strtol(operand, NULL, 10) & MASKA_8bit; // Ogranicavanje na 8 bitova
        return rezultat;
    }

    // Provera da li je operand labela
    Labela *labela = pronadjiLabelu(cpu, operand);
    if (labela != NULL && labela->ime_labele[0] != '\0') {
        rezultat.tip_operanda = LABELA;
        strcpy(rezultat.ime_labele, labela->ime_labele);
        return rezultat;
    }

    return rezultat; // Vraca nevalidan operand ako nije ni registar, ni promenljiva, ni vrednost
}

int parsirajVrednost(CPU *cpu, Operand operand) {

    if (operand.tip_operanda == REGISTAR) {
        return cpu->registri[operand.registar];
    } else if (operand.tip_operanda == PROMENLJIVA) {
        return operand.promenljiva->vrednost_promenljive;
    } else if (operand.tip_operanda == VREDNOST) {
        return operand.vrednost_operanda;
    } else {
        return 0; // Vraca 0 ako operand nije validan
    }

}

// Funkcija za upisivanje vrednosti u registar ili promenljivu

void upisiVrednost(CPU *cpu, Operand operand1, int vrednost) {
    if (operand1.tip_operanda == REGISTAR) {
        cpu->registri[operand1.registar] = vrednost & MASKA_8bit;
    } else if (operand1.tip_operanda == PROMENLJIVA) {
        operand1.promenljiva->vrednost_promenljive = vrednost & MASKA_8bit;
    } else {
        prijaviGresku("Nevalidan operand za upisivanje vrednosti.\n");
    }
}

void upisiFlagove(CPU *cpu, ImeInstrukcije instrukcija, int inicijalni, int rezultat, int Vrednost_Operand1, int Vrednost_Operand2) {
    switch(instrukcija) {
        case ADD:
            cpu->flagovi[ZF] = (rezultat == 0) ? 1 : 0;
            cpu->flagovi[SF] = (rezultat & ZNAKOVNI_8bit) != 0 ? 1 : 0;
            cpu->flagovi[CF] = (inicijalni > MASKA_8bit) ? 1 : 0;
            cpu->flagovi[OF] = ((Vrednost_Operand1 ^ inicijalni) & (Vrednost_Operand2 ^ inicijalni) & ZNAKOVNI_8bit) != 0 ? 1 : 0;
            break;
        case SUB:
        case CMP:
            cpu->flagovi[ZF] = (rezultat == 0) ? 1 : 0;
            cpu->flagovi[SF] = (rezultat & ZNAKOVNI_8bit) != 0 ? 1 : 0;
            cpu->flagovi[CF] = (Vrednost_Operand1 < Vrednost_Operand2) ? 1 : 0;
            cpu->flagovi[OF] = ((Vrednost_Operand1 ^ Vrednost_Operand2) & (Vrednost_Operand1 ^ inicijalni) & ZNAKOVNI_8bit) != 0 ? 1 : 0;
            break;
        case INC:
            cpu->flagovi[ZF] = (rezultat == 0) ? 1 : 0;
            cpu->flagovi[SF] = (rezultat & ZNAKOVNI_8bit) != 0 ? 1 : 0;
            cpu->flagovi[OF] = (Vrednost_Operand1 == 0x7F) ? 1 : 0;
            break;
        case DEC:
            cpu->flagovi[ZF] = (rezultat == 0) ? 1 : 0;
            cpu->flagovi[SF] = (rezultat & ZNAKOVNI_8bit) != 0 ? 1 : 0;
            cpu->flagovi[OF] = (Vrednost_Operand1 == 0x80) ? 1 : 0;
            break;
    }
}

// Funkcije za labele

Labela* kreirajLabelu(char *ime_labele, Instrukcija *instrukcija_labele) {
    Labela *nova_labela = (Labela*)malloc(sizeof(Labela));
    if (nova_labela == NULL) {
        printf("Greska pri alokaciji memorije za novu labelu.\n");
        exit(EXIT_FAILURE);
    }

    strcpy(nova_labela->ime_labele, ime_labele);
    nova_labela->instrukcija_labele = instrukcija_labele;

    return nova_labela;
}

void dodajLabelu(Labela *nova_labela, CPU *cpu) {

    // Provera da li labela vec postoji
    for (int i = 0; i < MAX_LABELA; i++) {
        if (strcmp(nova_labela->ime_labele, cpu->labele[i].ime_labele) == 0) {
            printf("Labela sa imenom '%s' vec postoji.\n", nova_labela->ime_labele);
            free(nova_labela);
            return;
        }
    }

    // Dodavanje nove labele u CPU
    for (int i = 0; i < MAX_LABELA; i++) {
        
        //Proverava da li ima mesta za labelu, ako ne postoji slobodno mesto, ispisuje poruku i oslobadja memoriju
        if (cpu->labele[MAX_LABELA - 1].ime_labele[0] != '\0') {
            printf("Nema mesta za novu labelu '%s'.\n", nova_labela->ime_labele);
            free(nova_labela);
            return;
        }

        //Dodaje labelu na prvo slobodno mesto
        if (cpu->labele[i].ime_labele[0] == '\0') { // Pronadjii prvo slobodno mesto
            cpu->labele[i] = *nova_labela;
            free(nova_labela);
            return;
        }
    }

}

Labela* pronadjiLabelu(CPU *cpu, char *ime_labele) {

    for (int i = 0; i < MAX_LABELA; i++) {
        if (strcmp(cpu->labele[i].ime_labele, ime_labele) == 0) {
            return &cpu->labele[i];
        }
    }

    return NULL; // Vraca NULL ako labela nije pronadjena

}

// Funkcije za instrukcije

Instrukcija* kreirajInstrukciju(char *ime_instrukcije, char *operand1, char *operand2) {

    // Alokacija memorije za novu instrukciju
    Instrukcija *nova_instrukcija = (Instrukcija*)malloc(sizeof(Instrukcija));
    if (nova_instrukcija == NULL) {
        printf("Greska pri alokaciji memorije za novu instrukciju.\n");
        exit(EXIT_FAILURE);
    }

    // Parsiranje instrukcije i provera validnosti
    int parsirana_instrukcija = parsirajInstrukciju(ime_instrukcije);
    if (parsirana_instrukcija == INSTRUKCIJA_NEISPRAVNA) {
        prijaviGresku("Nevalidna instrukcija.\n");
        free(nova_instrukcija);
        exit(EXIT_FAILURE);
    }
    nova_instrukcija->ime_instrukcije = parsirana_instrukcija;

    // Kopiranje operanada i osiguravanje da su null-terminirani
    strncpy(nova_instrukcija->operand1, operand1, sizeof(nova_instrukcija->operand1) - 1);
    nova_instrukcija->operand1[sizeof(nova_instrukcija->operand1) - 1] = '\0';

    strncpy(nova_instrukcija->operand2, operand2, sizeof(nova_instrukcija->operand2) - 1);
    nova_instrukcija->operand2[sizeof(nova_instrukcija->operand2) - 1] = '\0';

    // Inicijalizacija pokazivaca na sledecu instrukciju
    nova_instrukcija->sledeca_instrukcija = NULL;

    return nova_instrukcija;
}

void dodajInstrukciju(Instrukcija *nova_instrukcija) {
    if (glava_liste_instrukcija == NULL) {
        glava_liste_instrukcija = nova_instrukcija;
        rep_liste_instrukcija = nova_instrukcija;
    } else {
        rep_liste_instrukcija->sledeca_instrukcija = nova_instrukcija;
        rep_liste_instrukcija = nova_instrukcija;
    }
}

void oslobodiInstrukcije() {
    while (glava_liste_instrukcija != NULL) {
        Instrukcija *temp = glava_liste_instrukcija;
        glava_liste_instrukcija = glava_liste_instrukcija->sledeca_instrukcija;
        free(temp);
    }
}

// Funkcije za promenljive

Promenljiva* kreirajPromenljivu(char *ime_promenljive, int vrednost_promenljive) {
    Promenljiva *nova_promenljiva = (Promenljiva*)malloc(sizeof(Promenljiva));
    if (nova_promenljiva == NULL) {
        printf("Greska pri alokaciji memorije za novu promenljivu.\n");
        exit(EXIT_FAILURE);
    }

    strncpy(nova_promenljiva->ime_promenljive, ime_promenljive, sizeof(nova_promenljiva->ime_promenljive) - 1);
    nova_promenljiva->ime_promenljive[sizeof(nova_promenljiva->ime_promenljive) - 1] = '\0';
    nova_promenljiva->vrednost_promenljive = vrednost_promenljive;

    return nova_promenljiva;
}

// Funkcije ispisa

void ispisiInstrukciju(Instrukcija *instrukcija) {
    if (instrukcija != NULL) {

        // Provera da li operand2 postoji, i ispis u zavisnosti od toga
        if (instrukcija->operand2[0] == '\0') {
            printf("Instrukcija: %d, Operand1: %s\n", instrukcija->ime_instrukcije, instrukcija->operand1);
            return;
        }

        // Ako oba operanda postoje, ispisujemo oba
        printf("Instrukcija: %d, Operand1: %s, Operand2: %s\n", instrukcija->ime_instrukcije, instrukcija->operand1, instrukcija->operand2);

    } else {
        printf("Instrukcija je NULL.\n");
    }
}

void ispisPromenljivih(CPU *cpu) {
    printf("Promenljive:\n");
    for (int i = 0; i < MAX_PROMENLJIVIH; i++) {
        if (cpu->promenljive[i].ime_promenljive[0] != '\0') {
            printf("%s: %d, ", cpu->promenljive[i].ime_promenljive, cpu->promenljive[i].vrednost_promenljive);
        }
    }
    printf("\n");
}

void ispisCPU(CPU *cpu, int korak, Instrukcija *trenutna_instrukcija) {
    printf("\n--- Korak %d ---\n", korak);
    ispisiInstrukciju(trenutna_instrukcija);
    printf("\n");

    printf("Registri:\n");
    printf("AL:%d, ", cpu->registri[AL]);
    printf("BL:%d, ", cpu->registri[BL]);
    printf("CL:%d, ", cpu->registri[CL]);
    printf("DL:%d\n\n", cpu->registri[DL]);

    printf("Flagovi:\n");
    printf("CF:%d, ", cpu->flagovi[CF]);
    printf("ZF:%d, ", cpu->flagovi[ZF]);
    printf("OF:%d, ", cpu->flagovi[OF]);
    printf("SF:%d\n\n", cpu->flagovi[SF]);

    ispisPromenljivih(cpu);

    printf("--- Kraj Koraka %d ---\n\n", korak);
}

void ispisDatoteka() {
    printf("Dostupne datoteke:\n");
    for (int i = 0; i < broj_datoteka; i++) {
        printf("%d. %s\n", i+1, dostupne_datoteke[i].naziv);
    }
}

// Funkcije za fajlove

void citajDatotekeIzFoldera(char *putanja_foldera) {
    WIN32_FIND_DATA pronadji_datoteku;
    HANDLE drska = FindFirstFile(putanja_foldera, &pronadji_datoteku);

    if (drska == INVALID_HANDLE_VALUE) {
        printf("Greska pri citanju foldera.\n");
        return;
    }

    do {

        if (broj_datoteka < MAX_DATOTEKA) {

            // Formiranje putanje fajla
            char naziv_datoteke[MAX_PUTANJA_FAJLA] = "\0";
            strcat(naziv_datoteke,pronadji_datoteku.cFileName);

            // dodavanje u niz fajlova
            dostupne_datoteke[broj_datoteka].naziv[0] = '\0';
            datoteka *nova = (datoteka*)malloc(sizeof(datoteka));
            if (nova) {
                strncpy(nova->naziv, naziv_datoteke, sizeof(nova->naziv) - 1);
                nova->naziv[sizeof(nova->naziv) - 1] = '\0';
                dostupne_datoteke[broj_datoteka] = *nova;
                free(nova);

                broj_datoteka++;
            }
        }

    } while (FindNextFile(drska, &pronadji_datoteku));

    FindClose(drska);
}

int UcitajInstrukcijeIzFajla(char *putanja_fajla, CPU *cpu) {
    //Kontrolne promenljive
    char poslednja_labela[32];
    int labela_trazi_sledecu_instrukciju = 0;
    Segment trenutni_segment = SEGMENT_NULL;

    // Otvaranje fajla za citanje
    FILE *fajl = fopen(putanja_fajla, "r");
    if (fajl == NULL) {
        printf("Greska pri otvaranju fajla.\n");
        exit(EXIT_FAILURE);
    }

    int greskacitanja = 0;

    // Citanje linija iz fajla
    char linija[MAX_LINIJA_FAJLA];
    while (fgets(linija, sizeof(linija), fajl) != NULL) {

        // Promenljive za parsiranje instrukcije i operanada
        char ime_instrukcije[16];
        char operand1[16];
        char operand2[16];

        operand1[0] = '\0';
        operand2[0] = '\0';

        //Ako je linija prazna ili sadrzi samo razmake, preskacemo je
        char trim_linija[MAX_LINIJA_FAJLA];
        strcpy(trim_linija, linija);
        trim(trim_linija);

        if (strlen(trim_linija) == 0) {
            continue;
        }

        // Provera da li linija sadrzi segment
        if (strcmp(trim_linija, ".data") == 0) {
            trenutni_segment = SEGMENT_DATA;
            continue;
        } else if (strcmp(trim_linija, ".code") == 0) {
            trenutni_segment = SEGMENT_CODE;
            continue;
        }

        //Segmentovanje
        if (trenutni_segment == SEGMENT_NULL) {
            
            continue;

        } else if (trenutni_segment == SEGMENT_DATA) {
            
            // Provera da li linija sadrzi promenljivu
            char ime_promenljive[32];
            int vrednost_promenljive;
            int rezultat = sscanf(linija, "%31s %d", ime_promenljive, &vrednost_promenljive);
            if (rezultat != 2) {
                
                char greska[128];
                sprintf(greska, "Nevalidan format promenljive u fajlu: %s", linija);
                prijaviGresku(greska);

                greskacitanja = 1;
                break;
            }

            trim(ime_promenljive);
            vrednost_promenljive = vrednost_promenljive & MASKA_8bit;
            
            // Dodavanje promenljive u CPU
            Promenljiva *nova_promenljiva = kreirajPromenljivu(ime_promenljive, vrednost_promenljive);
            
            int dodato = 0;
            for (int i = 0; i < MAX_PROMENLJIVIH; i++) {
                if (strcmp(cpu->promenljive[i].ime_promenljive,ime_promenljive) == 0) {
                    printf("Promenljiva sa imenom '%s' vec postoji. Unosenje nove vrednosti na rezervisano mesto.\n", ime_promenljive);
                    cpu->promenljive[i] = *nova_promenljiva;
                    free(nova_promenljiva);
                    dodato = 1;
                    break;
                }
                
                if (cpu->promenljive[i].ime_promenljive[0] == '\0') { // Pronadji prvo slobodno mesto
                    cpu->promenljive[i] = *nova_promenljiva;
                    free(nova_promenljiva);
                    dodato = 1;
                    break;
                }
            }

            if (!dodato) {
                printf("Nema mesta za novu promenljivu '%s'.\n", ime_promenljive);
                free(nova_promenljiva);
            }


        } else if (trenutni_segment == SEGMENT_CODE) {

            printf("Obrada linije u CODE segmentu: %s\n", linija);

            // Provera da li linija sadrzi labelu
            if (detektujLabelu(linija, poslednja_labela)) {
                // Ako linija sadrzi labelu, preskacemo je
                labela_trazi_sledecu_instrukciju = 1;
                continue;
            }

            // Parsiranje instrukcije i provera validnosti
            int parsirana_instrukcija;
            sscanf(linija, "%15s", ime_instrukcije);
            trim(ime_instrukcije);
            parsirana_instrukcija = parsirajInstrukciju(ime_instrukcije);
            if (parsirana_instrukcija == INSTRUKCIJA_NEISPRAVNA) {

                char greska[128];
                sprintf(greska, "Nevalidna instrukcija u fajlu: %s\n", ime_instrukcije);
                prijaviGresku(greska);

                greskacitanja = 1;
                break;
            }

            switch(parsirana_instrukcija) {
                case MOV:
                case ADD:
                case SUB:
                case CMP:
                    int rezultat2op = sscanf(linija, "%*s %15[^,], %15s",operand1, operand2);

                    if (rezultat2op != 2) {
                        
                        char greska[128];
                        sprintf(greska, "Nevalidan format instrukcije u fajlu: %s\n", linija);
                        prijaviGresku(greska);

                        greskacitanja = 1;
                        break;
                    }

                    trim(operand1);
                    trim(operand2);

                    break;
                case INC:
                case DEC:
                case JMP:
                case JE:
                    int rezultat1op = sscanf(linija, "%*s %15s", operand1);

                    if (rezultat1op != 1) {
                        char greska[128];
                        sprintf(greska, "Nevalidan format instrukcije u fajlu: %s\n", linija);
                        prijaviGresku(greska);
                        greskacitanja = 1;
                        break;
                    }
                    
                    trim(operand1);
                    operand2[0] = '\0'; // Postavljanje drugog operanda na prazan string

                    break;
                default:
                    char greska[128];
                    sprintf(greska, "Nevalidna instrukcija u fajlu: %s\n", ime_instrukcije);
                    prijaviGresku(greska);

                    greskacitanja = 1;
                    break;
            }

            Instrukcija *nova_instrukcija = kreirajInstrukciju(ime_instrukcije, operand1, operand2);
            
            //Ako je pre ove linije bila detektovana labela, dodajemo instrukciju u tu labelu
            if (labela_trazi_sledecu_instrukciju) {
                //Dodati instrukciju u labelu
                Labela *nova_labela = kreirajLabelu(poslednja_labela, nova_instrukcija);
                dodajLabelu(nova_labela, cpu);

                labela_trazi_sledecu_instrukciju = 0;
            }
            
            dodajInstrukciju(nova_instrukcija);

        }
        
    }



    fclose(fajl);

    if (greskacitanja) {
        return 0;
    }
    return 1;
}

void napraviLogFajl(char *naziv_programa) {
    naziv_programa[strlen(naziv_programa) - 4] = '\0'; // Uklanjanje ekstenzije .txt iz naziva programa


    // Formiranje imena log fajla sa timestampom
    time_t sada = time(NULL);
    struct tm *vreme = localtime(&sada);

    char log_deo[64];
    strftime(log_deo, sizeof(log_deo), "_log_%Y%m%d_%H%M%S.txt", vreme);

    char ime_log_fajla[MAX_PUTANJA_FAJLA];
    ime_log_fajla[0] = '\0';
    
    strcat(ime_log_fajla, "logovi//");
    strcat(ime_log_fajla, naziv_programa);
    strcat(ime_log_fajla, log_deo);

    FILE *log_fajl = fopen(ime_log_fajla, "w");
    if (log_fajl == NULL) {
        printf("Greska pri kreiranju log fajla.\n");
        return;
    }

    strcpy(naziv_log_fajla, ime_log_fajla);

    fprintf(log_fajl, "Log fajl za program: %s\n", putanja_odabranog_fajla);
    fclose(log_fajl);
}

void upis_Log(CPU *cpu, int korak, Instrukcija *trenutna_instrukcija) {
    FILE *log_fajl = fopen(naziv_log_fajla, "a");
    if (log_fajl == NULL) {
        printf("Greska pri otvaranju log fajla za upis.\n");
        return;
    }
    fprintf(log_fajl, "\n--- Korak %d ---\n", korak);
    fprintf(log_fajl, "Instrukcija: %d, Operand1: %s, Operand2: %s\n", trenutna_instrukcija->ime_instrukcije, trenutna_instrukcija->operand1, trenutna_instrukcija->operand2);
    fprintf(log_fajl, "Registri: AL:%d, BL:%d, CL:%d, DL:%d\n", cpu->registri[AL], cpu->registri[BL], cpu->registri[CL], cpu->registri[DL]);
    fprintf(log_fajl, "Flagovi: CF:%d, ZF:%d, OF:%d, SF:%d\n", cpu->flagovi[CF], cpu->flagovi[ZF], cpu->flagovi[OF], cpu->flagovi[SF]);
    fprintf(log_fajl, "Promenljive: ");
    
    for (int i = 0; i < MAX_PROMENLJIVIH; i++) {
        if (cpu->promenljive[i].ime_promenljive[0] != '\0') {
            fprintf(log_fajl, "%s: %d, ", cpu->promenljive[i].ime_promenljive, cpu->promenljive[i].vrednost_promenljive);
        }
    }

    fclose(log_fajl);
}

// Funkcije za izvrsavanje instrukcija

void izvrsiInstrukcije(CPU *cpu) {

    int korak = 0;
    Instrukcija *trenutna = glava_liste_instrukcija;

    while (trenutna != NULL) {
        korak++;

        Operand Operand1 = parsirajOperand(cpu, trenutna->operand1);
        Operand Operand2 = parsirajOperand(cpu, trenutna->operand2);

        int treba_operand2 = 1;
        switch(trenutna->ime_instrukcije) {
            case INC:
            case DEC:
            case JMP:
            case JE:
                treba_operand2 = 0;
                break;
        }

        if (Operand1.tip_operanda == OPERAND_NEISPRAVAN || (Operand2.tip_operanda == OPERAND_NEISPRAVAN && treba_operand2)) {
            prijaviGresku("Operand ne postoji.\n");
            break;
        }

        int Vrednost_Operand1 = parsirajVrednost(cpu, Operand1) & MASKA_8bit;
        int Vrednost_Operand2 = parsirajVrednost(cpu, Operand2) & MASKA_8bit;

        //Labela samo za JMP instrukciju
        if (Operand1.tip_operanda == LABELA && trenutna->ime_instrukcije != JMP && trenutna->ime_instrukcije != JE) {
            
            char greska[128];
            sprintf(greska, "Nevalidan operand: Labela '%s' se ne moze koristiti kao operand osim u JMP instrukcijama.\n", Operand1.ime_labele);
            prijaviGresku(greska);

            trenutna = trenutna->sledeca_instrukcija;
            break;

        }
        
        Instrukcija *sledeca_instrukcija = trenutna->sledeca_instrukcija;

        //Izvrsavanje instrukcije na osnovu njenog imena
        switch(trenutna->ime_instrukcije) {
            case MOV: {

                upisiVrednost(cpu, Operand1, Vrednost_Operand2);
                break;
            
            }
            case ADD: {
                int inicijalni = Vrednost_Operand1 + Vrednost_Operand2;
                int rezultat =  inicijalni & MASKA_8bit; // Ogranicavanje rezultata na 8 bitova

                upisiVrednost(cpu, Operand1, rezultat);
                upisiFlagove(cpu, trenutna->ime_instrukcije, inicijalni, rezultat, Vrednost_Operand1, Vrednost_Operand2);

                break;
            }
            case SUB: {
                int inicijalni = Vrednost_Operand1 - Vrednost_Operand2;
                int rezultat = inicijalni & MASKA_8bit; // Ogranicavanje rezultata na 8 bitova

                upisiVrednost(cpu, Operand1, rezultat);
                upisiFlagove(cpu, trenutna->ime_instrukcije, inicijalni, rezultat, Vrednost_Operand1, Vrednost_Operand2);

                break;
            }
            case CMP: {
                int inicijalni = Vrednost_Operand1 - Vrednost_Operand2;
                int rezultat = inicijalni & MASKA_8bit; // Ogranicavanje rezultata na 8 bitova

                upisiFlagove(cpu, trenutna->ime_instrukcije, inicijalni, rezultat, Vrednost_Operand1, Vrednost_Operand2);

                break;
            }
            case INC: {
                int inicijalni = Vrednost_Operand1 + 1;
                int rezultat = inicijalni & MASKA_8bit; // Ogranicavanje rezultata na 8 bitova

                upisiVrednost(cpu, Operand1, rezultat);
                upisiFlagove(cpu, trenutna->ime_instrukcije, inicijalni, rezultat, Vrednost_Operand1, Vrednost_Operand2);

                break;
            }
            case DEC: {
                int inicijalni = Vrednost_Operand1 - 1;
                int rezultat = inicijalni & MASKA_8bit; // Ogranicavanje rezultata na 8 bitova

                upisiVrednost(cpu, Operand1, rezultat);
                upisiFlagove(cpu, trenutna->ime_instrukcije, inicijalni, rezultat, Vrednost_Operand1, Vrednost_Operand2);


                break;
            }
            case JMP: {
                // Implementacija skoka
                
                Labela *labela = pronadjiLabelu(cpu, Operand1.ime_labele);
                if (labela != NULL) {
                    sledeca_instrukcija = labela->instrukcija_labele;
                } else {
                    char greska[128];
                    sprintf(greska, "Labela '%s' nije pronadjena za JMP instrukciju.\n", Operand1.ime_labele);
                    prijaviGresku(greska);
                }

                break;
            }
            case JE: {
                if (cpu->flagovi[ZF] == 1) {
                    Labela *labela = pronadjiLabelu(cpu, Operand1.ime_labele);
                    
                    if (labela != NULL) {
                    
                        sledeca_instrukcija = labela->instrukcija_labele;
                    
                    } else {

                        char greska[128];
                        sprintf(greska, "Labela '%s' nije pronadjena za JE instrukciju.\n", Operand1.ime_labele);
                        prijaviGresku(greska);

                    }

                }
                break;
            }
            default: {
                prijaviGresku("Nevalidna instrukcija za izvrsavanje.\n");
                break;
            }
        }


        ispisCPU(cpu, korak, trenutna);
        upis_Log(cpu, korak, trenutna);

        trenutna = sledeca_instrukcija;

    }

}

// Funkcija za resetovanje CPU-a na pocetne vrednosti

void resetujCPU(CPU *cpu) {
    // Inicijalizacija registara
    cpu->registri[AL] = 0;
    cpu->registri[BL] = 0;
    cpu->registri[CL] = 0;
    cpu->registri[DL] = 0;

    // Inicijalizacija flagova
    cpu->flagovi[CF] = 0;
    cpu->flagovi[ZF] = 0;
    cpu->flagovi[OF] = 0;
    cpu->flagovi[SF] = 0;
   
    // Inicijalizacija promenljivih
    for (int i = 0; i < MAX_PROMENLJIVIH; i++) {
        cpu->promenljive[i].ime_promenljive[0] = '\0';
        cpu->promenljive[i].vrednost_promenljive = 0;
    }

    // Inicijalizacija labela
    for (int i = 0; i < MAX_LABELA; i++) {
        cpu->labele[i].ime_labele[0] = '\0';
        cpu->labele[i].instrukcija_labele = NULL;
    }

    oslobodiInstrukcije();
}

int main() {    

    // Allociranje memorije za CPU
    CPU *cpu = (CPU*)malloc(sizeof(CPU));

    // Prikaz dostupnih datoteka
    citajDatotekeIzFoldera("assembler_programi\\*.txt");
    ispisDatoteka();

    // Izbor datoteke od strane korisnika
    int izbor;
    printf("Unesite broj datoteke koju zelite da ucitate: ");
    scanf("%d", &izbor);

    if (izbor < 1 || izbor > broj_datoteka) {
        printf("Nevalidan izbor datoteke.\n");
        return 1;
    }

    char temp_putanja[MAX_PUTANJA_FAJLA] = "assembler_programi\\";
    strcat(temp_putanja, dostupne_datoteke[izbor - 1].naziv);

    strcpy(putanja_odabranog_fajla, temp_putanja);
    printf("Odabrana datoteka: %s\n", putanja_odabranog_fajla);
   
    // Kreiranje log fajla
    napraviLogFajl(dostupne_datoteke[izbor - 1].naziv);

    // Ucitavanje instrukcija iz odabrane datoteke i izvrsavanje
    resetujCPU(cpu);
    int rezultat_ucitavanja = UcitajInstrukcijeIzFajla(putanja_odabranog_fajla, cpu);
    if (rezultat_ucitavanja) {
        izvrsiInstrukcije(cpu);
    }

    scanf(" %c"); // Pauza na kraju programa, ceka unos sa tastature

    free(cpu);
    return 0;
}