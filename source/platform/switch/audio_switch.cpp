#ifdef BACKEND_SWITCH

#include <malloc.h>

#include <cstring>

#include <switch.h>

#include "platform/common/manager.h"
#include "platform/audio.h"
#include "platform/system.h"

#define BUFFER_SAMPLES 2048
#define NUM_BUFFERS 4

AudioOutBuffer buffers[NUM_BUFFERS];

AudioOutBuffer* currBuffer;
u32 currPos;

void audioInit() {
    audoutInitialize();
    audoutStartAudioOut();

    for(u32 i = 0; i < NUM_BUFFERS; i++) {
        buffers[i].next = nullptr;
        buffers[i].buffer_size = BUFFER_SAMPLES * sizeof(u32);
        buffers[i].buffer = memalign(0x1000, buffers[i].buffer_size);
        buffers[i].data_size = BUFFER_SAMPLES * sizeof(u32);
        buffers[i].data_offset = 0;

        memset(buffers[i].buffer, 0, buffers[i].buffer_size);
        audoutAppendAudioOutBuffer(&buffers[i]);
    }

    currBuffer = nullptr;
    currPos = 0;
}

void audioCleanup() {
    audoutStopAudioOut();
    audoutExit();

    for(u32 i = 0; i < NUM_BUFFERS; i++) {
        free(buffers[i].buffer);
    }

    currBuffer = nullptr;
    currPos = 0;
}

u32 audioGetSampleRate() {
    return audoutGetSampleRate();
}

void audioPlay(u32* buffer, long samples) {
    long remaining = samples;
    while(remaining > 0) {
        if(currBuffer == nullptr) {
            u32 count = 0;
            if(R_FAILED(audoutGetReleasedAudioOutBuffer(&currBuffer, &count))) {
                currBuffer = nullptr;
            }

            currPos = 0;
        }

        if(currBuffer != nullptr) {
            long currSamples = remaining;
            if(currSamples > BUFFER_SAMPLES - currPos) {
                currSamples = BUFFER_SAMPLES - currPos;
            }

            memcpy(&((u32*) currBuffer->buffer)[currPos], &buffer[samples - remaining], currSamples * sizeof(u32));

            currPos += currSamples;
            remaining -= currSamples;

            if(currPos >= BUFFER_SAMPLES) {
                audoutAppendAudioOutBuffer(currBuffer);

                currBuffer = nullptr;
                currPos = 0;
            }
        } else {
            break;
        }
    }
}

#endif