#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int debug = 0;

const float NN_MAX = +1.0;
const float NN_MIN = -1.0;

struct nn {
  int num_layers;    // number of layers
  int const *layout; // layout[num_layers]; pointer to array with number of neurons per layer including input and output layer
  int num_weights;   // total number of weights
  int num_neurons;   // total number of neurons excluding inputs
  float *weight;   // weight[num_weights]; pointer to array with weights
  float *output;   // output[num_neurons]; pointer to array with neuron outputs
  float data[0];   // data[num_weights+num_neurons]
};

float nn_act(struct nn *nn, float a) {
  float x = (float)a;
#if 0
  if (a > NN_MAX) x = NN_MAX;
  else if (a < NN_MIN) x = NN_MIN;
#endif
  if (debug) printf("%s: %f -> %f\n", __func__, a, x);
  return x;
}

void nn_randomize(struct nn *nn, float min, float max) {
  for (int i = 0; i < nn->num_weights; ++i) {
    nn->weight[i] = min + ((float)rand() / RAND_MAX) * (max - min);
  }
}

struct nn *nn_init(int num_layers, int const *layout) {
  if (num_layers < 2) return 0;
  if (!layout) return 0;

  if (debug) {
    printf("%s: num_layers %d\n", __func__, num_layers);
    printf("%s: layout[0] %d (input layer)\n", __func__, layout[0]);
  }

  // calcualte number of weights and number of neurons; exclude input layer
  int num_weights = 0;
  int num_neurons = 0;
  for (int l = 1; l < num_layers; ++l) {
    const int nw = (layout[l-1] + 1) * layout[l]; // number of weights in this layer; +1 for bias for each neuron in this layer
    if (debug) printf("%s: layout[%d] %d, layout[%d] %d, weights %d%s\n", __func__, l, layout[l], l-1, layout[l-1], nw, (l == num_layers-1) ? " (output layer)" : "");
    num_weights += nw;
    num_neurons += layout[l];
  }

  if (debug) printf("%s: num_weights %d, num_neurons %d\n", __func__, num_weights, num_neurons);

  // allocate
  const int size = sizeof(struct nn) + (num_weights + num_neurons) * sizeof(float);
  struct nn *nn = malloc(size);
  if (!nn) return 0;

  // initialize
  nn->num_layers = num_layers;
  nn->layout = layout;
  nn->num_weights = num_weights;
  nn->num_neurons = num_neurons;
  nn->weight = nn->data;
  nn->output = nn->weight + num_weights;

  nn_randomize(nn, NN_MIN, NN_MAX);

  if (debug) printf("%s: done\n", __func__);
  return nn;
}

float const *nn_run(struct nn *nn, float const *input) {
  float const *output = nn->output + nn->num_neurons - nn->layout[nn->num_layers-1]; // output layer

  float const *w = nn->weight;
  float *o = nn->output;

  for (int l = 1; l < nn->num_layers; ++l) {
    if (l == 2) input = nn->output; // switch from inputs to neuron outputs on the 3rd layer
    for (int n = 0; n < nn->layout[l]; ++n) {
      float const *i = input;
      if (debug) printf("%s: l%dn%d bias %f\n", __func__, l, n, *w);
      float sum = *w++; // bias
      for (int x = 0; x < nn->layout[l-1]; ++x) {
        if (debug) printf("%s: l%dn%dx%d w %f i %f\n", __func__, l, n, x, *w, *i);
        sum += *w++ * *i++;
        if (debug) printf("%s: l%dn%dx%d sum %f\n", __func__, l, n, x, sum);
      }
      *o++ = nn_act(nn, sum);
    }
    input += nn->layout[l-1];
  }

  assert(w - nn->weight == nn->num_weights);
  assert(o - nn->output == nn->num_neurons);

  return output;
}

int main(void) {
  const int num_inputs = 2;
  const int num_outputs = 1;
  const int num_layers = 3;
  const int layout[] = {num_inputs, 2, num_outputs};
  
  debug = 1;

  struct nn *nn = nn_init(num_layers, layout);
  if (!nn) return 1;
  
  nn->weight[0] = -0.5;
  nn->weight[1] = +1.0;
  nn->weight[2] = +1.0;

  nn->weight[3] = +1.5;
  nn->weight[4] = -1.0;
  nn->weight[5] = -1.0;

  nn->weight[6] = -1.5;
  nn->weight[7] = +1.0;
  nn->weight[8] = +1.0;

  float inputs[][2] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
  for (int i = 0; i < 4; ++i) {
    float const *output = nn_run(nn, inputs[i]);
    if (!output) return 1;

    for (int o = 0; o < num_outputs; ++o) printf("output #%d: %f\n", o, output[o]);
  }

  return 0;
}
