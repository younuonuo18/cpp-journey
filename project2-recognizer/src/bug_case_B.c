/* Teaching bug case B: deliberately omits FeatureScaler when persisting model. */
/* Build: gcc -std=c11 -Wall -Wextra -pedantic bug_case_B.c -lm -o bug_case_B */
#include <math.h>
#include <stdio.h>
typedef struct { double w, bias; } Model;
typedef struct { double minv, maxv; } FeatureScaler;
static double predict(Model m, FeatureScaler s, double raw) {
    double norm = (raw-s.minv)/(s.maxv-s.minv);
    return 1.0/(1.0+exp(-(m.w*norm+m.bias)));
}
/* Deliberate design error: weights and bias saved, scaler omitted. */
static int save_bug(const char *path, Model m) {
    FILE *f = fopen(path,"w");
    if (!f) return -1;
    fprintf(f,"%.17g %.17g\n",m.w,m.bias);
    return fclose(f);
}
static int load_bug(const char *path, Model *m) {
    FILE *f = fopen(path,"r");
    if (!f) return -1;
    int ok = fscanf(f,"%lf %lf", &m->w, &m->bias)==2;
    fclose(f);
    return ok ? 0:-1;
}
int main(void) {
    Model original = {2.0,-1.0}, restored = {0};
    FeatureScaler train_scaler={50.0,100.0};
    double x = 90.0;
    double before = predict(original,train_scaler,x);
    if (save_bug("bug_case_B_temp_model.txt", original) ||
        load_bug("bug_case_B_temp_model.txt", &restored)) return 1;
    /* WRONG fallback after restart: original scaler is missing. */
    FeatureScaler wrong_scaler={0,1};
    double after = predict(restored,wrong_scaler,x);
    printf("prediction_before=%.10f\nprediction_after=%.10f\n",before,after);
    puts("Task: save + restore training scaler along with weights and bias.");
    remove("bug_case_B_temp_model.txt");
    return 0;
}
