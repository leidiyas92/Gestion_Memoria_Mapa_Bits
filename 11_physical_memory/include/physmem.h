/**
 * @file
 * @ingroup kernel_code 
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License. 
 * @brief Contiene las definiciones relacionadas con las gesti�n
 * de memoria del kernel.
 */

#ifndef PHYSMEM_H_
#define PHYSMEM_H_

/** @brief Localizacion del mapa de bits de memoria. */
#define MMAP_LOCATION 0x500

 /** @brief Tama�o de la unidad de asignaci�n de memoria  */
#define MEMORY_UNIT_SIZE 4096

/** @brief N�mero de bytes que tiene una entrada en el mapa de bits */
#define BYTES_PER_ENTRY sizeof(unsigned int)

#define BITS_PER_ENTRY (8 * BYTES_PER_ENTRY)

/** @brief N�mero de unidades en la memoria disponible */
#define MEMORY_UNITS (memory_length / MEMORY_UNIT_SIZE)

/** @brief Entrada en el mapa de bits correspondiente a una direcci�n */
#define bitmap_entry(addr) \
	( addr /  MEMORY_UNIT_SIZE) / ( BITS_PER_ENTRY )

/** @brief Desplazamiento en bits dentro de la entrada en el mapa de bits */
#define bitmap_offset(addr) \
	(addr / MEMORY_UNIT_SIZE) % ( BITS_PER_ENTRY )

/** @brief Funci�n que redondea una direcci�n de memoria a la direcci�n
 *  m�s cercana por debajo que sea m�ltiplo de MEMORY_UNIT_SIZE */
static __inline__ unsigned int round_down_to_memory_unit(addr) {
	if (addr < 0) { return 0; }

	volatile remainder = addr % MEMORY_UNIT_SIZE;

    return addr - remainder;
}

/** @brief Funci�n que redondea una direcci�n de memoria a la direcci�n
 *  m�s cercana por encima que sea m�ltiplo de MEMORY_UNIT_SIZE */
static __inline__ unsigned int round_up_to_memory_unit(addr) {

	if (addr < 0) { return 0; }

	volatile remainder = addr % MEMORY_UNIT_SIZE;

	if (remainder > 0) {
		return addr + MEMORY_UNIT_SIZE - remainder;
	}

	return addr;

}

/**
 * @brief Esta rutina inicializa el mapa de bits de memoria,
 * a partir de la informacion obtenida del GRUB.
 */
void setup_memory(void);

/**
 @brief Busca una unidad libre dentro del mapa de bits de memoria.
 * @return Direcci�n de inicio de la unidad en memoria.
 */
char * allocate_unit(void);

/** @brief Busca una regi�n de memoria contigua libre dentro del mapa de bits
 * de memoria.
 * @param length Tama�o de la regi�n de memoria a asignar.
 * @return Direcci�n de inicio de la regi�n en memoria.
 */
char * allocate_unit_region(unsigned int length);

/**
 * @brief Permite liberar una unidad de memoria.
 * @param addr Direcci�n de memoria dentro del �rea a liberar.
 */
void free_unit(char *addr);

/**
 * @brief Permite liberar una regi�n de memoria.
 * @param start_addr Direcci�n de memoria del inicio de la regi�n a liberar
 * @param length Tama�o de la regi�n a liberar
 */
void free_region(char *start_addr, unsigned int length);

#endif /* PHYSMEM_H_ */
