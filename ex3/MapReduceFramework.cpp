//
// Created by alonn on 4/30/2022.
//
// includes:
#include "Barrier.h"
#include <atomic>
#include "MapReduceFramework.h"
#include <iostream>
#include <algorithm>


//defines:
typedef struct JobContext JobContext;
#define BIT_MASK 0x7ffffff

struct ThreadContext {
    int threadID;
    Barrier *barrier;
    IntermediateVec *intermediateVec;
    std::atomic<stage_t> *atomicState;
    const InputVec *inputVec;
    pthread_mutex_t *writeToOutMutex;
    JobContext *jobContext;
    IntermediateVec *postShuffle;
};

struct JobContext {
    std::atomic<bool> *join;
    pthread_t *allThreads;
    std::atomic<stage_t> *atomicState; // was supposed to be 64 bit but it got complicated.
    pthread_mutex_t *writeToOutMutex;
    pthread_mutex_t *reduceMutex;
    pthread_mutex_t *stateMutex;
    pthread_mutex_t *sizeMutex;
    ThreadContext *threadContext;
    Barrier barrier;
    const MapReduceClient *client;
    const InputVec *inputVec;
    OutputVec *outputVec;
    std::vector<IntermediateVec *> *intermediateVecs;
    int multiThreadNum;
    std::atomic<u_int> *mapPhaseInput;
    std::vector<IntermediateVec *> *shufflePhase;
    int *numberOfPairs;
    std::atomic<u_int> *numOfPairsTotal;
    std::atomic<u_int> *proccesed;
};

void *mapS(ThreadContext *threadContext) {
    int old_val;
    int inputSize = threadContext->jobContext->inputVec->size();
    old_val = (*(threadContext->jobContext->mapPhaseInput))++; //use old value and increment after.
    while (old_val < inputSize) {
        threadContext->jobContext->client->map(threadContext->jobContext->inputVec->at(old_val).first,
                                               threadContext->jobContext->inputVec->at(old_val).second, threadContext);
        old_val = (*(threadContext->jobContext->mapPhaseInput))++;
        (*(threadContext->jobContext->proccesed)) += 1;
    }

}

void *shuffle(ThreadContext *threadContext) {
    // all vectors already sorted when we reach this stage.
    while (true) {
        K2 *maxVal = nullptr;
        for (IntermediateVec *interVec: *threadContext->jobContext->intermediateVecs) {
            if (!interVec->empty()) {
                if (!maxVal) {
                    maxVal = interVec->back().first;
                } else {
                    if (*maxVal < *interVec->back().first) {
                        maxVal = interVec->back().first;
                    }
                }
            }
        }
        if (!maxVal) {
            break; // means all vecs are empty!
        }
        auto postDupVec = new IntermediateVec();
        for (IntermediateVec *interVec:*threadContext->jobContext->intermediateVecs) {
            while (!interVec->empty() && !(interVec->back().first->operator<(*maxVal)) &&
                   (!(*maxVal < *interVec->back().first))) {
                postDupVec->push_back(interVec->back());
                interVec->pop_back();
                (*(threadContext->jobContext->proccesed)) += 1;

            }
        }
        threadContext->jobContext->shufflePhase->push_back(postDupVec);
    }
}

