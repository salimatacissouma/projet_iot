/*
 * Copyright: Olivier Gruber (olivier dot gruber at acm dot org)
 *
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 */
#include "main.h"
#include "uart.h"
#include "isr.h"
#include "uart-mmio.h"
#include "isr-mmio.h"




extern uint32_t irq_stack_top;
extern uint32_t stack_top;

void check_stacks() {
  void *memsize = (void*)MEMORY;
  void *addr;
  addr = &stack_top;
  if (addr >= memsize)
    panic();

  addr = &irq_stack_top;
  if (addr >= memsize)
    panic();
}



void uart0_receive_handler(void *cookie) {
  char c;
  uart_receive(UART0, &c);
  while (c) {
    uart_send(UART0, c);
    uart_receive(UART0, &c);
  }
}




void uart_enable_rx_interrupt(void* uart) {
  uart_send_string(UART0, ">>> [INTERRUPT] Entered uart0_receive_handler\n");
  volatile uint32_t *uart_imsc = (volatile uint32_t *)(uart + UART_IMSC);
  volatile uint32_t *uart_icr  = (volatile uint32_t *)(uart + UART_ICR);

  *uart_icr = 0x7FF;        // Clear all pending interrupts
  *uart_imsc |= (1 << 4);   // RXIM (bit 4) = enable RX interrupt
}

void uart0_interrupt_handler(uint32_t vicirq, void* cookie) {
  uint32_t irqs = mmio_read32((void *)UART0_BASE_ADDRESS, UART_MIS);
  if (irqs & UART_IMSC_RXIM) {
    uart0_receive_handler(cookie);
  } else {
    panic();
  }
}

void irq_handler(void) {
  uint32_t irq = mmio_read32((void*)VIC_BASE_ADDR, VICIRQSTATUS);
  // dispatch en fonction du périphérique
  if (irq & (1 << UART0_IRQ)) {
    uart0_interrupt_handler(UART0_IRQ, NULL);
  } else {
    uart_send_string(UART0, "[IRQ] Unknown IRQ source.\n");
  }
}
/**
 * This is the C entry point,
 * upcalled once the hardware has been setup properly
 * in assembly language, see the startup.s file.
 */
void _start(void) {
 check_stacks();
  uarts_init();                // Initialise tous les UARTs
  uart_enable(UART0);          // Active UART0
  uart_send_string(UART0, ">>> [BOOT] _start() called.\n");
  vic_setup_irqs();            // Initialise les IRQs
  vic_enable_irq(UART0_IRQ, &uart0_interrupt_handler, NULL);  // Enregistre le handler pour UART0
  uart_enable_rx_interrupt(UART0_BASE_ADDRESS); // Active RX dans PL011 (IMSC)
  core_enable_irqs();  

  for(;;) {
    core_halt();  // Attente d'interruption (wfi)
  }
}


void panic() {
  for(;;)
    ;
}

