#ifndef STORAGE_H
#define STORAGE_H

#include "models.h"

int storage_init(ParkyData *data);
int storage_load(ParkyData *data);
int storage_save(const ParkyData *data);
void storage_seed(ParkyData *data);

#endif
