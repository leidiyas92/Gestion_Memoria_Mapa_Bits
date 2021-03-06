/**
 * @file
 * @ingroup kernel_code 
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License. 
 * @brief Contiene la implementaci�n de las rutinas relacionadas
 * con las gesti�n de memoria f�sica. La memoria se gestiona en unidades
 * de 4096 bytes.
 */

#include <physmem.h>
#include <multiboot.h>
#include <stdio.h>
#include <stdlib.h>

/** @brief Mapa de bits de memoria disponible
 * @details Esta variable almacena el apuntador del inicio del mapa de bits
 * que permite gestionar las unidades de memoria. Es decir la direccion en donde
 * esta el mapa de memoria (0x500 la primera region de memoria libre cuando arranca
 * un computador)*/
 unsigned int * memory_bitmap =(unsigned int*)MMAP_LOCATION;

 /** @brief Siguiente unidad disponible en el mapa de bits */
 unsigned int next_free_unit;

 /** @brief Numero de marcos libres en la memoria */
 int free_units;

 /** @brief Numero total de unidades en la memoria */
 int total_units;

 /** @brief Marco inicial de las unidades  disponibles en memoria */
 unsigned int base_unit;

 /** @brief Tama�o del mapa de bits en memoria.
  * @details
  * Para un espacio fisico de maximo 4 GB, se requiere un mapa de bits
  * de 128 KB. Si cada entrada ocupa 4 bytes, se requiere 32678 entradas.
  * @verbatim
   ~(0x0) / (MEMORY_UNIT_SIZE * BITS_PER_ENTRY)
       |                |               |
       |                |               |_ _ bits que tiene cada entrada
       |                |                     en la tabla.
       |	    tama�o de una unidad de memoria
       |
	  pone los 32 bits en 1. Esto representa el maximo
	  numero que se puede guardar
	  en un registro del procesador (4GB - 1).
  @endverbatim

  *Esto nos permite saber el total de elementos que tiene la tabla.
  */
unsigned int memory_bitmap_length =
		~(0x0) / (MEMORY_UNIT_SIZE * BITS_PER_ENTRY);

/** @brief Variable global del kernel que almacena el inicio de la regi�n
 * de memoria disponible */
unsigned int memory_start;
/** @brief Variable global del kernel que almacena el tama�o en bytes de
 * la memoria disponible */
unsigned int memory_length;

/** @brief M�nima direcci�n de memoria permitida para liberar */
unsigned int allowed_free_start;

/**
 * @brief Esta rutina inicializa el mapa de bits de memoria,
 * a partir de la informacion obtenida del GRUB.
 * para esto limpia el mapa de bits, es decir coloca todos los
 * bits en 0.
 * luego se asigna la minima direccion de memoria que se puede liberar
 * (la direccion lineal donde termina el kernel). Se verifica si los campos
 * mmap_length y mmap_addr son validos (si flags[6] = 1).
 * inmediatamente verifica si la rgion de memoria son validas y extrae
 * la region de memoria mas grande disponible siempre y cuando su direccion
 * base sea mayor o igual a la posicion del kernel.
 * se establece esta memoria como disponible para liberar memoria.
 */
