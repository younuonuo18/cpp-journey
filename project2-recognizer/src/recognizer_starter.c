/*
 * Student starter (intentionally incomplete) for a SAFE static-feature CSV exercise.
 * It compiles, but core functions marked TODO intentionally return "not implemented".
 * Do NOT read, execute, download, or distribute real malware samples.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FEATURE_COUNT 5
#define MAX_SAMPLES 512

typedef struct {
    double features[FEATURE_COUNT];
    int label;
} Sample;

typedef struct {
    Sample samples[MAX_SAMPLES];
    int count;
} Dataset;

typedef struct {
    double minv[FEATURE_COUNT];
    double maxv[FEATURE_COUNT];
} FeatureScaler;

typedef struct {
    double weights[FEATURE_COUNT];
    double bias;
    int trained;
} Model;

/* TODO 1: read a labeled CSV, validate each token, enforce MAX_SAMPLES. */
static int load_dataset(const char *path, Dataset *data) {
    (void)path;
    (void)data;
    puts("TODO: implement CSV parser (file opening + numeric checks).");
    return 1;
}

/* TODO 2: check sample counts, labels, entropy, byte ratios and other fields. */
static int validate_dataset(const Dataset *data) {
    (void)data;
    puts("TODO: implement independent dataset validation.");
    return 1;
}

/* TODO 3: fit parameters on train set only, then normalize with same parameters. */
static void fit_scaler(const Dataset *data, FeatureScaler *scaler) {
    (void)data;
    (void)scaler;
    puts("TODO: implement training-only min/max fitting.");
}

/* TODO 4: return sigmoid(b + sum_j weights[j] * scaled_feature[j]). */
static double predict_probability(const Model *model,
                                  const FeatureScaler *scaler,
                                  const double raw_features[FEATURE_COUNT]) {
    (void)model;
    (void)scaler;
    (void)raw_features;
    puts("TODO: implement normalized logistic prediction.");
    return -1.0;
}

/* TODO 5: read/save reference_model.txt; persist weights, bias and scaler. */
static int load_model(const char *path, Model *model, FeatureScaler *scaler) {
    (void)path;
    (void)model;
    (void)scaler;
    puts("TODO: load all model + scaler parameters.");
    return 1;
}

static void show_notice(void) {
    puts("Teaching use only: this program handles synthetic CSV static features,"
         " NOT executable files or real security judgments.");
}

int main(void) {
    Dataset data = {0};
    FeatureScaler scaler = {0};
    Model model = {0};
    char input[128];
    char path[256];
    show_notice();
    for (;;) {
        puts("\n1 Load labeled CSV | 2 Validate | 3 Load reference model"
             " | 4 Predict first loaded row | 5 Show notice | 0 Exit");
        printf("Select: ");
        if (!fgets(input, sizeof input, stdin)) break;
        int action = atoi(input);
        if (action == 0) break;
        if (action == 1 || action == 3) {
            printf("Path: ");
            if (!fgets(path, sizeof path, stdin)) break;
            path[strcspn(path,"\r\n")] = '\0';
        }
        switch (action) {
        case 1: (void)load_dataset(path, &data); break;
        case 2: (void)validate_dataset(&data); break;
        case 3: (void)load_model(path, &model, &scaler); break;
        case 4:
            if (data.count <= 0 || !model.trained) {
                puts("Load a dataset and model before prediction.");
            } else {
                printf("Probability: %.6f\n",
                       predict_probability(&model,&scaler,data.samples[0].features));
            }
            break;
        case 5: show_notice(); break;
        default: puts("Unknown menu selection."); break;
        }
    }
    /* Call this during advanced model training: fit_scaler(&data, &scaler); */
    if (0) fit_scaler(&data,&scaler); /* Only to illustrate the interface. */
    return 0;
}
