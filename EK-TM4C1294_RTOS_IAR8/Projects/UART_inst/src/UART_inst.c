#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "system_tm4c1294.h"
#include "cmsis_os2.h"

// includes do driverlib
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/pin_map.h"
#include "driverlib/uart.h"

#include "inc/hw_memmap.h"
#include "utils/uartstdio.h"

osThreadId_t elevadorEsquerda_id, elevadorCentral_id, elevadorDireita_id, leituraUART_id, escritaUART_id;
osMessageQueueId_t filaMensagensEsquerda_id, filaMensagensDireita_id, filaMensagensCentral_id, filaUartEscrita;

typedef struct {
  int andar; //Andar atual
  char posicao; //Esquerdo, direito ou central
  osMessageQueueId_t fila;
}Elevador;

extern void UARTStdioIntHandler(void);

void abrirPortas(Elevador elevador); //Função para abrir a porta do elevador
void fecharPortas(Elevador elevador); //Função para fechar a porta do elevador
void elevadorSubindo(Elevador elevador); //Função que faz o elevador subir
void elevadorDescendo(Elevador elevador); //Função que faz o elevador descer
void elevadorParado(Elevador elevador); //Função que faz o elevador parar
void elevadorPararAbrirFechar(Elevador elevador); //Função que para, abre e fecha a porta do elevador
int converterLetraEmNumeroAndar(char andar_num); //Função que converte a letra do botão para o número do andar
int converterDezenaEUnidadeAndar(char dezena, char unidade); //Função que converte o comando de dezena e unidade no número do andar
int leituraAndar(char dezena, char unidade); //Função que traz a leitura do andar atual

//Configuração da UART
void UARTInit(void){
  // Enable the GPIO Peripheral used by the UART.
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
  while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOA));
  
  // Enable UART0
  SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
  while(!SysCtlPeripheralReady(SYSCTL_PERIPH_UART0));
  
  // Configure GPIO Pins for UART mode.
  GPIOPinConfigure(GPIO_PA0_U0RX);
  GPIOPinConfigure(GPIO_PA1_U0TX);
  GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
  
  // Initialize the UART for console I/O.
  UARTStdioConfig(0,115200, SystemCoreClock);
} // UARTInit

void UART0_Handler(void){
  UARTStdioIntHandler();
} // UART0_Handler

//Thread de Leitura da UART
void leituraUart(void *arg){
  char comando[6];
  while(1){
    UARTgets(comando,20);
    if(comando[0]=='e'){
      osMessageQueuePut(filaMensagensEsquerda_id, comando, NULL, 0);
    }
    else if(comando[0]=='c'){
      osMessageQueuePut( filaMensagensCentral_id, comando, NULL, 0);
    }
    else if(comando[0]=='d'){
      osMessageQueuePut(filaMensagensDireita_id, comando, NULL, 0);
    }  
    osThreadYield();
  }
}

//Thread da escrita da UART
void escritaUart(void *arg){
  char comando[2];
  while(1){
    osMessageQueueGet(filaUartEscrita, comando, NULL, osWaitForever);
    UARTprintf("%c%c\r",comando[0],comando[1]); 
  }
}

//Thread do Elevador
void elevador(void *arg){
  
  int andar_destino;
  
  Elevador elevador;
  elevador.andar=0;
  elevador.posicao=(uint32_t)(arg);

  if(elevador.posicao=='e'){
    elevador.fila = filaMensagensEsquerda_id;
  }
  else if(elevador.posicao=='d'){
    elevador.fila = filaMensagensDireita_id;
  }
  else if(elevador.posicao=='c'){
    elevador.fila =  filaMensagensCentral_id;
  }
  
  elevadorPararAbrirFechar(elevador);

  while(1){
    
    char comando[10];
    osMessageQueueGet(elevador.fila, comando, NULL, osWaitForever);
    
    if(comando[1]=='E')
    {
      andar_destino = converterDezenaEUnidadeAndar(comando[2],comando[3]);
      if(andar_destino == elevador.andar){
        elevadorPararAbrirFechar(elevador);
      }
      if(andar_destino > elevador.andar){
        fecharPortas(elevador);
        elevadorSubindo(elevador);
      }
        else{
          fecharPortas(elevador);
          elevadorDescendo(elevador);
      }
    }
    else if(comando[1]=='I'){
      andar_destino=converterLetraEmNumeroAndar(comando[2]);
      if(andar_destino>elevador.andar){
          fecharPortas(elevador);
          elevadorSubindo(elevador);
      }
      else if(andar_destino<elevador.andar){
          fecharPortas(elevador);
          elevadorDescendo(elevador);
      }
      else{
        elevadorPararAbrirFechar(elevador);
      }
    }
    else if(comando[1]>='0' && comando[1]<='9')
    {
      elevador.andar=leituraAndar(comando[1],comando[2]);
      if(elevador.andar < andar_destino){
        elevadorSubindo(elevador);
      }
      else if(elevador.andar > andar_destino){
        elevadorDescendo(elevador);
      }
      else{
        elevadorPararAbrirFechar(elevador);
      }
    }  
  }
}