void setup_memory(void){

	extern multiboot_header_t multiboot_header;
	extern unsigned int multiboot_info_location;

	/* Variables temporales para hallar la region de memoria disponible */
	unsigned int tmp_start;
	unsigned int tmp_length;
	unsigned int tmp_end;
	int mod_count;
	multiboot_info_t * info = (multiboot_info_t *)multiboot_info_location;

	int i;

	unsigned int mods_end; /* Almacena la direcci�n de memoria final
	del ultimo modulo cargado, o 0 si no se cargaron modulos. */

	/*printf("Bitmap array size: %d\n", memory_bitmap_length);*/

	for(i=0; i<memory_bitmap_length; i++){
		memory_bitmap[i] = 0;
	}

	/*
	printf("Inicio del kernel: %x\n", multiboot_header.kernel_start);
	printf("Fin del segmento de datos: %x\n", multiboot_header.data_end);
	printf("Fin del segmento BSS: %x\n", multiboot_header.bss_end);
	printf("Punto de entrada del kernel: %x\n", multiboot_header.entry_point);
	*/

	/* si flags[3] = 1, se especificaron m�dulos que deben ser cargados junto
	 * con el kernel*/

	mods_end = 0;

	if (test_bit(info->flags, 3)) {
		mod_info_t * mod_info;
		/*
		printf("Modules available!. Start: %u Count: %u\n", info->mods_addr,
				info->mods_count);
		*/
		for (mod_info = (mod_info_t*)info->mods_addr, mod_count=0;
				mod_count <info->mods_count;
				mod_count++, mod_info++) {
			/*
			printf("[%d] start: %u end: %u cmdline: %s \n", mod_count,
					mod_info->mod_start, mod_info->mod_end,
					mod_info->string);*/
			if (mod_info->mod_end > mods_end) {
				/* Los modulos se redondean a limites de 4 KB, redondear
				 * la direcci�n final del modulo a un limite de 4096 */
				mods_end = mod_info->mod_end + (mod_info->mod_end % 4096);
			}
		}
	}

	//printf("Mods end: %u\n", mods_end);

	/* si flags[6] = 1, los campos mmap_length y mmap_addr son validos */

	/* Revisar las regiones de memoria, y extraer la region de memoria
	 * de mayor tamano, maracada como disponible, cuya direcci�n base sea
	* mayor o igual a la posicion del kernel en memoria.
	*/

	memory_start = 0;
	memory_length = 0;

	free_units = 0;

	/**@brief inicio pemitido para liberar
	 * Suponer que el inicio de la memoria disponible se encuentra
	 * al finalizar el kernel
	 * allowed_free_start me dice cual es la direccion lineal en donde termina el kernel
	 */
	allowed_free_start = round_up_to_memory_unit(multiboot_header.bss_end);

	/** Existe un mapa de memoria v�lido creado por GRUB? */
	if (test_bit(info->flags, 6)) {
		memory_map_t *mmap;/**si el bit 6 esta en 1 se crea un mapa valido de memoria*/

		/*printf ("mmap_addr = 0x%x, mmap_length = 0x%x\n",
			   (unsigned) info->mmap_addr, (unsigned) info->mmap_length);*/
		for (mmap = (memory_map_t *) info->mmap_addr;
			(unsigned int) mmap < info->mmap_addr + info->mmap_length;
			mmap = (memory_map_t *) ((unsigned int) mmap
									 + mmap->entry_size
									 + sizeof (mmap->entry_size))) {
		 printf (" size = 0x%x, base_addr = 0x%x%x,"
				 " length = 0x%x%x, type = 0x%x\n",
				  mmap->entry_size,
				  mmap->base_addr_high,
				  mmap->base_addr_low,
				  mmap->length_high,
				  mmap->length_low,
				  mmap->type);

	  /** Verificar si la regi�n de memoria cumple con las condiciones
	   * para ser considerada "memoria disponible".
	   *
	   * Importante: Si se supone un procesador de 32 bits, los valores
	   * de la parte alta de base y length (base_addr_high y
	   * length_high) son cero. Por esta razon se pueden ignorar y solo
	   * se usan los 32 bits menos significativos de base y length.
	   *
	   * Para que una region de memoria sea considerada "memoria
	   * disponible", debe cumplir con las siguientes condiciones:
	   *
	   * - Estar ubicada en una posicion de memoria mayor o igual que
	   * 	1 MB.
	   * - Tener su atributo 'type' en 1 = memoria disponible.
	   * */
		 /* La region esta marcada como disponible y su direcci�n base
		  * esta por encima de la posicion del kernel en memoria ?*/
		 if (mmap->type == 1 &&
			 mmap->base_addr_low >= multiboot_header.kernel_start) {
			 tmp_start = mmap->base_addr_low;
			 tmp_length = mmap->length_low;

			 /* Verificar si el kernel se encuentra en esta region */
			 if (multiboot_header.bss_end >= tmp_start &&
					 multiboot_header.bss_end <= tmp_start + tmp_length) {
				 //printf("Kernel is on this region!. Base: %u\n", tmp_start);
				 /* El kernel se encuentra en esta region. Tomar el inicio
				  * de la memoria disponible en la posicion en la cual
				  * finaliza el kernel
				 */
				 tmp_start = multiboot_header.bss_end;

				 /* Ahora verificar si ser cargaron modulos junto con el
				  * kernel. Estos modulos se cargan en regiones continuas
				  * al kernel.
				  * Si es asi, la nueva posicion inicial de la memoria
				  * disponible es la posicion en la cual terminan los modulos
				  * */
				 if (mods_end > 0 &&
						 mods_end >= tmp_start &&
						 mods_end <= tmp_start + tmp_length) {
						//printf("Adding module space...\n");
						tmp_start = mods_end;
				 }
				 /* Restar al espacio disponible.*/
				 tmp_length -= tmp_start - mmap->base_addr_low;
				 if (tmp_length > memory_length) {
					 memory_start = tmp_start;
					 memory_length = tmp_length; /* Tomar el espacio */
				 }
			 }else {
				 /* El kernel no se encuentra en esta region, verificar si
				  * su tamano es mayor que la region mas grande encontrada
				  * hasta ahora
				  */
				 if (tmp_length > memory_length) {
					 memory_start = tmp_start;
					 memory_length = tmp_length; /* Tomar el espacio */
				 }
			 }
		 }
		} //endfor
	}

	/* Existe una regi�n de memoria disponible? */
	if (memory_start > 0 && memory_length > 0) {
		/* Antes de retornar, establecer la minima direcci�n de memoria
		 * permitida para liberar*/

		//printf("Free units before setting up memory: %d\n", free_units);


		tmp_start = memory_start;
		/* Calcular la direcci�n en la cual finaliza la memoria disponible */
		tmp_end = tmp_start + tmp_length;

		/* Redondear el inicio y el fin de la regi�n de memoria disponible a
		 * unidades de memoria */
		tmp_end = round_down_to_memory_unit(tmp_end);
		tmp_start = round_up_to_memory_unit(tmp_start);

		/* Calcular el tama�o de la regi�n de memoria disponible, redondeada
		 * a l�mites de unidades de memoria */
		tmp_length = tmp_end - tmp_start;

		/* Actualizar las variables globales del kernel */
		memory_start = tmp_start;
		memory_length = tmp_length;

		/* Marcar la regi�n de memoria como disponible */
		free_region((char*)memory_start, memory_length);

		/* Establecer la direcci�n de memoria a partir
		 * de la cual se puede liberar memoria */
		allowed_free_start = memory_start;
		next_free_unit = allowed_free_start / MEMORY_UNIT_SIZE;

		total_units = free_units;
		base_unit = next_free_unit;

		/* printf("Available memory at: 0x%x units: %d Total memory: %d\n",
				memory_start, total_units, memory_length);*/
	}
 }

