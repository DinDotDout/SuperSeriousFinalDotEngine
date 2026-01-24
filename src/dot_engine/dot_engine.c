#include "dot_engine.h"

int main() {
    Application app;
    application_init(&app);
    application_run(&app);
    application_shutdown(&app);
    return 0;
}