void *reduce(ThreadContext *threadContext) {
    if (pthread_mutex_lock(threadContext->jobContext->reduceMutex) != 0) {
        fprintf(stderr, "System error: error on pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
    while (true) {
        if ((threadContext->jobContext->shufflePhase->empty())) {
            if (pthread_mutex_unlock(threadContext->jobContext->reduceMutex) != 0) {
                fprintf(stderr, "System error: error on pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        } else {
            IntermediateVec *vec = threadContext->jobContext->shufflePhase->back();
            threadContext->jobContext->shufflePhase->pop_back();
            if (pthread_mutex_unlock(threadContext->jobContext->reduceMutex) != 0) {
                fprintf(stderr, "System error: error on pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            threadContext->jobContext->client->reduce(vec, threadContext);
            (*(threadContext->jobContext->proccesed)) += vec->size();
            delete vec;
        }
    }
}


bool compare(IntermediatePair i, IntermediatePair j) {
    return i.first->operator<(*(j.first));
}

void getNumPairs(ThreadContext *threadContext) {
    if (!threadContext->intermediateVec->empty()) {
        if (pthread_mutex_lock(threadContext->jobContext->sizeMutex) != 0) {
            fprintf(stderr, "System error: error on pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        (*(threadContext->jobContext->numOfPairsTotal)) += threadContext->intermediateVec->size();
        if (pthread_mutex_unlock(threadContext->jobContext->sizeMutex) != 0) {
            fprintf(stderr, "System error: error on pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
}

void *mapReduce(void *threadContext) {
    auto threadC = (ThreadContext *) threadContext;
    *(threadC->atomicState) = MAP_STAGE; //set phase 1.
    *(threadC->jobContext->proccesed) = 0;
    mapS(threadC); //map phase.
    std::sort(threadC->intermediateVec->begin(), threadC->intermediateVec->end(), compare); // sort phase.
    getNumPairs(threadC);
    threadC->barrier->barrier(); //makes sure mapping is finished for all threads.
    if (threadC->threadID == 0) { //only thread 0 will shuffle.
        if (pthread_mutex_lock(threadC->jobContext->sizeMutex) != 0) {
            fprintf(stderr, "System error: error on pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        *(threadC->jobContext->atomicState) = SHUFFLE_STAGE;  //phase 1.
        *(threadC->jobContext->proccesed) = 0; //reset pairs.
        if (pthread_mutex_unlock(threadC->jobContext->sizeMutex) != 0) {
            fprintf(stderr, "System error: error on pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
        shuffle(threadC);
    }
    threadC->barrier->barrier(); // makes sure shuffle is finished for thread - without anyone going beyond
    *(threadC->jobContext->atomicState) = REDUCE_STAGE; //phase 3.
    *(threadC->jobContext->proccesed) = 0;
    reduce(threadC);
}

void emit2(K2 *key, V2 *value, void *context) {
    auto threadContext = (ThreadContext *) context;
    IntermediatePair intermediatePair = IntermediatePair(key, value);
    threadContext->intermediateVec->push_back(intermediatePair);
}

void emit3(K3 *key, V3 *value, void *context) {
    auto threadContext = (ThreadContext *) context;
    OutputPair intermediatePair = OutputPair(key, value);
    if (pthread_mutex_lock(threadContext->writeToOutMutex) != 0) {
        fprintf(stderr, "System error: error on pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
    threadContext->jobContext->outputVec->push_back(intermediatePair);
    if (pthread_mutex_unlock(threadContext->writeToOutMutex) != 0) {
        fprintf(stderr, "System error: error on pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
}

JobHandle
startMapReduceJob(const MapReduceClient &client, const InputVec &inputVec, OutputVec &outputVec, int multiThreadLevel) {
    auto threads = new pthread_t[multiThreadLevel];
    auto state = new std::atomic<stage_t>(UNDEFINED_STAGE);
    auto input = new std::atomic<u_int>(0);
    auto numOfTotal = new std::atomic<u_int>(0);
    auto processed = new std::atomic<u_int>(0);
    auto join = new std::atomic<bool>(false);
    auto *writeOutVec = new pthread_mutex_t(PTHREAD_MUTEX_INITIALIZER);
    auto *reduceMutex = new pthread_mutex_t(PTHREAD_MUTEX_INITIALIZER);
    auto *stateMutex = new pthread_mutex_t(PTHREAD_MUTEX_INITIALIZER);
    auto *sizeMutex = new pthread_mutex_t(PTHREAD_MUTEX_INITIALIZER);
    Barrier barrier(multiThreadLevel);
    auto intermediateVecs = new std::vector<IntermediateVec *>;
    auto postShuffleInterVecs = new std::vector<IntermediateVec *>;
    auto threadContext = new ThreadContext[multiThreadLevel];
    int *pairs = new int(0);
    auto *jobC = new JobContext{join, threads, state, writeOutVec, reduceMutex, stateMutex, sizeMutex,
                                threadContext, barrier, &client, &inputVec, &outputVec, intermediateVecs,
                                multiThreadLevel, input, postShuffleInterVecs, pairs, numOfTotal, processed};
    for (int i = 0; i < multiThreadLevel; i++) {
        auto intermediateVec = new std::vector<IntermediatePair>();
        intermediateVecs->push_back(intermediateVec);
        threadContext[i].threadID = i;
        threadContext[i].barrier = &jobC->barrier;
        threadContext[i].intermediateVec = intermediateVec;
        threadContext[i].inputVec = &inputVec;
        threadContext[i].atomicState = jobC->atomicState;
        threadContext[i].writeToOutMutex = jobC->writeToOutMutex;
        threadContext[i].jobContext = jobC;
        if (pthread_create(threads + i, NULL, mapReduce, threadContext + i) != 0) {
            fprintf(stderr, "System error: error on pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    return static_cast<JobHandle> (jobC);
}

// combines all threads so all threads will wait until finished.
void waitForJob(JobHandle job) {
    auto jobC = (JobContext *) job;
    bool flag = jobC->join->load();
    jobC->join->exchange(!flag);
    if (jobC->join) { // use of atomic boolean inorder to avoid joining twice.
        (*(jobC->join)) = true;
        for (int i = 0; i < jobC->multiThreadNum; i++) {
            if (pthread_join(jobC->allThreads[i], NULL) != 0) {
                fprintf(stderr, "System error: error on pthread_join");
                exit(EXIT_FAILURE);
            }
        }
    }
    (*(jobC->join)) = true;
}

void getJobState(JobHandle job, JobState *state) {
    auto jobC = (JobContext *) job;
    if (pthread_mutex_lock(jobC->stateMutex) != 0) {
        fprintf(stderr, "System error: error on pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
    state->stage = jobC->atomicState->load();
    if (state->stage == MAP_STAGE) {
        state->percentage = (float) jobC->proccesed->load() / jobC->inputVec->size() * 100;
    } else if (state->stage == SHUFFLE_STAGE || state->stage == REDUCE_STAGE) {
        state->percentage = (float) jobC->proccesed->load() / jobC->numOfPairsTotal->load() * 100;
    } else {
        state->percentage = 0;
    }
    if (pthread_mutex_unlock(jobC->stateMutex) != 0) {
        fprintf(stderr, "System error: error on pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
}

void closeJobHandle(JobHandle job) {
    waitForJob(job);
    auto jobC = (JobContext *) job;
    for (int i = 0; i < jobC->multiThreadNum; i++) {
        delete jobC->threadContext[i].intermediateVec;
    }
    pthread_mutex_destroy(jobC->reduceMutex);
    pthread_mutex_destroy(jobC->writeToOutMutex);
    pthread_mutex_destroy(jobC->stateMutex);
    pthread_mutex_destroy(jobC->sizeMutex);
    delete jobC->threadContext;
    delete jobC->allThreads;
    delete jobC->atomicState;
    delete jobC->intermediateVecs;
    delete jobC->shufflePhase;
    delete jobC->numberOfPairs;
    delete jobC->mapPhaseInput;
    delete jobC->join;
    delete jobC->numOfPairsTotal;
    delete jobC->proccesed;
    delete jobC;
}