/** @brief Permite verificar si la unidad se encuentra disponible.
 * @param unit unidad a verificar
 * @return int que es la direccion en donde empieza la unidad de memoria
 *
 * @verbatim
   Ejemplo
   unit = 1000000 (bytes)
   entry= 1000000/ 32 (bytes) =31250 (bytes)
   offset= 1000000 % 32 =0
   retorna memory_bitmap[31250] & 0x1 << 0
   si retorna un 1 significa que la unidad se encuentra libre
   si retorna 0 significa que la unidad se encuentra ocupada.

                        ....   1    0
                  ---------------------
31250 --------->  |   |   |   |   | x |  <---------- x = bit a verificar
                  ---------------------
                  |   |   |   |   |   |
                  ---------------------
                            ....


  @endverbatim
 */
static __inline__ int test_unit(unsigned int unit) {
	 volatile entry = unit / BITS_PER_ENTRY;
	 volatile offset = unit % BITS_PER_ENTRY;
	 return (memory_bitmap[entry] & 0x1 << offset);
}


/** @brief  Permite marcar la unidad como ocupada.
 * @param unit unidad a verificar
*@verbatim
   Ejemplo:
   unit= 1000000 (bytes)
   entry = 1000000/32 = 31250 (bytes)
   offset = 1000000 % 32 = 0
   memory_bitmap[31250] &= ~(0x1 << 0)
   esto quiere decir que la unidad 1000000 que se va a marcar como ocupada (0)
   se encuentra ubicada dentro del mapa de bits en la region 31250 en el bit 0
   en esa region tal como se muestra en la siguiente figura

               ....
    -------------------------
    | 1 | 1 | 1 | 1 | 1 | 1 |
    -------------------------
    | 1 | 1 | 1 | 1 | 1 | 0 | <----- bit 0 de la posicion 31250
    -------------------------            de memory_bitmap
    | 1 | 1 | 1 | 1 | 1 | 1 |
    -------------------------
               .....
    @endverbatim
   */
