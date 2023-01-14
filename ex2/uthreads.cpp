//
// Created by alonmarko208 on 29/03/2022.
//
#include "uthreads.h"
#include <queue>
#include <unordered_map>
#include <list>
#include <iostream>
#include <array>
#include <stdbool.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <setjmp.h>

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr) {
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}


#endif
struct sigaction sa;

#define FAILURE (-1)
#define SUCCESS 0
#define READY 0
#define BLOCKED 1
#define RUNNING 2
#define FIRST 0
#define CONVERSIONFACTOR 1000000
int quantum_total = 0;

// This is a class for a single thread which hosts its allocated stack , stack pointer(sp) , thread ID and runtime and
// state.
class Thread {
private:
    int tid;
    int curState;
    int quantumTime;
    int quantumWake;
    bool isSleep;
    address_t sp;
    char *stack;
public:
    sigjmp_buf env;
    /**
     * the constructor for a single thread object
     * @param stackAlloc - the stack that was allocated for the thread.
     * @param sp  - the stack pointer
     * @param id - the id of the thread.
     */
    Thread(int id, char* stackAlloc = nullptr) {
        this->stack = stackAlloc;
        this->tid = id;
//        this->sp = sp;
        this->curState = READY;
        this->quantumTime = 0;
        this->quantumWake = 0;
        this->isSleep = false;
    }
    ~Thread(){
        if (this->stack != nullptr){
            delete(this->stack);
            this->stack = nullptr;
        }
    }


    /**
     * returns the id of the current thread
     * @return int
     */
    int getTid() const {
        return this->tid;
    }

    int getState() const {
        return this->curState;
    }

    void setState(int state) {
        if (state == RUNNING) {
            increment();
        }
        this->curState = state;
    }

    int getQuantums() const {
        return this->quantumTime;
    }

    int getSleepingQuantums() const {
        return this->quantumWake;
    }

    void increment() {
        this->quantumTime++;
    }

    void decSleep() {
        this->quantumWake--;
    }

    void changeSleepState(bool value) {
        this->isSleep = value;
    }

    void setSleepQuantums(int quantums) {
        this->quantumWake = quantums;
    }

    bool getSleep() const {
        return isSleep;
    }
};

// this class is for thread management, holds the appropriate data structures and manages them, keeps track of time and
// etc. we are going to use this class in the uthread functions inorder to manage and switch the threads.
class Handler {
private:
    sigset_t set;
    struct itimerval timer;
    std::vector<Thread *> *readyQueue;
    std::unordered_map<int, Thread *> *threadsMap;
    std::vector<Thread *>::iterator it;
    std::array<Thread *, MAX_THREAD_NUM> *blockedArray;
    std::array<Thread *, MAX_THREAD_NUM> *sleepingArray;
    Thread *currentRunner;

public:
    bool blockFlag;

    /**
     * the constructor for the thread CEO class.
     * @param quantum_usecs
     */
    Handler(int quantum_usecs) {
        this->readyQueue = new std::vector<Thread *>;
        this->threadsMap = new std::unordered_map<int, Thread *>; //thread hash map
        this->blockedArray = new std::array<Thread *, MAX_THREAD_NUM>; //blocked list
        //this->sleepingQueue = new std::vector<Thread *>; // sleeping threads list - unsure yet!
        this->sleepingArray = new std::array<Thread *, MAX_THREAD_NUM>;
        //blockedArray = {}; // null init needed?
        //sleepingQueue = {};
        timer.it_value.tv_sec = quantum_usecs / CONVERSIONFACTOR;        // first time interval, seconds part
        timer.it_value.tv_usec = quantum_usecs % CONVERSIONFACTOR;        // first time interval, microseconds part
        // configure the timer to expire every quantum_seconds sec after that.
        timer.it_interval.tv_sec = quantum_usecs / CONVERSIONFACTOR;    // following time intervals, seconds part
        timer.it_interval.tv_usec = quantum_usecs % CONVERSIONFACTOR;// following time intervals, microseconds part
        sigaddset(&set, SIGVTALRM);
        blockFlag = false;
    }
    ~Handler(){
        delete(readyQueue);
        delete(threadsMap);
        delete(blockedArray);
        delete(sleepingArray);
    }