//Funções ------
void abrirPortas(Elevador elevador){
  char mensagem[2];
  mensagem[0] = elevador.posicao;
  mensagem[1] = 'a';
  osMessageQueuePut(filaUartEscrita, mensagem, NULL, osWaitForever);
  osDelay(1500);
}

void fecharPortas(Elevador elevador){
  char mensagem[2];
  mensagem[0] = elevador.posicao;
  mensagem[1] = 'f';
  osMessageQueuePut(filaUartEscrita, mensagem, NULL, osWaitForever);
  osDelay(1500);
}

void elevadorSubindo(Elevador elevador){
  char mensagem[2];
  mensagem[0] = elevador.posicao;
  mensagem[1] = 's';
  osMessageQueuePut(filaUartEscrita, mensagem, NULL, osWaitForever);
}

void elevadorDescendo(Elevador elevador){
  char mensagem[2];
  mensagem[0] = elevador.posicao;
  mensagem[1] = 'd';
  osMessageQueuePut(filaUartEscrita, mensagem, NULL, osWaitForever);
}

void elevadorParado(Elevador elevador){
  char mensagem[2];
  mensagem[0] = elevador.posicao;
  mensagem[1] = 'p';
  osMessageQueuePut(filaUartEscrita, mensagem, NULL, osWaitForever);
}

void elevadorPararAbrirFechar(Elevador elevador){
  elevadorParado(elevador);
  abrirPortas(elevador);
  fecharPortas(elevador);
}

int converterLetraEmNumeroAndar(char andar_num){
  int andar;
  if(andar_num == 'a')     {andar = 0;}
  else if(andar_num == 'b'){andar = 1;}
  else if(andar_num == 'c'){andar = 2;}
  else if(andar_num == 'd'){andar = 3;}
  else if(andar_num == 'e'){andar = 4;}
  else if(andar_num == 'f'){andar = 5;}
  else if(andar_num == 'g'){andar = 6;}
  else if(andar_num == 'h'){andar = 7;}
  else if(andar_num == 'i'){andar = 8;}
  else if(andar_num == 'j'){andar = 9;}
  else if(andar_num == 'k'){andar = 10;}
  else if(andar_num == 'l'){andar = 11;}
  else if(andar_num == 'm'){andar = 12;}
  else if(andar_num == 'n'){andar = 13;}
  else if(andar_num == 'o'){andar = 14;}
  else if(andar_num == 'p'){andar = 15;}
  return andar;
}

int converterDezenaEUnidadeAndar(char dezena, char unidade){
  return (dezena - 0x30) * 10 + (unidade - 0x30);  
}

int leituraAndar(char dezena, char unidade){
  if(unidade=='\0'){
    return   (dezena - 0x30);
  }
  return (dezena - 0x30) * 10 + (unidade - 0x30);
}

void main(void){
  
  UARTInit();
  
  SystemInit();
  
  osKernelInitialize();
  
  filaMensagensEsquerda_id =  osMessageQueueNew (20, sizeof(char)*6, NULL);
  filaMensagensCentral_id =  osMessageQueueNew (20, sizeof(char)*6, NULL); 
  filaMensagensDireita_id =  osMessageQueueNew (20, sizeof(char)*6, NULL);
  
  elevadorEsquerda_id = osThreadNew(elevador, (void*)'e', NULL);
  elevadorCentral_id = osThreadNew(elevador, (void*)'c', NULL);
  elevadorDireita_id = osThreadNew(elevador, (void*)'d', NULL);
  
  leituraUART_id= osThreadNew(leituraUart, NULL, NULL);
  escritaUART_id= osThreadNew(escritaUart, NULL, NULL);
  
  filaUartEscrita =  osMessageQueueNew (20, sizeof(char)*6, NULL);
    
  if(osKernelGetState() == osKernelReady)
    osKernelStart();
  
  while(1);
}