static __inline__ void clear_unit(unsigned int unit) {
	 volatile entry = unit / BITS_PER_ENTRY;
	 volatile offset = unit % BITS_PER_ENTRY;
	 memory_bitmap[entry] &= ~(0x1 << offset);
}

/** @brief Permite marcar la unidad como libre.
 *@param unit unidad a verificar
 @verbatim
   Ejemplo:
   unit= 335000 (bytes)
   entry = 335000/32 = 10468 (bytes)
   offset = 335000 % 32 = 24
   memory_bitmap[10468] &= ~(0x1 << 24)
   esto quiere decir que la unidad 335000 que se va a marcar como libre (1)
   se encuentra ubicada dentro del mapa de bits en la region 10468 en el bit 24
   en esa region tal como se muestra en la siguiente figura.

       bit 24 de la posicion 10468 de memory_bitmap
              |
              |
              V  ....
    ---------------------------
    | 0 | 0 | 1|...| 0 | 0 | 0 |
    ---------------------------
    | 1 | 1 | 1|...| 1 | 1 | 1 |
    ---------------------------
    | 0 | 0 | 0|...| 0 | 0 | 0 |
    ---------------------------
    | 1 | 1 | 1|...| 1 | 1 | 1 |
    ---------------------------
               .....
    @endverbatim

  */
static __inline__ void set_unit(unsigned int unit) {
	 volatile entry = unit / BITS_PER_ENTRY;
	 volatile offset = unit % BITS_PER_ENTRY;
	 memory_bitmap[entry] |= (0x1 << offset);
}


