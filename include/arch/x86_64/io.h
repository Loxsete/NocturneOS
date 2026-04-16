#ifndef IO_H
#define IO_H

#include <stdint.h>


// Read a byte from I/O ports
uint8_t inb(uint16_t port);

// Read a word from I/O ports
uint16_t inw(uint16_t port);

// Read a double word from I/O ports
uint32_t inl(uint16_t port);

// Write a byte to I/O ports
void outb(uint16_t port, uint8_t val);

// Write a word to I/O ports
void outw(uint16_t port, uint16_t val);

// Write a double word to I/O ports
void outl(uint16_t port, uint32_t val);

// Wait for I/O operations to complete
void io_wait();

#endif