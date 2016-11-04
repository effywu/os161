#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>
#include <array.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
 typedef struct vehicle{
	 Direction origin;
	 Direction destination;
 } vehicle;
bool turning_right(vehicle * v);
static struct cv* intersectionFree;
static struct lock* intersectionLock;
struct array* vehicles;


/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */
  
  intersectionFree=cv_create("intersectionFree");
  intersectionLock=lock_create("intersectionLock");
  vehicles=array_create();
  array_init(vehicles);
  if (intersectionFree==NULL||intersectionLock==NULL||vehicles==NULL) {
    panic("could not init intersection");
  }
  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
  KASSERT(intersectionFree != NULL);
  KASSERT(intersectionLock != NULL);
  KASSERT(vehicles != NULL);
  cv_destroy(intersectionFree);
  lock_destroy(intersectionLock);
  
  for (unsigned int i=0;i<array_num(vehicles);i++){
	  vehicle * currentVehicle=array_get(vehicles,i);
	  kfree(currentVehicle);
  }
  array_destroy(vehicles);
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

bool
turning_right(vehicle * car){
	switch (car->origin){
		case north:
			if (car->destination==west) return true;
			break;
		case east:
			if (car->destination==north) return true;
			break;
		case south:
			if (car->destination==east) return true;
			break;
		case west:
			if (car->destination==south) return true;
			break;
	}
	return false;
}
void
intersection_before_entry(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  KASSERT(intersectionFree != NULL);
  KASSERT(intersectionLock != NULL);
  KASSERT(vehicles != NULL);
  
  lock_acquire(intersectionLock);
  vehicle* incoming=kmalloc(sizeof(struct vehicle));
  KASSERT(incoming!=NULL);
  incoming->origin=origin;
  incoming->destination=destination;
  bool hasEntered=false;
  while(!hasEntered){
	  bool canEnter=true;
	  for (unsigned int i=0;i<array_num(vehicles);i++){
		  vehicle * currentVehicle=array_get(vehicles,i);
		  if (origin==currentVehicle->origin){
			  continue;
		  } else if (origin==currentVehicle->destination&&destination==currentVehicle->origin){
			  continue;
		  } else if (destination!=currentVehicle->destination){
			  if (turning_right(incoming)||turning_right(currentVehicle)){
				 continue;
			  } else {
				  canEnter=false;
				  cv_wait(intersectionFree,intersectionLock);
				  break;
			  }
		  } else {
			  canEnter=false;
			  cv_wait(intersectionFree,intersectionLock);
			  break;
		  }
	  }
	  if (canEnter){
		  array_add(vehicles,incoming,NULL);
		  hasEntered=true;
	  }
  }
  lock_release(intersectionLock);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  KASSERT(intersectionFree != NULL);
  KASSERT(intersectionLock != NULL);
  KASSERT(vehicles != NULL);
  
  lock_acquire(intersectionLock);
  for (unsigned int i=0;i<array_num(vehicles);i++){
	  vehicle * currentVehicle=array_get(vehicles,i);
	  if (currentVehicle->origin==origin&&currentVehicle->destination==destination){
		  array_remove(vehicles,i);
		  kfree(currentVehicle);
		  cv_broadcast(intersectionFree,intersectionLock);
		  break;
	  }
  }
  lock_release(intersectionLock);
}
