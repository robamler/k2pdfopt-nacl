/**
 * Copyright (c) 2012, The Chromium Authors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef QUEUE_H_
#define QUEUE_H_

#include "ppapi/c/pp_var.h"

/* This file implements a single-producer/single-consumer queue, using a mutex
 * and a condition variable.
 *
 * There are techniques to implement a queue like this without using memory
 * barriers or locks on x86, but ARM's memory system is different from x86, so
 * we cannot make the same assumptions about visibility order of writes. Using a
 * mutex is slower, but also simpler.
 *
 * We make the assumption that messages are only enqueued on the main thread
 * and consumed on the worker thread. Because we don't want to block the main
 * thread, EnqueueMessage will return zero if the message could not be enqueued.
 *
 * DequeueMessage will block until a message is available using a condition
 * variable. Again, this may not be as fast as spin-waiting, but will consume
 * much less CPU (and battery), which is important to consider for ChromeOS
 * devices. */

void InitializeMessageQueue();
int EnqueueMessage(struct PP_Var message);
struct PP_Var DequeueMessage();

#endif  /* QUEUE_H_ */