    /**
     * this function returns the lowest available tid for a new thread - if none is available than returns -1
     * @return
     */
    int minimalTid() {
        for (int i = 0; i < MAX_THREAD_NUM; i++) {
            if (threadsMap->count(i) == 0) {
                return i;
            }
        }
        return FAILURE;
    }

    /**
     * returns the current signals set.
     * @return sigset_t
     */
    sigset_t *getSet() {
        return &this->set;
    }


    Thread *runner() {
        return this->currentRunner;
    }

    /**
     * adds the thread to the threadsmap
     * @param pThread the newly born thread :)
     */
    void addThread(int id, Thread *pThread) {
        threadsMap->insert({id, pThread});
    }

    void setRunningThread(Thread *pThread) {
        quantum_total++;
        pThread->setState(RUNNING);
        currentRunner = pThread;
    }

    void addToReady(Thread *pThread) {
        readyQueue->push_back(pThread);
        pThread->setState(READY);
    }

    Thread *getThreadById(int id) {
        try {
            return this->threadsMap->at(id);
        }
        catch (const std::out_of_range &oor) {
            return nullptr;
        }
    }

    void removeFromReady(Thread *pThread) {
        int index = 0;
        for (int i = 0; i < readyQueue->size(); i++) {
            if (readyQueue->at(i) == pThread) {
                index = i;
                break;
            }
        }
        it = readyQueue->begin();
        for (int i = 0; i < index; i++) {
            it++;
        }
        readyQueue->erase(it);
    }

    Thread *popNextReady() {
        if (readyQueue->empty()) {
            return getThreadById(FIRST); // if it is empty then we run the main thread?
        }
        Thread *thread = readyQueue->front();
        pop_front(readyQueue);
        return thread;
    }

    static void pop_front(std::vector<Thread *> *v) {
        if (!v->empty()) {
            v->erase(v->begin());
        }
    }

    void switchThreadToRunning() {
        //std::cout << "step switch 0" << std::endl;
        Thread *newRunning = this->popNextReady();
        //std::cout << "step switch 1" << std::endl;
        this->setRunningThread(newRunning);
        //std::cout << "step switch 2" << std::endl;
        siglongjmp(currentRunner->env, 1);
    }

    void eraseThreadFromMap(int tid) {
        Thread *thread = getThreadById(tid);
        threadsMap->erase(tid);
        delete(thread);

    }

    void eraseBlockedThread(int tid) {
        blockedArray->at(tid) = nullptr;
    }

    void insertBlockedThread(int tid, Thread *thread) {
        blockedArray->at(tid) = thread;
    }

    void insertSleptThread(int tid, Thread *thread) {
        sleepingArray->at(tid) = thread;
    }

    void removeFromSleepArray(int tid) {
        sleepingArray->at(tid) = nullptr;
    }

    itimerval *getTime() {
        return &this->timer;
    }


    Thread *getSleeping(int i) {
        return this->sleepingArray->at(i);
    }
};

Handler *handler; // the guy that runs everything.

/**
 * masks the signals with block/unblock
 */
void sigMask(int signal) {
    if (sigprocmask(signal, handler->getSet(), NULL) < 0) {
        std::cerr << "system error: sigprocmask returned -1." << std::endl;
        exit(FAILURE);
    }
}

