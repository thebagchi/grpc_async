#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "app_msg.h"
#include "queue.hpp"

struct EventBase;

using EventQueue = Queue<EventBase>;

extern EventQueue event_queue;

void RunProcessor(void *arg);
#endif