/**
 @brief Busca una unidad libre dentro del mapa de bits de memoria.
 * @return Direcci�n de inicio de la unidad en memoria.
 * @verbatim
   Al inicio de esta funcion se verifica si no existen unidades
  libres lo cual retornaria 0. caso contrario Busca una unidad de memoria
  disponible. si al hacer la busqueda no encuentra una unidad disponible
  dentro del mapa de bits entonces retornara 0, caso contrario retorna
  la direccion de inicio de la unidad de memoria.
 @endverbatim
 */
  char * allocate_unit(void) {
	 unsigned int unit; /**unit cuenta las unidades disponibles.*/
	 unsigned int entry;
	 unsigned int offset;


	// printf ("%d ", free_units);
	 /* Si no existen unidades libres, retornar*/
	 if (free_units == 0) {
		 //printf("Warning! out of memory!\n");
		 return 0;
	 }

	/* Iterar por el mapa de bits*/
 	unit = next_free_unit;
 	 do {
 		 if (test_unit(unit)) {
 			 entry = unit / BITS_PER_ENTRY;
 			 offset = unit % BITS_PER_ENTRY;
 			 memory_bitmap[entry] &= ~(0x1 << offset);

 			 /* printf("\tFree unit at %x offset %d %b %x\n",
 					 unit * MEMORY_UNIT_SIZE,
 					 offset, memory_bitmap[entry], &memory_bitmap[entry]);*/
 			 /* Avanzar en la posicion de busqueda de la proxima unidad
 			  * disponible */
 			 next_free_unit++;
			 if (next_free_unit > base_unit + total_units) {
				 next_free_unit = base_unit;
			 }
 			/* Descontar la unidad tomada */

 			free_units--;
 			return (char*)(unit * MEMORY_UNIT_SIZE);
 		 }
 		unit++;
 		if (unit > base_unit + total_units) {
 			unit = base_unit;
 		}
 	 }while (unit != next_free_unit);

 	 return 0;
  }


  /** @brief Busca una regi�n de memoria contigua libre dentro del mapa de bits
   * de memoria.
   * @param length Tama�o de la regi�n de memoria a asignar.
   * @return Direcci�n de inicio de la regi�n en memoria.
   * @verbatim
     Al inicio de esta funcion se verifica si no existen regiones
     libres lo cual retornaria 0. caso contrario Busca una region de memoria que tenga
     un tama�o mayor o igual a length disponible. si al hacer la busqueda no encuentra
     una region disponible dentro del mapa de bits entonces retornara 0, caso contrario retorna
     la direccion de inicio de la region de memoria.
    @endverbatim
   */
  char * allocate_unit_region(unsigned int length) {
	unsigned int unit;
	unsigned int unit_count;
	unsigned int i;
	int result;

	unit_count = (length / MEMORY_UNIT_SIZE);

	if (length % MEMORY_UNIT_SIZE > 0) {
		unit_count++;
	}

	//printf("\tAllocating %d units\n", unit_count);

	if (free_units < unit_count) {
		 //printf("Warning! out of memory!\n");
		 return 0;
	}

	/* Iterar por el mapa de bits*/
	unit = next_free_unit;

	 /* Iterar por el mapa de bits*/
	unit = next_free_unit;
	 do {
		 if (test_unit(unit) &&
				 (unit + unit_count) < (base_unit + total_units)) {

			 result = 1;
			 for (i=unit; i<unit + unit_count; i++){
				 result = (result && test_unit(i));
			 }
			 /* Marcar la unidad como libre */
			 if (result) {
				 for (i=unit; i<unit + unit_count; i++){
					 //printf("\tFree unit at %x\n", i *  MEMORY_UNIT_SIZE);
					 /* Descontar la unidad tomada */
					 free_units--;
					 clear_unit(i);
				 }

				 /* Avanzar en la posicion de busqueda de la proxima unidad
				 			  * disponible */
				 next_free_unit = unit + unit_count;
				 if (next_free_unit > base_unit + total_units) {
					 next_free_unit = base_unit;
				 }

				return (char*)(unit * MEMORY_UNIT_SIZE);
			 }
		 }
		unit++;
		if (unit > base_unit + total_units) {
			unit = base_unit;
		}
	 }while (unit != next_free_unit);

	  return 0;
  }

/**
 * @brief Permite liberar una unidad de memoria.
 * @param addr Direcci�n de memoria dentro del �rea a liberar.
 @verbatim
  Verifica si la direccion que recibe es menor a la direccion del
  kernel o no, de ser asi sale sin realizar ninguna accion, caso
  contrario se convierte a una direccion lineal para posteriormente
  colocarla en el mapa de bits como una unidad disponible.
 @endverbatim*/
void free_unit(char * addr) {
	 unsigned int start;
	 unsigned int entry;
	 int offset;
	 unsigned int unit;

	 start = round_down_to_memory_unit((unsigned int)addr);

	 if (start < allowed_free_start) {return;}

	 unit = start / MEMORY_UNIT_SIZE;

	 set_unit(unit);

	 /* Marcar la unidad recien liberada como la proxima unidad
	  * para asignar */
	 next_free_unit = unit;

	 /* Aumentar en 1 el numero de unidades libres */
	 free_units ++;

 }

/**
 * @brief Permite liberar una regi�n de memoria.
 * @param start_addr Direcci�n de memoria del inicio de la regi�n a liberar
 * @param length Tama�o de la regi�n a liberar
  @verbatim
  Verifica si la direccion que recibe es menor a la direccion del
  kernel o no, de ser asi sale sin realizar ninguna accion, caso
  contrario se convierte a una direccion lineal para posteriormente
  colocarla en el mapa de bits como una region disponible.
 @endverbatim*/
void free_region(char * start_addr, unsigned int length) {
	 unsigned int start;
	 unsigned int end;

	 start = round_down_to_memory_unit((unsigned int)start_addr);

	 if (start < allowed_free_start) {return;}

	 end = start + length;

	 for (; start < end; start += MEMORY_UNIT_SIZE) {
		 free_unit((char*)start);
	 }

	 /* Almacenar el inicio de la regi�n liberada para una pr�xima asignaci�n */
	 next_free_unit = (unsigned int)start_addr / MEMORY_UNIT_SIZE;
 }