void roundRobin(int signum) {
    if (signum == SIGVTALRM) {
        sigMask(SIG_BLOCK);
        for (int i = 1; i < MAX_THREAD_NUM; i++) { // the loop to check sleeping threads
            if (handler->getSleeping(i) != nullptr) {
                Thread *current = handler->getSleeping(i);
                if (current->getSleepingQuantums() == 0) {
                    handler->removeFromSleepArray(i);
                    current->changeSleepState(false);
                    if (current->getState() == READY) {
                        handler->addToReady(current);
                    } else {
                        handler->insertBlockedThread(i, current);
                    }
                } else {
                    current->decSleep();
                }
            }
        }
        if (sigsetjmp(handler->runner()->env, 1) == FIRST) { // means its the first sigsetjump and not going back from siglongjump?
//            std::cout << "hi" << std::endl;
            if (handler->blockFlag) {
                handler->insertBlockedThread(handler->runner()->getTid(), handler->runner());
                handler->blockFlag = false;
            } else {
                handler->runner()->setState(READY);
                handler->addToReady(handler->runner());
            }
        }
        else {
            return;
        }
        handler->switchThreadToRunning();
        // Start a virtual timer. It counts down whenever this process is executing.
        if (setitimer(ITIMER_VIRTUAL, handler->getTime(), NULL) < 0) {
            std::cerr << "system error: setitimer returned -1." << std::endl;
            exit(FAILURE);
        }
        sigMask(SIG_UNBLOCK);
    }
}

int uthread_init(int quantum_usecs) {
    if (quantum_usecs <= 0) {
        std::cerr << "thread library error: quantum_usecs must be positive." << std::endl;
        return FAILURE;
    }
    sigMask(SIG_BLOCK);
    handler = new Handler(quantum_usecs);
    sa.sa_handler = &roundRobin; // sets the round robin function as the sa_handler for SIGVTALRM (this the only signal?).
    if (sigaction(SIGVTALRM, &sa, NULL) < 0) {
        std::cerr << "system error: sigaction returned -1" << std::endl;
        exit(FAILURE);
    }
    uthread_spawn(0); // program counter is set to 0?
    // Start a virtual timer. It counts down whenever this process is executing.
    if (setitimer(ITIMER_VIRTUAL, handler->getTime(), NULL)) {
        std::cerr << "system error: setitimer error." << std::endl;
        exit(FAILURE);
    }
    sigMask(SIG_UNBLOCK);
    return SUCCESS;
}

int uthread_terminate(int tid) {
//    std::cout << "uthread_terminate - tid: " << tid << std::endl;
//    std::cout << "step 1 " << std::endl;
    //termination of entire thread
    sigMask(SIG_BLOCK);
//    std::cout << "step 2" << std::endl;
    if (tid == FIRST) {
        for(int i=0;i<MAX_THREAD_NUM;i++){
            if (handler->getThreadById(i)!= nullptr){
                handler->eraseThreadFromMap(tid);
            }
        }
        //TODO - delete everything - for loop?
        delete (handler);
        exit(SUCCESS);
    }
//    std::cout << "step 3" << std::endl;
    Thread *thread = handler->getThreadById(tid);
    if (thread == nullptr) {
        std::cerr << "thread library error: No Such Thread Exists" << std::endl;
        return FAILURE;
    }
//    std::cout << "step 4" << std::endl;
    int state = thread->getState();
    if (state == READY) {
        if (thread->getSleep()) {
           handler->removeFromSleepArray(tid);
        }
        else {
            handler->removeFromReady(thread); //todo: check that function!
        }
    }
//    std::cout << "step 5" << std::endl;
    if (state == RUNNING) {
//        std::cout << "step 5.1" << std::endl;
        handler->switchThreadToRunning();
//        std::cout << "step 5.2" << std::endl;
    }
//    std::cout << "step 6" << std::endl;
    if (state == BLOCKED) {
        handler->eraseBlockedThread(tid);
    }
//    std::cout << "step 7" << std::endl;
    handler->eraseThreadFromMap(tid);

//    std::cout << "step 8" << std::endl;
    sigMask(SIG_UNBLOCK);
//    std::cout << "step 9" << std::endl;
    return SUCCESS;
}


