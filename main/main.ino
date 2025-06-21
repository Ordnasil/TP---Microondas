#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

#define LED 2
#define buzzer 11
#define MOTOR 3
#define interruptor A1
#define FRECUENCIA_ALTA 8
#define FRECUENCIA_BAJA 3
#define FRECUENCIA_TERMINADO 720
int rpm = 2;
bool cancelado = false;

//Globales para programa de coccion personalizado
unsigned long tiempoUsuarioCoccion = 0;
unsigned long tiempoUsuarioPausa = 0;
int repeticionesUsuario = 0;
bool usuarioConfigurado = false;
int cantidadRepes = 0;

// LCD I2C
LiquidCrystal_I2C lcd(0x20, 16, 2);

// Keypad
const byte fil = 4;
const byte col = 4;
char teclado[fil][col] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte filpin[fil] = {12,10,9,8};
byte colpin[col] = {7,6,5,4};
Keypad miTeclado = Keypad(makeKeymap(teclado), filpin, colpin, fil, col);

// ----------------- SETUP Y LOOP -----------------
void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.clear();
  lcd.backlight();

  pinMode(LED, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(MOTOR, OUTPUT);
  pinMode(interruptor, INPUT);

  // --- Simular datos guardados en EEPROM ---
  if (!usuarioConfigurado) {
    tiempoUsuarioCoccion = 10000;
    tiempoUsuarioPausa = 2000;
    repeticionesUsuario = 3;
    usuarioConfigurado = true;
  }
}

void loop() {
  lcd.setCursor(0,0);
  lcd.print("ELIGE UNA OPCION");
  lcd.setCursor(0,1);
  lcd.print("<< A B C D # >>");

  if (digitalRead(interruptor) == HIGH) {
    bloquearPuerta();
  }

  char tecla = miTeclado.getKey();
  if (tecla) {
    lcd.clear();
    lcd.setCursor(0, 0);
    Serial.println(tecla);
    delay(200);

    if (tecla == '#') {
      menuConfiguracion();  // CONFIGURACION PROGRAMA PERSONALIZADO
    } 
    else if (tecla >= '1' && tecla <= '9') {
      int cantidadRepes = tecla - '0'; // Tomo valor char a int para las repes
      coccionRapidaConRepe(cantidadRepes);
    } 
    else {
      programa(tecla);  // A, B, C, D
    }

    lcd.clear();
  }
}



// ----------------- FUNCIONES PRINCIPALES -----------------

void programa(char opcion) {
  cancelado = false;

  switch(opcion) {
    case 'A': coccionRapida(); break;
    case 'B': descongelado(); break;
    case 'C': recalentado(); break;
    case 'D': 
    if (usuarioConfigurado){
    modoPrograma(tiempoUsuarioCoccion, repeticionesUsuario, tiempoUsuarioPausa, "PERSONALIZADO");
  	}else{
      lcd.print("NO CONFIGURADO");
      delay(1500);
  		} 
    break;
    default:
      Serial.println("Opcion invalida...gil");
      lcd.clear();
      lcd.print("OPCION INVALIDA");
      delay(2000);
  }
}

//TESTEANDO
void coccionRapidaConRepe(int cantidadrepe){
  modoPrograma(3000, cantidadrepe, 0, "COCCION RAPIDA");
  cantidadRepes = 0;
}

void coccionRapida() {
  modoPrograma(3000, 1, 0, "COCCION RAPIDA");
}

void descongelado() {
  modoPrograma(2000, 5, 1000, "DESCONGELAR");
}

void recalentado() {
  modoPrograma(15000, 3, 3000, "RECALENTAR");
}

// ----------------- FUNCIONES DE LÓGICA -----------------

void modoPrograma(unsigned long duracionCoccion,
                  int repeticiones,
                  unsigned long pausaEntreRepeticiones,
                  const char* nombrePrograma) {

  for (int r = 0; r < repeticiones; r++) {
    if (cancelado) break;

    unsigned long tiempoInicio = millis();
    unsigned long tiempoPausado = 0;
    bool pausado = false;
    int segundosPrevios = -1;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(nombrePrograma);

    digitalWrite(LED, HIGH);
    analogWrite(MOTOR, rpm);

    while ((millis() - tiempoInicio - tiempoPausado) < duracionCoccion) {
      
      //TESTEO BUZZER
      tone(buzzer, FRECUENCIA_ALTA, 1000);
      
      if (verificarCancelacion()) {
        cancelado = true;
        break;
      }

      if (digitalRead(interruptor) == HIGH) {
        pausado = true;
        digitalWrite(LED, LOW);
        analogWrite(MOTOR, 0);
        tiempoPausado += bloquearPuerta();
        pausado = false;
        digitalWrite(LED, HIGH);
        analogWrite(MOTOR, rpm);
      }

      if (!pausado) {
        unsigned long tiempoRestante = duracionCoccion - (millis() - tiempoInicio - tiempoPausado);
        int segundosRestantes = tiempoRestante / 1000 + 1;

        if (segundosRestantes != segundosPrevios) {
          segundosPrevios = segundosRestantes;
          lcd.setCursor(0, 1);
          lcd.print(segundosRestantes);
          lcd.print(" s   ");
        }
      }

      delay(50);
    }

    analogWrite(MOTOR, 0);
    digitalWrite(LED, LOW);

    if (cancelado) break;

    if (r < repeticiones - 1) {
      lcd.clear();
      pausaRepeticion(millis(), 0, pausaEntreRepeticiones);
    }
  }

  lcd.clear();
  if (cancelado) {
    lcd.setCursor(0, 0);
    lcd.print("CANCELADO!");
    delay(1000);
  } else {
    lcd.setCursor(0, 0);
    lcd.print("FINALIZADO.");
    Serial.println(String(nombrePrograma) + " finalizado");
    //ALARMA TERMINADO
    alarmaTerminado();
    delay(1000);
  }
}

