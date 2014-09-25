/****
 *	listas.h - Primitivas para las listas enlazadas. 
 *
 *		2004, Christian Hentschel.
 *
 ***********************************************************************************/

#ifndef __LISTAS_H__
#define __LISTAS_H__

struct list_head {
	struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

#define INIT_LIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)


#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

/******************************************************************
 * container_of: Castea el miembro de una estruc. a la estruct 
 *		que lo contiene. 
 * Recibe:	ptr al miembro, tipo de Struct, nombre del miembro.
 ******************************************************************/
#define container_of(ptr, type, member) ({	\
        const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
        (type *)( (char *)__mptr - offsetof(type,member) );})


/***********************************************************************
 * iterate:	Recorre una lista enlazada
 * Recibe:	Ptr aux para recorrer la lista, Ptr al head de la lista.
 ***********************************************************************/
#define iterate(pos, lstHead) \
	for (pos = (lstHead)->next, __builtin_prefetch(pos->next); pos != (lstHead); \
        	pos = pos->next, __builtin_prefetch(pos->next))

/***********************************************************************
 * iterate_safe: Recorre una lista enlazada, protegido contra el borrado 
 *				de un nodo.
 * Recibe:		Ptr aux para recorrer la lista, Ptr al head de la lista.
 ***********************************************************************/
#define iterate_safe(pos, n, lstHead) \
	for (pos = (lstHead)->next, n = pos->next; pos != (lstHead); \
		pos = n, n = pos->next, __builtin_prefetch(pos->next))
					
/***********************************************************************
 * next_list_entry:	Obtiene el proximo nodo de la lista. 
 * Recibe:	Ptr a la posicion actual, Ptr al head de la lista.
 ***********************************************************************/
#define next_list_entry(pos, lstHead) \
({	( pos->next == lstHead ) ? pos->next->next : pos->next;	})


/*******************************************************************
 *	FIND_LIST_S():	Busca en una lista por comparacion de strings
 *	Recibe:	ptr a la estructura que retorna, miembro a comparar, 
 *			string a comparar, ptr a inicio de la lista, 
 *			nombre de la lista.
 *******************************************************************/
#define FIND_LIST_S(retStruct, structMbr, srchStr, lstHead, lstType)	\
({	\
	__label__ found;	\
	struct list_head *listPtr;	\
	iterate(listPtr, lstHead){	\
		retStruct = container_of(listPtr, typeof(*retStruct), lstType);	\
		if (!strcmp(retStruct->structMbr, srchStr))	\
			goto found;	\
	}	\
	retStruct = NULL;	\
	found: retStruct;	\
})


/*******************************************************************
 *	FIND_LIST_N():	Busca en una lista por comparacion de numeros
 *	Recibe:	ptr a la estructura que retorna, miembro a comparar, 
 *			numero a comparar, ptr a inicio de la lista, 
 *			nombre de la lista.
 *******************************************************************/
#define FIND_LIST_N(retStruct, structMbr, srchNum, lstHead, lstType)	\
({	\
	__label__ found;	\
	struct list_head *listPtr;	\
	iterate(listPtr, lstHead){	\
		retStruct = container_of(listPtr, typeof(*retStruct), lstType);	\
		if ( retStruct->structMbr == srchNum )	\
			goto found;	\
	}	\
	retStruct = NULL;	\
	found: retStruct;	\
})

/* FIXME:.. add these to FIND_LIST fncts.
 *
 
static __inline__ int cmp_by_str(char *str1, char *str2)
{
	return !strcmp(str1, str2);
}

static __inline__ int cmp_by_num(int num1, int num2)
{
	return num1 == num2;
}

*
*/

/*****************************************************************
 * list_del:	Borra una entrada de la lista.
 * Recibe:		Ptr al nodo a eliminar..
 *****************************************************************/
static __inline__ void list_del(struct list_head *nodePtr)
{
	(nodePtr->next)->prev = nodePtr->prev;
	(nodePtr->prev)->next = nodePtr->next;

	nodePtr->next = (void *)0;
	nodePtr->prev = (void *)0;
}


/*****************************************************************
 * list_empty:	Verifica si la lista esta vacia.
 * Recibe:		Puntero al inicio de la lista.
 *****************************************************************/
static __inline__ int list_empty(struct list_head *listPtr)
{
	return (listPtr->next == listPtr);
}


/*******************************************************************
 * 	__list_add:	Inserta un nuevo nodo entre dos nodos existentes. 
 *	Recibe:		Ptrs a el nuevo nodo, y los nodos prev y next. 
 ******************************************************************/
static __inline__ void __list_add(struct list_head *newNodePtr,
			struct list_head *prev,
			struct list_head *next)
{
	next->prev = newNodePtr;
	newNodePtr->next = next;
	newNodePtr->prev = prev;
	prev->next = newNodePtr;
}

/*******************************************************************
 *	list_add_tail:	Inserta un nuevo nodo en la lista al final de esta. 
 * 	Recibe:			Nuevo nodo a insertar, Ptr al inicio de la lista
 *******************************************************************/
static __inline__ void list_add_tail(struct list_head *newNodePtr, struct list_head *listPtr)
{
	__list_add(newNodePtr, listPtr->prev, listPtr);
}

/***************************************************************************
 *	list_add:	inserta un nuevo nodo a la lista al principio de esta. 
 *	Recibe:		Nuevo nodo a insertar, Ptr al inicio de la lista.  
 **************************************************************************/
static __inline__ void list_add(struct list_head *newNodePtr, struct list_head *listPtr)
{
	__list_add(newNodePtr, listPtr, listPtr->next);
}

/***************************************************************************
 *	move_to_tail:	borra un nodo de la lista, y lo agrega al final de otra. 
 *	Recibe:		Nodo a borrar, lista nueva en donde insertar.  
 **************************************************************************/
static __inline__ void move_to_tail(struct list_head *nodePtr, struct list_head *newlist)
{
        list_del(nodePtr);
        list_add_tail(nodePtr, newlist);
}


#endif /* __LISTAS_H__ */