int uthread_spawn(
        thread_entry_point entry_point) { //TODO - main thread should not be appointed a stack and pc/sp... how do we create it with no such arguments..
    sigMask(SIG_BLOCK);
    int id = handler->minimalTid();
    if (id == FAILURE) {
        std::cerr << "thread library error: no free id's for new threads!" << std::endl;
        return FAILURE;
    }
    char *stack;
    if (id == FIRST) {
        stack = nullptr;
    }
    else {
        stack = new char[STACK_SIZE];
    }
    auto *thread = new Thread(id, stack);
    if (id != FIRST) {
        address_t sp = (address_t) stack + STACK_SIZE - sizeof(address_t);
        address_t pc = (address_t) entry_point;
        sigsetjmp(thread->env, 1);
        (thread->env->__jmpbuf)[JB_SP] = translate_address(sp);
        (thread->env->__jmpbuf)[JB_PC] = translate_address(pc);
    } else {
        sigsetjmp(thread->env, 1);
    }
    sigemptyset(&thread->env->__saved_mask);
    handler->addThread(id, thread); // adds the thread to our map.
    if (id == FIRST) { // if main thread.
        handler->setRunningThread(thread);
    } else {
        handler->addToReady(thread);
    }
    sigMask(SIG_UNBLOCK);
    return id;
}

int uthread_block(int tid) {
    sigMask(SIG_BLOCK);
    Thread *thread = handler->getThreadById(tid);
    if (thread == nullptr) {
        std::cerr << "thread library error: there is no thread with that ID!" << std::endl;
        return FAILURE;
    }
    if (tid == FIRST) {
        std::cerr << "thread library error:you can not block the Main thread!" << std::endl;
        return FAILURE;
    }
    int state = thread->getState();
    if (state == RUNNING) { //TODO - scheduling decision here
        handler->blockFlag = true;
        roundRobin(SIGVTALRM);
//        handler->switchThreadToRunning();
    }
    if (state == READY) {
        if (!thread->getSleep()) { // if not asleep
            handler->removeFromReady(thread);
        }
        handler->insertBlockedThread(tid, thread);
    }
    thread->setState(BLOCKED);
    sigMask(SIG_UNBLOCK);
    return SUCCESS;
}

int uthread_resume(int tid) {
    sigMask(SIG_BLOCK);
    Thread *thread = handler->getThreadById(tid);
    if (thread == nullptr) {
        std::cerr << "thread library error: there is no thread with that ID!" << std::endl;
        return FAILURE;
    }
    if (thread->getState() == BLOCKED and !thread->getSleep()) { //and not asleep
        thread->setState(READY);
        handler->eraseBlockedThread(tid);
        handler->addToReady(thread);
    } else if (thread->getState() == BLOCKED and thread->getSleep()) { // and asleep
        thread->setState(READY);
        handler->eraseBlockedThread(tid);
    }
    sigMask(SIG_UNBLOCK);
    return SUCCESS;
}

int uthread_sleep(int num_quantums) { //TODO - scheduling decision
    sigMask(SIG_BLOCK);
    if (num_quantums <= 0) {
        std::cerr << "thread library error:quantum should be a positive number" << std::endl;
        return FAILURE;
    }
    if (handler->runner()->getTid() == FIRST) {
        std::cerr << "thread library error:main thread cant go to sleep" << std::endl;
        return FAILURE;
    }
    Thread *thread = handler->runner();
    handler->insertSleptThread(thread->getTid(), thread);
    thread->setSleepQuantums(num_quantums);
    thread->changeSleepState(true);
    roundRobin(SIGVTALRM);
    sigMask(SIG_UNBLOCK);
    return SUCCESS;
}

int uthread_get_tid() {
    return handler->runner()->getTid();
}

int uthread_get_quantums(int tid) {
    Thread *thread = handler->getThreadById(tid);
    if (thread == nullptr) {
        std::cerr << "thread library error: there is no thread with that ID!" << std::endl;
        return FAILURE;
    }
    return thread->getQuantums();
}

int uthread_get_total_quantums() {
    return quantum_total;
}



