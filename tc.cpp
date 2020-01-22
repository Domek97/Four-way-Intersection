#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <queue>
#include <semaphore.h>

using namespace std;

//global variables
clock_t startTime, currentTime;
double processTime;
int numThreads = 0, prevCar = -1;
queue <int> headOfLine, southCars, northCars, eastCars, westCars, lockedSemaphores;
pthread_mutex_t mutexes[24];
sem_t semaphore, headOfLineSem;

typedef struct directions {
    char dir_original;
    char dir_target;
}directions;

directions carDirections[8];
/*Returns time in seconds*/
double GetTime() {
    struct timeval t;
    int rc = gettimeofday(&t, NULL);
    assert(rc == 0);
    return (double)t.tv_sec + (double)t.tv_usec / 1e6;
}
/*Spins for a given number of seconds.*/
void Spin(int howLong) {
    double t = GetTime();
    while ((GetTime()- t) < (double)howLong){}; // do nothing in loop
}
void acquireLocks(int lock1, int lock2, int lock3, int lock4, int lock5, int lock6, int lockedMutexes[]) {//left turns and going straight require 6 locks
    int locksAcquired = 0;
    int successfulLocks[6];
    sem_wait(&semaphore); //this semaphore ensures the locks are not messed with while the thread is checking them
    if(pthread_mutex_trylock(&mutexes[lock1]) == 0) { successfulLocks[locksAcquired] = lock1; locksAcquired++;} //testing if each lock is open
    if(pthread_mutex_trylock(&mutexes[lock2]) == 0) { successfulLocks[locksAcquired] = lock2; locksAcquired++;}
    if(pthread_mutex_trylock(&mutexes[lock3]) == 0) { successfulLocks[locksAcquired] = lock3; locksAcquired++;}
    if(pthread_mutex_trylock(&mutexes[lock4]) == 0) { successfulLocks[locksAcquired] = lock4; locksAcquired++;}
    if(pthread_mutex_trylock(&mutexes[lock5]) == 0) { successfulLocks[locksAcquired] = lock5; locksAcquired++;}
    if(pthread_mutex_trylock(&mutexes[lock6]) == 0) { successfulLocks[locksAcquired] = lock6; locksAcquired++;}
    if(locksAcquired != 6) { //not all of the locks are available so we need to unlock the locks that resulted from trying
        for(int i = 0; i < locksAcquired; i++) {          
            pthread_mutex_unlock(&mutexes[successfulLocks[i]]);
            lockedMutexes[i] = -1;
        }
    }   
    else {//all locks are available and will be locked upon exit of the function
        for(int i = 0; i < 6; i++) {
            lockedMutexes[i] = successfulLocks[i];
        }
    }
    sem_post(&semaphore);
}
void acquireLocks(int lock1, int lock2, int lockedMutexes[]) {//right turns only require 2 locks
    int locksAcquired = 0;
    int successfulLocks[2];
    sem_wait(&semaphore);
    if(pthread_mutex_trylock(&mutexes[lock1]) == 0) { successfulLocks[locksAcquired] = lock1; locksAcquired++;} //testing if each lock is open
    if(pthread_mutex_trylock(&mutexes[lock2]) == 0) { successfulLocks[locksAcquired] = lock2; locksAcquired++;}

    if(locksAcquired != 2) { //not all of the locks are available so we need to unlock the locks that resulted from trying
        for(int i = 0; i < 2; i++) {
            pthread_mutex_unlock(&mutexes[successfulLocks[i]]);
            lockedMutexes[i] = -1;
        }
    }
    else {//all locks are available
        for(int i = 0; i < locksAcquired; i++) {
            lockedMutexes[i] = successfulLocks[i];
        }
    }
    sem_post(&semaphore);

}
void ArriveIntersection(directions *dir, int cid) {
    int temp;
    switch (carDirections[cid-1].dir_original) {//this switch finds where the car is coming from and puts it in that lane
        case '^' :
            if(southCars.empty() || southCars.front() == -1) { //-1 is gotten in front when we have a car in headOfLine; it lets the car know it is not the front

                if (southCars.front() == -1) {//a car is at the head of the line
                    southCars.pop();
                    southCars.push(cid);
                }
                else {// the car is the first one in the lane and stop-sign
                    headOfLine.push(cid);
                    southCars.push(-1);
                }
            }
            else {//there is already a car in the lane behind the head of the line
                southCars.push(cid);
            }
            break;
        case 'V' :
            if(northCars.empty() || northCars.front() == -1) {//repetition of above
                if (northCars.front() == -1) {
                    northCars.pop();
                    northCars.push(cid);
                }
                else {
                    headOfLine.push(cid);
                    northCars.push(-1);
                }
            }
            else {
                northCars.push(cid);
            }
            break;
        case '>' :
            if(westCars.empty() || westCars.front() == -1) {//repetition of above

                if (westCars.front() == -1) {
                    westCars.pop();
                    westCars.push(cid);
                }
                else {
                    headOfLine.push(cid);
                    westCars.push(-1);
                }
            }
            else {
                westCars.push(cid);
            }
            break;
        case '<' :
            if(eastCars.empty() || eastCars.front() == -1) {//repetition of above

                if (eastCars.front() == -1) {
                    eastCars.pop();
                    eastCars.push(cid);
                }
                else {
                    headOfLine.push(cid);
                    eastCars.push(-1);
                }
            }
            else {
                eastCars.push(cid);
            }
            break;
        default:
            printf("Could not find direction of car %d.", cid);
    }
    currentTime = GetTime(); //we use this every time we want to know how long has passed since we started the first thread
    processTime = ((double) (currentTime-startTime)) / CLOCKS_PER_SEC;
    processTime = processTime * 10e5;
    printf("Time %0.1f: Car %d (%c %c) arriving \n", processTime, cid, carDirections[cid-1].dir_original, carDirections[cid-1].dir_target);
    Spin(2);
}
void CrossIntersection(directions *dir, int cid) {
    int lockedMutexes[6]; //this is used to determine whether all locks are available for the car to cross, value is returned from acquireLocks()
    int nextCar;// the car that becomes headOfLine after headOfLine pops
    while(headOfLine.front() != (cid)){//waiting to be the car earliest to the stop sign
        usleep(10);
    }
    //left turn
    if((carDirections[cid-1].dir_original == '^' && carDirections[cid-1].dir_target == '<') || (carDirections[cid-1].dir_original == '>' && carDirections[cid-1].dir_target == '^') || (carDirections[cid-1].dir_original == 'V' && carDirections[cid-1].dir_target == '>') || (carDirections[cid-1].dir_original == '<' && carDirections[cid-1].dir_target == 'v')) {
        switch (carDirections[cid-1].dir_original) {//this is what handles syncrhonization. if the car can acquire all locks then it can continue. if not, it keeps trying until it can.
            case '^' :
                acquireLocks(2, 8, 11, 13, 19, 23, lockedMutexes);
                while (lockedMutexes[0] == -1) { usleep(10); acquireLocks(2, 8, 11, 13, 19, 23, lockedMutexes); } //keeps trying until it gets all locks
                lockedMutexes[0] = 2; lockedMutexes[1] = 8; lockedMutexes[2] = 11; lockedMutexes[3] = 13; lockedMutexes[4] = 19; lockedMutexes[5] = 23;
                break;
            case 'V' :
                acquireLocks(0, 4, 9, 12, 15, 21, lockedMutexes);
                while (lockedMutexes[0] == -1) { usleep(10); acquireLocks(0, 4, 9, 12, 15, 21, lockedMutexes); }
                lockedMutexes[0] = 0; lockedMutexes[1] = 4; lockedMutexes[2] = 9; lockedMutexes[3] = 12; lockedMutexes[4] = 15; lockedMutexes[5] = 21;
                break;
            case '>' :
                acquireLocks(1, 5, 9, 11, 14, 16, lockedMutexes);
                while (lockedMutexes[0] == -1) { usleep(10); acquireLocks(1, 5, 9, 11, 14, 16, lockedMutexes); }
                lockedMutexes[0] = 1; lockedMutexes[1] = 5; lockedMutexes[2] = 9; lockedMutexes[3] = 11; lockedMutexes[4] = 14; lockedMutexes[5] = 16;
                break;
            case '<' :
                acquireLocks(7, 10, 12, 13, 18, 22, lockedMutexes);
                while (lockedMutexes[0] == -1) { usleep(10); acquireLocks(7, 10, 12, 13, 18, 22, lockedMutexes); }
                lockedMutexes[0] = 7; lockedMutexes[1] = 10; lockedMutexes[2] = 12; lockedMutexes[3] = 13; lockedMutexes[4] = 18; lockedMutexes[5] = 22;
                break;
            default:
                printf("Could not find direction of car %d.", cid);
            }
        prevCar = cid;
        currentTime = GetTime();
        processTime = ((double) (currentTime-startTime)) / CLOCKS_PER_SEC;
        processTime = processTime * 10e5;
        printf("Time %0.1f: Car %d (%c %c)         crossing \n", processTime, cid, carDirections[cid-1].dir_original, carDirections[cid-1].dir_target);
        switch (carDirections[cid-1].dir_original) {//this switch allows the next car in that lane to take the front
            case '^' :
                headOfLine.pop(); //remove the car that is now crossing from headOfLine
                nextCar = southCars.front(); //prep the next car using a temporary variable
                southCars.pop(); //take the car out of its line
                headOfLine.push(nextCar); //and into the head of line
                
                break;
            case 'V' :
                headOfLine.pop();
                nextCar = northCars.front();
                northCars.pop();
                headOfLine.push(nextCar);
                break;
            case '>' :
                headOfLine.pop();
                nextCar = westCars.front();
                westCars.pop();
                headOfLine.push(nextCar);

                break;
            case '<' :
                headOfLine.pop();
                nextCar = eastCars.front();
                eastCars.pop();
                headOfLine.push(nextCar);
                break;
            default:
                printf("Could not find direction of car %d.", cid);
        }   
        Spin(5);
            for(int i = 0; i < 6; i++) {
                pthread_mutex_unlock(&mutexes[lockedMutexes[i]]);
            }
 

    }
    //right turn
    else if ((carDirections[cid-1].dir_original == '^' && carDirections[cid-1].dir_target == '>') || (carDirections[cid-1].dir_original == '>' && carDirections[cid-1].dir_target == 'V') || (carDirections[cid-1].dir_original == 'V' && carDirections[cid-1].dir_target == '>') || (carDirections[cid-1].dir_original == '<' && carDirections[cid-1].dir_target == '^')) {
        switch (carDirections[cid-1].dir_original) {
            case '^' :
                acquireLocks(21, 23, lockedMutexes);
                while (lockedMutexes[0] == -1) { usleep(10); acquireLocks(21, 23, lockedMutexes); }
                lockedMutexes[0] = 21; lockedMutexes[1] = 23;
                break;
            case 'V' :
                acquireLocks(0, 2, lockedMutexes);
                while (lockedMutexes[0] == -1) { usleep(10); acquireLocks(0, 2, lockedMutexes); }
                lockedMutexes[0] = 0; lockedMutexes[1] = 2;
                break;
            case '>' :
                acquireLocks(16, 22, lockedMutexes);
                while (lockedMutexes[0] == -1) { usleep(10); acquireLocks(16, 22, lockedMutexes); }
                lockedMutexes[0] = 16; lockedMutexes[1] = 22;
                break;
            case '<' :
                acquireLocks(1, 7, lockedMutexes);
                while (lockedMutexes[0] == -1) { usleep(10); acquireLocks(1, 7, lockedMutexes); }
                lockedMutexes[0] = 1; lockedMutexes[1] = 7;
                break;
            default:
                printf("Could not find direction of car %d.", cid);
        }
        prevCar = cid;
        currentTime = GetTime();
        processTime = ((double) (currentTime-startTime)) / CLOCKS_PER_SEC;
        processTime = processTime * 10e5 ;
        printf("Time %0.1f: Car %d (%c %c)         crossing \n", processTime, cid, carDirections[cid-1].dir_original, carDirections[cid-1].dir_target);
        switch (carDirections[cid-1].dir_original) {//it's going now, so remove it from the front of its lane and stop-sign queue
            case '^' :
                headOfLine.pop();
                nextCar = southCars.front();
                southCars.pop();
                headOfLine.push(nextCar);

                break;
            case 'V' :
                headOfLine.pop();
                nextCar = northCars.front();
                northCars.pop();
                headOfLine.push(nextCar);
                break;
            case '>' :
                headOfLine.pop();
                nextCar = westCars.front();
                westCars.pop();
                headOfLine.push(nextCar);
                break;
            case '<' :
                headOfLine.pop();
                nextCar = eastCars.front();
                eastCars.pop();
                headOfLine.push(nextCar);
                break;
            default:
                printf("Could not find direction of car %d.", cid);
        }
        Spin(3);
        
        for(int i = 0; i < 2; i++) {
            pthread_mutex_unlock(&mutexes[lockedMutexes[i]]);
        }
    }
    //straight
    else {
        
        switch (carDirections[cid-1].dir_original) {
            case '^' :
                acquireLocks(1, 6, 10, 15, 20, 23, lockedMutexes);
                while (lockedMutexes[0] == -1) { usleep(10); acquireLocks(1, 6, 10, 15, 20, 23, lockedMutexes); }
                lockedMutexes[0] = 1; lockedMutexes[1] = 6; lockedMutexes[2] = 10; lockedMutexes[3] = 15; lockedMutexes[4] = 20; lockedMutexes[5] = 23;
                break;
            case 'V' :
                acquireLocks(0, 3, 8, 14, 17, 22, lockedMutexes);
                while (lockedMutexes[0] == -1) { usleep(10); acquireLocks(0, 3, 8, 14, 17, 22, lockedMutexes); }
                lockedMutexes[0] = 0; lockedMutexes[1] = 3; lockedMutexes[2] = 8; lockedMutexes[3] = 14; lockedMutexes[4] = 17; lockedMutexes[5] = 22;
                break;
            case '>' :
                acquireLocks(16, 17, 18, 19, 20, 21, lockedMutexes);
                while (lockedMutexes[0] == -1) { usleep(10); acquireLocks(16, 17, 18, 19, 20, 21, lockedMutexes); }
                lockedMutexes[0] = 16; lockedMutexes[1] = 17; lockedMutexes[2] = 18; lockedMutexes[3] = 19; lockedMutexes[4] = 20; lockedMutexes[5] = 21;
                break;
            case '<' :
                acquireLocks(2, 3, 4, 5, 6, 7, lockedMutexes);
                while (lockedMutexes[0] == -1) { usleep(10); acquireLocks(2, 3, 4, 5, 6, 7, lockedMutexes); }
                lockedMutexes[0] = 2; lockedMutexes[1] = 3; lockedMutexes[2] = 4; lockedMutexes[3] = 5; lockedMutexes[4] = 6; lockedMutexes[5] = 7;
                break;
            default:
                printf("Could not find direction of car %d.", cid);
        }
        prevCar = cid;
        currentTime = GetTime();
        processTime = ((double) (currentTime-startTime)) / CLOCKS_PER_SEC;
        processTime = processTime * 10e5;
        printf("Time %0.1f: Car %d (%c %c)         crossing \n", processTime, cid, carDirections[cid-1].dir_original, carDirections[cid-1].dir_target);
        switch (carDirections[cid-1].dir_original) {//car finally gets so cross so we take it off the headofline and front of its lane queues
            case '^' :
                headOfLine.pop();
                nextCar = southCars.front();
                southCars.pop();
                if(nextCar != NULL)
                    southCars.push(-1);
                headOfLine.push(nextCar);
                break;
            case 'V' :
                headOfLine.pop();
                nextCar = northCars.front();
                northCars.pop();
                headOfLine.push(nextCar);
                break;
            case '>' :
                headOfLine.pop();
                nextCar = westCars.front();
                westCars.pop();
                headOfLine.push(nextCar);
                break;
            case '<' :
                headOfLine.pop();
                nextCar = eastCars.front();
                eastCars.pop();
                headOfLine.push(nextCar);
                break;
            default:
                printf("Could not find direction of car %d.", cid);
        }
        Spin(4);
        for(int i = 0; i < 6; i++) {
            pthread_mutex_unlock(&mutexes[lockedMutexes[i]]);
        }
    }
}
void ExitIntersection(directions *dir, int cid) {
    printf("Time %.1f: Car %d (%c %c)                  exiting \n", processTime, cid, carDirections[cid-1].dir_original, carDirections[cid-1].dir_target);

}
void Car(directions *dir) {
    int cid = numThreads + 1;
    numThreads++;
    usleep(1.1e6 * cid);
    ArriveIntersection(dir, cid);
    CrossIntersection(dir, cid);
    ExitIntersection(dir, cid);
}




