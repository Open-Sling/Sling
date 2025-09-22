#include "sling.h"
#include <stdio.h>

/* Native 'hello' implementation exposed to Sling */
static Value native_hello(int argc, Value *argv) {
    (void)argc; (void)argv;
    printf("Hello from native C module!\n");
    return (Value){ .type = VAL_NUMBER, .number = 0 };
}

/* Module init that registers the native with the runtime */
void sling_register_module_hello(void) {
    sling_register_native("hello", native_hello);
}
