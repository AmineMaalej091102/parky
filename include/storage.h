#ifndef STORAGE_H
#define STORAGE_H

#include "models.h"

int storage_init(ParkyData *data);
int storage_load(ParkyData *data);
int storage_save(const ParkyData *data);
void storage_cleanup(void);
void storage_seed(ParkyData *data);
int generate_demo_data(ParkyData *data);

#endif
