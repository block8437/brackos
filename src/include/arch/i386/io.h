#include <stdint.h>

static inline void outportb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outportw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outportl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inportb(uint16_t port) {
    uint8_t out;
    __asm__ volatile ("inb %1, %0" : "=a"(out) : "Nd"(port));
    return out;
}

static inline uint16_t inportw(uint16_t port) {
    uint16_t out;
    __asm__ volatile ("inw %1, %0" : "=a"(out) : "Nd"(port));
    return out;
}

static inline uint32_t inportl(uint16_t port) {
    uint32_t out;
    __asm__ volatile ("inl %1, %0" : "=a"(out) : "Nd"(port));
    return out;
}

static inline void io_wait(void) {
    __asm__ volatile ( "jmp 1f\n"
                       "1:jmp 2f\n"
                       "2:" );
}
