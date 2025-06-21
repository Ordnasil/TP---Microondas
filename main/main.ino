#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

#define LED 2
#define buzzer 11
#define MOTOR 3
#define interruptor A1
#define FRECUENCIA_ALTA 8
#define FRECUENCIA_BAJA 3
#define FRECUENCIA_TERMINADO 720
int rpm = 2; //La velocidad del motor del plato.
bool cancelado = false; // Con este bool activo la cancelacion usando '*'

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

// ---------------------------------- SET UP ----------------------------------
void setup() {
  Serial.begin(9600); //Lo uso para control de input de tecla y finalizacion de programa
  //Inicializo el LCD I2C
  lcd.init();
  lcd.clear();
  lcd.backlight();

  // Configuro los pines de los componentes
  pinMode(LED, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(MOTOR, OUTPUT);
  pinMode(interruptor, INPUT); //Este si bien no es necesario definir porque esta sobre un
  							   //Analog In. Lo configuro para poder llamarlo mas facil despues

  // --- Programa D pre-guardado para simular la EEPROM ---
  if (!usuarioConfigurado) {
    tiempoUsuarioCoccion = 10000;
    tiempoUsuarioPausa = 2000;
    repeticionesUsuario = 3;
    usuarioConfigurado = true;
  }
}
// --------------------------------------------------------------------
// --------------------- VOID LOOP ------------------
void loop() {
  // MENU PRINCIPAL EN DISPLAY
  lcd.setCursor(0,0);
  lcd.print("ELIGE UNA OPCION");
  lcd.setCursor(0,1);
  lcd.print("<< A B C D # >>");

  // BLOQUEO DE PUERTA
  if (digitalRead(interruptor) == HIGH) {
    bloquearPuerta();
  }

  // LECTURA DE TECLA INPUT SOBRE KEYPAD.
  // Esto inicia todos los programas.
  char tecla = miTeclado.getKey();
  if (tecla) {
    lcd.clear();
    lcd.setCursor(0, 0);
    Serial.println(tecla);
    delay(200);

    // Determine el caracter '#' como entrada al menu de configuracion de programa 
    // personalizado
    if (tecla == '#') {
      menuConfiguracion();  // CONFIGURACION PROGRAMA PERSONALIZADO
    } 
    else if (tecla >= '1' && tecla <= '9') {
      int cantidadRepes = tecla - '0'; // Tomo valor char a int para las repes
       coccionRapidaConRepe(cantidadRepes); //Esto incia la coccion rapida de 30seg que tiene
      									    // 'A' solo que le agrega repeticiones sin apagado.
    } 
    else {
      programa(tecla);  // A, B, C, D
    }

    lcd.clear();
  }
}
// ------------------------------------------------------
/*
||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
*/
// *********************** FUNCIONES Y PROCEDIMIENTOS *************************************

// ----------------- OPCIONES MENU PRINCIPAL -----------------
void programa(char opcion) {
  cancelado = false;

  switch(opcion) {
    case 'A':
    	coccionRapida();
    break;
    case 'B':
    	descongelado();
    break;
    case 'C':
    	recalentado();
    break;
    case 'D': 
    if (usuarioConfigurado){
    modoPrograma(tiempoUsuarioCoccion, repeticionesUsuario, tiempoUsuarioPausa, "PERSONALIZADO");
  	}else{
      lcd.print("NO CONFIGURADO");
      delay(1500);
  		} 
    break;
    default:
    // Este default incluye el caso de presionar '0' en menu principal
      Serial.println("Opcion invalida");
      lcd.clear();
      lcd.print("OPCION INVALIDA");
      delay(2000);
  }
}
// -----------------------------------------------------------------------------------------
// ------------------ TIPO DE COCCIONES EXCLUYENDO LA PERSONALIZADA --------------------------
void coccionRapidaConRepe(int cantidadrepe){
  modoPrograma(30000, cantidadrepe, 0, "COCCION RAPIDA");
  cantidadRepes = 0;
}

void coccionRapida() {
  modoPrograma(30000, 1, 0, "COCCION RAPIDA");
}

void descongelado() {
  modoPrograma(20000, 5, 10000, "DESCONGELAR");
}

void recalentado() {
  modoPrograma(15000, 3, 3000, "RECALENTAR");
}
// -------------------------------------------------------------------------------------
// ----------------- COCCION CORE ---------------------------------------------------
/* Este procedimiento es el mas importante. Es de aspecto genérico para poder reutilizarlo
   con cualquier tipo de coccion, incluyendo la personalizada. */
void modoPrograma(unsigned long duracionCoccion,
                  int repeticiones,
                  unsigned long pausaEntreRepeticiones,
                  const char* nombrePrograma) {

  for (int r = 0; r < repeticiones; r++) {//Iteraciones dependiendo de como llame a modoPrograma
    if (cancelado) break;//Pregunto si presionaron '*'

    //Inicializo las siguientes variables para administrar los tiempo y también para manejar
    //eventos, ya sea por pausado, por apertura de puerta o cancelacion.
    unsigned long tiempoInicio = millis();
    unsigned long tiempoPausado = 0;
    bool pausado = false;
    int segundosPrevios = -1;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(nombrePrograma);

    //Arrancamos motor y LED
    digitalWrite(LED, HIGH);
    analogWrite(MOTOR, rpm);

    while ((millis() - tiempoInicio - tiempoPausado) < duracionCoccion) {
      //Inicia el programa y cuenta regresiva sin tener en cuenta las pausas
      
      //MAGNETRON EN FUNCIONAMIENTO
      tone(buzzer, FRECUENCIA_ALTA, 1000);
      
      if (verificarCancelacion()) {
        cancelado = true;
        break;
      }

      //Evento de interruptor que abre la puerta
      if (digitalRead(interruptor) == HIGH) {
        pausado = true;
        digitalWrite(LED, LOW);
        analogWrite(MOTOR, 0);
        tiempoPausado += bloquearPuerta();
        pausado = false;
        //Aca tengo que setear el cursor de nuevo ya que bloquearPuerta() me
        //hace un lcd.clear() para imprimir su propio mensaje.
        lcd.setCursor(0,0);
        lcd.print(nombrePrograma);
        digitalWrite(LED, HIGH);
        analogWrite(MOTOR, rpm);
      }

      if (!pausado) {
        //Calculo los milisegundos reales restantes
        unsigned long tiempoRestante = duracionCoccion - (millis() - tiempoInicio - tiempoPausado);
        //Convierto los ms en s. Al final le sumo 1 para que el LCD haga el conteo sin tener
        //en cuenta el 0. Sino en un programa de 5seg el LCD muetra del 4 al 0
        int segundosRestantes = tiempoRestante / 1000 + 1;

        //Este if me reconfigura el LCD cuando pasamos de 2 dígitos a 1
 		//De lo contrario cuando pasamos de 10seg a 9 seg el LCD mostraria 10...90...80
        if (segundosRestantes != segundosPrevios) {
          segundosPrevios = segundosRestantes;
          lcd.setCursor(0, 1);
          lcd.print(segundosRestantes);
          lcd.print(" s   ");
        }
      }

      delay(50);
    }
	
    //Bajamos motor y led
    analogWrite(MOTOR, 0);
    digitalWrite(LED, LOW);

    if (cancelado) break;
	
    //Manejo del programa en las pausas acorde al programa (sus repeticiones)
    if (r < repeticiones - 1) {
      lcd.clear();
      pausaRepeticion(millis(), 0, pausaEntreRepeticiones);//Funcion explicada mas abajo
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
    Serial.println(String(nombrePrograma) + " FINALIZADO");
    //ALARMA TERMINADO
    alarmaTerminado();
    delay(1000);
  }
}

// ----------------- FUNCIONES DE UTILIDAD -----------------
//Sin mucho que explicar. Llamamos a este void para avisar el fin de un programa. Simple rutina
void alarmaTerminado(){
  tone(buzzer, FRECUENCIA_TERMINADO, 100);
  delay(1000);
  tone(buzzer, FRECUENCIA_TERMINADO, 100);
  delay(1000);
  tone(buzzer, FRECUENCIA_TERMINADO, 100);
  delay(1000);
}

//Esta funcion retorna los segundos que estuvo el programa en pausa por bloqueo de puerta.
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

//Parecido al 'void modoPrograma'. También manejamos los millis() pero sobre el evento de pausa
//La logica es muy parecida. Cambia en que tiene un unico evento de interrupcion que seria
//la activación del interruptor
void pausaRepeticion(unsigned long pausaInicio, unsigned long pausaPausada, unsigned long pausaEntreRepeticiones) {
  int segundosPrevios = -1;

  while ((millis() - pausaInicio - pausaPausada) < pausaEntreRepeticiones) {
    
    //MAGNETRON EN 'PAUSA'
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

// Nos deja cancelar cualquier operacion con '*'
bool verificarCancelacion() {
  char tecla = miTeclado.getKey();
  if (tecla == '*') {
    cancelado = true;
    return true;
  }
  return false;
}

// Procedimiento Programa Personalizado
// Esto nos habilita el ciclo de opciones para que el usuario pueda crear su propio
// programa de coccion.
void programaUsuario() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Config Usuario");

  //  Arrays para setear duracion de coccion, de pausa y cantidad de repeticiones
  unsigned long valores[3];  // [0]=coccion, [1]=pausa, [2]=repes
  const char* mensajes[3] = {"TIEMPO CALOR:", "TIEMPO APAGADO:", "REPETICIONES:"};
  
  for (int paso = 0; paso < 3; paso++) {// Inicio de configuracion de programa por input
    String input = "";
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(mensajes[paso]);
    lcd.setCursor(0, 1);
    lcd.print("> ");

    while (true) {//Esperamos lectura de input. Cuando se presione #, pasamos al siguiente paso
      char tecla = miTeclado.getKey();

      if (tecla >= '0' && tecla <= '9') {
        if (input.length() < 4) {  // Limito a 4 dígitos
          input += tecla;
          lcd.setCursor(2, 1);
          lcd.print(input + "    ");
        }
      } else if (tecla == '#') {
        if (input.length() > 0) {
          valores[paso] = input.toInt();
          break;  // pasa al siguiente paso
        }
      } else if (tecla == '*') {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("CANCELADO");
        delay(1000);
        lcd.clear();
        return;
      }
      delay(100);
    }
  }

  // Asigno valores globales
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
void menuConfiguracion() {//Este es nuestro sub-menu con dos opciones para personalizar coccion
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CONFIG MICRONDAS");
  lcd.setCursor(0, 1);
  lcd.print("1:SETEAR *:SALIR"); //Decidí activar la configuracion con la tecla '1'

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