int main() { 
    pthread_t *cars[8];
    //initialize values of array
    carDirections[0].dir_original = '^'; carDirections[0].dir_target = '^';
    carDirections[1].dir_original = '^'; carDirections[1].dir_target = '^';
    carDirections[2].dir_original = '^'; carDirections[2].dir_target = '<';
    carDirections[3].dir_original = 'V'; carDirections[3].dir_target = 'V';
    carDirections[4].dir_original = 'V'; carDirections[4].dir_target = '>';
    carDirections[5].dir_original = '^'; carDirections[5].dir_target = '^';
    carDirections[6].dir_original = '>'; carDirections[6].dir_target = '^';
    carDirections[7].dir_original = '<'; carDirections[7].dir_target = '^';

    //initialize mutexes and semaphores
    for( int i = 0; i < 24; i++) {
        pthread_mutex_init(&mutexes[i], NULL);
    }
    sem_init(&semaphore, 0, 1);
    sem_init(&headOfLineSem, 0, 1);
    
    //create and start threads
    for(int i = 0; i < 8; i++) {
      cars[i] = (pthread_t * )malloc(sizeof(*cars[i]));
      pthread_create(cars[i], NULL, (void*)Car, &carDirections[i]);
    }
    startTime = GetTime(); //start keeping track of time
    for(int i = 0; i < 8; i++) {
        pthread_join(*cars[i], NULL);
    }
    return 0;
}
