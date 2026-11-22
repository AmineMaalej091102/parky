#include <stdio.h>
#include "models.h"
#include "storage.h"
#include "ui.h"

int main(int argc, char **argv)
{
    ParkyData data;

    if (storage_init(&data) != 0) {
        fprintf(stderr, "Storage initialization failed.\n");
        return 1;
    }

    return ui_run(&data, argc, argv);
}
