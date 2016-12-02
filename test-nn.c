/*
 * Copyright 2016 Wink Saville
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#if !defined(DBG)
#define DBG 0
#endif

#include "NeuralNet.h"
#include "NeuralNetIo.h"
#include "dbg.h"
#include "rand0_1.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define INPUT_COUNT 2
typedef struct InputPattern {
  int count;
  double data[INPUT_COUNT];
} InputPattern;

#define OUTPUT_COUNT 1
typedef struct OutputPattern {
  int count;
  double data[OUTPUT_COUNT];
} OutputPattern;

InputPattern xor_input_patterns[] = {
  { .count = INPUT_COUNT, .data[0] = 0, .data[1] = 0 },
  { .count = INPUT_COUNT, .data[0] = 1, .data[1] = 0 },
  { .count = INPUT_COUNT, .data[0] = 0, .data[1] = 1 },
  { .count = INPUT_COUNT, .data[0] = 1, .data[1] = 1 },
};

OutputPattern xor_target_patterns[] = {
  { .count = OUTPUT_COUNT, .data[0] = 0 },
  { .count = OUTPUT_COUNT, .data[0] = 1 },
  { .count = OUTPUT_COUNT, .data[0] = 1 },
  { .count = OUTPUT_COUNT, .data[0] = 0 },
};

NeuralNet nn;

OutputPattern xor_output[sizeof(xor_target_patterns)/sizeof(OutputPattern)];

int main(int argc, char** argv) {
  Status status;
  int epoch = 0;

  NeuralNetIoWriter writer;


  dbg("test-nn:+\n");

  if (argc < 3) {
    printf("Usage: %s <number of epochs> <output path>\n", argv[0]);
    status = STATUS_ERR;
    goto donedone;
  }
  int epoch_count = atoi(argv[1]);
  char* out_path = argv[2];

  dbg("test-nn: epoch_count=%d\n", epoch_count);

  // seed the random number generator
#if 0
  struct timespec spec;
  clock_gettime(CLOCK_REALTIME, &spec);
  double dnow_us = (((double)spec.tv_sec * 1.0e9) + spec.tv_nsec) / 1.0e3;
  int now = (int)(long)dnow_us;
  dbg("dnow_us=%lf now=0x%x\n", dnow_us, now);
  srand(now);
#else
  srand(1);
#endif

  int num_inputs = 2;
  int num_hidden = 1;
  int num_outputs = 1;
  status = NeuralNet_init(&nn, num_inputs, num_hidden, num_outputs);
  if (StatusErr(status)) goto done;

  // Each hidden layer is fully connected plus a bias
  int hidden_neurons = 2;
  status = nn.add_hidden(&nn, hidden_neurons);
  if (StatusErr(status)) goto done;

  status = nn.start(&nn);
  if (StatusErr(status)) goto done;

  double error;
  double error_threshold = 0.0004;
  int pattern_count = sizeof(xor_input_patterns)/sizeof(InputPattern);
  int* rand_ps = calloc(pattern_count, sizeof(int));

  status = NeuralNetIoWriter_init(&writer, &nn, nn.get_points(&nn), out_path);
  if (StatusErr(status)) goto done;


  for (epoch = 0; epoch < epoch_count; epoch++) {
    error = 0.0;

    // Re-order rand_patterns
    for (int p = 0; p < pattern_count; p++) {
      rand_ps[p] = p;
    }

    // Shuffle rand_patterns by swapping the current
    // position t with a random location after the
    // current position.
    for (int p = 0; p < pattern_count; p++) {
      double r0_1 = rand0_1();
      int rp = p + (int)(r0_1 * (pattern_count - p));
      int t = rand_ps[p];
      rand_ps[p] = rand_ps[rp];
      rand_ps[rp] = t;
      dbg("r0_1=%lf rp=%d rand_ps[%d]=%d\n", r0_1, rp, p, rand_ps[p]);
    }

    for (int rp = 0; rp < pattern_count; rp++) {
      int p = rand_ps[rp];
      nn.set_inputs(&nn, (Pattern*)&xor_input_patterns[p]);
      nn.process(&nn);
      xor_output[p].count = OUTPUT_COUNT;
      nn.get_outputs(&nn, (Pattern*)&xor_output[p]);
      error += nn.adjust_weights(&nn, (Pattern*)&xor_output[p],
          (Pattern*)&xor_target_patterns[p]);

      writer.begin_epoch(&writer, (epoch * pattern_count) + rp);
      writer.write_epoch(&writer);
      writer.end_epoch(&writer);
    }
    if ((epoch % 100) == 0) {
      printf("\nEpoch=%-6d : error=%lf", epoch, error);
    }
    if (error < error_threshold) {
      break;
    }
  }
  printf("\n\nEpoch=%d Error=%lf\n", epoch, error);

  nn.stop(&nn);

  printf("\nPat");
  for (int i = 0; i < xor_input_patterns[0].count; i++) {
    printf("\tInput%-4d", i);
  }
  for (int t = 0; t < xor_target_patterns[0].count; t++) {
    printf("\tTarget%-4d", t);
  }
  for (int o = 0; o < xor_output[0].count; o++) {
    printf("\tOutput%-4d", o);
  }
  printf("\n");
  for (int p = 0; p < pattern_count; p++) {
    printf("%d", p);
    for (int i = 0; i < xor_input_patterns[p].count; i++) {
      printf("\t%lf", xor_input_patterns[p].data[i]);
    }
    for (int t = 0; t < xor_target_patterns[p].count; t++) {
      printf("\t%lf", xor_target_patterns[p].data[t]);
    }
    for (int o = 0; o < xor_output[p].count; o++) {
      printf("\t%lf", xor_output[p].data[o]);
    }
    printf("\n");

  }


done:
  writer.deinit(&writer, epoch);
  nn.deinit(&nn);

donedone:
  dbg("test-nn:- status=%d\n", status);
  return 0;
}