// ----------------- FUNCIONES DE UTILIDAD -----------------

void alarmaTerminado(){
  tone(buzzer, FRECUENCIA_TERMINADO, 100);
  delay(1000);
  tone(buzzer, FRECUENCIA_TERMINADO, 100);
  delay(1000);
  tone(buzzer, FRECUENCIA_TERMINADO, 100);
  delay(1000);
}

unsigned long bloquearPuerta() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CIERRE LA PUERTA");
  lcd.setCursor(1, 1);
  lcd.print("PARA COMENZAR!");

  unsigned long pausaInicio = millis();
  while (digitalRead(interruptor) == HIGH) {
    delay(50);
  }

  lcd.clear();
  return millis() - pausaInicio;
}

void pausaRepeticion(unsigned long pausaInicio, unsigned long pausaPausada, unsigned long pausaEntreRepeticiones) {
  int segundosPrevios = -1;

  while ((millis() - pausaInicio - pausaPausada) < pausaEntreRepeticiones) {
    
    //TESTEO BUZZER FRECUENCIA BAJA
    tone(buzzer, FRECUENCIA_BAJA, 1000);
    
    if (verificarCancelacion()) {
      cancelado = true;
      return;
    }

    if (digitalRead(interruptor) == HIGH) {
      digitalWrite(LED, LOW);
      analogWrite(MOTOR, 0);
      pausaPausada += bloquearPuerta();
    }

    unsigned long restante = pausaEntreRepeticiones - (millis() - pausaInicio - pausaPausada);
    int segundosRestantes = restante / 1000 + 1;

    if (segundosRestantes != segundosPrevios) {
      segundosPrevios = segundosRestantes;
      lcd.setCursor(0, 0);
      lcd.print("ESPERANDO");
      lcd.setCursor(0, 1);
      lcd.print(segundosRestantes);
      lcd.print(" s   ");
    }

    delay(50);
  }
}

bool verificarCancelacion() {
  char tecla = miTeclado.getKey();
  if (tecla == '*') {
    cancelado = true;
    return true;
  }
  return false;
}

// Procedimiento Programa Personalizado
void programaUsuario() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Config Usuario");

  unsigned long valores[3];  // [0]=coccion, [1]=pausa, [2]=repes
  const char* mensajes[3] = {"Tiempo Calor:", "Tiempo Apagado:", "Repeticiones:"};

  for (int paso = 0; paso < 3; paso++) {
    String input = "";
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(mensajes[paso]);
    lcd.setCursor(0, 1);
    lcd.print("> ");

    while (true) {
      char tecla = miTeclado.getKey();

      if (tecla >= '0' && tecla <= '9') {
        if (input.length() < 4) {  // Limita a 4 dígitos
          input += tecla;
          lcd.setCursor(2, 1);
          lcd.print(input + "    "); // Limpia residuos
        }
      } else if (tecla == '#') {
        if (input.length() > 0) {
          valores[paso] = input.toInt();
          break;  // pasa al siguiente paso
        }
      } else if (tecla == '*') {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Cancelado");
        delay(1000);
        lcd.clear();
        return;
      }
      delay(100);
    }
  }

  // Asignar valores globales
  tiempoUsuarioCoccion = valores[0] * 1000;
  tiempoUsuarioPausa   = valores[1] * 1000;
  repeticionesUsuario  = valores[2];
  usuarioConfigurado = true;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("GUARDADO OK!");
  delay(1000);
  lcd.clear();
}

//MENU PARA CONFIGURACION DE PROGRAMAS PERSONALIZADOS
void menuConfiguracion() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CONFIG MICRONDAS");
  lcd.setCursor(0, 1);
  lcd.print("1:Personaliz *:Salir");

  while (true) {
    char tecla = miTeclado.getKey();

    if (tecla == '1') {
      programaUsuario();
      break;
    } else if (tecla == '*') {
      lcd.clear();
      lcd.print("VOLVIENDO...");
      delay(1000);
      lcd.clear();
      break;
    }

    delay(100);
  }
}

